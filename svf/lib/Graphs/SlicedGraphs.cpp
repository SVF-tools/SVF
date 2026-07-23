//===- SlicedGraphs.cpp -- General sliced graph views ----------------------===//
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
 * SlicedGraphs.cpp
 *
 *      Author: Jiawei Yang
 *
 * Implementations of the general sliced graph views (SlicedGraphs.h): view
 * construction, bridged-edge contraction, and dot dumping.
 */

#include "Graphs/SlicedGraphs.h"
#include "Graphs/SVFGNode.h"
#include "Graphs/GraphPrinter.h"
#include "Graphs/DOTGraphTraits.h"
#include "Util/SVFUtil.h"
#include "Util/ThreadAPI.h"
#include "SVFIR/SVFIR.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <sstream>
#include <queue>
#include <iostream>

using namespace SVF;

namespace SVF
{

//===----------------------------------------------------------------------===//
// DOTGraphTraits for the sliced ICFG view (used by GraphWriter). Mirrors the
// placement of DOTGraphTraits<ICFG*> in ICFG.cpp.
//===----------------------------------------------------------------------===//
template <>
struct DOTGraphTraits<const SlicedICFGView*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

    static std::string getGraphName(const SlicedICFGView*)
    {
        return "SlicedICFG";
    }

    // Value NodeRef: identity is the underlying kept node.
    static const void* getNodeIdentifier(SlicedICFGNodeRef n)
    {
        return n.raw;
    }

    std::string getNodeLabel(SlicedICFGNodeRef n, const SlicedICFGView*)
    {
        return n.raw != nullptr ? n.raw->toString() : "";
    }

    static std::string getNodeAttributes(SlicedICFGNodeRef n, const SlicedICFGView*)
    {
        std::string str = "shape=record";
        const ICFGNode* node = n.raw;
        if (SVFUtil::isa<FunEntryICFGNode>(node)) str += ",color=yellow";
        else if (SVFUtil::isa<FunExitICFGNode>(node)) str += ",color=green";
        else if (SVFUtil::isa<CallICFGNode>(node)) str += ",color=red";
        else if (SVFUtil::isa<RetICFGNode>(node)) str += ",color=blue";
        else if (SVFUtil::isa<GlobalICFGNode>(node)) str += ",color=purple";
        else str += ",color=black";
        return str;
    }

    // Bridged edges are drawn dashed; kept original call/ret edges keep their colour.
    template <class EdgeIter>
    static std::string getEdgeAttributes(SlicedICFGNodeRef, EdgeIter EI, const SlicedICFGView*)
    {
        const SlicedICFGEdgeRef& e = EI.currentEdge();
        if (e.bridged) return "style=dashed,color=gray";
        if (e.underlying != nullptr && SVFUtil::isa<CallCFGEdge>(e.underlying))
            return "style=solid,color=red";
        if (e.underlying != nullptr && SVFUtil::isa<RetCFGEdge>(e.underlying))
            return "style=solid,color=blue";
        return "style=solid";
    }
};

//===----------------------------------------------------------------------===//
// DOTGraphTraits for the sliced ThreadCallGraph view.
//===----------------------------------------------------------------------===//
template <>
struct DOTGraphTraits<const SlicedThreadCallGraphView*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

    static std::string getGraphName(const SlicedThreadCallGraphView*)
    {
        return "SlicedThreadCallGraph";
    }

    static const void* getNodeIdentifier(SlicedCallGraphNodeRef n)
    {
        return n.raw;
    }

    std::string getNodeLabel(SlicedCallGraphNodeRef n, const SlicedThreadCallGraphView*)
    {
        return n.raw != nullptr ? n.raw->getName() : "";
    }

    static std::string getNodeAttributes(SlicedCallGraphNodeRef, const SlicedThreadCallGraphView*)
    {
        return "shape=record,color=black";
    }

    template <class EdgeIter>
    static std::string getEdgeAttributes(SlicedCallGraphNodeRef, EdgeIter EI, const SlicedThreadCallGraphView*)
    {
        const CallGraphEdge* e = EI.currentEdge().underlying;
        if (e != nullptr && e->getEdgeKind() == CallGraphEdge::TDForkEdge) return "color=green";
        if (e != nullptr && e->getEdgeKind() == CallGraphEdge::CallRetEdge) return "color=blue";
        return "color=black";
    }
};

