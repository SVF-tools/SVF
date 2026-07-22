//===- SlicedGraphs.h -- General sliced graph views ------------------------===//
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
 * SlicedGraphs.h
 *
 *      Author: Jiawei Yang
 *
 * General non-owning sliced views of the ICFG, PAG, and (Thread)CallGraph, and
 * their GenericGraphTraits specialisations, so a slice works with SVF's generic
 * graph algorithms and GraphWriter. Program-graph views with no MTA dependency.
 */

#ifndef GRAPHS_SLICEDGRAPHS_H
#define GRAPHS_SLICEDGRAPHS_H

#include "Graphs/ICFG.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/ICFGEdge.h"
#include "Graphs/CallGraph.h"
#include "Graphs/ThreadCallGraph.h"
#include "Graphs/SVFG.h"
#include "Graphs/SVFGNode.h"
#include "Graphs/GraphTraits.h"
#include "Graphs/GraphTraitsModels.h"
#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFVariables.h"
#include <cstddef>
#include <fstream>
#include <iterator>
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
class SlicedICFGView
{
public:
    /// Build complete ICFG view from keepNodes and keptFunctions
    // buildBridged=false skips bridged-edge construction for views whose control
    // flow is never walked (e.g. the FSPTA view, used only for isKeptNode).
    SlicedICFGView(ICFG* icfg,
                   CallGraph* cg,
                   const OrderedSet<const ICFGNode*>& keepNodes,
                   const OrderedSet<const FunObjVar*>& keptFunctions,
                   bool buildBridged = true);

    /// Get successor nodes (including bridged edges)
    void getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const;

    /// Get predecessor nodes (including bridged edges)
    void getPredNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const;

    /// Check if a node is in the sliced view
    bool isKeptNode(const ICFGNode* node) const;

    /// First kept node of fun's entry (the kept FunEntryICFGNode, else the first
    /// kept node of the entry block, else the original entry).
    const ICFGNode* getFunEntry(const FunObjVar* fun) const;

    /// Kept ICFG nodes of fun.
    void getFunICFGNodes(const FunObjVar* fun, std::vector<const ICFGNode*>& out) const;

    /// Get all kept nodes
    inline const OrderedSet<const ICFGNode*>& getKeptNodes() const
    {
        return keptNodes;
    }

    /// Bridged (synthetic) successors/predecessors of a kept node, or null if none.
    /// The traits iterators use these alongside the node's kept original edges.
    inline const OrderedSet<const ICFGNode*>* bridgedSuccsOf(const ICFGNode* n) const
    {
        auto it = bridgedEdges.find(n);
        return it == bridgedEdges.end() ? nullptr : &it->second;
    }
    inline const OrderedSet<const ICFGNode*>* bridgedPredsOf(const ICFGNode* n) const
    {
        auto it = bridgedPreds.find(n);
        return it == bridgedPreds.end() ? nullptr : &it->second;
    }

    /// Dump sliced ICFG to dot file
    void dump(const std::string& filename) const;

    /// Get original ICFG
    inline ICFG* getOriginalICFG() const
    {
        return icfg;
    }

private:
    ICFG* icfg;
    OrderedSet<const ICFGNode*> keptNodes;
    OrderedSet<const ICFGEdge*> keptEdges;
    Map<const ICFGNode*, OrderedSet<const ICFGNode*>> bridgedEdges;
    // Reverse of bridgedEdges (dst -> srcs), so getPredNodes is O(preds) instead
    // of scanning every bridged edge.
    Map<const ICFGNode*, OrderedSet<const ICFGNode*>> bridgedPreds;
    Set<const ICFGNode*> keptNodesSet; // For fast lookup

    void buildICFGSets(const OrderedSet<const ICFGNode*>& keepNodes,
                       const OrderedSet<const FunObjVar*>& keptFunctions);
    void buildBridgedEdges();
};

//===----------------------------------------------------------------------===//
// SlicedPAGView - a sliced view of the PAG, built from the kept statements.
//===----------------------------------------------------------------------===//
class SlicedPAGView
{
public:
    SlicedPAGView(SVFIR* pag, const OrderedSet<const SVFStmt*>& keptStmts);

    /// Get all kept statements
    inline const OrderedSet<const SVFStmt*>& getKeptStmts() const
    {
        return keptStmts;
    }

    /// Node IDs (SVFVars) touched by the kept statements -- the sliced PAG's nodes.
    inline const Set<NodeID>& getKeptNodeIds() const
    {
        return keptNodeIds;
    }

    /// Canonical edge membership: a kept SVFVar may have incident statements not
    /// in the slice, so traits/queries keep only statements in keptStmts.
    inline bool isKeptStmt(const SVFStmt* s) const
    {
        return keptStmts.count(s) > 0;
    }

    /// The underlying SVFIR (to resolve node ids to SVFVars).
    inline SVFIR* getSVFIR() const
    {
        return pag;
    }

    /// Dump the sliced PAG to a dot file
    void dump(const std::string& filename) const;

private:
    SVFIR* pag;
    OrderedSet<const SVFStmt*> keptStmts;
    Set<NodeID> keptNodeIds; // node IDs extracted from kept statements

    void buildKeptNodeIds();
};

//===----------------------------------------------------------------------===//
// SlicedThreadCallGraphView - sliced view of the ThreadCallGraph, built from the
// kept functions.
//===----------------------------------------------------------------------===//
class SlicedThreadCallGraphView
{
public:
    SlicedThreadCallGraphView(ThreadCallGraph* tcg,
                              const OrderedSet<const FunObjVar*>& keptFunctions,
                              const OrderedSet<const ICFGNode*>& extendedKeptNodes = OrderedSet<const ICFGNode*>());

