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

class SVFG;

/// Abstract Interpretation for `Options::AESparsity::SemiSparse`.
///
/// ValVars live at their SVFG-style def-sites: reads pull from there,
/// writes go there, state merges replace only the ObjVar map and skip
/// the ValVar map, and the cycle helpers gather/scatter cycle ValVars
/// around each widening iteration.
class SemiSparseAbstractInterpretation : public AbstractInterpretation
{
public:
    SemiSparseAbstractInterpretation()
    {
        preAnalysis->initCycleValVars();
    }
    ~SemiSparseAbstractInterpretation() override = default;

protected:
    AbstractState getFullCycleHeadState(const ICFGCycleWTO* cycle) override;

    bool widenCycleState(const AbstractState& prev,
                         const AbstractState& cur,
                         const ICFGCycleWTO* cycle) override;

    bool narrowCycleState(const AbstractState& prev,
                          const AbstractState& cur,
                          const ICFGCycleWTO* cycle) override;

    const AbstractValue& getAbsValue(const ValVar* var, const ICFGNode* node) override;
    using AbstractInterpretation::getAbsValue;

    bool hasAbsValue(const ValVar* var, const ICFGNode* node) const override;
    using AbstractInterpretation::hasAbsValue;

    void updateAbsValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node) override;
    using AbstractInterpretation::updateAbsValue;

    void updateAbsState(const ICFGNode* node, const AbstractState& state) override;

    void joinStates(AbstractState& dst, const AbstractState& src) override;

    const ICFGNode* getICFGNode(const ValVar* var) const;
};

/// Abstract Interpretation for `Options::AESparsity::Sparse` (full-sparse).
///
/// In full-sparse mode both ValVars and ObjVars live at their SVFG
/// def-sites; reads query the SVFG for the reaching-def site, writes
/// happen at def-sites.  ValVar reads currently assert(false) until the
/// SVFG-backed resolution lands (see `doc/plan-full-sparse.md`); cycle
/// helpers are inherited from the semi-sparse parent and will also need
/// extension for ObjVars.
class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation()
    {
        buildSVFG();
    }
    ~FullSparseAbstractInterpretation() override;

    // Full-sparse ValVar resolution will route through the SVFG once
    // implemented; fail loudly until then rather than silently inherit
    // semi-sparse semantics.
    const AbstractValue& getAbsValue(const ValVar* var, const ICFGNode* node) override;
    using SemiSparseAbstractInterpretation::getAbsValue;

    bool hasAbsValue(const ValVar* var, const ICFGNode* node) const override;
    using SemiSparseAbstractInterpretation::hasAbsValue;

    Set<const ICFGNode*> getUseSitesOfObjVar(const ObjVar* obj, const ICFGNode* node) const override;
    Set<const ICFGNode*> getUseSitesOfValVar(const ValVar* var) const override;
    const ICFGNode* getDefSiteOfValVar(const ValVar* var) const override;
    const ICFGNode* getDefSiteOfObjVar(const ObjVar* obj, const ICFGNode* node) const override;

protected:
    /// Build the SVFG on top of the semi-sparse precompute.
    void buildSVFG();

    SVFG* svfg{nullptr};
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
