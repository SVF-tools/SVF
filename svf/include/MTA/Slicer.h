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
#include <Graphs/ICFG.h>
#include <Graphs/ICFGNode.h>
#include <Graphs/ICFGEdge.h>
#include <Graphs/CallGraph.h>
#include <Util/WorkList.h>
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
// SlicedICFGView - sliced view of the ICFG.
// Provides getSuccNodes()/getPredNodes() restricted to kept nodes, plus the
// bridged edges produced by contracting removed nodes.
//===----------------------------------------------------------------------===//
class SlicedICFGView {
public:
    /// Build complete ICFG view from keepNodes and keptFunctions
    SlicedICFGView(SVF::ICFG* icfg,
                   SVF::CallGraph* cg,
                   const std::set<const SVF::ICFGNode*>& keepNodes,
                   const std::set<const SVF::FunObjVar*>& keptFunctions);

    /// Get successor nodes (including bridged edges)
    void getSuccNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const;

    /// Get predecessor nodes (including bridged edges)
    void getPredNodes(const SVF::ICFGNode* node, std::vector<const SVF::ICFGNode*>& out) const;

    /// Check if a node is in the sliced view
    bool isKeptNode(const SVF::ICFGNode* node) const;

    /// Get all kept nodes
    const std::set<const SVF::ICFGNode*>& getKeptNodes() const { return keptNodes; }

    /// Dump sliced ICFG to dot file
    void dump(const std::string& filename) const;

    /// Get original ICFG
    SVF::ICFG* getOriginalICFG() const { return icfg; }

private:
    SVF::ICFG* icfg;
    std::set<const SVF::ICFGNode*> keptNodes;
    std::set<const SVF::ICFGEdge*> keptEdges;
    std::unordered_map<const SVF::ICFGNode*, std::set<const SVF::ICFGNode*>> bridgedEdges;
    // Reverse of bridgedEdges (dst -> srcs), so getPredNodes is O(preds) instead
    // of scanning every bridged edge.
    std::unordered_map<const SVF::ICFGNode*, std::set<const SVF::ICFGNode*>> bridgedPreds;
    std::unordered_set<const SVF::ICFGNode*> keptNodesSet; // For fast lookup

    void buildICFGSets(const std::set<const SVF::ICFGNode*>& keepNodes,
                       const std::set<const SVF::FunObjVar*>& keptFunctions);
    void buildBridgedEdges();
};

//===----------------------------------------------------------------------===//
// SlicedPAGView - a sliced view of the PAG, built from the kept statements.
//===----------------------------------------------------------------------===//
class SlicedPAGView {
public:
    SlicedPAGView(SVF::SVFIR* pag, const std::set<const SVF::SVFStmt*>& keptStmts);

    /// Get all kept statements
    const std::set<const SVF::SVFStmt*>& getKeptStmts() const { return keptStmts; }

    /// Dump the sliced PAG to a dot file
    void dump(const std::string& filename) const;

private:
    SVF::SVFIR* pag;
    std::set<const SVF::SVFStmt*> keptStmts;
    std::unordered_set<SVF::NodeID> keptNodeIds; // node IDs extracted from kept statements

    void buildKeptNodeIds();
};

//===----------------------------------------------------------------------===//
// SlicedThreadCallGraphView - sliced view of the ThreadCallGraph, built from the
// kept functions.
//===----------------------------------------------------------------------===//
class SlicedThreadCallGraphView {
public:
    SlicedThreadCallGraphView(SVF::ThreadCallGraph* tcg,
                              const std::set<const SVF::FunObjVar*>& keptFunctions,
                              const std::set<const SVF::ICFGNode*>& extendedKeptNodes = std::set<const SVF::ICFGNode*>());

    /// Get out edges of a node (only returns kept edges and target nodes)
    void getOutEdgesOf(const SVF::CallGraphNode* node, std::vector<const SVF::CallGraphEdge*>& out) const;

    /// Get in edges of a node (only returns kept edges and source nodes)
    void getInEdgesOf(const SVF::CallGraphNode* node, std::vector<const SVF::CallGraphEdge*>& out) const;

    /// Check if a node is in the sliced view
    bool isKeptNode(const SVF::CallGraphNode* node) const;

    /// Get all kept nodes
    const std::set<const SVF::CallGraphNode*>& getKeptNodes() const { return keptNodes; }

