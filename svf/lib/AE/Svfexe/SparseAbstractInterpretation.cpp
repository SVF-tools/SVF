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
// FullSparse-specific propagation lives below alongside the rest of the
// subclass so its state and value-flow policies remain visible together.

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
//  Full-sparse — SVFG-driven object propagation.
//
//  The authoritative AbstractState remains keyed by ICFGNode.  Once an ICFG
//  node has executed, propagateOutgoingObjValues collects each relevant base
//  object and its sub-objects into one Obj-value map. Native SVFG edges and
//  supplementary base-object routes use the same representation. A
//  destination ICFG node joins its incoming Obj values before executing. Only
//  _freedAddrs, which has no MemorySSA representation, still travels on ICFG
//  edges.
// =====================================================================

void FullSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src)
{
    for (NodeID a : src.getFreedAddrs())
        dst.addToFreedAddrs(a);
}

void FullSparseAbstractInterpretation::updateAbsValue(
    const ObjVar* var, const AbstractValue& val, const ICFGNode* node)
{
    auto refinement = refinementTrace.find(node);
    if (refinement != refinementTrace.end())
        refinement->second.erase(var->getId());

    AbstractInterpretation::updateAbsValue(var, val, node);
    if (SVFUtil::isa<GepObjVar>(var))
    {
        NodeID baseObj = svfir->getBaseObject(var->getId())->getId();
        propagateBaseObjValuesToUses(node, baseObj);
    }
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    refinementTrace.erase(node);

    const bool reachable =
        AbstractInterpretation::mergeStatesFromPredecessors(node);
    if (reachable)
    {
        mergeIncomingObjValues(node);

        // Compose pred-inherited refinement on top of branch narrowings
        // just captured, then MEET into trace[node] so reads see narrowed.
        propagateAndApplyRefinement(node);
    }
    return reachable;
}

FullSparseAbstractInterpretation::ObjValueMap
FullSparseAbstractInterpretation::collectBaseObjValues(
    const ICFGNode* source, NodeID baseObj)
{
    const BaseObjVar* baseObjVar = svfir->getBaseObject(baseObj);
    ObjValueMap objValues;

    if (AbstractInterpretation::hasAbsValue(baseObjVar, source))
        objValues[baseObjVar->getId()] =
            AbstractInterpretation::getAbsValue(baseObjVar, source);

    for (NodeID objId : svfir->getAllFieldsObjVars(baseObjVar))
    {
        const ObjVar* obj =
            SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(objId));
        if (obj && AbstractInterpretation::hasAbsValue(obj, source))
            objValues[objId] =
                AbstractInterpretation::getAbsValue(obj, source);
    }
    return objValues;
}

void FullSparseAbstractInterpretation::writeObjValuesToTarget(
    const ICFGNode* source, const ICFGNode* target, NodeID baseObj)
{
    // Both SVFG endpoints already read the same state at one ICFG node. A
    // self-transfer is therefore redundant and can preserve a stale widened
    // MemoryPhi value while narrowing tries to recompute that state.
    if (source != target)
    {
        ObjValueOrigin origin{source, baseObj};
        incomingObjValues[target][origin] =
            collectBaseObjValues(source, baseObj);
    }
}

