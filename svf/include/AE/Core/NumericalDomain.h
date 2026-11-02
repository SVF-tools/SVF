//===- NumericalDomain.h -- Numerical properties and Box domain -*- C++ -*-===//
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
// Contributors: Jiawei Wang, Xiao Cheng, Jiawei Yang
//
//===----------------------------------------------------------------------===//

#ifndef SVF_AE_NUMERICAL_DOMAIN_H
#define SVF_AE_NUMERICAL_DOMAIN_H

#include "AE/Core/AbstractDomain.h"

#include <cstdlib>
#include <cassert>
#include <gmpxx.h>
#include <mpfr.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <iterator>

namespace SVF
{

class Integer
{
public:
    Integer();
    explicit Integer(std::int64_t value);
    explicit Integer(const std::string& value);

    const mpz_class& value() const
    {
        return value_;
    }
    std::string toString() const;

    friend bool operator==(const Integer& lhs, const Integer& rhs)
    {
        return lhs.value_ == rhs.value_;
    }

private:
    mpz_class value_;
};

class Rational
{
public:
    Rational();
    explicit Rational(std::int64_t value);
    explicit Rational(const Integer& value);
    explicit Rational(const std::string& value);
    Rational(const Integer& numerator, const Integer& denominator);

    static Rational fromRaw(const mpq_class& value);
    static Rational fromDouble(double value);

    const mpq_class& value() const
    {
        return value_;
    }
    bool isZero() const
    {
        return value_ == 0;
    }
    bool isInteger() const;
    bool tryToInt64(std::int64_t& result) const;
    std::int64_t toInt64() const;
    double toDouble() const;
    int sign() const
    {
        return mpq_sgn(value_.get_mpq_t());
    }
    std::string toString() const;

    Rational floor() const;
    Rational ceil() const;
    Rational dividedByPowerOfTwo(unsigned exponent) const;
    Rational& assignSum(const Rational& lhs, const Rational& rhs);
    Rational& divideByPowerOfTwoInPlace(unsigned exponent);

    Rational& operator+=(const Rational& rhs);
    Rational& operator-=(const Rational& rhs);
    Rational& operator*=(const Rational& rhs);
    Rational& operator/=(const Rational& rhs);

    friend Rational operator+(Rational lhs, const Rational& rhs)
    {
        return lhs += rhs;
    }
    friend Rational operator-(Rational lhs, const Rational& rhs)
    {
        return lhs -= rhs;
    }
    friend Rational operator*(Rational lhs, const Rational& rhs)
    {
        return lhs *= rhs;
    }
    friend Rational operator/(Rational lhs, const Rational& rhs)
    {
        return lhs /= rhs;
    }
    friend Rational operator-(const Rational& value)
    {
        return Rational::fromRaw(-value.value_);
    }

    friend bool operator==(const Rational& lhs, const Rational& rhs)
    {
        return lhs.value_ == rhs.value_;
    }
    friend bool operator!=(const Rational& lhs, const Rational& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(const Rational& lhs, const Rational& rhs)
    {
        return lhs.value_ < rhs.value_;
    }
    friend bool operator<=(const Rational& lhs, const Rational& rhs)
    {
        return lhs.value_ <= rhs.value_;
    }
    friend bool operator>(const Rational& lhs, const Rational& rhs)
    {
        return rhs < lhs;
    }
    friend bool operator>=(const Rational& lhs, const Rational& rhs)
    {
        return rhs <= lhs;
    }

private:
    explicit Rational(mpq_class value, int);
    mpq_class value_;
};

/// An ordered extended-rational endpoint.  For a finite upper bound, strict
/// means "< value" and non-strict means "<= value".  At an equal numeric
/// value a strict bound is tighter than a non-strict one.
class Bound
{
public:
    enum class Kind
    {
        MinusInfinity,
        Finite,
        PlusInfinity
    };

    Bound();
    static Bound minusInfinity();
    static Bound finite(Rational value, bool strict = false);
    static Bound plusInfinity();

