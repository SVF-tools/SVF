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
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"

using namespace SVF;

// SemiSparse state-access overrides (get/has/updateAbsValue,
// updateAbsState, joinStates) live in AbstractStateManager.cpp; the
// FullSparse-specific overrides — including the SVFG-backed def/use
// queries and the ValVar stubs — live below alongside the rest of
// FullSparse so the whole subclass stays in one file.

// =====================================================================
//  Full-sparse — class lifecycle + SVFG construction.
// =====================================================================

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation() = default;

void FullSparseAbstractInterpretation::buildSVFG()
{
    svfgBuilder = std::make_unique<SVFGBuilder>(true);
    svfg = svfgBuilder->buildFullSVFG(preAnalysis->getPointerAnalysis());
}

// =====================================================================
//  Full-sparse — Phase 1 state-access overrides.
//
//  ValVar reads route to the SVFG-reaching def-site (the trace there
//  holds the real entry).  ObjVar reads use the inherited base
//  behaviour: trace[N] is populated eagerly by the FullSparse merge
//  step (mergeStatesFromPredecessors) so `as.load(addr)` finds the
//  right entry without read-time synthesis.
// =====================================================================


const Set<const ICFGNode*> FullSparseAbstractInterpretation::getUseSitesOfValVar(
    const ValVar* var) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    Set<const ICFGNode*> useSites;
    for(const SVFGNode* svfgNode : svfg->getUseSitesOfValVar(var))
    {
        useSites.insert(svfgNode->getICFGNode());
    }
    return useSites;
}

const ICFGNode* FullSparseAbstractInterpretation::getDefSiteOfValVar(
    const ValVar* var) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    return svfg->getDefSiteOfValVar(var)->getICFGNode();
}

const Set<const ICFGNode*> FullSparseAbstractInterpretation::getDefSiteOfObjVar(
    const ObjVar* obj, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    Set<const ICFGNode*> defSites;
    for(auto* vNode : node->getVFGNodes())
    {
        for(const SVFGNode* svfgNode : svfg->getDefSiteOfObjVar(obj, vNode))
        {
            defSites.insert(svfgNode->getICFGNode());
        }
    }
    return defSites;
}

const Set<const ICFGNode*> FullSparseAbstractInterpretation::getUseSitesOfObjVar(
    const ObjVar* obj, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    Set<const ICFGNode*> useSites;
    for(auto* vNode : node->getVFGNodes())
    {
        for(const SVFGNode* svfgNode : svfg->getUseSitesOfObjVar(obj, vNode))
        {
            useSites.insert(svfgNode->getICFGNode());
        }
    }
    return useSites;
}


// =====================================================================
//  Full-sparse — pure SVFG-pull merge.
//
//  mergeStatesFromPredecessors builds trace[N] from scratch by walking
//  N's VFG nodes' indirect SVFG in-edges.  No ICFG-edge dense copy.
//  joinStates is a no-op safety net in case base machinery calls it.
// =====================================================================

void FullSparseAbstractInterpretation::joinStates(
    AbstractState&, const AbstractState&) // add one more ICFGNODE AS PARAM
{
    assert(false && "joinStates should not be called in full-sparse AE");
    // No-op: full-sparse doesn't propagate state along ICFG edges.
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(const ICFGNode* node)
{
    AbstractState& dst = abstractTrace[node];
    bool hasFeasiblePred = false;

    // Step 1: pure SVFG-pull for ObjVars at `node`.
    // For each VFG node v at `node`, follow each indirect SVFG in-edge
    // back to its source SVFGNode `s`; pull, for every obj id in the
    // edge's PTS, the value from trace[s->getICFGNode()][obj].  Multiple
    // sources (e.g. mphi operands) JOIN.
    Set<NodeID> pulled;
    for (const VFGNode* v : node->getVFGNodes())
    {
        for (auto eit = v->InEdgeBegin(); eit != v->InEdgeEnd(); ++eit)
        {
            const IndirectSVFGEdge* indEdge =
                SVFUtil::dyn_cast<IndirectSVFGEdge>(*eit);
            if (!indEdge) continue;
            const SVFGNode* src = SVFUtil::dyn_cast<SVFGNode>(indEdge->getSrcNode());
            if (!src) continue;
            const ICFGNode* srcICFG = src->getICFGNode();
            if (!srcICFG || !hasAbsState(srcICFG)) continue;
            AbstractState& srcTrace = getAbsState(srcICFG);
            for (NodeID id : indEdge->getPointsTo())
            {
                if (!srcTrace.getLocToVal().count(id)) continue;
                u32_t addr = AbstractState::getVirtualMemAddress(id);
                if (pulled.insert(id).second)
                    dst.store(addr, srcTrace.load(addr));
                else
                    dst.load(addr).join_with(srcTrace.load(addr));
            }
        }
    }

    // Minimal "is this node reachable" check for the function-contract
    // return value: any predecessor with state is enough.
    for (auto& edge : node->getInEdges())
    {
        if (hasAbsState(edge->getSrcNode()))
        {
            hasFeasiblePred = true;
            break;
        }
    }
    return hasFeasiblePred;
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
//  Semi-sparse state-access overrides (used by both SemiSparse and
//  FullSparse subclasses; the latter further restricts ValVar reads).
// =====================================================================

void SemiSparseAbstractInterpretation::updateAbsState(
    const ICFGNode* node, const AbstractState& state)
{
    // Only replace ObjVar state.  ValVars live at their def-sites and
    // must not be overwritten when the predecessor's state is merged in.
    abstractTrace[node].updateAddrStateOnly(state);
}

void SemiSparseAbstractInterpretation::joinStates(
    AbstractState& dst, const AbstractState& src)
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

const ICFGNode* SemiSparseAbstractInterpretation::getICFGNode(const ValVar* var) const
{
    // const ValVars are all defined in global node
    if (!var->getICFGNode())
    {
        return svfir->getICFG()->getGlobalICFGNode();
    }
    // for return value of callsite, use the ret-site as def-site
    else if (SVFUtil::isa<CallICFGNode>(var->getICFGNode()) && SVFUtil::isa<RetValPN>(var))
    {
        return SVFUtil::dyn_cast<CallICFGNode>(var->getICFGNode())->getRetICFGNode();
    }
    // for other ValVars, use their def-site as the node to query abstract value.
    else
    {
        return var->getICFGNode();
    }
}


void SemiSparseAbstractInterpretation::updateAbsValue(
    const ValVar* var, const AbstractValue& val, const ICFGNode* node)
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

bool SemiSparseAbstractInterpretation::hasAbsValue(
    const ValVar* var, const ICFGNode* node) const
{
    return AbstractInterpretation::hasAbsValue(var, getICFGNode(var));
}
