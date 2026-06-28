//===- Slicer.h -- Multi-stage on-demand program slicers ------------------===//
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
 * Slicer.h
 *
 *      Author: Jiawei Yang
 *
 * The MSli program slicers: a shared SlicerBase plus three concrete slicers.
 *   - MTASlicer   : ILA (sync + dual + call) slice for the thread-aware analysis
 *   - PTASlicer   : data-dependence slice over the thread-aware VFG_pre
 *   - SingleSlicer: one unified slice combining all three dependence kinds, shared
 *                   by both ILA and FSPTA (the single-pass baseline, MSli §3/§5.4)
 */

#ifndef MTA_SLICER_H
#define MTA_SLICER_H

#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include <WPA/Andersen.h>
#include <MTA/MHP.h>
#include <MTA/LockAnalysis.h>
#include <MTA/TCT.h>
#include <Graphs/ThreadCallGraph.h>
#include <Graphs/ICFGNode.h>
#include <set>
#include <vector>
#include <utility>

namespace SVF
{

// Forward declarations
class SVFG;
class VFGNode;          // SVFGNode is a typedef for VFGNode
class SlicedSVFIRView;

/**
 * SlicerBase - Base class for program slicing.
 *
 * Holds the shared helper methods and data members used by both concrete slicers
 * (MTASlicer, PTASlicer).
 */
class SlicerBase {
protected:
    SVFIR* svfIr;
    AndersenBase* pta;
    SVF::MHP* mhp;
    SVF::LockAnalysis* lockAnalysis;
    CallGraph* threadCallGraph;

    // === Data flow analysis helper ===
    /**
     * Paper-faithful (§4.3) data-dependence slice over the thread-aware SVFG
     * (VFG_pre): seed from the value-flow nodes of the given statements and
     * backward-traverse every value-flow edge -- direct (top-level def-use),
     * indirect (address-taken / MemSSA def-use), and thread-aware interference.
     * Returns the kept ICFG nodes. This is the single dependence model used
     * by PTASlicer.
     */
    std::set<const ICFGNode*> sliceDataDependenceOverVFG(
        const std::set<const SVFStmt*>& seeds, SVF::SVFG* vfg);

    /// The SVFG-node granularity of the data-dependence slice above: the set of
    /// VFG nodes reachable backward from the seeds. ThreadVF(VFG'_pre) is exactly
    /// the thread-aware edges whose *both* endpoints lie in this set, so ILA
    /// slicing uses it to restrict the [THREAD-VF] sources to surviving edges.
    std::set<const SVF::VFGNode*> computeDataDependenceSVFGNodes(
        const std::set<const SVFStmt*>& seeds, SVF::SVFG* vfg);

    /// Project the retained VFG nodes (plus the seeds) onto their ICFG nodes.
    std::set<const ICFGNode*> svfgNodesToICFGNodes(
        const std::set<const SVF::VFGNode*>& nodes, const std::set<const SVFStmt*>& seeds);

    // === Thread analysis helpers ===
    std::set<const SVFStmt*> getDependentThreadCreate(const SVFStmt* stmt);
    std::set<const TCTNode*> getTCTNodeSetFromNode(const ICFGNode* node);

    // === Lock analysis helpers ===
    std::set<const ICFGNode*> getLockSet(const ICFGNode* node);
    std::set<const CallICFGNode*> collectPthreadStatements(const std::set<const SVFStmt*>& vulnerableStmts);
    std::set<const CallICFGNode*> collectMutexStatements(const std::set<const SVFStmt*>& vulnerableStmts);

    // === Common slicing helpers ===
    /**
     * Collect common pthread and mutex statements (shared by PTA and MTA slicing).
     * @param vulnerableStatements Set of vulnerable statements
     * @return Pair of (pthreadCallNodes, mutexCallNodes)
     */
    std::pair<std::set<const CallICFGNode*>, std::set<const CallICFGNode*>>
        collectCommonThreadStatements(const std::set<const SVFStmt*>& vulnerableStatements);

    // === ICFG analysis helpers ===
    std::set<const ICFGNode*> buildBackwardICFGNodeSet(const std::set<const ICFGNode*>& vulnerableNodes);

    /**
     * Call-dependence expansion (used by MTASlicer): take the
     * kept functions of the given nodes, close upward over the call graph
     * (every transitive caller), then add each kept function's entry/exit nodes
     * and the call/ret nodes of every call site targeting it.
     * @param nodes Current set of ICFG nodes
     * @return The input nodes plus the call/ret and entry/exit nodes above
     */
    std::set<const ICFGNode*> expandCallDependence(const std::set<const ICFGNode*>& nodes);