    /// Get out edges of a node (only returns kept edges and target nodes)
    void getOutEdgesOf(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const;

    /// Get in edges of a node (only returns kept edges and source nodes)
    void getInEdgesOf(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const;

    /// Check if a node is in the sliced view
    bool isKeptNode(const CallGraphNode* node) const;

    /// Get all kept nodes
    inline const OrderedSet<const CallGraphNode*>& getKeptNodes() const
    {
        return keptNodes;
    }

    const Set<const FunObjVar*>& getKeptFunctions() const
    {
        return keptFunctionsSet;
    }

    /// Get all kept edges
    inline const CallGraph::CallGraphEdgeSet& getKeptEdges() const
    {
        return keptEdges;
    }

    /// Canonical edge membership: keptEdges excludes edges whose call site was
    /// pruned, so endpoint checks alone are not enough (queries/traits use this).
    inline bool isKeptEdge(const CallGraphEdge* e) const
    {
        return keptEdges.find(const_cast<CallGraphEdge*>(e)) != keptEdges.end();
    }

    /// Indirect call sites that lost all targets after filtering
    inline const Set<const CallICFGNode*>& getIndirectSitesWithEmptyTargets() const
    {
        return indirectSitesWithEmptyTargets;
    }

    /// Dump sliced ThreadCallGraph to dot file
    void dump(const std::string& filename) const;

    /// Get original ThreadCallGraph
    inline ThreadCallGraph* getOriginalThreadCallGraph() const
    {
        return tcg;
    }

    /// Get original CallGraph (ThreadCallGraph inherits from CallGraph)
    inline CallGraph* getOriginalCallGraph() const
    {
        return tcg;
    }

private:
    ThreadCallGraph* tcg;
    OrderedSet<const CallGraphNode*> keptNodes;
    CallGraph::CallGraphEdgeSet keptEdges;
    Set<const FunObjVar*> keptFunctionsSet; // For fast lookup
    Set<const CallICFGNode*> indirectSitesWithEmptyTargets;
    OrderedSet<const ICFGNode*> extendedKeptNodes; // For checking if call sites are kept

    void buildKeptNodes();
    void buildCallGraphSets();
};

//===----------------------------------------------------------------------===//
// SlicedSVFGView - non-owning sliced view of the (thread-aware) SVFG.
// Retained nodes: everything except Load/Store statement nodes whose ICFG node
// was sliced out; structural nodes (Addr/Copy/Gep, MSSA phi/mu/chi, call/return
// boundaries) always remain so inter-procedural and memory-SSA flow is intact.
// Deliberately NO bridge edges: bridging removed SVFG nodes could fabricate
// value-flow paths that do not exist in the original graph. Removed nodes act
// as propagation barriers (matching the sliced FSAM solve).
//===----------------------------------------------------------------------===//
class SlicedSVFGView
{
public:
    /// Membership needs only the sliced ICFG view; the SVFG handle (for node
    /// iteration and dumping) may be bound later, once the solver has built it.
    explicit SlicedSVFGView(const SlicedICFGView* icfgView, const SVFG* svfg = nullptr)
        : icfgView(icfgView), svfg(svfg) {}

    /// Whether the node is retained (see the class comment for the rule).
    bool isKeptNode(const SVFGNode* n) const;

    /// Whether the edge is retained: both endpoints kept (no bridges).
    inline bool isKeptEdge(const SVFGEdge* e) const
    {
        return isKeptNode(e->getSrcNode()) && isKeptNode(e->getDstNode());
    }

    inline const SlicedICFGView* getICFGView() const
    {
        return icfgView;
    }
    inline const SVFG* getSVFG() const
    {
        return svfg;
    }
    /// Bind the underlying SVFG (enables node iteration and dumping).
    inline void setSVFG(const SVFG* g)
    {
        svfg = g;
    }

    /// Dump the sliced SVFG (retained nodes/edges only) via GraphWriter.
    void dump(const std::string& filename) const;

private:
    const SlicedICFGView* icfgView;
    const SVFG* svfg;
};

//===----------------------------------------------------------------------===//
// SlicedSVFIRView - unified facade over the three sliced graph views.
// The MTA pipeline always slices over a ThreadCallGraph.
//===----------------------------------------------------------------------===//
class SlicedSVFIRView
{
public:
    SlicedSVFIRView(SVFIR* svfIr,
                    CallGraph* cg,
                    ICFG* icfg,
                    const OrderedSet<const ICFGNode*>& keepNodes,
                    bool buildBridged = true);

    /// Get SlicedICFGView
    inline const SlicedICFGView* getICFG() const
    {
        return icfgView.get();
    }
    inline SlicedICFGView* getICFG()
    {
        return icfgView.get();
    }

    /// Get SlicedPAGView
    inline const SlicedPAGView* getPAG() const
    {
        return pagView.get();
    }
    inline SlicedPAGView* getPAG()
    {
        return pagView.get();
    }

    /// Get SlicedThreadCallGraphView
    inline const SlicedThreadCallGraphView* getThreadCallGraph() const
    {
        return tcgView.get();
    }
    inline SlicedThreadCallGraphView* getThreadCallGraph()
    {
        return tcgView.get();
    }

    /// Get all kept functions
    inline const Set<const FunObjVar*>& getKeptFunctions() const
    {
        return tcgView->getKeptFunctions();
    }

    /// Get indirect call sites that lost all targets after filtering
    inline const Set<const CallICFGNode*>& getIndirectSitesWithEmptyTargets() const
    {
        return tcgView->getIndirectSitesWithEmptyTargets();
    }

    /// Get all kept statements
    inline const OrderedSet<const SVFStmt*>& getKeptStatements() const
    {
        return pagView->getKeptStmts();
    }

    /// In-edges of a call-graph node under the sliced ThreadCallGraph view (the
    /// full node in-edges if no TCG view was built).
    void getInEdgesOfCallGraphNode(const CallGraphNode* node,
                                   std::vector<const CallGraphEdge*>& out) const;

    /// The CallGraph the sliced analyses scan: the sliced ThreadCallGraph's
    /// original CallGraph, or the full PAG CallGraph if no TCG view was built.
    const CallGraph* getAnalysisCallGraph() const;

    /// Dump all views to files
    void dumpAll(const std::string& prefix) const;

    /// Get original SVFIR
    inline SVFIR* getSVFIR() const
    {
        return svfIr;
    }

    /// Output statistics
    void dumpStats(const std::string& prefix = "") const;

private:
    SVFIR* svfIr;
    std::unique_ptr<SlicedICFGView> icfgView;
    std::unique_ptr<SlicedPAGView> pagView;
    std::unique_ptr<SlicedThreadCallGraphView> tcgView;
};

//===----------------------------------------------------------------------===//
// GenericGraphTraits for the sliced leaf views. The views do NOT inherit
// GenericGraph (they are non-owning); these stateless specializations make them
// usable with SVF's generic graph algorithms and GraphWriter. The NodeRef is a
// light value carrying (view, raw-node) so the same raw node under two different
// slices is a distinct node. (Kept in this header next to the views, as ICFG.h
// keeps GenericGraphTraits<ICFG*>.)
//===----------------------------------------------------------------------===//

// Contextual value NodeRef: a raw node paired with the view it is viewed under.
template <class ViewT, class RawNodeT>
struct SlicedNodeRef
{
    const ViewT* view = nullptr;
    const RawNodeT* raw = nullptr;

    SlicedNodeRef() = default;
    SlicedNodeRef(const ViewT* v, const RawNodeT* r) : view(v), raw(r) {}

    explicit operator bool() const { return raw != nullptr; }

    friend bool operator==(SlicedNodeRef lhs, SlicedNodeRef rhs)
    {
        return lhs.view == rhs.view && lhs.raw == rhs.raw;
    }
    friend bool operator!=(SlicedNodeRef lhs, SlicedNodeRef rhs) { return !(lhs == rhs); }
};

using SlicedICFGNodeRef = SlicedNodeRef<SlicedICFGView, ICFGNode>;

// A sliced ICFG edge. underlying==nullptr && bridged==true for synthetic bridges.
struct SlicedICFGEdgeRef
{
    SlicedICFGNodeRef src;
    SlicedICFGNodeRef dst;
    const ICFGEdge* underlying = nullptr;
    bool bridged = false;
};

// Forward/reverse child-edge iterator over a kept node: first its kept original
// edges, then its bridged edges. Forward==true walks out-edges/bridgedSuccs;
// Forward==false walks in-edges/bridgedPreds. A standard forward iterator.
template <bool Forward>
class SlicedICFGEdgeIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedICFGEdgeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedICFGEdgeRef*;
    using reference = const SlicedICFGEdgeRef&;

    SlicedICFGEdgeIterImpl() = default;

    static SlicedICFGEdgeIterImpl begin(const SlicedICFGView* v, const ICFGNode* n)
    {
        SlicedICFGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.begin();
        it.realEnd = edges.end();
        it.bridged = Forward ? v->bridgedSuccsOf(n) : v->bridgedPredsOf(n);
        it.brIt = it.bridged ? it.bridged->begin() : emptySet().begin();
        it.brEnd = it.bridged ? it.bridged->end() : emptySet().end();
        it.skipNonKeptReal();
        it.refresh();
        return it;
    }
    static SlicedICFGEdgeIterImpl end(const SlicedICFGView* v, const ICFGNode* n)
    {
        SlicedICFGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.end();
        it.realEnd = edges.end();
        it.bridged = Forward ? v->bridgedSuccsOf(n) : v->bridgedPredsOf(n);
        it.brIt = it.bridged ? it.bridged->end() : emptySet().end();
        it.brEnd = it.bridged ? it.bridged->end() : emptySet().end();
        return it;
    }

    reference operator*() const { return cur; }
    pointer operator->() const { return &cur; }

    // The node this edge traverses to (successor for Forward, predecessor for Inverse).
    SlicedICFGNodeRef target() const { return Forward ? cur.dst : cur.src; }

    SlicedICFGEdgeIterImpl& operator++()
    {
        if (realIt != realEnd) { ++realIt; skipNonKeptReal(); }
        else if (brIt != brEnd) { ++brIt; }
        refresh();
        return *this;
    }
    SlicedICFGEdgeIterImpl operator++(int) { SlicedICFGEdgeIterImpl t = *this; ++*this; return t; }

    friend bool operator==(const SlicedICFGEdgeIterImpl& a, const SlicedICFGEdgeIterImpl& b)
    {
        return a.view == b.view && a.src == b.src && a.realIt == b.realIt && a.brIt == b.brIt;
    }
    friend bool operator!=(const SlicedICFGEdgeIterImpl& a, const SlicedICFGEdgeIterImpl& b)
    {
        return !(a == b);
    }

private:
    using EdgeIt = ICFGNode::const_iterator;
    using BrIt = OrderedSet<const ICFGNode*>::const_iterator;

    const SlicedICFGView* view = nullptr;
    const ICFGNode* src = nullptr;
    EdgeIt realIt{}, realEnd{};
    const OrderedSet<const ICFGNode*>* bridged = nullptr;
    BrIt brIt{}, brEnd{};
    SlicedICFGEdgeRef cur{};

    SlicedICFGEdgeIterImpl(const SlicedICFGView* v, const ICFGNode* n) : view(v), src(n) {}

    static const OrderedSet<const ICFGNode*>& emptySet()
    {
        static const OrderedSet<const ICFGNode*> e;
        return e;
    }
    static const ICFGNode* other(const ICFGEdge* e)
    {
        return Forward ? e->getDstNode() : e->getSrcNode();
    }
    void skipNonKeptReal()
    {
        while (realIt != realEnd && !view->isKeptNode(other(*realIt))) ++realIt;
    }
    void refresh()
    {
        if (realIt != realEnd)
        {
            const ICFGEdge* e = *realIt;
            SlicedICFGNodeRef a{view, e->getSrcNode()}, b{view, e->getDstNode()};
            cur = SlicedICFGEdgeRef{a, b, e, false};
        }
        else if (brIt != brEnd)
        {
            SlicedICFGNodeRef self{view, src}, adj{view, *brIt};
            cur = Forward ? SlicedICFGEdgeRef{self, adj, nullptr, true}
                          : SlicedICFGEdgeRef{adj, self, nullptr, true};
        }
        else
        {
            cur = SlicedICFGEdgeRef{};
        }
    }
};

// Node-child iterator: adapts the edge iterator, dereferencing to the traversed
// node; currentEdge() exposes the domain edge (for bridged-aware DOT styling).
template <bool Forward>
class SlicedICFGChildIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedICFGNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedICFGNodeRef*;
    using reference = SlicedICFGNodeRef;