void FullSparseAbstractInterpretation::propagateBaseObjValuesToUses(
    const ICFGNode* source, NodeID baseObj)
{
    for (auto it = svfg->begin(); it != svfg->end(); ++it)
    {
        const VFGNode* target = it->second;
        const ICFGNode* targetIcfg = target->getICFGNode();
        bool usesBaseObj = false;

        for (auto eit = target->InEdgeBegin();
                eit != target->InEdgeEnd() && !usesBaseObj; ++eit)
        {
            const IndirectSVFGEdge* edge =
                SVFUtil::dyn_cast<IndirectSVFGEdge>(*eit);
            if (edge)
            {
                for (NodeID objId : edge->getPointsTo())
                {
                    if (SVFUtil::isa<ObjVar>(svfir->getGNode(objId)) &&
                            svfir->getBaseObject(objId)->getId() == baseObj)
                    {
                        usesBaseObj = true;
                        break;
                    }
                }
            }
        }

        const CallICFGNode* call =
            SVFUtil::dyn_cast<CallICFGNode>(targetIcfg);
        if (!usesBaseObj && call && SVFUtil::isExtCall(call))
        {
            for (u32_t index = 0;
                    index < call->arg_size() && !usesBaseObj; ++index)
            {
                const ValVar* argument = call->getArgument(index);
                if (argument->getType()->isPointerTy())
                {
                    for (NodeID objId : preAnalysis->getPointerAnalysis()
                            ->getPts(argument->getId()))
                    {
                        if (SVFUtil::isa<ObjVar>(svfir->getGNode(objId)) &&
                                svfir->getBaseObject(objId)->getId() == baseObj)
                        {
                            usesBaseObj = true;
                            break;
                        }
                    }
                }
            }
        }

        if (usesBaseObj)
            writeObjValuesToTarget(source, targetIcfg, baseObj);
    }
}

void FullSparseAbstractInterpretation::propagateOutgoingObjValues(
    const ICFGNode* node)
{
    // Native routes. Example:
    //   LLVM IR: store i32 10, ptr %a9
    //   SVFG:    Store(a) --indirect {a}--> Load(a)
    // The edge selects target=Load and base=a. collectBaseObjValues adds every
    // currently available member of a, including the precise a.9 value.
    for (const VFGNode* source : node->getVFGNodes())
    {
        for (auto eit = source->OutEdgeBegin();
                eit != source->OutEdgeEnd(); ++eit)
        {
            const IndirectSVFGEdge* edge =
                SVFUtil::dyn_cast<IndirectSVFGEdge>(*eit);
            if (edge)
            {
                const ICFGNode* target = edge->getDstNode()->getICFGNode();
                for (NodeID objId : edge->getPointsTo())
                {
                    if (SVFUtil::isa<ObjVar>(svfir->getGNode(objId)))
                    {
                        NodeID baseObj =
                            svfir->getBaseObject(objId)->getId();
                        writeObjValuesToTarget(node, target, baseObj);
                    }
                }
            }
        }
    }
}

void FullSparseAbstractInterpretation::mergeIncomingObjValues(
    const ICFGNode* node)
{
    // Each entry is one incoming source/base pair. Keep every source and JOIN
    // colliding object values; there is no ICFG path or overwrite filter.
    AbstractState& targetState = abstractTrace[node];
    auto incoming = incomingObjValues.find(node);
    if (incoming != incomingObjValues.end())
    {
        for (const auto& incomingValues : incoming->second)
        {
            const ObjValueMap& objValues = incomingValues.second;
            for (const auto& [objId, value] : objValues)
            {
                u32_t address = AbstractState::getVirtualMemAddress(objId);
                if (targetState.getLocToVal().count(objId))
                    targetState.load(address).join_with(value);
                else
                    targetState.store(address, value);
            }
        }
    }
}

// =====================================================================
//  Full-sparse — refinement trace machinery.
// =====================================================================

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

bool FullSparseAbstractInterpretation::widenCycleState(
    const AbstractState& prev, const AbstractState& cur,
    const ICFGCycleWTO* cycle)
{
    bool fixpoint =
        SemiSparseAbstractInterpretation::widenCycleState(prev, cur, cycle);
    propagateOutgoingObjValues(cycle->head()->getICFGNode());
    return fixpoint;
}

bool FullSparseAbstractInterpretation::narrowCycleState(
    const AbstractState& prev, const AbstractState& cur,
    const ICFGCycleWTO* cycle)
{
    bool fixpoint =
        SemiSparseAbstractInterpretation::narrowCycleState(prev, cur, cycle);
    const ICFGNode* cycleHead = cycle->head()->getICFGNode();

    if (!fixpoint)
        propagateOutgoingObjValues(cycleHead);
    return fixpoint;
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
