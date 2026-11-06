//===- SparseAbstractInterpretation.h -- Sparse Abstract Execution------//
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

#ifndef SVF_AE_SPARSE_ABSTRACT_INTERPRETATION_H
#define SVF_AE_SPARSE_ABSTRACT_INTERPRETATION_H

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/SparseObjGraph.h"

namespace SVF
{

class IndirectSVFGEdge;
class SVFGBuilder;
class VFGNode;

/// Sparse scalar values are stored and read at their defining ICFG nodes.
class SemiSparseAbstractInterpretation : public AbstractInterpretation
{
public:
    using Base = AbstractInterpretation;
    using State = Base::State;
    SemiSparseAbstractInterpretation();

protected:
    const ICFGNode* getICFGNode(const ValVar* var) const;
    SVF::Interval getInterval(const ValVar*, const ICFGNode*) override;
    SVF::Interval getDefinedInterval(const ValVar*, const ICFGNode*) override;
    SVF::AddressSet getAddressSet(const ValVar*, const ICFGNode*) override;
    bool hasAbsValue(const ValVar*, const ICFGNode*) const override;
    bool numericalValueMayBeUninitialized(const ValVar*, const ICFGNode*) const override;
    void updateValue(const ValVar*, const SVF::Interval&, const SVF::AddressSet&, const ICFGNode*) override;
    void addUninitializedNumericalAlternative(const ValVar*, const ICFGNode*) override;
    using Base::getInterval;
    using Base::getAddressSet;
    using Base::hasAbsValue;
    using Base::updateValue;
    void copyAbstractState(const ICFGNode*, const ICFGNode*) override;
    bool mergeStatesFromPredecessors(const ICFGNode*) override;
    std::unique_ptr<SVF::AbstractDomain> cloneCycleHeadState(const ICFGCycleWTO*) override;
    bool widenCycleState(const SVF::AbstractDomain&, const SVF::AbstractDomain&, const ICFGCycleWTO*) override;
    bool narrowCycleState(const SVF::AbstractDomain&, const SVF::AbstractDomain&, const ICFGCycleWTO*) override;
    virtual State propagatedState(const State& state) const;
    void forgetScalarValues(State& state) const;
    void updateFlowState(const ICFGNode*, State state);
    void scatterCycleValues(const State&, const ICFGNode*);
};

class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    using Base = SemiSparseAbstractInterpretation;
    using State = typename Base::State;
    FullSparseAbstractInterpretation();
    ~FullSparseAbstractInterpretation() override;
protected:
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;
    State propagatedState(const State& source) const override;
    void storeValue(const ValVar* pointer, const Interval& interval,
                    const AddressSet& addresses, const ICFGNode* node) override;

private:
    std::unique_ptr<SVFGBuilder> svfgBuilder_;
    std::unique_ptr<SparseObjGraph> objRoutes_;
    mutable std::set<Variable> memoryVariables_;
    mutable Location classifiedMemoryLocation_;
    Map<const ICFGNode*, std::set<Variable>> memoryDefinitionSupport_;
};

} // namespace SVF

#endif // SVF_AE_SPARSE_ABSTRACT_INTERPRETATION_H
