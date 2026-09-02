//===- DenseAbstractInterpretation.h -- Domain-backed dense AE -*- C++ -*-===//

#ifndef SVF_AE_DENSE_ABSTRACT_INTERPRETATION_H
#define SVF_AE_DENSE_ABSTRACT_INTERPRETATION_H

#include "AE/Core/BoxDomain.h"
#include "AE/Core/BoxProgramState.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "AE/Svfexe/SVFIRAdapter.h"

namespace SVF
{

/// Native dense AE storage backed by one complete AbstractDomain state per
/// ICFG node. Values, memory, lifetimes, definedness, joins, widening, and
/// fixpoint checks all operate on BoxProgramState; no compatibility trace
/// is maintained by this implementation.
template <typename NumericalStateT>
class DenseAbstractInterpretation : public AbstractInterpretation
{
public:
    using DenseState = AbstractDomain::BoxProgramState;

    DenseAbstractInterpretation();
    ~DenseAbstractInterpretation() override = default;
    void runOnModule() override;

    const AbstractDomain::AbstractState& getAbstractState(
        const ICFGNode* node) const override;
    bool hasAbsState(const ICFGNode* node) const override;

    AbstractValue getAbsValue(const ValVar* var, const ICFGNode* node) override;
    AbstractValue getAbsValue(const ObjVar* var, const ICFGNode* node) override;
    AbstractValue getAbsValue(const SVFVar* var, const ICFGNode* node) override;

    bool hasAbsValue(const ValVar* var, const ICFGNode* node) const override;
    bool hasAbsValue(const ObjVar* var, const ICFGNode* node) const override;
    bool hasAbsValue(const SVFVar* var, const ICFGNode* node) const override;

    void updateAbsValue(const ValVar* var, const AbstractValue& value,
                        const ICFGNode* node) override;
    void updateAbsValue(const ObjVar* var, const AbstractValue& value,
                        const ICFGNode* node) override;
    void updateAbsValue(const SVFVar* var, const AbstractValue& value,
                        const ICFGNode* node) override;

    AbstractValue getMemoryValue(u32_t address, const ICFGNode* node) override;
    bool hasMemoryValue(u32_t address, const ICFGNode* node) const override;
    void updateMemoryValue(u32_t address, const AbstractValue& value,
                           const ICFGNode* node) override;
    void markFreedMemory(u32_t address, const ICFGNode* node) override;
    bool isFreedMemory(u32_t address, const ICFGNode* node) const override;

    AbstractValue loadValue(const ValVar* pointer,
                            const ICFGNode* node) override;
    void storeValue(const ValVar* pointer, const AbstractValue& value,
                    const ICFGNode* node) override;

protected:
    void handleGlobalNode() override;
    AbstractValue initializeObjectAddress(const ObjVar* object,
                                          const ICFGNode* node) override;
    void resetAbstractState(const ICFGNode* node) override;
    void copyAbstractState(const ICFGNode* source,
                           const ICFGNode* destination) override;
    std::unique_ptr<AbstractDomain::AbstractState> cloneAbstractState(
        const ICFGNode* node) const override;
    bool isAbstractStateEquivalent(
        const ICFGNode* node,
        const AbstractDomain::AbstractState& snapshot) const override;

    std::unique_ptr<AbstractDomain::AbstractState> cloneCycleHeadState(
        const ICFGCycleWTO* cycle) override;
    bool widenCycleState(const AbstractDomain::AbstractState& previous,
                         const AbstractDomain::AbstractState& current,
                         const ICFGCycleWTO* cycle) override;
    bool narrowCycleState(const AbstractDomain::AbstractState& previous,
                          const AbstractDomain::AbstractState& current,
                          const ICFGCycleWTO* cycle) override;
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;
    bool isBranchEdgeFeasibleAt(const IntraCFGEdge* edge,
                                const ICFGNode* predecessor) override;
    void recordBranchRefinement(NodeID objectId,
                                const IntervalValue& narrowed,
                                AbstractDomain::AbstractState& state,
                                const ICFGNode* loadNode,
                                const ICFGNode* successor) override;
    void initializeDomainState(const ICFGNode* node) override;
    void assignDomainInterval(const ICFGNode* node, const SVFVar* target,
                              const IntervalValue& interval) override;
    void updateDomainOnBinary(const BinaryOPStmt* binary,
                              const IntervalValue& result) override;
    void updateDomainOnCopy(const CopyStmt* copy) override;
    void updateDomainCopyValue(const ICFGNode* node, const SVFVar* target,
                               const SVFVar* source,
                               bool exactMathematicalCopy) override;

protected:
    DenseState& ensureState(const ICFGNode* node);
    const DenseState& state(const ICFGNode* node) const;
    DenseState topState(const ICFGNode* node) const;
    DenseState bottomState(const ICFGNode* node) const;
    NumericalStateT makeNumericalTop(
        const AbstractDomain::VariableEnvironment& environment) const;
    NumericalStateT makeNumericalBottom(
        const AbstractDomain::VariableEnvironment& environment) const;

    AbstractValue projectValue(const DenseState& state,
                               AbstractDomain::Variable variable) const;
    void assignValue(DenseState& state, AbstractDomain::Variable variable,
                     const AbstractValue& value);
    void ensureVariable(DenseState& state,
                        AbstractDomain::Variable variable) const;
    void assignInterval(DenseState& state, AbstractDomain::Variable variable,
                        const IntervalValue& interval);
    void constrainInterval(DenseState& state, AbstractDomain::Variable variable,
                           const IntervalValue& interval);
    virtual void materializeValue(DenseState& state, const ValVar* value,
                                  const ICFGNode* node);
    void forgetValue(DenseState& state,
                     AbstractDomain::Variable variable) const;
    void forgetScalarValues(DenseState& state) const;
    void assumeBranch(const IntraCFGEdge* edge, DenseState& state);

    SVFIRAdapter adapter_;
    Map<const ICFGNode*, DenseState> denseTrace_;
};

extern template class DenseAbstractInterpretation<AbstractDomain::BoxState>;

} // namespace SVF

#endif // SVF_AE_DENSE_ABSTRACT_INTERPRETATION_H
