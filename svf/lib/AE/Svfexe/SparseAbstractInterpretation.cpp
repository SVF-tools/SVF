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
//  ObjVar values remain at their memory definitions and are retrieved along
//  SVFG indirect edges. joinStates carries only _freedAddrs, which has no
//  MemorySSA representation.
// =====================================================================

void FullSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src)
{
    // GepObjVar values do not ride ICFG edges: copying every field into
    // every node recreates the dense O(program points x fields) state.
    for (NodeID a : src.getFreedAddrs())
        dst.addToFreedAddrs(a);
}

void FullSparseAbstractInterpretation::updateAbsValue(const ObjVar* var,
                                                      const AbstractValue& val,
                                                      const ICFGNode* node)
{
    auto refinementIt = refinementTrace.find(node);
    if (refinementIt != refinementTrace.end())
        refinementIt->second.erase(var->getId());

    AbstractInterpretation::updateAbsValue(var, val, node);
    if (SVFUtil::isa<GepObjVar>(var))
        subObjectDefinitions[var->getId()].insert(node);
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
    if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        const NodeBS callSubObjects = collectCallArgumentSubObjects(call);
        pullReachingSubObjectValues(callSubObjects, call);
    }

    for (const VFGNode* valueFlow : node->getVFGNodes())
    {
        const NodeBS loadSubObjects = resolveLoadSubObjects(valueFlow, node);
        for (const VFGEdge* incoming : valueFlow->getInEdges())
        {
            const IndirectSVFGEdge* indirect =
                SVFUtil::dyn_cast<IndirectSVFGEdge>(incoming);
            if (indirect)
                pullValuesFromIndirectEdge(indirect, valueFlow, node,
                                           loadSubObjects);
        }
    }
}

NodeBS FullSparseAbstractInterpretation::resolveLoadSubObjects(
    const VFGNode* valueFlow, const ICFGNode* use)
{
    NodeBS subObjects;
    if (const LoadVFGNode* load = SVFUtil::dyn_cast<LoadVFGNode>(valueFlow))
    {
        const AbstractValue& pointer = getAbsValue(load->getRHSVar(), use);
        if (pointer.isAddr())
        {
            const AbstractState& state = getAbsState(use);
            for (u32_t address : pointer.getAddrs())
            {
                if (AbstractState::isNullOrBlackHoleAddr(address))
                    continue;

                const NodeID objectId = state.getIDFromAddr(address);
                if (SVFUtil::isa<GepObjVar>(svfir->getGNode(objectId)))
                    subObjects.set(objectId);
            }
        }
    }
    return subObjects;
}

NodeBS FullSparseAbstractInterpretation::collectDefinedSubObjects(
    NodeID baseObjectId) const
{
    NodeBS subObjects;
    for (const auto& definitions : subObjectDefinitions)
    {
        const NodeID subObjectId = definitions.first;
        const GepObjVar* subObject =
            SVFUtil::dyn_cast<GepObjVar>(svfir->getGNode(subObjectId));
        if (subObject && subObject->getBaseNode() == baseObjectId)
            subObjects.set(subObjectId);
    }
    return subObjects;
}

NodeBS FullSparseAbstractInterpretation::collectCallArgumentSubObjects(
    const CallICFGNode* call)
{
    NodeBS subObjects;
    const AbstractState& state = getAbsState(call);
    for (u32_t index = 0; index < call->arg_size(); ++index)
    {
        const AbstractValue& argument =
            getAbsValue(call->getArgument(index), call);
        if (!argument.isAddr())
            continue;

        for (u32_t address : argument.getAddrs())
        {
            if (AbstractState::isNullOrBlackHoleAddr(address))
                continue;

            const NodeID objectId = state.getIDFromAddr(address);
            const GepObjVar* addressedSubObject =
                SVFUtil::dyn_cast<GepObjVar>(svfir->getGNode(objectId));
            const NodeID baseObjectId = addressedSubObject
                                            ? addressedSubObject->getBaseNode()
                                            : objectId;
            const NodeBS definedSubObjects =
                collectDefinedSubObjects(baseObjectId);
            for (NodeID subObjectId : definedSubObjects)
                subObjects.set(subObjectId);
        }
    }
    return subObjects;
}

void FullSparseAbstractInterpretation::pullValuesFromIndirectEdge(
    const IndirectSVFGEdge* edge, const VFGNode* destination,
    const ICFGNode* use, const NodeBS& loadSubObjects)
{
    const SVFGNode* source = SVFUtil::dyn_cast<SVFGNode>(edge->getSrcNode());
    assert(source && "SVFG incoming edge must have a source node");
    assert(destination && "SVFG incoming edge must have a destination node");

    const ICFGNode* sourceICFG = source->getICFGNode();
    assert(sourceICFG && "SVFG source node must have an ICFG node");
    assert(destination->getICFGNode() &&
           "SVFG destination node must have an ICFG node");
    if (hasAbsState(sourceICFG))
    {
        NodeBS objectsToPull = edge->getPointsTo();
        for (NodeID subObjectId : loadSubObjects)
            objectsToPull.set(subObjectId);

        for (NodeID objectId : edge->getPointsTo())
        {
            const BaseObjVar* baseObject =
                SVFUtil::dyn_cast<BaseObjVar>(svfir->getGNode(objectId));
            if (!baseObject)
                continue;

            const NodeBS definedSubObjects =
                collectDefinedSubObjects(baseObject->getId());
            for (NodeID subObjectId : definedSubObjects)
                objectsToPull.set(subObjectId);
        }

        const NodeBS resolvedSubObjects =
            pullReachingSubObjectValues(objectsToPull, use);
        const bool edgeIsFeasible =
            isIndirectSVFGEdgeFeasible(edge, destination);

        for (NodeID objectId : objectsToPull)
        {
            if (resolvedSubObjects.test(objectId))
                continue;

            const ObjVar* object =
                SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(objectId));
            const bool hasSourceValue =
                object && SemiSparseAbstractInterpretation::hasAbsValue(
                              object, sourceICFG);
            if (!edgeIsFeasible || !hasSourceValue)
                continue;

            AbstractValue reachingValue;
            if (SemiSparseAbstractInterpretation::hasAbsValue(object, use))
                reachingValue =
                    SemiSparseAbstractInterpretation::getAbsValue(object, use);
            reachingValue.join_with(
                SemiSparseAbstractInterpretation::getAbsValue(object,
                                                              sourceICFG));
            SemiSparseAbstractInterpretation::updateAbsValue(
                object, reachingValue, use);
        }
    }
}