    SlicedICFGChildIterImpl() = default;
    explicit SlicedICFGChildIterImpl(SlicedICFGEdgeIterImpl<Forward> it) : e(it) {}

    reference operator*() const { return e.target(); }
    const SlicedICFGEdgeRef& currentEdge() const { return *e; }

    SlicedICFGChildIterImpl& operator++() { ++e; return *this; }
    SlicedICFGChildIterImpl operator++(int) { SlicedICFGChildIterImpl t = *this; ++e; return t; }

    friend bool operator==(const SlicedICFGChildIterImpl& a, const SlicedICFGChildIterImpl& b)
    {
        return a.e == b.e;
    }
    friend bool operator!=(const SlicedICFGChildIterImpl& a, const SlicedICFGChildIterImpl& b)
    {
        return !(a == b);
    }

private:
    SlicedICFGEdgeIterImpl<Forward> e{};
};

// Node iterator over the kept-node set, yielding contextual NodeRefs.
class SlicedICFGNodeIter
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedICFGNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedICFGNodeRef*;
    using reference = SlicedICFGNodeRef;

    SlicedICFGNodeIter() = default;
    SlicedICFGNodeIter(const SlicedICFGView* v, OrderedSet<const ICFGNode*>::const_iterator i)
        : view(v), it(i) {}

    reference operator*() const { return SlicedICFGNodeRef{view, *it}; }
    SlicedICFGNodeIter& operator++() { ++it; return *this; }
    SlicedICFGNodeIter operator++(int) { SlicedICFGNodeIter t = *this; ++it; return t; }

    friend bool operator==(const SlicedICFGNodeIter& a, const SlicedICFGNodeIter& b)
    {
        return a.it == b.it;
    }
    friend bool operator!=(const SlicedICFGNodeIter& a, const SlicedICFGNodeIter& b)
    {
        return !(a == b);
    }

