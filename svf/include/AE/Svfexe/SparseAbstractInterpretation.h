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

    /// Write an ObjVar into State[node] and discard its old branch constraint.
    /// A precise GepObjVar write also
    /// propagates the base and sub-object values to its SVFG use sites.
    /// This supplements a dynamic `a[i][j]` load whose SVFG edge names only one
    /// representative field of `a`.
    void updateAbsValue(const ObjVar* var, const AbstractValue& val,
                        const ICFGNode* node) override;
    using SemiSparseAbstractInterpretation::updateAbsValue;

    /// Apply FullSparse store semantics when handling an ordinary StoreStmt.
    /// A strong update kills the previous value of one concrete object:
    ///
    ///   int x = 1;
    ///   int *p = &x;        // AE address candidates = {x}
    ///   *p = 2;             // State[x] becomes [2,2]
    ///
    /// A weak update preserves every possible old value and joins the stored
    /// value. It is required when the pointer has multiple targets or denotes
    /// a summary object such as a heap allocation or merged array elements:
    ///
    ///   int x = 1, y = 3;
    ///   int *p = cond ? &x : &y;  // AE address candidates = {x,y}
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

    /// Record a conditional edge's interval in branchConstraints[succ]. For
    /// `if (a[9] < 10)`, the true successor records the constraint on a.9.
    /// FullSparse applies it after incoming Obj values merge; writing it only
    /// to the temporary predecessor state would lose it because joinStates
    /// does not copy ObjVars along ordinary ICFG edges.
    void recordBranchRefinement(NodeID objId, const IntervalValue& narrowed,
                                AbstractState& as, const ICFGNode* loadIcfg,
                                const ICFGNode* succ) override;

private:
    /// Decide whether a store's AE address value identifies one concrete Obj.
    /// Heap objects, recursive locals, field-insensitive objects, and layouts
    /// exceeding the field limit cannot be strongly updated. An array element
    /// is eligible only when it has a separate GepObjVar in an unmerged layout.
    ///
    ///   int x; int *p = &x; *p = 1;       // strong update
    ///   int *p = malloc(sizeof(int));      // heap summary
    ///   *p = 1;                            // weak update
    ///   int a[4]; a[3] = 1;                // strong: one concrete sub-object
    ///   a[i] = 1;                          // weak: several candidate elements
    ///   int *q = cond ? &x : &y; *q = 1;   // weak update: two targets
    ///
    bool isStrongUpdate(const AddressValue& targets,
                        const AbstractState& state) const;

    /// Ignore an older value only when a later strong store must overwrite it
    /// before the use, in the same straight-line basic block:
    ///   S: int a[10] = {0, ..., 9};
    ///   W: a[9] = 10;
    ///   L: use(a[9]);          // S's a.9 is killed by W, other elements stay
    /// Stores in other blocks may be conditional, so their incoming values
    /// still JOIN. This checks instruction order, not MemorySSA versions.
    bool isObjValueOverwrittenInBlock(const ICFGNode* source,
                                      const ICFGNode* target, NodeID objId);

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
    /// For stores on alternative branches, the incoming maps contain
    /// `{a.9:[10,10]}` from Store1 and
    /// `{a.9:[20,20]}` from Store2. Before the load executes, this method sets
    /// `State[load][a.9]` to their JOIN, `[10,20]`. This is a conservative JOIN
    /// across different sources, not another store update. An older value is
    /// omitted only if a strong store overwrites it in the same basic block.
    /// No MemorySSA definition or Phi-operand filtering is performed here.
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
    ///   State[source] --collect(base)--> propagatedObjValues[target][source, base].
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

    /// Inherit branch constraints along ICFG edges, then apply them to incoming
    /// Obj values. In `if (x > 0) { use1(x); use2(x); }`, use1 gets the edge
    /// constraint and use2 inherits it. At a merge, retain only constraints
    /// present on every predecessor with state and JOIN their intervals.
    /// Finally MEET them into State[node] after the incoming Obj-value JOIN.
    void propagateAndApplyBranchConstraints(const ICFGNode* node);

    /// Path-local interval constraints, not another complete AbstractState.
    /// The key is the ICFG node where the constraint is currently valid, not
    /// just the conditional edge's first successor. Object writes remove the
    /// affected entry; subsequent nodes inherit only the remaining entries.
    Map<const ICFGNode*, Map<NodeID, IntervalValue>> branchConstraints;

    /// Propagated FullSparse Obj values, grouped by target, source, and base Obj.
    /// Some entries contain only a base Obj, while others also contain
    /// sub-objects. The authoritative states remain in abstractTrace.
    Map<const ICFGNode*, Map<ObjValueOrigin, ObjValueMap>> propagatedObjValues;

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
