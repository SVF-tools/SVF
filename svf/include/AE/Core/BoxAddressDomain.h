//===- BoxAddressDomain.h -- Box/address reduced product ------*- C++ -*-===//
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

#ifndef SVF_AE_BOX_ADDRESS_DOMAIN_H
#define SVF_AE_BOX_ADDRESS_DOMAIN_H

#include "AE/Core/AbstractDomain.h"
#include "AE/Core/AddressDomain.h"
#include "AE/Core/NumericalDomain.h"

#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace SVF
{

/// The powerset of {uninitialized, initialized}. This describes initialization
/// only: Initialized does not imply a precise numerical or pointer value.
enum class InitializationState : unsigned char
{
    Bottom = 0,
    Uninitialized = 1,
    Initialized = 2,
    Top = 3
};

/// Independent, total initialization property. Omitted variables have the
/// explicit defaultState(), not a storage-dependent interpretation. A uniform
/// default permits both sparse unknown states and sparse fresh-frame states.
/// This domain does not decide how an interpreter handles an uninitialized read.
class InitializationDomain final : public AbstractDomain
{
    friend class BoxAddressDomain;

public:
    static InitializationDomain top();
    static InitializationDomain bottom();
    static InitializationDomain uniform(InitializationState state);

    DomainKind kind() const noexcept override
    {
        return DomainKind::Initialization;
    }
    std::unique_ptr<AbstractDomain> clone() const override;

    InitializationState value(Variable variable) const;
    InitializationState defaultState() const;
    void assign(Variable variable, InitializationState state);
    void forget(Variable variable);
    std::vector<Variable> nonDefaultVariables() const;
    std::vector<Variable> nonDefaultVariablesBefore(Variable upperBound) const;

private:
    static constexpr unsigned ValuesPerPage = 64;

    /// Two bit planes encode non-default states. A zero pair is an omitted
    /// slot, not coordinate Bottom (which collapses the entire domain).
    struct Page
    {
        std::uint64_t uninitialized = 0;
        std::uint64_t initialized = 0;
    };
    struct PageEntry
    {
        // The ID is a page number. Type remains part of variable identity.
        Variable key;
        std::shared_ptr<Page> page;
    };
    using Directory = std::vector<PageEntry>;
    explicit InitializationDomain(InitializationState state);
    static std::shared_ptr<Directory> emptyPages();
    static Page expanded(const Page* page, InitializationState defaultState);
    static Page exceptions(Page values, InitializationState defaultState);
    std::vector<Variable> nonDefaultVariables(const Variable* upperBound) const;
    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<InitializationDomain>();
    }
    bool hasCompatibleDomain(const AbstractDomain& other) const override;
    void joinDomain(const AbstractDomain& other) override;
    void meetDomain(const AbstractDomain& other) override;
    void widenDomain(const AbstractDomain& next) override;
    void narrowDomain(const AbstractDomain& next) override;
    bool isBottomDomain() const override;
    bool isTopDomain() const override;
    bool leqDomain(const AbstractDomain& other) const override;
    std::string domainToString() const override;
    void combine(const InitializationDomain& other, bool join);

    InitializationState default_;
    // Copies share the directory and pages. A first write detaches the small
    // directory; only the modified page's bits are copied, not all facts.
    std::shared_ptr<Directory> pages_;
};


class LifetimeDomain final : public AbstractDomain
{
public:
    static LifetimeDomain top();
    static LifetimeDomain bottom();

    DomainKind kind() const noexcept override
    {
        return DomainKind::Lifetime;
    }
    std::unique_ptr<AbstractDomain> clone() const override;

    void allocate(Location location);
    void release(Location location);
    bool mayBeFreed(Location location) const;

private:
    using LocationIDs = std::unordered_set<std::uint32_t>;

