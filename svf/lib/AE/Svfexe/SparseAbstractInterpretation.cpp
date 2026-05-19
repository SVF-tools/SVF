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
//  Full-sparse — merge.
//
//  mergeStatesFromPredecessors is a thin wrapper: defer to base for
//  ICFG-edge bookkeeping (predecessor iteration, branch feasibility,
//  joinStates, updateAbsState, reachability return).  If the node is
//  reachable, run pullObjValueFlows to populate trace[node] with obj values
//  pulled along SVFG indirect in-edges.
//
//  joinStates carries only state that is not represented as MemorySSA
//  def-use flow: GepObjVar field snapshots and _freedAddrs.  Base/Dummy
//  ObjVars are handled by pullObjValueFlows, not by ICFG-edge joins.
// =====================================================================

void FullSparseAbstractInterpretation::joinStates(AbstractState& dst,
                                                  const AbstractState& src)
{
    // Propagate GepObjVar entries along ICFG edges.  Kill semantics
    // come from handleNode's as.store(addr, val) overwriting trace at
    // store sites (not from a JOIN here), so joinStates only forwards
    // the post-write snapshot.  This lets Gep fields scattered across
    // many store ICFG nodes converge at downstream use sites, and lets
    // extapi handlers (memcpy/memset/strlen) read upstream-written
    // values via plain as.load(srcAddr).  Base/Dummy are NOT propagated
    // here — they ride pullObjValueFlows Step 1 (SVFG indirect edges, with
    // MSSA chi/mu kill semantics).
    for (const auto& [id, val] : src.getLocToVal())
    {
        if (!SVFUtil::isa<GepObjVar>(svfir->getGNode(id)))
            continue;
        u32_t addr = AbstractState::getVirtualMemAddress(id);
        if (dst.getLocToVal().count(id))
            dst.load(addr).join_with(val);
        else
            dst.store(addr, val);
    }
    for (NodeID a : src.getFreedAddrs())
        dst.addToFreedAddrs(a);
}

void FullSparseAbstractInterpretation::storeValue(const ValVar* pointer,
                                                  const AbstractValue& val,
                                                  const ICFGNode* node)
{
    // Clear branch refinement for every ObjVar this store overwrites.
    // A store redefines the ObjVar; the pre-store branch constraint
    // (inherited into refinementTrace[node]) is immediately stale.
    // Without this, successors inherit the stale constraint and MEET
    // it onto the pulled post-store value, erasing the store's effect.
    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    AbstractState& as = getAbsState(node);
    for (auto addr : ptrVal.getAddrs())
    {
        NodeID objId = as.getIDFromAddr(addr);
        auto rit = refinementTrace.find(node);
        if (rit != refinementTrace.end())
            rit->second.erase(objId);
    }
    // Delegate to base for the actual ObjVar update.
    SemiSparseAbstractInterpretation::storeValue(pointer, val, node);
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    refinementTrace.erase(node);

    if (!AbstractInterpretation::mergeStatesFromPredecessors(node))
        return false;

    pullObjValueFlows(node);

    // Compose pred-inherited refinement on top of branch narrowings
    // just captured, then MEET into trace[node] so reads see narrowed.
    propagateAndApplyRefinement(node);
    return true;
}

