//===- AddressDomain.h -- Address-set abstract property --------*- C++ -*-===//
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
// Contributors: Xiao Cheng, Jiawei Yang
//
//===----------------------------------------------------------------------===//

#ifndef SVF_AE_ADDRESS_DOMAIN_H
#define SVF_AE_ADDRESS_DOMAIN_H

#include "AE/Core/AbstractDomain.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace SVF
{

class Location
{
public:
    explicit Location(std::uint32_t id = 0) : id_(id) {}

    static Location null()
    {
        return Location();
    }

    std::uint32_t id() const
    {
        return id_;
    }
    bool isNull() const
    {
        return id_ == 0;
    }

    friend bool operator==(Location lhs, Location rhs)
    {
        return lhs.id_ == rhs.id_;
    }
    friend bool operator!=(Location lhs, Location rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(Location lhs, Location rhs)
    {
        return lhs.id_ < rhs.id_;
    }

private:
    std::uint32_t id_;
};

/// Sorted finite location storage with two inline elements. Most AE
/// points-to facts are singletons, so they require no auxiliary allocation.
class FiniteLocationSet
{
public:
    using const_iterator = const Location*;

    FiniteLocationSet() = default;
    FiniteLocationSet(const FiniteLocationSet& other);
    FiniteLocationSet(FiniteLocationSet&& other) noexcept = default;
    FiniteLocationSet& operator=(const FiniteLocationSet& other);
    FiniteLocationSet& operator=(FiniteLocationSet&& other) noexcept = default;

    std::size_t size() const;
    bool empty() const;
    bool contains(Location location) const;
    const_iterator begin() const;
    const_iterator end() const;
    void insert(Location location);
    void assign(std::vector<Location> locations);

    friend bool operator==(const FiniteLocationSet& lhs,
                           const FiniteLocationSet& rhs);

private:
    static constexpr std::size_t InlineCapacity = 2;
    std::size_t size_ = 0;
    std::array<Location, InlineCapacity> inline_{Location(), Location()};
    std::unique_ptr<std::vector<Location>> overflow_;
};

/// Address value split into modeled-object, null, and raw-address components.
///
/// The modeled-object component is either a finite set or all modeled objects.
/// Null remains an explicit Location even when the object component is Top.
/// The raw bit records an unmodeled/invalid non-null representation. Keeping
/// the three components separate prevents integer casts and uninitialized
/// values from creating spurious flow through every modeled memory object.
class AddressSet
{
public:
    using const_iterator = FiniteLocationSet::const_iterator;

    AddressSet() = default;

    static AddressSet bottom();
    static AddressSet objectTop();
    /// Any non-null raw address without modeled-object provenance.
    static AddressSet rawTop();
    /// Raw addresses plus the explicit null location, still no objects.
    static AddressSet rawOrNull();
    static AddressSet top();
    static AddressSet singleton(Location location);

    bool isBottom() const;
    bool isTop() const;
    bool isObjectTop() const;
    bool isRawTop() const;
    bool hasUnknownObject() const;
    bool mayContainRawAddress() const;
    bool isFinite() const;
    bool isSingleton() const;
    bool contains(Location location) const;
    bool hasIntersection(const AddressSet& other) const;
    std::size_t size() const;
    bool empty() const;
    const FiniteLocationSet& locations() const;
    const_iterator begin() const
    {
        return locations().begin();
    }
    const_iterator end() const
    {
        return locations().end();
    }

    void insert(Location location);
    void joinWith(const AddressSet& other);
    void meetWith(const AddressSet& other);
    bool isSubsetOf(const AddressSet& other) const;
    std::string toString() const;

    friend bool operator==(const AddressSet& lhs, const AddressSet& rhs)
    {
        return lhs.allObjects_ == rhs.allObjects_ &&
               lhs.mayContainRawAddress_ == rhs.mayContainRawAddress_ &&
               lhs.locations_ == rhs.locations_;
    }
    friend bool operator!=(const AddressSet& lhs, const AddressSet& rhs)
    {
        return !(lhs == rhs);
    }

private:
    AddressSet(bool allObjects, bool mayContainRawAddress)
        : allObjects_(allObjects),
          mayContainRawAddress_(mayContainRawAddress)
    {
    }

    bool allObjects_ = false;
    bool mayContainRawAddress_ = false;
    FiniteLocationSet locations_;
};

/// Flow-sensitive address property with sparse non-Top support over stable
/// Variables. Missing entries in every non-Bottom property denote full
/// Address Top. Object-top, raw-top, finite sets, and the per-variable empty
/// set are explicit values. Whole-property Bottom denotes an unreachable
/// address carrier.
class AddressDomain final : public AbstractDomain
{
    friend class BoxAddressDomain;

public:
    static AddressDomain top();
    static AddressDomain bottom();

    DomainKind kind() const noexcept override
    {
        return DomainKind::Address;
    }
    std::unique_ptr<AbstractDomain> clone() const override;

    AddressSet addressSet(Variable variable) const;
    /// Variables with a value different from Address Top.
    std::vector<Variable> nonDefaultVariables() const;
    std::vector<Variable> nonDefaultVariablesBefore(
        Variable upperBound) const;
    void assign(Variable variable, AddressSet addresses);
    void forget(Variable variable);

private:
    static constexpr std::size_t ValuesPerPage = 16;
    static constexpr std::size_t SmallThreshold = 16;

    using Value = std::pair<Variable, AddressSet>;
    using SmallValues = std::vector<Value>;

    struct ValueSlot
    {
        Variable variable;
        AddressSet addresses;
    };

    struct ValuePage
    {
        std::array<std::optional<ValueSlot>, ValuesPerPage> values;
    };

    struct ValuePageEntry
    {
        std::size_t index;
        std::shared_ptr<ValuePage> page;
    };

    using ValuePageDirectory = std::vector<ValuePageEntry>;

    explicit AddressDomain(bool bottom)
        : bottom_(bottom), smallValues_(emptySmallValues())
    {
    }

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<AddressDomain>();
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

    const AddressDomain& requireAddress(const AbstractDomain& other) const;
    static std::shared_ptr<SmallValues> emptySmallValues();
    const AddressSet* findValue(Variable variable) const;
    void storeValue(Variable variable, AddressSet addresses);
    void eraseValue(Variable variable);
    void promoteToPages();
    SmallValues& writableSmallValues();
    ValuePage& writablePage(std::size_t pageIndex);
    static bool pageIsEmpty(const ValuePage& page);
    void makeBottom();

    bool bottom_ = false;
    bool paged_ = false;
    std::size_t size_ = 0;
    std::shared_ptr<SmallValues> smallValues_;
    ValuePageDirectory pages_;
};

} // namespace SVF

#endif // SVF_AE_ADDRESS_DOMAIN_H