private:
    const SlicedICFGView* view = nullptr;
    OrderedSet<const ICFGNode*>::const_iterator it{};
};

//===----------------------------------------------------------------------===//
// SlicedThreadCallGraphView traits support (no bridged edges; canonical
// adjacency is the kept-edge set, so pruned-call-site edges never appear).
//===----------------------------------------------------------------------===//

using SlicedCallGraphNodeRef = SlicedNodeRef<SlicedThreadCallGraphView, CallGraphNode>;

struct SlicedCallGraphEdgeRef
{
    SlicedCallGraphNodeRef src;
    SlicedCallGraphNodeRef dst;
    const CallGraphEdge* underlying = nullptr;
};

// Forward==true walks kept out-edges; Forward==false walks kept in-edges. Join
// edges are not part of the node's normal adjacency, so they never appear here.
template <bool Forward>
class SlicedCGEdgeIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedCallGraphEdgeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedCallGraphEdgeRef*;
    using reference = const SlicedCallGraphEdgeRef&;

    SlicedCGEdgeIterImpl() = default;

    static SlicedCGEdgeIterImpl begin(const SlicedThreadCallGraphView* v, const CallGraphNode* n)
    {
        SlicedCGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.begin();
        it.realEnd = edges.end();
        it.skipNonKept();
        it.refresh();
        return it;
    }
    static SlicedCGEdgeIterImpl end(const SlicedThreadCallGraphView* v, const CallGraphNode* n)
    {
        SlicedCGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.end();
        it.realEnd = edges.end();
        return it;
    }

    reference operator*() const { return cur; }
    pointer operator->() const { return &cur; }
    SlicedCallGraphNodeRef target() const { return Forward ? cur.dst : cur.src; }

    SlicedCGEdgeIterImpl& operator++()
    {
        if (realIt != realEnd) { ++realIt; skipNonKept(); }
        refresh();
        return *this;
    }
    SlicedCGEdgeIterImpl operator++(int) { SlicedCGEdgeIterImpl t = *this; ++*this; return t; }

    friend bool operator==(const SlicedCGEdgeIterImpl& a, const SlicedCGEdgeIterImpl& b)
    {
        return a.view == b.view && a.src == b.src && a.realIt == b.realIt;
    }
    friend bool operator!=(const SlicedCGEdgeIterImpl& a, const SlicedCGEdgeIterImpl& b) { return !(a == b); }

private:
    using EdgeIt = CallGraphNode::const_iterator;
    const SlicedThreadCallGraphView* view = nullptr;
    const CallGraphNode* src = nullptr;
    EdgeIt realIt{}, realEnd{};
    SlicedCallGraphEdgeRef cur{};

    SlicedCGEdgeIterImpl(const SlicedThreadCallGraphView* v, const CallGraphNode* n) : view(v), src(n) {}

    void skipNonKept()
    {
        while (realIt != realEnd && !view->isKeptEdge(*realIt)) ++realIt;
    }
    void refresh()
    {
        if (realIt != realEnd)
        {
            const CallGraphEdge* e = *realIt;
            cur = SlicedCallGraphEdgeRef{{view, e->getSrcNode()}, {view, e->getDstNode()}, e};
        }
        else
        {
            cur = SlicedCallGraphEdgeRef{};
        }
    }
};

template <bool Forward>
class SlicedCGChildIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedCallGraphNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedCallGraphNodeRef*;
    using reference = SlicedCallGraphNodeRef;

    SlicedCGChildIterImpl() = default;
    explicit SlicedCGChildIterImpl(SlicedCGEdgeIterImpl<Forward> it) : e(it) {}

    reference operator*() const { return e.target(); }
    const SlicedCallGraphEdgeRef& currentEdge() const { return *e; }
    SlicedCGChildIterImpl& operator++() { ++e; return *this; }
    SlicedCGChildIterImpl operator++(int) { SlicedCGChildIterImpl t = *this; ++e; return t; }
    friend bool operator==(const SlicedCGChildIterImpl& a, const SlicedCGChildIterImpl& b) { return a.e == b.e; }
    friend bool operator!=(const SlicedCGChildIterImpl& a, const SlicedCGChildIterImpl& b) { return !(a == b); }

private:
    SlicedCGEdgeIterImpl<Forward> e{};
};

