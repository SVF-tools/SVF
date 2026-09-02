//===- LinearConstraint.cpp -- Domain-neutral linear syntax -------------===//

#include "AE/Core/LinearConstraint.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace SVF::AbstractDomain
{

LinearExpression::LinearExpression() = default;

LinearExpression::LinearExpression(Rational constant)
    : constant_(std::move(constant))
{
}
LinearExpression::LinearExpression(Variable variable)
{
    terms_.emplace(variable, Rational(1));
}

Rational LinearExpression::coefficient(Variable variable) const
{
    const auto it = terms_.find(variable);
    return it == terms_.end() ? Rational() : it->second;
}

LinearExpression& LinearExpression::setCoefficient(Variable variable,
                                                   Rational coefficient)
{
    if (coefficient.isZero())
        terms_.erase(variable);
    else
        terms_[variable] = std::move(coefficient);
    return *this;
}

LinearExpression& LinearExpression::setConstant(Rational constant)
{
    constant_ = std::move(constant);
    return *this;
}

LinearExpression& LinearExpression::operator+=(const LinearExpression& rhs)
{
    constant_ += rhs.constant_;
    for (const auto& [variable, coefficient] : rhs.terms_)
        terms_[variable] += coefficient;
    removeZeroTerms();
    return *this;
}

LinearExpression& LinearExpression::operator-=(const LinearExpression& rhs)
{
    constant_ -= rhs.constant_;
    for (const auto& [variable, coefficient] : rhs.terms_)
        terms_[variable] -= coefficient;
    removeZeroTerms();
    return *this;
}

LinearExpression& LinearExpression::operator*=(const Rational& scalar)
{
    constant_ *= scalar;
    for (auto& [variable, coefficient] : terms_)
    {
        (void)variable;
        coefficient *= scalar;
    }
    removeZeroTerms();
    return *this;
}

LinearExpression LinearExpression::substituted(
    const std::map<Variable, LinearExpression>& replacements) const
{
    LinearExpression result(constant_);
    for (const auto& [variable, coefficient] : terms_)
    {
        const auto replacement = replacements.find(variable);
        if (replacement == replacements.end())
            result.setCoefficient(
                variable, result.coefficient(variable) + coefficient);
        else
            result += replacement->second * coefficient;
    }
    return result;
}

void LinearExpression::removeZeroTerms()
{
    for (auto it = terms_.begin(); it != terms_.end();)
    {
        if (it->second.isZero())
            it = terms_.erase(it);
        else
            ++it;
    }
}

std::string LinearExpression::toString(const VariableEnvironment* environment) const
{
    std::ostringstream output;
    bool first = true;
    for (const auto& [variable, coefficient] : terms_)
    {
        if (!first)
            output << " + ";
        first = false;
        output << coefficient.toString() << '*';
        if (environment && environment->contains(variable) &&
            !environment->nameOf(variable).empty())
            output << environment->nameOf(variable);
        else
            output << 'v' << variable.id();
    }
    if (!constant_.isZero() || first)
    {
        if (!first)
            output << " + ";
        output << constant_.toString();
    }
    return output.str();
}

TreeExpression TreeExpression::constant(Rational value, NumericType type)
{
    TreeExpression expression;
    expression.kind_ = Kind::Constant;
    expression.type_ = type;
    expression.constant_ = std::move(value);
    return expression;
}

TreeExpression TreeExpression::variable(Variable value, NumericType type)
{
    TreeExpression expression;
    expression.kind_ = Kind::Variable;
    expression.type_ = type;
    expression.variable_ = value;
    return expression;
}

TreeExpression TreeExpression::unary(UnaryOperator operation,
                                     TreeExpression operand, NumericType type,
                                     RoundingMode rounding)
{
    TreeExpression expression;
    expression.kind_ = Kind::Unary;
    expression.type_ = type;
    expression.unaryOperator_ = operation;
    expression.roundingMode_ = rounding;
    expression.lhs_ = std::make_shared<TreeExpression>(std::move(operand));
    return expression;
}

TreeExpression TreeExpression::binary(BinaryOperator operation,
                                      TreeExpression lhs, TreeExpression rhs,
                                      NumericType type, RoundingMode rounding)
{
    TreeExpression expression;
    expression.kind_ = Kind::Binary;
    expression.type_ = type;
    expression.binaryOperator_ = operation;
    expression.roundingMode_ = rounding;
    expression.lhs_ = std::make_shared<TreeExpression>(std::move(lhs));
    expression.rhs_ = std::make_shared<TreeExpression>(std::move(rhs));
    return expression;
}

const TreeExpression& TreeExpression::lhs() const
{
    if (!lhs_)
        throw std::logic_error("tree expression has no left operand");
    return *lhs_;
}

const TreeExpression& TreeExpression::rhs() const
{
    if (!rhs_)
        throw std::logic_error("tree expression has no right operand");
    return *rhs_;
}

std::optional<LinearExpression> TreeExpression::asLinear() const
{
    if (type_.kind == NumericKind::IEEEFloat)
        return std::nullopt;

    switch (kind_)
    {
    case Kind::Constant:
        return LinearExpression(constant_);
    case Kind::Variable:
        return LinearExpression(variable_);
    case Kind::Unary: {
        if (unaryOperator_ != UnaryOperator::Negate)
            return std::nullopt;
        std::optional<LinearExpression> operand = lhs().asLinear();
        return operand ? std::optional<LinearExpression>(-*operand)
                       : std::nullopt;
    }
    case Kind::Binary: {
        std::optional<LinearExpression> left = lhs().asLinear();
        std::optional<LinearExpression> right = rhs().asLinear();
        if (!left || !right)
            return std::nullopt;
        switch (binaryOperator_)
        {
        case BinaryOperator::Add:
            return *left + *right;
        case BinaryOperator::Subtract:
            return *left - *right;
        case BinaryOperator::Multiply:
            if (left->terms().empty())
                return *right * left->constant();
            if (right->terms().empty())
                return *left * right->constant();
            return std::nullopt;
        case BinaryOperator::Divide:
            if (right->terms().empty() && !right->constant().isZero())
                return *left * (Rational(1) / right->constant());
            return std::nullopt;
        case BinaryOperator::Remainder:
            return std::nullopt;
        }
    }
    }
    return std::nullopt;
}

LinearConstraint::LinearConstraint(LinearExpression expression,
                                   ConstraintKind kind)
    : expression_(std::move(expression)), kind_(kind)
{
}

std::string LinearConstraint::toString(const VariableEnvironment* environment) const
{
    const char* relation;
    switch (kind_)
    {
    case ConstraintKind::Equal:
        relation = "==";
        break;
    case ConstraintKind::NotEqual:
        relation = "!=";
        break;
    case ConstraintKind::LessThan:
        relation = "<";
        break;
    case ConstraintKind::LessEqual:
        relation = "<=";
        break;
    case ConstraintKind::GreaterThan:
        relation = ">";
        break;
    case ConstraintKind::GreaterEqual:
        relation = ">=";
        break;
    default:
        throw std::logic_error("unknown linear constraint kind");
    }
    return expression_.toString(environment) + ' ' + relation + " 0";
}

TreeConstraint::TreeConstraint(TreeExpression expression, ConstraintKind kind)
    : expression_(std::move(expression)), kind_(kind)
{
}

LinearConstraint equal(LinearExpression lhs,
                                            LinearExpression rhs)
{
    return LinearConstraint(std::move(lhs) - rhs, ConstraintKind::Equal);
}

LinearConstraint notEqual(LinearExpression lhs,
                                               LinearExpression rhs)
{
    return LinearConstraint(std::move(lhs) - rhs, ConstraintKind::NotEqual);
}

LinearConstraint lessEqual(LinearExpression lhs,
                                                LinearExpression rhs)
{
    return LinearConstraint(std::move(lhs) - rhs, ConstraintKind::LessEqual);
}

LinearConstraint lessThan(LinearExpression lhs,
                                               LinearExpression rhs)
{
    return LinearConstraint(std::move(lhs) - rhs, ConstraintKind::LessThan);
}

LinearConstraint greaterEqual(LinearExpression lhs,
                                                   LinearExpression rhs)
{
    return LinearConstraint(std::move(lhs) - rhs, ConstraintKind::GreaterEqual);
}

LinearConstraint greaterThan(LinearExpression lhs,
                                                  LinearExpression rhs)
{
    return LinearConstraint(std::move(lhs) - rhs, ConstraintKind::GreaterThan);
}

} // namespace SVF::AbstractDomain
