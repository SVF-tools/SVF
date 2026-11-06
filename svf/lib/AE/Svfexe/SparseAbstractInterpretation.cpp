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
#include "Graphs/SparseObjGraph.h"
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

FullSparseAbstractInterpretation::FullSparseAbstractInterpretation()
{
    svfgBuilder_ = std::make_unique<SVFGBuilder>(true);
    svfgBuilder_->buildFullSVFG(preAnalysis->getPointerAnalysis());
    objRoutes_ = std::make_unique<SparseObjGraph>(
        *svfgBuilder_->getSVFG(), *icfg, *preAnalysis->getPointerAnalysis(),
        *svfir);
}

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation() = default;

FullSparseAbstractInterpretation::State FullSparseAbstractInterpretation::
    propagatedState(const State& source) const
{
    const auto& cells = adapter_.memoryLayout().cells();
    for (auto it = cells.upper_bound(classifiedMemoryLocation_);
         it != cells.end(); ++it)
    {
        memoryVariables_.insert(it->second);
        classifiedMemoryLocation_ = it->first;
    }
    State result = source.withoutValues(memoryVariables_);
    forgetScalarValues(result);
    return result;
}

void FullSparseAbstractInterpretation::storeValue(const ValVar* pointer,
                                                  const Interval& interval,
                                                  const AddressSet& addresses,
                                                  const ICFGNode* node)
{
    const AddressSet targets = Base::getAddressSet(pointer, node);
    Base::storeValue(pointer, interval, addresses, node);
    auto record = [&](Location location) {
        const ObjVar* object = objectAt(location);
        if (object && memoryVariable(*object, state(node)) ==
                          adapter_.contentVariable(*object))
            memoryDefinitionSupport_[node].insert(
                adapter_.contentVariable(*object));
    };
    if (targets.hasUnknownObject())
        for (const auto& [location, variable] : adapter_.memoryLayout().cells())
        {
            (void)variable;
            record(location);
        }
    else
        for (Location location : targets)
            record(location);
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    std::optional<State> previous;
    const auto previousSupport = memoryDefinitionSupport_[node];
    const auto& edges = objRoutes_->getInputs(node);
    for (const auto* edge : edges)
        if (edge->getSrcNode()->getICFGNode() == node && hasAbsState(node))
        {
            previous = state(node);
            break;
        }
    memoryDefinitionSupport_.erase(node);
    if (!Base::mergeStatesFromPredecessors(node))
        return false;
    State& merged = ensureState(node);
    NodeBS pulled;
    for (const auto* edge : edges)
    {
        const auto* sourceNode = edge->getSrcNode();
        const ICFGNode* source = sourceNode->getICFGNode();
        const State* input =
            source == node ? (previous ? &*previous : nullptr)
                           : (hasAbsState(source) ? &state(source) : nullptr);
        if (!input)
            continue;
        State refined = *input;
        if (sourceNode->branch)
        {
            if (!isBranchEdgeFeasibleAt(sourceNode->branch, source))
                continue;
            collectBranchRefinement(sourceNode->branch, refined);
        }
        const auto& support =
            source == node ? previousSupport : memoryDefinitionSupport_[source];
        for (NodeID id :
             svfir->getAllFieldsObjVars(svfir->getBaseObject(sourceNode->base)))
        {
            const auto* object =
                SVFUtil::dyn_cast<ObjVar>(svfir->getSVFVar(id));
            if (!object)
                continue;
            const Variable content = adapter_.contentVariable(*object);
            if (!input->hasValue(content) && !support.count(content))
                continue;
            const Variable from = memoryVariable(*object, refined);
            const Variable to = memoryVariable(*object, merged);
            if (pulled.test(id))
                merged.joinValueFrom(to, refined, from);
            else
                merged.assignValueFrom(to, refined, from);
            pulled.set(id);
            memoryDefinitionSupport_[node].insert(content);
        }
    }
    return true;
}

} // namespace SVF
