//===- SparseAbstractInterpretation.cpp -- Sparse Abstract Execution----//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===---------------------------------------------------------------------===//

#include "AE/Svfexe/SparseAbstractInterpretation.h"
#include "AE/Svfexe/AEWTO.h"
#include "Graphs/SparseObjGraph.h"
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"

using namespace SVF;

// FullSparse Obj inputs follow prebuilt routes. Shared SSA def-site access
// and WTO gather/scatter retain their existing implementations.

// =====================================================================
//  Full-sparse — class lifecycle + SVFG construction.
// =====================================================================

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation() = default;

void FullSparseAbstractInterpretation::buildSVFG()
{
    svfgBuilder = std::make_unique<SVFGBuilder>(true);
    svfgBuilder->buildFullSVFG(preAnalysis->getPointerAnalysis());
    objRoutes = std::make_unique<SparseObjGraph>(
                    *svfgBuilder->getSVFG(), *svfir->getICFG(),
                    *preAnalysis->getPointerAnalysis(), *svfir);
}

void FullSparseAbstractInterpretation::mergeFreedAddrs(AbstractState& dst,
        const AbstractState& src)
{
    for (NodeID addr : src.getFreedAddrs())
        dst.addToFreedAddrs(addr);
}

void FullSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src, const NodeBS& objs)
{
    for (NodeID obj : objs)
    {
        const u32_t addr = AbstractState::getVirtualMemAddress(obj);
        const auto value = src.getLocToVal().find(src.getIDFromAddr(addr));
        if (value != src.getLocToVal().end())
        {
            if (dst.getLocToVal().count(dst.getIDFromAddr(addr)))
                dst.load(addr).join_with(value->second);
            else
                dst.store(addr, value->second);
        }
    }
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    const auto& graphEdges = objRoutes->getInputs(node);
    AbstractState previous;
    bool hasPrevious = false;
    for (const SparseObjGraph::Edge* edge : graphEdges)
    {
        if (edge->getSrcNode()->getICFGNode() == node && hasAbsState(node))
        {
            previous = getAbsState(node);
            hasPrevious = true;
            break;
        }
    }

    // The ICFG merge determines reachability and propagates freed addresses.
    const bool reachable = AbstractInterpretation::mergeStatesFromPredecessors(node);
    if (reachable)
    {
        AbstractState merged = getAbsState(node);
        for (const SparseObjGraph::Edge* edge : graphEdges)
        {
            const SparseObjGraph::Node* sourceNode = edge->getSrcNode();
            const ICFGNode* source = sourceNode->getICFGNode();
            const AbstractState* sourceState = source == node
                                               ? (hasPrevious ? &previous : nullptr)
                                               : (hasAbsState(source)
                                                  ? &getAbsState(source) : nullptr);
            if (!sourceState)
                continue;

            const AbstractState* input = sourceState;
            AbstractState refined;
            if (sourceNode->branch)
            {
                refined = *sourceState;
                if (!isBranchEdgeFeasible(sourceNode->branch, refined) ||
                        !applyBranchRefinement(sourceNode->branch, refined))
                    continue;
                input = &refined;
            }
            joinStates(merged, *input, svfir->getAllFieldsObjVars(
                           svfir->getBaseObject(sourceNode->base)));
        }
        updateAbsState(node, merged);
    }
    return reachable;
}

AbstractState SemiSparseAbstractInterpretation::getFullCycleHeadState(
    const ICFGCycleWTO* cycle)
{
    // Start from the dense snapshot (ObjVars + any ValVars that happen to
    // be cached at cycle_head's trace entry).
    AbstractState snap = AbstractInterpretation::getFullCycleHeadState(cycle);

    const Set<const ValVar*>& valVars = preAnalysis->getCycleValVars(cycle);
    if (valVars.empty())
        return snap;  // no cycle ValVars known: nothing to pull

    // Drop stale ValVar entries and pull each cycle ValVar from its
    // def-site.  ValVars without a genuine stored value are skipped to
    // avoid getAbsValue's top-fallback contaminating body def-sites on
    // the subsequent widen/narrow scatter.
    snap.clearValVars();
    for (const ValVar* v : valVars)
    {
        const ICFGNode* defSite = v->getICFGNode();
        if (!defSite || !hasAbsValue(v, defSite))
            continue;
        snap[v->getId()] = getAbsValue(v, defSite);
    }
    return snap;
}

