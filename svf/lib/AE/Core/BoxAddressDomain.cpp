//===- BoxAddressDomain.cpp -- Box/address reduced product -------------===//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//
// Contributors: Xiao Cheng, Jiawei Wang, Jiawei Yang
//
//===----------------------------------------------------------------------===//

#include "AE/Core/BoxAddressDomain.h"

#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <sstream>
#include "Util/SparseBitVector.h"

namespace SVF
{

namespace
{

std::unique_ptr<NumericalDomain> cloneNumerical(
    const NumericalDomain& numerical)
{
    std::unique_ptr<AbstractDomain> clone = numerical.clone();
    return std::unique_ptr<NumericalDomain>(
               static_cast<NumericalDomain*>(clone.release()));
}

} // namespace

BoxAddressDomain::BoxAddressDomain(
    std::unique_ptr<NumericalDomain> numerical, MemoryLayout memoryLayout,
    bool trackInitialization)
    : numerical_(std::move(numerical)),
      memoryLayout_(std::move(memoryLayout)),
      addresses_(AddressDomain::top()), lifetimes_(LifetimeDomain::bottom()),
      trackInitialization_(trackInitialization)
{
    if (!numerical_)
    {
        assert(false && "product requires a numerical domain");
        std::abort();
    }
}

BoxAddressDomain::BoxAddressDomain(
    std::unique_ptr<NumericalDomain> numerical, MemoryLayout memoryLayout,
    AddressDomain addresses, LifetimeDomain lifetimes)
    : numerical_(std::move(numerical)),
      memoryLayout_(std::move(memoryLayout)),
      addresses_(std::move(addresses)), lifetimes_(std::move(lifetimes))
{
    if (!numerical_)
    {
        assert(false && "product requires a numerical domain");
        std::abort();
    }
}

BoxAddressDomain::BoxAddressDomain(const BoxAddressDomain& other)
    : numerical_(cloneNumerical(*other.numerical_)),
      memoryLayout_(other.memoryLayout_), addresses_(other.addresses_),
      lifetimes_(other.lifetimes_),
      trackInitialization_(other.trackInitialization_),
      numericalInitialization_(other.numericalInitialization_),
      addressInitialization_(other.addressInitialization_)
{
}

BoxAddressDomain& BoxAddressDomain::operator=(const BoxAddressDomain& other)
{
    if (this == &other)
        return *this;
    numerical_ = cloneNumerical(*other.numerical_);
    memoryLayout_ = other.memoryLayout_;
    addresses_ = other.addresses_;
    lifetimes_ = other.lifetimes_;
    trackInitialization_ = other.trackInitialization_;
    numericalInitialization_ = other.numericalInitialization_;
    addressInitialization_ = other.addressInitialization_;
    return *this;
}

static bool mayBeInitialized(InitializationState state)
{
    return (static_cast<unsigned>(state) &
            static_cast<unsigned>(InitializationState::Initialized)) != 0;
}

static bool mayBeUninitialized(InitializationState state)
{
    return (static_cast<unsigned>(state) &
            static_cast<unsigned>(InitializationState::Uninitialized)) != 0;
}

static InitializationState joinInitialization(InitializationState lhs,
        InitializationState rhs)
{
    return static_cast<InitializationState>(static_cast<unsigned>(lhs) |
                                            static_cast<unsigned>(rhs));
}

static std::vector<Variable> mergedVariables(
    const std::vector<Variable>& lhs, const std::vector<Variable>& rhs)
{
    std::vector<Variable> result;
    result.reserve(lhs.size() + rhs.size());
    std::set_union(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                   std::back_inserter(result));
    return result;
}

Interval BoxAddressDomain::interval(Variable variable) const
{
    if (isBottomDomain() || (trackInitialization_ &&
                             !mayBeInitialized(numericalInitialization_.value(variable))))
        return Interval::bottom();
    return numerical_->bound(variable);
}

bool BoxAddressDomain::numericalMayBeUninitialized(Variable variable) const
{
    return trackInitialization_ &&
           mayBeUninitialized(numericalInitialization_.value(variable));
}

void BoxAddressDomain::addUninitializedNumericalAlternative(Variable variable)
{
    if (!trackInitialization_ || isBottomDomain())
        return;
    numericalInitialization_.assign(
        variable,
        joinInitialization(numericalInitialization_.value(variable),
                           InitializationState::Uninitialized));
}

AddressSet BoxAddressDomain::addressSet(Variable variable) const
{
    if (isBottomDomain() || (trackInitialization_ &&
                             !mayBeInitialized(addressInitialization_.value(variable))))
        return AddressSet::bottom();
    return addresses_.addressSet(variable);
}

bool BoxAddressDomain::hasValue(Variable variable) const
{
    if (isBottomDomain())
        return false;
    if (trackInitialization_)
        return mayBeInitialized(numericalInitialization_.value(variable)) ||
               mayBeInitialized(addressInitialization_.value(variable));
    return !numerical_->bound(variable).isTop() ||
           !addresses_.addressSet(variable).isTop();
}

void BoxAddressDomain::setInterval(Variable variable, const Interval& value)
{
    if (isBottomDomain())
        return;
    if (value.isBottom())
        numerical_->forget(variable);
    else
        numerical_->assignBound(variable, value);
    if (trackInitialization_)
        numericalInitialization_.assign(variable, value.isBottom()
                                        ? InitializationState::Uninitialized
                                        : InitializationState::Initialized);
}

void BoxAddressDomain::setAddressSet(Variable variable, const AddressSet& value)
{
    if (isBottomDomain())
        return;
    // No payload is needed for an uninitialized facet. Full Top also has no
    // payload, but its Initialized guard keeps it distinct on reads and joins.
    if (trackInitialization_ && value.isBottom())
        addresses_.forget(variable);
    else
        addresses_.assign(variable, value);
    if (trackInitialization_)
        addressInitialization_.assign(variable, value.isBottom()
                                      ? InitializationState::Uninitialized
                                      : InitializationState::Initialized);
}

void BoxAddressDomain::assignValueFrom(Variable target,
                                       const BoxAddressDomain& sourceState,
                                       Variable source)
{
    if (trackInitialization_ != sourceState.trackInitialization_)
    {
        assert(false && "incompatible initialization tracking");
        std::abort();
    }

    const Interval number = sourceState.interval(source);
    const AddressSet pointers = sourceState.addressSet(source);
    InitializationState numericGuard = InitializationState::Bottom;
    InitializationState pointerGuard = InitializationState::Bottom;
    if (trackInitialization_)
    {
        numericGuard = sourceState.numericalInitialization_.value(source);
        pointerGuard = sourceState.addressInitialization_.value(source);
    }
    setInterval(target, number);
    setAddressSet(target, pointers);
    if (trackInitialization_)
    {
        numericalInitialization_.assign(target, numericGuard);
        addressInitialization_.assign(target, pointerGuard);
    }
}

void BoxAddressDomain::joinValueFrom(Variable target,
                                     const BoxAddressDomain& sourceState,
                                     Variable source)
{
    if (trackInitialization_ != sourceState.trackInitialization_)
    {
        assert(false && "incompatible initialization tracking");
        std::abort();
    }

    Interval number = interval(target);
    AddressSet pointers = addressSet(target);
    InitializationState numericGuard = InitializationState::Bottom;
    InitializationState pointerGuard = InitializationState::Bottom;
    if (trackInitialization_)
    {
        numericGuard = joinInitialization(
                           numericalInitialization_.value(target),
                           sourceState.numericalInitialization_.value(source));
        pointerGuard = joinInitialization(
                           addressInitialization_.value(target),
                           sourceState.addressInitialization_.value(source));
    }
    number.joinWith(sourceState.interval(source));
    pointers.joinWith(sourceState.addressSet(source));
    setInterval(target, number);
    setAddressSet(target, pointers);
    if (trackInitialization_)
    {
        numericalInitialization_.assign(target, numericGuard);
        addressInitialization_.assign(target, pointerGuard);
    }
}

void BoxAddressDomain::resetValue(Variable variable)
{
    numerical_->forget(variable);
    addresses_.forget(variable);
    if (trackInitialization_)
    {
        numericalInitialization_.assign(variable, numericalInitialization_.defaultState());
        addressInitialization_.assign(variable, addressInitialization_.defaultState());
    }
}

BoxAddressDomain BoxAddressDomain::withoutValues(
    const std::set<Variable>& omitted) const
{
    if (numerical_->kind() != DomainKind::Box)
    {
        assert(false && "coordinate selection requires Box");
        std::abort();
    }
    if (isBottomDomain() || omitted.empty())
        return *this;

    const auto& numbers = static_cast<const BoxDomain&>(*numerical_);
    BoxAddressDomain result(BoxDomain::top(numbers.config()), memoryLayout_,
                            trackInitialization_);
    result.lifetimes_ = lifetimes_;
    auto& selectedNumbers = static_cast<BoxDomain&>(*result.numerical_);
    selectedNumbers.boundPages_.reserve(numbers.boundPages_.size());
    for (const auto& entry : numbers.boundPages_)
    {
        std::shared_ptr<BoxDomain::BoundPage> selected;
        for (std::size_t i = 0; i < BoxDomain::BoundsPerPage; ++i)
        {
            const auto& slot = entry.page->bounds[i];
            if (slot && omitted.count(slot->variable))
            {
                if (!selected)
                    selected = std::make_shared<BoxDomain::BoundPage>(*entry.page);
                selected->bounds[i].reset();
            }
        }
        if (!selected)
            selectedNumbers.boundPages_.push_back(entry);
        else if (!BoxDomain::pageIsEmpty(*selected))
            selectedNumbers.boundPages_.push_back({entry.index, std::move(selected)});
    }

    auto& pointers = result.addresses_;
    if (!addresses_.paged_)
    {
        pointers.smallValues_ = addresses_.smallValues_;
        pointers.size_ = addresses_.size_;
        const auto& values = *addresses_.smallValues_;
        const auto first = std::find_if(values.begin(), values.end(),
                                        [&](const auto& value)
        {
            return omitted.count(value.first);
        });
        if (first != values.end())
        {
            pointers.smallValues_ = std::make_shared<AddressDomain::SmallValues>(
                                        values.begin(), first);
            for (auto it = first; it != values.end(); ++it)
                if (!omitted.count(it->first))
                    pointers.smallValues_->push_back(*it);
            pointers.size_ = pointers.smallValues_->size();
        }
    }
    else
    {
        pointers.paged_ = true;
        pointers.size_ = addresses_.size_;
        pointers.pages_.reserve(addresses_.pages_.size());
        for (const auto& entry : addresses_.pages_)
        {
            std::shared_ptr<AddressDomain::ValuePage> selected;
            for (std::size_t i = 0; i < AddressDomain::ValuesPerPage; ++i)
            {
                const auto& slot = entry.page->values[i];
                if (slot && omitted.count(slot->variable))
                {
                    if (!selected)
                        selected = std::make_shared<AddressDomain::ValuePage>(*entry.page);
                    selected->values[i].reset();
                    --pointers.size_;
                }
            }
            if (!selected)
                pointers.pages_.push_back(entry);
            else if (!AddressDomain::pageIsEmpty(*selected))
                pointers.pages_.push_back({entry.index, std::move(selected)});
        }
    }

    const auto selectInitialization = [&](const InitializationDomain& source)
    {
        InitializationDomain selected = source;
        std::shared_ptr<InitializationDomain::Directory> pages;
        for (auto it = source.pages_->begin(); it != source.pages_->end(); ++it)
        {
            const std::uint64_t first =
                std::uint64_t{it->key.id()} * InitializationDomain::ValuesPerPage;
            std::uint64_t mask = 0;
            for (auto variable = omitted.lower_bound(Variable(first));
                    variable != omitted.end() &&
                    variable->id() < first + InitializationDomain::ValuesPerPage;
                    ++variable)
                if (variable->type() == it->key.type())
                    mask |= std::uint64_t{1} << (variable->id() % InitializationDomain::ValuesPerPage);
            const auto& page = *it->page;
            if (!((page.uninitialized | page.initialized) & mask))
            {
                if (pages)
                    pages->push_back(*it);
                continue;
            }
            if (!pages)
                pages = std::make_shared<InitializationDomain::Directory>(source.pages_->begin(), it);
            const auto uninitialized = page.uninitialized & ~mask;
            const auto initialized = page.initialized & ~mask;
            if (uninitialized | initialized)
                pages->push_back({it->key, std::make_shared<InitializationDomain::Page>(
                                      InitializationDomain::Page{uninitialized, initialized})});
        }
        if (pages)
            selected.pages_ = std::move(pages);
        return selected;
    };
    result.numericalInitialization_ = selectInitialization(numericalInitialization_);
    result.addressInitialization_ = selectInitialization(addressInitialization_);
    return result;
}

std::vector<Variable> BoxAddressDomain::initializedVariables() const
{
    if (!trackInitialization_)
        return {};
    return mergedVariables(numericalInitialization_.nonDefaultVariables(),
                           addressInitialization_.nonDefaultVariables());
}

std::vector<Variable> BoxAddressDomain::initializedVariablesBefore(
    Variable upperBound) const
{
    if (!trackInitialization_)
        return {};
    return mergedVariables(
               numericalInitialization_.nonDefaultVariablesBefore(upperBound),
               addressInitialization_.nonDefaultVariablesBefore(upperBound));
}

void BoxAddressDomain::combineInitialized(
    const BoxAddressDomain& other, Combination operation)
{
    const bool intersect = operation == Combination::Meet;
    if (isBottomDomain() || other.isBottomDomain())
    {
        if (intersect && !isBottomDomain())
            *this = other;
        else if (!intersect && isBottomDomain())
            *this = other;
        return;
    }
    // Guards handle initialization-only coordinates with Top payloads.
    // Share payload pages until a value changes.
    BoxAddressDomain result(*this);
    if (intersect)
    {
        result.numericalInitialization_.meetWith(other.numericalInitialization_);
        result.addressInitialization_.meetWith(other.addressInitialization_);
        result.lifetimes_.meetWith(other.lifetimes_);
    }
    else
    {
        result.numericalInitialization_.joinWith(other.numericalInitialization_);
        result.addressInitialization_.joinWith(other.addressInitialization_);
        result.lifetimes_.joinWith(other.lifetimes_);
    }
    // Box payloads need the per-coordinate conditional join below: an
    // uninitialized alternative has no initialized payload and therefore must
    // not contribute numerical Top to the conditional value. Relational
    // payloads cannot be combined coordinate-by-coordinate -- doing so can
    // retain a left-hand relation across a join with an incompatible relation
    // whose unary bounds happen to be identical. Use the native relational
    // lattice operation as a sound fallback. It may lose a relation when only
    // one side has an initialized alternative, but it never invents one.
    const bool relational = numerical_->kind() != DomainKind::Box;
    if (relational)
    {
        if (intersect)
            result.numerical_->meetWith(*other.numerical_);
        else if (operation == Combination::Widen)
            result.numerical_->widenWith(*other.numerical_);
        else
            result.numerical_->joinWith(*other.numerical_);
    }
    else
    {
        const auto numbers = mergedVariables(numerical_->supportVariables(),
                                             other.numerical_->supportVariables());
        for (Variable variable : numbers)
        {
            if (result.isBottomDomain())
                break;
            Interval number = interval(variable);
            const Interval nextNumber = other.interval(variable);
            if (intersect)
                number.meetWith(nextNumber);
            else
            {
                if (operation == Combination::Widen && !number.isBottom() &&
                        !nextNumber.isBottom())
                    number.widenWith(nextNumber);
                else
                    number.joinWith(nextNumber);
            }
            if (number.isBottom())
            {
                // Clear even a latent raw constraint under an inactive guard:
                // mutable domain access must not make it survive a combination.
                result.numerical_->forget(variable);
                if (intersect)
                {
                    const auto guard =
                        result.numericalInitialization_.value(variable);
                    result.numericalInitialization_.assign(
                        variable,
                        static_cast<InitializationState>(
                            static_cast<unsigned>(guard) & 1U));
                }
            }
            else if (number != numerical_->bound(variable))
                result.numerical_->assignBound(variable, number);
        }
    }
    const auto pointers = mergedVariables(addresses_.nonDefaultVariables(),
                                          other.addresses_.nonDefaultVariables());
    for (Variable variable : pointers)
    {
        if (result.isBottomDomain())
            break;
        AddressSet addresses = addressSet(variable);
        const AddressSet nextAddresses = other.addressSet(variable);
        if (intersect)
            addresses.meetWith(nextAddresses);
        else
            addresses.joinWith(nextAddresses);
        if (addresses.isBottom())
        {
            result.addresses_.forget(variable);
            if (intersect)
            {
                const auto guard = result.addressInitialization_.value(variable);
                result.addressInitialization_.assign(variable,
                                                     static_cast<InitializationState>(static_cast<unsigned>(guard) & 1U));
            }
        }
        else if (addresses != addresses_.addressSet(variable))
            result.addresses_.assign(variable, std::move(addresses));
    }
    *this = std::move(result);
}

bool BoxAddressDomain::initializedSubsetOf(const BoxAddressDomain& other) const
{
    if (numericalInitialization_.isSubsetOf(other.numericalInitialization_) != CheckResult::True ||
            addressInitialization_.isSubsetOf(other.addressInitialization_) != CheckResult::True ||
            lifetimes_.isSubsetOf(other.lifetimes_) != CheckResult::True)
        return false;
    // Native inclusion is sufficient once guards are included. This restores
    // shared-page fast paths, without confusing inactive payload Top with an
    // initialized Top. If native inclusion fails, only right-hand non-Top
    // constraints can witness a failure of conditional payload inclusion.
    if (numerical_->isSubsetOf(*other.numerical_) != CheckResult::True)
    {
        // Unary fallback is valid only for Box. Equal per-variable bounds do
        // not establish inclusion between relational properties and using
        // them here would let the fixpoint engine stop before relations have
        // stabilized.
        if (numerical_->kind() != DomainKind::Box)
            return false;
        for (Variable variable : other.numerical_->supportVariables())
            if (mayBeInitialized(numericalInitialization_.value(variable)) &&
                    !numerical_->bound(variable).isSubsetOf(
                        other.numerical_->bound(variable)))
                return false;
    }
    if (addresses_.isSubsetOf(other.addresses_) != CheckResult::True)
    {
        for (Variable variable : other.addresses_.nonDefaultVariables())
            if (mayBeInitialized(addressInitialization_.value(variable)) &&
                    !addresses_.addressSet(variable).isSubsetOf(other.addresses_.addressSet(variable)))
                return false;
    }
    return true;
}

LifetimeDomain LifetimeDomain::top()
{
    return LifetimeDomain(true);
}

LifetimeDomain LifetimeDomain::bottom()
{
    return LifetimeDomain(false);
}

std::unique_ptr<AbstractDomain> LifetimeDomain::clone() const
{
    return std::make_unique<LifetimeDomain>(*this);
}

std::shared_ptr<LifetimeDomain::LocationIDs>
LifetimeDomain::emptyLocationIDs()
{
    static const auto empty = std::make_shared<LocationIDs>();
    return empty;
}

void LifetimeDomain::allocate(Location location)
{
    setMayBeFreed(location, false);
}

void LifetimeDomain::release(Location location)
{
    setMayBeFreed(location, true);
}

bool LifetimeDomain::mayBeFreed(Location location) const
{
    return mayBeFreed(location.id());
}

bool LifetimeDomain::mayBeFreed(std::uint32_t locationID) const
{
    return defaultMayBeFreed_ !=
           (exceptions_->count(locationID) != 0);
}

bool LifetimeDomain::hasCompatibleDomain(const AbstractDomain& other) const
{
    return other.isDomain<LifetimeDomain>();
}

void LifetimeDomain::joinDomain(const AbstractDomain& other)
{
    const auto& state = static_cast<const LifetimeDomain&>(other);
    if (defaultMayBeFreed_ == state.defaultMayBeFreed_ &&
            (exceptions_ == state.exceptions_ ||
             *exceptions_ == *state.exceptions_))
        return;
    if (state.isBottomDomain())
        return;
    if (isBottomDomain())
    {
        *this = state;
        return;
    }
    if (state.leqDomain(*this))
        return;
    if (leqDomain(state))
    {
        *this = state;
        return;
    }
    combineWith(state, true);
}

void LifetimeDomain::meetDomain(const AbstractDomain& other)
{
    const auto& state = static_cast<const LifetimeDomain&>(other);
    if (defaultMayBeFreed_ == state.defaultMayBeFreed_ &&
            (exceptions_ == state.exceptions_ ||
             *exceptions_ == *state.exceptions_))
        return;
    if (state.isTopDomain())
        return;
    if (isTopDomain())
    {
        *this = state;
        return;
    }
    if (leqDomain(state))
        return;
    if (state.leqDomain(*this))
    {
        *this = state;
        return;
    }
    combineWith(state, false);
}

void LifetimeDomain::widenDomain(const AbstractDomain& next)
{
    joinDomain(next);
}

void LifetimeDomain::narrowDomain(const AbstractDomain& next)
{
    meetDomain(next);
}

bool LifetimeDomain::isBottomDomain() const
{
    return !defaultMayBeFreed_ && exceptions_->empty();
}

bool LifetimeDomain::isTopDomain() const
{
    return defaultMayBeFreed_ && exceptions_->empty();
}

bool LifetimeDomain::leqDomain(const AbstractDomain& other) const
{
    const auto& state = static_cast<const LifetimeDomain&>(other);
    if (defaultMayBeFreed_ == state.defaultMayBeFreed_ &&
            (exceptions_ == state.exceptions_ ||
             *exceptions_ == *state.exceptions_))
        return true;
    if (defaultMayBeFreed_ && !state.defaultMayBeFreed_)
        return false;

    for (std::uint32_t locationID : *exceptions_)
    {
        if (mayBeFreed(locationID) && !state.mayBeFreed(locationID))
            return false;
    }
    for (std::uint32_t locationID : *state.exceptions_)
    {
        if (exceptions_->count(locationID) == 0 &&
                mayBeFreed(locationID) && !state.mayBeFreed(locationID))
            return false;
    }
    return true;
}

std::string LifetimeDomain::domainToString() const
{
    std::vector<std::uint32_t> locations(
        exceptions_->begin(), exceptions_->end());
    std::sort(locations.begin(), locations.end());
    std::ostringstream output;
    output << "default="
           << (defaultMayBeFreed_ ? "may-freed" : "not-freed") << " {";
    bool first = true;
    for (std::uint32_t locationID : locations)
    {
        if (!first)
            output << ", ";
        first = false;
        output << locationID << "="
               << (mayBeFreed(locationID) ? "may-freed" : "not-freed");
    }
    output << "}";
    return output.str();
}

void LifetimeDomain::setMayBeFreed(Location location, bool mayBeFreedValue)
{
    const bool differsFromDefault =
        mayBeFreedValue != defaultMayBeFreed_;
    const bool stored = exceptions_->count(location.id()) != 0;
    if (differsFromDefault == stored)
        return;
    if (differsFromDefault)
        writableExceptions().insert(location.id());
    else
        writableExceptions().erase(location.id());
}

void LifetimeDomain::combineWith(
    const LifetimeDomain& other, bool join)
{
    const bool nextDefault = join
                             ? defaultMayBeFreed_ || other.defaultMayBeFreed_
                             : defaultMayBeFreed_ && other.defaultMayBeFreed_;
    LocationIDs next;
    next.reserve(exceptions_->size() + other.exceptions_->size());
    const auto addIfExceptional = [&](std::uint32_t locationID)
    {
        const bool value = join
                           ? mayBeFreed(locationID) || other.mayBeFreed(locationID)
                           : mayBeFreed(locationID) && other.mayBeFreed(locationID);
        if (value != nextDefault)
            next.insert(locationID);
    };
    for (std::uint32_t locationID : *exceptions_)
        addIfExceptional(locationID);
    for (std::uint32_t locationID : *other.exceptions_)
    {
        if (exceptions_->count(locationID) == 0)
            addIfExceptional(locationID);
    }
    defaultMayBeFreed_ = nextDefault;
    exceptions_ = std::make_shared<LocationIDs>(std::move(next));
}

LifetimeDomain::LocationIDs& LifetimeDomain::writableExceptions()
{
    if (exceptions_.use_count() != 1)
        exceptions_ = std::make_shared<LocationIDs>(*exceptions_);
    return *exceptions_;
}

Variable MemoryLayout::contentOf(Location location) const
{
    const auto it = cells_->find(location);
    if (it == cells_->end())
    {
        assert(false && "location has no content symbol");
        std::abort();
    }
    return it->second;
}

void MemoryLayout::extend(Location location, Variable content)
{
    const auto [iterator, inserted] = cells_->emplace(location, content);
    if (!inserted && iterator->second != content)
    {
        assert(false && "location already has a different content symbol");
        std::abort();
    }
}

void BoxAddressDomain::restoreMissingNumericalInitializationFrom(
    const BoxAddressDomain& caller, Variable variable)
{
    if (isBottom() || caller.isBottom() || !trackInitialization_)
        return;
    if (trackInitialization_ != caller.trackInitialization_)
    {
        assert(false && "incompatible initialization tracking");
        std::abort();
    }
    if (!mayBeInitialized(numericalInitialization_.value(variable)))
        numericalInitialization_.assign(
            variable, caller.numericalInitialization_.value(variable));
}

void BoxAddressDomain::restoreMissingMemoryFrom(
    const BoxAddressDomain& caller, Variable content)
{
    if (isBottom() || caller.isBottom())
        return;
    if (trackInitialization_)
    {
        if (!mayBeInitialized(numericalInitialization_.value(content)))
        {
            setInterval(content, caller.interval(content));
            numericalInitialization_.assign(content, caller.numericalInitialization_.value(content));
        }
        restoreMissingAddressFrom(caller, content);
        return;
    }
    if (numerical_->bound(content).isTop())
    {
        const Interval interval = caller.numerical_->bound(content);
        if (!interval.isTop())
            numerical_->assignBound(content, interval);
    }
    restoreMissingAddressFrom(caller, content);
}

void BoxAddressDomain::restoreMissingAddressFrom(
    const BoxAddressDomain& caller, Variable content)
{
    if (isBottom() || caller.isBottom())
        return;
    if (trackInitialization_)
    {
        if (!mayBeInitialized(addressInitialization_.value(content)))
        {
            setAddressSet(content, caller.addressSet(content));
            addressInitialization_.assign(content, caller.addressInitialization_.value(content));
        }
        return;
    }
    if (addresses_.addressSet(content).isTop())
    {
        const AddressSet addresses = caller.addresses_.addressSet(content);
        if (!addresses.isTop())
            addresses_.assign(content, addresses);
    }
}

std::shared_ptr<InitializationDomain::Directory> InitializationDomain::emptyPages()
{
    static const auto empty = std::make_shared<Directory>();
    return empty;
}

InitializationDomain::InitializationDomain(InitializationState state)
    : default_(state), pages_(emptyPages())
{
    if (static_cast<unsigned>(state) > 3)
    {
        assert(false && "invalid initialization state");
        std::abort();
    }
}

InitializationDomain InitializationDomain::top()
{
    return uniform(InitializationState::Top);
}

InitializationDomain InitializationDomain::bottom()
{
    return uniform(InitializationState::Bottom);
}

InitializationDomain InitializationDomain::uniform(InitializationState state)
{
    return InitializationDomain(state);
}

std::unique_ptr<AbstractDomain> InitializationDomain::clone() const
{
    return std::make_unique<InitializationDomain>(*this);
}

InitializationState InitializationDomain::defaultState() const
{
    return default_;
}

InitializationState InitializationDomain::value(Variable variable) const
{
    const Variable key(variable.id() / ValuesPerPage, variable.type());
    const auto it = std::lower_bound(pages_->begin(), pages_->end(), key,
                                     [](const PageEntry& entry, Variable target)
    {
        return entry.key < target;
    });
    if (it == pages_->end() || it->key != key)
        return default_;
    const auto bit = std::uint64_t{1} << (variable.id() % ValuesPerPage);
    const unsigned state = ((it->page->uninitialized & bit) ? 1U : 0U) |
                           ((it->page->initialized & bit) ? 2U : 0U);
    return state ? static_cast<InitializationState>(state) : default_;
}

void InitializationDomain::assign(Variable variable, InitializationState state)
{
    if (static_cast<unsigned>(state) > 3)
    {
        assert(false && "invalid initialization state");
        std::abort();
    }
    if (isBottomDomain() || value(variable) == state)
        return;
    if (state == InitializationState::Bottom)
    {
        *this = bottom();
        return;
    }
    if (pages_.use_count() != 1)
        pages_ = std::make_shared<Directory>(*pages_);
    const Variable key(variable.id() / ValuesPerPage, variable.type());
    auto it = std::lower_bound(pages_->begin(), pages_->end(), key,
                               [](const PageEntry& entry, Variable target)
    {
        return entry.key < target;
    });
    if (it == pages_->end() || it->key != key)
        it = pages_->insert(it, {key, std::make_shared<Page>()});
    else if (it->page.use_count() != 1)
        it->page = std::make_shared<Page>(*it->page);
    const auto bit = std::uint64_t{1} << (variable.id() % ValuesPerPage);
    const unsigned stored = state == default_ ? 0 : static_cast<unsigned>(state);
    auto& page = *it->page;
    page.uninitialized = (page.uninitialized & ~bit) | ((stored & 1U) ? bit : 0);
    page.initialized = (page.initialized & ~bit) | ((stored & 2U) ? bit : 0);
    if (!(page.uninitialized | page.initialized))
        pages_->erase(it);
}

void InitializationDomain::forget(Variable variable)
{
    assign(variable, InitializationState::Top);
}

std::vector<Variable> InitializationDomain::nonDefaultVariables() const
{
    return nonDefaultVariables(nullptr);
}

std::vector<Variable> InitializationDomain::nonDefaultVariablesBefore(
    Variable upperBound) const
{
    return nonDefaultVariables(&upperBound);
}

std::vector<Variable> InitializationDomain::nonDefaultVariables(
    const Variable* upperBound) const
{
    std::vector<Variable> variables;
    for (auto it = pages_->begin(); it != pages_->end();)
    {
        const auto first = variables.size();
        const auto index = it->key.id();
        if (upperBound && index > upperBound->id() / ValuesPerPage)
            break;
        unsigned types = 0;
        do
        {
            auto bits = it->page->uninitialized | it->page->initialized;
            while (bits)
            {
                const auto slot = countTrailingZeros(bits);
                const Variable variable(index * ValuesPerPage + slot, it->key.type());
                if (upperBound && !(variable < *upperBound))
                    break;
                variables.push_back(variable);
                bits &= bits - 1;
            }
            ++types;
            ++it;
        }
        while (it != pages_->end() && it->key.id() == index);
        // Different types can share the same numeric ID. Preserve the public
        // Variable ordering (ID first), not the internal page/type ordering.
        if (types > 1)
            std::sort(variables.begin() + first, variables.end());
    }
    return variables;
}

bool InitializationDomain::hasCompatibleDomain(const AbstractDomain& other) const
{
    return other.isDomain<InitializationDomain>();
}

InitializationDomain::Page InitializationDomain::expanded(
    const Page* page, InitializationState defaultState)
{
    Page result = page ? *page : Page{};
    const auto missing = ~(result.uninitialized | result.initialized);
    if (static_cast<unsigned>(defaultState) & 1U)
        result.uninitialized |= missing;
    if (static_cast<unsigned>(defaultState) & 2U)
        result.initialized |= missing;
    return result;
}

InitializationDomain::Page InitializationDomain::exceptions(
    Page values, InitializationState defaultState)
{
    const auto same = ((static_cast<unsigned>(defaultState) & 1U)
                       ? values.uninitialized : ~values.uninitialized) &
                      ((static_cast<unsigned>(defaultState) & 2U)
                       ? values.initialized : ~values.initialized);
    values.uninitialized &= ~same;
    values.initialized &= ~same;
    return values;
}

void InitializationDomain::combine(const InitializationDomain& other, bool join)
{
    if (pages_ == other.pages_ && default_ == other.default_)
        return;
    if (other.isBottomDomain())
    {
        if (!join)
            *this = bottom();
        return;
    }
    if (isBottomDomain())
    {
        if (join)
            *this = other;
        return;
    }
    const auto op = [join](InitializationState lhs, InitializationState rhs)
    {
        const auto a = static_cast<unsigned>(lhs);
        const auto b = static_cast<unsigned>(rhs);
        return static_cast<InitializationState>(join ? a | b : a & b);
    };
    InitializationDomain result(op(default_, other.default_));
    if (!result.isBottomDomain())
    {
        result.pages_ = std::make_shared<Directory>();
        result.pages_->reserve(std::max(pages_->size(), other.pages_->size()));
        auto left = pages_->begin();
        auto right = other.pages_->begin();
        while (left != pages_->end() || right != other.pages_->end())
        {
            const auto key = right == other.pages_->end() ? left->key :
                             left == pages_->end() ? right->key :
                             std::min(left->key, right->key);
            const PageEntry* a = left != pages_->end() && left->key == key
                                 ? &*left++ : nullptr;
            const PageEntry* b = right != other.pages_->end() && right->key == key
                                 ? &*right++ : nullptr;
            if (a && b && a->page == b->page && default_ == other.default_)
            {
                result.pages_->push_back(*a);
                continue;
            }
            const Page av = expanded(a ? a->page.get() : nullptr, default_);
            const Page bv = expanded(b ? b->page.get() : nullptr, other.default_);
            Page values;
            values.uninitialized = join ? av.uninitialized | bv.uninitialized
                                   : av.uninitialized & bv.uninitialized;
            values.initialized = join ? av.initialized | bv.initialized
                                 : av.initialized & bv.initialized;
            if (~(values.uninitialized | values.initialized))
            {
                *this = bottom();
                return;
            }
            values = exceptions(values, result.default_);
            if (!(values.uninitialized | values.initialized))
                continue;
            const auto matches = [&values](const PageEntry* entry)
            {
                return entry && entry->page->uninitialized == values.uninitialized &&
                       entry->page->initialized == values.initialized;
            };
            if (matches(a))
                result.pages_->push_back(*a);
            else if (matches(b))
                result.pages_->push_back(*b);
            else
                result.pages_->push_back({key, std::make_shared<Page>(values)});
        }
    }
    *this = std::move(result);
}

void InitializationDomain::joinDomain(const AbstractDomain& other)
{
    combine(static_cast<const InitializationDomain&>(other), true);
}

void InitializationDomain::meetDomain(const AbstractDomain& other)
{
    combine(static_cast<const InitializationDomain&>(other), false);
}

void InitializationDomain::widenDomain(const AbstractDomain& next)
{
    joinDomain(next);
}

void InitializationDomain::narrowDomain(const AbstractDomain& next)
{
    meetDomain(next);
}

bool InitializationDomain::isBottomDomain() const
{
    return default_ == InitializationState::Bottom;
}

bool InitializationDomain::isTopDomain() const
{
    return default_ == InitializationState::Top && pages_->empty();
}

bool InitializationDomain::leqDomain(const AbstractDomain& other) const
{
    const auto& rhs = static_cast<const InitializationDomain&>(other);
    if (isBottomDomain())
        return true;
    const auto included = [](InitializationState a, InitializationState b)
    {
        return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) ==
               static_cast<unsigned>(a);
    };
    if (!included(default_, rhs.default_))
        return false;
    if (default_ == rhs.default_ && pages_ == rhs.pages_)
        return true;
    auto left = pages_->begin();
    auto right = rhs.pages_->begin();
    while (left != pages_->end() || right != rhs.pages_->end())
    {
        const auto key = right == rhs.pages_->end() ? left->key :
                         left == pages_->end() ? right->key :
                         std::min(left->key, right->key);
        const Page* a = left != pages_->end() && left->key == key
                        ? (left++)->page.get() : nullptr;
        const Page* b = right != rhs.pages_->end() && right->key == key
                        ? (right++)->page.get() : nullptr;
        if (a == b && default_ == rhs.default_)
            continue;
        const Page av = expanded(a, default_);
        const Page bv = expanded(b, rhs.default_);
        if ((av.uninitialized & ~bv.uninitialized) ||
                (av.initialized & ~bv.initialized))
            return false;
    }
    return true;
}

std::string InitializationDomain::domainToString() const
{
    static const char* names[] = {"bottom", "uninitialized", "initialized", "top"};
    std::ostringstream out;
    out << "default=" << names[static_cast<unsigned>(default_)] << '{';
    bool first = true;
    for (Variable variable : nonDefaultVariables())
    {
        if (!first)
            out << ',';
        first = false;
        out << variable.id() << '=' << names[static_cast<unsigned>(value(variable))];
    }
    return out.str() + '}';
}

} // namespace SVF
