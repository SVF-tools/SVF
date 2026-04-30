//===- SparseAbstractInterpretation.h -- Sparse Abstract Execution------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===---------------------------------------------------------------------===//

#ifndef INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_
#define INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_

#include "AE/Svfexe/AbstractInterpretation.h"

namespace SVF
{

/// Abstract Interpretation driver for sparse modes (semi-sparse and
/// full-sparse).  Concrete subclasses are colocated in this header.
///
/// ValVars whose def-site is inside a cycle but NOT at cycle_head do not
/// flow through cycle_head's merge in semi-sparse mode, so the around-merge
/// widening cannot observe them.  This base overrides the three cycle
/// helpers in AbstractInterpretation:
///
///   * getFullCycleHeadState   — in addition to the base's trace read,
///                               pulls cycle ValVars from their def-sites
///                               into the transient AbstractState.
///   * widenCycleState         — after the base widens and writes trace,
///                               scatters the widened ValVars back to their
///                               def-sites so body nodes observe them.
///   * narrowCycleState        — same scatter on non-fixpoint narrowing.
class SparseAbstractInterpretation : public AbstractInterpretation
{
public:
    SparseAbstractInterpretation() = default;
    ~SparseAbstractInterpretation() override = default;

protected:
    bool needsCycleValVars() const override
    {
        return true;
    }

    AbstractState getFullCycleHeadState(const ICFGCycleWTO* cycle) override;

    bool widenCycleState(const AbstractState& prev,
                         const AbstractState& cur,
                         const ICFGCycleWTO* cycle) override;

    bool narrowCycleState(const AbstractState& prev,
                          const AbstractState& cur,
                          const ICFGCycleWTO* cycle) override;
};

/// AbstractStateManager for semi-sparse mode.
///
/// ValVars live at their def-sites: reads pull from def-site, writes go
/// to def-site, and `updateAbstractState` only replaces the ObjVar map
/// (the ValVar map at non-def nodes is intentionally empty).
class SemiSparseAbstractStateManager : public AbstractStateManager
{
public:
    SemiSparseAbstractStateManager(SVFIR* svfir, AndersenWaveDiff* pta);
    ~SemiSparseAbstractStateManager() override = default;

    const AbstractValue& getAbstractValue(const ValVar* var, const ICFGNode* node) override;
    using AbstractStateManager::getAbstractValue;

    bool hasAbstractValue(const ValVar* var, const ICFGNode* node) const override;
    using AbstractStateManager::hasAbstractValue;

    void updateAbstractValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node) override;
    using AbstractStateManager::updateAbstractValue;

    void updateAbstractState(const ICFGNode* node, const AbstractState& state) override;

    void joinStates(AbstractState& dst, const AbstractState& src) override;
};

/// AbstractStateManager for full-sparse mode.
///
/// Inherits semi-sparse ValVar handling and adds an SVFG: def/use site
/// queries are routed through it.  ObjVar def-site reads/writes are still
/// TODO (see `doc/plan-full-sparse.md`).
class FullSparseAbstractStateManager : public SemiSparseAbstractStateManager
{
public:
    FullSparseAbstractStateManager(SVFIR* svfir, AndersenWaveDiff* pta);
    ~FullSparseAbstractStateManager() override = default;

    // Full-sparse ValVar resolution will route through the SVFG once
    // implemented (see doc/plan-full-sparse.md).  Until then, fail loudly
    // rather than silently inherit semi-sparse semantics.
    const AbstractValue& getAbstractValue(const ValVar* var, const ICFGNode* node) override;
    using SemiSparseAbstractStateManager::getAbstractValue;

    bool hasAbstractValue(const ValVar* var, const ICFGNode* node) const override;
    using SemiSparseAbstractStateManager::hasAbstractValue;

    Set<const ICFGNode*> getUseSitesOfObjVar(const ObjVar* obj, const ICFGNode* node) const override;
    Set<const ICFGNode*> getUseSitesOfValVar(const ValVar* var) const override;
    const ICFGNode* getDefSiteOfValVar(const ValVar* var) const override;
    const ICFGNode* getDefSiteOfObjVar(const ObjVar* obj, const ICFGNode* node) const override;
};

/// Abstract Interpretation for `Options::AESparsity::SemiSparse`.
/// Inherits the sparse-shaped cycle helpers as-is from the base; exists
/// as the concrete type instantiated by the factory for SemiSparse.
class SemiSparseAbstractInterpretation : public SparseAbstractInterpretation
{
public:
    SemiSparseAbstractInterpretation() = default;
    ~SemiSparseAbstractInterpretation() override = default;

protected:
    AbstractStateManager* createStateMgr(SVFIR* svfir, AndersenWaveDiff* pta) override;
};

/// Abstract Interpretation for `Options::AESparsity::Sparse` (full-sparse).
///
/// In full-sparse mode both ValVars and ObjVars live at their SVFG
/// def-sites; reads query the SVFG for the reaching-def site, writes
/// happen at def-sites.  This is a TODO: the planned implementation
/// extends the cycle helpers to also pull/scatter ObjVars and routes
/// obj reads through SVFG (see `doc/plan-full-sparse.md`).  For now
/// this class is a compile-only stub that inherits semi-sparse
/// behaviour; it will not produce correct full-sparse results until
/// the overrides land.
class FullSparseAbstractInterpretation : public SparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation() = default;
    ~FullSparseAbstractInterpretation() override = default;

    // TODO(full-sparse): override getFullCycleHeadState / widenCycleState /
    //                    narrowCycleState to also handle ObjVars via SVFG
    //                    def-site queries (see doc/plan-full-sparse.md).

protected:
    AbstractStateManager* createStateMgr(SVFIR* svfir, AndersenWaveDiff* pta) override;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
