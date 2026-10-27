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

// TODO: write comments, step 1 2 3
class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation()
    {
        buildSVFG();
    }
    ~FullSparseAbstractInterpretation() override;

    /// Merge only freed addresses from a feasible ICFG predecessor.
    /// Obj values are collected separately through SVFG inputs.
    void mergeFreedAddrs(AbstractState& dst, const AbstractState& src);

protected:
    using SemiSparseAbstractInterpretation::joinStates;

    /// JOIN only the selected Obj IDs from src into dst; missing values
    /// contribute nothing. The caller selects base/sub Objs and handles
    /// path constraints and strong/weak updates. ValVars and freed addresses
    /// are not copied here. Memory lookups use AbstractState's
    /// existing address resolution, including its freed-address handling.
    void joinStates(AbstractState& dst, const AbstractState& src,
                    const NodeBS& objs);

    /// 1. Read Obj values from incoming indirect SVFG edges.
    /// 2. Filter/refine that contribution along feasible ICFG paths.
    /// 3. JOIN it into this ICFGNode's state, then execute normally.
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;

private:
    /// Step 1: collect only the indirect edge's base/sub Obj values.
    /// store i32 7, ptr %a2; ...; load i32, ptr %a2:
    /// an edge naming a or a.2 selects its base/sub Objs from State[store].
    /// The caller checks that the source state is available. Return the
    /// selected IDs in objs, even if their values are not available yet.
    /// Direct ValVar edges contribute no Obj values or IDs.
    AbstractState CollectObjValueFlowInput(const VFGEdge* edge, NodeBS& objs);

    /// Filter one SVFG input using branch-feasible ICFG paths.
    /// int x = input();   // D: x=[0,10]
    /// if (x > 5) {       // B
    ///     use(x);        // N: this path contributes x=[6,10]
    /// }
    /// See the implementation for D/B/N patterns at each decision branch,
    /// including conservative call/global handling and path/loop merges.
    /// A local worklist JOINs inputs at visited ICFG nodes, not whole paths.
    /// It does not execute arithmetic or cache results across WTO iterations.
    /// Along the path, reuse computed Store results only for carried Objs;
    /// shared isStrongUpdate selects replacement versus conservative JOIN.
    AbstractState filterValueFlowInputByPath(const VFGEdge* edge,
            const AbstractState& input);

    void buildSVFG();
    // Owns the graph whose VFG nodes are indexed by their ICFG nodes.
    std::unique_ptr<SVFGBuilder> svfgBuilder;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
