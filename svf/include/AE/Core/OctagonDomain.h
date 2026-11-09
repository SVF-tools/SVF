//===- OctagonDomain.h -- Native relational numerical domain -*- C++ -*-===//
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

#ifndef SVF_AE_OCTAGON_DOMAIN_H
#define SVF_AE_OCTAGON_DOMAIN_H

#include "AE/Core/NumericalDomain.h"

#include <memory>
#include <string>

namespace SVF
{
struct OctagonStorageStats
{
    OctagonStorageKind kind = OctagonStorageKind::DenseHalf;
    std::size_t dimensions = 0;
    std::size_t finiteStoredSlots = 0;
    std::size_t allocatedBoundSlots = 0;
    std::size_t components = 0;
    std::size_t maximumComponent = 0;
};

/// One Octagon abstract element for constraints +/-x +/-y <= c.
///
/// A property over all typed Variables. A private dimension layout maps its
/// finite active support to matrix coordinates; callers never align layouts.
/// Copies have value semantics; component matrices may share COW storage.
class OctagonDomain final : public NumericalDomain
{
public:
    using NumericalDomain::assignParallel;
    using NumericalDomain::bound;
    using NumericalDomain::substitute;
    using NumericalDomain::substituteParallel;

    static OctagonDomain top(const OctagonConfig& config = {});
    static OctagonDomain bottom(const OctagonConfig& config = {});
    static OctagonDomain fromConstraints(const LinearConstraintSet& constraints,
                                         const OctagonConfig& config = {});
    static OctagonDomain fromBox(const BoxDomain& box, const OctagonConfig& config = {});
    DomainKind kind() const noexcept override
    {
        return DomainKind::Octagon;
    }
    BoxDomain toBox() const;
    /// Existential projection: omitted variables become unrestricted.
    void project(const std::vector<Variable>& retained) override;
    OctagonDomain(const OctagonDomain& other);
    OctagonDomain(OctagonDomain&& other) noexcept;
    OctagonDomain& operator=(const OctagonDomain& other);
    OctagonDomain& operator=(OctagonDomain&& other) noexcept;
    ~OctagonDomain() override;

    std::unique_ptr<AbstractDomain> clone() const override;
    const char* name() const;

    void assign(Variable target,
                const LinearExpression& expression) override;
    void assign(Variable target, const TreeExpression& expression) override;
    void assignParallel(const LinearAssignmentList& assignments) override;
    void substitute(Variable target,
                    const LinearExpression& expression) override;
    void substituteParallel(
        const LinearAssignmentList& assignments) override;
    void assume(const LinearConstraint& constraint) override;
    void assume(const TreeConstraint& constraint) override;
    void assumeAll(const LinearConstraintSet& constraints) override;
    void forget(Variable variable) override;
    void assignInterval(Variable target, const Interval& value) override;
    void projectLowerBounds();
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

    const OctagonConfig& config() const;
    OctagonStorageStats storageStats() const;

    /// Explicitly converts the operation policy while retaining the represented
    /// concrete set. Enabling stronger normalization may improve precision;
    /// disabling it keeps existing facts but changes subsequent scheduling.
    OctagonDomain reconfigured(const OctagonConfig& config) const;

    OctagonDomain join(const OctagonDomain& other) const;
    OctagonDomain meet(const OctagonDomain& other) const;
    OctagonDomain widen(
        const OctagonDomain& next,
        const WideningPolicy& policy = {}) const;
    OctagonDomain narrow(const OctagonDomain& next) const;
    OctagonDomain projectedLowerBounds() const;

private:
    std::vector<Variable> relationalClosureState(
        const std::vector<Variable>& seeds) const override;
    static OctagonDomain top(const DimensionLayout& layout,
                             const OctagonConfig& config);
    static OctagonDomain bottom(const DimensionLayout& layout,
                                const OctagonConfig& config);
    static OctagonDomain fromConstraints(
        const DimensionLayout& layout,
        const LinearConstraintSet& constraints,
        const OctagonConfig& config);

    const DimensionLayout& layout() const
    {
        return layout_;
    }
    void changeLayout(const DimensionLayout& layout);
    void ensureVariables(const std::vector<Variable>& variables);
    void ensureExpression(const LinearExpression& expression);
    void expandDimensions(Variable source, const std::vector<DimensionEntry>& copies);
    class Impl;

    OctagonDomain(DimensionLayout layout, OctagonConfig config, bool bottom);
    OctagonDomain(DimensionLayout layout, std::unique_ptr<Impl> impl);

    const void* dynamicTypeToken() const noexcept override
    {
        return staticTypeToken<OctagonDomain>();
    }
    DiagnosticSink* diagnosticSink() const;
    void report(OperationKind operation, ApproximationKind approximation,
                std::string reason, bool best = true) const;
    bool hasCompatibleDomain(const AbstractDomain& other) const override;
    ApproximationKind assignState(
        Variable target, const LinearExpression& expression);
    ApproximationKind assumeState(
        const LinearConstraint& constraint);
    void forgetState(Variable variable);
    void joinDomain(const AbstractDomain& other) override;
    void meetDomain(const AbstractDomain& other) override;
    void widenDomain(const AbstractDomain& next) override;
    void narrowDomain(const AbstractDomain& next) override;
    void projectLowerBoundsState();
    void changeLayoutState(const DimensionLayout& oldLayout,
                           const DimensionLayout& newLayout);
    bool isBottomDomain() const override;
    bool isTopDomain() const override;
    bool leqDomain(const AbstractDomain& other) const override;
    Interval boundState(Variable variable) const;
    LinearConstraintSet constraintsState() const;
    std::string domainToString() const override;

    const OctagonDomain& requireOctagon(const AbstractDomain& other) const;

    DimensionLayout layout_;
    std::unique_ptr<Impl> impl_;
};

} // namespace SVF

#endif // SVF_AE_OCTAGON_DOMAIN_H