//===----------------------------------------------------------------------===//
// DOTGraphTraits for the sliced PAG view.
//===----------------------------------------------------------------------===//
template <>
struct DOTGraphTraits<const SlicedPAGView*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

    static std::string getGraphName(const SlicedPAGView*)
    {
        return "SlicedPAG";
    }

    static const void* getNodeIdentifier(SlicedPAGNodeRef n)
    {
        return n.raw;
    }

    std::string getNodeLabel(SlicedPAGNodeRef n, const SlicedPAGView*)
    {
        return n.raw != nullptr ? n.raw->toString() : "";
    }

    static std::string getNodeAttributes(SlicedPAGNodeRef, const SlicedPAGView*)
    {
        return "shape=record,color=black";
    }

    template <class EdgeIter>
    static std::string getEdgeAttributes(SlicedPAGNodeRef, EdgeIter EI, const SlicedPAGView*)
    {
        const SVFStmt* s = EI.currentEdge().underlying;
        if (SVFUtil::isa<LoadStmt>(s)) return "color=blue";
        if (SVFUtil::isa<StoreStmt>(s)) return "color=red";
        if (SVFUtil::isa<GepStmt>(s)) return "color=purple";
        if (SVFUtil::isa<AddrStmt>(s)) return "color=green";
        if (SVFUtil::isa<CallPE>(s)) return "color=orange";
        if (SVFUtil::isa<RetPE>(s)) return "color=cyan";
        return "color=black";
    }
};

//===----------------------------------------------------------------------===//
// DOTGraphTraits for the sliced SVFG view.
//===----------------------------------------------------------------------===//
template <>
struct DOTGraphTraits<const SlicedSVFGView*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

    static std::string getGraphName(const SlicedSVFGView*)
    {
        return "SlicedSVFG";
    }

    static const void* getNodeIdentifier(SlicedSVFGNodeRef n)
    {
        return n.raw;
    }

    std::string getNodeLabel(SlicedSVFGNodeRef n, const SlicedSVFGView*)
    {
        return n.raw != nullptr ? n.raw->toString() : "";
    }

    static std::string getNodeAttributes(SlicedSVFGNodeRef n, const SlicedSVFGView*)
    {
        if (SVFUtil::isa<StoreSVFGNode>(n.raw)) return "shape=record,color=red";
        if (SVFUtil::isa<LoadSVFGNode>(n.raw)) return "shape=record,color=blue";
        return "shape=record,color=black";
    }

    template <class EdgeIter>
    static std::string getEdgeAttributes(SlicedSVFGNodeRef, EdgeIter EI, const SlicedSVFGView*)
    {
        const SVFGEdge* e = EI.currentEdge().underlying;
        if (e != nullptr && SVFUtil::isa<IndirectSVFGEdge>(e)) return "style=dashed";
        return "style=solid";
    }
};

//===----------------------------------------------------------------------===//
// SlicedSVFGView
//===----------------------------------------------------------------------===//

// Retained rule (must mirror the sliced FSAM gate): Load/Store statement nodes
// whose ICFG node was sliced out are removed; every other node is structural
// for inter-procedural / memory-SSA flow and remains.
bool SlicedSVFGView::isKeptNode(const SVFGNode* n) const
{
    if (SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(n))
    {
        const StmtSVFGNode* stmt = SVFUtil::cast<StmtSVFGNode>(n);
        const ICFGNode* icfg = stmt->getICFGNode();
        if (icfg != nullptr && !icfgView->isKeptNode(icfg))
            return false;
    }
    return true;
}

void SlicedSVFGView::dump(const std::string& filename) const
{
    assert(svfg != nullptr && "SlicedSVFGView: bind the SVFG before dumping");
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
}

//===----------------------------------------------------------------------===//
// SlicedICFGView
//===----------------------------------------------------------------------===//

SlicedICFGView::SlicedICFGView(ICFG* icfg,
                               CallGraph* cg,
                               const OrderedSet<const ICFGNode*>& keepNodes,
                               const OrderedSet<const FunObjVar*>& keptFunctions,
                               bool buildBridged)
    : icfg(icfg)
{
    buildICFGSets(keepNodes, keptFunctions);
    if (buildBridged)
        buildBridgedEdges();
}