    explicit LifetimeDomain(bool defaultMayBeFreed)
        : defaultMayBeFreed_(defaultMayBeFreed),
          exceptions_(emptyLocationIDs())
    {
    }

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<LifetimeDomain>();
    }
    bool hasCompatibleDomain(const AbstractDomain& other) const override;
    void joinDomain(const AbstractDomain& other) override;
    void meetDomain(const AbstractDomain& other) override;
    void widenDomain(const AbstractDomain& next) override;
    void narrowDomain(const AbstractDomain& next) override;
    bool isBottomDomain() const override;
    bool isTopDomain() const override;
    bool leqDomain(const AbstractDomain& other) const override;
    std::string domainToString() const override;

    static std::shared_ptr<LocationIDs> emptyLocationIDs();
    bool mayBeFreed(std::uint32_t locationID) const;
    void setMayBeFreed(Location location, bool mayBeFreed);
    void combineWith(const LifetimeDomain& other, bool join);
    LocationIDs& writableExceptions();

    /// The set stores values differing from the default. Normal AE states use
    /// a false default, so only may-freed locations occupy storage. A true
    /// default represents Top and the same set stores known-safe exceptions.
    bool defaultMayBeFreed_ = false;
    std::shared_ptr<LocationIDs> exceptions_;
};

/// Monotone analysis-wide schema mapping abstract locations to the scalar
/// symbols denoting their stored contents. Copies share the schema so objects
/// discovered lazily by the frontend become visible to existing states. Such
/// an extension does not mutate abstract values: every new coordinate has the
/// domains' implicit Top value until a transfer assigns it.
class MemoryLayout
{
public:
    MemoryLayout() : cells_(std::make_shared<Cells>()) {}
    explicit MemoryLayout(std::map<Location, Variable> cells)
        : cells_(std::make_shared<Cells>(std::move(cells)))
    {
    }

    bool contains(Location location) const
    {
        return cells_->count(location) != 0;
    }
    void extend(Location location, Variable content);
    Variable contentOf(Location location) const;
    const std::map<Location, Variable>& cells() const
    {
        return *cells_;
    }

    friend bool operator==(const MemoryLayout& lhs, const MemoryLayout& rhs)
    {
        return lhs.cells_ == rhs.cells_ || *lhs.cells_ == *rhs.cells_;
    }

private:
    using Cells = std::map<Location, Variable>;
    std::shared_ptr<Cells> cells_;
};

/// Reduced product used by AE: Box tracks numerical facts, AddressDomain tracks
/// pointer facts, and LifetimeDomain tracks may-freed objects. Memory contents
/// are ordinary abstract variables in Box or AddressDomain, connected to
/// locations by the shared MemoryLayout.
class BoxAddressDomain final : public AbstractDomain
{
public:
    BoxAddressDomain(std::unique_ptr<NumericalDomain> numerical,
                     MemoryLayout memoryLayout,
                     bool trackInitialization = false);
    BoxAddressDomain(BoxDomain numerical, MemoryLayout memoryLayout,
                     bool trackInitialization = false)
        : BoxAddressDomain(std::make_unique<BoxDomain>(std::move(numerical)),
                           std::move(memoryLayout), trackInitialization)
    {
    }

    BoxAddressDomain(std::unique_ptr<NumericalDomain> numerical,
                     MemoryLayout memoryLayout,
                     AddressDomain addresses, LifetimeDomain lifetimes);
    BoxAddressDomain(BoxDomain numerical, MemoryLayout memoryLayout,
                     AddressDomain addresses, LifetimeDomain lifetimes)
        : BoxAddressDomain(std::make_unique<BoxDomain>(std::move(numerical)),
                           std::move(memoryLayout), std::move(addresses),
                           std::move(lifetimes))
    {
    }

    BoxAddressDomain(const BoxAddressDomain& other);
    BoxAddressDomain(BoxAddressDomain&& other) noexcept = default;
    BoxAddressDomain& operator=(const BoxAddressDomain& other);
    BoxAddressDomain& operator=(BoxAddressDomain&& other) noexcept = default;

    DomainKind kind() const noexcept override
    {
        return DomainKind::Product;
    }
    std::unique_ptr<AbstractDomain> clone() const override
    {
        return std::make_unique<BoxAddressDomain>(*this);
    }

