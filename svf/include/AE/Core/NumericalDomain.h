//===- NumericalDomain.h -- Shared numerical-domain API --------*- C++ -*-===//

#ifndef SVF_AE_NUMERICAL_DOMAIN_H
#define SVF_AE_NUMERICAL_DOMAIN_H

#include "AE/Core/AbstractState.h"
#include "AE/Core/LinearConstraint.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SVF::AbstractDomain
{

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
    EnvironmentChange,
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

/// APRON-style information about the most recently completed mutating
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

struct DomainCapabilities
{
    bool strictInequalities = false;
    bool integerTightening = false;
    bool thresholdWidening = false;
    bool narrowing = false;
    bool parallelAssignments = false;
    bool expressionBounds = false;
    bool backwardAssignments = false;
    bool topologicalClosure = false;
    bool canonicalization = false;
    bool expandFold = false;
    bool operationMetadata = false;
    bool generatorExchange = false;
    bool ieeeTreeExpressions = false;
    /// True only when nonlinear TreeExpression operations retain domain facts
    /// instead of applying a sound forget/ignore fallback.
    bool nonlinearTreeExpressions = false;
};

struct IntervalBox
{
    std::map<Variable, Interval> bounds;
};

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

    /// Constants used to delay a bound's jump to infinity.
    std::vector<Rational> thresholds;
    /// General linear thresholds retained by a widening when both operands
    /// entail them. Domains that cannot represent a threshold may ignore it
    /// conservatively when the selected domain cannot represent one.
    LinearConstraintSet linearThresholds;
};

struct LinearAssignment
{
    Variable target;
    LinearExpression expression;
};

using LinearAssignmentList = std::vector<LinearAssignment>;

struct TreeAssignment
{
    Variable target;
    TreeExpression expression;
};

using TreeAssignmentList = std::vector<TreeAssignment>;

/// Common interface for numerical abstract states. The representation and
/// lattice algorithms remain domain-specific; clients such as the SVF adapter
/// and test oracles only need this transfer/query surface.
class NumericalState : public AbstractState
{
public:
    using RawBuffer = std::vector<std::uint8_t>;

    ~NumericalState() override = default;

    /// Return a deterministic semantic hash. Compatible states that are
    /// equivalent according to isEquivalentTo() have the same hash. Hash
    /// equality is not a substitute for an exact equivalence check.
    std::uint64_t hash() const;

    /// Serialize the domain kind, operation-relevant configuration,
    /// environment, and canonical mathematical state into a versioned binary
    /// buffer. Diagnostic sinks are observational and are not serialized.
    RawBuffer serializeRaw() const;

    /// Restore a Box state from serializeRaw().
    /// Malformed, truncated, corrupt, or unsupported data is rejected.
    static std::unique_ptr<NumericalState> deserializeRaw(
        const RawBuffer& buffer);

    virtual DomainCapabilities capabilities() const = 0;
    const OperationMetadata& lastOperation() const
    {
        return lastOperation_;
    }
    virtual const VariableEnvironment& environment() const = 0;

    virtual void assign(Variable target,
                        const LinearExpression& expression) = 0;
    virtual void assign(Variable target, const TreeExpression& expression) = 0;
    /// Assign every target simultaneously. Every right-hand side reads the
    /// same incoming state, including old values of all assigned targets.
    /// The default implementation uses temporary dimensions; domains may
    /// override it with a representation-native implementation.
    virtual void assignParallel(const LinearAssignmentList& assignments);
    virtual void assignParallel(const TreeAssignmentList& assignments);
    /// Compute the preimage of this post-state under target := expression.
    /// This is APRON's substitute operation, not a forward strong update.
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
    virtual void changeEnvironment(
        const VariableEnvironment& environment,
        bool initializeNewVariablesToZero = false) = 0;

    /// Duplicate a summary dimension into new dimensions. Every copy has the
    /// source dimension's relations with all other dimensions, while the
    /// expanded dimensions remain mutually unrelated except where those
    /// duplicated relations logically imply otherwise. This is APRON's
    /// expand operation.
    virtual void expand(Variable source,
                        const std::vector<VariableDeclaration>& copies) = 0;
    /// Merge several materialized dimensions into `target` by taking the
    /// abstract hull of every possible representative, then remove the other
    /// dimensions. This is APRON's fold operation.
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
    virtual IntervalBox toBox() const = 0;
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

    /// Align both states to the union variable schema in one API-level
    /// operation. Lattice compatibility is still checked later.
    VariableEnvironment unifyEnvironmentWith(
        NumericalState& other, bool initializeNewVariablesToZero = false);

protected:
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

private:
    mutable OperationMetadata lastOperation_;
};

} // namespace SVF::AbstractDomain

#endif // SVF_AE_NUMERICAL_DOMAIN_H
