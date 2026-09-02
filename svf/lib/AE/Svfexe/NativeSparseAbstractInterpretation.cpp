//===- NativeSparseAbstractInterpretation.cpp -- Domain sparse AE -------===//

#include "AE/Svfexe/NativeSparseAbstractInterpretation.h"

#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <type_traits>

namespace SVF
{

namespace AD = AbstractDomain;

namespace
{

template <typename MetricT> class PhaseTimer
{
public:
    PhaseTimer(MetricT& metric, bool enabled)
        : metric_(metric), enabled_(enabled)
    {
        if (enabled_)
            start_ = Clock::now();
    }

    ~PhaseTimer()
    {
        if (!enabled_)
            return;
        ++metric_.calls;
        metric_.nanoseconds += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() -
                                                                 start_)
                .count());
    }

private:
    using Clock = std::chrono::steady_clock;
    MetricT& metric_;
    bool enabled_;
    Clock::time_point start_{};
};

} // namespace

template <typename NumericalStateT>
NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::NativeSemiSparseAbstractInterpretation()
{
    this->preAnalysis->initCycleValVars();
}

template <typename NumericalStateT>
typename NativeSemiSparseAbstractInterpretation<NumericalStateT>::DenseState
NativeSemiSparseAbstractInterpretation<NumericalStateT>::flowState(
    const FunObjVar* function, bool bottom) const
{
    const AD::VariableEnvironment& environment =
        this->adapter_.environment(function);
    return DenseState(bottom ? this->makeNumericalBottom(environment)
                             : this->makeNumericalTop(environment),
                      this->adapter_.memoryLayout());
}

template <typename NumericalStateT>
typename NativeSemiSparseAbstractInterpretation<NumericalStateT>::DenseState&
NativeSemiSparseAbstractInterpretation<NumericalStateT>::scalarState(
    const FunObjVar* function)
{
    if constexpr (std::is_same_v<NumericalStateT, AD::BoxState>)
        function = nullptr;
    auto iterator = scalarStates_.find(function);
    if (iterator == scalarStates_.end())
    {
        iterator =
            scalarStates_
                .emplace(
                    function,
                    DenseState(
                        this->makeNumericalTop(
                            std::is_same_v<NumericalStateT, AD::BoxState>
                                ? this->adapter_.allScalarEnvironment()
                                : this->adapter_.scalarEnvironment(function)),
                        this->adapter_.memoryLayout()))
                .first;
    }
    return iterator->second;
}

template <typename NumericalStateT>
const typename NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::DenseState*
NativeSemiSparseAbstractInterpretation<NumericalStateT>::findScalarState(
    const FunObjVar* function) const
{
    if constexpr (std::is_same_v<NumericalStateT, AD::BoxState>)
        function = nullptr;
    const auto iterator = scalarStates_.find(function);
    return iterator == scalarStates_.end() ? nullptr : &iterator->second;
}

template <typename NumericalStateT>
const AD::AbstractState* NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::getScalarAbstractState(const FunObjVar* function) const
{
    return findScalarState(function);
}

