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
    computeObjLiveness();
}

// Backward dataflow:
//   liveIn[N] = use[N] ∪ ∪{ liveIn[succ] | succ ∈ N.successors }
// where use[N] is the set of obj NodeIDs that some VFG node attached
// to N consumes via an indirect SVFG in-edge (= obj that flow into a
// load / mphi / FPIN / etc. at this node).
void FullSparseAbstractInterpretation::computeObjLiveness()
{
    ICFG* icfg = svfir->getICFG();

    // Step 1: precompute use[N] for every ICFG node.
    Map<const ICFGNode*, Set<NodeID>> useSet;
    for (auto it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
    {
        const ICFGNode* N = it->second;
        Set<NodeID>& u = useSet[N];
        for (const VFGNode* v : N->getVFGNodes())
        {
            for (auto eit2 = v->InEdgeBegin(); eit2 != v->InEdgeEnd(); ++eit2)
            {
                if (auto* ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(*eit2))
                    for (NodeID id : ie->getPointsTo())
                        u.insert(id);
            }
        }
    }

    // Step 2: fixpoint over liveIn.
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
        {
            const ICFGNode* N = it->second;
            Set<NodeID> newOut;
            for (auto& outEdge : N->getOutEdges())
            {
                const ICFGNode* succ = outEdge->getDstNode();
                auto sit = liveIn.find(succ);
                if (sit != liveIn.end())
                    newOut.insert(sit->second.begin(), sit->second.end());
            }
            // newIn = use[N] ∪ newOut
            Set<NodeID> newIn = useSet[N];
            newIn.insert(newOut.begin(), newOut.end());
            auto& cur = liveIn[N];
            if (newIn.size() != cur.size() || newIn != cur)
            {
                cur = std::move(newIn);
                changed = true;
            }
        }
    }
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

// ValVar reads/writes reuse the SemiSparse path: writes go to the var's
// declaring ICFGNode (var->getICFGNode()), reads come from the same place.
// SVFG def-site routing was tried earlier but breaks on phi-like ValVars
// whose declICFG ≠ SVFG def-node icfg (notably ActualRet on RetICFGNode
// vs the call's CallICFGNode declaration).  Reusing semi is the
// consistent fix.
const AbstractValue& FullSparseAbstractInterpretation::getAbsValue(
    const ValVar* var, const ICFGNode* node)
{
    return SemiSparseAbstractInterpretation::getAbsValue(var, node);
}

bool FullSparseAbstractInterpretation::hasAbsValue(
    const ValVar* var, const ICFGNode* node) const
{
    return SemiSparseAbstractInterpretation::hasAbsValue(var, node);
}

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
//  Full-sparse — merge-time ObjVar pull.
//
//  joinStates gates BOTH ValVars and ObjVars: in full-sparse mode
//  neither flows along ICFG edges.  ObjVars that an ICFGNode actually
//  needs are then pulled, per-MRSVFGNode, from the SVFG-reaching
//  def-sites' traces in mergeStatesFromPredecessors.  By WTO order
//  the def-sites have already been processed by the time `node` is.
// =====================================================================

void FullSparseAbstractInterpretation::joinStates(
    AbstractState& dst, const AbstractState& src)
{
    // Semi-style: gate ValVars only (they live at SVFG def-sites);
    // copy ObjVars + freedAddrs through joinWith.  Branch narrowing
    // (isCmpBranchFeasible / isSwitchBranchFeasible) modifies the
    // predState's _addrToAbsVal in-place; semi-style propagation
    // surfaces the narrowed values into trace[node].
    //
    // The full-sparse storage win comes from the post-merge prune in
    // mergeStatesFromPredecessors below, which drops obj that aren't
    // live at this node.
    auto savedVars = dst.getVarToVal();
    dst.joinWith(src);
    dst.clearValVars();
    for (const auto& [id, val] : savedVars)
        dst[id] = val;
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    // Base merge runs branch refinement (isBranchFeasible narrows
    // predState in-place) and joinStates (semi-style copies the
    // narrowed obj into merged → trace[node]).  After the base,
    // trace[node]._addrToAbsVal contains every obj that any feasible
    // predecessor carried — same density as semi-sparse.
    bool ok = AbstractInterpretation::mergeStatesFromPredecessors(node);

    // Liveness prune: keep only obj that are live at `node`.  An obj
    // is live if it's read at this node or some forward-reachable
    // node (use[N] ∪ liveOut[N]).  Dead obj never get read again, so
    // dropping them from trace[node] is sound and shrinks per-node
    // storage well below semi-sparse on programs with localised obj
    // usage.
    auto live_it = liveIn.find(node);
    if (live_it == liveIn.end()) return ok;
    const Set<NodeID>& live = live_it->second;

    AbstractState& dst = abstractTrace[node];
    auto saved = dst.getLocToVal();   // copy
    dst.clearAddrs();
    for (const auto& [id, val] : saved)
        if (live.count(id))
            dst.store(AbstractState::getVirtualMemAddress(id), val);
    return ok;
}

// =====================================================================
//  Full-sparse — Phase 2 GepObj overlay.
//
//  Bridges the GEP field-precision asymmetry: a const-offset store
//  (arr[1]=1 / s.f=1) writes a specific GepObjVar id; a dynamic-offset
//  or cross-function read often resolves Andersen pts to the *base*
//  obj id; the SVFG indirect edge between them carries the load's pts
//  so the gep-field id never reaches the load via the SVFG-pull path.
//  The overlay records every GepObjVar write in a flat map; reads
//  consult it before falling back to the SVFG-populated trace.
//
//  Trade-off: weak update / flow-insensitive (a[3]=7; a[3]=11 joins
//  to [7,11] rather than strong-updating to 11).  Same trade-off as
//  the 0413 reference design.
//
//  TODO: AbsExtAPI's memcpy / memmove / strcpy handlers call
//  as.store(addr,...) directly, bypassing this override.  If the
//  destination addr decodes to a GepObjVar, the value won't enter
//  the overlay — known precision leak; revisit if benchmark exposes.
// =====================================================================

void FullSparseAbstractInterpretation::storeValue(
    const ValVar* pointer, const AbstractValue& val, const ICFGNode* node)
{
    AbstractInterpretation::storeValue(pointer, val, node);

    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    for (auto addr : ptrVal.getAddrs())
    {
        if (!AbstractState::isVirtualMemAddress(addr)) continue;
        NodeID id = getAbsState(node).getIDFromAddr(addr);
        if (!SVFUtil::isa<GepObjVar>(svfir->getSVFVar(id))) continue;
        auto it = gepOverlay.find(id);
        if (it == gepOverlay.end())
            gepOverlay[id] = val;
        else
            it->second.join_with(val);
    }
}

AbstractValue FullSparseAbstractInterpretation::loadValue(
    const ValVar* pointer, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    AbstractState& as = getAbsState(node);
    AbstractValue res;

    for (auto addr : ptrVal.getAddrs())
    {
        if (AbstractState::isVirtualMemAddress(addr))
        {
            NodeID id = as.getIDFromAddr(addr);
            if (SVFUtil::isa<GepObjVar>(svfir->getSVFVar(id)))
            {
                auto it = gepOverlay.find(id);
                if (it != gepOverlay.end())
                {
                    res.join_with(it->second);
                    continue;
                }
            }
        }
        res.join_with(as.load(addr));
    }
    return res;
}

// =====================================================================
//  Semi-sparse cycle helpers (sparse-shape gather / scatter).
// =====================================================================

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