NodeBS FullSparseAbstractInterpretation::pullReachingSubObjectValues(
    const NodeBS& subObjectIds, const ICFGNode* use)
{
    NodeBS resolvedSubObjects;
    for (NodeID subObjectId : subObjectIds)
    {
        auto definitionsIt = subObjectDefinitions.find(subObjectId);
        if (definitionsIt == subObjectDefinitions.end())
            continue;

        const ObjVar* subObject =
            SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(subObjectId));
        if (!subObject)
            continue;

        AbstractValue reachingValue;
        bool foundDefinition = false;
        for (const ICFGNode* definition : definitionsIt->second)
        {
            if (!SemiSparseAbstractInterpretation::hasAbsValue(subObject,
                                                               definition) ||
                !doesSubObjectDefinitionReach(definition, use, subObjectId))
                continue;

            reachingValue.join_with(
                SemiSparseAbstractInterpretation::getAbsValue(subObject,
                                                              definition));
            foundDefinition = true;
        }

        if (foundDefinition)
        {
            SemiSparseAbstractInterpretation::updateAbsValue(
                subObject, reachingValue, use);
            resolvedSubObjects.set(subObjectId);
        }
    }
    return resolvedSubObjects;
}

// =====================================================================
//  Full-sparse — refinement trace machinery.
// =====================================================================

bool FullSparseAbstractInterpretation::redefinesIndirectEdgeObject(
    const ICFGNode* node, const IndirectSVFGEdge* edge) const
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

    const FunObjVar* function = srcICFG->getFun();
    const bool requiresIntraProceduralSearch =
        srcICFG != dstICFG && function && function == dstICFG->getFun();
    if (!requiresIntraProceduralSearch)
        return true;

    std::deque<const ICFGNode*> worklist{srcICFG};
    Set<const ICFGNode*> visited{srcICFG};
    while (!worklist.empty())
    {
        const ICFGNode* current = worklist.front();
        worklist.pop_front();

        const bool valueIsKilled =
            current != srcICFG && redefinesIndirectEdgeObject(current, edge);
        if (valueIsKilled)
            continue;

        for (const ICFGNode* successor :
             collectFeasibleIntraSuccessors(current, function))
        {
            if (successor == dstICFG)
                return true;
            if (visited.insert(successor).second)
                worklist.push_back(successor);
        }
    }
    return false;
}

bool FullSparseAbstractInterpretation::doesSubObjectDefinitionReach(
    const ICFGNode* definition, const ICFGNode* use, NodeID subObjectId)
{
    const FunObjVar* function = definition ? definition->getFun() : nullptr;
    const bool requiresIntraProceduralSearch =
        definition && use && definition != use && function &&
        function == use->getFun();
    if (!requiresIntraProceduralSearch)
        return true;

    const auto definitionsIt = subObjectDefinitions.find(subObjectId);
    assert(definitionsIt != subObjectDefinitions.end() &&
           "A queried sub-object must have a recorded definition");

    std::deque<const ICFGNode*> worklist{definition};
    Set<const ICFGNode*> visited{definition};
    while (!worklist.empty())
    {
        const ICFGNode* current = worklist.front();
        worklist.pop_front();

        const bool definitionIsKilled =
            current != definition && definitionsIt->second.count(current);
        if (definitionIsKilled)
            continue;

        for (const ICFGNode* successor :
             collectFeasibleIntraSuccessors(current, function))
        {
            if (successor == use)
                return true;
            if (visited.insert(successor).second)
                worklist.push_back(successor);
        }
    }
    return false;
}

std::vector<const ICFGNode*> FullSparseAbstractInterpretation::
    collectFeasibleIntraSuccessors(const ICFGNode* node,
                                   const FunObjVar* function)
{
    std::vector<const ICFGNode*> successors;

    // Calls act as caller-side summary edges. Callee feasibility is handled by
    // normal abstract execution; this path query continues at the return site.
    if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        const ICFGNode* returnSite = call->getRetICFGNode();
        if (returnSite && returnSite->getFun() == function)
            successors.push_back(returnSite);
    }

    for (const ICFGEdge* edge : node->getOutEdges())
    {
        const IntraCFGEdge* intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
        if (!intraEdge)
            continue;

        const ICFGNode* successor = intraEdge->getDstNode();
        const bool staysInFunction =
            successor && successor->getFun() == function;
        if (staysInFunction && isIntraEdgeBranchFeasible(intraEdge, node))
            successors.push_back(successor);
    }
    return successors;
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