    NumericalDomain& numerical()
    {
        return *numerical_;
    }
    const NumericalDomain& numerical() const
    {
        return *numerical_;
    }
    /// Executor-only storage conversion; the caller must preserve semantics
    /// through its component store before exposing this state to a transfer.
    void replaceNumericalStorage(std::unique_ptr<NumericalDomain> numerical)
    {
        if (!numerical)
        {
            assert(false && "null numerical storage");
            std::abort();
        }
        numerical_ = std::move(numerical);
    }
    AddressDomain& addresses()
    {
        return addresses_;
    }
    const AddressDomain& addresses() const
    {
        return addresses_;
    }
    LifetimeDomain& lifetimes()
    {
        return lifetimes_;
    }
    const LifetimeDomain& lifetimes() const
    {
        return lifetimes_;
    }
    const MemoryLayout& memoryLayout() const
    {
        return memoryLayout_;
    }

    /// Numerical and address initialization are independent of poison/lifetime.
    /// Payload bounds hold only in the corresponding Initialized case.
    const InitializationDomain& numericalInitialization() const
    {
        return numericalInitialization_;
    }
    const InitializationDomain& addressInitialization() const
    {
        return addressInitialization_;
    }
    /// Whether the numerical facet includes an uninitialized alternative.
    bool numericalMayBeUninitialized(Variable variable) const;
    /// Preserve an uninitialized alternative alongside the current numerical
    /// payload. The payload remains conditional on the initialized case.
    void addUninitializedNumericalAlternative(Variable variable);
    Interval interval(Variable variable) const;
    AddressSet addressSet(Variable variable) const;
    bool hasValue(Variable variable) const;
    bool hasSameValue(Variable variable, const BoxAddressDomain& other) const;
    /// Compare visible values, excluding lifetime and initialization alternatives.
    bool hasSameValues(const BoxAddressDomain& other) const;
    void setInterval(Variable variable, const Interval& value);
    void setAddressSet(Variable variable, const AddressSet& value);
    /// Copy or join one reduced-product coordinate without copying a state.
    /// Initialization guards are transferred with their conditional payloads.
    void assignValueFrom(Variable target, const BoxAddressDomain& sourceState,
                         Variable source);
    void joinValueFrom(Variable target, const BoxAddressDomain& sourceState,
                       Variable source);
    /// Remove a coordinate from a sparse carrier, including its guards.
    /// Unlike logical forget, this restores the carrier's initial default.
    void resetValue(Variable variable);
    /// Select a Box carrier snapshot, restoring omitted coordinates to their
    /// defaults while sharing fully retained pages and preserving lifetimes.
    BoxAddressDomain withoutValues(const std::set<Variable>& omitted) const;
    std::vector<Variable> initializedVariables() const;
    std::vector<Variable> initializedVariablesBefore(Variable upperBound) const;

    /// Restore absent facets from a caller frame after a shared callee.
    /// With initialization tracking, a defined Top is never treated as absent.
    /// Untracked clients retain the payload-Top restoration policy.
    void restoreMissingNumericalInitializationFrom(
        const BoxAddressDomain& caller, Variable variable);
    void restoreMissingMemoryFrom(const BoxAddressDomain& caller,
                                  Variable content);
    void restoreMissingAddressFrom(const BoxAddressDomain& caller,
                                   Variable content);

    void assignPointer(Variable target, const AddressSet& value)
    {
        setAddressSet(target, value);
        setInterval(target, Interval::bottom());
    }

    void assignNumeric(Variable target, const LinearExpression& expression)
    {
        numerical_->assign(target, expression);
        if (trackInitialization_)
        {
            numericalInitialization_.assign(target, InitializationState::Initialized);
            setAddressSet(target, AddressSet::bottom());
        }
        else
            addresses_.forget(target);
    }

    void assignNumericParallel(const LinearAssignmentList& assignments)
    {
        numerical_->assignParallel(assignments);
        for (const LinearAssignment& assignment : assignments)
        {
            if (trackInitialization_)
            {
                numericalInitialization_.assign(assignment.target, InitializationState::Initialized);
                setAddressSet(assignment.target, AddressSet::bottom());
            }
            else
                addresses_.forget(assignment.target);
        }
    }

