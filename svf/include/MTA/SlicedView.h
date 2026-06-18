//===- SlicedView.h -- Sliced views of the SVFIR graphs -------------------===//
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
 * SlicedView.h
 *
 * Filtered ("sliced") views over the SVFIR graphs. A slice is a set of kept ICFG
 * nodes; these views present the ICFG / PAG / ThreadCallGraph restricted to that
 * set (with bridged edges across removed nodes), without mutating the originals.
 *   - SlicedICFGView            : kept ICFG nodes/edges + bridged edges
 *   - SlicedPAGView             : PAG restricted to the kept statements
 *   - SlicedThreadCallGraphView : call graph restricted to kept functions
 *   - SlicedSVFIRView           : facade bundling the three above
 *   - SlicedViewAdapter         : shared traversal helpers + dot-label escaping
 */

#ifndef MTA_SLICEDVIEW_H
#define MTA_SLICEDVIEW_H

#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include <Graphs/ICFG.h>
#include <Graphs/ICFGNode.h>
#include <Graphs/ICFGEdge.h>
#include <Graphs/CallGraph.h>
#include <Graphs/ThreadCallGraph.h>
#include <Util/WorkList.h>   // FIFOWorkList, before TCT.h (TCT.h -> SCC.h uses it)
#include <MTA/TCT.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SVF
{

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
    SVF::ICFG* getICFG() const { return icfg; }

private:
    SVF::ICFG* icfg;
    SVF::CallGraph* callGraph; // Optional, for building complete view
    std::set<const SVF::ICFGNode*> keptNodes;
    std::set<const SVF::ICFGEdge*> keptEdges;
    std::unordered_map<const SVF::ICFGNode*, std::set<const SVF::ICFGNode*>> bridgedEdges;
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

    const std::set<const SVF::FunObjVar*> getKeptFunctions() const
    {
        std::set<const SVF::FunObjVar*> keptFunctions;
        keptFunctions.insert(keptFunctionsSet.begin(), keptFunctionsSet.end());
        return keptFunctions;
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
    SVF::ThreadCallGraph* getThreadCallGraph() const { return tcg; }

    /// Get original CallGraph (ThreadCallGraph inherits from CallGraph)
    SVF::CallGraph* getCallGraph() const { return tcg; }

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
    const std::set<const SVF::FunObjVar*> getKeptFunctions() const {
        return tcgView->getKeptFunctions();
    }

    /// Get indirect call sites that lost all targets after filtering
    const std::unordered_set<const SVF::CallICFGNode*>& getIndirectSitesWithEmptyTargets() const {
        return tcgView->getIndirectSitesWithEmptyTargets();
    }

    /// Get all kept statements
    const std::set<const SVF::SVFStmt*> getKeptStatements() const {
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
    SVF::CallGraph* callGraph;
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

    const SlicedSVFIRView* slicedView;
    const SlicedThreadCallGraphView* tcgView; // ThreadCallGraph view (from slicedView)
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

/// Predecessors of node under the sliced view (full ICFG predecessors if no view).
void getPredNodes(const SlicedICFGView* icfgView, const SVF::ICFGNode* node,
                  std::vector<const SVF::ICFGNode*>& out);

/// Whether node is kept by the sliced view (true if no view is supplied).
bool acceptsNode(const SlicedICFGView* icfgView, const SVF::ICFGNode* node);

/// In-edges of a call-graph node under the sliced view (full in-edges if none).
void getInEdgesOfCallGraphNode(const SlicedSVFIRView* slicedView, const SVF::CallGraphNode* node,
                               std::vector<const SVF::CallGraphEdge*>& out);
} // namespace SlicedViewAdapter

} // namespace SVF

#endif // MTA_SLICEDVIEW_H