    Kind kind() const
    {
        return kind_;
    }
    bool isFinite() const
    {
        return kind_ == Kind::Finite;
    }
    bool isMinusInfinity() const
    {
        return kind_ == Kind::MinusInfinity;
    }
    bool isPlusInfinity() const
    {
        return kind_ == Kind::PlusInfinity;
    }
    const Rational& value() const;
    bool isStrict() const
    {
        return strict_;
    }

    /// Ordering used by upper bounds: tighter/smaller first.
    static int compare(const Bound& lhs, const Bound& rhs);
    static Bound min(const Bound& lhs, const Bound& rhs);
    static Bound max(const Bound& lhs, const Bound& rhs);
    static Bound add(const Bound& lhs, const Bound& rhs);
    Bound& assignSum(const Bound& lhs, const Bound& rhs);
    Bound& divideByTwoInPlace();
    static Bound divideByTwo(const Bound& bound);
    static Bound divideByPositive(const Bound& bound, const Rational& divisor);

    std::string toString() const;

    friend bool operator==(const Bound& lhs, const Bound& rhs)
    {
        return compare(lhs, rhs) == 0;
    }
    friend bool operator!=(const Bound& lhs, const Bound& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(const Bound& lhs, const Bound& rhs)
    {
        return compare(lhs, rhs) < 0;
    }
    friend bool operator<=(const Bound& lhs, const Bound& rhs)
    {
        return compare(lhs, rhs) <= 0;
    }

private:
    Bound(Kind kind, Rational value, bool strict);

    Kind kind_ = Kind::PlusInfinity;
    Rational value_;
    bool strict_ = false;
};

class Interval
{
public:
    Interval();
    Interval(Bound lower, Bound upper);

    static Interval top();
    static Interval bottom();
    static Interval singleton(const Rational& value);
    static Interval closed(const Rational& lower, const Rational& upper);

    const Bound& lower() const
    {
        return lower_;
    }
    const Bound& upper() const
    {
        return upper_;
    }
    bool isTop() const;
    bool isBottom() const;
    bool isSingleton() const;
    bool isZero() const;
    bool contains(const Rational& value) const;
    bool isSubsetOf(const Interval& other) const;
    const Rational& singletonValue() const;
    void joinWith(const Interval& other);
    void meetWith(const Interval& other);
    void widenWith(const Interval& next);
    void narrowWith(const Interval& next);
    std::string toString() const;

    friend bool operator==(const Interval& lhs, const Interval& rhs)
    {
        return lhs.lower_ == rhs.lower_ && lhs.upper_ == rhs.upper_;
    }
    friend bool operator!=(const Interval& lhs, const Interval& rhs)
    {
        return !(lhs == rhs);
    }

private:
    Bound lower_;
    Bound upper_;
};

Interval add(const Interval& lhs, const Interval& rhs);
Interval subtract(const Interval& lhs, const Interval& rhs);
Interval multiply(const Interval& lhs, const Interval& rhs);
Interval divide(const Interval& lhs, const Interval& rhs,
                bool integerDivision = true);
Interval remainder(const Interval& lhs, const Interval& rhs);
Interval bitwiseAnd(const Interval& lhs, const Interval& rhs);
Interval bitwiseOr(const Interval& lhs, const Interval& rhs);
Interval bitwiseXor(const Interval& lhs, const Interval& rhs);
Interval shiftLeft(const Interval& lhs, const Interval& rhs);
Interval shiftRight(const Interval& lhs, const Interval& rhs);
Interval equalTo(const Interval& lhs, const Interval& rhs);
Interval notEqualTo(const Interval& lhs, const Interval& rhs);
Interval lessThan(const Interval& lhs, const Interval& rhs);
Interval lessEqual(const Interval& lhs, const Interval& rhs);
Interval greaterThan(const Interval& lhs, const Interval& rhs);
Interval greaterEqual(const Interval& lhs, const Interval& rhs);
/// Complete mathematical range of a fixed-width signed or unsigned integer.
Interval integerRange(unsigned bitWidth, bool isSigned);
/// Convex hull of the fixed-width two's-complement image of an integer
/// interval.  This is exact when the interval does not cross a modular
/// discontinuity and otherwise conservatively returns the complete range.
Interval wrapIntegerInterval(const Interval& operand, unsigned bitWidth,
                             bool isSigned);
/// Convex hull of an LLVM zero extension from a fixed-width source.  The
/// source interval uses the signedness selected by the frontend.
Interval zeroExtendIntegerInterval(const Interval& operand,
                                   unsigned sourceBitWidth,
                                   bool sourceIsSigned);
/// Convert a floating interval to a fixed-width integer using LLVM's
/// round-toward-zero semantics. If the operand may produce poison because it
/// is non-finite or outside the destination range, return that complete range.
Interval floatToInteger(const Interval& operand, unsigned bitWidth,
                        bool isSigned);

enum class RoundingMode
{
    NearestTiesToEven,
    TowardZero,
    TowardPositive,
    TowardNegative
};

class MpfrValue
{
public:
    explicit MpfrValue(mpfr_prec_t precision);
    MpfrValue(const MpfrValue& rhs);
    MpfrValue(MpfrValue&& rhs) noexcept;
    MpfrValue& operator=(const MpfrValue& rhs);
    MpfrValue& operator=(MpfrValue&& rhs) noexcept;
    ~MpfrValue();

