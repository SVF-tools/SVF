//===- MTASlicer.h -- Multi-stage on-demand program slicers ---------------===//
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
//===----------------------------------------------------------------------===//

/*
 * MTASlicer.h
 *
 *      Author: Jiawei Yang
 */

#ifndef MTA_MTASLICER_H
#define MTA_MTASLICER_H

#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFVariables.h"
#include "WPA/Andersen.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/TCT.h"
#include "Graphs/SlicedGraphs.h"
#include "Graphs/ThreadCallGraph.h"
#include "Graphs/ICFG.h"
#include "Graphs/ICFGNode.h"
#include <deque>
#include "Graphs/ICFGEdge.h"
#include "Graphs/CallGraph.h"
#include "Util/WorkList.h"
#include <memory>
#include <vector>
#include <utility>

namespace SVF
{

// Forward declarations
class SVFG;
class VFGNode;          // SVFGNode is a typedef for VFGNode
class PointerAnalysis;

struct ValueFlowSlice
{
    OrderedSet<const SVFGNode*> svfgNodes;
    OrderedSet<const ICFGNode*> icfgNodes;

    NodeBS nodeIds() const
    {
        NodeBS ids;
        for (const SVFGNode* node : svfgNodes)
            ids.set(node->getId());
        return ids;
    }
};


//===----------------------------------------------------------------------===//
// SlicedTCT - the Thread-Create-Tree rebuilt over a SlicedThreadCallGraphView.
//
// Inherits TCT and overrides the ThreadCallGraph-traversing steps to use the
// sliced view. It sits at the sliced-representation layer (built from the view,
// consumed by the sliced MHP/LockAnalysis graph-access), which is why it lives
// here with the views rather than beside the analyses.
//===----------------------------------------------------------------------===//
class SlicedTCT : public TCT
{
public:
    /// @param pointerAnalysis the shared Andersen pre-analysis
    /// @param slicedView SlicedSVFIRView containing a SlicedThreadCallGraphView
    /// @param contextLimit maximum context length for the main analysis
    static std::unique_ptr<SlicedTCT> create(
        PointerAnalysis& pointerAnalysis, const SlicedSVFIRView& slicedView,
        u32_t contextLimit);

    ~SlicedTCT() override = default;

protected:
    void build() override;
    void markRelProcs() override;
    void markRelProcs(const FunObjVar* fun) override;
    void collectLoopInfoForJoin() override;
    void handleCallRelation(CxtThreadProc& ctp, const CallGraphEdge* cgEdge, const CallICFGNode* cs) override;

private:
    SlicedTCT(PointerAnalysis& pointerAnalysis,
              const SlicedSVFIRView& slicedView, u32_t contextLimit);

    void collectEntryFunInCallGraph() override;

    const SlicedThreadCallGraphView& tcgView;
    bool isKeptNode(const CallGraphNode* node) const;
    bool isKeptEdge(const CallGraphEdge* edge) const;
    void getKeptForkSites(std::vector<const ICFGNode*>& out) const;
    void getKeptJoinSites(std::vector<const ICFGNode*>& out) const;
};


/**
 * MTASlicerBase - Base class for program slicing.
 *
 * Holds the shared helper methods and data members used by both concrete slicers
 * (the ILA and FSPTA stages of MultiStageSlicer).
 */
class MTASlicerBase
{
public:
    MTASlicerBase(SVFIR* svfir, AndersenBase* pta, MHP* mhp,
                  LockAnalysis* lockAnalysis, SVFG* svfg = nullptr);

protected:
    SVFIR* svfir;
    AndersenBase* pta;
    MHP* mhp;
    LockAnalysis* lockAnalysis;
    CallGraph* callGraph;
    SVFG* svfg;   ///< thread-aware VFG_pre (PTA/Single slicers; null for MTA)

    // === Data flow analysis helper ===
    /// The SVFG-node granularity of the data-dependence slice above: the set of
    /// VFG nodes reachable backward from the seeds. ThreadVF(VFG'_pre) is exactly
    /// the thread-aware edges whose *both* endpoints lie in this set, so ILA
    /// slicing uses it to restrict the [THREAD-VF] sources to surviving edges.
    OrderedSet<const VFGNode*> computeDataDependenceSVFGNodes(
        const OrderedSet<const SVFStmt*>& seeds, SVFG* svfg);

    static void enqueueSVFGNode(const SVFGNode* node,
                                OrderedSet<const SVFGNode*>& visited,
                                std::deque<const SVFGNode*>& worklist);

    /// Project the retained VFG nodes (plus the seeds) onto their ICFG nodes.
    OrderedSet<const ICFGNode*> svfgNodesToICFGNodes(
        const OrderedSet<const VFGNode*>& nodes, const OrderedSet<const SVFStmt*>& seeds);

    // === Thread analysis helpers ===
    OrderedSet<const CallICFGNode*> getDependentThreadCreate(const ICFGNode* node);
    OrderedSet<const TCTNode*> getTCTNodeSetFromNode(const ICFGNode* node);

