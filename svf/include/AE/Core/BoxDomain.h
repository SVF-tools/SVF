//===- BoxDomain.h -- Exact-rational interval box state --------*- C++ -*-===//

#ifndef SVF_AE_BOX_DOMAIN_H
#define SVF_AE_BOX_DOMAIN_H

#include "AE/Core/AbstractState.h"
#include "AE/Core/NumericalDomain.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace SVF::AbstractDomain
{

struct BoxConfig
{
    bool integerTightening = true;
    std::shared_ptr<DiagnosticSink> diagnostics;

    bool operationCompatible(const BoxConfig& other) const
    {
        return integerTightening == other.integerTightening;
    }
};

/// Non-relational numerical state with one exact-rational interval per
/// environment dimension.
class BoxState final : public NumericalState
{
public:
    using NumericalState::assignParallel;
    using NumericalState::bound;
    using NumericalState::substitute;
    using NumericalState::substituteParallel;

    static BoxState top(const VariableEnvironment& environment,
                        const BoxConfig& config = {});
    static BoxState bottom(const VariableEnvironment& environment,
                           const BoxConfig& config = {});
    static BoxState fromBox(const VariableEnvironment& environment,
                            const IntervalBox& box,
                            const BoxConfig& config = {});
    static BoxState fromConstraints(const VariableEnvironment& environment,
                                    const LinearConstraintSet& constraints,
                                    const BoxConfig& config = {});

    BoxState(const BoxState& other);
    BoxState(BoxState&& other) noexcept = default;
    BoxState& operator=(const BoxState& other) = default;
    BoxState& operator=(BoxState&& other) noexcept = default;

    std::unique_ptr<AbstractState> clone() const override;
    const char* name() const override;
    DomainCapabilities capabilities() const override;

    const VariableEnvironment& environment() const override
    {
        return environment_;
    }
    const BoxConfig& config() const
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
    void forget(Variable variable) override;
    void changeEnvironment(const VariableEnvironment& environment,
                           bool initializeNewVariablesToZero = false) override;
    void expand(Variable source,
                const std::vector<VariableDeclaration>& copies) override;
    void fold(Variable target, const std::vector<Variable>& folded) override;

    CheckResult entails(const LinearConstraint& constraint) const override;
    Interval bound(Variable variable) const override;
    Interval bound(const LinearExpression& expression) const override;
    IntervalBox toBox() const override;
    LinearConstraintSet toConstraints() const override;
    void close() override;
    void canonicalize() override;

    BoxState join(const BoxState& other) const;
    BoxState meet(const BoxState& other) const;
    BoxState widen(const BoxState& next,
                   const WideningPolicy& policy = {}) const;
    BoxState narrow(const BoxState& next) const;

private:
    static constexpr std::size_t BoundsPerPage = 64;

    struct BoundPage
    {
        std::array<std::optional<Interval>, BoundsPerPage> bounds;
    };

    struct BoundPageEntry
    {
        std::size_t index;
        std::shared_ptr<BoundPage> page;
    };

    using BoundPageDirectory = std::vector<BoundPageEntry>;
    BoxState(VariableEnvironment environment, BoxConfig config, bool bottom);

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<BoxState>();
    }
    bool hasCompatibleDomain(const AbstractState& other) const override;
    void joinState(const AbstractState& other) override;
    void meetState(const AbstractState& other) override;
    void widenState(const AbstractState& next) override;
    void narrowState(const AbstractState& next) override;
    bool isBottomState() const override;
    bool isTopState() const override;
    bool leqState(const AbstractState& other) const override;
    std::string stateToString() const override;

    const BoxState& requireBox(const AbstractState& other) const;
    const Interval& boundAt(Dimension dimension) const;
    BoundPage& writablePage(std::size_t pageIndex);
    void eraseBound(Dimension dimension);
    static bool pageIsEmpty(const BoundPage& page);
    std::vector<Dimension> boundedDimensions() const;
    void makeBottom();
    void canonicalize(Dimension dimension);
    void setBound(Dimension dimension, Interval interval);
    void report(OperationKind operation, ApproximationKind approximation,
                std::string reason, bool best = true) const;
    VariableEnvironment environment_;
    BoxConfig config_;
    /// Missing pages and empty slots denote top. Active pages are kept sorted,
    /// shared by state copies, and detached only when one of their bounds
    /// changes.
    BoundPageDirectory boundPages_;
    bool bottom_ = false;
};

} // namespace SVF::AbstractDomain

#endif // SVF_AE_BOX_DOMAIN_H