// getSuccNodes/getPredNodes and the GenericGraphTraits iterators must agree on the
// slice topology, so both go through the one traits definition (kept original
// edges + bridged edges). The sliced MHP/Lock analyses reach the slice here.
void SlicedICFGView::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    out.clear();
    if (!isKeptNode(node))
    {
        return;
    }
    using GT = GenericGraphTraits<const SlicedICFGView*>;
    const SlicedICFGNodeRef n{this, node};
    for (auto it = GT::child_begin(n), e = GT::child_end(n); it != e; ++it)
        out.push_back((*it).raw);
}

void SlicedICFGView::getPredNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    out.clear();
    if (!isKeptNode(node))
    {
        return;
    }
    using GT = GenericGraphTraits<Inverse<const SlicedICFGView*>>;
    const SlicedICFGNodeRef n{this, node};
    for (auto it = GT::child_begin(n), e = GT::child_end(n); it != e; ++it)
        out.push_back((*it).raw);
}

bool SlicedICFGView::isKeptNode(const ICFGNode* node) const
{
    return keptNodesSet.count(node) > 0;
}

void SlicedICFGView::dump(const std::string& filename) const
{
    // Kept nodes + kept original edges + bridged edges are all produced by the
    // GenericGraphTraits<const SlicedICFGView*> iterators; GraphWriter styles
    // bridged edges dashed via DOTGraphTraits::getEdgeAttributes.
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
}

void SlicedICFGView::buildICFGSets(const OrderedSet<const ICFGNode*>& keepNodes,
                                   const OrderedSet<const FunObjVar*>& keptFunctions)
{
    // keepNodes already includes all necessary nodes (call/ret nodes and entry/exit nodes)
    // so we just need to copy them and build edges.

    // Copy keepNodes to keptNodes
    keptNodes.clear();
    keptNodes.insert(keepNodes.begin(), keepNodes.end());

    // Build keptNodesSet for fast lookup
    keptNodesSet.clear();
    keptNodesSet.insert(keptNodes.begin(), keptNodes.end());

    // Build edges: only when both src and dst are in keptNodes
    keptEdges.clear();
    for (const ICFGNode* node : keptNodes)
    {
        for (const ICFGEdge* edge : node->getOutEdges())
        {
            if (edge && keptNodesSet.count(edge->getDstNode()))
            {
                keptEdges.insert(edge);
            }
        }
    }
}