class SlicedCGNodeIter
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedCallGraphNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedCallGraphNodeRef*;
    using reference = SlicedCallGraphNodeRef;

    SlicedCGNodeIter() = default;
    SlicedCGNodeIter(const SlicedThreadCallGraphView* v, OrderedSet<const CallGraphNode*>::const_iterator i)
        : view(v), it(i) {}

    reference operator*() const { return SlicedCallGraphNodeRef{view, *it}; }
    SlicedCGNodeIter& operator++() { ++it; return *this; }
    SlicedCGNodeIter operator++(int) { SlicedCGNodeIter t = *this; ++it; return t; }
    friend bool operator==(const SlicedCGNodeIter& a, const SlicedCGNodeIter& b) { return a.it == b.it; }
    friend bool operator!=(const SlicedCGNodeIter& a, const SlicedCGNodeIter& b) { return !(a == b); }

private:
    const SlicedThreadCallGraphView* view = nullptr;
    OrderedSet<const CallGraphNode*>::const_iterator it{};
};

//===----------------------------------------------------------------------===//
// SlicedPAGView traits support. Nodes are the SVFVars touched by kept statements;
// edges are the kept SVFStmts (a kept var may have non-kept incident statements,
// so keptStmts membership is the canonical filter). MultiOpndStmts use the
// underlying SVFStmt src/dst (first operand -> result), not an operand fan-out.
//===----------------------------------------------------------------------===//

using SlicedPAGNodeRef = SlicedNodeRef<SlicedPAGView, SVFVar>;

struct SlicedPAGEdgeRef
{
    SlicedPAGNodeRef src;
    SlicedPAGNodeRef dst;
    const SVFStmt* underlying = nullptr;
};

template <bool Forward>
class SlicedPAGEdgeIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedPAGEdgeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedPAGEdgeRef*;
    using reference = const SlicedPAGEdgeRef&;

    SlicedPAGEdgeIterImpl() = default;

    static SlicedPAGEdgeIterImpl begin(const SlicedPAGView* v, const SVFVar* n)
    {
        SlicedPAGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.begin();
        it.realEnd = edges.end();
        it.skipNonKept();
        it.refresh();
        return it;
    }
    static SlicedPAGEdgeIterImpl end(const SlicedPAGView* v, const SVFVar* n)
    {
        SlicedPAGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.end();
        it.realEnd = edges.end();
        return it;
    }

    reference operator*() const { return cur; }
    pointer operator->() const { return &cur; }
    SlicedPAGNodeRef target() const { return Forward ? cur.dst : cur.src; }

    SlicedPAGEdgeIterImpl& operator++()
    {
        if (realIt != realEnd) { ++realIt; skipNonKept(); }
        refresh();
        return *this;
    }
    SlicedPAGEdgeIterImpl operator++(int) { SlicedPAGEdgeIterImpl t = *this; ++*this; return t; }

    friend bool operator==(const SlicedPAGEdgeIterImpl& a, const SlicedPAGEdgeIterImpl& b)
    {
        return a.view == b.view && a.src == b.src && a.realIt == b.realIt;
    }
    friend bool operator!=(const SlicedPAGEdgeIterImpl& a, const SlicedPAGEdgeIterImpl& b) { return !(a == b); }

private:
    using EdgeIt = SVFVar::const_iterator;
    const SlicedPAGView* view = nullptr;
    const SVFVar* src = nullptr;
    EdgeIt realIt{}, realEnd{};
    SlicedPAGEdgeRef cur{};

    SlicedPAGEdgeIterImpl(const SlicedPAGView* v, const SVFVar* n) : view(v), src(n) {}

    void skipNonKept()
    {
        while (realIt != realEnd && !view->isKeptStmt(*realIt)) ++realIt;
    }
    void refresh()
    {
        if (realIt != realEnd)
        {
            const SVFStmt* s = *realIt;
            cur = SlicedPAGEdgeRef{{view, s->getSrcNode()}, {view, s->getDstNode()}, s};
        }
        else
        {
            cur = SlicedPAGEdgeRef{};
        }
    }
};

template <bool Forward>
class SlicedPAGChildIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedPAGNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedPAGNodeRef*;
    using reference = SlicedPAGNodeRef;

    SlicedPAGChildIterImpl() = default;
    explicit SlicedPAGChildIterImpl(SlicedPAGEdgeIterImpl<Forward> it) : e(it) {}

    reference operator*() const { return e.target(); }
    const SlicedPAGEdgeRef& currentEdge() const { return *e; }
    SlicedPAGChildIterImpl& operator++() { ++e; return *this; }
    SlicedPAGChildIterImpl operator++(int) { SlicedPAGChildIterImpl t = *this; ++e; return t; }
    friend bool operator==(const SlicedPAGChildIterImpl& a, const SlicedPAGChildIterImpl& b) { return a.e == b.e; }
    friend bool operator!=(const SlicedPAGChildIterImpl& a, const SlicedPAGChildIterImpl& b) { return !(a == b); }

private:
    SlicedPAGEdgeIterImpl<Forward> e{};
};

// Node iterator over the kept SVFVar ids (resolved against the SVFIR).
class SlicedPAGNodeIter
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedPAGNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedPAGNodeRef*;
    using reference = SlicedPAGNodeRef;

    SlicedPAGNodeIter() = default;
    SlicedPAGNodeIter(const SlicedPAGView* v, Set<NodeID>::const_iterator i)
        : view(v), it(i) {}

    reference operator*() const
    {
        return SlicedPAGNodeRef{view, view->getSVFIR()->getGNode(*it)};
    }
    SlicedPAGNodeIter& operator++() { ++it; return *this; }
    SlicedPAGNodeIter operator++(int) { SlicedPAGNodeIter t = *this; ++it; return t; }
    friend bool operator==(const SlicedPAGNodeIter& a, const SlicedPAGNodeIter& b) { return a.it == b.it; }
    friend bool operator!=(const SlicedPAGNodeIter& a, const SlicedPAGNodeIter& b) { return !(a == b); }

private:
    const SlicedPAGView* view = nullptr;
    Set<NodeID>::const_iterator it{};
};

//===----------------------------------------------------------------------===//
// SlicedSVFGView traits support (no bridged edges; membership is the view's
// retained-node rule, so removed nodes are propagation barriers).
//===----------------------------------------------------------------------===//