void FullSparseAbstractInterpretation::pullObjValueFlows(const ICFGNode* node)
{
    NodeBS denseLocalObjs;
    for (const auto& item : abstractTrace[node].getLocToVal())
    {
        NodeID id = item.first;
        if (SVFUtil::isa<GepObjVar>(svfir->getGNode(id)))
            denseLocalObjs.set(id);
    }
    // e.g.
    //     store i32 7, i32* %p   ; def-site D for obj_p
    //     ...
    //     %v = load i32, i32* %p ; use-site U
    // Step 1: intra-node SVFG-pull.  For each VFG node hosted at U, walk
    // the indirect SVFG in-edges back to D; for every obj id labelling
    // the edge, JOIN the obj's value at D into U's trace.  GepObjVar
    // labels are pulled exactly.  BaseObjVar labels are expanded to every
    // sibling field via getAllFieldsObjVars because Andersen may label a
    // field-sensitive consumer with the field-insensitive base.
    //
    // Gep fields already present at this node came through the dense
    // ICFG propagation in joinStates.  Treat those as authoritative and
    // do not re-join older SVFG defs over them; otherwise a killed init
    // field can be reintroduced at the load site (e.g. a[9] initialized
    // to 9, overwritten to 10, then pulled back to [9,10]).
    // Reads/writes go through SemiSparse to bypass FullSparse's refinement
    // layer (these are def-site pulls, not real stores; refinement is
    // applied later in propagateAndApplyRefinement).
    for (const VFGNode* v : node->getVFGNodes())
    {
        for (auto eit = v->InEdgeBegin(); eit != v->InEdgeEnd(); ++eit)
        {
            const IndirectSVFGEdge* indEdge =
                SVFUtil::dyn_cast<IndirectSVFGEdge>(*eit);
            if (indEdge)
            {
                const SVFGNode* src =
                    SVFUtil::dyn_cast<SVFGNode>(indEdge->getSrcNode());
                assert(src && "SVFG incoming edge must have a source node");
                assert(v && "SVFG incoming edge must have a destination node");

                const ICFGNode* srcICFG = src->getICFGNode();
                const ICFGNode* dstICFG = v->getICFGNode();
                assert(srcICFG && "SVFG source node must have an ICFG node");
                assert(dstICFG &&
                       "SVFG destination node must have an ICFG node");

                if (!isIndirectSVFGEdgeFeasible(indEdge, v))
                    continue;

                if (srcICFG && hasAbsState(srcICFG))
                {
                    for (NodeID id : indEdge->getPointsTo())
                    {
                        SVFVar* gn = svfir->getGNode(id);
                        NodeBS idsToPull;

                        if (SVFUtil::isa<GepObjVar>(gn))
                        {
                            idsToPull.set(id);
                        }
                        else if (auto* base = SVFUtil::dyn_cast<BaseObjVar>(gn))
                        {
                            idsToPull = svfir->getAllFieldsObjVars(base);
                        }
                        else
                        {
                            idsToPull.set(id);
                        }

                        for (NodeID fid : idsToPull)
                        {
                            const ObjVar* obj =
                                SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(fid));
                            // Dense Gep propagation has already carried the
                            // current value to this node.
                            if (denseLocalObjs.test(fid))
                            {
                                continue;
                            }
                            if (obj &&
                                SemiSparseAbstractInterpretation::hasAbsValue(
                                    obj, srcICFG))
                            {
                                AbstractValue cur;
                                if (SemiSparseAbstractInterpretation::
                                        hasAbsValue(obj, node))
                                {
                                    cur = SemiSparseAbstractInterpretation::
                                        getAbsValue(obj, node);
                                }
                                cur.join_with(SemiSparseAbstractInterpretation::
                                                  getAbsValue(obj, srcICFG));
                                SemiSparseAbstractInterpretation::
                                    updateAbsValue(obj, cur, node);
                            }
                        }
                    }
                }
            }
        }
    }

    // Step 2 (boundary pull) intentionally removed: with GepObjVar dense
    // propagation in joinStates above, Gep field values arrive at use
    // sites along ICFG edges without needing the boundary pull.
}

// =====================================================================
//  Full-sparse — refinement trace machinery.
// =====================================================================

static bool hasRedefineToSameObj(const ICFGNode* node,
                                 const IndirectSVFGEdge* edge)
{
    for (const VFGNode* vfgNode : node->getVFGNodes())
    {
        if (SVFUtil::isa<StoreVFGNode>(vfgNode) &&
            vfgNode->getDefSVFVars().intersects(edge->getPointsTo()))
            return true;
    }

    return false;
}