void SlicedICFGView::buildBridgedEdges()
{
    // bridgedEdges[u] (u kept) = kept nodes reachable from u through removed-only
    // paths = U reachKept(s) over removed successors s of u, where reachKept(r) is
    // computed by SCC-condensing the removed subgraph (cyclic) and propagating
    // kept-reachability over the condensation -- linear, vs. node contraction whose
    // cross-products blow up when the removed region is large (small slices).

    // Index the removed nodes and their removed-only adjacency + kept successors.
    std::vector<const ICFGNode*> removed;
    Map<const ICFGNode*, int> rid;
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
    {
        const ICFGNode* n = it->second;
        if (n == nullptr || keptNodesSet.count(n)) continue;
        rid[n] = static_cast<int>(removed.size());
        removed.push_back(n);
    }
    const int R = static_cast<int>(removed.size());

    // call_i -> ret_i summary for call sites with an omitted callee (some resolved
    // callee entry not retained), for every call site so paths can compose through
    // removed ones. ret_i is in the same caller, so the seed stays intra-procedural.
    Map<const ICFGNode*, const ICFGNode*> seedRet;
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
    {
        const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(it->second);
        if (call == nullptr || call->getRetICFGNode() == nullptr) continue;
        for (const ICFGEdge* e : call->getOutEdges())
            if (e && SVFUtil::isa<CallCFGEdge>(e) && keptNodesSet.count(e->getDstNode()) == 0)
            {
                seedRet[call] = call->getRetICFGNode();
                break;
            }
    }
    // Local successors = intra edges + matched call->ret seeds; the only edges
    // contraction may traverse. Original call/ret edges are excluded.
    auto localSuccs = [&](const ICFGNode* n, std::vector<const ICFGNode*>& out)
    {
        out.clear();
        for (const ICFGEdge* e : n->getOutEdges())
            if (e && SVFUtil::isa<IntraCFGEdge>(e) && e->getDstNode())
                out.push_back(e->getDstNode());
        auto sit = seedRet.find(n);
        if (sit != seedRet.end()) out.push_back(sit->second);
    };

    std::vector<std::vector<int>> radj(R);                 // removed -> removed succ ids
    std::vector<std::vector<const ICFGNode*>> keptSucc(R);  // removed -> kept succs
    std::vector<const ICFGNode*> succs;
    for (int i = 0; i < R; ++i)
    {
        localSuccs(removed[i], succs);
        for (const ICFGNode* d : succs)
        {
            if (keptNodesSet.count(d)) keptSucc[i].push_back(d);
            else radj[i].push_back(rid[d]);
        }
    }

    // Iterative Tarjan SCC over the removed subgraph. Components are produced in
    // reverse-topological order, so comp ids of a node's successors are < its own.
    std::vector<int> idx(R, -1), low(R, 0), comp(R, -1);
    std::vector<char> onstk(R, 0);
    std::vector<int> tstk;
    int counter = 0, ncomp = 0;
    for (int s0 = 0; s0 < R; ++s0)
    {
        if (idx[s0] != -1) continue;
        std::vector<std::pair<int, size_t>> work;
        work.emplace_back(s0, 0);
        while (!work.empty())
        {
            int v = work.back().first;
            size_t& pi = work.back().second;
            if (pi == 0)
            {
                idx[v] = low[v] = counter++;
                tstk.push_back(v);
                onstk[v] = 1;
            }
            bool descend = false;
            while (pi < radj[v].size())
            {
                int w = radj[v][pi++];
                if (idx[w] == -1)
                {
                    work.emplace_back(w, 0);
                    descend = true;
                    break;
                }
                else if (onstk[w] && idx[w] < low[v]) low[v] = idx[w];
            }
            if (descend) continue;
            if (low[v] == idx[v])
            {
                while (true)
                {
                    int w = tstk.back();
                    tstk.pop_back();
                    onstk[w] = 0;
                    comp[w] = ncomp;
                    if (w == v) break;
                }
                ++ncomp;
            }
            work.pop_back();
            if (!work.empty())
            {
                int p = work.back().first;
                if (low[v] < low[p]) low[p] = low[v];
            }
        }
    }

    // Condensation: base kept-successors and DAG successors per component.
    std::vector<OrderedSet<const ICFGNode*>> baseKept(ncomp);
    std::vector<OrderedSet<int>> dagSucc(ncomp);
    for (int i = 0; i < R; ++i)
    {
        int ci = comp[i];
        for (const ICFGNode* k : keptSucc[i]) baseKept[ci].insert(k);
        for (int j : radj[i]) if (comp[j] != ci) dagSucc[ci].insert(comp[j]);
    }

    // Propagate reachKept in ascending comp order (successors have smaller ids).
    std::vector<OrderedSet<const ICFGNode*>> reachKept(ncomp);
    for (int c = 0; c < ncomp; ++c)
    {
        OrderedSet<const ICFGNode*>& acc = reachKept[c];
        acc = baseKept[c];
        for (int d : dagSucc[c]) acc.insert(reachKept[d].begin(), reachKept[d].end());
    }

    // bridgedEdges[u] = kept nodes reached from kept u through removed local paths,
    // plus a matched call->ret summary when u is a seeded call site (kept ret).
    for (const ICFGNode* u : keptNodesSet)
    {
        localSuccs(u, succs);
        auto sit = seedRet.find(u);
        for (const ICFGNode* s : succs)
        {
            if (keptNodesSet.count(s))
            {
                // Kept seed target: no real edge exists, so record the bridge; a
                // kept intra target is a real edge handled by getSuccNodes.
                if (sit != seedRet.end() && sit->second == s)
                {
                    bridgedEdges[u].insert(s);
                    bridgedPreds[s].insert(u);
                }
                continue;
            }
            for (const ICFGNode* v : reachKept[comp[rid[s]]])
            {
                bridgedEdges[u].insert(v);
                bridgedPreds[v].insert(u);
            }
        }
    }

    size_t totalBridgedEdges = 0;
    for (const auto& pair : bridgedEdges) totalBridgedEdges += pair.second.size();
    SVFUtil::outs() << "[SlicedICFGView] Built " << totalBridgedEdges
                    << " bridged edges across " << bridgedEdges.size() << " source nodes\n";
}