    // === Lock analysis helpers ===
    OrderedSet<const ICFGNode*> getLockSet(const ICFGNode* node);
    OrderedSet<const CallICFGNode*> collectPthreadStatements(
        const OrderedSet<const ICFGNode*>& sourceNodes);
    OrderedSet<const CallICFGNode*> collectMutexStatements(
        const OrderedSet<const ICFGNode*>& sourceNodes);

    // === Common slicing helpers ===
    /**
     * Collect common pthread and mutex statements (shared by PTA and MTA slicing).
     * @param sourceNodes Complete ILA source set ([INIT] union [THREAD-VF])
     * @return Pair of (pthreadCallNodes, mutexCallNodes)
     */
    std::pair<OrderedSet<const CallICFGNode*>, OrderedSet<const CallICFGNode*>>
            collectCommonThreadStatements(const OrderedSet<const ICFGNode*>& sourceNodes);

    /// Add synchronization primitives and the control-flow anchors required by
    /// the sliced MHP/lock analyses.
    void addSynchronizationDependencies(
        const OrderedSet<const CallICFGNode*>& pthreadCallNodes,
        const OrderedSet<const CallICFGNode*>& mutexCallNodes,
        OrderedSet<const ICFGNode*>& retainedNodes);

    // === ICFG analysis helpers ===
    /**
     * Call-dependence expansion (used by MultiStageSlicer): take the
     * kept functions of the given nodes, close upward over the call graph
     * (every transitive caller), then add each kept function's entry/exit nodes
     * and the call/ret nodes of every call site targeting it.
     * @param nodes Current set of ICFG nodes
     * @return The input nodes plus the call/ret and entry/exit nodes above
     */
    OrderedSet<const ICFGNode*> expandCallDependence(const OrderedSet<const ICFGNode*>& nodes);

};

/**
 * MultiStageSlicer - the multi-stage (differential) slicer of MSli. The
 * pre-candidate closure scopes ILA queries and the Main-TVF overlay; the final
 * closure is recomputed over that refined main graph.
 *   Stage 1 (ILA):  runILASlicing  -- synchronization slicing + function
 *                                     expansion, feeding the sliced MHP/lock.
 *   Stage 2 (FSPTA): runPTASlicing -- backward data-dependence slice feeding
 *                                     the sliced flow-sensitive solve.
 * Contrast: SingleSlicer below folds everything into ONE unified slice.
 */
class MultiStageSlicer : public MTASlicerBase
{
public:
    MultiStageSlicer(SVFIR* svfir, AndersenBase* pta, MHP* mhp,
                     LockAnalysis* lockAnalysis, SVFG* svfg = nullptr);

    /**
     * Stage 1: the ILA slice (synchronization + function expansion for the IRView).
     * @param vulnerableStatements Set of vulnerable statements to start slicing from
     *        (the [INIT] rule: pre-analysis race statements).
     * @param threadVFSources Extra ILA slicing sources from the [THREAD-VF] rule
     *        (MSli 4.2): statements whose MHP/lock-span results are queried during
     *        the main-phase thread-aware value-flow construction (endpoints and
     *        in-span non-interference witnesses collected while building VFG_pre).
     * @return Set of ICFG nodes in the slice (including call/ret and entry/exit nodes)
     */
    OrderedSet<const ICFGNode*> runILASlicing(
        const OrderedSet<const SVFStmt*>& vulnerableStatements,
        const OrderedSet<const ICFGNode*>& threadVFSources = {});

    /**
     * Stage 2: the FSPTA slice (backward data dependence over the refined main
     * value-flow graph; node set only, no function expansion).
     */
    ValueFlowSlice runPTASlicing(
        const OrderedSet<const SVFStmt*>& vulnerableStatements,
        SVFG* refinedMainVFG);

    /// Compute the pre-candidate slice used to restrict [THREAD-VF] sources and
    /// scope construction of the refined main overlay.
    void computePreCandidateSlice(
        const OrderedSet<const SVFStmt*>& vulnerableStatements);

    /// Return the pre-candidate slice after computePreCandidateSlice().
    const ValueFlowSlice& getPreCandidateSlice() const;

private:
    ValueFlowSlice preCandidateSlice;
    bool preCandidateComputed = false;
};

/**
 * SingleSlicer - Unified slicer combining synchronization, data, and call
 * dependence into ONE slice (the single-pass baseline, MSli §3/§5.4: the
 * transitive closure of the target statements under the combined dependence
 * graph). Both ILA and FSPTA run on this single slice, so V_ILA, V_PTA subset
 * V_Single. Used by the differential-slicing ablation (-mta-slicing-single).
 *
 * Iteratively applies synchronization, data, and call dependence over the
 * thread-aware VFG_pre until convergence.
 */
class SingleSlicer : public MTASlicerBase
{
public:
    SingleSlicer(SVFIR* svfir, AndersenBase* pta, MHP* mhp,
                 LockAnalysis* lockAnalysis, SVFG* svfg = nullptr);

    /**
     * Perform unified slicing combining synchronization, data, and call dependence.
     * @param vulnerableStatements Set of vulnerable statements to start slicing from
     * @return Exact SVFG data slice plus its synchronization/call-complete ICFG view
     */
    ValueFlowSlice runSlicing(
        const OrderedSet<const SVFStmt*>& vulnerableStatements);
};

} // End namespace SVF

#endif // MTA_MTASLICER_H