template <typename NumericalStateT>
const AD::AbstractState* NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::getScalarAbstractState(const ValVar* value) const
{
    if (!value)
        return nullptr;
    const auto iterator = scalarCheckpoints_.find(value);
    return iterator == scalarCheckpoints_.end()
               ? getScalarAbstractState(value->getFunction())
               : &iterator->second;
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::handleGlobalNode()
{
    Base::handleGlobalNode();
    finalizeAbstractState(this->icfg->getGlobalICFGNode());
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::runOnModule()
{
    {
        PhaseTimer timer(sparseProfile_.total, Options::AESparseProfile());
        Base::runOnModule();
    }
    if (Options::AESparseProfile())
        reportSparseProfile();
}

template <typename NumericalStateT>
const char* NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::sparseProfileMode() const
{
    return "semi";
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::reportSparseProfile() const
{
    const std::ios::fmtflags previousFlags = std::cout.flags();
    const std::streamsize previousPrecision = std::cout.precision();
    auto report = [&](const char* phase, const PhaseMetric& metric) {
        const double seconds =
            static_cast<double>(metric.nanoseconds) / 1'000'000'000.0;
        const double nanosecondsPerCall =
            metric.calls == 0 ? 0.0
                              : static_cast<double>(metric.nanoseconds) /
                                    static_cast<double>(metric.calls);
        std::cout << "AE_SPARSE_PHASE mode=" << sparseProfileMode()
                  << " phase=" << phase << " calls=" << metric.calls
                  << " seconds=" << std::fixed << std::setprecision(6)
                  << seconds << " ns_per_call=" << std::setprecision(1)
                  << nanosecondsPerCall << '\n';
    };
    report("total", sparseProfile_.total);
    report("state-copy", sparseProfile_.stateCopy);
    report("state-merge", sparseProfile_.stateMerge);
    report("environment-alignment", sparseProfile_.environmentAlignment);
    report("state-join", sparseProfile_.stateJoin);
    report("state-equivalence", sparseProfile_.stateEquivalence);
    report("scalar-materialization", sparseProfile_.scalarMaterialization);
    report("scalar-checkpoint", sparseProfile_.scalarCheckpoint);
    report("state-filtering", sparseProfile_.stateFiltering);
    report("cycle", sparseProfile_.cycle);
    report("svfg-build", sparseProfile_.svfgBuild);
    report("object-pull", sparseProfile_.objectPull);
    report("path-feasibility", sparseProfile_.pathFeasibility);
    report("memory-refinement", sparseProfile_.memoryRefinement);
    std::cout.flags(previousFlags);
    std::cout.precision(previousPrecision);
}

template <typename NumericalStateT>
AbstractValue NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::getAbsValue(const ValVar* value, const ICFGNode* node)
{
    (void)node;
    if (const auto* integer = SVFUtil::dyn_cast<ConstIntValVar>(value))
        return IntervalValue(integer->getSExtValue());
    if (!value || !this->adapter_.contains(*value))
        return IntervalValue::top();

    DenseState& scalars = scalarState(value->getFunction());
    const AD::Variable variable = this->adapter_.variable(*value);
    if (!scalars.shapes().isDefined(variable))
        this->assignValue(scalars, variable, IntervalValue::top());
    AbstractValue result = this->projectValue(scalars, variable);
    if (value->isPointer())
        result.interval = IntervalValue::bottom();
    return result;
}

template <typename NumericalStateT>
bool NativeSemiSparseAbstractInterpretation<NumericalStateT>::hasAbsValue(
    const ValVar* value, const ICFGNode* node) const
{
    (void)node;
    if (SVFUtil::isa<ConstIntValVar>(value))
        return true;
    if (!value || !this->adapter_.contains(*value))
        return false;
    const DenseState* scalars = findScalarState(value->getFunction());
    return scalars &&
           scalars->shapes().isDefined(this->adapter_.variable(*value));
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::updateAbsValue(
    const ValVar* value, const AbstractValue& abstractValue,
    const ICFGNode* node)
{
    (void)node;
    if (value && this->adapter_.contains(*value))
        this->assignValue(scalarState(value->getFunction()),
                          this->adapter_.variable(*value), abstractValue);
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::copyAbstractState(
    const ICFGNode* source, const ICFGNode* destination)
{
    PhaseTimer timer(sparseProfile_.stateCopy, Options::AESparseProfile());
    DenseState copy = this->state(source);
    const AD::VariableEnvironment& destinationEnvironment =
        this->adapter_.environment(destination->getFun());
    if (copy.numerical().environment() != destinationEnvironment)
        copy.changeEnvironment(destinationEnvironment);
    this->denseTrace_.insert_or_assign(destination, std::move(copy));
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::resetAbstractState(const ICFGNode* node)
{
    this->denseTrace_.insert_or_assign(node, flowState(node->getFun()));
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::finalizeAbstractState(const ICFGNode* node)
{
    PhaseTimer timer(sparseProfile_.stateFiltering, Options::AESparseProfile());
    DenseState& denseState = this->ensureState(node);
    forgetActiveScalarValues(denseState);
}

template <typename NumericalStateT>
bool NativeSemiSparseAbstractInterpretation<NumericalStateT>::
    isAbstractStateEquivalent(const ICFGNode* node,
                              const AD::AbstractState& snapshot) const
{
    PhaseTimer timer(sparseProfile_.stateEquivalence,
                     Options::AESparseProfile());
    return Base::isAbstractStateEquivalent(node, snapshot);
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::forgetActiveScalarValues(DenseState& denseState) const
{
    if constexpr (std::is_same_v<NumericalStateT, AD::BoxState>)
    {
        const std::vector<AD::Variable> defined =
            denseState.shapes().definedVariables(
                denseState.numerical().environment());
        for (AD::Variable variable : defined)
        {
            if (this->adapter_.value(variable))
                this->forgetValue(denseState, variable);
        }
    }
    else
    {
        // Box checkpoints may constrain a variable without exposing
        // it through the definedness facet, so relational domains retain the
        // conservative full-environment purge.
        this->forgetScalarValues(denseState);
    }
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::forgetMemoryValues(DenseState& denseState) const
{
    const std::vector<AD::Variable> defined =
        denseState.shapes().definedVariables(
            denseState.numerical().environment());
    for (AD::Variable variable : defined)
    {
        if (this->adapter_.contentObject(variable))
            this->forgetValue(denseState, variable);
    }
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::applyScalarCheckpoint(DenseState& denseState,
                                            const DenseState& checkpoint)
{
    PhaseTimer timer(sparseProfile_.scalarCheckpoint,
                     Options::AESparseProfile());
    if constexpr (std::is_same_v<NumericalStateT, AD::BoxState>)
    {
        const std::vector<AD::Variable> defined =
            checkpoint.shapes().definedVariables(
                checkpoint.numerical().environment());
        for (AD::Variable variable : defined)
        {
            if (!this->adapter_.value(variable) ||
                !checkpoint.shapes().hasNumeric(variable))
                continue;
            if (!denseState.numerical().environment().contains(variable))
                this->ensureVariable(denseState, variable);
            const AbstractValue value =
                this->projectValue(checkpoint, variable);
            if (!value.isInterval())
                continue;
            this->constrainInterval(denseState, variable, value.getInterval());
            denseState.pointers().assign(variable, AD::PointeeSet::bottom());
            denseState.shapes().assign(variable, true);
        }
        return;
    }

    if (checkpoint.isTop())
        return;
    DenseState scalar = checkpoint;
    forgetMemoryValues(scalar);
    if (denseState.numerical().environment() !=
        scalar.numerical().environment())
    {
        const AD::VariableEnvironment environment =
            denseState.numerical().environment().merge(
                scalar.numerical().environment());
        denseState.changeEnvironment(environment);
        scalar.changeEnvironment(environment);
    }
    denseState.numerical().meetWith(scalar.numerical());
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::materializeValue(
    DenseState& denseState, const ValVar* value, const ICFGNode* node)
{
    PhaseTimer timer(sparseProfile_.scalarMaterialization,
                     Options::AESparseProfile());
    if (!value || !this->adapter_.contains(*value))
        return;
    const AD::Variable variable = this->adapter_.variable(*value);
    if (denseState.shapes().isDefined(variable))
        return;
    auto materializeFacets = [&]() {
        const AbstractValue projected = getAbsValue(value, node);
        AD::PointeeSet addresses = AD::PointeeSet::bottom();
        for (u32_t address : projected.getAddrs())
        {
            const auto* object = SVFUtil::dyn_cast<ObjVar>(
                this->svfir->getGNode(Base::objectIdFromAddress(address)));
            if (object && this->adapter_.contains(*object))
                addresses.insert(this->adapter_.location(*object));
        }
        denseState.pointers().assign(variable, std::move(addresses));
        denseState.shapes().assign(variable, projected.isInterval());
    };
    if constexpr (!std::is_same_v<NumericalStateT, AD::BoxState>)
    {
        const auto checkpoint = scalarCheckpoints_.find(value);
        if (checkpoint != scalarCheckpoints_.end())
        {
            applyScalarCheckpoint(denseState, checkpoint->second);
            materializeFacets();
            return;
        }
    }
    // Branch-refinement states carry numerical constraints without marking
    // the corresponding scalar as a persistent product value. Preserve that
    // latent constraint and materialize only its address/shape facets.
    if (denseState.numerical().environment().contains(variable))
    {
        materializeFacets();
        return;
    }
    this->assignValue(denseState, variable, getAbsValue(value, node));
}

template <typename NumericalStateT>
AbstractValue NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::loadValue(const ValVar* pointer, const ICFGNode* node)
{
    AbstractValue result = Base::loadValue(pointer, node);
    if (pointer && this->adapter_.contains(*pointer))
        this->forgetValue(this->ensureState(node),
                          this->adapter_.variable(*pointer));
    return result;
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::storeValue(
    const ValVar* pointer, const AbstractValue& value, const ICFGNode* node)
{
    Base::storeValue(pointer, value, node);
    if (pointer && this->adapter_.contains(*pointer))
        this->forgetValue(this->ensureState(node),
                          this->adapter_.variable(*pointer));
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::filterPropagatedState(DenseState& denseState) const
{
    (void)denseState;
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::collectMemoryBranchRefinement(const IntraCFGEdge* edge,
                                                     DenseState& state)
{
    this->collectBranchRefinement(edge, state);
}

template <typename NumericalStateT>
bool NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::mergeStatesFromPredecessors(const ICFGNode* node)
{
    PhaseTimer timer(sparseProfile_.stateMerge, Options::AESparseProfile());
    DenseState merged = flowState(node->getFun(), true);
    std::optional<DenseState> mergedRefinement;
    bool refinementIsTop = false;
    bool hasFeasiblePredecessor = false;

    for (const ICFGEdge* edge : node->getInEdges())
    {
        const ICFGNode* predecessor = edge->getSrcNode();
        if (!this->hasAbsState(predecessor))
            continue;

        bool shouldMerge = false;
        const auto* conditional = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
        if (conditional || SVFUtil::isa<CallCFGEdge>(edge))
        {
            shouldMerge = true;
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            shouldMerge = Options::HandleRecur() == Base::TOP;
            if (!shouldMerge)
            {
                const auto* returnSite = SVFUtil::dyn_cast<RetICFGNode>(node);
                shouldMerge = returnSite &&
                              this->hasAbsState(returnSite->getCallICFGNode());
            }
        }
        if (!shouldMerge)
            continue;

        const auto refinementIterator = refinementTrace_.find(predecessor);
        const bool hasConditional = conditional && conditional->getCondition();
        const bool needsRefinement =
            hasConditional || refinementIterator != refinementTrace_.end();
        std::optional<DenseState> refinement;
        if (needsRefinement)
        {
            refinement = refinementIterator != refinementTrace_.end()
                             ? refinementIterator->second
                             : this->topState(node);
            const AD::VariableEnvironment& destinationEnvironment =
                this->adapter_.environment(node->getFun());
            if (refinement->numerical().environment() != destinationEnvironment)
            {
                PhaseTimer environmentTimer(sparseProfile_.environmentAlignment,
                                            Options::AESparseProfile());
                refinement->changeEnvironment(destinationEnvironment);
            }
            if (hasConditional)
                this->assumeBranch(conditional, *refinement);
            if (refinement->isBottom())
                continue;
        }

        DenseState source = this->state(predecessor);
        filterPropagatedState(source);
        const AD::VariableEnvironment& destinationEnvironment =
            this->adapter_.environment(node->getFun());
        if (source.numerical().environment() != destinationEnvironment)
        {
            PhaseTimer environmentTimer(sparseProfile_.environmentAlignment,
                                        Options::AESparseProfile());
            source.changeEnvironment(destinationEnvironment);
        }
        if (hasConditional)
            collectMemoryBranchRefinement(conditional, source);

        {
            PhaseTimer joinTimer(sparseProfile_.stateJoin,
                                 Options::AESparseProfile());
            merged.joinWith(source);
        }
        if (!refinement || refinement->isTop())
        {
            refinementIsTop = true;
            mergedRefinement.reset();
        }
        else if (!refinementIsTop)
        {
            forgetMemoryValues(*refinement);
            if (!mergedRefinement)
                mergedRefinement = std::move(*refinement);
            else
                mergedRefinement->joinWith(*refinement);
        }
        hasFeasiblePredecessor = true;
    }

    if (!hasFeasiblePredecessor)
        return false;
    if (mergedRefinement && !refinementIsTop && !mergedRefinement->isTop())
    {
        refinementTrace_.insert_or_assign(node, *mergedRefinement);
        applyScalarCheckpoint(merged, *mergedRefinement);
    }
    else
    {
        refinementTrace_.erase(node);
    }
    this->denseTrace_.insert_or_assign(node, std::move(merged));
    return true;
}

template <typename NumericalStateT>
std::unique_ptr<AD::AbstractState> NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::cloneCycleHeadState(const ICFGCycleWTO* cycle)
{
    PhaseTimer timer(sparseProfile_.cycle, Options::AESparseProfile());
    const ICFGNode* head = cycle->head()->getICFGNode();
    DenseState snapshot = this->state(head);
    for (const ValVar* value : this->preAnalysis->getCycleValVars(cycle))
    {
        if (!value || !this->adapter_.contains(*value) ||
            !hasAbsValue(value, head))
            continue;
        this->assignValue(snapshot, this->adapter_.variable(*value),
                          getAbsValue(value, head));
    }
    return std::make_unique<DenseState>(std::move(snapshot));
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::scatterCycleValues(const ICFGCycleWTO* cycle,
                                         const DenseState& cycleState)
{
    for (const ValVar* value : this->preAnalysis->getCycleValVars(cycle))
    {
        if (!value || !this->adapter_.contains(*value))
            continue;
        const AD::Variable variable = this->adapter_.variable(*value);
        if (!cycleState.shapes().isDefined(variable))
            continue;
        updateAbsValue(value, this->projectValue(cycleState, variable),
                       cycle->head()->getICFGNode());
    }
}

template <typename NumericalStateT>
bool NativeSemiSparseAbstractInterpretation<NumericalStateT>::widenCycleState(
    const AD::AbstractState& previous, const AD::AbstractState& current,
    const ICFGCycleWTO* cycle)
{
    PhaseTimer timer(sparseProfile_.cycle, Options::AESparseProfile());
    const bool fixpoint = Base::widenCycleState(previous, current, cycle);
    scatterCycleValues(cycle, this->state(cycle->head()->getICFGNode()));
    finalizeAbstractState(cycle->head()->getICFGNode());
    return fixpoint;
}

template <typename NumericalStateT>
bool NativeSemiSparseAbstractInterpretation<NumericalStateT>::narrowCycleState(
    const AD::AbstractState& previous, const AD::AbstractState& current,
    const ICFGCycleWTO* cycle)
{
    PhaseTimer timer(sparseProfile_.cycle, Options::AESparseProfile());
    const bool fixpoint = Base::narrowCycleState(previous, current, cycle);
    if (!fixpoint)
    {
        scatterCycleValues(cycle, this->state(cycle->head()->getICFGNode()));
    }
    finalizeAbstractState(cycle->head()->getICFGNode());
    return fixpoint;
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::assignDomainInterval(const ICFGNode* node,
                                           const SVFVar* target,
                                           const IntervalValue& interval)
{
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(target))
    {
        if (!this->adapter_.contains(*value) || interval.isBottom())
            return;
        this->assignInterval(scalarState(value->getFunction()),
                             this->adapter_.variable(*value), interval);
        return;
    }
    Base::assignDomainInterval(node, target, interval);
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::commitBinaryResult(const BinaryOPStmt* binary,
                                         const DenseState& transferState,
                                         const IntervalValue& fallback)
{
    const auto* target = SVFUtil::dyn_cast<ValVar>(binary->getRes());
    if (!target || !this->adapter_.contains(*target))
        return;

    const AD::Variable targetVariable = this->adapter_.variable(*target);
    AbstractValue projected = this->projectValue(transferState, targetVariable);
    const IntervalValue interval =
        projected.isInterval() ? projected.getInterval() : fallback;
    DenseState& scalars = scalarState(target->getFunction());
    this->assignInterval(scalars, targetVariable, interval);
    if constexpr (!std::is_same_v<NumericalStateT, AD::BoxState>)
    {
        DenseState checkpoint = transferState;
        checkpoint.changeEnvironment(
            this->adapter_.scalarEnvironment(target->getFunction()));
        scalarCheckpoints_.insert_or_assign(target, std::move(checkpoint));
    }
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::updateDomainOnBinary(const BinaryOPStmt* binary,
                                           const IntervalValue& result)
{
    Base::updateDomainOnBinary(binary, result);
    DenseState& transferState = this->ensureState(binary->getICFGNode());
    if (const auto* target = SVFUtil::dyn_cast<ValVar>(binary->getRes());
        target && this->adapter_.contains(*target))
    {
        const AD::Variable variable = this->adapter_.variable(*target);
        transferState.pointers().assign(variable, AD::PointeeSet::bottom());
        transferState.shapes().assign(variable, true);
    }
    commitBinaryResult(binary, transferState, result);
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<NumericalStateT>::commitCopyResult(
    const SVFVar* target, bool exactMathematicalCopy,
    const DenseState& transferState)
{
    const auto* targetValue = SVFUtil::dyn_cast<ValVar>(target);
    if (!targetValue || !this->adapter_.contains(*targetValue))
        return;
    const AD::Variable targetVariable = this->adapter_.variable(*targetValue);
    const AbstractValue projected =
        this->projectValue(transferState, targetVariable);
    if (!projected.isInterval())
        return;

    DenseState& scalars = scalarState(targetValue->getFunction());
    this->assignInterval(scalars, targetVariable, projected.getInterval());
    if constexpr (!std::is_same_v<NumericalStateT, AD::BoxState>)
    {
        if (exactMathematicalCopy)
        {
            DenseState checkpoint = transferState;
            checkpoint.changeEnvironment(
                this->adapter_.scalarEnvironment(targetValue->getFunction()));
            scalarCheckpoints_.insert_or_assign(targetValue,
                                                std::move(checkpoint));
        }
    }
}

template <typename NumericalStateT>
void NativeSemiSparseAbstractInterpretation<
    NumericalStateT>::updateDomainCopyValue(const ICFGNode* node,
                                            const SVFVar* target,
                                            const SVFVar* source,
                                            bool exactMathematicalCopy)
{
    Base::updateDomainCopyValue(node, target, source, exactMathematicalCopy);
    DenseState& transferState = this->ensureState(node);
    const auto* targetValue = SVFUtil::dyn_cast<ValVar>(target);
    if (targetValue && this->adapter_.contains(*targetValue) &&
        getAbsValue(targetValue, node).isInterval())
    {
        const AD::Variable variable = this->adapter_.variable(*targetValue);
        transferState.pointers().assign(variable, AD::PointeeSet::bottom());
        transferState.shapes().assign(variable, true);
    }
    commitCopyResult(target, exactMathematicalCopy, transferState);
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

template <typename NumericalStateT>
NativeFullSparseAbstractInterpretation<
    NumericalStateT>::NativeFullSparseAbstractInterpretation()
{
    PhaseTimer timer(this->sparseProfile_.svfgBuild,
                     Options::AESparseProfile());
    svfgBuilder_ = std::make_unique<SVFGBuilder>(true);
    svfgBuilder_->buildFullSVFG(this->preAnalysis->getPointerAnalysis());
}

template <typename NumericalStateT>
NativeFullSparseAbstractInterpretation<
    NumericalStateT>::~NativeFullSparseAbstractInterpretation() = default;

template <typename NumericalStateT>
const char* NativeFullSparseAbstractInterpretation<
    NumericalStateT>::sparseProfileMode() const
{
    return "full";
}

template <typename NumericalStateT>
void NativeFullSparseAbstractInterpretation<
    NumericalStateT>::filterPropagatedState(DenseState& denseState) const
{
    PhaseTimer timer(this->sparseProfile_.stateFiltering,
                     Options::AESparseProfile());
    this->forgetActiveScalarValues(denseState);
    const std::vector<AD::Variable> defined =
        denseState.shapes().definedVariables(
            denseState.numerical().environment());
    for (AD::Variable variable : defined)
    {
        const ObjVar* object = this->adapter_.contentObject(variable);
        if (object && !SVFUtil::isa<GepObjVar>(object))
            this->forgetValue(denseState, variable);
    }
}

template <typename NumericalStateT>
void NativeFullSparseAbstractInterpretation<
    NumericalStateT>::collectMemoryBranchRefinement(const IntraCFGEdge* edge,
                                                     DenseState& state)
{
    this->collectBranchRefinement(edge, state);
}

template <typename NumericalStateT>
void NativeFullSparseAbstractInterpretation<
    NumericalStateT>::recordBranchRefinement(NodeID objectId,
                                             const IntervalValue& narrowed,
                                             AD::AbstractState&,
                                             const ICFGNode*,
                                             const ICFGNode* successor)
{
    if (narrowed.isBottom())
        return;
    auto& refinements = memoryRefinementTrace_[successor];
    const auto iterator = refinements.find(objectId);
    if (iterator == refinements.end())
        refinements.emplace(objectId, narrowed);
    else
        iterator->second.join_with(narrowed);
}

template <typename NumericalStateT>
void NativeFullSparseAbstractInterpretation<NumericalStateT>::storeValue(
    const ValVar* pointer, const AbstractValue& value, const ICFGNode* node)
{
    const AbstractValue addresses = Base::getAbsValue(pointer, node);
    auto refinement = memoryRefinementTrace_.find(node);
    if (refinement != memoryRefinementTrace_.end())
    {
        for (u32_t address : addresses.getAddrs())
            refinement->second.erase(this->objectIdFromAddress(address));
    }
    Base::storeValue(pointer, value, node);
}

template <typename NumericalStateT>
bool NativeFullSparseAbstractInterpretation<
    NumericalStateT>::mergeStatesFromPredecessors(const ICFGNode* node)
{
    memoryRefinementTrace_.erase(node);
    if (!Base::mergeStatesFromPredecessors(node))
        return false;
    // A direct object constraint collected from one incoming branch cannot be
    // applied after another incoming path has joined without that constraint.
    // Inherited constraints below already implement the precise all-preds
    // intersection rule; discard edge-local constraints at explicit merges.
    if (node->getInEdges().size() > 1)
        memoryRefinementTrace_.erase(node);
    pullObjectValueFlows(node);
    propagateAndApplyMemoryRefinement(node);
    return true;
}

template <typename NumericalStateT>
void NativeFullSparseAbstractInterpretation<
    NumericalStateT>::pullObjectValueFlows(const ICFGNode* node)
{
    PhaseTimer timer(this->sparseProfile_.objectPull,
                     Options::AESparseProfile());
    NodeBS denseLocalObjects;
    const DenseState& destination = this->state(node);
    const std::vector<AD::Variable> defined =
        destination.shapes().definedVariables(
            destination.numerical().environment());
    for (AD::Variable variable : defined)
    {
        const ObjVar* object = this->adapter_.contentObject(variable);
        if (object && SVFUtil::isa<GepObjVar>(object) &&
            destination.shapes().isDefined(variable))
            denseLocalObjects.set(object->getId());
    }

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
                    if (!object || !Base::hasAbsValue(object, source))
                        continue;

                    AbstractValue joined;
                    if (Base::hasAbsValue(object, node))
                        joined = Base::getAbsValue(object, node);
                    joined.join_with(Base::getAbsValue(object, source));
                    Base::updateAbsValue(object, joined, node);
                }
            }
        }
    }
}

template <typename NumericalStateT>
bool NativeFullSparseAbstractInterpretation<
    NumericalStateT>::isIntraEdgeBranchFeasible(const IntraCFGEdge* edge,
                                                const ICFGNode* source)
{
    return !edge->getCondition() || !this->hasAbsState(source) ||
           this->isBranchEdgeFeasibleAt(edge, source);
}

template <typename NumericalStateT>
bool NativeFullSparseAbstractInterpretation<
    NumericalStateT>::isIndirectSVFGEdgeFeasible(const IndirectSVFGEdge* edge,
                                                 const VFGNode* destination)
{
    PhaseTimer timer(this->sparseProfile_.pathFeasibility,
                     Options::AESparseProfile());
    assert(edge && destination && "SVFG edge and destination must exist");
    const auto* sourceNode = SVFUtil::dyn_cast<SVFGNode>(edge->getSrcNode());
    assert(sourceNode && "indirect SVFG edge must have an SVFG source");
    const ICFGNode* source = sourceNode->getICFGNode();
    const ICFGNode* target = destination->getICFGNode();
    assert(source && target && "SVFG endpoints must have ICFG nodes");

    const FunObjVar* function = source->getFun();
    if (source == target || !function || function != target->getFun())
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

template <typename NumericalStateT>
void NativeFullSparseAbstractInterpretation<
    NumericalStateT>::propagateAndApplyMemoryRefinement(const ICFGNode* node)
{
    PhaseTimer timer(this->sparseProfile_.memoryRefinement,
                     Options::AESparseProfile());
    Map<NodeID, IntervalValue> inherited;
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
                iterator->second.join_with(incoming->second);
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
                current->second.meet_with(constraint);
        }
    }

    const auto refinements = memoryRefinementTrace_.find(node);
    if (refinements == memoryRefinementTrace_.end())
        return;
    DenseState& denseState = this->ensureState(node);
    for (const auto& [objectId, constraint] : refinements->second)
    {
        const auto* object =
            SVFUtil::dyn_cast<ObjVar>(this->svfir->getGNode(objectId));
        if (!object || !this->adapter_.contains(*object))
            continue;
        const AD::Variable content = this->adapter_.contentVariable(*object);
        if (denseState.shapes().isDefined(content))
            this->constrainInterval(denseState, content, constraint);
    }
}

template class NativeSemiSparseAbstractInterpretation<AD::BoxState>;
template class NativeFullSparseAbstractInterpretation<AD::BoxState>;

} // namespace SVF
