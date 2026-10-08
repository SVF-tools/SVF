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
/// In full-sparse mode ValVars remain at their SSA def-sites. ObjVars are
/// projected from an ICFG-node state into incoming Obj values, then joined
/// into the destination ICFG-node state before its transfer executes. A
/// transfer carries one base object and any of its precise GepObjVar
/// sub-objects currently known to AE.
///
/// Running example:
///   LLVM IR:  store i32 10, ptr %a9
///   SVFG:     StoreVFGNode --indirect {a}--> LoadVFGNode
///   AE:       State[store][a.9]=10 --{a.9:10}--> State[load]
class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation()
    {
        buildSVFG();
    }
    ~FullSparseAbstractInterpretation() override;

protected:
    /// Do not copy ObjVars along ordinary ICFG edges. For `store a[9] = 10`,
    /// the value reaches a later load through SVFG propagation rather than
    /// by copying the whole store-node state through every intermediate node.
    /// `_freedAddrs` is still copied because it has no SVFG representation.
    void joinStates(AbstractState& dst, const AbstractState& src) override;

    /// Write an ObjVar into State[node] and discard any stale branch constraint
    /// for that object. A precise GepObjVar write also propagates its values to
    /// the current SVFG use sites of its base object. This supplements cases
    /// such
    /// as a dynamic `a[i][j]` load whose SVFG edge names only one
    /// representative field of `a`.
    void updateAbsValue(const ObjVar* var, const AbstractValue& val,
                        const ICFGNode* node) override;
    using SemiSparseAbstractInterpretation::updateAbsValue;

    /// Apply FullSparse store semantics when handling an ordinary StoreStmt.
    /// A strong update kills the previous value of one concrete object:
    ///
    ///   int x = 1;
    ///   int *p = &x;        // Andersen pts(p) = {x}
    ///   *p = 2;             // State[x] becomes [2,2]
    ///
    /// A weak update preserves every possible old value and joins the stored
    /// value. It is required when the pointer has multiple targets or denotes
    /// a summary object such as a heap allocation or array:
    ///
    ///   int x = 1, y = 3;
    ///   int *p = cond ? &x : &y;  // pts(p) = {x,y}
    ///   *p = 2;                   // x: [1,2], y: [2,3]
    ///
    /// The distinction is local to FullSparse; dense and semi-sparse retain
    /// their existing store implementation.
    void storeValue(const ValVar* pointer, const AbstractValue& val,
                    const ICFGNode* node) override;

    /// Prepare State[node] before its transfer. ICFG predecessors first carry
    /// control-flow side state; incoming Obj values then add ObjVars. For
    /// `%x = load i32, ptr %a9`, the load state receives `{a.9: 10}` before
    /// evaluating `%x`.
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;

    /// Scan outgoing indirect SVFG edges after node executes. This method
    /// chooses each target and base object, then delegates the target update
    /// to writeObjValuesToTarget. Precise GepObjVar writes use the same
    /// operation for supplementary base-object routes.
    void propagateOutgoingObjValues(const ICFGNode* node) override;

    /// Republish the widened loop-head state. For `a[i] = i` in a loop, the
    /// WTO state remains the fixpoint authority; propagation only transports
    /// its latest base and sub-object values to body uses.
    bool widenCycleState(const AbstractState& prev,
                         const AbstractState& cur,
                         const ICFGCycleWTO* cycle) override;

    /// Republish a narrowed loop-head state when narrowing changes it. This
    /// lets later body loads observe the narrowed `a[i]` values without making
    /// propagated-value changes a second fixpoint condition.
    bool narrowCycleState(const AbstractState& prev,
                          const AbstractState& cur,
                          const ICFGCycleWTO* cycle) override;

    /// Capture branch narrowings into refinementTrace[succ]. For
    /// `if (a[9] < 10)`, incoming Obj values first supply the value of a.9;
    /// the conditional ICFG edge then records `a.9 < 10` for the true
    /// successor. It cannot be written only to the temporary predecessor state
    /// because FullSparse does not copy ObjVars along ordinary ICFG edges.
    void recordBranchRefinement(NodeID objId, const IntervalValue& narrowed,
                                AbstractState& as, const ICFGNode* loadIcfg,
                                const ICFGNode* succ) override;