//===----------------------------------------------------------------------===//
// SlicedPAGView
//===----------------------------------------------------------------------===//

SlicedPAGView::SlicedPAGView(SVFIR* pag, const OrderedSet<const SVFStmt*>& keptStmts)
    : pag(pag), keptStmts(keptStmts)
{
    buildKeptNodeIds();
}

void SlicedPAGView::buildKeptNodeIds()
{
    for (const SVFStmt* stmt : keptStmts)
    {
        // extract the node IDs involved in the statement
        const AssignStmt* assignStmt = SVFUtil::dyn_cast<AssignStmt>(stmt);
        if (assignStmt)
        {
            keptNodeIds.insert(assignStmt->getLHSVarID());
            keptNodeIds.insert(assignStmt->getRHSVarID());
            continue;
        }

        const LoadStmt* loadStmt = SVFUtil::dyn_cast<LoadStmt>(stmt);
        if (loadStmt)
        {
            keptNodeIds.insert(loadStmt->getLHSVarID());
            keptNodeIds.insert(loadStmt->getRHSVarID());
            continue;
        }

        const StoreStmt* storeStmt = SVFUtil::dyn_cast<StoreStmt>(stmt);
        if (storeStmt)
        {
            keptNodeIds.insert(storeStmt->getLHSVarID());
            keptNodeIds.insert(storeStmt->getRHSVarID());
            continue;
        }

        const CopyStmt* copyStmt = SVFUtil::dyn_cast<CopyStmt>(stmt);
        if (copyStmt)
        {
            keptNodeIds.insert(copyStmt->getLHSVarID());
            keptNodeIds.insert(copyStmt->getRHSVarID());
            continue;
        }

        const GepStmt* gepStmt = SVFUtil::dyn_cast<GepStmt>(stmt);
        if (gepStmt)
        {
            keptNodeIds.insert(gepStmt->getLHSVarID());
            keptNodeIds.insert(gepStmt->getRHSVarID());
            continue;
        }

        const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt);
        if (addrStmt)
        {
            keptNodeIds.insert(addrStmt->getLHSVarID());
            keptNodeIds.insert(addrStmt->getRHSVarID());
            continue;
        }

        const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt);
        if (callPE)
        {
            // CallPE is a MultiOpndStmt (result + per-call-site operands)
            // rather than a single-LHS/RHS AssignStmt.
            keptNodeIds.insert(callPE->getResID());
            for (u32_t i = 0; i < callPE->getOpVarNum(); ++i)
                keptNodeIds.insert(callPE->getOpVarID(i));
            continue;
        }

        const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt);
        if (retPE)
        {
            keptNodeIds.insert(retPE->getLHSVarID());
            keptNodeIds.insert(retPE->getRHSVarID());
            continue;
        }
    }
}

void SlicedPAGView::dump(const std::string& filename) const
{
    // Nodes = SVFVars of kept statements; edges = kept SVFStmts via
    // GenericGraphTraits<const SlicedPAGView*>. MultiOpndStmts use the
    // underlying src/dst (no operand fan-out).
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
}

//===----------------------------------------------------------------------===//
// SlicedThreadCallGraphView
//===----------------------------------------------------------------------===//

SlicedThreadCallGraphView::SlicedThreadCallGraphView(ThreadCallGraph* tcg,
        const OrderedSet<const FunObjVar*>& keptFunctions,
        const OrderedSet<const ICFGNode*>& extendedKeptNodes)
    : tcg(tcg)
{
    for (const FunObjVar* fun : keptFunctions)
    {
        keptFunctionsSet.insert(fun);
    }
    this->extendedKeptNodes = extendedKeptNodes;
    buildKeptNodes();
    // extendedKeptNodes already includes all necessary nodes from performSpatialSlicing;
    // buildCallGraphSets builds the kept edges (filtering pruned call sites when it is non-empty).
    buildCallGraphSets();
}