using SlicedSVFGNodeRef = SlicedNodeRef<SlicedSVFGView, SVFGNode>;

struct SlicedSVFGEdgeRef
{
    SlicedSVFGNodeRef src;
    SlicedSVFGNodeRef dst;
    const SVFGEdge* underlying = nullptr;
};

// Forward==true walks kept out-edges; Forward==false walks kept in-edges.
template <bool Forward>
class SlicedSVFGEdgeIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedSVFGEdgeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedSVFGEdgeRef*;
    using reference = const SlicedSVFGEdgeRef&;

    SlicedSVFGEdgeIterImpl() = default;

    static SlicedSVFGEdgeIterImpl begin(const SlicedSVFGView* v, const SVFGNode* n)
    {
        SlicedSVFGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.begin();
        it.realEnd = edges.end();
        it.skipNonKept();
        it.refresh();
        return it;
    }
    static SlicedSVFGEdgeIterImpl end(const SlicedSVFGView* v, const SVFGNode* n)
    {
        SlicedSVFGEdgeIterImpl it(v, n);
        const auto& edges = Forward ? n->getOutEdges() : n->getInEdges();
        it.realIt = edges.end();
        it.realEnd = edges.end();
        return it;
    }

    reference operator*() const { return cur; }
    pointer operator->() const { return &cur; }
    SlicedSVFGNodeRef target() const { return Forward ? cur.dst : cur.src; }

    SlicedSVFGEdgeIterImpl& operator++()
    {
        if (realIt != realEnd) { ++realIt; skipNonKept(); }
        refresh();
        return *this;
    }
    SlicedSVFGEdgeIterImpl operator++(int) { SlicedSVFGEdgeIterImpl t = *this; ++*this; return t; }

    friend bool operator==(const SlicedSVFGEdgeIterImpl& a, const SlicedSVFGEdgeIterImpl& b)
    {
        return a.view == b.view && a.src == b.src && a.realIt == b.realIt;
    }
    friend bool operator!=(const SlicedSVFGEdgeIterImpl& a, const SlicedSVFGEdgeIterImpl& b) { return !(a == b); }

private:
    using EdgeIt = SVFGNode::const_iterator;
    const SlicedSVFGView* view = nullptr;
    const SVFGNode* src = nullptr;
    EdgeIt realIt{}, realEnd{};
    SlicedSVFGEdgeRef cur{};

    SlicedSVFGEdgeIterImpl(const SlicedSVFGView* v, const SVFGNode* n) : view(v), src(n) {}

    void skipNonKept()
    {
        while (realIt != realEnd &&
               !view->isKeptNode(Forward ? (*realIt)->getDstNode() : (*realIt)->getSrcNode()))
            ++realIt;
    }
    void refresh()
    {
        if (realIt != realEnd)
        {
            const SVFGEdge* e = *realIt;
            cur = SlicedSVFGEdgeRef{{view, e->getSrcNode()}, {view, e->getDstNode()}, e};
        }
        else
        {
            cur = SlicedSVFGEdgeRef{};
        }
    }
};

template <bool Forward>
class SlicedSVFGChildIterImpl
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedSVFGNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedSVFGNodeRef*;
    using reference = SlicedSVFGNodeRef;

    SlicedSVFGChildIterImpl() = default;
    explicit SlicedSVFGChildIterImpl(SlicedSVFGEdgeIterImpl<Forward> it) : e(it) {}

    reference operator*() const { return e.target(); }
    const SlicedSVFGEdgeRef& currentEdge() const { return *e; }
    SlicedSVFGChildIterImpl& operator++() { ++e; return *this; }
    SlicedSVFGChildIterImpl operator++(int) { SlicedSVFGChildIterImpl t = *this; ++e; return t; }
    friend bool operator==(const SlicedSVFGChildIterImpl& a, const SlicedSVFGChildIterImpl& b) { return a.e == b.e; }
    friend bool operator!=(const SlicedSVFGChildIterImpl& a, const SlicedSVFGChildIterImpl& b) { return !(a == b); }

private:
    SlicedSVFGEdgeIterImpl<Forward> e{};
};

// Node iterator: filters the underlying SVFG's node map by the retained rule.
class SlicedSVFGNodeIter
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = SlicedSVFGNodeRef;
    using difference_type = std::ptrdiff_t;
    using pointer = const SlicedSVFGNodeRef*;
    using reference = SlicedSVFGNodeRef;

    SlicedSVFGNodeIter() = default;
    SlicedSVFGNodeIter(const SlicedSVFGView* v, SVFG::const_iterator i, SVFG::const_iterator e)
        : view(v), it(i), endIt(e)
    {
        skipNonKept();
    }

    reference operator*() const { return SlicedSVFGNodeRef{view, it->second}; }
    SlicedSVFGNodeIter& operator++() { ++it; skipNonKept(); return *this; }
    SlicedSVFGNodeIter operator++(int) { SlicedSVFGNodeIter t = *this; ++*this; return t; }
    friend bool operator==(const SlicedSVFGNodeIter& a, const SlicedSVFGNodeIter& b) { return a.it == b.it; }
    friend bool operator!=(const SlicedSVFGNodeIter& a, const SlicedSVFGNodeIter& b) { return !(a == b); }

private:
    const SlicedSVFGView* view = nullptr;
    SVFG::const_iterator it{}, endIt{};
    void skipNonKept()
    {
        while (it != endIt && !view->isKeptNode(it->second)) ++it;
    }
};

} // End namespace SVF

//===----------------------------------------------------------------------===//
// Traits specializations must be at namespace SVF scope too (that is where the
// GenericGraphTraits primary template and Inverse live).
//===----------------------------------------------------------------------===//
namespace SVF
{

// Forward traits for SlicedICFGView.
template <>
struct GenericGraphTraits<const SlicedICFGView*>
{
    using NodeRef = SlicedICFGNodeRef;
    using EdgeRef = SlicedICFGEdgeRef;
    using nodes_iterator = SlicedICFGNodeIter;
    using ChildIteratorType = SlicedICFGChildIterImpl<true>;
    using ChildEdgeIteratorType = SlicedICFGEdgeIterImpl<true>;

