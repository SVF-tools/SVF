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
 * SlicedGraphView.h -- general sliced views of the ICFG / PAG /
 * (Thread)CallGraph, and their shared dot-dump / traversal adapter. These are
 * program-graph views with no MTA dependency (moved out of MTA/Slicer.h).
 */

#ifndef GRAPHS_SLICEDGRAPHVIEW_H
#define GRAPHS_SLICEDGRAPHVIEW_H

#include <Graphs/ICFG.h>
#include <Graphs/ICFGNode.h>
#include <Graphs/ICFGEdge.h>
#include <Graphs/CallGraph.h>
#include <Graphs/ThreadCallGraph.h>
#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>

namespace SVF
{

//===----------------------------------------------------------------------===//
// SlicedICFGView - sliced view of the ICFG.
// Provides getSuccNodes()/getPredNodes() restricted to kept nodes, plus the
// bridged edges produced by contracting removed nodes.
//===----------------------------------------------------------------------===//
class SlicedICFGView {
public:
    /// Build complete ICFG view from keepNodes and keptFunctions
    // buildBridged=false skips bridged-edge construction for views whose control
    // flow is never walked (e.g. the FSPTA view, used only for isKeptNode).
    SlicedICFGView(SVF::ICFG* icfg,
                   SVF::CallGraph* cg,
                   const std::set<const SVF::ICFGNode*>& keepNodes,
                   const std::set<const SVF::FunObjVar*>& keptFunctions,
                   bool buildBridged = true);

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
                    const std::set<const SVF::ICFGNode*>& keepNodes,
                    bool buildBridged = true);

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

/// Dot dumping shared by the three sliced views (ICFG/PAG/ThreadCallGraph).
//@{
/// Open <filename>.dot and write the `digraph` header. Returns false (logging an
/// error) if the file cannot be opened.
bool openDotGraph(std::ofstream& out, const std::string& filename,
                  const char* graphName, const char* rankdir, const char* nodeAttr);
/// Emit one node line:  <idPrefix><id> [label="<escaped label>"];
void emitDotNode(std::ofstream& out, const char* idPrefix, SVF::NodeID id, const std::string& label);
/// Close the graph, the stream, and log "[<tag>] <what> dumped to <filename>.dot".
void finishDotGraph(std::ofstream& out, const std::string& filename, const char* tag, const char* what);
//@}

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

/// The CallGraph the sliced analyses scan: the sliced ThreadCallGraph's original
/// CallGraph if a view is supplied, else the full PAG CallGraph.
const SVF::CallGraph* getAnalysisCallGraph(const SlicedSVFIRView* slicedView);
} // namespace SlicedViewAdapter

} // namespace SVF

#endif // GRAPHS_SLICEDGRAPHVIEW_H
