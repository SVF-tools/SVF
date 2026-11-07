//===- AbstractStateManager.cpp -- AE state-access implementations ---//
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
//===----------------------------------------------------------------------===//
//
// State-access bodies factored out of AbstractInterpretation.cpp /
// SparseAbstractInterpretation.cpp.  Class declarations stay in their
// respective headers; this file just hosts the implementations of the
// methods that used to live on the (now-folded) AbstractStateManager —
// trace lookup, value get/has/update, GEP / load / store helpers, and
// def/use queries — for dense and semi-sparse.  Full-sparse stubs and
// SVFG-backed overrides live in SparseAbstractInterpretation.cpp.
//

#include "AE/Svfexe/AbstractInterpretation.h"

#include "SVFIR/SVFIR.h"
#include "Util/Options.h"

#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace SVF
{

namespace
{

s64_t finiteEndpoint(const SVF::Bound& bound, s64_t fallback)
{
    if (!bound.isFinite())
        return fallback;
    std::int64_t result = 0;
    return bound.value().tryToInt64(result) ? result : fallback;
}

SVF::Interval finiteInterval(s64_t lower, s64_t upper)
{
    return SVF::Interval::closed(SVF::Rational(lower), SVF::Rational(upper));
}

bool constantInterval(const ValVar* value, SVF::Interval& interval)
{
    if (const auto* integer = SVFUtil::dyn_cast<ConstIntValVar>(value))
    {
        interval =
            SVF::Interval::singleton(SVF::Rational(integer->getSExtValue()));
        return true;
    }
    if (const auto* floating = SVFUtil::dyn_cast<ConstFPValVar>(value))
    {
        interval = SVFIRAdapter::floatingConstant(floating->getFPValue());
        return true;
    }
    return false;
}

} // namespace

void AbstractInterpretation::updateInterval(const SVFVar* variable,
        const SVF::Interval& interval,
        const ICFGNode* node)
{
    updateValue(variable, interval, SVF::AddressSet::bottom(), node);
}

void AbstractInterpretation::updateAddressSet(const SVFVar* variable,
        const SVF::AddressSet& addresses,
        const ICFGNode* node)
{
    updateValue(variable, SVF::Interval::bottom(), addresses, node);
}

SVF::Interval AbstractInterpretation::getGepElementIndex(const GepStmt* gep)
{
    const ICFGNode* node = gep->getICFGNode();
    if (gep->isConstantOffset())
        return SVF::Interval::singleton(
                   SVF::Rational(static_cast<s64_t>(gep->accumulateConstantOffset())));

    SVF::Interval result = SVF::Interval::singleton(SVF::Rational());
    for (int index =
                static_cast<int>(gep->getOffsetVarAndGepTypePairVec().size()) - 1;
            index >= 0; --index)
    {
        const ValVar* variable =
            gep->getOffsetVarAndGepTypePairVec()[index].first;
        const SVFType* type =
            gep->getOffsetVarAndGepTypePairVec()[index].second;

        s64_t lower = 0;
        s64_t upper = 0;
        if (const auto* integer = SVFUtil::dyn_cast<ConstIntValVar>(variable))
        {
            lower = upper = integer->getSExtValue();
        }
        else
        {
            const SVF::Interval value = getInterval(variable, node);
            // Original treats an uninitialized index as zero, not as an
            // unknown range spanning every field. Keep this AE policy apart
            // from the domain's Top/Bottom and finite-endpoint semantics.
            if (!value.isBottom())
            {
                lower = finiteEndpoint(value.lower(), 0);
                upper = finiteEndpoint(value.upper(), Options::MaxFieldLimit());
            }
        }

        if (SVFUtil::isa<SVFPointerType>(type))
        {
            const u32_t elements = gep->getAccessPath().getElementNum(
                                       gep->getAccessPath().gepSrcPointeeType());
            lower = (double)Options::MaxFieldLimit() / elements < lower
                    ? Options::MaxFieldLimit()
                    : lower * elements;
            upper = (double)Options::MaxFieldLimit() / elements < upper
                    ? Options::MaxFieldLimit()
                    : upper * elements;
        }
        else if (Options::ModelArrays())
        {
            const std::vector<u32_t>& flattened =
                PAG::getPAG()->getTypeInfo(type)->getFlattenedElemIdxVec();
            if (flattened.empty() ||
                    upper >= static_cast<APOffset>(flattened.size()) || lower < 0)
            {
                lower = upper = 0;
            }
            else
            {
                lower = PAG::getPAG()->getFlattenedElemIdx(type, lower);
                upper = PAG::getPAG()->getFlattenedElemIdx(type, upper);
            }
        }
        else
        {
            lower = upper = 0;
        }
        result = SVF::add(result, finiteInterval(lower, upper));
    }
    result.meetWith(
        finiteInterval(0, static_cast<s64_t>(Options::MaxFieldLimit())));
    return result.isBottom() ? SVF::Interval::singleton(SVF::Rational()) : result;
}

SVF::Interval AbstractInterpretation::getGepByteOffset(const GepStmt* gep)
{
    const ICFGNode* node = gep->getICFGNode();
    if (gep->isConstantOffset())
        return SVF::Interval::singleton(SVF::Rational(
                                            static_cast<s64_t>(gep->accumulateConstantByteOffset())));

    SVF::Interval result = SVF::Interval::singleton(SVF::Rational());
    for (int index =
                static_cast<int>(gep->getOffsetVarAndGepTypePairVec().size()) - 1;
            index >= 0; --index)
    {
        const ValVar* variable =
            gep->getOffsetVarAndGepTypePairVec()[index].first;
        const SVFType* type =
            gep->getOffsetVarAndGepTypePairVec()[index].second;

        if (SVFUtil::isa<SVFArrayType>(type) ||
                SVFUtil::isa<SVFPointerType>(type))
        {
            u32_t elementSize = 1;
            if (const auto* array = SVFUtil::dyn_cast<SVFArrayType>(type))
                elementSize = array->getTypeOfElement()->getByteSize();
            else
                elementSize =
                    gep->getAccessPath().gepSrcPointeeType()->getByteSize();

            s64_t lower = 0;
            s64_t upper = 0;
            if (const auto* integer =
                        SVFUtil::dyn_cast<ConstIntValVar>(variable))
            {
                lower = upper = integer->getSExtValue();
            }
            else
            {
                const SVF::Interval value = getInterval(variable, node);
                lower = finiteEndpoint(value.lower(), 0);
                upper = finiteEndpoint(value.upper(),
                                       Options::MaxFieldLimit());
            }
            lower = std::max<s64_t>(0, lower);
            upper = std::max<s64_t>(0, upper);
            lower = (double)Options::MaxFieldLimit() / elementSize >= lower
                    ? lower * elementSize
                    : Options::MaxFieldLimit();
            upper = (double)Options::MaxFieldLimit() / elementSize >= upper
                    ? upper * elementSize
                    : Options::MaxFieldLimit();
            result = SVF::add(result, finiteInterval(lower, upper));
        }
        else if (const auto* structure = SVFUtil::dyn_cast<SVFStructType>(type))
        {
            const s64_t offset =
                gep->getAccessPath().getStructFieldOffset(variable, structure);
            result =
                SVF::add(result, SVF::Interval::singleton(SVF::Rational(offset)));
        }
        else
        {
            assert(false && "GEP type pair must be array, pointer, or structure");
            std::abort();
        }
    }
    return result;
}

SVF::AddressSet AbstractInterpretation::getGepObjAddrs(
    const ValVar* pointer, const SVF::Interval& offset, const ICFGNode* node)
{
    const ICFGNode* definition = pointer->getICFGNode();
    const SVF::AddressSet bases = getAddressSet(pointer, definition);
    SVF::AddressSet result = SVF::AddressSet::bottom();
    if (offset.isBottom() || !bases.isFinite())
        return result;
    const auto endpoint = [](const SVF::Bound& bound)
    {
        return bound.isMinusInfinity() ? std::numeric_limits<s64_t>::min() :
               bound.isPlusInfinity() ? std::numeric_limits<s64_t>::max() :
               bound.value().toInt64();
    };
    const APOffset lower = std::min<s64_t>(endpoint(offset.lower()), Options::MaxFieldLimit());
    const APOffset upper = std::min<s64_t>(endpoint(offset.upper()), Options::MaxFieldLimit());
    for (APOffset index = lower; index <= upper; ++index)
        for (SVF::Location location : bases)
        {
            const ObjVar* object = objectAt(location);
            if (!object || svfir->isNullOrBlkObj(object->getId()))
                continue;
            const ObjVar* base = object;
            if (isFreedMemory(location, definition))
                base = SVFUtil::cast<ObjVar>(svfir->getGNode(svfir->getBlackHoleNode()));
            const NodeID field = svfir->getGepObjVar(base->getId(), index);
            result.insert(locationOf(SVFUtil::cast<ObjVar>(svfir->getGNode(field))));
        }
    return result;
}

AbstractInterpretation::AllocationSizeBounds
AbstractInterpretation::getAllocaInstByteSize(const AddrStmt* address)
{
    const ICFGNode* node = address->getICFGNode();
    const auto* object = SVFUtil::dyn_cast<ObjVar>(address->getRHSVar());
    if (!object)
    {
        assert(false && "Addr rhs value is not ObjVar");
        std::abort();
    }
    const BaseObjVar* base = svfir->getBaseObject(object->getId());
    if (!base)
        return {0, Options::MaxFieldLimit()};
    if (base->isConstantByteSize())
    {
        const u32_t size = base->getByteSizeOfObj();
        return {size, size};
    }

    u64_t lower = address->getArrSize().empty() ? 0 : 1;
    u64_t upper = 1;
    const auto multiply = [](u64_t product, u64_t factor)
    {
        return factor != 0 && product > Options::MaxFieldLimit() / factor
               ? Options::MaxFieldLimit() : product * factor;
    };
    for (const SVFVar* value : address->getArrSize())
    {
        const SVF::Interval size = getInterval(value, node);
        const u64_t lb = size.isBottom() ? 0 : static_cast<u64_t>(std::clamp<s64_t>(
                finiteEndpoint(size.lower(), 0), 0, Options::MaxFieldLimit()));
        const u64_t ub = size.isBottom() ? Options::MaxFieldLimit() :
                         static_cast<u64_t>(std::clamp<s64_t>(
                                                finiteEndpoint(size.upper(), Options::MaxFieldLimit()),
                                                0, Options::MaxFieldLimit()));
        lower = multiply(lower, lb);
        upper = multiply(upper, ub);
    }
    return {static_cast<u32_t>(std::min(lower, upper)), static_cast<u32_t>(upper)};
}

SVF::Interval AbstractInterpretation::unknownInterval(const SVFVar* var) const
{
    // The upstream Box interpreter uses mathematical intervals for unknown
    // numerical inputs. Machine-range guards belong to relational execution.
    return SVF::Interval::top();
}

const SVF::AbstractDomain& AbstractInterpretation::
getAbstractState(const ICFGNode* node) const
{
    return state(node);
}

bool AbstractInterpretation::hasAbsState(const ICFGNode* node) const
{
    return abstractTrace_.count(node) != 0;
}

SVF::Location AbstractInterpretation::locationOf(const ObjVar* object) const
{
    return object ? adapter_.location(*object) : SVF::Location::null();
}

const ObjVar* AbstractInterpretation::objectAt(SVF::Location location) const
{
    return location.isNull() ? nullptr : &adapter_.object(location);
}

AbstractInterpretation::State AbstractInterpretation::topState()
const
{
    return State(makeNumericalDomain(false), adapter_.memoryLayout(), true);
}

AbstractInterpretation::State AbstractInterpretation::
bottomState() const
{
    return State(makeNumericalDomain(true), adapter_.memoryLayout(), true);
}

std::unique_ptr<SVF::NumericalDomain>
AbstractInterpretation::makeNumericalDomain(bool bottom) const
{
    return std::make_unique<SVF::BoxDomain>(bottom ? SVF::BoxDomain::bottom() : SVF::BoxDomain::top());
}

AbstractInterpretation::State& AbstractInterpretation::
ensureState(const ICFGNode* node)
{

    auto iterator = abstractTrace_.find(node);
    if (iterator == abstractTrace_.end())
        iterator = abstractTrace_.emplace(node, topState()).first;
    return iterator->second;
}

const AbstractInterpretation::State& AbstractInterpretation::
state(const ICFGNode* node) const
{

    const auto iterator = abstractTrace_.find(node);
    if (iterator == abstractTrace_.end())
    {
        assert(false && "no dense abstract state for ICFG node");
        std::abort();
    }
    return iterator->second;
}

void AbstractInterpretation::resetAbstractState(const ICFGNode* node)
{
    abstractTrace_.insert_or_assign(node, topState());
    definedValues_[node].clear();
}

void AbstractInterpretation::copyAbstractState(const ICFGNode* source,
        const ICFGNode* destination)
{
    abstractTrace_.insert_or_assign(destination, state(source));
    definedValues_[destination] = definedValues_[source];
}

std::unique_ptr<SVF::AbstractDomain> AbstractInterpretation::
cloneAbstractState(const ICFGNode* node) const
{
    return state(node).clone();
}

bool AbstractInterpretation::equivalentValues(const State& left, const State& right) const
{
    return left.hasSameValues(right);
}

void AbstractInterpretation::assignInterval(State& denseState,
        SVF::Variable variable,
        const SVF::Interval& interval)
{
    denseState.setInterval(variable, interval);
}

void AbstractInterpretation::constrainInterval(
    State& denseState, SVF::Variable variable,
    const SVF::Interval& interval) const
{
    if (interval.isBottom())
        return;

    // Constraints refine an initialized observation. Keep Top observations
    // defined even when they need no Box payload slot.
    if (denseState.interval(variable).isBottom())
        denseState.setInterval(variable, SVF::Interval::top());

    SVF::LinearConstraintSet constraints;
    SVF::LinearExpression expression(variable);
    if (interval.lower().isFinite())
    {
        constraints.emplace_back(
            expression - SVF::LinearExpression(interval.lower().value()),
            interval.lower().isStrict() ? SVF::ConstraintKind::GreaterThan
            : SVF::ConstraintKind::GreaterEqual);
    }
    if (interval.upper().isFinite())
    {
        constraints.emplace_back(
            expression - SVF::LinearExpression(interval.upper().value()),
            interval.upper().isStrict() ? SVF::ConstraintKind::LessThan
            : SVF::ConstraintKind::LessEqual);
    }
    denseState.numerical().assumeAll(constraints);
}

void AbstractInterpretation::assignValue(State& denseState,
        SVF::Variable variable,
        const SVF::Interval& interval,
        const SVF::AddressSet& addresses)
{
    // SVF's synthetic extractvalue/extractelement edges can give even an
    // integer-typed SSA value an address facet. Preserve both facets through
    // assignment and a later store, just as for pointer-typed unknown values.
    assignInterval(denseState, variable, interval);
    denseState.setAddressSet(variable, addresses);
}

void AbstractInterpretation::assignMemoryValue(
    State& denseState, SVF::Variable content,
    const SVF::Interval& interval, const SVF::AddressSet& addresses)
{
    assignInterval(denseState, content, interval);
    denseState.setAddressSet(content, addresses);
}

SVF::Variable AbstractInterpretation::memoryVariable(
    const ObjVar& object, const State& denseState) const
{
    // Original's getIDFromAddr redirects freed-object accesses to its
    // BlackHole memory cell. This is an interpreter policy, not Address Top.
    if (denseState.lifetimes().mayBeFreed(adapter_.location(object)))
        return adapter_.contentVariable(*SVFUtil::cast<ObjVar>(
                                            svfir->getGNode(svfir->getBlackHoleNode())));
    return adapter_.contentVariable(object);
}

void AbstractInterpretation::forgetValue(State& denseState,
        SVF::Variable variable) const
{
    denseState.resetValue(variable);
}

SVF::Interval AbstractInterpretation::getInterval(const ValVar* var,
        const ICFGNode* node)
{
    const SVF::Interval result = getDefinedInterval(var, node);
    // Upstream reads the stored interval even when an uninitialized
    // alternative is present; initialization remains a separate detector fact.
    return result;
}

SVF::Interval AbstractInterpretation::getDefinedInterval(const ValVar* var,
        const ICFGNode* node)
{
    SVF::Interval constant;
    if (constantInterval(var, constant))
        return constant;
    // SVFIR also uses its canonical unknown pointer as the numeric source of
    // nondeterministic external-input stores (STORE_TOP annotations).
    if (var->getId() == svfir->getBlkPtr() ||
            SVFUtil::isa<BlackHoleValVar>(var))
        return SVF::Interval::top();
    if (!adapter_.contains(*var))
        return SVF::Interval::top();

    State& denseState = ensureState(node);
    const SVF::Variable variable = adapter_.variable(*var);
    if (!denseState.hasValue(variable) && !definedValues_[node].test(var->getId()))
    {
        assignValue(denseState, variable,
                    var->isPointer() ? SVF::Interval::bottom() : SVF::Interval::top(),
                    var->isPointer() ? blackHoleAddressSet() : SVF::AddressSet::bottom());
        definedValues_[node].set(var->getId());
    }
    // Global initializers can refer to a function/global before its AddrStmt.
    // This is an unresolved symbolic value, not an uninitialized memory load.
    if ((SVFUtil::isa<FunValVar>(var) || SVFUtil::isa<GlobalValVar>(var)) &&
            !denseState.hasValue(variable))
        return SVF::Interval::top();
    return denseState.interval(variable);
}

SVF::Interval AbstractInterpretation::getInterval(const ObjVar* var,
        const ICFGNode* node)
{
    const State& denseState = ensureState(node);
    const SVF::Variable content = memoryVariable(*var, denseState);
    return denseState.interval(content);
}

SVF::Interval AbstractInterpretation::getInterval(const SVFVar* var,
        const ICFGNode* node)
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        return getInterval(object, node);
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(var))
        return getInterval(value, node);
    assert(false && "unsupported SVF variable kind");
    std::abort();
}