    // Escape hatch to the underlying node so graph-generic algorithms can reach
    // domain data (getFun/getSVFStmts) uniformly. Full-ICFG traits return the node.
    static const ICFGNode* getRawNode(NodeRef n) { return n.raw; }

    // Graph-intrinsic queries mirroring GenericGraphTraits<ICFG*>, so a
    // graph-parameterised analysis resolves the right behaviour from the type.
    //@{
    static const ICFGNode* getFunEntry(const SlicedICFGView* g, const FunObjVar* fun)
    {
        return g->getFunEntry(fun);
    }
    static void getFunICFGNodes(const SlicedICFGView* g, const FunObjVar* fun,
                                std::vector<const ICFGNode*>& out)
    {
        g->getFunICFGNodes(fun, out);
    }
    static void getSuccNodes(const SlicedICFGView* g, const ICFGNode* n,
                             std::vector<const ICFGNode*>& out)
    {
        g->getSuccNodes(n, out);
    }
    static void getPredNodes(const SlicedICFGView* g, const ICFGNode* n,
                             std::vector<const ICFGNode*>& out)
    {
        g->getPredNodes(n, out);
    }
    static bool containsNode(const SlicedICFGView* g, const ICFGNode* n)
    {
        return g->isKeptNode(n);
    }
    //@}

    static NodeRef getEntryNode(const SlicedICFGView*) { return NodeRef{}; }

    static nodes_iterator nodes_begin(const SlicedICFGView* v)
    {
        return SlicedICFGNodeIter(v, v->getKeptNodes().begin());
    }
    static nodes_iterator nodes_end(const SlicedICFGView* v)
    {
        return SlicedICFGNodeIter(v, v->getKeptNodes().end());
    }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildIteratorType direct_child_begin(NodeRef n) { return child_begin(n); }
    static ChildIteratorType direct_child_end(NodeRef n) { return child_end(n); }

    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.dst; }

    static unsigned graphSize(const SlicedICFGView* v)
    {
        return static_cast<unsigned>(v->getKeptNodes().size());
    }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
    static NodeRef getNode(const SlicedICFGView* v, NodeID id)
    {
        const ICFGNode* raw = v->getOriginalICFG()->getGNode(id);
        return NodeRef{v, (raw != nullptr && v->isKeptNode(raw)) ? raw : nullptr};
    }
};

// Inverse (reverse-traversal) traits for SlicedICFGView.
template <>
struct GenericGraphTraits<Inverse<const SlicedICFGView*>>
{
    using NodeRef = SlicedICFGNodeRef;
    using EdgeRef = SlicedICFGEdgeRef;
    using ChildIteratorType = SlicedICFGChildIterImpl<false>;
    using ChildEdgeIteratorType = SlicedICFGEdgeIterImpl<false>;

    static NodeRef getEntryNode(Inverse<const SlicedICFGView*>) { return NodeRef{}; }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }

    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    // Inverse: the traversed "child" is the predecessor -> the edge source.
    static NodeRef edge_dest(const EdgeRef& e) { return e.src; }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
};

// Forward traits for SlicedThreadCallGraphView.
template <>
struct GenericGraphTraits<const SlicedThreadCallGraphView*>
{
    using NodeRef = SlicedCallGraphNodeRef;
    using EdgeRef = SlicedCallGraphEdgeRef;
    using nodes_iterator = SlicedCGNodeIter;
    using ChildIteratorType = SlicedCGChildIterImpl<true>;
    using ChildEdgeIteratorType = SlicedCGEdgeIterImpl<true>;

    static const CallGraphNode* getRawNode(NodeRef n) { return n.raw; }

    // Graph-intrinsic queries mirroring GenericGraphTraits<CallGraph*>.
    //@{
    static void getInEdges(const SlicedThreadCallGraphView* g, const CallGraphNode* n,
                           std::vector<const CallGraphEdge*>& out)
    {
        g->getInEdgesOf(n, out);
    }
    static const CallGraph* getCallGraph(const SlicedThreadCallGraphView* g)
    {
        return g->getOriginalCallGraph();
    }
    //@}

    static NodeRef getEntryNode(const SlicedThreadCallGraphView*) { return NodeRef{}; }

    static nodes_iterator nodes_begin(const SlicedThreadCallGraphView* v)
    {
        return SlicedCGNodeIter(v, v->getKeptNodes().begin());
    }
    static nodes_iterator nodes_end(const SlicedThreadCallGraphView* v)
    {
        return SlicedCGNodeIter(v, v->getKeptNodes().end());
    }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildIteratorType direct_child_begin(NodeRef n) { return child_begin(n); }
    static ChildIteratorType direct_child_end(NodeRef n) { return child_end(n); }

    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.dst; }

    static unsigned graphSize(const SlicedThreadCallGraphView* v)
    {
        return static_cast<unsigned>(v->getKeptNodes().size());
    }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
    static NodeRef getNode(const SlicedThreadCallGraphView* v, NodeID id)
    {
        const CallGraphNode* raw = v->getOriginalCallGraph()->getGNode(id);
        return NodeRef{v, (raw != nullptr && v->isKeptNode(raw)) ? raw : nullptr};
    }
};

// Inverse traits for SlicedThreadCallGraphView.
template <>
struct GenericGraphTraits<Inverse<const SlicedThreadCallGraphView*>>
{
    using NodeRef = SlicedCallGraphNodeRef;
    using EdgeRef = SlicedCallGraphEdgeRef;
    using ChildIteratorType = SlicedCGChildIterImpl<false>;
    using ChildEdgeIteratorType = SlicedCGEdgeIterImpl<false>;

    static NodeRef getEntryNode(Inverse<const SlicedThreadCallGraphView*>) { return NodeRef{}; }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.src; }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
};

// Forward traits for SlicedPAGView.
template <>
struct GenericGraphTraits<const SlicedPAGView*>
{
    using NodeRef = SlicedPAGNodeRef;
    using EdgeRef = SlicedPAGEdgeRef;
    using nodes_iterator = SlicedPAGNodeIter;
    using ChildIteratorType = SlicedPAGChildIterImpl<true>;
    using ChildEdgeIteratorType = SlicedPAGEdgeIterImpl<true>;

