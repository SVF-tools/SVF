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

#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>

namespace SVF
{

SemiSparseAbstractInterpretation::SemiSparseAbstractInterpretation()
{
    preAnalysis->initCycleValVars();
}

const ICFGNode* SemiSparseAbstractInterpretation::getICFGNode(const ValVar* var) const
{
    if (!var->getICFGNode())
        return icfg->getGlobalICFGNode();
    if (SVFUtil::isa<CallICFGNode>(var->getICFGNode()) && SVFUtil::isa<RetValPN>(var))
        return SVFUtil::cast<CallICFGNode>(var->getICFGNode())->getRetICFGNode();
    return var->getICFGNode();
}

SVF::Interval SemiSparseAbstractInterpretation::getInterval(const ValVar* var, const ICFGNode*)
{
    return Base::getInterval(var, getICFGNode(var));
}

SVF::Interval SemiSparseAbstractInterpretation::getDefinedInterval(const ValVar* var, const ICFGNode*)
{
    return Base::getDefinedInterval(var, getICFGNode(var));
}

SVF::AddressSet SemiSparseAbstractInterpretation::getAddressSet(const ValVar* var, const ICFGNode*)
{
    return Base::getAddressSet(var, getICFGNode(var));
}

bool SemiSparseAbstractInterpretation::hasAbsValue(const ValVar* var, const ICFGNode*) const
{
    return Base::hasAbsValue(var, getICFGNode(var));
}

bool SemiSparseAbstractInterpretation::numericalValueMayBeUninitialized(const ValVar* var, const ICFGNode*) const
{
    return Base::numericalValueMayBeUninitialized(var, getICFGNode(var));
}

void SemiSparseAbstractInterpretation::updateValue(const ValVar* var,
        const SVF::Interval& interval, const SVF::AddressSet& addresses, const ICFGNode* node)
{
    Base::updateValue(var, interval, addresses, var->getICFGNode() ? var->getICFGNode() : node);
}

void SemiSparseAbstractInterpretation::addUninitializedNumericalAlternative(const ValVar* var, const ICFGNode* node)
{
    Base::addUninitializedNumericalAlternative(var, var->getICFGNode() ? var->getICFGNode() : node);
}

void SemiSparseAbstractInterpretation::forgetScalarValues(State& state) const
{
    const SVF::Variable end = adapter_.firstObjectContentVariable();
    for (SVF::Variable variable : state.initializedVariablesBefore(end))
        state.resetValue(variable);
    for (SVF::Variable variable : state.numerical().supportVariablesBefore(end))
        state.numerical().forget(variable);
    for (SVF::Variable variable : state.addresses().nonDefaultVariablesBefore(end))
        state.addresses().forget(variable);
}

SemiSparseAbstractInterpretation::State
SemiSparseAbstractInterpretation::propagatedState(const State& source) const
{
    State result = source;
    forgetScalarValues(result);
    return result;
}

void SemiSparseAbstractInterpretation::updateFlowState(const ICFGNode* node, State incoming)
{
    // SSA values remain at their definition sites when memory is replaced.
    const auto old = abstractTrace_.find(node);
    if (old != abstractTrace_.end())
    {
        const SVF::Variable end = adapter_.firstObjectContentVariable();
        std::set<SVF::Variable> scalars;
        for (SVF::Variable var : old->second.initializedVariablesBefore(end)) scalars.insert(var);
        for (SVF::Variable var : old->second.numerical().supportVariablesBefore(end)) scalars.insert(var);
        for (SVF::Variable var : old->second.addresses().nonDefaultVariablesBefore(end)) scalars.insert(var);
        for (SVF::Variable var : scalars)
            incoming.assignValueFrom(var, old->second, var);
    }
    abstractTrace_.insert_or_assign(node, std::move(incoming));
}

void SemiSparseAbstractInterpretation::copyAbstractState(const ICFGNode* source, const ICFGNode* destination)
{
    State incoming = state(source);
    forgetScalarValues(incoming);
    updateFlowState(destination, std::move(incoming));
}

bool SemiSparseAbstractInterpretation::mergeStatesFromPredecessors(const ICFGNode* node)
{
    State merged = bottomState();
    bool hasFeasiblePredecessor = false;
    for (const ICFGEdge* edge : node->getInEdges())
    {
        const ICFGNode* predecessor = edge->getSrcNode();
        if (!hasAbsState(predecessor)) continue;
        const auto* branch = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
        if (!branch && !SVFUtil::isa<CallCFGEdge>(edge) && !SVFUtil::isa<RetCFGEdge>(edge)) continue;
        if (SVFUtil::isa<RetCFGEdge>(edge) && Options::HandleRecur() != TOP &&
                !hasAbsState(SVFUtil::cast<RetICFGNode>(node)->getCallICFGNode())) continue;
        State incoming = state(predecessor);
        if (branch && branch->getCondition())
        {
            if (!isBranchEdgeFeasibleAt(branch, predecessor)) continue;
            collectBranchRefinement(branch, incoming);
        }
        merged.joinWith(propagatedState(incoming));
        hasFeasiblePredecessor = true;
    }
    if (hasFeasiblePredecessor)
        updateFlowState(node, std::move(merged));
    return hasFeasiblePredecessor;
}

std::unique_ptr<SVF::AbstractDomain>
SemiSparseAbstractInterpretation::cloneCycleHeadState(const ICFGCycleWTO* cycle)
{
    State snapshot = state(cycle->head()->getICFGNode());
    forgetScalarValues(snapshot);
    for (const ValVar* var : preAnalysis->getCycleValVars(cycle))
    {
        const ICFGNode* defSite = var->getICFGNode();
        if (!defSite || !hasAbsValue(var, defSite) || !adapter_.contains(*var)) continue;
        assignValue(snapshot, adapter_.variable(*var), getInterval(var, defSite), getAddressSet(var, defSite));
    }
    return std::make_unique<State>(std::move(snapshot));
}

void SemiSparseAbstractInterpretation::scatterCycleValues(const State& snapshot, const ICFGNode* node)
{
    for (SVF::Variable variable : snapshot.initializedVariablesBefore(adapter_.firstObjectContentVariable()))
        if (const ValVar* var = adapter_.value(variable))
            updateValue(var, snapshot.interval(variable), snapshot.addressSet(variable), node);
}

bool SemiSparseAbstractInterpretation::widenCycleState(const SVF::AbstractDomain& previous,
        const SVF::AbstractDomain& current, const ICFGCycleWTO* cycle)
{
    const bool fixpoint = Base::widenCycleState(previous, current, cycle);
    const ICFGNode* head = cycle->head()->getICFGNode();
    const State snapshot = state(head);
    scatterCycleValues(snapshot, head);
    return fixpoint;
}

bool SemiSparseAbstractInterpretation::narrowCycleState(const SVF::AbstractDomain& previous,
        const SVF::AbstractDomain& current, const ICFGCycleWTO* cycle)
{
    const bool fixpoint = Base::narrowCycleState(previous, current, cycle);
    if (!fixpoint)
    {
        const ICFGNode* head = cycle->head()->getICFGNode();
        const State snapshot = state(head);
        scatterCycleValues(snapshot, head);
    }
    return fixpoint;
}

namespace
{

bool hasRedefinitionOf(const ICFGNode* node, const IndirectSVFGEdge* edge)
{
    for (const VFGNode* valueFlowNode : node->getVFGNodes())
    {
        if (SVFUtil::isa<StoreVFGNode>(valueFlowNode) &&
                valueFlowNode->getDefSVFVars().intersects(edge->getPointsTo()))
            return true;
    }
    return false;
}

} // namespace

FullSparseAbstractInterpretation::FullSparseAbstractInterpretation()
{
    svfgBuilder_ = std::make_unique<SVFGBuilder>(true);
    svfgBuilder_->buildFullSVFG(preAnalysis->getPointerAnalysis());
}

FullSparseAbstractInterpretation::
~FullSparseAbstractInterpretation() = default;

void FullSparseAbstractInterpretation::classifyMemoryVariables() const
{
    const auto& cells = this->adapter_.memoryLayout().cells();
    for (auto cell = cells.upper_bound(classifiedMemoryLocation_);
            cell != cells.end(); ++cell)
    {
        const ObjVar* object = this->adapter_.contentObject(cell->second);
        if (object && SVFUtil::isa<GepObjVar>(object))
            fieldVariables_.emplace(cell->second, object->getId());
        if (object && !SVFUtil::isa<GepObjVar>(object))
            svfgMemoryVariables_.insert(cell->second);
        classifiedMemoryLocation_ = cell->first;
    }
}

FullSparseAbstractInterpretation::State
FullSparseAbstractInterpretation::propagatedState(const State& state) const
{
    if (state.isBottom())
        return state;
    classifyMemoryVariables();
    State result = state.withoutValues(svfgMemoryVariables_);
    forgetScalarValues(result);
    return result;
}

void FullSparseAbstractInterpretation::recordBranchRefinement(
    NodeID objectId, const SVF::Interval& narrowed, SVF::AbstractDomain&,
    const ICFGNode*, const ICFGNode* successor)
{
    if (narrowed.isBottom())
        return;
    auto& refinements = memoryRefinementTrace_[successor];
    const auto iterator = refinements.find(objectId);
    if (iterator == refinements.end())
        refinements.emplace(objectId, narrowed);
    else
        iterator->second.joinWith(narrowed);
}

void FullSparseAbstractInterpretation::storeValue(
    const ValVar* pointer, const SVF::Interval& interval,
    const SVF::AddressSet& valueAddresses, const ICFGNode* node)
{
    const SVF::AddressSet addresses = Base::getAddressSet(pointer, node);
    if (addresses.isFinite())
        for (SVF::Location location : addresses)
            if (const ObjVar* object = objectAt(location))
                memoryRefinementTrace_[node].erase(object->getId());
    Base::storeValue(pointer, interval, valueAddresses, node);
    recordMemoryDefinition(node, addresses);
}

void FullSparseAbstractInterpretation::recordMemoryDefinition(
    const ICFGNode* node, const SVF::AddressSet& targets)
{
    auto record = [&](SVF::Location location)
    {
        if (location.isNull() ||
                !this->adapter_.memoryLayout().contains(location))
            return;
        const ObjVar* object = this->objectAt(location);
        if (!object)
            return;
        // A freed-target store updated BlackHole's cell, not the original
        // object named by this MemorySSA edge. Do not invent a definition for
        // that original object merely because its payload slot is absent.
        if (this->memoryVariable(*object, this->state(node)) !=
                this->adapter_.contentVariable(*object))
        {
            const auto definitions = memoryDefinitionSupport_.find(node);
            if (definitions != memoryDefinitionSupport_.end())
            {
                definitions->second.erase(this->adapter_.contentVariable(*object));
                if (definitions->second.empty())
                    memoryDefinitionSupport_.erase(definitions);
            }
            return;
        }
        if (Base::hasAbsValue(object, node))
            return;
        memoryDefinitionSupport_[node].insert(
            this->adapter_.memoryLayout().contentOf(location));
    };

    if (targets.hasUnknownObject())
    {
        for (const auto& [location, content] :
                this->adapter_.memoryLayout().cells())
        {

            (void)content;
            record(location);
        }
    }
    else
    {
        for (SVF::Location location : targets.locations())
            record(location);
    }
}

bool FullSparseAbstractInterpretation::hasMemoryDefinition(
    const ICFGNode* node, SVF::Variable content) const
{
    const auto support = memoryDefinitionSupport_.find(node);
    return support != memoryDefinitionSupport_.end() &&
           support->second.count(content) != 0;
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    memoryRefinementTrace_.erase(node);
    memoryDefinitionSupport_.erase(node);
    if (!Base::mergeStatesFromPredecessors(node))
        return false;
    pullObjectValueFlows(node);
    propagateAndApplyMemoryRefinement(node);
    return true;
}

void FullSparseAbstractInterpretation::pullObjectValueFlows(
    const ICFGNode* node)
{
    NodeBS denseLocalObjects;
    State& destination = this->ensureState(node);
    classifyMemoryVariables();
    for (const auto& [variable, object] : fieldVariables_)
    {
        const bool stored =
            destination.numericalInitialization().value(variable) !=
            destination.numericalInitialization().defaultState() ||
            destination.addressInitialization().value(variable) !=
            destination.addressInitialization().defaultState() ||
            (!destination.numerical().isBottom() &&
             !destination.numerical().bound(variable).isTop()) ||
            (!destination.addresses().isBottom() &&
             !destination.addresses().addressSet(variable).isTop());
        if (stored)
            denseLocalObjects.set(object);
    }
    NodeBS pulledObjects;

    for (const VFGNode* valueFlowNode : node->getVFGNodes())
    {
        for (auto edgeIterator = valueFlowNode->InEdgeBegin();
                edgeIterator != valueFlowNode->InEdgeEnd(); ++edgeIterator)
        {
            const auto* indirect =
                SVFUtil::dyn_cast<IndirectSVFGEdge>(*edgeIterator);
            if (!indirect ||
                    !isIndirectSVFGEdgeFeasible(indirect, valueFlowNode))
                continue;

            const auto* sourceNode =
                SVFUtil::dyn_cast<SVFGNode>(indirect->getSrcNode());
            assert(sourceNode && sourceNode->getICFGNode() &&
                   "SVFG source must have an ICFG node");
            const ICFGNode* source = sourceNode->getICFGNode();
            if (!this->hasAbsState(source))
                continue;

            for (NodeID objectId : indirect->getPointsTo())
            {
                SVFVar* graphNode = this->svfir->getGNode(objectId);
                NodeBS objectsToPull;
                if (SVFUtil::isa<GepObjVar>(graphNode))
                    objectsToPull.set(objectId);
                else if (auto* base = SVFUtil::dyn_cast<BaseObjVar>(graphNode))
                    objectsToPull = this->svfir->getAllFieldsObjVars(base);
                else
                    objectsToPull.set(objectId);

                for (NodeID fieldId : objectsToPull)
                {
                    if (denseLocalObjects.test(fieldId))
                        continue;
                    const auto* object = SVFUtil::dyn_cast<ObjVar>(
                                             this->svfir->getGNode(fieldId));
                    if (!object)
                        continue;
                    const SVF::Variable content =
                        this->adapter_.contentVariable(*object);
                    // Model-side writes have no StoreStmt/SVFG definition.
                    // Keep their ICFG value, even when both facets are Top.
                    const bool hasDefinition = Base::hasAbsValue(object, source) ||
                                               hasMemoryDefinition(source, content);
                    if (!hasDefinition)
                        continue;

                    const State& sourceState = this->state(source);
                    const SVF::Variable destinationContent =
                        this->memoryVariable(*object, destination);
                    const SVF::Variable sourceContent =
                        this->memoryVariable(*object, sourceState);
                    if (pulledObjects.test(fieldId))
                        destination.joinValueFrom(destinationContent,
                                                  sourceState, sourceContent);
                    else
                        destination.assignValueFrom(destinationContent,
                                                    sourceState, sourceContent);
                    if (!Base::hasAbsValue(object, node) &&
                            this->memoryVariable(*object, this->state(node)) == content)
                        memoryDefinitionSupport_[node].insert(content);
                    pulledObjects.set(fieldId);
                }
            }
        }
    }
}

bool FullSparseAbstractInterpretation::isIntraEdgeBranchFeasible(
    const IntraCFGEdge* edge, const ICFGNode* source)
{
    return !edge->getCondition() || !this->hasAbsState(source) ||
           this->isBranchEdgeFeasibleAt(edge, source);
}

bool FullSparseAbstractInterpretation::isIndirectSVFGEdgeFeasible(
    const IndirectSVFGEdge* edge, const VFGNode* destination)
{
    assert(edge && destination && "SVFG edge and destination must exist");
    const auto* sourceNode = SVFUtil::dyn_cast<SVFGNode>(edge->getSrcNode());
    assert(sourceNode && "indirect SVFG edge must have an SVFG source");
    const ICFGNode* source = sourceNode->getICFGNode();
    const ICFGNode* target = destination->getICFGNode();
    assert(source && target && "SVFG endpoints must have ICFG nodes");

    const FunObjVar* function = source->getFun();
    if (!edge->isIntraVFGEdge() || source == target || !function ||
            function != target->getFun())
        return true;

    std::deque<const ICFGNode*> worklist;
    Set<const ICFGNode*> visited;
    worklist.push_back(source);
    visited.insert(source);
    while (!worklist.empty())
    {
        const ICFGNode* current = worklist.front();
        worklist.pop_front();
        if (current != source && hasRedefinitionOf(current, edge))
            continue;

        if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(current))
        {
            const ICFGNode* successor = call->getRetICFGNode();
            if (successor && successor->getFun() == function)
            {
                if (successor == target)
                    return true;
                if (visited.insert(successor).second)
                    worklist.push_back(successor);
            }
        }

        for (const ICFGEdge* cfgEdge : current->getOutEdges())
        {
            const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(cfgEdge);
            const ICFGNode* successor = intra ? intra->getDstNode() : nullptr;
            if (!successor || successor->getFun() != function ||
                    !isIntraEdgeBranchFeasible(intra, current))
                continue;
            if (successor == target)
                return true;
            if (visited.insert(successor).second)
                worklist.push_back(successor);
        }
    }
    return false;
}

