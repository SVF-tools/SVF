//===- LinearConstraint.h -- Domain-neutral linear syntax -------*- C++ -*-===//

#ifndef SVF_AE_LINEAR_CONSTRAINT_H
#define SVF_AE_LINEAR_CONSTRAINT_H

#include "AE/Core/VariableEnvironment.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SVF::AbstractDomain
{

class LinearExpression
{
public:
    using Terms = std::map<Variable, Rational>;

    LinearExpression();
    explicit LinearExpression(Rational constant);
    explicit LinearExpression(Variable variable);

    const Terms& terms() const
    {
        return terms_;
    }
    const Rational& constant() const
    {
        return constant_;
    }
    Rational coefficient(Variable variable) const;

    LinearExpression& setCoefficient(Variable variable, Rational coefficient);
    LinearExpression& setConstant(Rational constant);
    LinearExpression& operator+=(const LinearExpression& rhs);
    LinearExpression& operator-=(const LinearExpression& rhs);
    LinearExpression& operator*=(const Rational& scalar);

    /// Simultaneously replace variables in this expression. Replacement
    /// expressions are inserted verbatim: variables occurring inside a
    /// replacement are pre-state variables and are not recursively replaced.
    LinearExpression substituted(
        const std::map<Variable, LinearExpression>& replacements) const;

    std::string toString(const VariableEnvironment* environment = nullptr) const;

    friend LinearExpression operator+(LinearExpression lhs,
                                      const LinearExpression& rhs)
    {
        return lhs += rhs;
    }
    friend LinearExpression operator-(LinearExpression lhs,
                                      const LinearExpression& rhs)
    {
        return lhs -= rhs;
    }
    friend LinearExpression operator*(LinearExpression lhs,
                                      const Rational& scalar)
    {
        return lhs *= scalar;
    }
    friend LinearExpression operator*(const Rational& scalar,
                                      LinearExpression rhs)
    {
        return rhs *= scalar;
    }
    friend LinearExpression operator-(LinearExpression expression)
    {
        return expression *= Rational(-1);
    }

private:
    void removeZeroTerms();

    Terms terms_;
    Rational constant_;
};

enum class UnaryOperator
{
    Negate,
    Cast,
    SquareRoot
};

enum class BinaryOperator
{
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder
};

class TreeExpression
{
public:
    enum class Kind
    {
        Constant,
        Variable,
        Unary,
        Binary
    };

    static TreeExpression constant(Rational value,
                                   NumericType type = NumericType::real());
    static TreeExpression variable(Variable value, NumericType type);
    static TreeExpression unary(
        UnaryOperator operation, TreeExpression operand, NumericType type,
        RoundingMode rounding = RoundingMode::NearestTiesToEven);
    static TreeExpression binary(
        BinaryOperator operation, TreeExpression lhs, TreeExpression rhs,
        NumericType type,
        RoundingMode rounding = RoundingMode::NearestTiesToEven);

    Kind kind() const
    {
        return kind_;
    }
    const NumericType& type() const
    {
        return type_;
    }
    const Rational& constant() const
    {
        return constant_;
    }
    Variable variable() const
    {
        return variable_;
    }
    UnaryOperator unaryOperator() const
    {
        return unaryOperator_;
    }
    BinaryOperator binaryOperator() const
    {
        return binaryOperator_;
    }
    RoundingMode roundingMode() const
    {
        return roundingMode_;
    }
    const TreeExpression& lhs() const;
    const TreeExpression& rhs() const;

    /// Return an exact affine expression when the tree is affine under
    /// mathematical integer/real semantics.  Floating and nonlinear trees
    /// deliberately return nullopt and must use a sound backend fallback.
    std::optional<LinearExpression> asLinear() const;

private:
    Kind kind_ = Kind::Constant;
    NumericType type_ = NumericType::real();
    Rational constant_;
    Variable variable_;
    UnaryOperator unaryOperator_ = UnaryOperator::Negate;
    BinaryOperator binaryOperator_ = BinaryOperator::Add;
    RoundingMode roundingMode_ = RoundingMode::NearestTiesToEven;
    std::shared_ptr<const TreeExpression> lhs_;
    std::shared_ptr<const TreeExpression> rhs_;
};

enum class ConstraintKind
{
    Equal,
    NotEqual,
    LessThan,
    LessEqual,
    GreaterThan,
    GreaterEqual
};

/// A normalized constraint of the form expression (relation) 0.
class LinearConstraint
{
public:
    LinearConstraint(LinearExpression expression, ConstraintKind kind);

    const LinearExpression& expression() const
    {
        return expression_;
    }
    ConstraintKind kind() const
    {
        return kind_;
    }
    std::string toString(const VariableEnvironment* environment = nullptr) const;

private:
    LinearExpression expression_;
    ConstraintKind kind_;
};

class TreeConstraint
{
public:
    TreeConstraint(TreeExpression expression, ConstraintKind kind);

    const TreeExpression& expression() const
    {
        return expression_;
    }
    ConstraintKind kind() const
    {
        return kind_;
    }

private:
    TreeExpression expression_;
    ConstraintKind kind_;
};

using LinearConstraintSet = std::vector<LinearConstraint>;

LinearConstraint equal(LinearExpression lhs, LinearExpression rhs);
LinearConstraint notEqual(LinearExpression lhs, LinearExpression rhs);
LinearConstraint lessEqual(LinearExpression lhs, LinearExpression rhs);
LinearConstraint lessThan(LinearExpression lhs, LinearExpression rhs);
LinearConstraint greaterEqual(LinearExpression lhs, LinearExpression rhs);
LinearConstraint greaterThan(LinearExpression lhs, LinearExpression rhs);

} // namespace SVF::AbstractDomain

#endif // SVF_AE_LINEAR_CONSTRAINT_H
