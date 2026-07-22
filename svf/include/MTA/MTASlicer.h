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
 *
 * The program slicers of "Multi-Stage On-Demand Program Slicing for Modular
 * Analysis of Multi-Threaded Programs" (ISSTA 2026): a shared MTASlicerBase plus
 * three concrete slicers.
 *   - MultiStageSlicer   : ILA (sync + dual + call) slice for the thread-aware analysis
 *   (its FSPTA stage: data-dependence slice over the thread-aware VFG_pre)
 *   - SingleSlicer: one unified slice combining all three dependence kinds, shared
 *                   by both ILA and FSPTA (the single-pass baseline, MSli §3/§5.4)
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
#include <fstream>
#include "Graphs/ICFGEdge.h"
#include "Graphs/CallGraph.h"
#include "Util/WorkList.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace SVF
{

// Forward declarations
class SVFG;
class VFGNode;          // SVFGNode is a typedef for VFGNode
class PointerAnalysis;


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
    /// @param p PointerAnalysis (the shared Andersen pre-analysis)
    /// @param slicedView SlicedSVFIRView containing a SlicedThreadCallGraphView
    /// @param maxContextLen Max context length (0 = use Options::MaxContextLen())
    SlicedTCT(PointerAnalysis* p, const SlicedSVFIRView* slicedView, u32_t maxContextLen = 0);

    ~SlicedTCT() override = default;

    /// Override pushCxt to use the custom maxContextLen
    void pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee) override;

protected:
    void build() override;
    void markRelProcs() override;
    void markRelProcs(const FunObjVar* fun) override;
    void collectLoopInfoForJoin() override;
    void handleCallRelation(CxtThreadProc& ctp, const CallGraphEdge* cgEdge, const CallICFGNode* cs) override;

private:
    void collectEntryFunInCallGraph() override;

    const SlicedThreadCallGraphView* tcgView; // ThreadCallGraph view (from the sliced view)
    u32_t maxContextLen; // 0 means use Options::MaxContextLen()

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
    MTASlicerBase(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
               LockAnalysis* lockAnalysis, SVFG* vfg = nullptr);
    virtual ~MTASlicerBase();

protected:
    SVFIR* svfIr;
    AndersenBase* pta;
    MHP* mhp;
    LockAnalysis* lockAnalysis;
    CallGraph* callGraph;
    SVFG* vfg;   ///< thread-aware VFG_pre (PTA/Single slicers; null for MTA)

    // === Data flow analysis helper ===
    /**
     * Paper-faithful (§4.3) data-dependence slice over the thread-aware SVFG
     * (VFG_pre): seed from the value-flow nodes of the given statements and
     * backward-traverse every value-flow edge -- direct (top-level def-use),
     * indirect (address-taken / MemSSA def-use), and thread-aware interference.
     * Returns the kept ICFG nodes. This is the single dependence model used
     * by the FSPTA stage.
     */
    OrderedSet<const ICFGNode*> sliceDataDependenceOverVFG(
        const OrderedSet<const SVFStmt*>& seeds, SVFG* vfg);

    /// The SVFG-node granularity of the data-dependence slice above: the set of
    /// VFG nodes reachable backward from the seeds. ThreadVF(VFG'_pre) is exactly
    /// the thread-aware edges whose *both* endpoints lie in this set, so ILA
    /// slicing uses it to restrict the [THREAD-VF] sources to surviving edges.
    OrderedSet<const VFGNode*> computeDataDependenceSVFGNodes(
        const OrderedSet<const SVFStmt*>& seeds, SVFG* vfg);

    /// Project the retained VFG nodes (plus the seeds) onto their ICFG nodes.
    OrderedSet<const ICFGNode*> svfgNodesToICFGNodes(
        const OrderedSet<const VFGNode*>& nodes, const OrderedSet<const SVFStmt*>& seeds);

    // === Thread analysis helpers ===
    OrderedSet<const SVFStmt*> getDependentThreadCreate(const SVFStmt* stmt);
    OrderedSet<const TCTNode*> getTCTNodeSetFromNode(const ICFGNode* node);

    // === Lock analysis helpers ===
    OrderedSet<const ICFGNode*> getLockSet(const ICFGNode* node);
    OrderedSet<const CallICFGNode*> collectPthreadStatements(const OrderedSet<const SVFStmt*>& vulnerableStmts);
    OrderedSet<const CallICFGNode*> collectMutexStatements(const OrderedSet<const SVFStmt*>& vulnerableStmts);

    // === Common slicing helpers ===
    /**
     * Collect common pthread and mutex statements (shared by PTA and MTA slicing).
     * @param vulnerableStatements Set of vulnerable statements
     * @return Pair of (pthreadCallNodes, mutexCallNodes)
     */
    std::pair<OrderedSet<const CallICFGNode*>, OrderedSet<const CallICFGNode*>>
        collectCommonThreadStatements(const OrderedSet<const SVFStmt*>& vulnerableStatements);

    // === ICFG analysis helpers ===
    OrderedSet<const ICFGNode*> buildBackwardICFGNodeSet(const OrderedSet<const ICFGNode*>& vulnerableNodes);

    /**
     * Call-dependence expansion (used by MultiStageSlicer): take the
     * kept functions of the given nodes, close upward over the call graph
     * (every transitive caller), then add each kept function's entry/exit nodes
     * and the call/ret nodes of every call site targeting it.
     * @param nodes Current set of ICFG nodes
     * @return The input nodes plus the call/ret and entry/exit nodes above
     */
    OrderedSet<const ICFGNode*> expandCallDependence(const OrderedSet<const ICFGNode*>& nodes);

    /**
     * Perform dual slicing (temporal slicing): filter statements based on control flow and parallel execution.
     * This is shared by both PTA and MTA slicing.
     * @param slicedNodes Set of statements from statement-level slicing
     * @return Set of ICFG nodes in the dual slice
     */
    OrderedSet<const ICFGNode*> runDualSlicing(
        const OrderedSet<const ICFGNode*>& slicedNodes);
};