    mpfr_ptr raw()
    {
        return value_;
    }
    mpfr_srcptr raw() const
    {
        return value_;
    }
    mpfr_prec_t precision() const
    {
        return mpfr_get_prec(value_);
    }

    void set(const Rational& value, mpfr_rnd_t rounding);
    Rational toRational() const;

private:
    mpfr_t value_;
};

/// Ground MPFR operations used at the floating-semantics boundary.  The
/// returned rational is the exact dyadic value of the rounded MPFR result.
class FloatSemantics
{
public:
    static Rational add(const Rational& lhs, const Rational& rhs,
                        unsigned significandBits, RoundingMode rounding);
    static Rational subtract(const Rational& lhs, const Rational& rhs,
                             unsigned significandBits, RoundingMode rounding);
    static Rational multiply(const Rational& lhs, const Rational& rhs,
                             unsigned significandBits, RoundingMode rounding);
    static Rational divide(const Rational& lhs, const Rational& rhs,
                           unsigned significandBits, RoundingMode rounding);

private:
    enum class BinaryOperation
    {
        Add,
        Subtract,
        Multiply,
        Divide
    };

    static Rational evaluate(BinaryOperation operation, const Rational& lhs,
                             const Rational& rhs, unsigned significandBits,
                             RoundingMode rounding);
};

class LinearExpression;
class TreeExpression;
class LinearConstraint;
class TreeConstraint;
struct LinearAssignment;
struct TreeAssignment;
struct WideningPolicy;
using LinearAssignmentList = std::vector<LinearAssignment>;
using TreeAssignmentList = std::vector<TreeAssignment>;
using LinearConstraintSet = std::vector<LinearConstraint>;

enum class ApproximationKind
{
    Exact,
    SoundOverApproximation,
    UnsupportedFallback
};

enum class OperationKind
{
    Assignment,
    Assumption,
    Substitution,
    Forget,
    Join,
    Meet,
    Widening,
    Narrowing,
    TopologicalClosure,
    Canonicalization,
    Expand,
    Fold,
    GeneratorImport,
    GeneratorExport
};

/// Information about the most recently completed mutating
/// operation. `exact` means that no semantic approximation beyond the
/// selected abstract domain was introduced. `best` means that the operation
/// used the strongest implemented transformer for that domain and syntax.
struct OperationMetadata
{
    OperationKind operation = OperationKind::Assignment;
    ApproximationKind approximation = ApproximationKind::Exact;
    bool exact = true;
    bool best = true;
    std::string reason;
};

struct NumericalTelemetry
{
    std::uint64_t relationalClosureCalls = 0;
    std::uint64_t relationalClosureConstraintExports = 0;
    std::uint64_t relationalClosureIndexedQueries = 0;
    std::uint64_t joinCalls = 0;
    std::uint64_t wideningCalls = 0;
    std::uint64_t narrowingCalls = 0;
    std::uint64_t partialInsideOperations = 0;
    std::uint64_t partialFallbackOperations = 0;
    std::uint64_t partialProjections = 0;
};

struct Diagnostic
{
    OperationKind operation;
    ApproximationKind approximation;
    std::string reason;
};

class DiagnosticSink
{
public:
    virtual ~DiagnosticSink() = default;
    virtual void report(const Diagnostic& diagnostic) = 0;
};

/// Common interface for numerical abstract properties. The representation and
/// lattice algorithms remain domain-specific; clients such as the SVF adapter
/// and test oracles only need this transfer/query surface.
class NumericalDomain : public AbstractDomain
{
public:
    using RawBuffer = std::vector<std::uint8_t>;

