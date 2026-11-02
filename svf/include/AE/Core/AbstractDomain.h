//===- AbstractDomain.h -- Abstract-property lattice API -------*- C++ -*-===//
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

#ifndef SVF_AE_ABSTRACT_DOMAIN_H
#define SVF_AE_ABSTRACT_DOMAIN_H

#include <cstdint>
#include <tuple>
#include <memory>
#include <string>

namespace SVF
{

struct FloatFormat
{
    unsigned exponentBits = 0;
    unsigned significandBits = 0;

    static FloatFormat binary32()
    {
        return {8, 24};
    }
    static FloatFormat binary64()
    {
        return {11, 53};
    }
};

enum class NumericKind
{
    Integer,
    Real,
    IEEEFloat
};

struct NumericType
{
    NumericKind kind = NumericKind::Integer;
    FloatFormat floatFormat{};

    static NumericType integer()
    {
        return {NumericKind::Integer, {}};
    }
    static NumericType real()
    {
        return {NumericKind::Real, {}};
    }
    static NumericType ieee(FloatFormat format)
    {
        return {NumericKind::IEEEFloat, format};
    }

    friend bool operator==(const NumericType& lhs, const NumericType& rhs)
    {
        return lhs.kind == rhs.kind &&
               lhs.floatFormat.exponentBits == rhs.floatFormat.exponentBits &&
               lhs.floatFormat.significandBits ==
               rhs.floatFormat.significandBits;
    }
    friend bool operator!=(const NumericType& lhs, const NumericType& rhs)
    {
        return !(lhs == rhs);
    }
};

/// Typed scalar or object-content identity; its ID is the sparse-storage coordinate.
class Variable
{
public:
    explicit Variable(std::uint32_t id = 0,
                      NumericType type = NumericType::integer())
        : id_(id), type_(type)
    {
    }

    std::uint32_t id() const
    {
        return id_;
    }
    const NumericType& type() const
    {
        return type_;
    }

    friend bool operator==(Variable lhs, Variable rhs)
    {
        return lhs.id_ == rhs.id_ && lhs.type_ == rhs.type_;
    }
    friend bool operator!=(Variable lhs, Variable rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(Variable lhs, Variable rhs)
    {
        return std::tie(lhs.id_, lhs.type_.kind,
                        lhs.type_.floatFormat.exponentBits,
                        lhs.type_.floatFormat.significandBits) <
               std::tie(rhs.id_, rhs.type_.kind,
                        rhs.type_.floatFormat.exponentBits,
                        rhs.type_.floatFormat.significandBits);
    }

private:
    std::uint32_t id_;
    NumericType type_;
};

enum class CheckResult
{
    False,
    True,
    Unknown
};

enum class DomainKind
{
    Box,
    Address,
    Lifetime,
    Product,
    Initialization,
    Octagon,
    ConvexPolyhedra
};

const char* toString(CheckResult result);

/// Common interface implemented by every self-contained abstract property.
///
/// It deliberately contains only lattice operations. Transfer functions live
/// on more specific interfaces such as NumericalDomain and AddressDomain.
class AbstractDomain
{
public:
    virtual ~AbstractDomain();

    virtual DomainKind kind() const noexcept = 0;
    virtual std::unique_ptr<AbstractDomain> clone() const = 0;

    void joinWith(const AbstractDomain& other);
    void meetWith(const AbstractDomain& other);
    void widenWith(const AbstractDomain& next);
    void narrowWith(const AbstractDomain& next);

    bool isBottom() const;
    bool isTop() const;
    /// Return whether every concrete state represented by this state is also
    /// represented by `other`.
    CheckResult isSubsetOf(const AbstractDomain& other) const;
    CheckResult isEquivalentTo(const AbstractDomain& other) const;
    /// Return whether lattice operations between the two properties are
    /// defined. Product domains use this to validate dynamically owned
    /// components before dispatching an operation.
    bool isCompatibleWith(const AbstractDomain& other) const
    {
        return hasCompatibleDomain(other);
    }
    std::string toString() const;

    /// RTTI-free concrete-state query. SVF is commonly built with -fno-rtti,
    /// so abstract domains use stable per-C++-type tokens for checked dispatch.
    template <typename DomainT> bool isDomain() const noexcept
    {
        return dynamicTypeToken() == staticTypeToken<DomainT>();
    }

protected:
    AbstractDomain() = default;
    AbstractDomain(const AbstractDomain&) = default;
    AbstractDomain(AbstractDomain&&) noexcept = default;
    AbstractDomain& operator=(const AbstractDomain&) = default;
    AbstractDomain& operator=(AbstractDomain&&) noexcept = default;

    void requireCompatible(const AbstractDomain& other) const;

    template <typename StateT> static const void* staticTypeToken() noexcept
    {
        static const char token = 0;
        return &token;
    }

private:
    virtual const void* dynamicTypeToken() const noexcept = 0;
    virtual bool hasCompatibleDomain(const AbstractDomain& other) const = 0;
    virtual void joinDomain(const AbstractDomain& other) = 0;
    virtual void meetDomain(const AbstractDomain& other) = 0;
    virtual void widenDomain(const AbstractDomain& next) = 0;
    virtual void narrowDomain(const AbstractDomain& next) = 0;
    virtual bool isBottomDomain() const = 0;
    virtual bool isTopDomain() const = 0;
    virtual bool leqDomain(const AbstractDomain& other) const = 0;
    virtual std::string domainToString() const = 0;
};

} // namespace SVF

#endif // SVF_AE_ABSTRACT_DOMAIN_H