SVF::AddressSet AbstractInterpretation::getAddressSet(const ValVar* var,
        const ICFGNode* node)
{
    if (var->getId() == IRGraph::NullPtr ||
            SVFUtil::isa<ConstNullPtrValVar>(var))
        return SVF::AddressSet::singleton(SVF::Location::null());
    if (var->getId() == svfir->getBlkPtr() ||
            SVFUtil::isa<BlackHoleValVar>(var))
        return blackHoleAddressSet();
    if (!adapter_.contains(*var))
        return SVF::AddressSet::bottom();
    State& denseState = ensureState(node);
    const SVF::Variable variable = adapter_.variable(*var);
    if (!denseState.hasValue(variable) && !definedValues_[node].test(var->getId()))
    {
        assignValue(denseState, variable,
                    var->isPointer() ? SVF::Interval::bottom() : SVF::Interval::top(),
                    var->isPointer() ? blackHoleAddressSet() : SVF::AddressSet::bottom());
        definedValues_[node].set(var->getId());
    }
    return denseState.addressSet(variable);
}

SVF::AddressSet AbstractInterpretation::getAddressSet(const ObjVar* var,
        const ICFGNode* node)
{
    const State& denseState = ensureState(node);
    const SVF::Variable content = memoryVariable(*var, denseState);
    return denseState.addressSet(content);
}