    ~NumericalDomain() override = default;

    /// Process-local counters used only by explicitly enabled profiling runs.
    /// Analysis is single-threaded while these counters are active.
    static void beginTelemetry();
    static NumericalTelemetry endTelemetry();

    /// Return a deterministic semantic hash. Compatible properties that are
    /// equivalent according to isEquivalentTo() have the same hash. Hash
    /// equality is not a substitute for an exact equivalence check.
    std::uint64_t hash() const;

    /// Serialize the domain kind, operation-relevant configuration, and
    /// canonical mathematical state into a versioned binary buffer.
    /// Diagnostic sinks are observational and are not serialized.
    RawBuffer serializeRaw() const;

    /// Restore a Box, Octagon, or Convex Polyhedra property from serializeRaw().
    /// Malformed, truncated, corrupt, or unsupported data is rejected.
    static std::unique_ptr<NumericalDomain> deserializeRaw(
        const RawBuffer& buffer);

    const OperationMetadata& lastOperation() const
    {
        return lastOperation_;
    }
    virtual void assign(Variable target,
                        const LinearExpression& expression) = 0;
    virtual void assign(Variable target, const TreeExpression& expression) = 0;
    /// Assign every target simultaneously. Every right-hand side reads the
    /// same incoming state, including old values of all assigned targets.
    virtual void assignParallel(const LinearAssignmentList& assignments) = 0;
    virtual void assignParallel(const TreeAssignmentList& assignments);
    /// Compute the preimage of this post-state under target := expression.
    virtual void substitute(Variable target,
                            const LinearExpression& expression) = 0;
    void substitute(Variable target, const TreeExpression& expression);
    /// Simultaneous backward substitution. Every replacement is interpreted
    /// over the same pre-state, including cyclic replacements.
    virtual void substituteParallel(
        const LinearAssignmentList& assignments) = 0;
    void substituteParallel(const TreeAssignmentList& assignments);
    virtual void assume(const LinearConstraint& constraint) = 0;
    virtual void assume(const TreeConstraint& constraint) = 0;
    virtual void forget(Variable variable) = 0;
    /// Existentially remove every variable not present in `retained`.
    virtual void project(const std::vector<Variable>& retained) = 0;
    /// Duplicate a summary variable into fresh variables. Every copy has the
    /// source variable's relations with all other variables, while the
    /// expanded variables remain mutually unrelated except where those
    /// duplicated relations logically imply otherwise.
    virtual void expand(Variable source,
                        const std::vector<Variable>& copies) = 0;
    /// Merge several materialized variables into `target` by taking the
    /// abstract hull of every possible representative, then remove the other
    /// variables.
    virtual void fold(Variable target, const std::vector<Variable>& folded) = 0;

    /// Assume every constraint, letting them propagate into each other until
    /// the state stops moving.
    ///
    /// Assuming them one at a time is weaker than a client of a guard such as
    /// `a && b && c` expects: a bound learned from the last constraint cannot
    /// flow back into the first. A domain that is exact on linear constraints
    /// settles in one pass and pays only the comparison; a non-relational or
    /// octagonal domain is the reason this exists.
    virtual void assumeAll(const LinearConstraintSet& constraints);