    void assignNumericParallel(const TreeAssignmentList& assignments)
    {
        numerical_->assignParallel(assignments);
        for (const TreeAssignment& assignment : assignments)
        {
            if (trackInitialization_)
            {
                numericalInitialization_.assign(assignment.target, InitializationState::Initialized);
                setAddressSet(assignment.target, AddressSet::bottom());
            }
            else
                addresses_.forget(assignment.target);
        }
    }

    void assume(const LinearConstraint& constraint)
    {
        if (trackInitialization_)
        {
            for (const auto& term : constraint.expression().terms())
            {
                if (interval(term.first).isBottom())
                    setInterval(term.first, Interval::top());
            }
        }
        numerical_->assume(constraint);
    }

    void load(Variable target, Variable pointer)
    {
        const AddressSet pointees = addressSet(pointer);
        if (pointees.hasUnknownObject() || pointees.isBottom())
        {
            setInterval(target, pointees.hasUnknownObject()
                        ? Interval::top() : Interval::bottom());
            if (pointees.hasUnknownObject())
                setAddressSet(target, AddressSet::top());
            else
                setAddressSet(target, AddressSet::bottom());
            return;
        }

        bool first = true;
        BoxAddressDomain result(*this);
        for (Location location : pointees.locations())
        {
            if (!memoryLayout_.contains(location))
                continue;
            BoxAddressDomain alternative(*this);
            const Variable content = memoryLayout_.contentOf(location);
            alternative.strongStore(target, content);
            if (first)
            {
                result = std::move(alternative);
                first = false;
            }
            else
            {
                result.joinDomain(alternative);
            }
        }
        if (first)
        {
            setInterval(target, Interval::top());
            setAddressSet(target, AddressSet::top());
        }
        else
        {
            *this = std::move(result);
        }
    }

    /// Overwrite one interpreter-selected memory target.
    void assignMemory(Location location, Variable source)
    {
        if (memoryLayout_.contains(location))
            strongStore(memoryLayout_.contentOf(location), source);
    }

    /// Join into one interpreter-selected memory target.
    void joinMemory(Location location, Variable source)
    {
        if (memoryLayout_.contains(location))
            weakStore(memoryLayout_.contentOf(location), source);
    }

    void allocate(Location location)
    {
        lifetimes_.allocate(location);
    }

