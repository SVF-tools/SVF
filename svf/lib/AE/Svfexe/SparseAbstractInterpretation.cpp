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
    buildObjDefSiteIndex();
}

void FullSparseAbstractInterpretation::buildObjDefSiteIndex()
{
    AndersenWaveDiff* pta = preAnalysis->getPointerAnalysis();
    for (auto& it : *svfg)
    {
        const auto* sn = SVFUtil::dyn_cast<StoreVFGNode>(it.second);
        if (!sn) continue;
        const ICFGNode* icfg = sn->getICFGNode();
        if (!icfg) continue;
        NodeID dst = sn->getLHSVar()->getId();
        for (NodeID o : pta->getPts(dst))
        {
            objDefSites[o].insert(icfg);
            // Andersen's static pts may give either the base obj or a
            // (different) Gep id under that base — AE resolves the
            // runtime gep at read time via getGepObjAddrs and may pick
            // a sibling that wasn't in pts.  Register the store under
            // every sibling field of the same base so subsequent reads
            // find a writer regardless of which Gep id AE picks.
            const BaseObjVar* base = nullptr;
            const SVFVar* sv = svfir->getGNode(o);
            if (auto* b = SVFUtil::dyn_cast<BaseObjVar>(sv))
                base = b;
            else if (auto* g = SVFUtil::dyn_cast<GepObjVar>(sv))
                base = g->getBaseObj();
            if (base)
                for (NodeID fid : svfir->getAllFieldsObjVars(base))
                    objDefSites[fid].insert(icfg);
        }
    }
}

void FullSparseAbstractInterpretation::recordObjWrite(
    NodeID oid, const ICFGNode* node)
{
    if (!node) return;
    objDefSites[oid].insert(node);
    // Extapi handlers (memcpy/memset/strcpy/...) typically bulk-write
    // a whole base region — the gep id Andersen produces here may not
    // exactly match the gep id AE resolves at a downstream load
    // (interval-aware gep can land on a sibling).  Register the same
    // def-site under every sibling field of the same base, so reads
    // hit a writer regardless of which specific Gep id AE picks.
    const auto* gep = SVFUtil::dyn_cast<GepObjVar>(svfir->getGNode(oid));
    if (!gep) return;
    const auto* base = gep->getBaseObj();
    if (!base) return;
    for (NodeID fid : svfir->getAllFieldsObjVars(base))
        objDefSites[fid].insert(node);
}

const AbstractValue& FullSparseAbstractInterpretation::getAbsValue(
    const ObjVar* var, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    NodeID oid = var->getId();
    u32_t addr = AbstractState::getVirtualMemAddress(oid);

    // Base / Dummy 走 inherited 路径 (Step 1 SVFG-pull populated trace
    // with MSSA-kill-respecting reaching-def).  Gep 也先看 trace —— Step
    // 1 / handleNode 已经写进去的话, 直接用; trace 缺位才查 objDefSites.
    const auto* gep = SVFUtil::dyn_cast<GepObjVar>(var);
    // Base / Dummy 走 inherited (Step 1 SVFG-pull populated, with MSSA kill).
    if (!gep)
        return SemiSparseAbstractInterpretation::getAbsValue(var, node);

    // Gep:对所有 def-sites 算 JOIN, 并和 trace[node][gep] (Step 1 SVFG-pull
    // 的结果, 带 kill) JOIN 在一起.  Gep 字段间没有 kill 关系, JOIN 多写者
    // 等于 path-conservative widening, sound;  与 Step 1 的 path-aware 值再
    // JOIN 一次只会扩大区间, 同样 sound. 这样兼顾 (a) Step 1 拉到的精度,
    // (b) MSSA 漏配 chi 时静态 def-site 兜底.
    AbstractValue acc;
    if (as.inAddrToValTable(oid))
        acc.join_with(SemiSparseAbstractInterpretation::getAbsValue(var, node));

    auto it = objDefSites.find(oid);
    if (it != objDefSites.end())
    {
        for (const ICFGNode* def : it->second)
        {
            if (!hasAbsState(def)) continue;
            if (!SemiSparseAbstractInterpretation::hasAbsValue(gep, def))
                continue;
            acc.join_with(
                SemiSparseAbstractInterpretation::getAbsValue(gep, def));
        }
    }
    else
    {
        // Gep 没自己 def-site, 退到 base def-sites.
        NodeID baseId = gep->getBaseObj()->getId();
        auto bit = objDefSites.find(baseId);
        if (bit != objDefSites.end())
        {
            const auto* base = gep->getBaseObj();
            for (const ICFGNode* def : bit->second)
            {
                if (!hasAbsState(def)) continue;
                if (!SemiSparseAbstractInterpretation::hasAbsValue(base, def))
                    continue;
                acc.join_with(
                    SemiSparseAbstractInterpretation::getAbsValue(base, def));
            }
        }
    }
    // Re-apply branch-refinement constraint after the def-site JOIN, so
    // path-sensitive narrowings (if (gep > 5)) survive a JOIN-with-all-
    // writers that would otherwise widen them back to TOP.
    auto rit = refinementTrace.find(node);
    if (rit != refinementTrace.end())
    {
        auto cit = rit->second.find(oid);
        if (cit != rit->second.end())
            acc.getInterval().meet_with(cit->second);
    }
    as.store(addr, acc);
    return as.load(addr);
}

