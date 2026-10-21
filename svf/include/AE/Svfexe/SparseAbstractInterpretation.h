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
/// ValVars remain at their SSA def-sites. Obj values travel along SVFG edges
/// between states keyed by ICFGNode. An edge naming either a base or a sub-Obj
/// transports the source state's available values for that base and its sub-Objs.
/// The existing ICFG transport of GEP values is retained for AE sub-Objs that
/// are not distinguished by the pre-analysis graph.
class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation()
    {
        buildSVFG();
    }
    ~FullSparseAbstractInterpretation() override;

protected:
    /// Retain the existing ICFG transport of GEP values and freed addresses.
    /// Base Obj values arrive through SVFG edges; ValVars stay at def-sites.
    void joinStates(AbstractState& dst, const AbstractState& src) override;

    /// After `if (a[0] > 0) memcpy(a, zeros, 1)`, the old a[0] > 0
    /// constraint no longer applies. Ordinary stores and external summaries
    /// both use this existing Obj-value write entry point.
    void updateAbsValue(const ObjVar* var, const AbstractValue& val,
                        const ICFGNode* node) override;
    using SemiSparseAbstractInterpretation::updateAbsValue;

    /// Rebuild control-flow state, merge propagated Obj values, then refine.
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;

    /// LLVM: store i32 10, ptr %a2; ...; %v = load i32, ptr %a2.
    /// SVFG: Store --indirect {a or a.2}--> Load.
    /// State[store]'s a and available sub-Objs become incoming values for Load.
    void propagateOutgoingObjValues(const ICFGNode* node) override;

    /// Republish the WTO's final widened/narrowed state, not the pre-widen value.
    bool widenCycleState(const AbstractState& prev, const AbstractState& cur,
                         const ICFGCycleWTO* cycle) override;
    bool narrowCycleState(const AbstractState& prev, const AbstractState& cur,
                          const ICFGCycleWTO* cycle) override;

    /// On `if (x > 0)`, record x >= 1 for the successor separately from
    /// incoming Obj values. Apply it after the SVFG JOIN so that merging
    /// values cannot erase the branch constraint.
    void recordBranchRefinement(NodeID objId, const IntervalValue& narrowed,
                                AbstractState& as, const ICFGNode* loadIcfg,
                                const ICFGNode* succ) override;

private:
    using ObjValues = Map<NodeID, AbstractValue>;
    using ObjOrigin = std::pair<const ICFGNode*, NodeID>;

    /// Example: a[2]=10. State[source] may hold {a.0:1, a.1:2, a.2:10}.
    /// Query SVFIR membership and collect existing values only; do not create TOP.
    ObjValues collectBaseObjValues(const ICFGNode* source, NodeID baseObj);

    /// Example: if (c) x=10; else x=20; use(x).
    /// SVFG: Store1 --{x}--+--> Phi receives x=10 and x=20.
    ///       Store2 --{x}--+    State[Phi][x] becomes [10,20].
    /// Keep the existing feasibility filter and authoritative ICFG GEP values;
    /// JOIN the other inputs before branch refinement. This is not a store.
    void mergeIncomingObjValues(const ICFGNode* node);

    /// Target preparation rebuilds its state, so retain incoming values here.
    /// A revisited source/base replaces its old input; different sources JOIN.
    Map<const ICFGNode*, Map<ObjOrigin, ObjValues>> propagatedObjValues;

    /// Return whether an indirect SVFG edge should contribute to dst.
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