    virtual CheckResult entails(const LinearConstraint& constraint) const = 0;
    virtual Interval bound(Variable variable) const = 0;
    /// Bound a complete affine expression using the relational backend, not
    /// merely interval arithmetic over its individual variables.
    virtual Interval bound(const LinearExpression& expression) const = 0;
    /// Affine integer/real trees use bound(LinearExpression). Nonlinear and
    /// finite IEEE trees use sound interval evaluation with outward rounding;
    /// exceptional IEEE outcomes that cannot be represented numerically lose
    /// the affected bound to top.
    Interval bound(const TreeExpression& expression) const;
    /// Variables with explicitly represented numerical support. Absence means
    /// unconstrained, never undefined. The result is sorted and unique.
    virtual std::vector<Variable> supportVariables() const = 0;
    std::vector<Variable> supportVariablesBefore(Variable upperBound) const;
    /// Return the transitive variable component connected to `seeds` by the
    /// represented linear constraints. This is used when a semi-sparse client
    /// must materialize a complete relation rather than unary projections.
    std::vector<Variable> relationalClosure(
        const std::vector<Variable>& seeds) const;
    /// Strongly replace one coordinate by an interval while forgetting its
    /// previous relations. This is the public product-domain entry point for
    /// an interval-valued transfer.
    void assignBound(Variable target, const Interval& value)
    {
        assignInterval(target, value);
    }
    virtual LinearConstraintSet toConstraints() const = 0;

    /// Replace strict boundaries by non-strict boundaries. This is the
    /// topological closure operation, not DBM/polyhedral normalization.
    virtual void close() = 0;
    /// Materialize the backend's canonical representation and remove semantic
    /// redundancy where the representation supports it.
    virtual void canonicalize() = 0;
    /// Dense native representations use canonicalization as their minimize
    /// operation.
    void minimize()
    {
        canonicalize();
    }

protected:
    /// Domain-specific closure hook. Relational domains with an indexed
    /// physical representation can answer this without exporting constraints.
    virtual std::vector<Variable> relationalClosureState(
        const std::vector<Variable>& seeds) const;
    /// Delegate a product wrapper's already-counted closure query to one of
    /// its numerical facets without incrementing the public call counter a
    /// second time. The facet still records export-vs-index telemetry.
    static std::vector<Variable> delegateRelationalClosureState(
        const NumericalDomain& facet,
        const std::vector<Variable>& seeds);
    void recordRelationalClosureConstraintExport() const;
    void recordRelationalClosureIndexedQuery() const;
    /// Evaluate nonlinear and finite IEEE trees by sound interval semantics,
    /// applying each IEEE node's requested rounding mode at its endpoints.
    /// Exceptional IEEE outcomes conservatively produce top.
    Interval evaluateTreeExpression(const TreeExpression& expression) const;
    /// Necessary affine consequences of a nonlinear tree guard. The result
    /// may be empty when the guard cannot safely refine the selected domain.
    LinearConstraintSet treeConstraintConsequences(
        const TreeConstraint& constraint) const;
    /// Strongly update a target from an already-computed interval. This is
    /// used to preserve simultaneous semantics for nonlinear tree batches.
    virtual void assignInterval(Variable target, const Interval& value);
    void recordOperation(OperationKind operation,
                         ApproximationKind approximation, bool best,
                         std::string reason = {}) const;
    void recordPartialOperation(bool inside) const;
    void recordPartialProjection() const;

private:
    mutable OperationMetadata lastOperation_;
};

struct BoxSemanticConfig
{
    bool integerTightening = true;
    std::shared_ptr<DiagnosticSink> diagnostics;

    bool operationCompatible(const BoxSemanticConfig& other) const
    {
        return integerTightening == other.integerTightening;
    }
};

/// Non-relational numerical property with finite non-Top support over stable
/// typed Variables. Variable IDs are the global sparse page coordinates.
class BoxDomain final : public NumericalDomain
{
    friend class BoxAddressDomain;

public:
    using NumericalDomain::assignParallel;
    using NumericalDomain::bound;
    using NumericalDomain::substitute;
    using NumericalDomain::substituteParallel;

