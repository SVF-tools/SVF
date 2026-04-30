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
/// In addition to the cycle helpers, this class hosts the semi-sparse
/// state-access overrides (ValVar reads/writes go to def-sites; state
/// merges skip ValVars).  FullSparseAbstractInterpretation inherits
/// those and adds SVFG-backed def/use queries on top.
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

    // ---- Semi-sparse state-access overrides ---------------------------
    // ValVars live at their def-sites; reads pull from there, writes go
    // there, and updateAbstractState only replaces the ObjVar map.
    const AbstractValue& getAbstractValue(const ValVar* var, const ICFGNode* node) override;
    using AbstractInterpretation::getAbstractValue;

    bool hasAbstractValue(const ValVar* var, const ICFGNode* node) const override;
    using AbstractInterpretation::hasAbstractValue;

    void updateAbstractValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node) override;
    using AbstractInterpretation::updateAbstractValue;

    void updateAbstractState(const ICFGNode* node, const AbstractState& state) override;

    void joinStates(AbstractState& dst, const AbstractState& src) override;
};

/// Abstract Interpretation for `Options::AESparsity::SemiSparse`.
/// Concrete leaf — inherits the sparse-shaped cycle helpers and the
/// semi-sparse state-access overrides from SparseAbstractInterpretation
/// without modification.
class SemiSparseAbstractInterpretation : public SparseAbstractInterpretation
{
public:
    SemiSparseAbstractInterpretation() = default;
    ~SemiSparseAbstractInterpretation() override = default;
};

/// Abstract Interpretation for `Options::AESparsity::Sparse` (full-sparse).
///
/// In full-sparse mode both ValVars and ObjVars live at their SVFG
/// def-sites; reads query the SVFG for the reaching-def site, writes
/// happen at def-sites.  ValVar reads currently assert(false) until the
/// SVFG-backed resolution lands (see `doc/plan-full-sparse.md`); cycle
/// helpers are inherited from the semi-sparse parent and will also need
/// extension for ObjVars.
class FullSparseAbstractInterpretation : public SparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation() = default;
    ~FullSparseAbstractInterpretation() override = default;

    // Full-sparse ValVar resolution will route through the SVFG once
    // implemented; fail loudly until then rather than silently inherit
    // semi-sparse semantics.
    const AbstractValue& getAbstractValue(const ValVar* var, const ICFGNode* node) override;
    using SparseAbstractInterpretation::getAbstractValue;

    bool hasAbstractValue(const ValVar* var, const ICFGNode* node) const override;
    using SparseAbstractInterpretation::hasAbstractValue;

    Set<const ICFGNode*> getUseSitesOfObjVar(const ObjVar* obj, const ICFGNode* node) const override;
    Set<const ICFGNode*> getUseSitesOfValVar(const ValVar* var) const override;
    const ICFGNode* getDefSiteOfValVar(const ValVar* var) const override;
    const ICFGNode* getDefSiteOfObjVar(const ObjVar* obj, const ICFGNode* node) const override;

protected:
    /// Build the SVFG once PTA is available (runOnModule timing).
    void initAuxState(AndersenWaveDiff* pta) override;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
