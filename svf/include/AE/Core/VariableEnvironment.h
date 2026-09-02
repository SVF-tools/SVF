//===- VariableEnvironment.h -- Variables and dimensions ------*- C++ -*-===//

#ifndef SVF_AE_VARIABLE_ENVIRONMENT_H
#define SVF_AE_VARIABLE_ENVIRONMENT_H

#include "AE/Core/NumericPrimitives.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SVF::AbstractDomain
{

using Dimension = std::size_t;

class Variable
{
public:
    explicit Variable(std::uint32_t id = 0) : id_(id) {}
    std::uint32_t id() const { return id_; }

    friend bool operator==(Variable lhs, Variable rhs)
    {
        return lhs.id_ == rhs.id_;
    }
    friend bool operator!=(Variable lhs, Variable rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(Variable lhs, Variable rhs)
    {
        return lhs.id_ < rhs.id_;
    }

private:
    std::uint32_t id_;
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

    static NumericType integer() { return {NumericKind::Integer, {}}; }
    static NumericType real() { return {NumericKind::Real, {}}; }
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

struct VariableDeclaration
{
    Variable variable;
    NumericType type;
    std::string name;
};

/// Immutable, reference-counted mapping from public variables to dense domain
/// dimensions.  It deliberately has no dependency on SVF NodeID or LLVM.
class VariableEnvironment
{
public:
    VariableEnvironment();
    explicit VariableEnvironment(std::vector<VariableDeclaration> variables);

    std::size_t size() const;
    bool empty() const { return size() == 0; }
    bool contains(Variable variable) const;
    Dimension dimensionOf(Variable variable) const;
    Variable variableOf(Dimension dimension) const;
    const NumericType& typeOf(Variable variable) const;
    const std::string& nameOf(Variable variable) const;
    const std::vector<VariableDeclaration>& variables() const;

    VariableEnvironment add(std::vector<VariableDeclaration> variables) const;
    VariableEnvironment remove(const std::vector<Variable>& variables) const;
    VariableEnvironment merge(const VariableEnvironment& other) const;

    friend bool operator==(const VariableEnvironment& lhs, const VariableEnvironment& rhs);
    friend bool operator!=(const VariableEnvironment& lhs, const VariableEnvironment& rhs)
    {
        return !(lhs == rhs);
    }

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

// Keep a namespace-scope declaration in addition to the friend declaration.
// This makes qualified out-of-line definitions well-formed without relying on
// friend-only argument-dependent lookup behavior.
bool operator==(const VariableEnvironment& lhs, const VariableEnvironment& rhs);

} // namespace SVF::AbstractDomain

#endif // SVF_AE_VARIABLE_ENVIRONMENT_H