void SlicedThreadCallGraphView::buildKeptNodes()
{
    for (CallGraph::iterator it = tcg->begin(), eit = tcg->end(); it != eit; ++it)
    {
        const CallGraphNode* node = it->second;
        if (node && node->getFunction() && keptFunctionsSet.count(node->getFunction()))
        {
            keptNodes.insert(node);
        }
    }
}

void SlicedThreadCallGraphView::getOutEdgesOf(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    if (!isKeptNode(node))
    {
        return;
    }

    using GT = GenericGraphTraits<const SlicedThreadCallGraphView*>;
    const SlicedCallGraphNodeRef n{this, node};
    for (auto it = GT::child_edge_begin(n), e = GT::child_edge_end(n); it != e; ++it)
        out.push_back((*it).underlying);
}

void SlicedThreadCallGraphView::getInEdgesOf(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    if (!isKeptNode(node))
    {
        return;
    }
    using GT = GenericGraphTraits<Inverse<const SlicedThreadCallGraphView*>>;
    const SlicedCallGraphNodeRef n{this, node};
    for (auto it = GT::child_edge_begin(n), e = GT::child_edge_end(n); it != e; ++it)
        out.push_back((*it).underlying);
}

bool SlicedThreadCallGraphView::isKeptNode(const CallGraphNode* node) const
{
    return keptNodes.count(node) > 0;
}

void SlicedThreadCallGraphView::buildCallGraphSets()
{
    // rebuild kept edges, accounting for whether the call site is kept
    keptEdges.clear();
    indirectSitesWithEmptyTargets.clear();

    // CallGraph edge: src/dst both in kept functions and the call site still in the kept ICFG node set
    for (const CallGraphNode* srcNode : keptNodes)
    {
        for (const CallGraphEdge* edge : srcNode->getOutEdges())
        {
            const CallGraphNode* dstNode = edge ? edge->getDstNode() : nullptr;
            if (!dstNode || !keptNodes.count(dstNode))
            {
                continue;
            }

            const CallGraphEdge::CallInstSet &directCalls = edge->getDirectCalls();
            const CallGraphEdge::CallInstSet &indirectCalls = edge->getIndirectCalls();
            const CallICFGNode* callSite = nullptr;
            if (!directCalls.empty())
            {
                callSite = *directCalls.begin();
            }
            else if (!indirectCalls.empty())
            {
                callSite = *indirectCalls.begin();
            }

            if (!extendedKeptNodes.empty() && callSite && !extendedKeptNodes.count(callSite))
            {
                // call site was pruned, skip this edge
                continue;
            }

            // indirect call: record if all targets were filtered out
            if (callSite && edge->getEdgeKind() == CallGraphEdge::CallRetEdge &&
                    tcg->hasIndCSCallees(callSite))
            {
                const CallGraph::FunctionSet& targets = tcg->getIndCSCallees(callSite);
                bool hasKeptTarget = false;
                for (const FunObjVar* tgt : targets)
                {
                    if (keptFunctionsSet.count(tgt))
                    {
                        hasKeptTarget = true;
                        break;
                    }
                }
                if (!hasKeptTarget)
                {
                    indirectSitesWithEmptyTargets.insert(callSite);
                }
            }

            keptEdges.insert(const_cast<CallGraphEdge*>(edge));
        }
    }
}

void SlicedThreadCallGraphView::dump(const std::string& filename) const
{
    // Kept nodes + canonical kept edges via GenericGraphTraits; join edges are
    // not in the normal adjacency, so they are not drawn here.
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
}

//===----------------------------------------------------------------------===//
// SlicedSVFIRView
//===----------------------------------------------------------------------===//