    static const SVFVar* getRawNode(NodeRef n) { return n.raw; }

    static NodeRef getEntryNode(const SlicedPAGView*) { return NodeRef{}; }

    static nodes_iterator nodes_begin(const SlicedPAGView* v)
    {
        return SlicedPAGNodeIter(v, v->getKeptNodeIds().begin());
    }
    static nodes_iterator nodes_end(const SlicedPAGView* v)
    {
        return SlicedPAGNodeIter(v, v->getKeptNodeIds().end());
    }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildIteratorType direct_child_begin(NodeRef n) { return child_begin(n); }
    static ChildIteratorType direct_child_end(NodeRef n) { return child_end(n); }

    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.dst; }

    static unsigned graphSize(const SlicedPAGView* v)
    {
        return static_cast<unsigned>(v->getKeptNodeIds().size());
    }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
    static NodeRef getNode(const SlicedPAGView* v, NodeID id)
    {
        const bool kept = v->getKeptNodeIds().count(id) > 0;
        return NodeRef{v, kept ? v->getSVFIR()->getGNode(id) : nullptr};
    }
};

// Inverse traits for SlicedPAGView.
template <>
struct GenericGraphTraits<Inverse<const SlicedPAGView*>>
{
    using NodeRef = SlicedPAGNodeRef;
    using EdgeRef = SlicedPAGEdgeRef;
    using ChildIteratorType = SlicedPAGChildIterImpl<false>;
    using ChildEdgeIteratorType = SlicedPAGEdgeIterImpl<false>;

    static NodeRef getEntryNode(Inverse<const SlicedPAGView*>) { return NodeRef{}; }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.src; }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
};

// Forward traits for SlicedSVFGView.
template <>
struct GenericGraphTraits<const SlicedSVFGView*>
{
    using NodeRef = SlicedSVFGNodeRef;
    using EdgeRef = SlicedSVFGEdgeRef;
    using nodes_iterator = SlicedSVFGNodeIter;
    using ChildIteratorType = SlicedSVFGChildIterImpl<true>;
    using ChildEdgeIteratorType = SlicedSVFGEdgeIterImpl<true>;

    static const SVFGNode* getRawNode(NodeRef n) { return n.raw; }

    /// Whether n is retained by this sliced SVFG (the solver's restriction test).
    static bool containsNode(const SlicedSVFGView* g, const SVFGNode* n)
    {
        return g->isKeptNode(n);
    }

    static NodeRef getEntryNode(const SlicedSVFGView*) { return NodeRef{}; }

    static nodes_iterator nodes_begin(const SlicedSVFGView* v)
    {
        assert(v->getSVFG() && "SlicedSVFGView: bind the SVFG before iterating nodes");
        return SlicedSVFGNodeIter(v, v->getSVFG()->begin(), v->getSVFG()->end());
    }
    static nodes_iterator nodes_end(const SlicedSVFGView* v)
    {
        assert(v->getSVFG() && "SlicedSVFGView: bind the SVFG before iterating nodes");
        return SlicedSVFGNodeIter(v, v->getSVFG()->end(), v->getSVFG()->end());
    }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildIteratorType direct_child_begin(NodeRef n) { return child_begin(n); }
    static ChildIteratorType direct_child_end(NodeRef n) { return child_end(n); }

    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.dst; }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
    static NodeRef getNode(const SlicedSVFGView* v, NodeID id)
    {
        const SVFGNode* raw =
            (v->getSVFG() != nullptr) ? v->getSVFG()->getGNode(id) : nullptr;
        return NodeRef{v, (raw != nullptr && v->isKeptNode(raw)) ? raw : nullptr};
    }
};

// Inverse traits for SlicedSVFGView.
template <>
struct GenericGraphTraits<Inverse<const SlicedSVFGView*>>
{
    using NodeRef = SlicedSVFGNodeRef;
    using EdgeRef = SlicedSVFGEdgeRef;
    using ChildIteratorType = SlicedSVFGChildIterImpl<false>;
    using ChildEdgeIteratorType = SlicedSVFGEdgeIterImpl<false>;

    static NodeRef getEntryNode(Inverse<const SlicedSVFGView*>) { return NodeRef{}; }

    static ChildIteratorType child_begin(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::begin(n.view, n.raw));
    }
    static ChildIteratorType child_end(NodeRef n)
    {
        return ChildIteratorType(ChildEdgeIteratorType::end(n.view, n.raw));
    }
    static ChildEdgeIteratorType child_edge_begin(NodeRef n)
    {
        return ChildEdgeIteratorType::begin(n.view, n.raw);
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef n)
    {
        return ChildEdgeIteratorType::end(n.view, n.raw);
    }

    static NodeRef edge_dest(const EdgeRef& e) { return e.src; }
    static inline unsigned getNodeID(NodeRef n) { return n.raw->getId(); }
};

//===----------------------------------------------------------------------===//
// Lock in the capabilities each sliced view provides: a clear compile error here
// (rather than deep inside a template) if a future change drops one.
//===----------------------------------------------------------------------===//
static_assert(BidirectionalGraphModel<const SlicedICFGView*>::value,
              "SlicedICFGView must support forward and reverse traversal");
static_assert(EdgeAwareGraphModel<const SlicedICFGView*>::value,
              "SlicedICFGView must expose first-class edges (child_edge + edge_dest)");
static_assert(IndexedGraphModel<const SlicedICFGView*>::value,
              "SlicedICFGView must support node id <-> node lookup");
static_assert(BidirectionalGraphModel<const SlicedThreadCallGraphView*>::value,
              "SlicedThreadCallGraphView must support forward and reverse traversal");
static_assert(EdgeAwareGraphModel<const SlicedThreadCallGraphView*>::value,
              "SlicedThreadCallGraphView must expose first-class edges");
static_assert(BidirectionalGraphModel<const SlicedPAGView*>::value,
              "SlicedPAGView must support forward and reverse traversal");
static_assert(EdgeAwareGraphModel<const SlicedPAGView*>::value,
              "SlicedPAGView must expose first-class edges");

} // End namespace SVF

#endif // GRAPHS_SLICEDGRAPHS_H