void FullSparseAbstractInterpretation::propagateAndApplyMemoryRefinement(
    const ICFGNode* node)
{
    Map<NodeID, SVF::Interval> inherited;
    bool canInherit = true;
    bool first = true;
    for (const ICFGEdge* edge : node->getInEdges())
    {
        const ICFGNode* predecessor = edge->getSrcNode();
        if (!this->hasAbsState(predecessor))
            continue;
        const auto predecessorRefinement =
            memoryRefinementTrace_.find(predecessor);
        if (predecessorRefinement == memoryRefinementTrace_.end())
        {
            canInherit = false;
            break;
        }
        if (first)
        {
            inherited = predecessorRefinement->second;
            first = false;
            continue;
        }
        for (auto iterator = inherited.begin(); iterator != inherited.end();)
        {
            const auto incoming =
                predecessorRefinement->second.find(iterator->first);
            if (incoming == predecessorRefinement->second.end())
                iterator = inherited.erase(iterator);
            else
            {
                iterator->second.joinWith(incoming->second);
                ++iterator;
            }
        }
    }

    if (canInherit && !first)
    {
        auto& refinements = memoryRefinementTrace_[node];
        for (const auto& [objectId, constraint] : inherited)
        {
            const auto current = refinements.find(objectId);
            if (current == refinements.end())
                refinements.emplace(objectId, constraint);
            else
                current->second.meetWith(constraint);
        }
    }

    const auto refinements = memoryRefinementTrace_.find(node);
    if (refinements == memoryRefinementTrace_.end())
        return;
    State& denseState = this->ensureState(node);
    for (const auto& [objectId, constraint] : refinements->second)
    {
        const auto* object =
            SVFUtil::dyn_cast<ObjVar>(this->svfir->getGNode(objectId));
        if (!object)
            continue;
        const SVF::Variable content = this->adapter_.contentVariable(*object);
        if (!object->isPointer() && Base::hasAbsValue(object, node))
            this->constrainInterval(denseState, content, constraint);
    }
}

} // namespace SVF