bool FullSparseAbstractInterpretation::isIndirectSVFGEdgeFeasible(
    const IndirectSVFGEdge* edge, const VFGNode* dst)
{
    assert(edge && "Indirect SVFG edge must exist");
    assert(dst && "Indirect SVFG edge must have a destination node");

    const SVFGNode* src = SVFUtil::dyn_cast<SVFGNode>(edge->getSrcNode());
    assert(src && "Indirect SVFG edge must have an SVFG source node");

    const ICFGNode* srcICFG = src->getICFGNode();
    const ICFGNode* dstICFG = dst->getICFGNode();
    assert(srcICFG && "SVFG source node must have an ICFG node");
    assert(dstICFG && "SVFG destination node must have an ICFG node");

    const FunObjVar* fun = srcICFG->getFun();
    bool feasible = true;
    if (srcICFG == dstICFG)
    {
        feasible = true;
    }
    else if (!fun || fun != dstICFG->getFun())
    {
        feasible = true;
    }
    else
    {
        feasible = false;
        std::deque<const ICFGNode*> worklist;
        Set<const ICFGNode*> visited;
        worklist.push_back(srcICFG);
        visited.insert(srcICFG);

        while (!worklist.empty() && !feasible)
        {
            const ICFGNode* cur = worklist.front();
            worklist.pop_front();

            if (cur != srcICFG && hasRedefineToSameObj(cur, edge))
            {
                // This ICFG path redefines the same object before dst.
            }
            else
            {
                // Treat a call as an intra-procedural summary edge for path
                // queries. Feasibility of the callee body is handled by the
                // normal analysis; here we only need caller-side reachability,
                // e.g. entry -> ret-site.
                if (const CallICFGNode* call =
                        SVFUtil::dyn_cast<CallICFGNode>(cur))
                {
                    const ICFGNode* succ = call->getRetICFGNode();
                    if (!succ || succ->getFun() != fun)
                    {
                        // Ignore missing or cross-function return summaries.
                    }
                    else if (succ == dstICFG)
                    {
                        feasible = true;
                    }
                    else if (!visited.count(succ))
                    {
                        visited.insert(succ);
                        worklist.push_back(succ);
                    }
                    else
                    {
                        // Already visited.
                    }
                }

                for (const ICFGEdge* icfgEdge : cur->getOutEdges())
                {
                    const IntraCFGEdge* intraEdge =
                        SVFUtil::dyn_cast<IntraCFGEdge>(icfgEdge);
                    const ICFGNode* succ =
                        intraEdge ? intraEdge->getDstNode() : nullptr;

                    if (!intraEdge)
                    {
                        // Non-intra ICFG edges are not part of this path query.
                    }
                    else if (!succ || succ->getFun() != fun)
                    {
                        // Keep the query inside src's function.
                    }
                    else if (!isIntraEdgeBranchFeasible(intraEdge, cur))
                    {
                        // The conditional edge is unreachable in cur's state.
                    }
                    else if (succ == dstICFG)
                    {
                        feasible = true;
                    }
                    else if (!visited.count(succ))
                    {
                        visited.insert(succ);
                        worklist.push_back(succ);
                    }
                    else
                    {
                        // Already visited.
                    }
                }
            }
        }
    }

    return feasible;
}

bool FullSparseAbstractInterpretation::isICFGPathFeasible(const ICFGNode* src,
                                                          const ICFGNode* dst)
{
    bool feasible = true;
    if (!src || !dst)
    {
        feasible = true;
    }
    else if (src == dst)
    {
        feasible = true;
    }
    else
    {
        const FunObjVar* fun = src->getFun();
        if (!fun || fun != dst->getFun())
        {
            feasible = true;
        }
        else
        {
            feasible = false;
            std::deque<const ICFGNode*> worklist;
            Set<const ICFGNode*> visited;
            worklist.push_back(src);
            visited.insert(src);

            while (!worklist.empty() && !feasible)
            {
                const ICFGNode* cur = worklist.front();
                worklist.pop_front();

                // Treat a call as an intra-procedural summary edge for path
                // queries. Feasibility of the callee body is handled by the
                // normal analysis; here we only need caller-side reachability,
                // e.g. entry -> ret-site.
                if (const CallICFGNode* call =
                        SVFUtil::dyn_cast<CallICFGNode>(cur))
                {
                    const ICFGNode* succ = call->getRetICFGNode();
                    if (!succ || succ->getFun() != fun)
                    {
                        // Ignore missing or cross-function return summaries.
                    }
                    else if (succ == dst)
                    {
                        feasible = true;
                    }
                    else if (!visited.count(succ))
                    {
                        visited.insert(succ);
                        worklist.push_back(succ);
                    }
                    else
                    {
                        // Already visited.
                    }
                }

                for (const ICFGEdge* edge : cur->getOutEdges())
                {
                    const IntraCFGEdge* intraEdge =
                        SVFUtil::dyn_cast<IntraCFGEdge>(edge);
                    const ICFGNode* succ =
                        intraEdge ? intraEdge->getDstNode() : nullptr;

                    if (!intraEdge)
                    {
                        // Non-intra ICFG edges are not part of this path query.
                    }
                    else if (!succ || succ->getFun() != fun)
                    {
                        // Keep the query inside src's function.
                    }
                    else if (!isIntraEdgeBranchFeasible(intraEdge, cur))
                    {
                        // The conditional edge is unreachable in cur's state.
                    }
                    else if (succ == dst)
                    {
                        feasible = true;
                    }
                    else if (!visited.count(succ))
                    {
                        visited.insert(succ);
                        worklist.push_back(succ);
                    }
                    else
                    {
                        // Already visited.
                    }
                }
            }
        }
    }

    return feasible;
}