SlicedSVFIRView::SlicedSVFIRView(SVFIR* svfIr,
                                 CallGraph* cg,
                                 ICFG* icfg,
                                 const OrderedSet<const ICFGNode*>& keepNodes,
                                 bool buildBridged)
    : svfIr(svfIr)
{
    // Derive keptFunctions from keepNodes
    OrderedSet<const FunObjVar*> keptFunctions;
    for (const ICFGNode* node : keepNodes)
    {
        if (node != nullptr && node->getFun() != nullptr)
        {
            keptFunctions.insert(node->getFun());
        }
    }

    // The MTA pipeline always slices over a ThreadCallGraph.
    ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(cg);
    assert(tcg != nullptr && "SlicedSVFIRView expects a ThreadCallGraph");
    tcgView = std::make_unique<SlicedThreadCallGraphView>(tcg, keptFunctions, keepNodes);

    // Create ICFG view (based on keepNodes and keptFunctions)
    icfgView = std::make_unique<SlicedICFGView>(icfg, cg, keepNodes, keptFunctions, buildBridged);

    // Create PAG view (extract statements from keepNodes)
    OrderedSet<const SVFStmt*> keptStmts;
    for (const ICFGNode* node : keepNodes)
    {
        const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
        keptStmts.insert(stmts.begin(), stmts.end());
    }
    pagView = std::make_unique<SlicedPAGView>(svfIr, keptStmts);
}

void SlicedSVFIRView::dumpAll(const std::string& prefix) const
{
    icfgView->dump(prefix + "_icfg");
    tcgView->dump(prefix + "_threadcallgraph");
    pagView->dump(prefix + "_pag");
}

void SlicedSVFIRView::dumpStats(const std::string& prefix) const
{
    std::string label = prefix.empty() ? "[SlicedSVFIRView]" : "[" + prefix + "]";
    SVFUtil::outs() << label << " Statistics:\n";
    SVFUtil::outs() << "  ICFG nodes: " << icfgView->getKeptNodes().size() << "\n";
    SVFUtil::outs() << "  Functions: " << getKeptFunctions().size() << "\n";
    SVFUtil::outs() << "  PAG statements: " << getKeptStatements().size() << "\n";
    if (!getIndirectSitesWithEmptyTargets().empty())
    {
        SVFUtil::outs() << "  Indirect callsites that lost all targets: "
                        << getIndirectSitesWithEmptyTargets().size() << "\n";
    }
}

//===----------------------------------------------------------------------===//
// SlicedICFGView traversal helpers used by the sliced analyses.
//===----------------------------------------------------------------------===//

const ICFGNode* SlicedICFGView::getFunEntry(const FunObjVar* fun) const
{
    // Prefer the kept FunEntryICFGNode: MultiStageSlicer::expandCallDependence keeps it
    // for every kept function and buildBridgedEdges links it to the kept body, so
    // the MHP interleaving fixpoint can flow from it to every kept statement.
    // The entry basic block's first instruction, by contrast, may be sliced out;
    // returning it (a removed node) would strand the root/thread seed there --
    // getSuccNodes() yields nothing for a non-kept node -- so the function body
    // would never receive the thread's interleaving (a soundness bug).
    if (const ICFGNode* fe = icfg->getFunEntryICFGNode(fun))
    {
        if (isKeptNode(fe))
            return fe;
    }
    // Otherwise: first kept node in the entry block, or the original entry.
    const ICFGNode* entry = fun->getEntryBlock()->front();
    if (isKeptNode(entry))
        return entry;
    for (const ICFGNode* node : fun->getEntryBlock()->getICFGNodeList())
    {
        if (isKeptNode(node))
            return node;
    }
    return entry;
}

void SlicedICFGView::getFunICFGNodes(const FunObjVar* fun,
                                     std::vector<const ICFGNode*>& out) const
{
    out.clear();
    for (auto it : *fun)
    {
        const SVFBasicBlock* svfbb = it.second;
        for (const ICFGNode* node : svfbb->getICFGNodeList())
            if (isKeptNode(node))
                out.push_back(node);
    }
}

//===----------------------------------------------------------------------===//
// SlicedSVFIRView call-graph helpers used by the sliced analyses.
//===----------------------------------------------------------------------===//

void SlicedSVFIRView::getInEdgesOfCallGraphNode(const CallGraphNode* node,
        std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    if (getThreadCallGraph() != nullptr)
        getThreadCallGraph()->getInEdgesOf(node, out);
    else
        for (CallGraphEdge* edge : node->getInEdges())
            out.push_back(edge);
}

const CallGraph* SlicedSVFIRView::getAnalysisCallGraph() const
{
    if (getThreadCallGraph() != nullptr)
        return getThreadCallGraph()->getOriginalCallGraph();
    return PAG::getPAG()->getCallGraph();
}

} // End namespace SVF