    const std::unordered_set<const SVF::FunObjVar*>& getKeptFunctions() const
    {
        return keptFunctionsSet;
    }

    /// Get all kept edges
    const SVF::CallGraph::CallGraphEdgeSet& getKeptEdges() const { return keptEdges; }

    /// Indirect call sites that lost all targets after filtering
    const std::unordered_set<const SVF::CallICFGNode*>& getIndirectSitesWithEmptyTargets() const {
        return indirectSitesWithEmptyTargets;
    }

    /// Dump sliced ThreadCallGraph to dot file
    void dump(const std::string& filename) const;

    /// Get original ThreadCallGraph
    SVF::ThreadCallGraph* getOriginalThreadCallGraph() const { return tcg; }

    /// Get original CallGraph (ThreadCallGraph inherits from CallGraph)
    SVF::CallGraph* getOriginalCallGraph() const { return tcg; }

private:
    SVF::ThreadCallGraph* tcg;
    std::set<const SVF::CallGraphNode*> keptNodes;
    SVF::CallGraph::CallGraphEdgeSet keptEdges;
    std::unordered_set<const SVF::FunObjVar*> keptFunctionsSet; // For fast lookup
    std::unordered_set<const SVF::CallICFGNode*> indirectSitesWithEmptyTargets;
    std::set<const SVF::ICFGNode*> extendedKeptNodes; // For checking if call sites are kept

    void buildKeptNodes();
    void buildCallGraphSets();
};

//===----------------------------------------------------------------------===//
// SlicedSVFIRView - unified facade over the three sliced graph views.
// The MTA pipeline always slices over a ThreadCallGraph.
//===----------------------------------------------------------------------===//
class SlicedSVFIRView {
public:
    SlicedSVFIRView(SVF::SVFIR* svfIr,
                    SVF::CallGraph* cg,
                    SVF::ICFG* icfg,
                    const std::set<const SVF::ICFGNode*>& keepNodes);

    /// Get SlicedICFGView
    const SlicedICFGView* getICFG() const { return icfgView.get(); }
    SlicedICFGView* getICFG() { return icfgView.get(); }

    /// Get SlicedPAGView
    const SlicedPAGView* getPAG() const { return pagView.get(); }
    SlicedPAGView* getPAG() { return pagView.get(); }

    /// Get SlicedThreadCallGraphView
    const SlicedThreadCallGraphView* getThreadCallGraph() const { return tcgView.get(); }
    SlicedThreadCallGraphView* getThreadCallGraph() { return tcgView.get(); }

    /// Get all kept functions
    const std::unordered_set<const SVF::FunObjVar*>& getKeptFunctions() const {
        return tcgView->getKeptFunctions();
    }

    /// Get indirect call sites that lost all targets after filtering
    const std::unordered_set<const SVF::CallICFGNode*>& getIndirectSitesWithEmptyTargets() const {
        return tcgView->getIndirectSitesWithEmptyTargets();
    }

    /// Get all kept statements
    const std::set<const SVF::SVFStmt*>& getKeptStatements() const {
        return pagView->getKeptStmts();
    }

    /// Dump all views to files
    void dumpAll(const std::string& prefix) const;

    /// Get original SVFIR
    SVF::SVFIR* getSVFIR() const { return svfIr; }

    /// Output statistics
    void dumpStats(const std::string& prefix = "") const;

private:
    SVF::SVFIR* svfIr;
    std::unique_ptr<SlicedICFGView> icfgView;
    std::unique_ptr<SlicedPAGView> pagView;
    std::unique_ptr<SlicedThreadCallGraphView> tcgView;
};

//===----------------------------------------------------------------------===//
// SlicedTCT - the Thread-Create-Tree rebuilt over a SlicedThreadCallGraphView.
//
// Inherits SVF::TCT and overrides the ThreadCallGraph-traversing steps to use the
// sliced view. It sits at the sliced-representation layer (built from the view,
// consumed by SlicedMHP / SlicedLockAnalysis), which is why it lives here with
// the views rather than beside the sliced analyses.
//===----------------------------------------------------------------------===//
class SlicedTCT : public SVF::TCT {
public:
    /// @param p PointerAnalysis (the shared Andersen pre-analysis)
    /// @param slicedView SlicedSVFIRView containing a SlicedThreadCallGraphView
    /// @param maxContextLen Max context length (0 = use Options::MaxContextLen())
    SlicedTCT(SVF::PointerAnalysis* p, const SlicedSVFIRView* slicedView, SVF::u32_t maxContextLen = 0);

