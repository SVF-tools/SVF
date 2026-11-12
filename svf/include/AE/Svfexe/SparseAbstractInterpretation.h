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
#include <optional>

#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/SparseObjGraph.h"
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

/// Full-sparse Obj values follow prebuilt packed-object routes. ValVars retain
/// semi-sparse def-site access; freed addresses follow feasible ICFG edges.
class FullSparseAbstractInterpretation : public SemiSparseAbstractInterpretation
{
public:
    FullSparseAbstractInterpretation()
    {
        buildSVFG();
    }
    ~FullSparseAbstractInterpretation() override;

    /// Merge only freed addresses from a feasible ICFG predecessor.
    /// Obj values are collected separately through sparse object routes.
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

    /// Read incoming route sources, refine branch inputs, and JOIN their
    /// selected Obj values into this ICFG node's state.
    bool mergeStatesFromPredecessors(const ICFGNode* node) override;

    /// Skip a WTO component, a single node or a cycle, when the inputs it
    /// reads from outside itself match those of its last run (preAbsTrace):
    /// abstractTrace then still holds what running it again would produce.
    /// A node written by another (a return site by its call, a def-site by
    /// widening or narrowing) loses its record, so it is run again.
    void handleWTOComponent(const ICFGWTOComp* comp,
                            const CallICFGNode* caller) override;
    bool widenCycleState(const AbstractState& prev, const AbstractState& cur,
                         const ICFGCycleWTO* cycle) override;
    bool narrowCycleState(const AbstractState& prev, const AbstractState& cur,
                          const ICFGCycleWTO* cycle) override;

private:
    /// Compare the inputs that nodes read from outside nodes with inputs,
    /// or record them when save is set: the freed addresses (and
    /// reachability) of their ICFG predecessors, the fields of the base Objs
    /// on their routes, and their ValVars.
    bool matchInputs(std::vector<std::optional<AbstractValue>>& inputs,
                     const std::vector<const ICFGNode*>& nodes, bool save);

    void forgetDefSites(const ICFGCycleWTO* cycle);

    void buildSVFG();
    // The prebuilt packed-Obj route graph; the SVFG is released once it is built.
    std::unique_ptr<SparseObjGraph> objRoutes;
    /// The inputs each component read from outside itself at its last run,
    /// by its first node (a single node, or the head of a cycle).
    Map<const ICFGNode*, std::vector<std::optional<AbstractValue>>> preAbsTrace;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
