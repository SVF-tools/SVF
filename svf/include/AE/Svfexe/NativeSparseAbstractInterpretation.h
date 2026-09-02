//===- NativeSparseAbstractInterpretation.h -- Domain sparse AE -*- C++ -*-===//

#ifndef SVF_AE_NATIVE_SPARSE_ABSTRACT_INTERPRETATION_H
#define SVF_AE_NATIVE_SPARSE_ABSTRACT_INTERPRETATION_H

#include <cstdint>
#include <memory>

#include "AE/Svfexe/DenseAbstractInterpretation.h"

namespace SVF
{

class IndirectSVFGEdge;
class SVFGBuilder;
class VFGNode;

/// Semi-sparse AE backed by BoxProgramState. Box values use one module-wide
/// scalar carrier plus sparse definition checkpoints. Persistent ICFG states
/// carry memory and lifetime values, while transfers materialize scalar
/// operands only temporarily.
template <typename NumericalStateT>
class NativeSemiSparseAbstractInterpretation
    : public DenseAbstractInterpretation<NumericalStateT>
{
public:
    using Base = DenseAbstractInterpretation<NumericalStateT>;
    using DenseState = typename Base::DenseState;

    NativeSemiSparseAbstractInterpretation();
    ~NativeSemiSparseAbstractInterpretation() override = default;
    void runOnModule() override;
    const AbstractDomain::AbstractState* getScalarAbstractState(
        const FunObjVar* function) const override;
    const AbstractDomain::AbstractState* getScalarAbstractState(
        const ValVar* value) const override;

protected:
    void handleGlobalNode() override;
    struct PhaseMetric
    {
        std::uint64_t calls = 0;
        std::uint64_t nanoseconds = 0;
    };

    struct SparsePhaseProfile
    {
        PhaseMetric total;
        PhaseMetric stateCopy;
        PhaseMetric stateMerge;
        PhaseMetric environmentAlignment;
        PhaseMetric stateJoin;
        PhaseMetric stateEquivalence;
        PhaseMetric scalarMaterialization;
        PhaseMetric scalarCheckpoint;
        PhaseMetric stateFiltering;
        PhaseMetric cycle;
        PhaseMetric svfgBuild;
        PhaseMetric objectPull;
        PhaseMetric pathFeasibility;
        PhaseMetric memoryRefinement;
    };

    AbstractValue getAbsValue(const ValVar* var, const ICFGNode* node) override;
    using Base::getAbsValue;
    bool hasAbsValue(const ValVar* var, const ICFGNode* node) const override;
    using Base::hasAbsValue;
    void updateAbsValue(const ValVar* var, const AbstractValue& value,
                        const ICFGNode* node) override;
    using Base::updateAbsValue;

    void copyAbstractState(const ICFGNode* source,
                           const ICFGNode* destination) override;
    void resetAbstractState(const ICFGNode* node) override;
    void finalizeAbstractState(const ICFGNode* node) override;
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;
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

    void assignDomainInterval(const ICFGNode* node, const SVFVar* target,
                              const IntervalValue& interval) override;
    void updateDomainOnBinary(const BinaryOPStmt* binary,
                              const IntervalValue& result) override;
    void updateDomainCopyValue(const ICFGNode* node, const SVFVar* target,
                               const SVFVar* source,
                               bool exactMathematicalCopy) override;
    void materializeValue(DenseState& state, const ValVar* value,
                          const ICFGNode* node) override;
    AbstractValue loadValue(const ValVar* pointer,
                            const ICFGNode* node) override;
    void storeValue(const ValVar* pointer, const AbstractValue& value,
                    const ICFGNode* node) override;

    /// Keep only the state facets that should flow along ordinary ICFG
    /// edges. Full-sparse overrides this to remove MemorySSA-managed objects.
    virtual void filterPropagatedState(DenseState& state) const;

    /// Optional memory refinement hook after a conditional edge has been
    /// proven feasible by the native numerical state.
    virtual void collectMemoryBranchRefinement(const IntraCFGEdge* edge,
                                               DenseState& state);

    DenseState& scalarState(const FunObjVar* function);
    const DenseState* findScalarState(const FunObjVar* function) const;
    DenseState flowState(const FunObjVar* function, bool bottom = false) const;
    void commitBinaryResult(const BinaryOPStmt* binary,
                            const DenseState& transferState,
                            const IntervalValue& fallback);
    void commitCopyResult(const SVFVar* target, bool exactMathematicalCopy,
                          const DenseState& transferState);
    void forgetActiveScalarValues(DenseState& state) const;
    void forgetMemoryValues(DenseState& state) const;
    void applyScalarCheckpoint(DenseState& state, const DenseState& checkpoint);
    void scatterCycleValues(const ICFGCycleWTO* cycle, const DenseState& state);
    virtual const char* sparseProfileMode() const;
    void reportSparseProfile() const;

    Map<const ICFGNode*, DenseState> refinementTrace_;
    Map<const FunObjVar*, DenseState> scalarStates_;
    Map<const ValVar*, DenseState> scalarCheckpoints_;
    mutable SparsePhaseProfile sparseProfile_;
};

/// Full-sparse AE backed by BoxProgramState. Scalar SSA values remain at
/// definition sites as in semi-sparse mode. Base/Dummy ObjVar contents move
/// along MemorySSA/SVFG def-use edges; GepObjVar snapshots and lifetime facts
/// continue to flow along the ICFG because they are not fully represented by
/// those edges.
template <typename NumericalStateT>
class NativeFullSparseAbstractInterpretation
    : public NativeSemiSparseAbstractInterpretation<NumericalStateT>
{
public:
    using Base = NativeSemiSparseAbstractInterpretation<NumericalStateT>;
    using DenseState = typename Base::DenseState;

    NativeFullSparseAbstractInterpretation();
    ~NativeFullSparseAbstractInterpretation() override;

protected:
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;
    void storeValue(const ValVar* pointer, const AbstractValue& value,
                    const ICFGNode* node) override;
    void filterPropagatedState(DenseState& state) const override;
    void collectMemoryBranchRefinement(const IntraCFGEdge* edge,
                                       DenseState& state) override;
    void recordBranchRefinement(NodeID objectId, const IntervalValue& narrowed,
                                AbstractDomain::AbstractState& state,
                                const ICFGNode* loadNode,
                                const ICFGNode* successor) override;

private:
    const char* sparseProfileMode() const override;
    void pullObjectValueFlows(const ICFGNode* node);
    bool isIndirectSVFGEdgeFeasible(const IndirectSVFGEdge* edge,
                                    const VFGNode* destination);
    bool isIntraEdgeBranchFeasible(const IntraCFGEdge* edge,
                                   const ICFGNode* source);
    void propagateAndApplyMemoryRefinement(const ICFGNode* node);

    Map<const ICFGNode*, Map<NodeID, IntervalValue>> memoryRefinementTrace_;
    std::unique_ptr<SVFGBuilder> svfgBuilder_;
};

extern template class NativeSemiSparseAbstractInterpretation<
    AbstractDomain::BoxState>;
extern template class NativeFullSparseAbstractInterpretation<
    AbstractDomain::BoxState>;

} // namespace SVF

#endif // SVF_AE_NATIVE_SPARSE_ABSTRACT_INTERPRETATION_H