SVF::AddressSet AbstractInterpretation::getAddressSet(const SVFVar* var,
        const ICFGNode* node)
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        return getAddressSet(object, node);
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(var))
        return getAddressSet(value, node);
    assert(false && "unsupported SVF variable kind");
    std::abort();
}

bool AbstractInterpretation::hasAbsValue(const ValVar* var,
        const ICFGNode* node) const
{
    if (SVFUtil::isa<ConstIntValVar>(var) ||
            SVFUtil::isa<ConstFPValVar>(var))
        return true;
    const auto found = abstractTrace_.find(node);
    const auto defined = definedValues_.find(node);
    return found != abstractTrace_.end() && adapter_.contains(*var) &&
           (found->second.hasValue(adapter_.variable(*var)) ||
            (defined != definedValues_.end() && defined->second.test(var->getId())));
}

bool AbstractInterpretation::hasAbsValue(const ObjVar* var,
        const ICFGNode* node) const
{
    const auto stateIterator = abstractTrace_.find(node);
    if (stateIterator == abstractTrace_.end())
        return false;
    // Defined Top is a real incoming definition even without a payload slot.
    // Inspect both guards because aggregate cells can carry either facet.
    const SVF::Variable content = adapter_.contentVariable(*var);
    return stateIterator->second.hasValue(content);
}