private:
    /// Decide whether `*pointer = value` is a strong update. This uses the same
    /// policy as FlowSensitive::isStrongUpdate: Andersen must report exactly
    /// one target, and that target must not be a heap object, array object,
    /// field-insensitive base, or local object in a recursive function.
    ///
    ///   int x; int *p = &x; *p = 1;       // strong update
    ///   int *p = malloc(sizeof(int));      // heap summary
    ///   *p = 1;                            // weak update
    ///   int a[4]; a[i] = 1;                // weak update: array summary
    ///   int *q = cond ? &x : &y; *q = 1;   // weak update: two targets
    ///
    /// On success, targetObj receives the unique object killed by the store.
    bool isStrongUpdate(const ValVar* pointer, NodeID& targetObj) const;

    /// Match a MemorySSA version to the ICFG node that produced it. This is
    /// used only while resolving operands of an IntraMSSAPHISVFGNode:
    ///
    ///   S1: *p = 1;             // defines MR_p.v1 through StoreCHI
    ///   S2: ext_write(p);       // may define MR_p.v2 through CallCHI
    ///   J:  MemoryPhi(v1, v2)
    ///
    /// Definitions may come from a store, call, function entry, or an earlier
    /// MemoryPhi. The check lets AE associate v1 with S1 and v2 with S2 without
    /// adding an MRVer lookup API to SVFG.
    bool isMemoryVersionDefinedAt(const MRVer* version,
                                  const ICFGNode* source) const;

    /// Test whether an incoming object value is carried by a reachable operand
    /// of a MemorySSA Phi. For example:
    ///
    ///   int x = 0;
    ///   if (cond) x = 1; else x = 2;
    ///   use(x);                 // MemoryPhi(x_then, x_else)
    ///
    /// If both branches are reachable, both versions are retained and JOINed.
    /// If `cond` is true and the else predecessor has no abstract state, x_else
    /// is omitted. When no matching IntraMSSAPHISVFGNode exists, this function
    /// returns true so non-Phi value flow remains conservative.
    bool isMemoryPhiOperandReachable(const ICFGNode* source,
                                     const ICFGNode* target,
                                     NodeID baseObj) const;

    /// Obj values transported between two ICFG-node states. A base-only map
    /// is `{a: value}`; a map with sub-objects may be
    /// `{a: value, a.3: value, a.9: value}`.
    using ObjValueMap = Map<NodeID, AbstractValue>;

    /// Identifies replaceable Obj values by their producer and base object.
    /// Re-executing the same loop node replaces `{source, a}` instead of
    /// appending stale values; values from different sources still JOIN.
    using ObjValueOrigin = std::pair<const ICFGNode*, NodeID>;

    /// Join all incoming Obj values addressed to node into State[node]. For
    /// two incoming SVFG edges:
    ///
    ///   Store1(a.9 = 10) --indirect {a}--+
    ///                                      +--> Load(a.9)
    ///   Store2(a.9 = 20) --indirect {a}--+
    ///
    /// the incoming maps contain `{a.9:[10,10]}` from Store1 and
    /// `{a.9:[20,20]}` from Store2. Before the load executes, this method sets
    /// `State[load][a.9]` to their JOIN, `[10,20]`. This is a conservative JOIN
    /// across different reaching definitions, not another store update. An
    /// incoming MemorySSA-Phi operand is omitted only when its corresponding
    /// CFG predecessor is unreachable; all remaining versions are retained.
    void mergeIncomingObjValues(const ICFGNode* node);

    /// Collect one base object and its sub-objects from State[source]. For
    /// `a[9] = 10`, a source state containing `a.9`
    /// produces `{a.9:10}`. Missing objects are omitted rather than inserted
    /// as TOP. Base/sub-object membership is queried directly
    /// from SVFIR rather than cached by AE.
    ObjValueMap collectBaseObjValues(const ICFGNode* source,
                                     NodeID baseObj);

    /// Write or replace incoming Obj values after their route has selected a
    /// source, target, and base object. Both native SVFG edges and supplementary
    /// base-object use-site routes use this operation:
    ///   State[source] --collect(base)--> incoming[target][source, base].
    void writeObjValuesToTarget(const ICFGNode* source,
                                const ICFGNode* target,
                                NodeID baseObj);

    /// Find SVFG use sites of baseObj without keeping a persistent index.
    /// For:
    ///
    ///   Store S: a[2][2] = 8     // writes sub-object a.8, base a
    ///   Load  L: x = a[i][j]     // an incoming edge may name a.0, base a
    ///
    /// the native SVFG may have no S-to-L edge. This method scans each target's
    /// incoming indirect edges, normalizes their object labels to a base, and
    /// finds that L also uses base a. It then propagates S's `{a.8:[8,8]}`
    /// to L. External calls without an indirect edge are matched in the same
    /// way through the points-to sets of their pointer arguments.
    void propagateBaseObjValuesToUses(const ICFGNode* source,
                                      NodeID baseObj);

    /// Propagate and apply branch constraints after incoming Obj values merge.
    /// For
    /// `if (a[9] < 10) { use1(a[9]); use2(a[9]); }`, use1 receives the edge
    /// constraint and use2 inherits it. Multi-predecessor constraints JOIN;
    /// the result is then MEETed into State[node] so normal object reads see it.
    void propagateAndApplyRefinement(const ICFGNode* node);

    /// Path-refined obj values produced by branch narrowing.  Each
    /// entry is the *interval constraint* (not effective value) so
    /// base trace can widen/narrow independently.  Cached at branch
    /// successors by recordBranchRefinement; propagated and applied by
    /// propagateAndApplyRefinement at the end of
    /// mergeStatesFromPredecessors.
    Map<const ICFGNode*, Map<NodeID, IntervalValue>> refinementTrace;

    /// Incoming FullSparse Obj values, grouped by target, source, and base Obj.
    /// Some entries contain only a base Obj, while others also contain
    /// sub-objects. The authoritative states remain in abstractTrace.
    Map<const ICFGNode*, Map<ObjValueOrigin, ObjValueMap>> incomingObjValues;

    /// Build the SVFG used by FullSparse value propagation.
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
