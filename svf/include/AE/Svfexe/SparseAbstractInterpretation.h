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

#include <memory>

#include "AE/Svfexe/AbstractInterpretation.h"
#include "MSSA/SVFGBuilder.h"

namespace SVF
{

class SVFG;
class SVFGBuilder;
class IndirectSVFGEdge;
class VFGNode;

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

protected:
    /// Full-sparse does not merge normal value-flow state along ICFG
    /// edges.  The ICFG join carries only side-channel state that is not
    /// represented as MemorySSA def-use flow: GepObjVar field snapshots
    /// and `_freedAddrs`.  Base/Dummy ObjVars are populated later by
    /// pullObjValueFlows from SVFG indirect in-edges; ValVars stay at their
    /// def-sites.
    void joinStates(AbstractState& dst, const AbstractState& src) override;

    /// After a store overwrites an ObjVar, clear any branch refinement
    /// for that ObjVar at the store's node so stale branch constraints
    /// don't propagate past the redefinition.
    void storeValue(const ValVar* pointer, const AbstractValue& val,
                    const ICFGNode* node) override;

    /// Thin wrapper: defer to base for ICFG-edge bookkeeping
    /// (predecessor iteration, branch feasibility, joinStates,
    /// updateAbsState, reachability return).  For reachable nodes,
    /// additionally run pullObjValueFlows to populate trace[node] with obj
    /// values from SVFG def-sites.
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;

    /// Capture branch narrowings into refinementTrace[succ] instead of
    /// writing them into the local `as`: in FullSparse `as` would be
    /// discarded by joinStates (no-op for ObjVar), so we route the
    /// narrowing to refinementTrace and let propagateAndApplyRefinement
    /// bake it into trace at the end of mergeStatesFromPredecessors.
    void recordBranchRefinement(NodeID objId, const IntervalValue& narrowed,
                                AbstractState& as, const ICFGNode* loadIcfg,
                                const ICFGNode* succ) override;

private:
    /// SVFG-pull helper: walk each VFG node's indirect SVFG in-edges
    /// and pull obj values from upstream def-site traces into
    /// trace[node].  Multiple sources (e.g. mphi operands) JOIN.
    void pullObjValueFlows(const ICFGNode* node);

    /// Return whether an indirect SVFG edge should be pulled into dst.
    /// Besides branch-feasible ICFG reachability, this rejects paths where
    /// another store to the same points-to object kills the edge's value.
    bool isIndirectSVFGEdgeFeasible(const IndirectSVFGEdge* edge,
                                    const VFGNode* dst);

    /// Return whether a branch-feasible ICFG path exists from src to dst.
    /// Conditional edges are checked with a pure branch-feasibility query,
    /// so path probing does not create branch-refinement side effects.
    bool isICFGPathFeasible(const ICFGNode* src, const ICFGNode* dst);

    /// Return whether this intra edge is allowed by the current branch state.
    bool isIntraEdgeBranchFeasible(const IntraCFGEdge* edge,
                                   const ICFGNode* src);

    /// Compose pred-inherited refinement into refinementTrace[node]
    /// (single-pred linear copy / multi-pred intersect-JOIN; any pred
    /// without refinement drops the inheritance), then MEET the final
    /// refinementTrace[node] into trace[node]._addrToAbsVal so the
    /// inherited base getAbsValue(ObjVar*, node) returns the narrowed
    /// value directly — no read-time override or cache.  Called once
    /// per merge as the last step.
    void propagateAndApplyRefinement(const ICFGNode* node);

    /// Path-refined obj values produced by branch narrowing.  Each
    /// entry is the *interval constraint* (not effective value) so
    /// base trace can widen/narrow independently.  Cached at branch
    /// successors by recordBranchRefinement; propagated and applied by
    /// propagateAndApplyRefinement at the end of
    /// mergeStatesFromPredecessors.
    Map<const ICFGNode*, Map<NodeID, IntervalValue>> refinementTrace;

    /// Build the SVFG on top of the semi-sparse precompute.
    void buildSVFG();

    /// Owns the SVFG (via SVFGBuilder's internal unique_ptr).  Without
    /// this, SVFGBuilder would be a local in buildSVFG() and free the
    /// graph at scope exit, leaving `svfg` dangling.
    std::unique_ptr<SVFGBuilder> svfgBuilder;
    /// View pointer into svfgBuilder's graph; non-null after buildSVFG().
    SVFG* svfg{nullptr};
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