bool AbstractInterpretation::hasAbsValue(const SVFVar* var,
        const ICFGNode* node) const
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        return hasAbsValue(object, node);
    if (const auto* value = SVFUtil::dyn_cast<ValVar>(var))
        return hasAbsValue(value, node);
    return false;
}

bool AbstractInterpretation::numericalValueMayBeUninitialized(
    const ValVar* var, const ICFGNode* node) const
{
    if (!var || !adapter_.contains(*var))
        return true;
    const auto stateIterator = abstractTrace_.find(node);
    return stateIterator == abstractTrace_.end() ||
           stateIterator->second.numericalMayBeUninitialized(
               adapter_.variable(*var));
}

void AbstractInterpretation::updateValue(const ValVar* var,
        const SVF::Interval& interval,
        const SVF::AddressSet& addresses,
        const ICFGNode* node)
{
    definedValues_[node].set(var->getId());
    if (adapter_.contains(*var))
        assignValue(ensureState(node), adapter_.variable(*var), interval,
                    addresses);
}

void AbstractInterpretation::addUninitializedNumericalAlternative(
    const ValVar* var, const ICFGNode* node)
{
    if (var && adapter_.contains(*var))
        ensureState(node).addUninitializedNumericalAlternative(
            adapter_.variable(*var));
}