    static BoxDomain top(const BoxSemanticConfig& config = {});
    static BoxDomain bottom(const BoxSemanticConfig& config = {});
    static BoxDomain fromConstraints(const LinearConstraintSet& constraints,
                                     const BoxSemanticConfig& config = {});

    BoxDomain(const BoxDomain& other);
    BoxDomain(BoxDomain&& other) noexcept = default;
    BoxDomain& operator=(const BoxDomain& other) = default;
    BoxDomain& operator=(BoxDomain&& other) noexcept = default;

    DomainKind kind() const noexcept override
    {
        return DomainKind::Box;
    }
    std::unique_ptr<AbstractDomain> clone() const override;
    const BoxSemanticConfig& config() const
    {
        return config_;
    }

    void assign(Variable target, const LinearExpression& expression) override;
    void assign(Variable target, const TreeExpression& expression) override;
    void assignParallel(const LinearAssignmentList& assignments) override;
    void substitute(Variable target,
                    const LinearExpression& expression) override;
    void substituteParallel(const LinearAssignmentList& assignments) override;
    void assume(const LinearConstraint& constraint) override;
    void assume(const TreeConstraint& constraint) override;
    void assumeAll(const LinearConstraintSet& constraints) override;
    void forget(Variable variable) override;
    void project(const std::vector<Variable>& retained) override;
    void expand(Variable source, const std::vector<Variable>& copies) override;
    void fold(Variable target, const std::vector<Variable>& folded) override;

    CheckResult entails(const LinearConstraint& constraint) const override;
    Interval bound(Variable variable) const override;
    Interval bound(const LinearExpression& expression) const override;
    /// Variables whose bounds are represented explicitly because they are
    /// stricter than the analysis-wide Top default. This is a storage
    /// observation for sparse scheduling; absence never means undefined.
    std::vector<Variable> constrainedVariables() const;
    std::vector<Variable> constrainedVariablesBefore(
        Variable upperBound) const;
    std::vector<Variable> supportVariables() const override
    {
        return constrainedVariables();
    }
    LinearConstraintSet toConstraints() const override;
    void close() override;
    void canonicalize() override;

    BoxDomain join(const BoxDomain& other) const;
    BoxDomain meet(const BoxDomain& other) const;
    BoxDomain widen(const BoxDomain& next) const;
    BoxDomain widen(const BoxDomain& next, const WideningPolicy& policy) const;
    BoxDomain narrow(const BoxDomain& next) const;

private:
    static constexpr std::size_t BoundsPerPage = 8;

    struct BoundSlot
    {
        Variable variable;
        Interval interval;

        friend bool operator==(const BoundSlot& lhs, const BoundSlot& rhs)
        {
            return lhs.variable == rhs.variable && lhs.interval == rhs.interval;
        }
    };

    struct BoundPage
    {
        std::array<std::optional<BoundSlot>, BoundsPerPage> bounds;
    };

    struct BoundPageEntry
    {
        std::size_t index;
        std::shared_ptr<BoundPage> page;
    };

    using BoundPageDirectory = std::vector<BoundPageEntry>;
    BoxDomain(BoxSemanticConfig config, bool bottom);

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<BoxDomain>();
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

    const BoxDomain& requireBox(const AbstractDomain& other) const;
    const Interval& boundAt(Variable variable) const;
    BoundPage& writablePage(std::size_t pageIndex);
    void eraseBound(Variable variable);
    static bool pageIsEmpty(const BoundPage& page);
    std::vector<Variable> boundedVariables() const;
    std::vector<Variable> boundedVariablesBefore(
        std::uint32_t upperBound) const;
    void makeBottom();
    void canonicalize(Variable variable);
    void setBound(Variable variable, Interval interval);
    void report(OperationKind operation, ApproximationKind approximation,
                std::string reason, bool best = true) const;
    BoxSemanticConfig config_;
    /// Missing pages and empty slots denote top. Active pages are kept sorted,
    /// shared by property copies, and detached only when one of their bounds
    /// changes.
    BoundPageDirectory boundPages_;
    bool bottom_ = false;
};

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