bool SemiSparseAbstractInterpretation::widenCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    // Base widens, writes trace[cycle_head], and returns fixpoint bool.
    bool fixpoint = AbstractInterpretation::widenCycleState(prev, cur, cycle);

    // Scatter the widened ValVars back to their def-sites so body nodes
    // observe the widened values on the next iteration.  Matches the
    // pre-refactor semantics: scatter unconditionally, including at
    // widening fixpoint (see the narrowing-starts-with-stale-body issue
    // fixed by always writing widened state back).
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    const AbstractState& next = abstractTrace[cycle_head];
    for (const auto& [id, val] : next.getVarToVal())
        updateAbsValue(svfir->getSVFVar(id), val, cycle_head);
    return fixpoint;
}

bool SemiSparseAbstractInterpretation::narrowCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    // Delegate to base.  It returns true on the two non-scatter cases
    // (narrowing disabled, or narrow fixpoint); we preserve the original
    // "skip scatter at fixpoint" semantics by bailing early here.
    bool fixpoint = AbstractInterpretation::narrowCycleState(prev, cur, cycle);
    if (fixpoint)
        return true;

    // Non-fixpoint: base wrote the narrowed state to trace.  Scatter the
    // narrowed ValVars back to def-sites.
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    const AbstractState& next = abstractTrace[cycle_head];
    for (const auto& [id, val] : next.getVarToVal())
        updateAbsValue(svfir->getSVFVar(id), val, cycle_head);
    return false;
}

// =====================================================================
//  Shared SemiSparse/FullSparse state-access overrides. Both use the
//  established ValVar def-site reads and writes below.
// =====================================================================

void SemiSparseAbstractInterpretation::updateAbsState(
    const ICFGNode* node, const AbstractState& state)
{
    // Only replace ObjVar state.  ValVars live at their def-sites and
    // must not be overwritten when the predecessor's state is merged in.
    abstractTrace[node].updateAddrStateOnly(state);
}

void SemiSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src)
{
    // ValVars live at def-sites in semi-sparse mode; they don't flow
    // through state merges.  Iterate src's ObjVar (_addrToAbsVal) entries
    // directly and join into dst, leaving dst's ValVar map untouched.
    // _freedAddrs (used by the null-deref detector) also rides along
    // ICFG edges — there is no SVFG-level encoding of free events.
    for (const auto& [id, val] : src.getLocToVal())
    {
        u32_t addr = AbstractState::getVirtualMemAddress(id);
        if (dst.getLocToVal().count(id))
            dst.load(addr).join_with(val);
        else
            dst.store(addr, val);
    }
    for (NodeID a : src.getFreedAddrs())
        dst.addToFreedAddrs(a);
}

const ICFGNode* SemiSparseAbstractInterpretation::getICFGNode(
    const ValVar* var) const
{
    // const ValVars are all defined in global node
    if (!var->getICFGNode())
    {
        return svfir->getICFG()->getGlobalICFGNode();
    }
    // for return value of callsite, use the ret-site as def-site
    else if (SVFUtil::isa<CallICFGNode>(var->getICFGNode()) &&
             SVFUtil::isa<RetValPN>(var))
    {
        return SVFUtil::dyn_cast<CallICFGNode>(var->getICFGNode())
               ->getRetICFGNode();
    }
    // for other ValVars, use their def-site as the node to query abstract
    // value.
    else
    {
        return var->getICFGNode();
    }
}

void SemiSparseAbstractInterpretation::updateAbsValue(const ValVar* var,
        const AbstractValue& val,
        const ICFGNode* node)
{
    // Write to the var's def-site so getAbsValue stays consistent.
    const ICFGNode* defNode = var->getICFGNode();
    abstractTrace[defNode ? defNode : node][var->getId()] = val;
}

const AbstractValue& SemiSparseAbstractInterpretation::getAbsValue(
    const ValVar* var, const ICFGNode* node)
{
    // Read from the var's def-site (where updateAbsValue wrote it).
    return AbstractInterpretation::getAbsValue(var, getICFGNode(var));
}

bool SemiSparseAbstractInterpretation::hasAbsValue(const ValVar* var,
        const ICFGNode* node) const
{
    return AbstractInterpretation::hasAbsValue(var, getICFGNode(var));
}