void AbstractInterpretation::updateValue(const ObjVar* var,
        const SVF::Interval& interval,
        const SVF::AddressSet& addresses,
        const ICFGNode* node)
{
    State& denseState = ensureState(node);
    assignMemoryValue(denseState, memoryVariable(*var, denseState), interval,
                      addresses);
}

SVF::Interval AbstractInterpretation::getMemoryInterval(
    SVF::Location location, const ICFGNode* node)
{
    if (location.isNull())
        return SVF::Interval::bottom();
    return getInterval(&adapter_.object(location), node);
}

SVF::AddressSet AbstractInterpretation::getMemoryAddressSet(
    SVF::Location location, const ICFGNode* node)
{
    if (location.isNull())
        return SVF::AddressSet::bottom();
    return getAddressSet(&adapter_.object(location), node);
}

bool AbstractInterpretation::hasMemoryValue(SVF::Location location,
        const ICFGNode* node) const
{
    return !location.isNull() && hasAbsValue(&adapter_.object(location), node);
}

void AbstractInterpretation::updateMemoryValue(
    SVF::Location location, const SVF::Interval& interval,
    const SVF::AddressSet& addresses, const ICFGNode* node)
{
    if (!location.isNull())
        updateValue(&adapter_.object(location), interval, addresses, node);
}