    ~SlicedTCT() override = default;

    /// Override pushCxt to use the custom maxContextLen
    void pushCxt(SVF::CallStrCxt& cxt, const SVF::CallICFGNode* call, const SVF::FunObjVar* callee) override;

protected:
    void build() override;
    void markRelProcs() override;
    void markRelProcs(const SVF::FunObjVar* fun) override;
    void collectLoopInfoForJoin() override;
    void handleCallRelation(SVF::CxtThreadProc& ctp, const SVF::CallGraphEdge* cgEdge, const SVF::CallICFGNode* cs) override;

private:
    void collectEntryFunInCallGraph() override;

    const SlicedThreadCallGraphView* tcgView; // ThreadCallGraph view (from the sliced view)
    SVF::u32_t maxContextLen; // 0 means use Options::MaxContextLen()

    bool isKeptNode(const SVF::CallGraphNode* node) const;
    bool isKeptEdge(const SVF::CallGraphEdge* edge) const;
    void getKeptForkSites(std::vector<const SVF::ICFGNode*>& out) const;
    void getKeptJoinSites(std::vector<const SVF::ICFGNode*>& out) const;
};

//===----------------------------------------------------------------------===//
// SlicedViewAdapter - shared helpers for the sliced views and analyses.
//
// The traversal adapters route ICFG/CallGraph queries through the sliced view
// when one is supplied and fall back to the full graph when it is null; they are
// identical across the sliced analyses (SlicedMHP, SlicedLockAnalysis), which
// only differ in the SVF base they extend, so the logic lives here once instead
// of being copy-pasted into each override. escapeDotLabel() is the dot-rendering
// escape shared by the sliced views' dump().
//===----------------------------------------------------------------------===//
namespace SlicedViewAdapter
{
/// Escape a string for use as a Graphviz dot label.
std::string escapeDotLabel(const std::string& s);

/// First kept node of fun's entry block under the sliced view (original entry
/// if none kept, or if no view is supplied).
const SVF::ICFGNode* getFunEntry(const SlicedICFGView* icfgView, const SVF::FunObjVar* fun);

/// Kept ICFG nodes of fun (all of fun's nodes if no view is supplied).
void getFunICFGNodes(const SlicedICFGView* icfgView, const SVF::FunObjVar* fun,
                     std::vector<const SVF::ICFGNode*>& out);

/// Successors of node under the sliced view (full ICFG successors if no view).
void getSuccNodes(const SlicedICFGView* icfgView, const SVF::ICFGNode* node,
                  std::vector<const SVF::ICFGNode*>& out);

/// Project an interleaving seed onto kept nodes: the node itself if kept (or no
/// view), else the first kept node(s) reachable forward (intra-procedurally).
void projectSeedToKept(const SlicedICFGView* icfgView, const SVF::ICFGNode* node,
                       std::vector<const SVF::ICFGNode*>& out);

/// Predecessors of node under the sliced view (full ICFG predecessors if no view).
void getPredNodes(const SlicedICFGView* icfgView, const SVF::ICFGNode* node,
                  std::vector<const SVF::ICFGNode*>& out);

/// Whether node is kept by the sliced view (true if no view is supplied).
bool acceptsNode(const SlicedICFGView* icfgView, const SVF::ICFGNode* node);

/// In-edges of a call-graph node under the sliced view (full in-edges if none).
void getInEdgesOfCallGraphNode(const SlicedSVFIRView* slicedView, const SVF::CallGraphNode* node,
                               std::vector<const SVF::CallGraphEdge*>& out);
} // namespace SlicedViewAdapter

/**
 * SlicerBase - Base class for program slicing.
 *
 * Holds the shared helper methods and data members used by both concrete slicers
 * (MTASlicer, PTASlicer).
 */
class SlicerBase {
public:
    SlicerBase(SVFIR* svfIr, AndersenBase* pta, SVF::MHP* mhp,
               SVF::LockAnalysis* lockAnalysis, SVF::SVFG* vfg = nullptr);
    virtual ~SlicerBase();

protected:
    SVFIR* svfIr;
    AndersenBase* pta;
    SVF::MHP* mhp;
    SVF::LockAnalysis* lockAnalysis;
    CallGraph* callGraph;
    SVF::SVFG* vfg;   ///< thread-aware VFG_pre (PTA/Single slicers; null for MTA)

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
