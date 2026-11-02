//===- ConvexPolyhedraDomain.cpp -- Native relational numerical domain -------===//
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
// Contributors: Jiawei Yang
//
//===----------------------------------------------------------------------===//

#include "AE/Core/ConvexPolyhedraDomain.h"

#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <map>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

namespace SVF
{

namespace
{
struct Inequality
{
    std::vector<Rational> coefficients;
    Rational bound;
    bool strict = false;
};

/// A homogeneous generator `(t,x)`, or `(t,epsilon,x)` for an NNC system.
/// A positive `t` denotes the point `x/t`; `t == 0` denotes a recession
/// direction. In an NNC system `epsilon > 0` marks an included point and
/// `epsilon == 0` a closure point. A line is stored once and marked explicitly
/// rather than encoded as two opposite rays.
struct Generator
{
    std::vector<mpz_class> coordinates;
    bool line = false;
    /// Bit i is set when this generator saturates processed homogeneous
    /// constraint i.  The cache is rebuilt whenever its constraint basis
    /// changes; it is not part of generator identity.
    std::vector<std::uint64_t> saturation;
};

struct GeneratorSystem
{
    std::vector<Generator> generators;
    /// Homogeneous halfspaces already reflected by `generators`. Saturation
    /// bit i in every generator refers to row i here.
    std::vector<std::vector<mpz_class>> constraints;
    bool nnc = false;
};

std::vector<Inequality> projectRows(
    std::vector<Inequality> inequalities,
    const std::vector<std::size_t>& dimensions);
bool feasible(std::vector<Inequality> inequalities, std::size_t dimensions);
bool entailsInequality(const std::vector<Inequality>& premises,
                       std::size_t dimensions,
                       const Inequality& conclusion);
std::vector<Inequality> irredundant(std::vector<Inequality> inequalities,
                                    std::size_t dimensions);
GeneratorSystem generatorsFromConstraints(
    const std::vector<Inequality>& inequalities, std::size_t dimensions);
std::vector<Inequality> constraintsFromGenerators(
    const std::vector<Generator>& generators, std::size_t dimensions,
    bool nnc,
    GeneratorSystem* minimizedPrimal = nullptr,
    GeneratorSystem* polar = nullptr);
std::vector<Generator> uniqueGenerators(std::vector<Generator> generators);
std::vector<Generator> sortUniqueGenerators(
    std::vector<Generator> generators);
std::vector<Generator> mergeGeneratorSets(
    const std::vector<Generator>& lhs, const std::vector<Generator>& rhs);
bool generatorsEntail(const std::vector<Generator>& generators,
                      const Inequality& inequality, bool nnc);
Interval boundFromGenerators(const std::vector<Generator>& generators,
                             const std::vector<Rational>& objective,
                             const Rational& constant, bool nnc);
GeneratorSystem intersectGeneratorsWithConstraints(
    std::vector<Generator> generators,
    std::vector<std::vector<mpz_class>> processed,
    const std::vector<Inequality>& added, std::size_t dimensions, bool nnc);
GeneratorSystem intersectCone(
    std::vector<Generator> generators,
    std::vector<std::vector<mpz_class>> processedConstraints,
    std::vector<std::vector<mpz_class>> constraints,
    bool inputCoherent = false);
GeneratorSystem primalSystem(const GeneratorSystem& polar);
std::vector<Inequality> inequalitiesFromForms(
    const std::vector<Generator>& forms, bool nnc);
std::vector<std::vector<mpz_class>> generatorHalfspaces(
    const std::vector<Generator>& generators);
void setGeneratorCoordinates(Generator& generator,
                             const std::vector<mpq_class>& coordinates);

std::size_t generatorVariableOffset(bool nnc)
{
    return nnc ? 2 : 1;
}

bool hasStrictConstraint(const std::vector<Inequality>& inequalities)
{
    return std::any_of(inequalities.begin(), inequalities.end(),
                       [](const Inequality& inequality)
    {
        return inequality.strict;
    });
}

bool sameInequality(const Inequality& lhs, const Inequality& rhs)
{
    return lhs.coefficients == rhs.coefficients && lhs.bound == rhs.bound &&
           lhs.strict == rhs.strict;
}

bool hasIntegerVariable(const DimensionLayout& layout)
{
    return std::any_of(
               layout.variables().begin(), layout.variables().end(),
               [](const DimensionEntry& declaration)
    {
        return declaration.variable.type().kind == NumericKind::Integer;
    });
}

bool falseConstant(const Inequality& inequality)
{
    const bool allZero = std::all_of(
                             inequality.coefficients.begin(), inequality.coefficients.end(),
                             [](const Rational& coefficient)
    {
        return coefficient.isZero();
    });
    if (!allZero)
        return false;
    return inequality.bound.sign() < 0 ||
           (inequality.bound.isZero() && inequality.strict);
}

bool trueConstant(const Inequality& inequality)
{
    const bool allZero = std::all_of(
                             inequality.coefficients.begin(), inequality.coefficients.end(),
                             [](const Rational& coefficient)
    {
        return coefficient.isZero();
    });
    return allZero && !falseConstant(inequality);
}

Inequality scaled(const Inequality& inequality, const Rational& factor)
{
    if (factor.sign() <= 0)
    {
        assert(false && "inequality scale must be positive");
        std::abort();
    }
    Inequality result = inequality;
    for (Rational& coefficient : result.coefficients)
        coefficient *= factor;
    result.bound *= factor;
    return result;
}

Inequality add(const Inequality& lhs, const Inequality& rhs)
{
    Inequality result;
    result.coefficients.resize(lhs.coefficients.size());
    for (std::size_t index = 0; index < result.coefficients.size(); ++index)
        result.coefficients[index] =
            lhs.coefficients[index] + rhs.coefficients[index];
    result.bound = lhs.bound + rhs.bound;
    result.strict = lhs.strict || rhs.strict;
    return result;
}

std::vector<Inequality> eliminate(std::vector<Inequality> inequalities,
                                  std::size_t dimension)
{
    std::vector<Inequality> positive;
    std::vector<Inequality> negative;
    std::vector<Inequality> zero;
    for (Inequality& inequality : inequalities)
    {
        const int sign = inequality.coefficients[dimension].sign();
        if (sign > 0)
            positive.push_back(std::move(inequality));
        else if (sign < 0)
            negative.push_back(std::move(inequality));
        else
            zero.push_back(std::move(inequality));
    }

    if (positive.empty() || negative.empty())
        return zero;

    zero.reserve(zero.size() + positive.size() * negative.size());
    for (const Inequality& upper : positive)
    {
        for (const Inequality& lower : negative)
        {
            const Rational upperFactor = -lower.coefficients[dimension];
            const Rational lowerFactor = upper.coefficients[dimension];
            Inequality combined =
                add(scaled(upper, upperFactor), scaled(lower, lowerFactor));
            combined.coefficients[dimension] = Rational();
            zero.push_back(std::move(combined));
        }
    }
    return zero;
}

bool tighter(const Inequality& lhs, const Inequality& rhs)
{
    if (lhs.bound < rhs.bound)
        return true;
    if (rhs.bound < lhs.bound)
        return false;
    return lhs.strict && !rhs.strict;
}

std::vector<Inequality> normalized(std::vector<Inequality> inequalities,
                                   bool& bottom)
{
    using Key = std::vector<Rational>;
    struct KeyLess
    {
        bool operator()(const Key& lhs, const Key& rhs) const
        {
            return std::lexicographical_compare(lhs.begin(), lhs.end(),
                                                rhs.begin(), rhs.end());
        }
    };

    std::map<Key, Inequality, KeyLess> unique;
    for (Inequality inequality : inequalities)
    {
        if (falseConstant(inequality))
        {
            bottom = true;
            return {};
        }
        if (trueConstant(inequality))
            continue;

        const auto first = std::find_if(
                               inequality.coefficients.begin(), inequality.coefficients.end(),
                               [](const Rational& coefficient)
        {
            return !coefficient.isZero();
        });
        const Rational divisor = first->sign() > 0 ? *first : -*first;
        for (Rational& coefficient : inequality.coefficients)
            coefficient /= divisor;
        inequality.bound /= divisor;

        auto [it, inserted] =
            unique.emplace(inequality.coefficients, inequality);
        if (!inserted && tighter(inequality, it->second))
            it->second = std::move(inequality);
    }

    std::vector<Inequality> result;
    result.reserve(unique.size());
    for (auto& [key, inequality] : unique)
    {
        (void)key;
        result.push_back(std::move(inequality));
    }
    return result;
}

bool oppositeRows(const Inequality& lhs, const Inequality& rhs)
{
    if (lhs.strict || rhs.strict || lhs.bound != -rhs.bound ||
            lhs.coefficients.size() != rhs.coefficients.size())
        return false;
    for (std::size_t dimension = 0; dimension < lhs.coefficients.size();
            ++dimension)
    {
        if (lhs.coefficients[dimension] != -rhs.coefficients[dimension])
            return false;
    }
    return true;
}

/// Put explicit equality pairs in reduced row-echelon form and reduce every
/// inequality modulo them.  A facet of a lower-dimensional polyhedron is only
/// unique modulo its affine hull; without this step equivalent H/V histories
/// can choose different-looking rows and consequently different hashes.
std::vector<Inequality> canonicalizeAffineHull(
    std::vector<Inequality> inequalities, bool& bottom)
{
    if (inequalities.size() < 2)
        return inequalities;

    std::vector<bool> equalityMember(inequalities.size(), false);
    std::vector<Inequality> equations;
    for (std::size_t lhs = 0; lhs < inequalities.size(); ++lhs)
    {
        if (equalityMember[lhs] || inequalities[lhs].strict)
            continue;
        for (std::size_t rhs = lhs + 1; rhs < inequalities.size(); ++rhs)
        {
            if (equalityMember[rhs] ||
                    !oppositeRows(inequalities[lhs], inequalities[rhs]))
                continue;
            equalityMember[lhs] = true;
            equalityMember[rhs] = true;
            const auto first = std::find_if(
                                   inequalities[lhs].coefficients.begin(),
                                   inequalities[lhs].coefficients.end(),
                                   [](const Rational& coefficient)
            {
                return !coefficient.isZero();
            });
            equations.push_back(first->sign() > 0 ? inequalities[lhs]
                                : inequalities[rhs]);
            break;
        }
    }
    if (equations.empty())
        return inequalities;

    std::vector<std::size_t> pivots;
    std::size_t row = 0;
    const std::size_t dimensions = equations.front().coefficients.size();
    for (std::size_t dimension = 0;
            dimension < dimensions && row < equations.size(); ++dimension)
    {
        auto pivot = std::find_if(
                         equations.begin() + static_cast<std::ptrdiff_t>(row),
                         equations.end(),
                         [&](const Inequality& equation)
        {
            return !equation.coefficients[dimension].isZero();
        });
        if (pivot == equations.end())
            continue;
        std::iter_swap(
            equations.begin() + static_cast<std::ptrdiff_t>(row), pivot);
        const Rational divisor = equations[row].coefficients[dimension];
        for (Rational& coefficient : equations[row].coefficients)
            coefficient /= divisor;
        equations[row].bound /= divisor;
        for (std::size_t other = 0; other < equations.size(); ++other)
        {
            if (other == row ||
                    equations[other].coefficients[dimension].isZero())
                continue;
            const Rational factor =
                equations[other].coefficients[dimension];
            for (std::size_t column = 0; column < dimensions; ++column)
            {
                equations[other].coefficients[column] -=
                    factor * equations[row].coefficients[column];
            }
            equations[other].bound -= factor * equations[row].bound;
        }
        pivots.push_back(dimension);
        ++row;
    }
    for (std::size_t dependent = row; dependent < equations.size();
            ++dependent)
    {
        const bool zero = std::all_of(
                              equations[dependent].coefficients.begin(),
                              equations[dependent].coefficients.end(),
                              [](const Rational& coefficient)
        {
            return coefficient.isZero();
        });
        if (zero && !equations[dependent].bound.isZero())
        {
            bottom = true;
            return {};
        }
    }
    equations.resize(row);

    std::vector<Inequality> result;
    result.reserve(2 * equations.size() + inequalities.size());
    for (const Inequality& equation : equations)
    {
        result.push_back(equation);
        Inequality opposite = equation;
        for (Rational& coefficient : opposite.coefficients)
            coefficient = -coefficient;
        opposite.bound = -opposite.bound;
        result.push_back(std::move(opposite));
    }
    for (std::size_t index = 0; index < inequalities.size(); ++index)
    {
        if (equalityMember[index])
            continue;
        Inequality reduced = std::move(inequalities[index]);
        for (std::size_t equation = 0; equation < equations.size(); ++equation)
        {
            const Rational factor = reduced.coefficients[pivots[equation]];
            if (factor.isZero())
                continue;
            for (std::size_t dimension = 0; dimension < dimensions; ++dimension)
            {
                reduced.coefficients[dimension] -=
                    factor * equations[equation].coefficients[dimension];
            }
            reduced.bound -= factor * equations[equation].bound;
        }
        result.push_back(std::move(reduced));
    }
    return normalized(std::move(result), bottom);
}

Inequality negateForCounterexample(const Inequality& inequality)
{
    Inequality result = inequality;
    for (Rational& coefficient : result.coefficients)
        coefficient = -coefficient;
    result.bound = -result.bound;
    result.strict = !inequality.strict;
    return result;
}

std::vector<Inequality> constraintRows(const DimensionLayout& layout,
                                       const LinearConstraint& constraint)
{
    const auto makeRow = [&](const LinearExpression& expression, bool strict)
    {
        Inequality row;
        row.coefficients.resize(layout.size());
        for (const auto& [variable, coefficient] : expression.terms())
        {
            if (!layout.contains(variable))
            {
                assert(false && "polyhedron constraint uses an unknown variable");
                std::abort();
            }
            row.coefficients[layout.dimensionOf(variable)] = coefficient;
        }
        row.bound = -expression.constant();
        row.strict = strict;
        return row;
    };

    switch (constraint.kind())
    {
    case ConstraintKind::LessEqual:
        return {makeRow(constraint.expression(), false)};
    case ConstraintKind::LessThan:
        return {makeRow(constraint.expression(), true)};
    case ConstraintKind::GreaterEqual:
        return {makeRow(-constraint.expression(), false)};
    case ConstraintKind::GreaterThan:
        return {makeRow(-constraint.expression(), true)};
    case ConstraintKind::Equal:
        return {makeRow(constraint.expression(), false),
                makeRow(-constraint.expression(), false)};
    case ConstraintKind::NotEqual:
        return {};
    }
    return {};
}

LinearConstraint rowConstraint(const DimensionLayout& layout,
                               const Inequality& inequality)
{
    LinearExpression expression(-inequality.bound);
    for (Dimension dimension = 0; dimension < layout.size(); ++dimension)
    {
        if (!inequality.coefficients[dimension].isZero())
            expression.setCoefficient(layout.variableOf(dimension),
                                      inequality.coefficients[dimension]);
    }
    return LinearConstraint(std::move(expression),
                            inequality.strict ? ConstraintKind::LessThan
                            : ConstraintKind::LessEqual);
}

void tightenIntegerRow(Inequality& inequality,
                       const DimensionLayout& layout)
{
    mpz_class scale = 1;
    bool hasVariable = false;
    for (Dimension dimension = 0; dimension < layout.size(); ++dimension)
    {
        if (inequality.coefficients[dimension].isZero())
            continue;
        hasVariable = true;
        if (layout.typeOf(layout.variableOf(dimension)).kind !=
                NumericKind::Integer)
            return;
        const mpz_class denominator =
            inequality.coefficients[dimension].value().get_den();
        mpz_lcm(scale.get_mpz_t(), scale.get_mpz_t(),
                denominator.get_mpz_t());
    }
    if (!hasVariable)
        return;
    const mpz_class boundDenominator = inequality.bound.value().get_den();
    mpz_lcm(scale.get_mpz_t(), scale.get_mpz_t(),
            boundDenominator.get_mpz_t());

    mpz_class divisor = 0;
    for (const Rational& coefficient : inequality.coefficients)
    {
        if (coefficient.isZero())
            continue;
        const mpq_class scaledCoefficient = coefficient.value() * scale;
        const mpz_class integerCoefficient = scaledCoefficient.get_num();
        mpz_gcd(divisor.get_mpz_t(), divisor.get_mpz_t(),
                integerCoefficient.get_mpz_t());
    }
    if (divisor == 0)
        return;
    if (divisor < 0)
        divisor = -divisor;

    const mpq_class scaledBound = inequality.bound.value() * scale;
    const mpz_class integerBound = scaledBound.get_num();
    mpz_class quotient;
    mpz_fdiv_q(quotient.get_mpz_t(), integerBound.get_mpz_t(),
               divisor.get_mpz_t());
    mpz_class tightened = quotient * divisor;
    if (inequality.strict && tightened == integerBound)
        tightened -= divisor;

    inequality.bound = Rational::fromRaw(mpq_class(tightened, scale));
    inequality.strict = false;
}

} // namespace

class ConvexPolyhedraDomain::Impl
{
public:
    mutable std::vector<Inequality> inequalities;
    mutable std::vector<Generator> generators;
    mutable std::vector<std::vector<mpz_class>> generatorConstraints;
    mutable GeneratorSystem polar;
    mutable bool constraintsValid = true;
    /// Exact H rows may temporarily contain semantic redundancy after a
    /// transfer that maintained both H and V caches incrementally.
    mutable bool constraintsMinimal = true;
    mutable bool generatorsValid = false;
    /// True when generators are the minimized DD frame required by the
    /// adjacency invariant. Affine images and joins remain exact but may make
    /// this false until the next H -> V minimization.
    mutable bool generatorsMinimal = false;
    mutable bool polarValid = false;
    mutable bool generatorsExact = false;
    /// True when homogeneous coordinates include the epsilon coordinate used
    /// to distinguish included points from closure points.
    mutable bool generatorsNNC = false;
    bool bottom = false;
};

ConvexPolyhedraDomain::ConvexPolyhedraDomain(
    DimensionLayout layout, ConvexPolyhedraConfig config, bool bottom)
    : layout_(std::move(layout)), config_(std::move(config)),
      impl_(std::make_unique<Impl>())
{
    impl_->bottom = bottom;
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::top(
    const DimensionLayout& layout, const ConvexPolyhedraConfig& config)
{
    return ConvexPolyhedraDomain(layout, config, false);
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::bottom(
    const DimensionLayout& layout, const ConvexPolyhedraConfig& config)
{
    return ConvexPolyhedraDomain(layout, config, true);
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::fromConstraints(
    const DimensionLayout& layout, const LinearConstraintSet& constraints,
    const ConvexPolyhedraConfig& config)
{
    ConvexPolyhedraDomain result = top(layout, config);
    result.assumeAll(constraints);
    return result;
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::fromGeneratorsInLayout(
    const DimensionLayout& layout,
    const PolyhedraGeneratorSet& generators,
    const ConvexPolyhedraConfig& config)
{
    if (generators.empty())
    {
        ConvexPolyhedraDomain result = bottom(layout, config);
        result.recordOperation(OperationKind::GeneratorImport,
                               ApproximationKind::Exact, true);
        return result;
    }
    const bool nnc = std::any_of(
                         generators.begin(), generators.end(),
                         [](const PolyhedraGenerator& generator)
    {
        return generator.kind == PolyhedraGeneratorKind::ClosurePoint;
    });
    const bool hasIncludedPoint = std::any_of(
                                      generators.begin(), generators.end(),
                                      [](const PolyhedraGenerator& generator)
    {
        return generator.kind == PolyhedraGeneratorKind::Point;
    });
    if (!hasIncludedPoint)
    {
        assert(false && "a nonempty generator system requires an included point");
        std::abort();
    }

    // Public PPL/APRON-style NNC generators carry point/closure membership,
    // not the internal numeric epsilon witness. First compute the facets of
    // their closed hull, then mark exactly those facets strict that no
    // included point saturates. This is invariant under the internal epsilon
    // scaling chosen by a previous H -> V conversion.
    std::vector<Generator> closureGenerators;
    closureGenerators.reserve(generators.size());
    for (const PolyhedraGenerator& generator : generators)
    {
        if (generator.coordinates.size() != layout.size())
        {
            assert(false && "generator coordinates do not match the layout");
            std::abort();
        }
        std::vector<mpq_class> coordinates(layout.size() + 1);
        const bool finite =
            generator.kind == PolyhedraGeneratorKind::Point ||
            generator.kind == PolyhedraGeneratorKind::ClosurePoint;
        if (finite)
            coordinates.front() = 1;
        for (Dimension dimension = 0; dimension < layout.size();
                ++dimension)
            coordinates[dimension + 1] =
                generator.coordinates[dimension].value();
        Generator converted;
        converted.line =
            generator.kind == PolyhedraGeneratorKind::Line;
        setGeneratorCoordinates(converted, coordinates);
        closureGenerators.push_back(std::move(converted));
    }
    closureGenerators = uniqueGenerators(std::move(closureGenerators));
    std::vector<Inequality> inequalities = constraintsFromGenerators(
            closureGenerators, layout.size(), false);
    if (nnc)
    {
        for (Inequality& inequality : inequalities)
        {
            const bool includedBoundaryPoint = std::any_of(
                                                   generators.begin(), generators.end(),
                                                   [&](const PolyhedraGenerator& generator)
            {
                if (generator.kind != PolyhedraGeneratorKind::Point)
                    return false;
                Rational value;
                for (Dimension dimension = 0;
                        dimension < layout.size(); ++dimension)
                    value += inequality.coefficients[dimension] *
                             generator.coordinates[dimension];
                return value == inequality.bound;
            });
            inequality.strict = !includedBoundaryPoint;
        }
    }
    ConvexPolyhedraDomain result = top(layout, config);
    result.impl_->inequalities = std::move(inequalities);
    result.normalize();
    if (!result.impl_->bottom && nnc)
        result.ensureGenerators();
    result.recordOperation(OperationKind::GeneratorImport,
                           ApproximationKind::Exact, true);
    return result;
}

ConvexPolyhedraDomain::ConvexPolyhedraDomain(
    const ConvexPolyhedraDomain& other)
    : NumericalDomain(other), layout_(other.layout_),
      config_(other.config_), impl_(std::make_unique<Impl>(*other.impl_))
{
}

ConvexPolyhedraDomain::ConvexPolyhedraDomain(
    ConvexPolyhedraDomain&& other) noexcept = default;

ConvexPolyhedraDomain& ConvexPolyhedraDomain::operator=(
    const ConvexPolyhedraDomain& other)
{
    if (this == &other)
        return *this;
    NumericalDomain::operator=(other);
    layout_ = other.layout_;
    config_ = other.config_;
    impl_ = std::make_unique<Impl>(*other.impl_);
    return *this;
}

ConvexPolyhedraDomain& ConvexPolyhedraDomain::operator=(
    ConvexPolyhedraDomain&& other) noexcept = default;

ConvexPolyhedraDomain::~ConvexPolyhedraDomain() = default;

std::unique_ptr<AbstractDomain> ConvexPolyhedraDomain::clone() const
{
    return std::make_unique<ConvexPolyhedraDomain>(*this);
}

const char* ConvexPolyhedraDomain::name() const
{
    return "ConvexPolyhedraDomain";
}

void ConvexPolyhedraDomain::assign(
    Variable target, const LinearExpression& expression)
{
    assignParallel({{target, expression}});
}

void ConvexPolyhedraDomain::assignParallel(
    const LinearAssignmentList& assignments)
{
    std::vector<Variable> variables;
    std::set<Variable> targets;
    for (const auto& assignment : assignments)
    {
        if (!targets.insert(assignment.target).second)
        {
            assert(false && "duplicate parallel target");
            std::abort();
        }
        variables.push_back(assignment.target);
        for (const auto& term : assignment.expression.terms())
            variables.push_back(term.first);
    }
    ensureVariables(variables);

    std::map<Variable, const LinearExpression*> expressions;
    for (const LinearAssignment& assignment : assignments)
    {
        if (!layout_.contains(assignment.target))
        {
            assert(false && "parallel assignment target is not in layout");
            std::abort();
        }
        if (!expressions.emplace(assignment.target, &assignment.expression)
                .second)
        {
            assert(false && "parallel assignment contains a duplicate target");
            std::abort();
        }
        for (const auto& [variable, coefficient] :
                assignment.expression.terms())
        {
            (void)coefficient;
            if (!layout_.contains(variable))
            {
                assert(false && "parallel assignment expression uses an unknown variable");
                std::abort();
            }
        }
    }
    recordOperation(OperationKind::Assignment, ApproximationKind::Exact, true);
    if (assignments.empty() || impl_->bottom)
        return;

    ensureGenerators();
    if (impl_->generatorsExact)
    {
        const std::size_t variableOffset =
            generatorVariableOffset(impl_->generatorsNNC);
        for (Generator& generator : impl_->generators)
        {
            const std::vector<mpz_class> old = generator.coordinates;
            std::vector<mpq_class> next(old.size());
            for (std::size_t coordinate = 0; coordinate < old.size();
                    ++coordinate)
                next[coordinate] = old[coordinate];
            for (const auto& [target, expression] : expressions)
            {
                mpq_class value =
                    expression->constant().value() * old.front();
                for (const auto& [variable, coefficient] : expression->terms())
                {
                    value += coefficient.value() *
                             old[layout_.dimensionOf(variable) +
                                 variableOffset];
                }
                next[layout_.dimensionOf(target) + variableOffset] =
                    std::move(value);
            }
            setGeneratorCoordinates(generator, next);
        }
        // An affine image of a generating set stays exact even when some
        // generators become redundant. Normalize/deduplicate cheaply and let
        // the next H conversion remove semantic redundancy in one batch.
        impl_->generators = uniqueGenerators(std::move(impl_->generators));
        impl_->generatorConstraints.clear();
        impl_->generatorsMinimal = false;
        impl_->polar = {};
        impl_->polarValid = false;
        invalidateConstraints();
        // Real-only states can remain V-only until a constraint operation
        // requests H. Integer states materialize H now so the existing per-row
        // tightening remains part of assignment semantics.
        if (config_.integerTightening && hasIntegerVariable(layout_))
            ensureConstraints();
        return;
    }

    ensureConstraints();

    const std::size_t dimensions = layout_.size();
    std::vector<Inequality> extended;
    extended.reserve(impl_->inequalities.size() + 2 * dimensions);

    // New variables occupy [0,n), old variables [n,2n).
    for (const Inequality& inequality : impl_->inequalities)
    {
        Inequality old;
        old.coefficients.resize(2 * dimensions);
        std::copy(inequality.coefficients.begin(),
                  inequality.coefficients.end(),
                  old.coefficients.begin() +
                  static_cast<std::ptrdiff_t>(dimensions));
        old.bound = inequality.bound;
        old.strict = inequality.strict;
        extended.push_back(std::move(old));
    }

    for (Dimension dimension = 0; dimension < dimensions; ++dimension)
    {
        Inequality equality;
        equality.coefficients.resize(2 * dimensions);
        equality.coefficients[dimension] = Rational(1);
        const Variable target = layout_.variableOf(dimension);
        const auto assigned = expressions.find(target);
        if (assigned != expressions.end())
        {
            for (const auto& [variable, coefficient] :
                    assigned->second->terms())
            {
                equality.coefficients[dimensions +
                                      layout_.dimensionOf(variable)] -= coefficient;
            }
            equality.bound = assigned->second->constant();
        }
        else
        {
            equality.coefficients[dimensions + dimension] = Rational(-1);
        }
        extended.push_back(equality);
        for (Rational& coefficient : equality.coefficients)
            coefficient = -coefficient;
        equality.bound = -equality.bound;
        extended.push_back(std::move(equality));
    }

    std::vector<std::size_t> oldDimensions(dimensions);
    std::iota(oldDimensions.begin(), oldDimensions.end(), dimensions);
    extended = projectRows(std::move(extended), oldDimensions);
    for (Inequality& inequality : extended)
        inequality.coefficients.resize(dimensions);
    impl_->inequalities = std::move(extended);
    normalize();
}

void ConvexPolyhedraDomain::substitute(
    Variable target, const LinearExpression& expression)
{
    substituteParallel({{target, expression}});
}

void ConvexPolyhedraDomain::substituteParallel(
    const LinearAssignmentList& assignments)
{
    std::vector<Variable> variables;
    std::set<Variable> targets;
    for (const auto& assignment : assignments)
    {
        if (!targets.insert(assignment.target).second)
        {
            assert(false && "duplicate parallel target");
            std::abort();
        }
        variables.push_back(assignment.target);
        for (const auto& term : assignment.expression.terms())
            variables.push_back(term.first);
    }
    ensureVariables(variables);

    std::map<Variable, LinearExpression> replacements;
    for (const LinearAssignment& assignment : assignments)
    {
        if (!layout_.contains(assignment.target))
        {
            assert(false && "substitution target is not in layout");
            std::abort();
        }
        if (!replacements.emplace(assignment.target, assignment.expression)
                .second)
        {
            assert(false && "parallel substitution contains a duplicate target");
            std::abort();
        }
        for (const auto& [variable, coefficient] :
                assignment.expression.terms())
        {
            (void)coefficient;
            if (!layout_.contains(variable))
            {
                assert(false && "substitution expression uses an unknown variable");
                std::abort();
            }
        }
    }
    recordOperation(OperationKind::Substitution, ApproximationKind::Exact,
                    true);
    if (assignments.empty() || impl_->bottom)
        return;

    LinearConstraintSet preimage;
    for (const LinearConstraint& constraint : toConstraints())
        preimage.emplace_back(
            constraint.expression().substituted(replacements),
            constraint.kind());
    *this = fromConstraints(layout_, preimage, config_);
    recordOperation(OperationKind::Substitution, ApproximationKind::Exact, true);
}

void ConvexPolyhedraDomain::assign(Variable target,
                                   const TreeExpression& expression)
{
    const std::optional<LinearExpression> linear = expression.asLinear();
    if (linear)
    {
        assign(target, *linear);
        return;
    }
    const Interval value = evaluateTreeExpression(expression);
    assignInterval(target, value);
    report(OperationKind::Assignment,
           ApproximationKind::SoundOverApproximation,
           "nonlinear or finite IEEE assignment was interval-linearized",
           false);
}

void ConvexPolyhedraDomain::assume(const LinearConstraint& constraint)
{
    ensureExpression(constraint.expression());

    recordOperation(OperationKind::Assumption, ApproximationKind::Exact, true);
    if (impl_->bottom)
        return;
    if (constraint.kind() == ConstraintKind::NotEqual)
    {
        const LinearConstraint equality(constraint.expression(),
                                        ConstraintKind::Equal);
        if (entails(equality) == CheckResult::True)
        {
            impl_->bottom = true;
            impl_->inequalities.clear();
            impl_->generators.clear();
            impl_->generatorConstraints.clear();
            impl_->polar = {};
            impl_->constraintsValid = true;
            impl_->generatorsValid = false;
            impl_->generatorsMinimal = false;
            impl_->generatorsExact = false;
            impl_->generatorsNNC = false;
            impl_->polarValid = false;
        }
        else
            report(OperationKind::Assumption,
                   ApproximationKind::SoundOverApproximation,
                   "non-convex disequality is ignored");
        return;
    }

    std::vector<Inequality> rows = constraintRows(layout_, constraint);
    // Integer guards must stay on the H-representation path. Intersecting an
    // already materialized rational generator system and then rebuilding the
    // integer-tightened constraints can admit a spurious lattice point for an
    // inconsistent affine equality plus guard. The constraint path detects
    // the empty conjunction directly.
    if (!rows.empty() && !hasIntegerVariable(layout_) &&
            impl_->generatorsValid && impl_->generatorsExact &&
            impl_->generatorsMinimal &&
            (impl_->generatorsNNC || !hasStrictConstraint(rows)))
    {
        const bool preserveConstraints =
            impl_->constraintsValid &&
            !(config_.integerTightening && hasIntegerVariable(layout_));
        GeneratorSystem system = intersectGeneratorsWithConstraints(
                                     std::move(impl_->generators),
                                     std::move(impl_->generatorConstraints),
                                     rows, layout_.size(), impl_->generatorsNNC);
        GeneratorSystem polar = primalSystem(system);
        impl_->generators = std::move(system.generators);
        impl_->generatorConstraints = std::move(system.constraints);
        impl_->polar = std::move(polar);
        impl_->polarValid = true;
        if (impl_->generators.empty())
        {
            impl_->bottom = true;
            impl_->inequalities.clear();
            impl_->generatorConstraints.clear();
            impl_->polar = {};
            impl_->constraintsValid = true;
            impl_->generatorsValid = false;
            impl_->generatorsMinimal = false;
            impl_->generatorsExact = false;
            impl_->generatorsNNC = false;
            impl_->polarValid = false;
            return;
        }
        impl_->generatorsValid = true;
        impl_->generatorsMinimal = true;
        impl_->generatorsExact = true;
        impl_->generatorsNNC = system.nnc;
        if (preserveConstraints)
        {
            impl_->inequalities.insert(impl_->inequalities.end(), rows.begin(),
                                       rows.end());
            bool impossible = false;
            impl_->inequalities =
                normalized(std::move(impl_->inequalities), impossible);
            if (impossible)
            {
                assert(false && "nonempty generator intersection has inconsistent H cache");
                std::abort();
            }
            impl_->constraintsMinimal = false;
        }
        else
            invalidateConstraints();
        if (!preserveConstraints && config_.integerTightening &&
                hasIntegerVariable(layout_))
            ensureConstraints();
        return;
    }

    ensureConstraints();
    impl_->inequalities.insert(impl_->inequalities.end(), rows.begin(),
                               rows.end());
    normalize();
    if (!impl_->bottom && hasStrictConstraint(impl_->inequalities))
        ensureGenerators();
}

void ConvexPolyhedraDomain::assumeAll(
    const LinearConstraintSet& constraints)
{
    std::vector<Variable> variables;
    for (const auto& constraint : constraints)
        for (const auto& term : constraint.expression().terms())
            variables.push_back(term.first);
    ensureVariables(variables);

    recordOperation(OperationKind::Assumption, ApproximationKind::Exact, true);
    if (impl_->bottom || constraints.empty())
        return;

    LinearConstraintSet disequalities;
    std::vector<Inequality> rows;
    for (const LinearConstraint& constraint : constraints)
    {
        if (constraint.kind() == ConstraintKind::NotEqual)
        {
            disequalities.push_back(constraint);
            continue;
        }
        std::vector<Inequality> next =
            constraintRows(layout_, constraint);
        rows.insert(rows.end(), std::make_move_iterator(next.begin()),
                    std::make_move_iterator(next.end()));
    }

    if (!rows.empty() && !hasIntegerVariable(layout_) &&
            impl_->generatorsValid && impl_->generatorsExact &&
            impl_->generatorsMinimal &&
            (impl_->generatorsNNC || !hasStrictConstraint(rows)))
    {
        const bool preserveConstraints =
            impl_->constraintsValid &&
            !(config_.integerTightening && hasIntegerVariable(layout_));
        GeneratorSystem system = intersectGeneratorsWithConstraints(
                                     std::move(impl_->generators),
                                     std::move(impl_->generatorConstraints),
                                     rows, layout_.size(), impl_->generatorsNNC);
        GeneratorSystem polar = primalSystem(system);
        impl_->generators = std::move(system.generators);
        impl_->generatorConstraints = std::move(system.constraints);
        impl_->polar = std::move(polar);
        impl_->polarValid = true;
        if (impl_->generators.empty())
        {
            impl_->bottom = true;
            impl_->inequalities.clear();
            impl_->generatorConstraints.clear();
            impl_->polar = {};
            impl_->constraintsValid = true;
            impl_->generatorsValid = false;
            impl_->generatorsMinimal = false;
            impl_->generatorsExact = false;
            impl_->generatorsNNC = false;
            impl_->polarValid = false;
        }
        else
        {
            impl_->generatorsValid = true;
            impl_->generatorsMinimal = true;
            impl_->generatorsExact = true;
            impl_->generatorsNNC = system.nnc;
            if (preserveConstraints)
            {
                impl_->inequalities.insert(impl_->inequalities.end(),
                                           rows.begin(), rows.end());
                bool impossible = false;
                impl_->inequalities = normalized(
                                          std::move(impl_->inequalities), impossible);
                if (impossible)
                {
                    assert(false && "nonempty generator intersection has inconsistent H cache");
                    std::abort();
                }
                impl_->constraintsMinimal = false;
            }
            else
                invalidateConstraints();
            if (!preserveConstraints && config_.integerTightening &&
                    hasIntegerVariable(layout_))
                ensureConstraints();
        }
    }
    else if (!rows.empty())
    {
        ensureConstraints();
        impl_->inequalities.insert(impl_->inequalities.end(), rows.begin(),
                                   rows.end());
        // A convex conjunction needs one normalization pass. Disequalities
        // are checked afterwards so x != 0 and x = 0 are order independent.
        normalize();
        if (!impl_->bottom && hasStrictConstraint(impl_->inequalities))
            ensureGenerators();
    }
    for (const LinearConstraint& disequality : disequalities)
        assume(disequality);
}

void ConvexPolyhedraDomain::assume(const TreeConstraint& constraint)
{
    const std::optional<LinearExpression> linear =
        constraint.expression().asLinear();
    if (linear)
    {
        assume(LinearConstraint(*linear, constraint.kind()));
        return;
    }
    const LinearConstraintSet consequences =
        treeConstraintConsequences(constraint);
    assumeAll(consequences);
    report(OperationKind::Assumption,
           ApproximationKind::SoundOverApproximation,
           consequences.empty()
           ? "nonlinear or finite IEEE guard had no affine consequence"
           : "nonlinear guard was reduced to sound affine consequences",
           false);
}

void ConvexPolyhedraDomain::forget(Variable variable)
{
    if (!layout_.contains(variable))
        return;

    if (!layout_.contains(variable))
    {
        assert(false && "forgotten variable is not in layout");
        std::abort();
    }
    recordOperation(OperationKind::Forget, ApproximationKind::Exact, true);
    if (impl_->bottom)
        return;

    ensureGenerators();
    if (impl_->generatorsExact)
    {
        const std::size_t variableOffset =
            generatorVariableOffset(impl_->generatorsNNC);
        const Dimension forgotten =
            layout_.dimensionOf(variable) + variableOffset;
        for (Generator& generator : impl_->generators)
            generator.coordinates[forgotten] = 0;
        Generator direction;
        direction.coordinates.resize(layout_.size() + variableOffset);
        direction.coordinates[forgotten] = 1;
        direction.line = true;
        impl_->generators.push_back(std::move(direction));
        impl_->generators = uniqueGenerators(std::move(impl_->generators));
        impl_->generatorConstraints.clear();
        impl_->generatorsMinimal = false;
        impl_->polar = {};
        impl_->polarValid = false;
        invalidateConstraints();
        if (config_.integerTightening && hasIntegerVariable(layout_))
            ensureConstraints();
        return;
    }

    ensureConstraints();
    impl_->inequalities = projectRows(
                              std::move(impl_->inequalities), {layout_.dimensionOf(variable)});
    normalize();
}

void ConvexPolyhedraDomain::changeLayout(const DimensionLayout& layout)
{
    if (layout_ == layout)
    {
        recordOperation(OperationKind::Canonicalization,
                        ApproximationKind::Exact, true);
        return;
    }
    for (const DimensionEntry& declaration : layout.variables())
    {
        if (layout_.contains(declaration.variable) &&
                layout_.typeOf(declaration.variable) != declaration.variable.type())
        {
            assert(false && "layout change modifies a variable's numeric type");
            std::abort();
        }
    }
    recordOperation(OperationKind::Canonicalization,
                    ApproximationKind::Exact, true);
    if (impl_->bottom)
    {
        layout_ = layout;
        impl_->inequalities.clear();
        impl_->generators.clear();
        impl_->generatorConstraints.clear();
        impl_->polar = {};
        impl_->constraintsValid = true;
        impl_->generatorsValid = false;
        impl_->generatorsMinimal = false;
        impl_->generatorsExact = false;
        impl_->generatorsNNC = false;
        impl_->polarValid = false;
        impl_->polar = {};
        impl_->polarValid = false;
        return;
    }

    ensureGenerators();
    if (impl_->generatorsExact)
    {
        const std::size_t variableOffset =
            generatorVariableOffset(impl_->generatorsNNC);
        std::vector<Generator> remapped;
        remapped.reserve(impl_->generators.size() + 2 * layout.size());
        for (const Generator& generator : impl_->generators)
        {
            Generator next;
            next.coordinates.resize(layout.size() + variableOffset);
            std::copy_n(generator.coordinates.begin(), variableOffset,
                        next.coordinates.begin());
            next.line = generator.line;
            for (Dimension old = 0; old < layout_.size(); ++old)
            {
                const Variable variable = layout_.variableOf(old);
                if (layout.contains(variable))
                {
                    next.coordinates[layout.dimensionOf(variable) +
                                     variableOffset] =
                                         generator.coordinates[old + variableOffset];
                }
            }
            remapped.push_back(std::move(next));
        }

        for (const DimensionEntry& declaration :
                layout.variables())
        {
            if (layout_.contains(declaration.variable))
                continue;
            Generator direction;
            direction.coordinates.resize(layout.size() +
                                         variableOffset);
            direction.coordinates[
                layout.dimensionOf(declaration.variable) +
                variableOffset] =
                    1;
            direction.line = true;
            remapped.push_back(std::move(direction));
        }

        layout_ = layout;
        impl_->generators = uniqueGenerators(std::move(remapped));
        impl_->generatorConstraints.clear();
        impl_->generatorsValid = true;
        impl_->generatorsMinimal = false;
        impl_->generatorsExact = true;
        impl_->polar = {};
        impl_->polarValid = false;
        invalidateConstraints();
        if (config_.integerTightening && hasIntegerVariable(layout_))
            ensureConstraints();
        return;
    }

    ensureConstraints();

    std::vector<std::size_t> removed;
    for (Dimension old = 0; old < layout_.size(); ++old)
    {
        if (!layout.contains(layout_.variableOf(old)))
            removed.push_back(old);
    }
    impl_->inequalities = projectRows(std::move(impl_->inequalities), removed);

    std::vector<Inequality> remapped;
    remapped.reserve(impl_->inequalities.size());
    for (Inequality inequality : impl_->inequalities)
    {
        Inequality next;
        next.coefficients.resize(layout.size());
        next.bound = inequality.bound;
        next.strict = inequality.strict;
        for (Dimension old = 0; old < layout_.size(); ++old)
        {
            const Variable variable = layout_.variableOf(old);
            if (layout.contains(variable))
                next.coefficients[layout.dimensionOf(variable)] =
                    inequality.coefficients[old];
        }
        remapped.push_back(std::move(next));
    }
    layout_ = layout;
    impl_->inequalities = std::move(remapped);

    normalize();
}

void ConvexPolyhedraDomain::expandDimensions(
    Variable source, const std::vector<DimensionEntry>& copies)
{
    if (!layout_.contains(source))
    {
        assert(false && "expanded variable is not in layout");
        std::abort();
    }
    std::set<Variable> seen;
    for (const DimensionEntry& copy : copies)
    {
        if (layout_.contains(copy.variable) ||
                !seen.insert(copy.variable).second)
        {
            assert(false && "expanded variables must be new and unique");
            std::abort();
        }
        if (copy.variable.type() != layout_.typeOf(source))
        {
            assert(false && "expanded variables must have the source numeric type");
            std::abort();
        }
    }
    if (copies.empty())
    {
        recordOperation(OperationKind::Expand, ApproximationKind::Exact, true);
        return;
    }
    if (impl_->bottom)
    {
        changeLayout(layout_.add(copies));
        recordOperation(OperationKind::Expand, ApproximationKind::Exact, true);
        return;
    }

    ensureConstraints();
    const DimensionLayout oldLayout = layout_;
    const DimensionLayout expandedLayout = layout_.add(copies);
    const std::vector<Inequality> original = impl_->inequalities;
    std::vector<Inequality> expanded;
    expanded.reserve(original.size() * (copies.size() + 1));
    // APRON expansion is the conjunction of one renamed copy of the original
    // H system per representative. Building that fiber product in one batch
    // avoids a complete H/V cycle for every new dimension.
    std::vector<Variable> representatives{source};
    representatives.reserve(copies.size() + 1);
    for (const DimensionEntry& copy : copies)
        representatives.push_back(copy.variable);
    for (Variable representative : representatives)
    {
        for (const Inequality& inequality : original)
        {
            Inequality duplicated;
            duplicated.coefficients.resize(expandedLayout.size());
            duplicated.bound = inequality.bound;
            duplicated.strict = inequality.strict;
            for (Dimension old = 0; old < oldLayout.size(); ++old)
            {
                const Variable oldVariable = oldLayout.variableOf(old);
                const Variable nextVariable =
                    oldVariable == source ? representative : oldVariable;
                duplicated.coefficients[
                    expandedLayout.dimensionOf(nextVariable)] +=
                        inequality.coefficients[old];
            }
            expanded.push_back(std::move(duplicated));
        }
    }
    bool impossible = false;
    layout_ = expandedLayout;
    impl_->inequalities = normalized(std::move(expanded), impossible);
    if (impossible)
    {
        assert(false && "expanding a feasible polyhedron became bottom");
        std::abort();
    }
    impl_->constraintsValid = true;
    impl_->constraintsMinimal = false;
    invalidateGenerators();
    recordOperation(OperationKind::Expand, ApproximationKind::Exact, true);
}

void ConvexPolyhedraDomain::fold(
    Variable target, const std::vector<Variable>& folded)
{
    std::vector<Variable> variables = folded;
    variables.push_back(target);
    ensureVariables(variables);

    if (!layout_.contains(target))
    {
        assert(false && "fold target is not in layout");
        std::abort();
    }
    std::set<Variable> seen;
    std::vector<Variable> sources{target};
    for (Variable variable : folded)
    {
        if (variable == target || !layout_.contains(variable) ||
                !seen.insert(variable).second)
        {
            assert(false && "folded variables must be distinct non-target dimensions");
            std::abort();
        }
        if (layout_.typeOf(variable) != layout_.typeOf(target))
        {
            assert(false && "folded variables must have the target numeric type");
            std::abort();
        }
        sources.push_back(variable);
    }
    if (folded.empty())
    {
        recordOperation(OperationKind::Fold, ApproximationKind::Exact, true);
        return;
    }

    const DimensionLayout foldedLayout =
        layout_.remove(folded);
    if (impl_->bottom)
    {
        layout_ = foldedLayout;
        recordOperation(OperationKind::Fold, ApproximationKind::Exact, true);
        return;
    }

    ensureGenerators();
    const bool nnc = impl_->generatorsNNC;
    const std::size_t offset = generatorVariableOffset(nnc);
    std::vector<Generator> foldedGenerators;
    foldedGenerators.reserve(impl_->generators.size() * sources.size());
    // Each fold branch is a linear projection/renaming of the same generator
    // system. The convex hull of those images is represented exactly by their
    // generator union, so no branch-local H materialization is needed.
    for (Variable source : sources)
    {
        for (const Generator& generator : impl_->generators)
        {
            Generator mapped;
            mapped.coordinates.resize(foldedLayout.size() + offset);
            mapped.line = generator.line;
            for (std::size_t coordinate = 0; coordinate < offset;
                    ++coordinate)
                mapped.coordinates[coordinate] =
                    generator.coordinates[coordinate];
            for (Dimension next = 0; next < foldedLayout.size(); ++next)
            {
                const Variable variable = foldedLayout.variableOf(next);
                const Variable oldVariable =
                    variable == target ? source : variable;
                mapped.coordinates[next + offset] =
                    generator.coordinates[
                        layout_.dimensionOf(oldVariable) + offset];
            }
            foldedGenerators.push_back(std::move(mapped));
        }
    }

    ConvexPolyhedraDomain result = top(foldedLayout, config_);
    result.impl_->inequalities.clear();
    result.impl_->constraintsValid = false;
    result.impl_->constraintsMinimal = false;
    result.impl_->generators =
        uniqueGenerators(std::move(foldedGenerators));
    result.impl_->generatorConstraints.clear();
    result.impl_->generatorsValid = true;
    result.impl_->generatorsMinimal = false;
    result.impl_->generatorsExact = true;
    result.impl_->generatorsNNC = nnc;
    result.impl_->polar = {};
    result.impl_->polarValid = false;
    if (config_.integerTightening && hasIntegerVariable(foldedLayout))
        result.ensureConstraints();
    *this = std::move(result);
    recordOperation(OperationKind::Fold, ApproximationKind::Exact, true);
}

CheckResult ConvexPolyhedraDomain::entails(
    const LinearConstraint& constraint) const
{
    for (const auto& term : constraint.expression().terms())
        if (!layout_.contains(term.first))
        {
            auto extended = *this;
            extended.ensureExpression(constraint.expression());
            return extended.entails(constraint);
        }

    if (impl_->bottom)
        return CheckResult::True;
    if (constraint.kind() == ConstraintKind::NotEqual)
    {
        // A disequality is provable when one strict side is entailed.
        const LinearConstraint less(constraint.expression(),
                                    ConstraintKind::LessThan);
        const LinearConstraint greater(constraint.expression(),
                                       ConstraintKind::GreaterThan);
        return entails(less) == CheckResult::True ||
               entails(greater) == CheckResult::True
               ? CheckResult::True
               : CheckResult::Unknown;
    }
    if (constraint.kind() == ConstraintKind::Equal)
    {
        const LinearConstraint lower(constraint.expression(),
                                     ConstraintKind::LessEqual);
        const LinearConstraint upper(-constraint.expression(),
                                     ConstraintKind::LessEqual);
        return entails(lower) == CheckResult::True &&
               entails(upper) == CheckResult::True
               ? CheckResult::True
               : CheckResult::Unknown;
    }
    const std::vector<Inequality> rows =
        constraintRows(layout_, constraint);
    if (rows.size() != 1)
        return CheckResult::Unknown;
    if (impl_->generatorsValid && impl_->generatorsExact)
        return generatorsEntail(impl_->generators, rows.front(),
                                impl_->generatorsNNC)
               ? CheckResult::True
               : CheckResult::Unknown;
    ensureConstraints();
    return entailsInequality(impl_->inequalities, layout_.size(),
                             rows.front())
           ? CheckResult::True
           : CheckResult::Unknown;
}

Interval ConvexPolyhedraDomain::bound(Variable variable) const
{
    if (isBottom())
        return Interval::bottom();
    if (!layout_.contains(variable))
        return Interval::top();

    if (!layout_.contains(variable))
    {
        assert(false && "bounded variable is not in layout");
        std::abort();
    }
    if (impl_->bottom)
        return Interval(Bound::plusInfinity(), Bound::minusInfinity());

    if (impl_->generatorsValid && impl_->generatorsExact)
    {
        std::vector<Rational> objective(layout_.size());
        objective[layout_.dimensionOf(variable)] = Rational(1);
        return boundFromGenerators(impl_->generators, objective, Rational(),
                                   impl_->generatorsNNC);
    }
    ensureConstraints();

    const Dimension target = layout_.dimensionOf(variable);
    std::vector<std::size_t> removed;
    for (Dimension dimension = 0; dimension < layout_.size(); ++dimension)
    {
        if (dimension != target)
            removed.push_back(dimension);
    }
    const std::vector<Inequality> projected =
        projectRows(impl_->inequalities, removed);
    Bound lower = Bound::minusInfinity();
    Bound upper = Bound::plusInfinity();
    for (const Inequality& inequality : projected)
    {
        const Rational coefficient = inequality.coefficients[target];
        if (coefficient.sign() > 0)
        {
            const Bound candidate = Bound::finite(
                                        inequality.bound / coefficient, inequality.strict);
            upper = Bound::min(upper, candidate);
        }
        else if (coefficient.sign() < 0)
        {
            const Bound candidate = Bound::finite(
                                        inequality.bound / coefficient, inequality.strict);
            if (lower.isMinusInfinity() ||
                    lower.value() < candidate.value() ||
                    (lower.value() == candidate.value() &&
                     candidate.isStrict() && !lower.isStrict()))
                lower = candidate;
        }
    }
    return Interval(lower, upper);
}

Interval ConvexPolyhedraDomain::bound(
    const LinearExpression& expression) const
{
    if (isBottom())
        return Interval::bottom();
    for (const auto& term : expression.terms())
        if (!layout_.contains(term.first))
            return Interval::top();

    for (const auto& [variable, coefficient] : expression.terms())
    {
        (void)coefficient;
        if (!layout_.contains(variable))
        {
            assert(false && "bounded expression uses an unknown variable");
            std::abort();
        }
    }
    if (impl_->bottom)
        return Interval(Bound::plusInfinity(), Bound::minusInfinity());
    if (expression.terms().empty())
        return Interval::singleton(expression.constant());

    if (impl_->generatorsValid && impl_->generatorsExact)
    {
        std::vector<Rational> objective(layout_.size());
        for (const auto& [variable, coefficient] : expression.terms())
            objective[layout_.dimensionOf(variable)] = coefficient;
        return boundFromGenerators(impl_->generators, objective,
                                   expression.constant(),
                                   impl_->generatorsNNC);
    }
    ensureConstraints();

    const std::size_t dimensions = layout_.size();
    const std::size_t valueDimension = dimensions;
    std::vector<Inequality> extended = impl_->inequalities;
    for (Inequality& inequality : extended)
        inequality.coefficients.resize(dimensions + 1);

    Inequality equality;
    equality.coefficients.resize(dimensions + 1);
    equality.coefficients[valueDimension] = Rational(1);
    for (const auto& [variable, coefficient] : expression.terms())
        equality.coefficients[layout_.dimensionOf(variable)] =
            -coefficient;
    equality.bound = expression.constant();
    extended.push_back(equality);
    for (Rational& coefficient : equality.coefficients)
        coefficient = -coefficient;
    equality.bound = -equality.bound;
    extended.push_back(std::move(equality));

    std::vector<std::size_t> removed(dimensions);
    std::iota(removed.begin(), removed.end(), 0);
    const std::vector<Inequality> projected =
        projectRows(std::move(extended), removed);
    Bound lower = Bound::minusInfinity();
    Bound upper = Bound::plusInfinity();
    for (const Inequality& inequality : projected)
    {
        const Rational coefficient =
            inequality.coefficients[valueDimension];
        if (coefficient.sign() > 0)
        {
            upper = Bound::min(
                        upper, Bound::finite(inequality.bound / coefficient,
                                             inequality.strict));
        }
        else if (coefficient.sign() < 0)
        {
            const Bound candidate = Bound::finite(
                                        inequality.bound / coefficient, inequality.strict);
            if (lower.isMinusInfinity() || lower.value() < candidate.value() ||
                    (lower.value() == candidate.value() && candidate.isStrict() &&
                     !lower.isStrict()))
                lower = candidate;
        }
    }
    return Interval(lower, upper);
}

LinearConstraintSet ConvexPolyhedraDomain::toConstraints() const
{
    LinearConstraintSet result;
    if (impl_->bottom)
    {
        result.emplace_back(LinearExpression(Rational(1)),
                            ConstraintKind::LessEqual);
        return result;
    }
    ensureConstraints();
    result.reserve(impl_->inequalities.size());
    for (const Inequality& inequality : impl_->inequalities)
        result.push_back(rowConstraint(layout_, inequality));
    return result;
}

PolyhedraGeneratorSet ConvexPolyhedraDomain::toGenerators() const
{
    PolyhedraGeneratorSet result;
    if (impl_->bottom)
    {
        recordOperation(OperationKind::GeneratorExport,
                        ApproximationKind::Exact, true);
        return result;
    }
    ensureGenerators();
    const std::size_t offset =
        generatorVariableOffset(impl_->generatorsNNC);
    result.reserve(impl_->generators.size());
    for (const Generator& generator : impl_->generators)
    {
        PolyhedraGenerator exported;
        if (generator.line)
            exported.kind = PolyhedraGeneratorKind::Line;
        else if (generator.coordinates.front() == 0)
            exported.kind = PolyhedraGeneratorKind::Ray;
        else if (impl_->generatorsNNC && generator.coordinates[1] == 0)
            exported.kind = PolyhedraGeneratorKind::ClosurePoint;
        else
            exported.kind = PolyhedraGeneratorKind::Point;
        exported.coordinates.reserve(layout_.size());
        for (Dimension dimension = 0; dimension < layout_.size();
                ++dimension)
        {
            mpq_class coordinate(
                generator.coordinates[dimension + offset]);
            if (generator.coordinates.front() != 0)
                coordinate /= generator.coordinates.front();
            exported.coordinates.push_back(
                Rational::fromRaw(coordinate));
        }
        result.push_back(std::move(exported));
    }
    recordOperation(OperationKind::GeneratorExport,
                    ApproximationKind::Exact, true);
    return result;
}

void ConvexPolyhedraDomain::close()
{
    recordOperation(OperationKind::TopologicalClosure,
                    ApproximationKind::Exact, true);
    if (impl_->bottom)
        return;
    ensureConstraints();
    for (Inequality& inequality : impl_->inequalities)
        inequality.strict = false;
    normalize();
}

void ConvexPolyhedraDomain::canonicalize()
{
    if (impl_->constraintsValid && impl_->constraintsMinimal)
    {
        recordOperation(OperationKind::Canonicalization,
                        ApproximationKind::Exact, true);
        return;
    }
    normalize();
    recordOperation(OperationKind::Canonicalization,
                    ApproximationKind::Exact, true);
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::join(
    const ConvexPolyhedraDomain& other) const
{
    requireCompatible(other);
    if (layout_ != other.layout_)
    {
        const auto merged = layout_.merge(other.layout_);
        auto left = *this;
        auto right = other;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.join(right);
    }

    requirePolyhedron(other);
    if (impl_->bottom)
    {
        ConvexPolyhedraDomain result(other);
        result.recordOperation(OperationKind::Join,
                               ApproximationKind::Exact, true);
        return result;
    }
    if (other.impl_->bottom)
    {
        ConvexPolyhedraDomain result(*this);
        result.recordOperation(OperationKind::Join,
                               ApproximationKind::Exact, true);
        return result;
    }

    // In homogeneous V form the NNC convex hull is the cone generated by the
    // union. Epsilon-positive generators retain included points while
    // epsilon-zero generators retain closure points.
    ensureGenerators();
    other.ensureGenerators();

    // NewPolka exposes this as its lazy join algorithm: the exact hull is the
    // cone generated by the union. Like that algorithm, avoid a geometric
    // inclusion test here: it costs constraint-by-generator products on every
    // join and any interior generators are removed by the one deferred DD
    // materialization. Keep a sorted primitive union throughout a join chain.
    ConvexPolyhedraDomain result = top(layout_, config_);
    const bool resultNNC =
        impl_->generatorsNNC || other.impl_->generatorsNNC;
    const auto promote = [resultNNC](const std::vector<Generator>& source,
                                     bool sourceNNC)
    {
        if (!resultNNC || sourceNNC)
            return source;
        std::vector<Generator> promoted;
        promoted.reserve(2 * source.size());
        for (const Generator& generator : source)
        {
            Generator closure = generator;
            closure.coordinates.insert(closure.coordinates.begin() + 1, 0);
            closure.saturation.clear();
            promoted.push_back(closure);
            if (!generator.line && generator.coordinates.front() > 0)
            {
                Generator included = std::move(closure);
                included.coordinates[1] = included.coordinates.front();
                promoted.push_back(std::move(included));
            }
        }
        return sortUniqueGenerators(std::move(promoted));
    };
    const std::vector<Generator> lhs =
        promote(impl_->generators, impl_->generatorsNNC);
    const std::vector<Generator> rhs =
        promote(other.impl_->generators, other.impl_->generatorsNNC);
    result.impl_->generators = mergeGeneratorSets(lhs, rhs);
    result.impl_->generatorConstraints.clear();
    result.impl_->polar = {};
    result.impl_->constraintsValid = false;
    result.impl_->constraintsMinimal = false;
    result.impl_->generatorsValid = true;
    result.impl_->generatorsMinimal = false;
    result.impl_->generatorsExact = true;
    result.impl_->generatorsNNC = resultNNC;
    result.impl_->polarValid = false;
    if (config_.integerTightening && hasIntegerVariable(layout_))
        result.ensureConstraints();
    result.recordOperation(OperationKind::Join, ApproximationKind::Exact,
                           true);
    return result;
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::meet(
    const ConvexPolyhedraDomain& other) const
{
    requireCompatible(other);
    if (layout_ != other.layout_)
    {
        const auto merged = layout_.merge(other.layout_);
        auto left = *this;
        auto right = other;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.meet(right);
    }

    ConvexPolyhedraDomain result(*this);
    result.meetDomain(other);
    result.recordOperation(OperationKind::Meet, ApproximationKind::Exact,
                           true);
    return result;
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::widen(
    const ConvexPolyhedraDomain& next, const WideningPolicy& policy) const
{
    requireCompatible(next);
    if (layout_ != next.layout_)
    {
        const auto merged = layout_.merge(next.layout_);
        auto left = *this;
        auto right = next;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.widen(right, policy);
    }

    requirePolyhedron(next);
    if (impl_->bottom)
    {
        ConvexPolyhedraDomain result(next);
        result.recordOperation(OperationKind::Widening,
                               ApproximationKind::SoundOverApproximation,
                               true);
        return result;
    }
    if (next.impl_->bottom)
    {
        ConvexPolyhedraDomain result(*this);
        result.recordOperation(OperationKind::Widening,
                               ApproximationKind::SoundOverApproximation,
                               true);
        return result;
    }
    ensureConstraints();
    next.ensureConstraints();
    ConvexPolyhedraDomain result = top(layout_, config_);
    for (const Inequality& inequality : impl_->inequalities)
    {
        if (entailsInequality(next.impl_->inequalities, layout_.size(),
                              inequality))
            result.impl_->inequalities.push_back(inequality);
    }
    result.normalize();
    const auto retainApplicableThreshold =
        [&](const LinearConstraint& threshold)
    {
        if (entails(threshold) == CheckResult::True &&
                next.entails(threshold) == CheckResult::True)
            result.assume(threshold);
    };
    for (const Rational& threshold : policy.thresholds)
    {
        for (const DimensionEntry& declaration :
                layout_.variables())
        {
            retainApplicableThreshold(
                lessEqual(LinearExpression(declaration.variable),
                          LinearExpression(threshold)));
            retainApplicableThreshold(
                greaterEqual(LinearExpression(declaration.variable),
                             LinearExpression(threshold)));
        }
    }
    for (const LinearConstraint& threshold : policy.linearThresholds)
        retainApplicableThreshold(threshold);
    result.recordOperation(OperationKind::Widening,
                           ApproximationKind::SoundOverApproximation, true);
    return result;
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::narrow(
    const ConvexPolyhedraDomain& next) const
{
    requireCompatible(next);
    if (layout_ != next.layout_)
    {
        const auto merged = layout_.merge(next.layout_);
        auto left = *this;
        auto right = next;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.narrow(right);
    }

    if (next.isSubsetOf(*this) != CheckResult::True)
    {
        assert(false && "narrowing requires next to be included in current");
        std::abort();
    }
    ConvexPolyhedraDomain result = meet(next);
    result.recordOperation(OperationKind::Narrowing,
                           ApproximationKind::Exact, true);
    return result;
}

bool ConvexPolyhedraDomain::hasCompatibleDomain(
    const AbstractDomain& other) const
{
    const auto* polyhedron = other.isDomain<ConvexPolyhedraDomain>()
                             ? &static_cast<const ConvexPolyhedraDomain&>(other)
                             : nullptr;
    return polyhedron &&
           config_.operationCompatible(polyhedron->config_);
}

void ConvexPolyhedraDomain::joinDomain(const AbstractDomain& other)
{
    const auto& alignedOperand = requirePolyhedron(other);
    if (layout_ != alignedOperand.layout_)
    {
        *this = join(alignedOperand);
        return;
    }

    *this = join(requirePolyhedron(other));
}

void ConvexPolyhedraDomain::meetDomain(const AbstractDomain& other)
{
    if (this == &other)
    {
        recordOperation(OperationKind::Meet, ApproximationKind::Exact, true);
        return;
    }
    const auto& alignedOperand = requirePolyhedron(other);
    if (layout_ != alignedOperand.layout_)
    {
        *this = meet(alignedOperand);
        return;
    }

    const ConvexPolyhedraDomain& polyhedron = requirePolyhedron(other);
    if (impl_->bottom || polyhedron.impl_->bottom)
    {
        impl_->bottom = true;
        impl_->inequalities.clear();
        impl_->generators.clear();
        impl_->generatorConstraints.clear();
        impl_->polar = {};
        impl_->constraintsValid = true;
        impl_->generatorsValid = false;
        impl_->generatorsMinimal = false;
        impl_->generatorsExact = false;
        impl_->generatorsNNC = false;
        impl_->polarValid = false;
        return;
    }
    polyhedron.ensureConstraints();
    if (impl_->generatorsValid && impl_->generatorsExact &&
            impl_->generatorsMinimal &&
            (impl_->generatorsNNC ||
             !hasStrictConstraint(polyhedron.impl_->inequalities)))
    {
        const bool preserveConstraints =
            impl_->constraintsValid &&
            !(config_.integerTightening && hasIntegerVariable(layout_));
        GeneratorSystem system = intersectGeneratorsWithConstraints(
                                     std::move(impl_->generators),
                                     std::move(impl_->generatorConstraints),
                                     polyhedron.impl_->inequalities, layout_.size(),
                                     impl_->generatorsNNC);
        GeneratorSystem polar = primalSystem(system);
        impl_->generators = std::move(system.generators);
        impl_->generatorConstraints = std::move(system.constraints);
        impl_->polar = std::move(polar);
        impl_->polarValid = true;
        if (impl_->generators.empty())
        {
            impl_->bottom = true;
            impl_->inequalities.clear();
            impl_->generatorConstraints.clear();
            impl_->polar = {};
            impl_->constraintsValid = true;
            impl_->generatorsValid = false;
            impl_->generatorsMinimal = false;
            impl_->generatorsExact = false;
            impl_->generatorsNNC = false;
            impl_->polarValid = false;
            return;
        }
        impl_->generatorsValid = true;
        impl_->generatorsMinimal = true;
        impl_->generatorsExact = true;
        impl_->generatorsNNC = system.nnc;
        if (preserveConstraints)
        {
            impl_->inequalities.insert(
                impl_->inequalities.end(),
                polyhedron.impl_->inequalities.begin(),
                polyhedron.impl_->inequalities.end());
            bool impossible = false;
            impl_->inequalities =
                normalized(std::move(impl_->inequalities), impossible);
            if (impossible)
            {
                assert(false && "nonempty generator intersection has inconsistent H cache");
                std::abort();
            }
            impl_->constraintsMinimal = false;
        }
        else
            invalidateConstraints();
        if (!preserveConstraints && config_.integerTightening &&
                hasIntegerVariable(layout_))
            ensureConstraints();
        return;
    }

    ensureConstraints();
    if (polyhedron.impl_->generatorsValid &&
            polyhedron.impl_->generatorsExact &&
            polyhedron.impl_->generatorsMinimal &&
            (polyhedron.impl_->generatorsNNC ||
             !hasStrictConstraint(impl_->inequalities)))
    {
        const bool preserveConstraints =
            !(config_.integerTightening && hasIntegerVariable(layout_));
        GeneratorSystem system = intersectGeneratorsWithConstraints(
                                     polyhedron.impl_->generators,
                                     polyhedron.impl_->generatorConstraints,
                                     impl_->inequalities, layout_.size(),
                                     polyhedron.impl_->generatorsNNC);
        GeneratorSystem polar = primalSystem(system);
        impl_->generators = std::move(system.generators);
        impl_->generatorConstraints = std::move(system.constraints);
        impl_->polar = std::move(polar);
        impl_->polarValid = true;
        if (impl_->generators.empty())
        {
            impl_->bottom = true;
            impl_->inequalities.clear();
            impl_->generatorConstraints.clear();
            impl_->polar = {};
            impl_->constraintsValid = true;
            impl_->generatorsValid = false;
            impl_->generatorsMinimal = false;
            impl_->generatorsExact = false;
            impl_->generatorsNNC = false;
            impl_->polarValid = false;
            return;
        }
        impl_->generatorsValid = true;
        impl_->generatorsMinimal = true;
        impl_->generatorsExact = true;
        impl_->generatorsNNC = system.nnc;
        if (preserveConstraints)
        {
            impl_->inequalities.insert(
                impl_->inequalities.end(),
                polyhedron.impl_->inequalities.begin(),
                polyhedron.impl_->inequalities.end());
            bool impossible = false;
            impl_->inequalities =
                normalized(std::move(impl_->inequalities), impossible);
            if (impossible)
            {
                assert(false && "nonempty generator intersection has inconsistent H cache");
                std::abort();
            }
            impl_->constraintsMinimal = false;
        }
        else
            invalidateConstraints();
        if (!preserveConstraints && config_.integerTightening &&
                hasIntegerVariable(layout_))
            ensureConstraints();
        return;
    }

    impl_->inequalities.insert(impl_->inequalities.end(),
                               polyhedron.impl_->inequalities.begin(),
                               polyhedron.impl_->inequalities.end());
    normalize();
}

void ConvexPolyhedraDomain::widenDomain(const AbstractDomain& next)
{
    const auto& alignedOperand = requirePolyhedron(next);
    if (layout_ != alignedOperand.layout_)
    {
        *this = widen(alignedOperand);
        return;
    }

    *this = widen(requirePolyhedron(next));
}

void ConvexPolyhedraDomain::narrowDomain(const AbstractDomain& next)
{
    const auto& alignedOperand = requirePolyhedron(next);
    if (layout_ != alignedOperand.layout_)
    {
        *this = narrow(alignedOperand);
        return;
    }

    *this = narrow(requirePolyhedron(next));
}

bool ConvexPolyhedraDomain::isBottomDomain() const
{
    return impl_->bottom;
}

bool ConvexPolyhedraDomain::isTopDomain() const
{
    ensureConstraints();
    return !impl_->bottom && impl_->inequalities.empty();
}

bool ConvexPolyhedraDomain::leqDomain(const AbstractDomain& other) const
{
    const auto& rhs = requirePolyhedron(other);
    requireCompatible(rhs);
    if (layout_ != rhs.layout_)
    {
        const auto merged = layout_.merge(rhs.layout_);
        auto left = *this;
        auto right = rhs;
        left.changeLayout(merged);
        right.changeLayout(merged);
        return left.leqDomain(right);
    }

    const ConvexPolyhedraDomain& polyhedron = requirePolyhedron(other);
    if (impl_->bottom)
        return true;
    if (polyhedron.impl_->bottom)
        return false;
    polyhedron.ensureConstraints();
    if (impl_->generatorsValid && impl_->generatorsExact)
    {
        return std::all_of(
                   polyhedron.impl_->inequalities.begin(),
                   polyhedron.impl_->inequalities.end(),
                   [&](const Inequality& inequality)
        {
            return generatorsEntail(impl_->generators, inequality,
                                    impl_->generatorsNNC);
        });
    }
    ensureConstraints();
    return std::all_of(
               polyhedron.impl_->inequalities.begin(),
               polyhedron.impl_->inequalities.end(),
               [&](const Inequality& inequality)
    {
        return entailsInequality(impl_->inequalities,
                                 layout_.size(), inequality);
    });
}

std::string ConvexPolyhedraDomain::domainToString() const
{
    if (impl_->bottom)
        return "bottom";
    ensureConstraints();
    std::ostringstream output;
    output << "{";
    for (std::size_t index = 0; index < impl_->inequalities.size(); ++index)
    {
        if (index != 0)
            output << ", ";
        output << rowConstraint(layout_, impl_->inequalities[index])
               .toString();
    }
    output << "}";
    return output.str();
}

const ConvexPolyhedraDomain& ConvexPolyhedraDomain::requirePolyhedron(
    const AbstractDomain& other) const
{
    requireCompatible(other);
    return static_cast<const ConvexPolyhedraDomain&>(other);
}

void ConvexPolyhedraDomain::ensureConstraints() const
{
    if (impl_->bottom || impl_->constraintsValid)
        return;
    if (!impl_->generatorsValid || !impl_->generatorsExact)
    {
        assert(false && "polyhedron has no exact representation for H conversion");
        std::abort();
    }

    const bool convertedDual = !impl_->polarValid;
    GeneratorSystem minimizedPrimal;
    GeneratorSystem polar;
    std::vector<Inequality> inequalities;
    if (impl_->polarValid)
        inequalities = inequalitiesFromForms(impl_->polar.generators,
                                             impl_->generatorsNNC);
    else
        inequalities = constraintsFromGenerators(
                           impl_->generators, layout_.size(), impl_->generatorsNNC,
                           &minimizedPrimal, &polar);
    bool bottom = false;
    // In epsilon representation several strict forms can describe the same
    // open facet implication. Ordinary DD adjacency minimizes the underlying
    // homogeneous cone but does not perform NewPolka's epsilon-minimization.
    // Remove those semantic redundancies before publishing canonical H rows.
    if (impl_->generatorsNNC)
        inequalities =
            irredundant(std::move(inequalities), layout_.size());
    bool tightened = false;
    if (config_.integerTightening && hasIntegerVariable(layout_))
    {
        for (Inequality& inequality : inequalities)
        {
            const Inequality before = inequality;
            tightenIntegerRow(inequality, layout_);
            tightened = tightened || !sameInequality(before, inequality);
        }
        inequalities = normalized(std::move(inequalities), bottom);
    }
    const GeneratorSystem& outputPolar =
        impl_->polarValid ? impl_->polar : polar;
    const bool hasEquality = std::any_of(
                                 outputPolar.generators.begin(), outputPolar.generators.end(),
                                 [](const Generator& generator)
    {
        return generator.line;
    });
    if (!bottom && tightened && hasEquality)
        inequalities =
            canonicalizeAffineHull(std::move(inequalities), bottom);
    // A real-valued V cache contains a positive-homogeneous point by
    // construction, and the saturation DD conversion above already returns
    // its irredundant facets. Integer row tightening is the only step here
    // that can change that set or make it infeasible, so reserve the global
    // LP validation/minimization for that case.
    if (!bottom && tightened)
    {
        if (!feasible(inequalities, layout_.size()))
            bottom = true;
        else
            inequalities =
                irredundant(std::move(inequalities), layout_.size());
    }

    if (bottom)
    {
        // This should only be reachable for a malformed internal V cache.  Be
        // fail-closed instead of publishing an inconsistent pair of caches.
        auto* self = const_cast<ConvexPolyhedraDomain*>(this);
        self->impl_->bottom = true;
        self->impl_->inequalities.clear();
        self->impl_->generators.clear();
        self->impl_->generatorConstraints.clear();
        self->impl_->polar = {};
        self->impl_->constraintsValid = true;
        self->impl_->generatorsValid = false;
        self->impl_->generatorsMinimal = false;
        self->impl_->generatorsExact = false;
        self->impl_->generatorsNNC = false;
        self->impl_->polarValid = false;
        return;
    }
    impl_->inequalities = std::move(inequalities);
    impl_->constraintsValid = true;
    impl_->constraintsMinimal = true;
    if (tightened)
    {
        impl_->generators.clear();
        impl_->generatorConstraints.clear();
        impl_->generatorsValid = false;
        impl_->generatorsMinimal = false;
        impl_->generatorsExact = false;
        impl_->generatorsNNC = false;
        impl_->polar = {};
        impl_->polarValid = false;
    }
    else if (convertedDual)
    {
        impl_->generators = std::move(minimizedPrimal.generators);
        impl_->generatorConstraints =
            std::move(minimizedPrimal.constraints);
        impl_->generatorsValid = true;
        impl_->generatorsMinimal = true;
        impl_->generatorsExact = true;
        impl_->generatorsNNC = minimizedPrimal.nnc;
        impl_->polar = std::move(polar);
        impl_->polarValid = true;
    }
}

void ConvexPolyhedraDomain::ensureGenerators() const
{
    if (impl_->bottom || impl_->generatorsValid)
        return;
    ensureConstraints();
    GeneratorSystem system = generatorsFromConstraints(
                                 impl_->inequalities, layout_.size());
    GeneratorSystem polar = primalSystem(system);
    impl_->generators = std::move(system.generators);
    impl_->generatorConstraints = std::move(system.constraints);
    if (impl_->generators.empty())
    {
        auto* self = const_cast<ConvexPolyhedraDomain*>(this);
        self->impl_->bottom = true;
        self->impl_->inequalities.clear();
        self->impl_->generatorConstraints.clear();
        self->impl_->constraintsValid = true;
        self->impl_->generatorsValid = false;
        self->impl_->generatorsMinimal = false;
        self->impl_->generatorsExact = false;
        self->impl_->generatorsNNC = false;
        self->impl_->polar = {};
        self->impl_->polarValid = false;
        return;
    }
    impl_->generatorsValid = true;
    impl_->generatorsMinimal = true;
    impl_->generatorsExact = true;
    impl_->generatorsNNC = system.nnc;
    impl_->polar = std::move(polar);
    impl_->polarValid = true;
}

void ConvexPolyhedraDomain::invalidateConstraints()
{
    if (!impl_->generatorsValid || !impl_->generatorsExact)
    {
        assert(false && "cannot invalidate constraints without an exact V representation");
        std::abort();
    }
    impl_->inequalities.clear();
    impl_->constraintsValid = false;
    impl_->constraintsMinimal = false;
}

void ConvexPolyhedraDomain::invalidateGenerators()
{
    impl_->generators.clear();
    impl_->generatorConstraints.clear();
    impl_->polar = {};
    impl_->generatorsValid = false;
    impl_->generatorsMinimal = false;
    impl_->generatorsExact = false;
    impl_->generatorsNNC = false;
    impl_->polarValid = false;
}

void ConvexPolyhedraDomain::normalize()
{
    ensureConstraints();
    invalidateGenerators();
    if (impl_->bottom)
    {
        impl_->inequalities.clear();
        impl_->constraintsValid = true;
        impl_->constraintsMinimal = true;
        return;
    }
    impl_->inequalities =
        normalized(std::move(impl_->inequalities), impl_->bottom);
    if (!impl_->bottom && config_.integerTightening)
    {
        for (Inequality& inequality : impl_->inequalities)
            tightenIntegerRow(inequality, layout_);
        impl_->inequalities =
            normalized(std::move(impl_->inequalities), impl_->bottom);
    }
    if (!impl_->bottom)
        impl_->inequalities = canonicalizeAffineHull(
                                  std::move(impl_->inequalities), impl_->bottom);
    if (!impl_->bottom && !feasible(impl_->inequalities, layout_.size()))
    {
        impl_->bottom = true;
        impl_->inequalities.clear();
        impl_->constraintsValid = true;
        return;
    }
    if (!impl_->bottom)
        impl_->inequalities =
            irredundant(std::move(impl_->inequalities), layout_.size());
    impl_->constraintsValid = true;
    impl_->constraintsMinimal = true;
}

void ConvexPolyhedraDomain::report(OperationKind operation,
                                   ApproximationKind approximation,
                                   std::string reason, bool best) const
{
    recordOperation(operation, approximation, best, reason);
    if (config_.diagnostics)
        config_.diagnostics->report(
    {operation, approximation, std::move(reason)});
}

namespace
{
std::vector<Inequality> projectRows(
    std::vector<Inequality> inequalities,
    const std::vector<std::size_t>& dimensions)
{
    const std::size_t width =
        inequalities.empty() ? 0 : inequalities.front().coefficients.size();
    std::vector<std::size_t> order = dimensions;
    std::sort(order.begin(), order.end());
    order.erase(std::unique(order.begin(), order.end()), order.end());

    // The caller fixes which dimensions go, not the order they go in, and one
    // elimination step emits #positive * #negative rows. Taking the cheapest
    // dimension first (the Cha-Chan-Loo rule) keeps the intermediate systems
    // small, which matters far more than the step count.
    const auto stepCost = [&](std::size_t dimension)
    {
        std::size_t positive = 0;
        std::size_t negative = 0;
        for (const Inequality& inequality : inequalities)
        {
            const int sign = inequality.coefficients[dimension].sign();
            if (sign > 0)
                ++positive;
            else if (sign < 0)
                ++negative;
        }
        return positive * negative;
    };

    while (!order.empty())
    {
        auto cheapest = order.begin();
        std::size_t best = stepCost(*cheapest);
        for (auto candidate = std::next(order.begin()); candidate != order.end();
                ++candidate)
        {
            const std::size_t cost = stepCost(*candidate);
            if (cost < best)
            {
                best = cost;
                cheapest = candidate;
            }
        }
        const std::size_t dimension = *cheapest;
        order.erase(cheapest);

        inequalities = eliminate(std::move(inequalities), dimension);
        bool bottom = false;
        inequalities = normalized(std::move(inequalities), bottom);
        if (bottom)
        {
            Inequality contradiction;
            contradiction.coefficients.resize(width);
            contradiction.bound = Rational(-1);
            return {std::move(contradiction)};
        }
        // Reduce between elimination steps, not only at the end: each step
        // costs one row per (upper, lower) pair of the previous step, so
        // carrying implied rows forward is what makes the elimination
        // superexponential rather than merely exponential. Reduction itself
        // costs one linear program per row, so it only pays once a step can
        // actually produce a large product; below that the next elimination is
        // cheaper than the test would be.
        constexpr std::size_t reductionThreshold = 16;
        if (inequalities.size() > reductionThreshold)
            inequalities = irredundant(std::move(inequalities), width);
    }
    return inequalities;
}

// ---------------------------------------------------------------------------
// Exact rational simplex.
//
// Feasibility used to be decided by running Fourier-Motzkin to completion,
// which costs one row per (upper, lower) pair at every step. That is fine as a
// definition and unusable as a subroutine: the redundancy test below calls it
// once per candidate row, so an exponential feasibility check makes redundancy
// removal cost more than the redundancy it removes. A two-phase primal simplex
// over GMP rationals decides the same question exactly, with Bland's rule for
// guaranteed termination.
// ---------------------------------------------------------------------------

/// Tableau: `rows` equality rows plus one objective row, `columns` variable
/// columns plus one right-hand-side column. `basis[i]` is the column basic in
/// row i. Maximizes the objective; returns false when it is unbounded.
class Tableau
{
public:
    Tableau(std::size_t rows, std::size_t columns)
        : columns_(columns), cells_(rows + 1,
                                    std::vector<mpq_class>(columns + 1)),
          basis_(rows)
    {
    }

    std::size_t rows() const
    {
        return basis_.size();
    }
    std::size_t columns() const
    {
        return columns_;
    }
    mpq_class& at(std::size_t row, std::size_t column)
    {
        return cells_[row][column];
    }
    const mpq_class& at(std::size_t row, std::size_t column) const
    {
        return cells_[row][column];
    }
    mpq_class& rhs(std::size_t row)
    {
        return cells_[row][columns_];
    }
    const mpq_class& rhs(std::size_t row) const
    {
        return cells_[row][columns_];
    }
    std::size_t& basis(std::size_t row)
    {
        return basis_[row];
    }
    std::size_t basis(std::size_t row) const
    {
        return basis_[row];
    }
    /// The objective row lives one past the last constraint row.
    std::size_t objective() const
    {
        return basis_.size();
    }

    void pivot(std::size_t row, std::size_t column)
    {
        const mpq_class inverse = 1 / cells_[row][column];
        for (mpq_class& cell : cells_[row])
            cell *= inverse;
        for (std::size_t other = 0; other < cells_.size(); ++other)
        {
            if (other == row || cells_[other][column] == 0)
                continue;
            const mpq_class factor = cells_[other][column];
            for (std::size_t index = 0; index <= columns_; ++index)
                cells_[other][index] -= factor * cells_[row][index];
        }
        basis_[row] = column;
    }

    /// Primal simplex with Bland's rule: always take the lowest eligible
    /// entering column and break ratio ties on the lowest basic column index,
    /// which makes cycling impossible.
    bool maximize()
    {
        const std::size_t last = objective();
        while (true)
        {
            std::size_t entering = columns_;
            for (std::size_t column = 0; column < columns_; ++column)
            {
                if (cells_[last][column] > 0)
                {
                    entering = column;
                    break;
                }
            }
            if (entering == columns_)
                return true;

            std::size_t leaving = basis_.size();
            mpq_class best;
            for (std::size_t row = 0; row < basis_.size(); ++row)
            {
                if (cells_[row][entering] <= 0)
                    continue;
                const mpq_class ratio =
                    cells_[row][columns_] / cells_[row][entering];
                if (leaving == basis_.size() || ratio < best ||
                        (ratio == best && basis_[row] < basis_[leaving]))
                {
                    best = ratio;
                    leaving = row;
                }
            }
            if (leaving == basis_.size())
                return false;
            pivot(leaving, entering);
        }
    }

    /// Maximized objective value.
    mpq_class value() const
    {
        return -cells_[basis_.size()][columns_];
    }

    /// Discard the trailing columns and every row still basic on one of them.
    ///
    /// Phase I's artificial variables must not merely be kept out of the
    /// basis for phase II: an artificial that is still basic sits at zero but
    /// grows again as soon as an entering column has a negative entry in its
    /// row, because the ratio test only bounds rows with positive entries. A
    /// phase II run over such a tableau optimizes a relaxation of the original
    /// system. Deleting the columns removes the variables outright. A row
    /// whose artificial cannot be pivoted out has no support outside them, so
    /// it is the redundant equality 0 = 0 and goes with them.
    void dropTrailing(std::size_t keep)
    {
        std::vector<std::vector<mpq_class>> cells;
        std::vector<std::size_t> basis;
        const auto compact = [&](const std::vector<mpq_class>& row)
        {
            std::vector<mpq_class> result(
                row.begin(),
                row.begin() + static_cast<std::ptrdiff_t>(keep));
            result.push_back(row[columns_]);
            return result;
        };
        for (std::size_t row = 0; row < basis_.size(); ++row)
        {
            if (basis_[row] >= keep)
                continue;
            cells.push_back(compact(cells_[row]));
            basis.push_back(basis_[row]);
        }
        cells.push_back(compact(cells_.back()));
        cells_ = std::move(cells);
        basis_ = std::move(basis);
        columns_ = keep;
    }

private:
    std::size_t columns_;
    std::vector<std::vector<mpq_class>> cells_;
    std::vector<std::size_t> basis_;
};

bool feasible(std::vector<Inequality> inequalities, std::size_t dimensions)
{
    // Drop rows that carry no information, and fail fast on `0 <= negative`.
    std::vector<Inequality> rows;
    rows.reserve(inequalities.size());
    for (Inequality& inequality : inequalities)
    {
        if (falseConstant(inequality))
            return false;
        if (trueConstant(inequality))
            continue;
        rows.push_back(std::move(inequality));
    }
    if (rows.empty())
        return true;

    const bool anyStrict =
        std::any_of(rows.begin(), rows.end(),
                    [](const Inequality& row)
    {
        return row.strict;
    });

    // Projection zeroes a dimension's column but keeps its width, and the join
    // lift triples the width before eliminating most of it. Carrying those
    // dead columns into the tableau costs two simplex variables each for no
    // information, so map only the live ones.
    std::vector<std::size_t> live;
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension)
    {
        const bool used = std::any_of(
                              rows.begin(), rows.end(), [&](const Inequality& row)
        {
            return !row.coefficients[dimension].isZero();
        });
        if (used)
            live.push_back(dimension);
    }
    dimensions = live.size();

    // Each free x_j becomes u_j - v_j with u, v >= 0. A strict row a.x < b
    // becomes a.x + epsilon <= b, and the system is strictly feasible exactly
    // when max epsilon > 0 under the extra row epsilon <= 1.
    const std::size_t constraintCount = rows.size() + (anyStrict ? 1 : 0);
    const std::size_t positivePart = 0;
    const std::size_t negativePart = dimensions;
    const std::size_t epsilonColumn = 2 * dimensions;
    const std::size_t slackBase = epsilonColumn + (anyStrict ? 1 : 0);
    const std::size_t artificialBase = slackBase + constraintCount;
    const std::size_t columns = artificialBase + constraintCount;

    Tableau tableau(constraintCount, columns);
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        for (std::size_t dimension = 0; dimension < dimensions; ++dimension)
        {
            const mpq_class& coefficient =
                rows[row].coefficients[live[dimension]].value();
            tableau.at(row, positivePart + dimension) = coefficient;
            tableau.at(row, negativePart + dimension) = -coefficient;
        }
        if (anyStrict && rows[row].strict)
            tableau.at(row, epsilonColumn) = 1;
        tableau.at(row, slackBase + row) = 1;
        tableau.rhs(row) = rows[row].bound.value();
    }
    if (anyStrict)
    {
        const std::size_t row = rows.size();
        tableau.at(row, epsilonColumn) = 1;
        tableau.at(row, slackBase + row) = 1;
        tableau.rhs(row) = 1;
    }

    // Phase I needs a non-negative right-hand side and a starting basis.
    for (std::size_t row = 0; row < constraintCount; ++row)
    {
        if (tableau.rhs(row) < 0)
        {
            for (std::size_t column = 0; column <= columns; ++column)
                tableau.at(row, column) = -tableau.at(row, column);
        }
        tableau.at(row, artificialBase + row) = 1;
        tableau.basis(row) = artificialBase + row;
    }

    // Maximize -(sum of artificials); the system is feasible iff that is 0.
    const std::size_t objective = tableau.objective();
    for (std::size_t row = 0; row < constraintCount; ++row)
    {
        for (std::size_t column = 0; column <= columns; ++column)
            tableau.at(objective, column) += tableau.at(row, column);
    }
    for (std::size_t row = 0; row < constraintCount; ++row)
        tableau.at(objective, artificialBase + row) = 0;

    tableau.maximize();
    if (tableau.value() < 0)
        return false;
    if (!anyStrict)
        return true;

    // Phase II. First pivot every artificial out of the basis where the row
    // has any other support, then delete the artificial columns entirely.
    for (std::size_t row = 0; row < constraintCount; ++row)
    {
        if (tableau.basis(row) < artificialBase)
            continue;
        for (std::size_t column = 0; column < artificialBase; ++column)
        {
            if (tableau.at(row, column) != 0)
            {
                tableau.pivot(row, column);
                break;
            }
        }
    }
    tableau.dropTrailing(artificialBase);

    // The system is strictly feasible exactly when epsilon can be made
    // positive, so maximize it over what phase I left behind.
    const std::size_t phaseTwo = tableau.objective();
    for (std::size_t column = 0; column <= tableau.columns(); ++column)
        tableau.at(phaseTwo, column) = 0;
    tableau.at(phaseTwo, epsilonColumn) = 1;
    for (std::size_t row = 0; row < tableau.rows(); ++row)
    {
        const mpq_class factor = tableau.at(phaseTwo, tableau.basis(row));
        if (factor == 0)
            continue;
        for (std::size_t column = 0; column <= tableau.columns(); ++column)
            tableau.at(phaseTwo, column) -= factor * tableau.at(row, column);
    }

    tableau.maximize();
    return tableau.value() > 0;
}

bool entailsInequality(const std::vector<Inequality>& premises,
                       std::size_t dimensions,
                       const Inequality& conclusion)
{
    std::vector<Inequality> counterexample = premises;
    counterexample.push_back(negateForCounterexample(conclusion));
    return !feasible(std::move(counterexample), dimensions);
}

/// Drop every row implied by the others.
///
/// Fourier-Motzkin elimination produces one row per (upper, lower) pair, so
/// the vast majority of what it emits is implied by the rest of the system.
/// `normalized` only merges rows whose scaled coefficient vectors are
/// identical, which leaves that redundancy in place: the join of two rational
/// points in the plane keeps 42 rows for a segment that needs three. The rows
/// are all correct, but the next operation's cost is driven by how many of
/// them survive, so the count compounds multiplicatively across joins and the
/// domain stops terminating at three dimensions.
///
/// Each candidate is tested against every row that is still in the system --
/// those already kept plus those not yet examined -- so dropping one row never
/// invalidates a later test.
std::vector<Inequality> irredundant(std::vector<Inequality> inequalities,
                                    std::size_t dimensions)
{
    if (inequalities.size() < 2)
        return inequalities;

    std::vector<Inequality> kept;
    kept.reserve(inequalities.size());
    for (std::size_t index = 0; index < inequalities.size(); ++index)
    {
        std::vector<Inequality> rest = kept;
        rest.insert(rest.end(),
                    inequalities.begin() +
                    static_cast<std::ptrdiff_t>(index + 1),
                    inequalities.end());
        if (!entailsInequality(rest, dimensions, inequalities[index]))
            kept.push_back(std::move(inequalities[index]));
    }
    return kept;
}

mpz_class dot(const std::vector<mpz_class>& lhs,
              const std::vector<mpz_class>& rhs)
{
    mpz_class result = 0;
    for (std::size_t index = 0; index < lhs.size(); ++index)
        mpz_addmul(result.get_mpz_t(), lhs[index].get_mpz_t(),
                   rhs[index].get_mpz_t());
    return result;
}

void assignDifferenceOfProducts(mpz_class& result,
                                const mpz_class& lhsFactor,
                                const mpz_class& lhs,
                                const mpz_class& rhsFactor,
                                const mpz_class& rhs)
{
    mpz_mul(result.get_mpz_t(), lhsFactor.get_mpz_t(), lhs.get_mpz_t());
    mpz_submul(result.get_mpz_t(), rhsFactor.get_mpz_t(), rhs.get_mpz_t());
}

bool generatorsEntail(const std::vector<Generator>& generators,
                      const Inequality& inequality, bool nnc)
{
    const std::size_t variableOffset = generatorVariableOffset(nnc);
    for (const Generator& generator : generators)
    {
        Rational value = Rational::fromRaw(
                             -inequality.bound.value() * generator.coordinates.front());
        for (std::size_t dimension = 0;
                dimension < inequality.coefficients.size(); ++dimension)
        {
            value += Rational::fromRaw(
                         inequality.coefficients[dimension].value() *
                         generator.coordinates[dimension + variableOffset]);
        }
        if (generator.line)
        {
            if (!value.isZero())
                return false;
        }
        else if (generator.coordinates.front() == 0)
        {
            if (value.sign() > 0)
                return false;
        }
        else if (value.sign() > 0 ||
                 (value.isZero() && inequality.strict &&
                  (!nnc || generator.coordinates[1] > 0)))
            return false;
    }
    return true;
}

Interval boundFromGenerators(const std::vector<Generator>& generators,
                             const std::vector<Rational>& objective,
                             const Rational& constant, bool nnc)
{
    const std::size_t variableOffset = generatorVariableOffset(nnc);
    std::optional<Rational> minimum;
    std::optional<Rational> maximum;
    bool minimumIncluded = false;
    bool maximumIncluded = false;
    bool lowerUnbounded = false;
    bool upperUnbounded = false;
    for (const Generator& generator : generators)
    {
        Rational value = Rational::fromRaw(
                             constant.value() * generator.coordinates.front());
        for (std::size_t dimension = 0; dimension < objective.size();
                ++dimension)
        {
            value += Rational::fromRaw(
                         objective[dimension].value() *
                         generator.coordinates[dimension + variableOffset]);
        }
        if (generator.line)
        {
            lowerUnbounded = lowerUnbounded || !value.isZero();
            upperUnbounded = upperUnbounded || !value.isZero();
            continue;
        }
        if (generator.coordinates.front() == 0)
        {
            lowerUnbounded = lowerUnbounded || value.sign() < 0;
            upperUnbounded = upperUnbounded || value.sign() > 0;
            continue;
        }
        value /= Rational::fromRaw(mpq_class(generator.coordinates.front()));
        const bool included = !nnc || generator.coordinates[1] > 0;
        if (!minimum || value < *minimum)
        {
            minimum = value;
            minimumIncluded = included;
        }
        else if (value == *minimum)
            minimumIncluded = minimumIncluded || included;
        if (!maximum || *maximum < value)
        {
            maximum = value;
            maximumIncluded = included;
        }
        else if (value == *maximum)
            maximumIncluded = maximumIncluded || included;
    }
    if (!minimum || !maximum)
        return Interval(Bound::plusInfinity(), Bound::minusInfinity());
    return Interval(
               lowerUnbounded ? Bound::minusInfinity()
               : Bound::finite(*minimum, !minimumIncluded),
               upperUnbounded ? Bound::plusInfinity()
               : Bound::finite(*maximum, !maximumIncluded));
}

bool normalizeGenerator(Generator& generator)
{
    if (std::all_of(generator.coordinates.begin(),
                    generator.coordinates.end(),
                    [](const mpz_class& coordinate)
{
    return coordinate == 0;
}))
    return false;

    mpz_class divisor = 0;
    for (const mpz_class& coordinate : generator.coordinates)
        mpz_gcd(divisor.get_mpz_t(), divisor.get_mpz_t(),
                coordinate.get_mpz_t());
    if (divisor < 0)
        divisor = -divisor;
    if (divisor != 1)
        for (mpz_class& coordinate : generator.coordinates)
            coordinate /= divisor;
    if (generator.line)
    {
        const auto first = std::find_if(
                               generator.coordinates.begin(), generator.coordinates.end(),
                               [](const mpz_class& coordinate)
        {
            return coordinate != 0;
        });
        if (first != generator.coordinates.end() && *first < 0)
            for (mpz_class& coordinate : generator.coordinates)
                coordinate = -coordinate;
    }
    return true;
}

void setGeneratorCoordinates(Generator& generator,
                             const std::vector<mpq_class>& coordinates)
{
    mpz_class scale = 1;
    for (const mpq_class& coordinate : coordinates)
        mpz_lcm(scale.get_mpz_t(), scale.get_mpz_t(),
                coordinate.get_den().get_mpz_t());
    generator.coordinates.resize(coordinates.size());
    for (std::size_t index = 0; index < coordinates.size(); ++index)
    {
        const mpq_class scaled = coordinates[index] * scale;
        generator.coordinates[index] = scaled.get_num();
    }
    normalizeGenerator(generator);
}

struct GeneratorLess
{
    bool operator()(const Generator& lhs, const Generator& rhs) const
    {
        if (lhs.line != rhs.line)
            return lhs.line && !rhs.line;
        return std::lexicographical_compare(
                   lhs.coordinates.begin(), lhs.coordinates.end(),
                   rhs.coordinates.begin(), rhs.coordinates.end());
    }
};

std::vector<Generator> uniqueGenerators(std::vector<Generator> generators)
{
    std::size_t kept = 0;
    for (std::size_t index = 0; index < generators.size(); ++index)
    {
        if (!normalizeGenerator(generators[index]))
            continue;
        if (kept != index)
            generators[kept] = std::move(generators[index]);
        ++kept;
    }
    generators.resize(kept);
    return sortUniqueGenerators(std::move(generators));
}

std::vector<Generator> sortUniqueGenerators(
    std::vector<Generator> generators)
{
    const GeneratorLess less;
    std::sort(generators.begin(), generators.end(), less);
    generators.erase(
        std::unique(generators.begin(), generators.end(),
                    [&](const Generator& lhs, const Generator& rhs)
    {
        return !less(lhs, rhs) && !less(rhs, lhs);
    }),
    generators.end());
    return generators;
}

std::vector<Generator> mergeGeneratorSets(
    const std::vector<Generator>& lhs, const std::vector<Generator>& rhs)
{
    std::vector<Generator> result;
    result.reserve(lhs.size() + rhs.size());
    const GeneratorLess less;
    const auto appendIdentity = [&](const Generator& generator)
    {
        Generator copy;
        copy.coordinates = generator.coordinates;
        copy.line = generator.line;
        result.push_back(std::move(copy));
    };
    std::size_t left = 0;
    std::size_t right = 0;
    while (left < lhs.size() && right < rhs.size())
    {
        if (less(lhs[left], rhs[right]))
            appendIdentity(lhs[left++]);
        else if (less(rhs[right], lhs[left]))
            appendIdentity(rhs[right++]);
        else
        {
            appendIdentity(lhs[left]);
            ++left;
            ++right;
        }
    }
    while (left < lhs.size())
        appendIdentity(lhs[left++]);
    while (right < rhs.size())
        appendIdentity(rhs[right++]);
    return result;
}

constexpr std::size_t saturationWordBits = 64;

std::size_t saturationWords(std::size_t constraints)
{
    return (constraints + saturationWordBits - 1) / saturationWordBits;
}

void clearSaturation(Generator& generator, std::size_t constraints)
{
    generator.saturation.assign(saturationWords(constraints), 0);
}

void setSaturation(Generator& generator, std::size_t constraint)
{
    const std::size_t word = constraint / saturationWordBits;
    if (generator.saturation.size() <= word)
        generator.saturation.resize(word + 1);
    generator.saturation[word] |=
        std::uint64_t(1) << (constraint % saturationWordBits);
}

void setAllSaturation(Generator& generator, std::size_t constraints)
{
    generator.saturation.assign(saturationWords(constraints),
                                ~std::uint64_t(0));
    const std::size_t remainder = constraints % saturationWordBits;
    if (remainder != 0)
    {
        generator.saturation.back() =
                                (std::uint64_t(1) << remainder) - 1;
    }
}

bool saturated(const Generator& generator, std::size_t constraint)
{
    const std::size_t word = constraint / saturationWordBits;
    return word < generator.saturation.size() &&
           (generator.saturation[word] &
            (std::uint64_t(1) << (constraint % saturationWordBits))) != 0;
}

std::vector<std::uint64_t> commonSaturation(const Generator& lhs,
        const Generator& rhs)
{
    const std::size_t words =
        std::min(lhs.saturation.size(), rhs.saturation.size());
    std::vector<std::uint64_t> result(words);
    for (std::size_t word = 0; word < words; ++word)
        result[word] = lhs.saturation[word] & rhs.saturation[word];
    return result;
}

void rebuildSaturation(
    std::vector<Generator>& generators,
    const std::vector<std::vector<mpz_class>>& processedConstraints)
{
    for (Generator& generator : generators)
    {
        clearSaturation(generator, processedConstraints.size());
        for (std::size_t constraint = 0;
                constraint < processedConstraints.size(); ++constraint)
        {
            if (dot(processedConstraints[constraint], generator.coordinates) ==
                    0)
                setSaturation(generator, constraint);
        }
    }
}

bool bitSubset(const std::vector<std::uint64_t>& subset,
               const std::vector<std::uint64_t>& superset)
{
    for (std::size_t word = 0; word < subset.size(); ++word)
    {
        const std::uint64_t other =
            word < superset.size() ? superset[word] : 0;
        if ((subset[word] & ~other) != 0)
            return false;
    }
    return true;
}

std::vector<Generator> simplifiedInputGenerators(
    const GeneratorSystem& dual)
{
    const std::size_t constraintCount = dual.constraints.size();
    const std::size_t generatorCount = dual.generators.size();
    const std::size_t words = saturationWords(generatorCount);
    std::vector<std::vector<std::uint64_t>> active(
        constraintCount, std::vector<std::uint64_t>(words));
    std::vector<bool> equality(constraintCount, true);
    for (std::size_t constraint = 0; constraint < constraintCount;
            ++constraint)
    {
        for (std::size_t generator = 0; generator < generatorCount;
                ++generator)
        {
            if (saturated(dual.generators[generator], constraint))
            {
                active[constraint][generator / saturationWordBits] |=
                    std::uint64_t(1) <<
                    (generator % saturationWordBits);
            }
            else
                equality[constraint] = false;
        }
    }

    std::vector<bool> keep(constraintCount, true);
    for (std::size_t candidate = 0; candidate < constraintCount; ++candidate)
    {
        if (equality[candidate])
            continue;
        for (std::size_t other = 0; other < constraintCount; ++other)
        {
            if (candidate == other || equality[other])
                continue;
            if (!bitSubset(active[candidate], active[other]))
                continue;
            if (active[candidate] != active[other] || other < candidate)
            {
                keep[candidate] = false;
                break;
            }
        }
    }

    // Equality constraints describe the lineality generators of the dual
    // cone. Keep a reduced independent basis rather than opposite pairs.
    std::vector<std::vector<mpz_class>> equations;
    for (std::size_t constraint = 0; constraint < constraintCount;
            ++constraint)
        if (equality[constraint])
            equations.push_back(dual.constraints[constraint]);
    std::size_t rank = 0;
    const std::size_t dimensions = equations.empty()
                                   ? 0
                                   : equations.front().size();
    for (std::size_t column = 0;
            column < dimensions && rank < equations.size(); ++column)
    {
        auto pivot = std::find_if(
                         equations.begin() + static_cast<std::ptrdiff_t>(rank),
                         equations.end(),
                         [&](const std::vector<mpz_class>& row)
        {
            return row[column] != 0;
        });
        if (pivot == equations.end())
            continue;
        std::iter_swap(
            equations.begin() + static_cast<std::ptrdiff_t>(rank), pivot);
        const mpz_class pivotValue = equations[rank][column];
        for (std::size_t other = 0; other < equations.size(); ++other)
        {
            if (other == rank || equations[other][column] == 0)
                continue;
            const mpz_class factor = equations[other][column];
            for (std::size_t coordinate = 0; coordinate < dimensions;
                    ++coordinate)
                assignDifferenceOfProducts(
                    equations[other][coordinate], pivotValue,
                    equations[other][coordinate], factor,
                    equations[rank][coordinate]);
            Generator normalizedLine;
            normalizedLine.coordinates = equations[other];
            normalizedLine.line = true;
            normalizeGenerator(normalizedLine);
            equations[other] = std::move(normalizedLine.coordinates);
        }
        Generator normalizedPivot;
        normalizedPivot.coordinates = equations[rank];
        normalizedPivot.line = true;
        normalizeGenerator(normalizedPivot);
        equations[rank] = std::move(normalizedPivot.coordinates);
        ++rank;
    }
    equations.resize(rank);

    std::vector<Generator> result;
    result.reserve(rank + constraintCount);
    std::vector<std::size_t> outputConstraint(generatorCount + 1);
    for (std::size_t generator = 0; generator < generatorCount; ++generator)
    {
        outputConstraint[generator + 1] =
            outputConstraint[generator] +
            (dual.generators[generator].line ? 2 : 1);
    }
    const std::size_t outputConstraintCount = outputConstraint.back();
    for (std::vector<mpz_class>& equation : equations)
    {
        Generator line;
        line.coordinates = std::move(equation);
        line.line = true;
        setAllSaturation(line, outputConstraintCount);
        result.push_back(std::move(line));
    }
    for (std::size_t constraint = 0; constraint < constraintCount;
            ++constraint)
    {
        if (!equality[constraint] && keep[constraint])
        {
            Generator ray;
            ray.coordinates = dual.constraints[constraint];
            clearSaturation(ray, outputConstraintCount);
            for (std::size_t generator = 0; generator < generatorCount;
                    ++generator)
            {
                if (!saturated(dual.generators[generator], constraint))
                    continue;
                setSaturation(ray, outputConstraint[generator]);
                if (dual.generators[generator].line)
                    setSaturation(ray, outputConstraint[generator] + 1);
            }
            result.push_back(std::move(ray));
        }
    }
    return uniqueGenerators(std::move(result));
}

bool adjacentGenerators(const std::vector<Generator>& generators,
                        std::size_t lhs, std::size_t rhs,
                        std::size_t ambientDimensions,
                        std::size_t lineDimensions)
{
    const std::size_t words = std::min(
                                  generators[lhs].saturation.size(),
                                  generators[rhs].saturation.size());
    std::size_t commonCount = 0;
    for (std::size_t word = 0; word < words; ++word)
    {
        std::uint64_t common = generators[lhs].saturation[word] &
                               generators[rhs].saturation[word];
        commonCount += std::bitset<64>(common).count();
    }
    const std::size_t required =
        ambientDimensions > lineDimensions + 2
        ? ambientDimensions - lineDimensions - 2
        : 0;
    // The rank of common active constraints cannot exceed their count.  Fewer
    // than D-2 therefore cannot define a two-dimensional face.
    if (commonCount < required)
        return false;

    for (std::size_t other = 0; other < generators.size(); ++other)
    {
        if (other == lhs || other == rhs)
            continue;
        if (generators[other].line)
            continue;
        bool subset = true;
        for (std::size_t word = 0; word < words; ++word)
        {
            const std::uint64_t common =
                generators[lhs].saturation[word] &
                generators[rhs].saturation[word];
            const std::uint64_t candidate =
                word < generators[other].saturation.size()
                ? generators[other].saturation[word]
                : 0;
            if ((common & ~candidate) != 0)
            {
                subset = false;
                break;
            }
        }
        if (subset)
            return false;
    }
    return true;
}

struct ConstraintSplit
{
    std::size_t inside = 0;
    std::size_t outside = 0;
    std::size_t boundary = 0;

    std::size_t estimatedSize() const
    {
        if (outside == 0)
            return inside + boundary;
        if (inside == 0)
            return boundary;
        if (inside > std::numeric_limits<std::size_t>::max() / outside)
            return std::numeric_limits<std::size_t>::max();
        const std::size_t pairs = inside * outside;
        if (pairs > std::numeric_limits<std::size_t>::max() -
                inside - boundary)
            return std::numeric_limits<std::size_t>::max();
        return inside + boundary + pairs;
    }
};

std::pair<std::vector<mpz_class>, ConstraintSplit> evaluateConstraint(
    const std::vector<Generator>& generators,
    const std::vector<mpz_class>& constraint)
{
    std::vector<mpz_class> values;
    values.reserve(generators.size());
    ConstraintSplit split;
    for (const Generator& generator : generators)
    {
        values.push_back(dot(constraint, generator.coordinates));
        const int sign = mpz_sgn(values.back().get_mpz_t());
        if (sign < 0)
            ++split.inside;
        else if (sign > 0)
            ++split.outside;
        else
            ++split.boundary;
    }
    return {std::move(values), split};
}

/// Incremental Chernikova conversion for a minimized homogeneous cone.
/// Lineality is explicit, so every non-line step may use the saturation
/// adjacency invariant; no conic LP redundancy pass is required.
GeneratorSystem intersectCone(
    std::vector<Generator> generators,
    std::vector<std::vector<mpz_class>> processedConstraints,
    std::vector<std::vector<mpz_class>> constraints, bool inputCoherent)
{
    if (!inputCoherent)
    {
        generators = uniqueGenerators(std::move(generators));
        rebuildSaturation(generators, processedConstraints);
    }
    std::sort(constraints.begin(), constraints.end(),
              [](const std::vector<mpz_class>& lhs,
                 const std::vector<mpz_class>& rhs)
    {
        return std::lexicographical_compare(
                   lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    });

    while (!constraints.empty() && !generators.empty())
    {
        // NewPolka canonicalizes and sorts the incoming matrix once, then
        // processes it linearly. Re-scoring every remaining row repeats all
        // scalar products quadratically and costs more than it saves for the
        // already canonical H/V matrices used by domain operations.
        std::vector<mpz_class> constraint = std::move(constraints.back());
        constraints.pop_back();
        auto [values, selectedSplit] =
            evaluateConstraint(generators, constraint);

        // A row cutting a lineality direction can be handled by a single
        // Chernikova line pivot. Move every other generator along that free
        // direction onto the new boundary, and retain only the feasible
        // orientation of the pivot as a ray. This removes one line dimension
        // without materializing all crossing pairs or solving redundancy LPs.
        std::optional<std::size_t> pivotIndex;
        for (std::size_t index = 0; index < generators.size(); ++index)
        {
            if (generators[index].line && values[index] != 0)
            {
                pivotIndex = index;
                break;
            }
        }
        if (pivotIndex)
        {
            Generator pivot = std::move(generators[*pivotIndex]);
            mpz_class pivotValue = std::move(values[*pivotIndex]);
            if (pivotValue < 0)
            {
                for (mpz_class& coordinate : pivot.coordinates)
                    coordinate = -coordinate;
                pivotValue = -pivotValue;
            }
            std::vector<Generator> next;
            next.reserve(generators.size());
            for (std::size_t index = 0; index < generators.size(); ++index)
            {
                if (index == *pivotIndex)
                    continue;
                const mpz_class& value = values[index];
                Generator boundary = std::move(generators[index]);
                if (value != 0)
                {
                    for (std::size_t coordinate = 0;
                            coordinate < boundary.coordinates.size(); ++coordinate)
                        assignDifferenceOfProducts(
                            boundary.coordinates[coordinate], pivotValue,
                            boundary.coordinates[coordinate], value,
                            pivot.coordinates[coordinate]);
                    normalizeGenerator(boundary);
                }
                setSaturation(boundary, processedConstraints.size());
                next.push_back(std::move(boundary));
            }
            pivot.line = false;
            for (mpz_class& coordinate : pivot.coordinates)
                coordinate = -coordinate;
            next.push_back(std::move(pivot));
            processedConstraints.push_back(std::move(constraint));
            generators = sortUniqueGenerators(std::move(next));
            continue;
        }

        // Every ray satisfies the row and every line saturates it, so the row
        // is redundant and need not consume a saturation column.
        if (selectedSplit.outside == 0)
            continue;

        const std::size_t newConstraint = processedConstraints.size();
        std::vector<std::pair<std::size_t, mpz_class>> inside;
        std::vector<std::pair<std::size_t, mpz_class>> outside;
        std::vector<std::size_t> boundary;
        for (std::size_t index = 0; index < generators.size(); ++index)
        {
            mpz_class& value = values[index];
            if (generators[index].line || value == 0)
                boundary.push_back(index);
            else if (value < 0)
                inside.emplace_back(index, std::move(value));
            else if (value > 0)
                outside.emplace_back(index, std::move(value));
        }

        std::vector<Generator> next;
        const std::size_t estimated = selectedSplit.estimatedSize();
        if (estimated != std::numeric_limits<std::size_t>::max())
            next.reserve(estimated);
        const std::size_t lineDimensions =
            static_cast<std::size_t>(std::count_if(
                                         generators.begin(), generators.end(),
                                         [](const Generator& generator)
        {
            return generator.line;
        }));
        for (const auto& [inIndex, inValue] : inside)
        {
            for (const auto& [outIndex, outValue] : outside)
            {
                if (!adjacentGenerators(generators, inIndex, outIndex,
                                        generators[inIndex].coordinates.size(),
                                        lineDimensions))
                    continue;
                Generator intersection;
                intersection.coordinates.resize(
                    generators[inIndex].coordinates.size());
                for (std::size_t coordinate = 0;
                        coordinate < intersection.coordinates.size(); ++coordinate)
                    assignDifferenceOfProducts(
                        intersection.coordinates[coordinate], outValue,
                        generators[inIndex].coordinates[coordinate], inValue,
                        generators[outIndex].coordinates[coordinate]);
                intersection.saturation = commonSaturation(
                                              generators[inIndex], generators[outIndex]);
                setSaturation(intersection, newConstraint);
                if (normalizeGenerator(intersection))
                    next.push_back(std::move(intersection));
            }
        }
        // Crossing pairs above need both source generators. Move survivors
        // only after all intersections have been formed, avoiding a deep GMP
        // coordinate/saturation copy on every processed row.
        for (std::size_t index : boundary)
        {
            Generator generator = std::move(generators[index]);
            setSaturation(generator, newConstraint);
            next.push_back(std::move(generator));
        }
        for (const auto& [index, value] : inside)
        {
            (void)value;
            next.push_back(std::move(generators[index]));
        }
        processedConstraints.push_back(std::move(constraint));
        generators = sortUniqueGenerators(std::move(next));
    }
    return {std::move(generators), std::move(processedConstraints)};
}

std::vector<Generator> fullSpaceCone(std::size_t dimensions)
{
    std::vector<Generator> result;
    result.reserve(dimensions);
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension)
    {
        Generator line;
        line.coordinates.resize(dimensions);
        line.coordinates[dimension] = 1;
        line.line = true;
        result.push_back(std::move(line));
    }
    return result;
}

std::vector<mpz_class> homogeneousConstraint(const Inequality& inequality,
        std::size_t dimensions, bool nnc)
{
    const std::size_t variableOffset = generatorVariableOffset(nnc);
    std::vector<mpq_class> rational(dimensions + variableOffset);
    rational[0] = -inequality.bound.value();
    if (nnc && inequality.strict)
        rational[1] = 1;
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension)
        rational[dimension + variableOffset] =
            inequality.coefficients[dimension].value();
    Generator row;
    setGeneratorCoordinates(row, rational);
    return row.coordinates;
}

std::vector<std::vector<mpz_class>> homogeneousConstraints(
    const std::vector<Inequality>& inequalities, std::size_t dimensions,
    bool nnc)
{
    std::vector<std::vector<mpz_class>> result;
    result.reserve(inequalities.size());
    for (const Inequality& inequality : inequalities)
        result.push_back(homogeneousConstraint(inequality, dimensions, nnc));
    return result;
}

std::vector<std::vector<mpz_class>> generatorHalfspaces(
    const std::vector<Generator>& generators)
{
    std::vector<std::vector<mpz_class>> result;
    result.reserve(2 * generators.size());
    for (const Generator& generator : generators)
    {
        result.push_back(generator.coordinates);
        if (generator.line)
        {
            std::vector<mpz_class> opposite = generator.coordinates;
            for (mpz_class& coordinate : opposite)
                coordinate = -coordinate;
            result.push_back(std::move(opposite));
        }
    }
    return result;
}

std::vector<Generator> canonicalizePolarForms(
    const std::vector<Generator>& forms, bool nnc)
{
    std::vector<Generator> equations;
    for (const Generator& form : forms)
    {
        if (form.line)
            equations.push_back(form);
    }
    // Facets of a lower-dimensional polyhedron are defined only modulo its
    // affine hull. Compute a primitive fraction-free RREF of the line forms
    // and reduce each ray while the data is still homogeneous. Positive pivot
    // scaling preserves inequality orientation and avoids all mpq allocation.
    std::vector<std::size_t> pivots;
    std::size_t rank = 0;
    const std::size_t dimensions = forms.front().coordinates.size();
    for (std::size_t column = generatorVariableOffset(nnc);
            column < dimensions && rank < equations.size(); ++column)
    {
        auto pivot = std::find_if(
                         equations.begin() + static_cast<std::ptrdiff_t>(rank),
                         equations.end(),
                         [&](const Generator& equation)
        {
            return equation.coordinates[column] != 0;
        });
        if (pivot == equations.end())
            continue;
        std::iter_swap(
            equations.begin() + static_cast<std::ptrdiff_t>(rank), pivot);
        if (equations[rank].coordinates[column] < 0)
            for (mpz_class& coordinate : equations[rank].coordinates)
                coordinate = -coordinate;
        const mpz_class pivotValue = equations[rank].coordinates[column];
        for (std::size_t other = 0; other < equations.size(); ++other)
        {
            if (other == rank || equations[other].coordinates[column] == 0)
                continue;
            const mpz_class factor = equations[other].coordinates[column];
            for (std::size_t coordinate = 0; coordinate < dimensions;
                    ++coordinate)
            {
                assignDifferenceOfProducts(
                    equations[other].coordinates[coordinate], pivotValue,
                    equations[other].coordinates[coordinate], factor,
                    equations[rank].coordinates[coordinate]);
            }
            normalizeGenerator(equations[other]);
        }
        pivots.push_back(column);
        ++rank;
    }
    equations.resize(rank);

    std::vector<Generator> result;
    result.reserve(forms.size());
    for (std::size_t equation = 0; equation < equations.size(); ++equation)
    {
        if (equations[equation].coordinates[pivots[equation]] < 0)
            for (mpz_class& coordinate : equations[equation].coordinates)
                coordinate = -coordinate;
        equations[equation].saturation.clear();
        result.push_back(equations[equation]);
    }
    for (const Generator& form : forms)
    {
        if (form.line)
            continue;
        Generator reduced = form;
        reduced.saturation.clear();
        for (std::size_t equation = 0; equation < equations.size(); ++equation)
        {
            const mpz_class factor = reduced.coordinates[pivots[equation]];
            if (factor == 0)
                continue;
            const mpz_class pivotValue =
                equations[equation].coordinates[pivots[equation]];
            for (std::size_t coordinate = 0; coordinate < dimensions;
                    ++coordinate)
            {
                assignDifferenceOfProducts(
                    reduced.coordinates[coordinate], pivotValue,
                    reduced.coordinates[coordinate], factor,
                    equations[equation].coordinates[coordinate]);
            }
            normalizeGenerator(reduced);
        }
        result.push_back(std::move(reduced));
    }
    return uniqueGenerators(std::move(result));
}

std::vector<Inequality> inequalitiesFromForms(
    const std::vector<Generator>& forms, bool nnc)
{
    std::vector<Generator> canonicalForms;
    const std::vector<Generator>* source = &forms;
    if (std::any_of(forms.begin(), forms.end(),
                    [](const Generator& form)
{
    return form.line;
}))
    {
        canonicalForms = canonicalizePolarForms(forms, nnc);
        source = &canonicalForms;
    }
    const std::size_t variableOffset = generatorVariableOffset(nnc);
    std::vector<Inequality> result;
    result.reserve(2 * source->size());
    for (const Generator& form : *source)
    {
        const auto first = std::find_if(
                               form.coordinates.begin() +
                               static_cast<std::ptrdiff_t>(variableOffset),
                               form.coordinates.end(),
                               [](const mpz_class& coordinate)
        {
            return coordinate != 0;
        });
        if (first == form.coordinates.end())
        {
            // Forms over only t/epsilon encode the homogeneous slice
            // invariants t>=epsilon>=0. They do not constrain public
            // variables and are omitted from H output.
            continue;
        }
        mpz_class divisor = *first;
        if (divisor < 0)
            divisor = -divisor;
        Inequality inequality;
        inequality.coefficients.reserve(form.coordinates.size() -
                                        variableOffset);
        for (auto coordinate =
                    form.coordinates.begin() +
                    static_cast<std::ptrdiff_t>(variableOffset);
                coordinate != form.coordinates.end(); ++coordinate)
        {
            inequality.coefficients.push_back(Rational::fromRaw(
                                                  mpq_class(*coordinate, divisor)));
        }
        inequality.bound = Rational::fromRaw(
                               mpq_class(-form.coordinates.front(), divisor));
        inequality.strict = nnc && form.coordinates[1] > 0;
        result.push_back(inequality);
        if (form.line)
        {
            for (Rational& coefficient : inequality.coefficients)
                coefficient = -coefficient;
            inequality.bound = -inequality.bound;
            inequality.strict = false;
            result.push_back(std::move(inequality));
        }
    }

    // Polar generators are already unique primitive rays. Normalize their
    // public order by coefficient vector without the generic map-based pass,
    // which would allocate and divide every Rational a second time.
    const auto less = [](const Inequality& lhs, const Inequality& rhs)
    {
        return std::lexicographical_compare(
                   lhs.coefficients.begin(), lhs.coefficients.end(),
                   rhs.coefficients.begin(), rhs.coefficients.end());
    };
    std::sort(result.begin(), result.end(), less);
    std::size_t kept = 0;
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        if (kept != 0 &&
                result[kept - 1].coefficients == result[index].coefficients)
        {
            if (tighter(result[index], result[kept - 1]))
                result[kept - 1] = std::move(result[index]);
            continue;
        }
        if (kept != index)
            result[kept] = std::move(result[index]);
        ++kept;
    }
    result.resize(kept);
    return result;
}

GeneratorSystem primalSystem(const GeneratorSystem& polar)
{
    GeneratorSystem result;
    result.generators = simplifiedInputGenerators(polar);
    result.constraints = generatorHalfspaces(polar.generators);
    result.nnc = polar.nnc;
    return result;
}

GeneratorSystem intersectGeneratorsWithConstraints(
    std::vector<Generator> generators,
    std::vector<std::vector<mpz_class>> processed,
    const std::vector<Inequality>& added, std::size_t dimensions, bool nnc)
{
    GeneratorSystem result = intersectCone(
                                 std::move(generators), std::move(processed),
                                 homogeneousConstraints(added, dimensions, nnc), true);
    result.nnc = nnc;
    return result;
}

GeneratorSystem generatorsFromConstraints(
    const std::vector<Inequality>& inequalities, std::size_t dimensions)
{
    const bool nnc = hasStrictConstraint(inequalities);
    const std::size_t variableOffset = generatorVariableOffset(nnc);
    const std::size_t homogeneousDimensions = dimensions + variableOffset;
    // The closed top cone has ray t. The NNC top cone has the two rays
    // (t,epsilon)=(1,0),(1,1), which generate t>=epsilon>=0. Affine
    // coordinates remain explicit lines in both representations.
    std::vector<Generator> initial;
    Generator closureOrigin;
    closureOrigin.coordinates.resize(homogeneousDimensions);
    closureOrigin.coordinates[0] = 1;
    initial.push_back(closureOrigin);
    if (nnc)
    {
        Generator includedOrigin = std::move(closureOrigin);
        includedOrigin.coordinates[1] = 1;
        initial.push_back(std::move(includedOrigin));
    }
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension)
    {
        Generator line;
        line.coordinates.resize(homogeneousDimensions);
        line.coordinates[dimension + variableOffset] = 1;
        line.line = true;
        initial.push_back(std::move(line));
    }

    std::vector<std::vector<mpz_class>> rows =
        homogeneousConstraints(inequalities, dimensions, nnc);
    std::vector<std::vector<mpz_class>> homogeneousBounds;
    if (nnc)
    {
        std::vector<mpz_class> nonnegativeEpsilon(homogeneousDimensions);
        nonnegativeEpsilon[1] = -1;
        homogeneousBounds.push_back(std::move(nonnegativeEpsilon));
        std::vector<mpz_class> epsilonAtMostHomogeneous(
            homogeneousDimensions);
        epsilonAtMostHomogeneous[0] = -1;
        epsilonAtMostHomogeneous[1] = 1;
        homogeneousBounds.push_back(std::move(epsilonAtMostHomogeneous));
    }
    else
    {
        std::vector<mpz_class> nonnegativeHomogeneous(homogeneousDimensions);
        nonnegativeHomogeneous[0] = -1;
        homogeneousBounds.push_back(std::move(nonnegativeHomogeneous));
    }
    GeneratorSystem result = intersectCone(
                                 std::move(initial), std::move(homogeneousBounds), std::move(rows));
    result.nnc = nnc;

    // A non-empty polyhedron must have a generator with positive homogeneous
    // coordinate.  A cone containing only recession directions is the
    // homogenization of the empty affine slice.
    const bool hasPoint = std::any_of(
                              result.generators.begin(), result.generators.end(),
                              [nnc](const Generator& generator)
    {
        return !generator.line && generator.coordinates.front() > 0 &&
               (!nnc || generator.coordinates[1] > 0);
    });
    if (!hasPoint)
        result.generators.clear();
    return result;
}

std::vector<Inequality> constraintsFromGenerators(
    const std::vector<Generator>& generators, std::size_t dimensions,
    bool nnc,
    GeneratorSystem* minimizedPrimal, GeneratorSystem* polarOutput)
{
    const std::size_t homogeneousDimensions =
        dimensions + generatorVariableOffset(nnc);
    std::vector<std::vector<mpz_class>> halfspaces =
        generatorHalfspaces(generators);

    // The valid homogeneous linear forms are the polar cone of the generator
    // cone.  Applying the same double-description kernel to that polar gives
    // every facet/equality row of the original polyhedron.
    GeneratorSystem polar = intersectCone(
                                fullSpaceCone(homogeneousDimensions), {},
                                std::move(halfspaces));
    polar.nnc = nnc;
    if (minimizedPrimal != nullptr)
        *minimizedPrimal = primalSystem(polar);
    std::vector<Inequality> result =
        inequalitiesFromForms(polar.generators, nnc);
    if (polarOutput != nullptr)
        *polarOutput = std::move(polar);
    // The pointed phase of the DD kernel emits only adjacent extreme forms;
    // after normalization these are already the facets/equality directions.
    // Running one LP per row here repeats the minimization just performed by
    // the saturation matrix and dominates V -> H conversion in dimensions
    // where a hull has many facets.
    return result;
}

} // namespace

ConvexPolyhedraDomain ConvexPolyhedraDomain::top(const ConvexPolyhedraConfig& config)
{
    return top(DimensionLayout(), config);
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::bottom(const ConvexPolyhedraConfig& config)
{
    return bottom(DimensionLayout(), config);
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::fromConstraints(const LinearConstraintSet& constraints, const ConvexPolyhedraConfig& config)
{
    auto result = top(config);
    result.assumeAll(constraints);
    return result;
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::fromBox(const BoxDomain& box, const ConvexPolyhedraConfig& config)
{
    return box.isBottom() ? bottom(config) : fromConstraints(box.toConstraints(), config);
}

BoxDomain ConvexPolyhedraDomain::toBox() const
{
    BoxSemanticConfig options;
    options.integerTightening = config().integerTightening;
    auto result = isBottom() ? BoxDomain::bottom(options) : BoxDomain::top(options);
    if (!isBottom())
        for (const auto& entry : layout_.variables())
        {
            const auto value = bound(entry.variable);
            const LinearExpression variable(entry.variable);
            if (value.lower().isFinite())
                result.assume(LinearConstraint(
                                  variable - LinearExpression(value.lower().value()),
                                  value.lower().isStrict() ? ConstraintKind::GreaterThan : ConstraintKind::GreaterEqual));
            if (value.upper().isFinite())
                result.assume(LinearConstraint(
                                  variable - LinearExpression(value.upper().value()),
                                  value.upper().isStrict() ? ConstraintKind::LessThan : ConstraintKind::LessEqual));
        }
    return result;
}

std::vector<Variable> ConvexPolyhedraDomain::supportVariables() const
{
    std::vector<Variable> result;
    result.reserve(layout_.size());
    for (const auto& entry : layout_.variables())
        result.push_back(entry.variable);
    return result;
}

void ConvexPolyhedraDomain::ensureVariables(const std::vector<Variable>& variables)
{
    std::set<Variable> missing;
    for (Variable variable : variables)
        if (!layout_.contains(variable))
            missing.insert(variable);
    if (missing.empty())
        return;
    std::vector<DimensionEntry> added;
    for (Variable variable : missing)
        added.emplace_back(variable);
    changeLayout(layout_.add(added));
}

void ConvexPolyhedraDomain::ensureExpression(const LinearExpression& expression)
{
    std::vector<Variable> variables;
    for (const auto& term : expression.terms())
        variables.push_back(term.first);
    ensureVariables(variables);
}

void ConvexPolyhedraDomain::project(const std::vector<Variable>& retained)
{
    std::set<Variable> keep(retained.begin(), retained.end());
    std::vector<Variable> removed;
    for (const auto& entry : layout_.variables())
        if (!keep.count(entry.variable))
            removed.push_back(entry.variable);
    changeLayout(layout_.remove(removed));
    recordOperation(OperationKind::Forget, ApproximationKind::Exact, true);
}

void ConvexPolyhedraDomain::expand(Variable source, const std::vector<Variable>& copies)
{
    std::set<Variable> seen;
    for (Variable variable : copies)
        if (variable == source || variable.type() != source.type() || !seen.insert(variable).second)
        {
            assert(false && "expand requires distinct, same-type copies");
            std::abort();
        }
    // Copies are overwritten, just as in Box. A coordinate retained after a
    // previous forget must not change the meaning of a fresh expansion.
    changeLayout(layout_.remove(copies));
    ensureVariables({source});
    std::vector<DimensionEntry> added;
    for (Variable variable : copies)
        added.emplace_back(variable);
    expandDimensions(source, added);
}

ConvexPolyhedraDomain ConvexPolyhedraDomain::fromGenerators(
    const std::vector<Variable>& variables, const PolyhedraGeneratorSet& generators,
    const ConvexPolyhedraConfig& config)
{
    std::vector<DimensionEntry> entries;
    for (Variable variable : variables)
        entries.emplace_back(variable);
    const DimensionLayout layout(std::move(entries));
    auto ordered = generators;
    for (auto& generator : ordered)
    {
        if (generator.coordinates.size() != variables.size())
        {
            assert(false && "generator coordinate count does not match variables");
            std::abort();
        }
        const auto source = generator.coordinates;
        for (std::size_t i = 0; i < variables.size(); ++i)
            generator.coordinates[layout.dimensionOf(variables[i])] = source[i];
    }
    return fromGeneratorsInLayout(layout, ordered, config);
}

PolyhedraGeneratorSet ConvexPolyhedraDomain::toGenerators(
    const std::vector<Variable>& variables) const
{
    std::vector<DimensionEntry> entries;
    for (Variable variable : variables)
        entries.emplace_back(variable);
    auto projected = *this;
    projected.changeLayout(DimensionLayout(std::move(entries)));
    auto result = projected.toGenerators();
    for (auto& generator : result)
    {
        const auto source = generator.coordinates;
        for (std::size_t i = 0; i < variables.size(); ++i)
            generator.coordinates[i] = source[projected.layout_.dimensionOf(variables[i])];
    }
    return result;
}

} // namespace SVF