    std::string toString() const;

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
    std::string toString() const;

private:
    LinearExpression expression_;
    ConstraintKind kind_;
};

using LinearConstraintSet = std::vector<LinearConstraint>;

struct WideningPolicy
{
    WideningPolicy() = default;

    explicit WideningPolicy(std::vector<Rational> thresholdValues)
        : thresholds(std::move(thresholdValues))
    {
    }

    WideningPolicy(std::vector<Rational> thresholdValues,
                   LinearConstraintSet linearThresholdValues)
        : thresholds(std::move(thresholdValues)),
          linearThresholds(std::move(linearThresholdValues))
    {
    }

    std::vector<Rational> thresholds;
    LinearConstraintSet linearThresholds;
};

struct LinearAssignment
{
    Variable target;
    LinearExpression expression;
};

using LinearAssignmentList = std::vector<LinearAssignment>;
LinearConstraint equal(LinearExpression lhs, LinearExpression rhs);
LinearConstraint notEqual(LinearExpression lhs, LinearExpression rhs);
LinearConstraint lessEqual(LinearExpression lhs, LinearExpression rhs);
LinearConstraint lessThan(LinearExpression lhs, LinearExpression rhs);
LinearConstraint greaterEqual(LinearExpression lhs, LinearExpression rhs);
LinearConstraint greaterThan(LinearExpression lhs, LinearExpression rhs);

enum class UnaryNumericOp
{
    Negate,
    Cast,
    SquareRoot
};

enum class BinaryNumericOp
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
        UnaryNumericOp operation, TreeExpression operand, NumericType type,
        RoundingMode rounding = RoundingMode::NearestTiesToEven);
    static TreeExpression binary(
        BinaryNumericOp operation, TreeExpression lhs, TreeExpression rhs,
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
    UnaryNumericOp unaryOperator() const
    {
        return unaryOperator_;
    }
    BinaryNumericOp binaryOperator() const
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
    /// mathematical integer/real semantics. Floating and nonlinear trees
    /// deliberately return nullopt and must use a sound backend fallback.
    std::optional<LinearExpression> asLinear() const;

private:
    Kind kind_ = Kind::Constant;
    NumericType type_ = NumericType::real();
    Rational constant_;
    Variable variable_;
    UnaryNumericOp unaryOperator_ = UnaryNumericOp::Negate;
    BinaryNumericOp binaryOperator_ = BinaryNumericOp::Add;
    RoundingMode roundingMode_ = RoundingMode::NearestTiesToEven;
    std::shared_ptr<const TreeExpression> lhs_;
    std::shared_ptr<const TreeExpression> rhs_;
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

struct TreeAssignment
{
    Variable target;
    TreeExpression expression;
};

using TreeAssignmentList = std::vector<TreeAssignment>;

/// Selects the physical carrier of an Octagon DBM.  This is deliberately not
/// part of the abstract semantics: states using different carriers may be
/// combined, compared, and converted without approximation.
enum class OctagonStorageKind
{
    DenseHalf,
    SparseFinite,
    ComponentDense
};

const char* octagonStorageKindName(OctagonStorageKind kind);
OctagonStorageKind octagonStorageKindFromName(const std::string& name);

struct OctagonConfig
{
    bool strongClosure = true;
    bool integerTightening = true;
    OctagonStorageKind storage = OctagonStorageKind::DenseHalf;
    /// ComponentDense only: share component matrices across abstract-state
    /// copies until the first write. Disabling this keeps identical abstract
    /// semantics but moves the matrix-copy cost from mutation to clone.
    bool componentCopyOnWrite = true;
    /// Use the quadratic single-variable closure repair where its precondition
    /// holds. Disabling it computes the same canonical state with full closure.
    bool incrementalClosure = true;
    std::shared_ptr<DiagnosticSink> diagnostics;

    /// Diagnostics affect observation only, not abstract-state semantics.
    bool operationCompatible(const OctagonConfig& other) const
    {
        return strongClosure == other.strongClosure &&
               integerTightening == other.integerTightening;
    }
};

using Dimension = std::size_t;

struct DimensionEntry
{
    Variable variable;
    explicit DimensionEntry(Variable value) : variable(value) {}
};

/// Physical coordinates, not a mathematical environment. Every domain denotes
/// a property over all typed Variables; variables absent here are unrestricted.
/// Immutable layouts are shared by copies. A relational operation extends or
/// aligns them internally before touching its matrix.
class DimensionLayout
{
public:
    DimensionLayout() : DimensionLayout(std::vector<DimensionEntry> {}) {}
    explicit DimensionLayout(std::vector<DimensionEntry> entries)
    {
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
        {
            return a.variable < b.variable;
        });
        for (std::size_t i = 1; i < entries.size(); ++i)
            if (entries[i - 1].variable == entries[i].variable)
            {
                assert(false && "duplicate relational coordinate");
                std::abort();
            }
        entries_ = std::make_shared<const std::vector<DimensionEntry>>(std::move(entries));
    }
    std::size_t size() const
    {
        return entries_->size();
    }
    bool empty() const
    {
        return entries_->empty();
    }
    bool contains(Variable variable) const
    {
        const auto it = find(variable);
        return it != entries_->end() && it->variable == variable;
    }
    Dimension dimensionOf(Variable variable) const
    {
        const auto it = find(variable);
        if (it == entries_->end() || it->variable != variable)
        {
            assert(false && "missing relational coordinate");
            std::abort();
        }
        return static_cast<Dimension>(it - entries_->begin());
    }
    Variable variableOf(Dimension dimension) const
    {
        return entries_->at(dimension).variable;
    }
    const NumericType& typeOf(Variable variable) const
    {
        return entries_->at(dimensionOf(variable)).variable.type();
    }
    const std::vector<DimensionEntry>& variables() const
    {
        return *entries_;
    }
    DimensionLayout add(const std::vector<DimensionEntry>& added) const
    {
        auto entries = *entries_;
        entries.insert(entries.end(), added.begin(), added.end());
        return DimensionLayout(std::move(entries));
    }
    DimensionLayout remove(const std::vector<Variable>& removed) const
    {
        std::vector<DimensionEntry> entries;
        for (const auto& entry : *entries_)
            if (std::find(removed.begin(), removed.end(), entry.variable) == removed.end())
                entries.push_back(entry);
        return DimensionLayout(std::move(entries));
    }
    DimensionLayout merge(const DimensionLayout& other) const
    {
        if (*this == other)
            return *this;
        std::vector<DimensionEntry> entries;
        std::set_union(entries_->begin(), entries_->end(), other.entries_->begin(), other.entries_->end(),
                       std::back_inserter(entries), [](const auto& a, const auto& b)
        {
            return a.variable < b.variable;
        });
        return DimensionLayout(std::move(entries));
    }
    friend bool operator==(const DimensionLayout& a, const DimensionLayout& b)
    {
        return a.entries_ == b.entries_ || (a.size() == b.size() &&
                                            std::equal(a.entries_->begin(), a.entries_->end(), b.entries_->begin(),
                                                    [](const auto& x, const auto& y)
        {
            return x.variable == y.variable;
        }));
    }
    friend bool operator!=(const DimensionLayout& a, const DimensionLayout& b)
    {
        return !(a == b);
    }

private:
    std::vector<DimensionEntry>::const_iterator find(Variable variable) const
    {
        return std::lower_bound(entries_->begin(), entries_->end(), variable,
                                [](const auto& entry, Variable value)
        {
            return entry.variable < value;
        });
    }
    std::shared_ptr<const std::vector<DimensionEntry>> entries_;
};

std::unique_ptr<NumericalDomain> makeNumericalDomain(
    DomainKind kind, bool bottom, const OctagonConfig& config = {});
std::unique_ptr<NumericalDomain> makeOctagonDomain(
    bool bottom = false, const OctagonConfig& config = {});

} // namespace SVF

#endif // SVF_AE_NUMERICAL_DOMAIN_H
