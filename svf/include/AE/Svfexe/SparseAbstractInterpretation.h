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

    bool reuseFunctionResult(const ICFGNode* funEntry) override;
    void recordFunctionResult(const ICFGNode* funEntry) override;
    bool reuseCycleResult(const ICFGCycleWTO* cycle) override;
    void recordCycleResult(const ICFGCycleWTO* cycle) override;

private:
    struct FunctionInputSignature
    {
        AbstractState scalarAndFreedInputs;
        std::vector<AbstractState> objInputs;

        bool operator==(const FunctionInputSignature& other) const
        {
            return scalarAndFreedInputs == other.scalarAndFreedInputs &&
                   objInputs == other.objInputs;
        }
    };

    /// One version number per subgraph function; the inputs themselves are
    /// stored once per function in latestFunctionInputs (avoids copying every
    /// callee's inputs into every caller's signature).
    using FunctionSubgraphSignature = std::vector<u64_t>;

    /// Static description of what can reach a loop from outside it. The
    /// region is the cycle's ICFG nodes plus the call closure of its calls.
    struct CycleRegion
    {
        /// ICFG nodes of the cycle, including nested cycles.
        Set<const ICFGNode*> nodes;
        /// Functions called from the cycle, transitively.
        Set<const FunObjVar*> closure;
        /// The same functions in call-graph DFS order.
        std::vector<const FunObjVar*> functions;
        std::vector<const ValVar*> externalValVars;
        std::vector<const SparseObjGraph::Edge*> externalObjRoutes;
        std::vector<const ICFGEdge*> externalICFGEdges;

        bool contains(const ICFGNode* node) const
        {
            return nodes.count(node) || closure.count(node->getFun());
        }
    };

    struct CycleInputSignature
    {
        FunctionSubgraphSignature functions;
        /// External ValVar values, then this cycle's own ValVars. An
        /// enclosing cycle's widening can rewrite the latter at def-sites.
        AbstractState scalars;
        AbstractState cycleValVars;
        std::vector<AbstractState> objInputs;
        /// Feasibility of each external ICFG in-edge and its freed addresses.
        std::vector<bool> feasibleEdges;
        AbstractState freed;

        bool operator==(const CycleInputSignature& other) const
        {
            return functions == other.functions && scalars == other.scalars &&
                   cycleValVars == other.cycleValVars &&
                   objInputs == other.objInputs &&
                   feasibleEdges == other.feasibleEdges && freed == other.freed;
        }
    };

    FunctionInputSignature collectFunctionInputs(const ICFGNode* funEntry);
    bool buildReusableFunctionSubgraph(const FunObjVar* fun,
                                       Set<const FunObjVar*>& visited,
                                       std::vector<const FunObjVar*>& functions);
    const std::vector<const FunObjVar*>* getReusableFunctionSubgraph(
        const FunObjVar* fun);
    FunctionSubgraphSignature collectFunctionSubgraphInputs(
        const std::vector<const FunObjVar*>& functions);
    bool allowRecursiveReuse() const;
    const CycleRegion* getCycleRegion(const ICFGCycleWTO* cycle);
    void collectCycleNodes(const ICFGCycleWTO* cycle,
                           Set<const ICFGNode*>& nodes) const;
    bool collectCycleCallees(CycleRegion& region);
    void collectCycleExternalInputs(CycleRegion& region);
    void addExternalValVar(const SVFVar* var, CycleRegion& region,
                           Set<const ValVar*>& visited);
    CycleInputSignature collectCycleInputs(const ICFGCycleWTO* cycle,
                                           const CycleRegion& region);
    void buildSVFG();
    // The prebuilt packed-Obj route graph; the SVFG is released once it is built.
    std::unique_ptr<SparseObjGraph> objRoutes;
    Map<const FunObjVar*, std::vector<const FunObjVar*>> reusableFunctionSubgraphs;
    Set<const FunObjVar*> nonReusableFunctions;
    Map<const FunObjVar*, FunctionSubgraphSignature> lastFunctionInputs;
    Map<const FunObjVar*, std::pair<FunctionInputSignature, u64_t>> latestFunctionInputs;
    u64_t functionInputVersion(const FunObjVar* fun);
    Set<const FunObjVar*> completedFunctions;
    Map<const ICFGCycleWTO*, CycleRegion> cycleRegions;
    Set<const ICFGCycleWTO*> nonReusableCycles;
    Map<const ICFGCycleWTO*, CycleInputSignature> lastCycleInputs;
    Set<const ICFGCycleWTO*> completedCycles;
};

} // namespace SVF

#endif /* INCLUDE_AE_SVFEXE_SPARSEABSTRACTINTERPRETATION_H_ */