// =====================================================================
//  Full-sparse — merge.
//
//  mergeStatesFromPredecessors is a thin wrapper: defer to base for
//  ICFG-edge bookkeeping (predecessor iteration, branch feasibility,
//  joinStates, updateAbsState, reachability return).  When base reports
//  a feasible predecessor, run pullValueFlow to populate trace[node]
//  with obj values pulled along SVFG indirect in-edges.
//
//  joinStates stays a near-no-op (only _freedAddrs rides ICFG edges,
//  matching semi-sparse minus the obj loop) since value flow is
//  handled by pullValueFlow, not by ICFG-edge joins.
// =====================================================================

void FullSparseAbstractInterpretation::joinStates(
    AbstractState& dst, const AbstractState& src)
{
    // ObjVar 不沿 ICFG 边累积. Base/Dummy 由 pullValueFlow Step 1 (SVFG
    // indirect, 带 MSSA kill) 在 use 处补值; Gep 由 getAbsValue(ObjVar*)
    // 在被读到时 lazy materialize 进 trace. 只有 _freedAddrs 沿 ICFG 边传
    // (没有 SVFG 编码) 给 null-deref detector 用.
    for (NodeID a : src.getFreedAddrs())
        dst.addToFreedAddrs(a);
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    // Reset refinement at this node — base merge below will repopulate
    // via our recordBranchNarrowing override (per conditional in-edge).
    refinementTrace.erase(node);

    if (!AbstractInterpretation::mergeStatesFromPredecessors(node))
        return false;

    pullValueFlow(node);

    // Compose pred-inherited refinement on top of branch narrowings
    // just captured, then MEET into trace[node] so reads see narrowed.
    propagateAndApplyRefinement(node);
    return true;
}

void FullSparseAbstractInterpretation::pullValueFlow(const ICFGNode* node)
{
    // e.g.
    //     store i32 7, i32* %p   ; def-site D for obj_p
    //     ...
    //     %v = load i32, i32* %p ; use-site U
    // Step 1: intra-node SVFG-pull.  For each VFG node hosted at U, walk
    // the indirect SVFG in-edges back to D; for every obj id labelling
    // the edge, JOIN the obj's value at D into U's trace.  When the
    // edge's PTS carries a base obj id (Andersen is field-insensitive),
    // expand to every sibling field via getAllFieldsObjVars — MSSA may
    // have stamped only one field id on the edge even though the writer
    // touched several siblings, and this expansion lets those sibling
    // stores reach the consumer.  Reads/writes go through SemiSparse to
    // bypass FullSparse's refinement layer (these are def-site pulls,
    // not real stores; refinement is applied later in Step 2 of
    // propagateAndApplyRefinement).
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
                const ICFGNode* srcICFG = src ? src->getICFGNode() : nullptr;
                if (srcICFG && hasAbsState(srcICFG))
                {
                    for (NodeID id : indEdge->getPointsTo())
                    {
                        SVFVar* gn = svfir->getGNode(id);
                        const BaseObjVar* base = nullptr;
                        if (auto* b = SVFUtil::dyn_cast<BaseObjVar>(gn))
                        {
                            base = b;
                        }
                        else if (auto* g = SVFUtil::dyn_cast<GepObjVar>(gn))
                        {
                            base = g->getBaseObj();
                        }

                        NodeBS singleton;
                        if (!base) singleton.set(id);
                        const NodeBS& fids =
                            base ? svfir->getAllFieldsObjVars(base) : singleton;

                        for (NodeID fid : fids)
                        {
                            const ObjVar* obj =
                                SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(fid));
                            if (obj
                                && SemiSparseAbstractInterpretation::hasAbsValue(obj, srcICFG))
                            {
                                AbstractValue cur;
                                if (SemiSparseAbstractInterpretation::hasAbsValue(obj, node))
                                {
                                    cur = SemiSparseAbstractInterpretation::getAbsValue(obj, node);
                                }
                                cur.join_with(
                                    SemiSparseAbstractInterpretation::getAbsValue(obj, srcICFG));
                                SemiSparseAbstractInterpretation::updateAbsValue(obj, cur, node);
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

void FullSparseAbstractInterpretation::recordBranchNarrowing(
    NodeID objId, const IntervalValue& narrowed, AbstractState& /*as*/,
    const ICFGNode* /*loadIcfg*/, const ICFGNode* succ)
{
    // FullSparse: route narrowing into refinementTrace[succ] instead
    // of writing into `as` (which joinStates would discard).
    // Multiple conditional in-edges to `succ` may call us in turn —
    // JOIN narrowings across edges (path-aware merge).
    if (!narrowed.isBottom())
    {
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
    // At use2, we don't have conditional intra-edge, but we can inherit the refinement from use1's conditional edge.  When multiple preds, JOIN the inherited constraints.
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
                for (auto it = inherited.begin(); it != inherited.end(); )
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
    // Step 2: at use1, recordBranchNarrowing captures the predState's narrowed constraint into refinementTrace[use1].  At use2, we find the inherited refinement from use1 and MEET it into the base value so the use observes the narrowed constraint.
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
