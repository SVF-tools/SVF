//===- DenseAbstractInterpretation.cpp -- Domain-backed dense AE --------===//

#include "AE/Svfexe/DenseAbstractInterpretation.h"

#include "SVFIR/SVFIR.h"
#include "Util/Options.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace SVF
{

namespace AD = AbstractDomain;

namespace
{

std::vector<const ICFGEdge*> orderedIncomingEdges(const ICFGNode* node)
{
    std::vector<const ICFGEdge*> edges(node->getInEdges().begin(),
                                       node->getInEdges().end());
    std::sort(edges.begin(), edges.end(),
              [](const ICFGEdge* lhs, const ICFGEdge* rhs) {
                  return std::make_tuple(lhs->getSrcID(),
                                         lhs->getEdgeKindWithoutMask()) <
                         std::make_tuple(rhs->getSrcID(),
                                         rhs->getEdgeKindWithoutMask());
              });
    return edges;
}

s64_t toSigned64(const mpz_class& value, bool upper)
{
    if (!mpz_fits_slong_p(value.get_mpz_t()))
        return upper ? std::numeric_limits<s64_t>::max()
                     : std::numeric_limits<s64_t>::min();
    return static_cast<s64_t>(value.get_si());
}

BoundedInt lowerBound(const AD::Bound& bound)
{
    if (bound.isMinusInfinity())
        return IntervalValue::minus_infinity();
    if (bound.isPlusInfinity())
        return IntervalValue::plus_infinity();
    AD::Rational integer = bound.isStrict()
                               ? bound.value().floor() + AD::Rational(1)
                               : bound.value().ceil();
    return BoundedInt(toSigned64(integer.value().get_num(), false));
}

BoundedInt upperBound(const AD::Bound& bound)
{
    if (bound.isPlusInfinity())
        return IntervalValue::plus_infinity();
    if (bound.isMinusInfinity())
        return IntervalValue::minus_infinity();
    AD::Rational integer = bound.isStrict()
                               ? bound.value().ceil() - AD::Rational(1)
                               : bound.value().floor();
    return BoundedInt(toSigned64(integer.value().get_num(), true));
}

IntervalValue projectInterval(const AD::Interval& interval)
{
    if (interval.isBottom())
        return IntervalValue::bottom();
    return IntervalValue(lowerBound(interval.lower()),
                         upperBound(interval.upper()));
}

AD::ConstraintKind negatePredicate(AD::ConstraintKind kind)
{
    switch (kind)
    {
    case AD::ConstraintKind::Equal:
        return AD::ConstraintKind::NotEqual;
    case AD::ConstraintKind::NotEqual:
        return AD::ConstraintKind::Equal;
    case AD::ConstraintKind::LessThan:
        return AD::ConstraintKind::GreaterEqual;
    case AD::ConstraintKind::LessEqual:
        return AD::ConstraintKind::GreaterThan;
    case AD::ConstraintKind::GreaterThan:
        return AD::ConstraintKind::LessEqual;
    case AD::ConstraintKind::GreaterEqual:
        return AD::ConstraintKind::LessThan;
    }
    return kind;
}

bool constraintKind(u32_t predicate, AD::ConstraintKind& kind)
{
    switch (predicate)
    {
    case CmpStmt::ICMP_EQ:
        kind = AD::ConstraintKind::Equal;
        return true;
    case CmpStmt::ICMP_NE:
        kind = AD::ConstraintKind::NotEqual;
        return true;
    case CmpStmt::ICMP_SLT:
        kind = AD::ConstraintKind::LessThan;
        return true;
    case CmpStmt::ICMP_SLE:
        kind = AD::ConstraintKind::LessEqual;
        return true;
    case CmpStmt::ICMP_SGT:
        kind = AD::ConstraintKind::GreaterThan;
        return true;
    case CmpStmt::ICMP_SGE:
        kind = AD::ConstraintKind::GreaterEqual;
        return true;
    default:
        return false;
    }
}

template <typename DenseStateT>
void alignEnvironments(DenseStateT& lhs, DenseStateT& rhs)
{
    if (lhs.numerical().environment() == rhs.numerical().environment())
        return;
    const AD::VariableEnvironment environment =
        lhs.numerical().environment().merge(rhs.numerical().environment());
    if (lhs.numerical().environment() != environment)
        lhs.changeEnvironment(environment);
    if (rhs.numerical().environment() != environment)
        rhs.changeEnvironment(environment);
}

} // namespace

template <typename NumericalStateT>
DenseAbstractInterpretation<NumericalStateT>::DenseAbstractInterpretation()
    : adapter_(*svfir)
{
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::runOnModule()
{
    AbstractInterpretation::runOnModule();
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    // The global ICFG node contains address initializers for local pointer
    // values from every function.  Growing a relational environment once per
    // initializer repeatedly rebuilds and normalizes the same state.  Batch
    // those result dimensions into the initial top state instead.
    std::vector<AD::VariableDeclaration> initialDeclarations;
    std::set<AD::Variable> initialVariables;
    auto addInitialValue = [&](const SVFVar* variable) {
        const auto* value = SVFUtil::dyn_cast<ValVar>(variable);
        if (!value || !adapter_.contains(*value))
            return;
        const AD::Variable symbol = adapter_.variable(*value);
        if (!adapter_.environment().contains(symbol) &&
            initialVariables.insert(symbol).second)
            initialDeclarations.push_back(adapter_.declaration(symbol));
    };
    for (const SVFStmt* statement : node->getSVFStmts())
    {
        if (const auto* assignment = SVFUtil::dyn_cast<AssignStmt>(statement))
            addInitialValue(assignment->getLHSVar());
        else if (const auto* multi =
                     SVFUtil::dyn_cast<MultiOpndStmt>(statement))
            addInitialValue(multi->getRes());
    }
    if (const auto* blackHole = SVFUtil::dyn_cast<ValVar>(
            svfir->getGNode(PAG::getPAG()->getBlkPtr())))
        addInitialValue(blackHole);
    const AD::VariableEnvironment initialEnvironment =
        adapter_.environment().add(std::move(initialDeclarations));
    denseTrace_.insert_or_assign(
        node, DenseState(makeNumericalTop(initialEnvironment),
                         adapter_.memoryLayout()));
    for (const SVFStmt* statement : node->getSVFStmts())
        handleSVFStatement(statement);

    AbstractValue blackHole(IntervalValue::top());
    blackHole.getAddrs().insert(BlackHoleObjAddr);
    if (const auto* variable = SVFUtil::dyn_cast<ValVar>(
            svfir->getGNode(PAG::getPAG()->getBlkPtr())))
        updateAbsValue(variable, blackHole, node);
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<
    NumericalStateT>::initializeObjectAddress(const ObjVar* object,
                                              const ICFGNode* node)
{
    DenseState& denseState = ensureState(node);
    if (adapter_.contains(*object))
        denseState.allocate(adapter_.location(*object));

    const BaseObjVar* base = PAG::getPAG()->getBaseObject(object->getId());
    if (base->isConstDataOrConstGlobal() || base->isConstantArray() ||
        base->isConstantStruct())
    {
        if (const auto* integer = SVFUtil::dyn_cast<ConstIntObjVar>(object))
            return IntervalValue(integer->getSExtValue());
        if (const auto* floating = SVFUtil::dyn_cast<ConstFPObjVar>(object))
            return IntervalValue(floating->getFPValue(),
                                 floating->getFPValue());
        if (SVFUtil::isa<ConstNullPtrObjVar>(object))
            return IntervalValue(0, 0);
        if (!SVFUtil::isa<GlobalObjVar>(object))
            return IntervalValue::top();
    }
    return AddressValue(AddressValue::getVirtualMemAddress(object->getId()));
}

template <typename NumericalStateT>
const AbstractDomain::AbstractState& DenseAbstractInterpretation<
    NumericalStateT>::getAbstractState(const ICFGNode* node) const
{
    return state(node);
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::hasAbsState(
    const ICFGNode* node) const
{
    return denseTrace_.count(node) != 0;
}

template <typename NumericalStateT>
typename DenseAbstractInterpretation<NumericalStateT>::DenseState
DenseAbstractInterpretation<NumericalStateT>::topState(
    const ICFGNode* node) const
{
    return DenseState(
        makeNumericalTop(adapter_.environment(node ? node->getFun() : nullptr)),
        adapter_.memoryLayout());
}

template <typename NumericalStateT>
typename DenseAbstractInterpretation<NumericalStateT>::DenseState
DenseAbstractInterpretation<NumericalStateT>::bottomState(
    const ICFGNode* node) const
{
    return DenseState(makeNumericalBottom(adapter_.environment(
                          node ? node->getFun() : nullptr)),
                      adapter_.memoryLayout());
}

template <typename NumericalStateT>
NumericalStateT DenseAbstractInterpretation<NumericalStateT>::makeNumericalTop(
    const AD::VariableEnvironment& environment) const
{
    if constexpr (std::is_same_v<NumericalStateT, AD::BoxState>)
        return AD::BoxState::top(environment);
    else
        return NumericalStateT::top(environment);
}

template <typename NumericalStateT>
NumericalStateT DenseAbstractInterpretation<NumericalStateT>::
    makeNumericalBottom(const AD::VariableEnvironment& environment) const
{
    if constexpr (std::is_same_v<NumericalStateT, AD::BoxState>)
        return AD::BoxState::bottom(environment);
    else
        return NumericalStateT::bottom(environment);
}

template <typename NumericalStateT>
typename DenseAbstractInterpretation<NumericalStateT>::DenseState&
DenseAbstractInterpretation<NumericalStateT>::ensureState(const ICFGNode* node)
{
    auto iterator = denseTrace_.find(node);
    if (iterator == denseTrace_.end())
        iterator = denseTrace_.emplace(node, topState(node)).first;
    return iterator->second;
}

template <typename NumericalStateT>
const typename DenseAbstractInterpretation<NumericalStateT>::DenseState&
DenseAbstractInterpretation<NumericalStateT>::state(const ICFGNode* node) const
{
    const auto iterator = denseTrace_.find(node);
    if (iterator == denseTrace_.end())
        throw std::out_of_range("no dense abstract state for ICFG node");
    return iterator->second;
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::resetAbstractState(
    const ICFGNode* node)
{
    denseTrace_.insert_or_assign(node, topState(node));
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::copyAbstractState(
    const ICFGNode* source, const ICFGNode* destination)
{
    DenseState copy = state(source);
    const AD::VariableEnvironment& destinationEnvironment =
        adapter_.environment(destination->getFun());
    if (copy.numerical().environment() == destinationEnvironment)
    {
        denseTrace_.insert_or_assign(destination, std::move(copy));
        return;
    }

    copy.changeEnvironment(destinationEnvironment);
    denseTrace_.insert_or_assign(destination, std::move(copy));
}

template <typename NumericalStateT>
std::unique_ptr<AbstractDomain::AbstractState> DenseAbstractInterpretation<
    NumericalStateT>::cloneAbstractState(const ICFGNode* node) const
{
    return state(node).clone();
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::isAbstractStateEquivalent(
    const ICFGNode* node, const AbstractDomain::AbstractState& snapshot) const
{
    DenseState current = state(node);
    DenseState previous = static_cast<const DenseState&>(snapshot);
    alignEnvironments(current, previous);
    return current.isEquivalentTo(previous) ==
           AbstractDomain::CheckResult::True;
}

template <typename NumericalStateT>
std::unique_ptr<AbstractDomain::AbstractState> DenseAbstractInterpretation<
    NumericalStateT>::cloneCycleHeadState(const ICFGCycleWTO* cycle)
{
    return cloneAbstractState(cycle->head()->getICFGNode());
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::widenCycleState(
    const AbstractDomain::AbstractState& previous,
    const AbstractDomain::AbstractState& current, const ICFGCycleWTO* cycle)
{
    DenseState previousDense = static_cast<const DenseState&>(previous);
    DenseState currentDense = static_cast<const DenseState&>(current);
    alignEnvironments(previousDense, currentDense);
    DenseState next = previousDense;
    next.widenWith(currentDense);
    const bool fixpoint =
        next.isEquivalentTo(previousDense) == AbstractDomain::CheckResult::True;
    const ICFGNode* head = cycle->head()->getICFGNode();
    denseTrace_.insert_or_assign(head, std::move(next));
    return fixpoint;
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::narrowCycleState(
    const AbstractDomain::AbstractState& previous,
    const AbstractDomain::AbstractState& current, const ICFGCycleWTO* cycle)
{
    const ICFGNode* head = cycle->head()->getICFGNode();
    if (!shouldApplyNarrowing(head->getFun()))
        return true;
    DenseState previousDense = static_cast<const DenseState&>(previous);
    DenseState currentDense = static_cast<const DenseState&>(current);
    alignEnvironments(previousDense, currentDense);
    // Sparse transfers may materialize a new MemorySSA/cycle facet during the
    // descending phase. Enforce narrowing's generic next <= current contract.
    // The normal descending path already satisfies that contract. Avoid
    // rebuilding and closing a relational meet when the lattice check proves
    // that the meet would be exactly currentDense. False and Unknown retain
    // the original conservative meet.
    if (currentDense.isSubsetOf(previousDense) != AD::CheckResult::True)
        currentDense.meetWith(previousDense);
    DenseState next = previousDense;
    next.narrowWith(currentDense);
    const bool fixpoint =
        next.isEquivalentTo(previousDense) == AbstractDomain::CheckResult::True;
    if (!fixpoint)
        denseTrace_.insert_or_assign(head, std::move(next));
    return fixpoint;
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::ensureVariable(
    DenseState& denseState, AD::Variable variable) const
{
    if (denseState.numerical().environment().contains(variable))
        return;
    denseState.changeEnvironment(denseState.numerical().environment().add(
        {adapter_.declaration(variable)}));
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::assignInterval(
    DenseState& denseState, AD::Variable variable,
    const IntervalValue& interval)
{
    ensureVariable(denseState, variable);
    denseState.numerical().forget(variable);
    constrainInterval(denseState, variable, interval);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::constrainInterval(
    DenseState& denseState, AD::Variable variable,
    const IntervalValue& interval)
{
    if (interval.isBottom())
        return;

    AD::LinearConstraintSet constraints;
    AD::LinearExpression expression(variable);
    if (!interval.lb().is_minus_infinity())
    {
        constraints.push_back(AD::greaterEqual(
            expression,
            AD::LinearExpression(AD::Rational(interval.lb().getIntNumeral()))));
    }
    if (!interval.ub().is_plus_infinity())
    {
        constraints.push_back(AD::lessEqual(
            expression,
            AD::LinearExpression(AD::Rational(interval.ub().getIntNumeral()))));
    }
    denseState.numerical().assumeAll(constraints);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::initializeDomainState(
    const ICFGNode* node)
{
    (void)ensureState(node);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::assignDomainInterval(
    const ICFGNode* node, const SVFVar* target, const IntervalValue& interval)
{
    DenseState& denseState = ensureState(node);
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(target))
    {
        if (adapter_.contains(*value))
        {
            const AD::Variable variable = adapter_.variable(*value);
            if (interval.isBottom() ||
                projectInterval(denseState.numerical().bound(variable))
                    .equals(interval))
                return;
            assignInterval(denseState, variable, interval);
        }
    }
    else if (const auto* object = SVFUtil::dyn_cast<ObjVar>(target))
    {
        if (adapter_.contains(*object))
        {
            const AD::Variable variable = adapter_.contentVariable(*object);
            if (interval.isBottom() ||
                projectInterval(denseState.numerical().bound(variable))
                    .equals(interval))
                return;
            assignInterval(denseState, variable, interval);
        }
    }
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateDomainOnBinary(
    const BinaryOPStmt* binary, const IntervalValue& result)
{
    DenseState& denseState = ensureState(binary->getICFGNode());
    const auto* target = SVFUtil::dyn_cast<ValVar>(binary->getRes());
    if (!target || !adapter_.contains(*target))
        return;
    const AD::Variable targetVariable = adapter_.variable(*target);

    auto operand = [&](const SVFVar* value,
                       AD::LinearExpression& expression) -> bool {
        if (const auto* integer = SVFUtil::dyn_cast<ConstIntValVar>(value))
        {
            expression =
                AD::LinearExpression(AD::Rational(integer->getSExtValue()));
            return true;
        }
        const auto* scalar = SVFUtil::dyn_cast<ValVar>(value);
        if (!scalar || !adapter_.contains(*scalar))
            return false;
        materializeValue(denseState, scalar, binary->getICFGNode());
        ensureVariable(denseState, adapter_.variable(*scalar));
        expression = AD::LinearExpression(adapter_.variable(*scalar));
        return true;
    };

    AD::LinearExpression lhs;
    AD::LinearExpression rhs;
    const bool affine = (binary->getOpcode() == BinaryOPStmt::Add ||
                         binary->getOpcode() == BinaryOPStmt::Sub) &&
                        operand(binary->getOpVar(0), lhs) &&
                        operand(binary->getOpVar(1), rhs);
    if (!affine)
    {
        assignInterval(denseState, targetVariable, result);
        return;
    }

    ensureVariable(denseState, targetVariable);
    denseState.numerical().assign(
        targetVariable,
        binary->getOpcode() == BinaryOPStmt::Add ? lhs + rhs : lhs - rhs);
    constrainInterval(denseState, targetVariable, result);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateDomainCopyValue(
    const ICFGNode* node, const SVFVar* target, const SVFVar* source,
    bool exactMathematicalCopy)
{
    const auto* targetValue = SVFUtil::dyn_cast<ValVar>(target);
    if (!targetValue || !adapter_.contains(*targetValue))
        return;
    DenseState& denseState = ensureState(node);
    const AD::Variable targetVariable = adapter_.variable(*targetValue);
    const AbstractValue result = getAbsValue(targetValue, node);
    if (!result.isInterval())
    {
        if (denseState.numerical().environment().contains(targetVariable))
            denseState.numerical().forget(targetVariable);
        return;
    }

    const auto* sourceValue = SVFUtil::dyn_cast<ValVar>(source);
    if (exactMathematicalCopy && sourceValue && adapter_.contains(*sourceValue))
    {
        materializeValue(denseState, sourceValue, node);
        const AD::Variable sourceVariable = adapter_.variable(*sourceValue);
        ensureVariable(denseState, targetVariable);
        ensureVariable(denseState, sourceVariable);
        denseState.numerical().assign(targetVariable,
                                      AD::LinearExpression(sourceVariable));
        constrainInterval(denseState, targetVariable, result.getInterval());
    }
    else
    {
        assignInterval(denseState, targetVariable, result.getInterval());
    }
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateDomainOnCopy(
    const CopyStmt* copy)
{
    const bool exact = copy->getCopyKind() == CopyStmt::COPYVAL ||
                       copy->getCopyKind() == CopyStmt::SEXT;
    updateDomainCopyValue(copy->getICFGNode(), copy->getLHSVar(),
                          copy->getRHSVar(), exact);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::assignValue(
    DenseState& denseState, AD::Variable variable, const AbstractValue& value)
{
    ensureVariable(denseState, variable);
    if (value.isInterval())
        assignInterval(denseState, variable, value.getInterval());
    else
        denseState.numerical().forget(variable);

    AD::PointeeSet addresses = AD::PointeeSet::bottom();
    for (u32_t address : value.getAddrs())
    {
        const NodeID objectId = address & FlippedAddressMask;
        const auto* object =
            SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(objectId));
        if (object && adapter_.contains(*object))
            addresses.insert(adapter_.location(*object));
    }
    denseState.pointers().assign(variable, std::move(addresses));
    denseState.shapes().assign(variable, value.isInterval());
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::materializeValue(
    DenseState&, const ValVar*, const ICFGNode*)
{
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::forgetValue(
    DenseState& denseState, AD::Variable variable) const
{
    if (!denseState.numerical().environment().contains(variable))
        return;
    denseState.numerical().forget(variable);
    // This helper removes an AE value; it does not model an unknown pointer.
    // PointerMap::forget means address-top and would retain an explicit map
    // entry for every purged sparse scalar.
    denseState.pointers().assign(variable, AD::PointeeSet::bottom());
    denseState.shapes().forget(variable);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::forgetScalarValues(
    DenseState& denseState) const
{
    for (const AD::VariableDeclaration& declaration :
         denseState.numerical().environment().variables())
    {
        if (adapter_.value(declaration.variable))
            forgetValue(denseState, declaration.variable);
    }
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<NumericalStateT>::projectValue(
    const DenseState& denseState, AD::Variable variable) const
{
    if (!denseState.numerical().environment().contains(variable))
        return AbstractValue(IntervalValue::top());
    if (!denseState.shapes().isDefined(variable))
        return AbstractValue();
    AbstractValue result;
    if (denseState.shapes().hasNumeric(variable))
        result.interval =
            projectInterval(denseState.numerical().bound(variable));
    const AD::PointeeSet addresses = denseState.pointers().pointeesOf(variable);
    if (addresses.isTop())
    {
        result.getAddrs().insert(BlackHoleObjAddr);
        return result;
    }
    for (AD::Location location : addresses.locations())
    {
        const ObjVar& object = adapter_.object(location);
        result.getAddrs().insert(
            AddressValue::getVirtualMemAddress(object.getId()));
    }
    return result;
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<NumericalStateT>::getAbsValue(
    const ValVar* var, const ICFGNode* node)
{
    if (const auto* integer = SVFUtil::dyn_cast<ConstIntValVar>(var))
        return IntervalValue(integer->getSExtValue());
    if (!adapter_.contains(*var))
        return IntervalValue::top();

    DenseState& denseState = ensureState(node);
    const AD::Variable variable = adapter_.variable(*var);
    if (!denseState.shapes().isDefined(variable))
        assignValue(denseState, variable, IntervalValue::top());
    AbstractValue value = projectValue(denseState, variable);
    if (var->isPointer())
        value.interval = IntervalValue::bottom();
    return value;
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<NumericalStateT>::getAbsValue(
    const ObjVar* var, const ICFGNode* node)
{
    if (!adapter_.contains(*var))
        return AbstractValue();
    DenseState& denseState = ensureState(node);
    const AD::Variable content = adapter_.contentVariable(*var);
    if (!denseState.shapes().isDefined(content))
        assignValue(denseState, content, AbstractValue());
    return projectValue(denseState, content);
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<NumericalStateT>::getAbsValue(
    const SVFVar* var, const ICFGNode* node)
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        return getAbsValue(object, node);
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(var))
        return getAbsValue(value, node);
    throw std::invalid_argument("unsupported SVF variable kind");
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::hasAbsValue(
    const ValVar* var, const ICFGNode* node) const
{
    if (SVFUtil::isa<ConstIntValVar>(var))
        return true;
    if (denseTrace_.count(node) == 0 || !adapter_.contains(*var))
        return false;
    return state(node).shapes().isDefined(adapter_.variable(*var));
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::hasAbsValue(
    const ObjVar* var, const ICFGNode* node) const
{
    if (denseTrace_.count(node) == 0 || !adapter_.contains(*var))
        return false;
    return state(node).shapes().isDefined(adapter_.contentVariable(*var));
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::hasAbsValue(
    const SVFVar* var, const ICFGNode* node) const
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        return hasAbsValue(object, node);
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(var))
        return hasAbsValue(value, node);
    return false;
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateAbsValue(
    const ValVar* var, const AbstractValue& value, const ICFGNode* node)
{
    if (adapter_.contains(*var))
        assignValue(ensureState(node), adapter_.variable(*var), value);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateAbsValue(
    const ObjVar* var, const AbstractValue& value, const ICFGNode* node)
{
    if (adapter_.contains(*var))
        assignValue(ensureState(node), adapter_.contentVariable(*var), value);
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<NumericalStateT>::getMemoryValue(
    u32_t address, const ICFGNode* node)
{
    const auto* object = SVFUtil::dyn_cast<ObjVar>(
        svfir->getGNode(objectIdFromAddress(address)));
    return object ? getAbsValue(object, node) : AbstractValue();
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::hasMemoryValue(
    u32_t address, const ICFGNode* node) const
{
    const auto* object = SVFUtil::dyn_cast<ObjVar>(
        svfir->getGNode(objectIdFromAddress(address)));
    return object && hasAbsValue(object, node);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateMemoryValue(
    u32_t address, const AbstractValue& value, const ICFGNode* node)
{
    const auto* object = SVFUtil::dyn_cast<ObjVar>(
        svfir->getGNode(objectIdFromAddress(address)));
    if (object)
        updateAbsValue(object, value, node);
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::markFreedMemory(
    u32_t address, const ICFGNode* node)
{
    const auto* object = SVFUtil::dyn_cast<ObjVar>(
        svfir->getGNode(objectIdFromAddress(address)));
    if (object && adapter_.contains(*object))
        ensureState(node).lifetimes().release(adapter_.location(*object));
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::isFreedMemory(
    u32_t address, const ICFGNode* node) const
{
    if (denseTrace_.count(node) == 0)
        return false;
    const auto* object = SVFUtil::dyn_cast<ObjVar>(
        svfir->getGNode(objectIdFromAddress(address)));
    return object && adapter_.contains(*object) &&
           state(node).lifetimes().mayBeFreed(adapter_.location(*object));
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::updateAbsValue(
    const SVFVar* var, const AbstractValue& value, const ICFGNode* node)
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        updateAbsValue(object, value, node);
    else if (const auto* scalar = SVFUtil::dyn_cast<ValVar>(var))
        updateAbsValue(scalar, value, node);
    else
        throw std::invalid_argument("unsupported SVF variable kind");
}

template <typename NumericalStateT>
AbstractValue DenseAbstractInterpretation<NumericalStateT>::loadValue(
    const ValVar* pointer, const ICFGNode* node)
{
    if (!adapter_.contains(*pointer))
        return AbstractInterpretation::loadValue(pointer, node);
    DenseState& denseState = ensureState(node);
    materializeValue(denseState, pointer, node);
    const AD::PointeeSet pointees =
        denseState.pointers().pointeesOf(adapter_.variable(*pointer));
    if (pointees.isTop())
        return AbstractValue(IntervalValue::top());

    AbstractValue result;
    for (AD::Location location : pointees.locations())
    {
        if (denseState.lifetimes().mayBeFreed(location))
        {
            result.join_with(AbstractValue(IntervalValue::top()));
            continue;
        }
        if (denseState.memoryLayout().contains(location))
        {
            result.join_with(projectValue(
                denseState, denseState.memoryLayout().contentOf(location)));
        }
    }
    return result;
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::storeValue(
    const ValVar* pointer, const AbstractValue& value, const ICFGNode* node)
{
    if (!adapter_.contains(*pointer))
    {
        AbstractInterpretation::storeValue(pointer, value, node);
        return;
    }
    DenseState& denseState = ensureState(node);
    materializeValue(denseState, pointer, node);
    const AD::PointeeSet pointees =
        denseState.pointers().pointeesOf(adapter_.variable(*pointer));
    const bool strong = pointees.isSingleton();
    auto write = [&](AD::Location location) {
        if (!denseState.memoryLayout().contains(location))
            return;
        const AD::Variable content =
            denseState.memoryLayout().contentOf(location);
        if (strong)
        {
            assignValue(denseState, content, value);
            return;
        }
        AbstractValue joined = projectValue(denseState, content);
        joined.join_with(value);
        assignValue(denseState, content, joined);
    };

    if (pointees.isTop())
    {
        for (const auto& [location, content] :
             denseState.memoryLayout().cells())
        {
            (void)content;
            write(location);
        }
    }
    else
    {
        for (AD::Location location : pointees.locations())
            write(location);
    }
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::assumeBranch(
    const IntraCFGEdge* edge, DenseState& denseState)
{
    const SVFVar* condition = edge->getCondition();
    if (!condition || condition->getInEdges().empty())
        return;
    const auto* comparison =
        SVFUtil::dyn_cast<CmpStmt>(*condition->getInEdges().begin());
    if (!comparison)
    {
        const auto* value = SVFUtil::dyn_cast<ValVar>(condition);
        if (!value || !adapter_.contains(*value))
            return;
        materializeValue(denseState, value, edge->getSrcNode());
        ensureVariable(denseState, adapter_.variable(*value));
        denseState.assume(AD::equal(
            AD::LinearExpression(adapter_.variable(*value)),
            AD::LinearExpression(AD::Rational(edge->getSuccessorCondValue()))));
        return;
    }

    AD::ConstraintKind kind;
    if (!constraintKind(comparison->getPredicate(), kind))
        return;
    if (edge->getSuccessorCondValue() == 0)
        kind = negatePredicate(kind);

    auto operand = [&](const SVFVar* variable,
                       AD::LinearExpression& expression) -> bool {
        if (const auto* integer = SVFUtil::dyn_cast<ConstIntValVar>(variable))
        {
            expression =
                AD::LinearExpression(AD::Rational(integer->getSExtValue()));
            return true;
        }
        const auto* value = SVFUtil::dyn_cast<ValVar>(variable);
        if (!value || !adapter_.contains(*value))
            return false;
        materializeValue(denseState, value, edge->getSrcNode());
        ensureVariable(denseState, adapter_.variable(*value));
        expression = AD::LinearExpression(adapter_.variable(*value));
        return true;
    };

    AD::LinearExpression lhs;
    AD::LinearExpression rhs;
    if (!operand(comparison->getOpVar(0), lhs) ||
        !operand(comparison->getOpVar(1), rhs))
        return;
    denseState.assume(AD::LinearConstraint(lhs - rhs, kind));
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    DenseState merged = bottomState(node);
    bool hasFeasiblePredecessor = false;

    for (const ICFGEdge* edge : orderedIncomingEdges(node))
    {
        const ICFGNode* predecessor = edge->getSrcNode();
        if (denseTrace_.count(predecessor) == 0)
            continue;

        bool shouldMerge = false;
        const IntraCFGEdge* conditional = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
        if (conditional)
            shouldMerge = true;
        else if (SVFUtil::isa<CallCFGEdge>(edge))
        {
            shouldMerge = true;
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            shouldMerge = Options::HandleRecur() == TOP;
            if (!shouldMerge)
            {
                const auto* returnSite = SVFUtil::dyn_cast<RetICFGNode>(node);
                shouldMerge =
                    returnSite &&
                    denseTrace_.count(returnSite->getCallICFGNode()) != 0;
            }
        }
        if (!shouldMerge)
            continue;

        DenseState source = state(predecessor);
        const AD::VariableEnvironment& destinationEnvironment =
            adapter_.environment(node->getFun());
        if (source.numerical().environment() != destinationEnvironment)
            source.changeEnvironment(destinationEnvironment);
        if (conditional && conditional->getCondition())
        {
            assumeBranch(conditional, source);
            collectBranchRefinement(conditional, source);
        }
        if (source.isBottom())
            continue;

        // Branch refinement can materialize a condition variable that is not
        // present in the destination function's precomputed environment (for
        // example, a value returned across a call edge).  Join over the union
        // environment just as the other fixpoint comparison paths do.
        alignEnvironments(merged, source);
        merged.joinWith(source);
        hasFeasiblePredecessor = true;
    }

    if (!hasFeasiblePredecessor)
        return false;
    denseTrace_.insert_or_assign(node, std::move(merged));
    return true;
}

template <typename NumericalStateT>
void DenseAbstractInterpretation<NumericalStateT>::recordBranchRefinement(
    NodeID objectId, const IntervalValue& narrowed,
    AD::AbstractState& abstractState, const ICFGNode*, const ICFGNode*)
{
    const auto* object =
        SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(objectId));
    if (!object || !adapter_.contains(*object))
        return;

    DenseState& denseState = static_cast<DenseState&>(abstractState);
    const AD::Variable content = adapter_.contentVariable(*object);
    AbstractValue current = projectValue(denseState, content);
    if (!current.isInterval())
        return;
    IntervalValue refined = current.getInterval();
    refined.meet_with(narrowed);
    assignValue(denseState, content, AbstractValue(refined));
}

template <typename NumericalStateT>
bool DenseAbstractInterpretation<NumericalStateT>::isBranchEdgeFeasibleAt(
    const IntraCFGEdge* edge, const ICFGNode* predecessor)
{
    DenseState candidate = state(predecessor);
    assumeBranch(edge, candidate);
    return !candidate.isBottom();
}

#ifndef SVF_DENSE_AE_SUPPRESS_EXPLICIT_INSTANTIATIONS
template class DenseAbstractInterpretation<AD::BoxState>;
#endif

} // namespace SVF