    void release(Variable pointer)
    {
        const AddressSet pointees = addressSet(pointer);
        if (pointees.hasUnknownObject())
        {
            for (const auto& [location, content] : memoryLayout_.cells())
            {
                (void)content;
                lifetimes_.release(location);
            }
            return;
        }
        for (Location location : pointees.locations())
            lifetimes_.release(location);
    }

private:
    const Interval& boxInterval(Variable variable) const;
    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<BoxAddressDomain>();
    }
    bool hasCompatibleDomain(const AbstractDomain& other) const override
    {
        const auto* product = other.isDomain<BoxAddressDomain>()
                              ? &static_cast<const BoxAddressDomain&>(other)
                              : nullptr;
        return product && trackInitialization_ == product->trackInitialization_ &&
               memoryLayout_ == product->memoryLayout_ &&
               numerical_->isCompatibleWith(*product->numerical_);
    }

    void joinDomain(const AbstractDomain& other) override
    {
        const BoxAddressDomain& product = requireProduct(other);
        if (product.isBottomDomain())
            return;
        if (isBottomDomain())
        {
            *this = product;
            return;
        }
        if (trackInitialization_)
        {
            combineInitialized(product, Combination::Join);
            return;
        }
        numerical_->joinWith(*product.numerical_);
        addresses_.joinWith(product.addresses_);
        lifetimes_.joinWith(product.lifetimes_);
    }

    void meetDomain(const AbstractDomain& other) override
    {
        const BoxAddressDomain& product = requireProduct(other);
        if (trackInitialization_)
        {
            combineInitialized(product, Combination::Meet);
            return;
        }
        if (product.isTopDomain())
            return;
        if (isTopDomain())
        {
            *this = product;
            return;
        }
        numerical_->meetWith(*product.numerical_);
        addresses_.meetWith(product.addresses_);
        lifetimes_.meetWith(product.lifetimes_);
    }

    void widenDomain(const AbstractDomain& next) override
    {
        const BoxAddressDomain& product = requireProduct(next);
        if (product.isBottomDomain())
            return;
        if (isBottomDomain())
        {
            *this = product;
            return;
        }
        if (trackInitialization_)
        {
            combineInitialized(product, Combination::Widen);
            return;
        }
        numerical_->widenWith(*product.numerical_);
        addresses_.widenWith(product.addresses_);
        lifetimes_.widenWith(product.lifetimes_);
    }

    void narrowDomain(const AbstractDomain& next) override
    {
        const BoxAddressDomain& product = requireProduct(next);
        if (trackInitialization_)
        {
            combineInitialized(product, Combination::Meet);
            return;
        }
        if (isTopDomain())
        {
            *this = product;
            return;
        }
        numerical_->narrowWith(*product.numerical_);
        addresses_.narrowWith(product.addresses_);
        lifetimes_.narrowWith(product.lifetimes_);
    }

    bool isBottomDomain() const override
    {
        return numerical_->isBottom() || addresses_.isBottom() ||
               (trackInitialization_ && (numericalInitialization_.isBottom() ||
                                         addressInitialization_.isBottom()));
    }

    bool isTopDomain() const override
    {
        // At the typed program-state layer an absent address facet means that
        // no pointer-specific constraint has been materialized; pointer reads
        // conservatively project it as Address Top. Likewise, absent lifetime
        // facts impose no release constraint. This is the canonical
        // unconstrained flow state used by dense and sparse AE.
        return numerical_->isTop() && addresses_.isTop() &&
               lifetimes_.isBottom() &&
               (!trackInitialization_ || (numericalInitialization_.isTop() &&
                                          addressInitialization_.isTop()));
    }

    bool leqDomain(const AbstractDomain& other) const override
    {
        const BoxAddressDomain& product = requireProduct(other);
        if (isBottomDomain() || product.isTopDomain())
            return true;
        if (product.isBottomDomain())
            return false;
        if (trackInitialization_)
            return initializedSubsetOf(product);
        return numerical_->isSubsetOf(*product.numerical_) == CheckResult::True &&
               addresses_.isSubsetOf(product.addresses_) == CheckResult::True &&
               lifetimes_.isSubsetOf(product.lifetimes_) == CheckResult::True;
    }

    std::string domainToString() const override
    {
        return "numeric=" + numerical_->toString() +
               ", addresses=" + addresses_.toString() +
               ", lifetimes=" + lifetimes_.toString() +
               (trackInitialization_
                ? ", numeric-init=" + numericalInitialization_.toString() +
                ", address-init=" + addressInitialization_.toString() : "");
    }

    const BoxAddressDomain& requireProduct(const AbstractDomain& other) const
    {
        requireCompatible(other);
        return static_cast<const BoxAddressDomain&>(other);
    }

    void strongStore(Variable content, Variable source)
    {
        assignValueFrom(content, *this, source);
    }

    void weakStore(Variable content, Variable source)
    {
        BoxAddressDomain alternative(*this);
        alternative.strongStore(content, source);
        joinDomain(alternative);
    }

    std::unique_ptr<NumericalDomain> numerical_;
    MemoryLayout memoryLayout_;
    AddressDomain addresses_;
    LifetimeDomain lifetimes_;
    enum class Combination { Join, Meet, Widen };
    void combineInitialized(const BoxAddressDomain& other, Combination operation);
    bool initializedSubsetOf(const BoxAddressDomain& other) const;
    bool trackInitialization_ = false;
    InitializationDomain numericalInitialization_ =
        InitializationDomain::uniform(InitializationState::Uninitialized);
    InitializationDomain addressInitialization_ =
        InitializationDomain::uniform(InitializationState::Uninitialized);
};

} // namespace SVF

#endif // SVF_AE_BOX_ADDRESS_DOMAIN_H
