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
#include <memory>

namespace SVF
{

class SVFG;
class SVFGBuilder;

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
/// happen at def-sites.  See `doc/plan-full-sparse.md` for the
/// phase plan; Phase 1 routes ValVar and ObjVar reads through the SVFG.
class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation()
    {
        buildSVFG();
    }
    ~FullSparseAbstractInterpretation() override;

    const Set<const ICFGNode*> getUseSitesOfValVar(const ValVar* var) const;
    const ICFGNode* getDefSiteOfValVar(const ValVar* var) const;
    /// Given an ObjVar and its def-site ICFGNode, find all use-site ICFGNodes
    /// by following outgoing IndirectSVFGEdges whose pts contains the ObjVar
    const Set<const ICFGNode*> getDefSiteOfObjVar(const ObjVar* obj, const ICFGNode* node) const;
    /// Given an ObjVar and its def-site ICFGNode, find all use-site ICFGNodes
    /// by following outgoing IndirectSVFGEdges whose pts contains the ObjVar
    const Set<const ICFGNode*> getUseSitesOfObjVar(const ObjVar* obj, const ICFGNode* node) const;

protected:
    /// Value flow does not propagate along ICFG edges in full-sparse;
    /// both ValVar and ObjVar are pulled in pullValueFlow via SVFG
    /// indirect in-edges.  Only `_freedAddrs` (no SVFG encoding) rides
    /// ICFG edges, matching semi-sparse minus the obj loop.
    void joinStates(AbstractState& dst, const AbstractState& src) override;

    /// Thin wrapper: defer to base for ICFG-edge bookkeeping
    /// (predecessor iteration, branch feasibility, joinStates,
    /// updateAbsState, reachability return).  When base reports a
    /// feasible predecessor, additionally run pullValueFlow to populate
    /// trace[node] with obj values from SVFG def-sites.
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;

private:
    /// SVFG-pull helper: walk each VFG node's indirect SVFG in-edges
    /// and pull obj values from upstream def-site traces into
    /// trace[node].  Multiple sources (e.g. mphi operands) JOIN.
    void pullValueFlow(const ICFGNode* node);

    /// Build the SVFG on top of the semi-sparse precompute.
    void buildSVFG();

    /// Owns the SVFG (via SVFGBuilder's internal unique_ptr).  Without
    /// this, SVFGBuilder would be a local in buildSVFG() and free the
    /// graph at scope exit, leaving `svfg` dangling.
    std::unique_ptr<SVFGBuilder> svfgBuilder;
    /// View pointer into svfgBuilder's graph; non-null after buildSVFG().
    SVFG* svfg{nullptr};

    /// Flow-insensitive overlay for GepObjVar abstract values.
    /// Keyed by GepObjVar NodeID (encodes (base, offset) uniquely via
    /// SVFIR::getGepObjVar).  Trade-off: weak update / flow-insensitive.
    Map<NodeID, AbstractValue> gepOverlay;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