void AbstractInterpretation::markFreedMemory(SVF::Location location,
        const ICFGNode* node)
{
    if (!location.isNull())
        ensureState(node).lifetimes().release(location);
}

bool AbstractInterpretation::isFreedMemory(SVF::Location location,
        const ICFGNode* node) const
{
    if (abstractTrace_.count(node) == 0 || location.isNull())
        return false;
    return state(node).lifetimes().mayBeFreed(location);
}

void AbstractInterpretation::updateValue(const SVFVar* var,
        const SVF::Interval& interval,
        const SVF::AddressSet& addresses,
        const ICFGNode* node)
{
    if (const auto* object = SVFUtil::dyn_cast<ObjVar>(var))
        updateValue(object, interval, addresses, node);
    else if (const auto* scalar = SVFUtil::dyn_cast<ValVar>(var))
        updateValue(scalar, interval, addresses, node);
    else
    {
        assert(false && "unsupported SVF variable kind");
        std::abort();
    }
}

void AbstractInterpretation::loadValue(const ValVar* pointer,
                                       SVF::Interval& interval,
                                       SVF::AddressSet& addresses,
                                       bool& numericalMayBeUninitialized,
                                       const ICFGNode* node)
{
    numericalMayBeUninitialized = false;
    if (!adapter_.contains(*pointer))
    {
        interval = SVF::Interval::bottom();
        addresses = SVF::AddressSet::bottom();
        const SVF::AddressSet pointees = getAddressSet(pointer, node);
        if (pointees.hasUnknownObject())
        {

            interval = SVF::Interval::top();
            addresses = SVF::AddressSet::top();
            return;
        }
        for (SVF::Location location : pointees)
        {
            interval.joinWith(getMemoryInterval(location, node));
            addresses.joinWith(getMemoryAddressSet(location, node));
        }
        return;
    }
    State& denseState = ensureState(node);
    const SVF::AddressSet pointees = getAddressSet(pointer, node);
    if (pointees.hasUnknownObject())
    {

        interval = SVF::Interval::top();
        addresses = SVF::AddressSet::top();
        return;
    }

    interval = SVF::Interval::bottom();
    addresses = SVF::AddressSet::bottom();
    for (SVF::Location location : pointees.locations())
    {
        // getInterval/getAddressSet implement Original's freed-cell routing;
        // lifetime alone must not inject Top into value propagation.
        if (denseState.memoryLayout().contains(location))
        {
            const ObjVar* object = objectAt(location);
            if (!object || svfir->isNullOrBlkObj(object->getId()))
                continue;
            const SVF::Variable content = memoryVariable(*object, denseState);
            numericalMayBeUninitialized |=
                denseState.numericalMayBeUninitialized(content);
            interval.joinWith(denseState.interval(content));
            addresses.joinWith(denseState.addressSet(content));
        }
    }
}