    /**
     * Perform dual slicing (temporal slicing): filter statements based on control flow and parallel execution.
     * This is shared by both PTA and MTA slicing.
     * @param slicedNodes Set of statements from statement-level slicing
     * @return Set of ICFG nodes in the dual slice
     */
    std::set<const ICFGNode*> runDualSlicing(
        const std::set<const ICFGNode*>& slicedNodes);

public:
    SlicerBase(SVFIR* svfIr, AndersenBase* pta, SVF::MHP* mhp,
               SVF::LockAnalysis* lockAnalysis);
    virtual ~SlicerBase();
};

/**
 * MTASlicer - Slicer for Multi-Threaded Analysis.
 *
 * Performs the ILA slice for MTA, including function expansion and temporal
 * (dual) slicing.
 */
class MTASlicer : public SlicerBase {
public:
    MTASlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
              LockAnalysis* lockAnalysis);

    /**
     * Perform slicing for MTA (includes dual slicing and function expansion for IRView).
     * @param vulnerableStatements Set of vulnerable statements to start slicing from
     *        (the [INIT] rule: pre-analysis race statements).
     * @param threadVFSources Extra ILA slicing sources from the [THREAD-VF] rule
     *        (MSli §4.2): statements whose MHP/lock-span results are queried during
     *        the main-phase thread-aware value-flow construction (endpoints and
     *        in-span non-interference witnesses collected while building VFG_pre).
     * @return Set of ICFG nodes in the slice (including call/ret and entry/exit nodes)
     */
    std::set<const ICFGNode*> runSlicing(
        const std::set<const SVFStmt*>& vulnerableStatements,
        const std::set<const ICFGNode*>& threadVFSources = {});
};

/**
 * PTASlicer - Slicer for Pointer Analysis.
 *
 * Performs backward data-dependence slicing over the thread-aware VFG_pre.
 */
class PTASlicer : public SlicerBase {
private:
    SVF::SVFG* vfg; ///< thread-aware VFG_pre (built once in pre-analysis)

public:
    PTASlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
              LockAnalysis* lockAnalysis, SVF::SVFG* vfg = nullptr);

    /**
     * Perform slicing for pointer analysis (returns only node set, no IRView needed).
     * @param vulnerableStatements Set of vulnerable statements to start slicing from
     * @return Set of ICFG nodes in the slice (without function expansion)
     */
    std::set<const ICFGNode*> runSlicing(
        const std::set<const SVFStmt*>& vulnerableStatements);

    /**
     * The FSPTA data-dependence slice at SVFG-node granularity (memoised). ILA
     * slicing queries this before PTA slicing runs, to restrict the [THREAD-VF]
     * sources to ThreadVF(VFG'_pre); runSlicing reuses the same set, so the
     * backward closure over VFG_pre is computed once and shared.
     */
    const std::set<const SVF::VFGNode*>& getRetainedSVFGNodes(
        const std::set<const SVFStmt*>& vulnerableStatements);

private:
    std::set<const SVF::VFGNode*> retainedSVFGNodes; ///< memoised data-dependence slice
    bool retainedComputed = false;
};

/**
 * SingleSlicer - Unified slicer combining synchronization, data, and call
 * dependence into ONE slice (the single-pass baseline, MSli §3/§5.4: the
 * transitive closure of the target statements under the combined dependence
 * graph). Both ILA and FSPTA run on this single slice, so V_ILA, V_PTA subset
 * V_Single. Used by the differential-slicing ablation (-slicing-single).
 *
 * Iteratively applies data dependence (over the thread-aware VFG_pre) and call
 * dependence until convergence, then a single dual-slicing pass.
 */
class SingleSlicer : public SlicerBase {
private:
    SVF::SVFG* vfg; ///< thread-aware VFG_pre (built once in pre-analysis)

public:
    SingleSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                 LockAnalysis* lockAnalysis, SVF::SVFG* vfg = nullptr);

    /**
     * Perform unified slicing combining synchronization, data, and call dependence.
     * @param vulnerableStatements Set of vulnerable statements to start slicing from
     * @return Set of ICFG nodes in the slice (including call/ret and entry/exit nodes)
     */
    std::set<const ICFGNode*> runSlicing(
        const std::set<const SVFStmt*>& vulnerableStatements);
};

} // namespace SVF

#endif // MTA_SLICER_H