bool FullSparseAbstractInterpretation::isIntraEdgeBranchFeasible(
    const IntraCFGEdge* edge, const ICFGNode* src)
{
    bool feasible = true;
    if (!edge->getCondition())
    {
        feasible = true;
    }
    else if (!hasAbsState(src))
    {
        feasible = true;
    }
    else
    {
        AbstractState edgeState = getAbsState(src);
        feasible = isBranchEdgeFeasible(edge, edgeState);
    }

    return feasible;
}

void FullSparseAbstractInterpretation::recordBranchRefinement(
    NodeID objId, const IntervalValue& narrowed, AbstractState&,
    const ICFGNode*, const ICFGNode* succ)
{
    if (narrowed.isBottom())
        return;

    auto& succRef = refinementTrace[succ];
    auto rit = succRef.find(objId);
    if (rit == succRef.end())
    {
        succRef[objId] = narrowed;
    }
    else
    {
        rit->second.join_with(narrowed);
    }
}

void FullSparseAbstractInterpretation::propagateAndApplyRefinement(
    const ICFGNode* node)
{
    // e.g.
    // if (x > 0) {
    //     use(x); // use 1
    //     use(x); // use 2
    // }
    // Step 1: compose pred-inherited refinement into refinementTrace[node].
    // At use2, we don't have conditional intra-edge, but we can inherit the
    // refinement from use1's conditional edge.  When multiple preds, JOIN the
    // inherited constraints.
    Map<NodeID, IntervalValue> inherited;
    bool inheritOk = true;
    bool first = true;
    for (auto& e : node->getInEdges())
    {
        const ICFGNode* pred = e->getSrcNode();
        if (hasAbsState(pred))
        {
            auto pit = refinementTrace.find(pred);
            if (pit == refinementTrace.end())
            {
                inheritOk = false;
                break;
            }
            else if (first)
            {
                inherited = pit->second;
                first = false;
            }
            else
            {
                for (auto it = inherited.begin(); it != inherited.end();)
                {
                    auto eit = pit->second.find(it->first);
                    if (eit == pit->second.end())
                    {
                        it = inherited.erase(it);
                    }
                    else
                    {
                        it->second.join_with(eit->second);
                        ++it;
                    }
                }
            }
        }
    }
    if (inheritOk && !first && !inherited.empty())
    {
        auto& nodeRef = refinementTrace[node];
        for (const auto& [id, val] : inherited)
        {
            auto rit = nodeRef.find(id);
            if (rit == nodeRef.end())
            {
                nodeRef[id] = val;
            }
            else
            {
                rit->second.meet_with(val);
            }
        }
    }
    // e.g.
    // if (x > 0) {
    //     use(x); // use 1
    //     use(x); // use 2
    // }
    // Step 2: at use1, recordBranchRefinement captures the predState's narrowed
    // constraint into refinementTrace[use1].  At use2, we find the inherited
    // refinement from use1 and MEET it into the base value so the use observes
    // the narrowed constraint.
    auto nit = refinementTrace.find(node);
    if (nit != refinementTrace.end())
    {
        AbstractState& trace = abstractTrace[node];
        for (const auto& [id, constraint] : nit->second)
        {
            if (trace.inAddrToValTable(id))
            {
                u32_t addr = AbstractState::getVirtualMemAddress(id);
                trace.load(addr).getInterval().meet_with(constraint);
            }
        }
    }
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