bool AbstractInterpretation::canStrongStore(
    const SVF::AddressSet& pointees, const ObjVar& object, const State& state)
{
    const BaseObjVar* base = svfir->getBaseObject(object.getId());
    {
        const NodeID id = object.getId();
        const auto* pta = preAnalysis->getPointerAnalysis();
        return pointees.isSingleton() && base && !svfir->isNullOrBlkObj(id) &&
               !pta->isHeapMemObj(id) && !base->isFieldInsensitive() &&
               !pta->isLocalVarInRecursiveFun(id) &&
               (!pta->isArrayMemObj(id) || svfir->isGepObjVar(id)) &&
               base->getType()->getTypeInfo()->getNumOfFlattenElements() <=
               base->getMaxFieldOffsetLimit() &&
               !state.lifetimes().mayBeFreed(adapter_.location(object));
    }

}

void AbstractInterpretation::storeValue(const ValVar* pointer,
                                        const SVF::Interval& interval,
                                        const SVF::AddressSet& addresses,
                                        const ICFGNode* node)
{
    State& denseState = ensureState(node);
    const SVF::AddressSet pointees = getAddressSet(pointer, node);
    auto write = [&](SVF::Location location)
    {
        if (!denseState.memoryLayout().contains(location))
            return;
        const ObjVar* object = objectAt(location);
        if (!object || svfir->isNullOrBlkObj(object->getId()))
            return;
        const SVF::Variable content = memoryVariable(*object, denseState);
        if (canStrongStore(pointees, *object, denseState))
        {
            assignMemoryValue(denseState, content, interval, addresses);
            return;
        }
        // This cell may not be written (or may summarize several concrete
        // cells). Retain the unchanged alternative, including initialization
        // and address facets. Sequential weak writes overapproximate the
        // choice of one target; they do not assert that all targets changed.
        {
            State written = topState();
            assignMemoryValue(written, content, interval, addresses);
            denseState.joinValueFrom(content, written, content);
        }
    };

    if (pointees.hasUnknownObject())
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
        for (SVF::Location location : pointees.locations())
            write(location);
    }
}

void AbstractInterpretation::assumeBranch(const IntraCFGEdge* edge,
        State& denseState)
{
    if (!isBranchEdgeFeasibleAt(edge, edge->getSrcNode()))
        denseState = bottomState();
}

bool AbstractInterpretation::isBranchEdgeFeasibleAt(
    const IntraCFGEdge* edge, const ICFGNode* predecessor)
{
    const SVFVar* condition = edge->getCondition();
    assert(condition && "branch condition is missing");
    if (condition->getInEdges().empty())
    {
        const auto* branch =
            SVFUtil::cast<BranchStmt>(edge->getSrcNode()->getSVFStmts().back());
        if (branch->getNumSuccessors() == 2 &&
            branch->getSuccessorCondValue(0) == 1 &&
            branch->getSuccessorCondValue(1) == 0)
        {
            Interval value = getInterval(condition, predecessor);
            value.meetWith(
                Interval::singleton(Rational(edge->getSuccessorCondValue())));
            return !value.isBottom();
        }
        return isSwitchBranchEdgeFeasible(edge,
                                          getInterval(condition, predecessor));
    }
    if (!SVFUtil::isa<CmpStmt>(*condition->getInEdges().begin()))
        return isSwitchBranchEdgeFeasible(edge, getInterval(condition, predecessor));
    SVF::Interval result = getInterval(condition, predecessor);
    result.meetWith(SVF::Interval::singleton(
                        SVF::Rational(edge->getSuccessorCondValue())));
    return !result.isBottom();
}

} // namespace SVF