/**
 * MultiStageSlicer - the multi-stage (differential) slicer of MSli: one class,
 * two stages sharing one memoised data-dependence closure over VFG_pre.
 *   Stage 1 (ILA):  runILASlicing  -- synchronization/dual slicing + function
 *                                     expansion, feeding the sliced MHP/lock.
 *   Stage 2 (FSPTA): runPTASlicing -- backward data-dependence slice feeding
 *                                     the sliced flow-sensitive solve.
 * Contrast: SingleSlicer below folds everything into ONE unified slice.
 */
class MultiStageSlicer : public MTASlicerBase
{
public:
    MultiStageSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
              LockAnalysis* lockAnalysis, SVFG* vfg = nullptr);

    /**
     * Stage 1: the ILA slice (dual slicing + function expansion for the IRView).
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
     * Stage 2: the FSPTA slice (backward data dependence over the thread-aware
     * VFG_pre; node set only, no function expansion).
     */
    OrderedSet<const ICFGNode*> runPTASlicing(
        const OrderedSet<const SVFStmt*>& vulnerableStatements);

    /**
     * The FSPTA data-dependence slice at SVFG-node granularity (memoised). The
     * ILA stage queries this first, to restrict the [THREAD-VF] sources to
     * ThreadVF(VFG'_pre); runPTASlicing reuses the same set, so the backward
     * closure over VFG_pre is computed once and shared across both stages.
     */
    const OrderedSet<const VFGNode*>& getRetainedSVFGNodes(
        const OrderedSet<const SVFStmt*>& vulnerableStatements);

private:
    OrderedSet<const VFGNode*> retainedSVFGNodes; ///< memoised data-dependence slice
    bool retainedComputed = false;
};

/**
 * SingleSlicer - Unified slicer combining synchronization, data, and call
 * dependence into ONE slice (the single-pass baseline, MSli §3/§5.4: the
 * transitive closure of the target statements under the combined dependence
 * graph). Both ILA and FSPTA run on this single slice, so V_ILA, V_PTA subset
 * V_Single. Used by the differential-slicing ablation (-mta-slicing-single).
 *
 * Iteratively applies data dependence (over the thread-aware VFG_pre) and call
 * dependence until convergence, then a single dual-slicing pass.
 */
class SingleSlicer : public MTASlicerBase
{
public:
    SingleSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                 LockAnalysis* lockAnalysis, SVFG* vfg = nullptr);

    /**
     * Perform unified slicing combining synchronization, data, and call dependence.
     * @param vulnerableStatements Set of vulnerable statements to start slicing from
     * @return Set of ICFG nodes in the slice (including call/ret and entry/exit nodes)
     */
    OrderedSet<const ICFGNode*> runSlicing(
        const OrderedSet<const SVFStmt*>& vulnerableStatements);
};

} // End namespace SVF

#endif // MTA_MTASLICER_H
