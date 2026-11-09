//===- ConvexPolyhedraDomain.h -- Native relational numerical domain -*- C++ -*-===//
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

#ifndef SVF_AE_CONVEX_POLYHEDRA_DOMAIN_H
#define SVF_AE_CONVEX_POLYHEDRA_DOMAIN_H

#include "AE/Core/NumericalDomain.h"

#include <memory>
#include <vector>

namespace SVF
{
struct ConvexPolyhedraConfig
{
    std::shared_ptr<DiagnosticSink> diagnostics;
    bool integerTightening = true;

    bool operationCompatible(const ConvexPolyhedraConfig& other) const
    {
        return integerTightening == other.integerTightening;
    }
};

enum class PolyhedraGeneratorKind
{
    Point,
    ClosurePoint,
    Ray,
    Line
};

/// Public, layout-ordered V-representation element. Point coordinates
/// are affine values; Ray/Line coordinates are directions. A closure point is
/// available only in an NNC generator system. An empty system denotes bottom.
struct PolyhedraGenerator
{
    PolyhedraGeneratorKind kind = PolyhedraGeneratorKind::Point;
    std::vector<Rational> coordinates;
};

using PolyhedraGeneratorSet = std::vector<PolyhedraGenerator>;

/// Closed/non-closed convex polyhedra over exact GMP rationals. The internal
/// representation lazily caches normalized constraints (H) and homogeneous
/// points/rays/lines (V), converting with an exact double-description kernel.
/// Operations select the cheaper valid side without exposing H/V state to the
/// abstract interpreter; exact Fourier-Motzkin remains the NNC projection
/// fallback.
class ConvexPolyhedraDomain final : public NumericalDomain
{
public:
    using NumericalDomain::assignParallel;
    using NumericalDomain::bound;
    using NumericalDomain::substitute;
    using NumericalDomain::substituteParallel;

    static ConvexPolyhedraDomain top(const ConvexPolyhedraConfig& config = {});
    static ConvexPolyhedraDomain bottom(const ConvexPolyhedraConfig& config = {});
    static ConvexPolyhedraDomain fromConstraints(const LinearConstraintSet& constraints,
            const ConvexPolyhedraConfig& config = {});
    static ConvexPolyhedraDomain fromBox(const BoxDomain& box, const ConvexPolyhedraConfig& config = {});
    DomainKind kind() const noexcept override
    {
        return DomainKind::ConvexPolyhedra;
    }
    BoxDomain toBox() const;
    /// Existential projection: omitted variables become unrestricted.
    void project(const std::vector<Variable>& retained) override;
    /// Generator coordinates use exactly the caller-supplied variable order.
    static ConvexPolyhedraDomain fromGenerators(const std::vector<Variable>& variables,
            const PolyhedraGeneratorSet& generators, const ConvexPolyhedraConfig& config = {});
    PolyhedraGeneratorSet toGenerators(const std::vector<Variable>& variables) const;
    ConvexPolyhedraDomain(const ConvexPolyhedraDomain& other);
    ConvexPolyhedraDomain(ConvexPolyhedraDomain&& other) noexcept;
    ConvexPolyhedraDomain& operator=(const ConvexPolyhedraDomain& other);
    ConvexPolyhedraDomain& operator=(ConvexPolyhedraDomain&& other) noexcept;
    ~ConvexPolyhedraDomain() override;

    std::unique_ptr<AbstractDomain> clone() const override;
    const char* name() const;

    const ConvexPolyhedraConfig& config() const
    {
        return config_;
    }

    void assign(Variable target,
                const LinearExpression& expression) override;
    void assign(Variable target, const TreeExpression& expression) override;
    void assignParallel(const LinearAssignmentList& assignments) override;
    void substitute(Variable target,
                    const LinearExpression& expression) override;
    void substituteParallel(
        const LinearAssignmentList& assignments) override;
    void assume(const LinearConstraint& constraint) override;
    void assumeAll(const LinearConstraintSet& constraints) override;
    void assume(const TreeConstraint& constraint) override;
    void forget(Variable variable) override;
    void expand(Variable source,
                const std::vector<Variable>& copies) override;
    void fold(Variable target,
              const std::vector<Variable>& folded) override;

    CheckResult entails(const LinearConstraint& constraint) const override;
    Interval bound(Variable variable) const override;
    Interval bound(const LinearExpression& expression) const override;
    std::vector<Variable> supportVariables() const override;
    LinearConstraintSet toConstraints() const override;
    void close() override;
    void canonicalize() override;

    ConvexPolyhedraDomain join(
        const ConvexPolyhedraDomain& other) const;
    ConvexPolyhedraDomain meet(
        const ConvexPolyhedraDomain& other) const;
    ConvexPolyhedraDomain widen(
        const ConvexPolyhedraDomain& next,
        const WideningPolicy& policy = {}) const;
    ConvexPolyhedraDomain narrow(
        const ConvexPolyhedraDomain& next) const;

private:
    static ConvexPolyhedraDomain top(
        const DimensionLayout& layout,
        const ConvexPolyhedraConfig& config);
    static ConvexPolyhedraDomain bottom(
        const DimensionLayout& layout,
        const ConvexPolyhedraConfig& config);
    static ConvexPolyhedraDomain fromConstraints(
        const DimensionLayout& layout,
        const LinearConstraintSet& constraints,
        const ConvexPolyhedraConfig& config);
    static ConvexPolyhedraDomain fromGeneratorsInLayout(
        const DimensionLayout& layout,
        const PolyhedraGeneratorSet& generators,
        const ConvexPolyhedraConfig& config);

    const DimensionLayout& layout() const
    {
        return layout_;
    }
    void changeLayout(const DimensionLayout& layout);
    void ensureVariables(const std::vector<Variable>& variables);
    void ensureExpression(const LinearExpression& expression);
    void expandDimensions(Variable source, const std::vector<DimensionEntry>& copies);
    PolyhedraGeneratorSet toGenerators() const;
    class Impl;
    ConvexPolyhedraDomain(DimensionLayout layout,
                          ConvexPolyhedraConfig config, bool bottom);

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<ConvexPolyhedraDomain>();
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

    const ConvexPolyhedraDomain& requirePolyhedron(
        const AbstractDomain& other) const;
    void ensureConstraints() const;
    void ensureGenerators() const;
    void invalidateConstraints();
    void invalidateGenerators();
    void normalize();
    void report(OperationKind operation, ApproximationKind approximation,
                std::string reason, bool best = true) const;

    DimensionLayout layout_;
    ConvexPolyhedraConfig config_;
    std::unique_ptr<Impl> impl_;
};

} // namespace SVF

#endif // SVF_AE_CONVEX_POLYHEDRA_DOMAIN_H
