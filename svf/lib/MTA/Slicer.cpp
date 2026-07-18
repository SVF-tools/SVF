//===- Slicer.cpp -- Multi-stage on-demand program slicers ----------------===//
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
 * Slicer.cpp
 *
 *      Author: Jiawei Yang
 *
 * SlicerBase + MTASlicer + PTASlicer + SingleSlicer -- the program slicers of
 * "Multi-Stage On-Demand Program Slicing for Modular Analysis of Multi-Threaded
 * Programs" (ISSTA 2026).
 */

#include "MTA/Slicer.h"
#include <MTA/TCT.h>
#include <SVFIR/SVFIR.h>
#include <Util/SVFUtil.h>
#include <Util/CxtStmt.h>
#include <Util/ThreadAPI.h>
#include <deque>
#include <algorithm>
#include <cassert>
#include <Graphs/ICFGEdge.h>
#include <Graphs/ICFGNode.h>
#include <Graphs/CallGraph.h>
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include <fstream>
#include <sstream>
#include <queue>
#include <unordered_map>
#include <iostream>
#include <cctype>
#include <Graphs/SVFG.h>
#include <Graphs/VFGNode.h>
#include <Graphs/VFGEdge.h>

using namespace SVF;

namespace SVF
{

//===----------------------------------------------------------------------===//
// SlicedICFGView
//===----------------------------------------------------------------------===//

SlicedICFGView::SlicedICFGView(ICFG* icfg,
                               CallGraph* cg,
                               const std::set<const ICFGNode*>& keepNodes,
                               const std::set<const FunObjVar*>& keptFunctions)
    : icfg(icfg)
{
    buildICFGSets(keepNodes, keptFunctions);
    buildBridgedEdges();
}

void SlicedICFGView::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const {
    out.clear();
    if (!isKeptNode(node)) {
        return;
    }

    // 1) kept nodes among original out edges
    for (const ICFGEdge* edge : node->getOutEdges()) {
        const ICFGNode* dst = edge->getDstNode();
        if (isKeptNode(dst)) {
            out.push_back(dst);
        }
    }

    // 2) bridged edges
    auto it = bridgedEdges.find(node);
    if (it != bridgedEdges.end()) {
        for (const ICFGNode* dst : it->second) {
            if (isKeptNode(dst)) {
                out.push_back(dst);
            }
        }
    }
}

void SlicedICFGView::getPredNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const {
    out.clear();
    if (!isKeptNode(node)) {
        return;
    }

    // 1) kept nodes among original in edges
    for (const ICFGEdge* edge : node->getInEdges()) {
        const ICFGNode* src = edge->getSrcNode();
        if (isKeptNode(src)) {
            out.push_back(src);
        }
    }

    // 2) bridged predecessors (precomputed reverse map; srcs are already kept)
    auto it = bridgedPreds.find(node);
    if (it != bridgedPreds.end()) {
        for (const ICFGNode* src : it->second) {
            out.push_back(src);
        }
    }
}

bool SlicedICFGView::isKeptNode(const ICFGNode* node) const {
    return keptNodesSet.count(node) > 0;
}

void SlicedICFGView::dump(const std::string& filename) const {
    std::ofstream out;
    if (!SlicedViewAdapter::openDotGraph(out, filename, "ICFG", "TB", "shape=box, style=rounded"))
        return;

    for (const ICFGNode* node : keptNodes)
        SlicedViewAdapter::emitDotNode(out, "N", node->getId(), node->toString());

    out << "\n";

    for (const ICFGEdge* edge : keptEdges) {
        const ICFGNode* src = edge->getSrcNode();
        const ICFGNode* dst = edge->getDstNode();
        if (!isKeptNode(src) || !isKeptNode(dst)) {
            continue;
        }

        std::string srcId = "N" + std::to_string(src->getId());
        std::string dstId = "N" + std::to_string(dst->getId());
        std::string color = "black";

        if (SVFUtil::isa<CallCFGEdge>(edge)) {
            color = "red";
        } else if (SVFUtil::isa<RetCFGEdge>(edge)) {
            color = "blue";
        } else if (SVFUtil::isa<IntraCFGEdge>(edge)) {
            color = "black";
        }

        out << "  " << srcId << " -> " << dstId << " [color=" << color << "];\n";
    }

    // bridged edges (dashed)
    for (const auto& pair : bridgedEdges) {
        const ICFGNode* src = pair.first;
        for (const ICFGNode* dst : pair.second) {
            if (!isKeptNode(src) || !isKeptNode(dst)) {
                continue;
            }
            std::string srcId = "N" + std::to_string(src->getId());
            std::string dstId = "N" + std::to_string(dst->getId());
            out << "  " << srcId << " -> " << dstId << " [style=dashed, color=gray];\n";
        }
    }

    SlicedViewAdapter::finishDotGraph(out, filename, "SlicedICFGView", "ICFG");
}

void SlicedICFGView::buildICFGSets(const std::set<const ICFGNode*>& keepNodes,
                                    const std::set<const FunObjVar*>& keptFunctions) {
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
    for (const ICFGNode* node : keptNodes) {
        for (const ICFGEdge* edge : node->getOutEdges()) {
            if (edge && keptNodesSet.count(edge->getDstNode())) {
                keptEdges.insert(edge);
            }
        }
    }
}

void SlicedICFGView::buildBridgedEdges() {
    // bridgedEdges[u] (u kept) = kept nodes reachable from u through removed-only
    // paths = U reachKept(s) over removed successors s of u, where reachKept(r) is
    // computed by SCC-condensing the removed subgraph (cyclic) and propagating
    // kept-reachability over the condensation -- linear, vs. node contraction whose
    // cross-products blow up when the removed region is large (small slices).

    // Index the removed nodes and their removed-only adjacency + kept successors.
    std::vector<const ICFGNode*> removed;
    std::unordered_map<const ICFGNode*, int> rid;
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* n = it->second;
        if (n == nullptr || keptNodesSet.count(n)) continue;
        rid[n] = static_cast<int>(removed.size());
        removed.push_back(n);
    }
    const int R = static_cast<int>(removed.size());

    std::vector<std::vector<int>> radj(R);                 // removed -> removed succ ids
    std::vector<std::vector<const ICFGNode*>> keptSucc(R);  // removed -> kept succs
    for (int i = 0; i < R; ++i) {
        for (const ICFGEdge* e : removed[i]->getOutEdges()) {
            const ICFGNode* d = e ? e->getDstNode() : nullptr;
            if (d == nullptr) continue;
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
    for (int s0 = 0; s0 < R; ++s0) {
        if (idx[s0] != -1) continue;
        std::vector<std::pair<int, size_t>> work;
        work.emplace_back(s0, 0);
        while (!work.empty()) {
            int v = work.back().first;
            size_t& pi = work.back().second;
            if (pi == 0) { idx[v] = low[v] = counter++; tstk.push_back(v); onstk[v] = 1; }
            bool descend = false;
            while (pi < radj[v].size()) {
                int w = radj[v][pi++];
                if (idx[w] == -1) { work.emplace_back(w, 0); descend = true; break; }
                else if (onstk[w] && idx[w] < low[v]) low[v] = idx[w];
            }
            if (descend) continue;
            if (low[v] == idx[v]) {
                while (true) {
                    int w = tstk.back(); tstk.pop_back(); onstk[w] = 0; comp[w] = ncomp;
                    if (w == v) break;
                }
                ++ncomp;
            }
            work.pop_back();
            if (!work.empty()) { int p = work.back().first; if (low[v] < low[p]) low[p] = low[v]; }
        }
    }

    // Condensation: base kept-successors and DAG successors per component.
    std::vector<std::set<const ICFGNode*>> baseKept(ncomp);
    std::vector<std::set<int>> dagSucc(ncomp);
    for (int i = 0; i < R; ++i) {
        int ci = comp[i];
        for (const ICFGNode* k : keptSucc[i]) baseKept[ci].insert(k);
        for (int j : radj[i]) if (comp[j] != ci) dagSucc[ci].insert(comp[j]);
    }

    // Propagate reachKept in ascending comp order (successors have smaller ids).
    std::vector<std::set<const ICFGNode*>> reachKept(ncomp);
    for (int c = 0; c < ncomp; ++c) {
        std::set<const ICFGNode*>& acc = reachKept[c];
        acc = baseKept[c];
        for (int d : dagSucc[c]) acc.insert(reachKept[d].begin(), reachKept[d].end());
    }

    // bridgedEdges[u] = U reachKept(s) over removed successors s of kept u.
    for (const ICFGNode* u : keptNodesSet) {
        for (const ICFGEdge* e : u->getOutEdges()) {
            const ICFGNode* s = e ? e->getDstNode() : nullptr;
            if (s == nullptr || keptNodesSet.count(s)) continue;   // direct kept edge: not bridged
            for (const ICFGNode* v : reachKept[comp[rid[s]]]) {
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

SlicedPAGView::SlicedPAGView(SVFIR* pag, const std::set<const SVFStmt*>& keptStmts)
    : pag(pag), keptStmts(keptStmts)
{
    buildKeptNodeIds();
}

void SlicedPAGView::buildKeptNodeIds() {
    for (const SVFStmt* stmt : keptStmts) {
        // extract the node IDs involved in the statement
        const AssignStmt* assignStmt = SVFUtil::dyn_cast<AssignStmt>(stmt);
        if (assignStmt) {
            keptNodeIds.insert(assignStmt->getLHSVarID());
            keptNodeIds.insert(assignStmt->getRHSVarID());
            continue;
        }

        const LoadStmt* loadStmt = SVFUtil::dyn_cast<LoadStmt>(stmt);
        if (loadStmt) {
            keptNodeIds.insert(loadStmt->getLHSVarID());
            keptNodeIds.insert(loadStmt->getRHSVarID());
            continue;
        }

        const StoreStmt* storeStmt = SVFUtil::dyn_cast<StoreStmt>(stmt);
        if (storeStmt) {
            keptNodeIds.insert(storeStmt->getLHSVarID());
            keptNodeIds.insert(storeStmt->getRHSVarID());
            continue;
        }

        const CopyStmt* copyStmt = SVFUtil::dyn_cast<CopyStmt>(stmt);
        if (copyStmt) {
            keptNodeIds.insert(copyStmt->getLHSVarID());
            keptNodeIds.insert(copyStmt->getRHSVarID());
            continue;
        }

        const GepStmt* gepStmt = SVFUtil::dyn_cast<GepStmt>(stmt);
        if (gepStmt) {
            keptNodeIds.insert(gepStmt->getLHSVarID());
            keptNodeIds.insert(gepStmt->getRHSVarID());
            continue;
        }

        const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt);
        if (addrStmt) {
            keptNodeIds.insert(addrStmt->getLHSVarID());
            keptNodeIds.insert(addrStmt->getRHSVarID());
            continue;
        }

        const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt);
        if (callPE) {
            // CallPE is a MultiOpndStmt (result + per-call-site operands)
            // rather than a single-LHS/RHS AssignStmt.
            keptNodeIds.insert(callPE->getResID());
            for (u32_t i = 0; i < callPE->getOpVarNum(); ++i)
                keptNodeIds.insert(callPE->getOpVarID(i));
            continue;
        }

        const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt);
        if (retPE) {
            keptNodeIds.insert(retPE->getLHSVarID());
            keptNodeIds.insert(retPE->getRHSVarID());
            continue;
        }
    }
}

void SlicedPAGView::dump(const std::string& filename) const {
    std::ofstream out;
    if (!SlicedViewAdapter::openDotGraph(out, filename, "PAG", "LR", "shape=box"))
        return;

    // emit nodes
    for (NodeID nodeId : keptNodeIds) {
        const SVFVar* var = pag->getGNode(nodeId);
        if (!var) continue;
        SlicedViewAdapter::emitDotNode(out, "PAG", nodeId, var->toString());
    }

    out << "\n";

    // emit edges
    for (const SVFStmt* stmt : keptStmts) {
        NodeID srcId = 0, dstId = 0;
        std::string edgeLabel;
        std::string color = "black";

        const AssignStmt* assignStmt = SVFUtil::dyn_cast<AssignStmt>(stmt);
        if (assignStmt) {
            srcId = assignStmt->getRHSVarID();
            dstId = assignStmt->getLHSVarID();
            edgeLabel = "Assign";
        }

        const LoadStmt* loadStmt = SVFUtil::dyn_cast<LoadStmt>(stmt);
        if (loadStmt) {
            srcId = loadStmt->getRHSVarID();
            dstId = loadStmt->getLHSVarID();
            edgeLabel = "Load";
            color = "blue";
        }

        const StoreStmt* storeStmt = SVFUtil::dyn_cast<StoreStmt>(stmt);
        if (storeStmt) {
            srcId = storeStmt->getRHSVarID();
            dstId = storeStmt->getLHSVarID();
            edgeLabel = "Store";
            color = "red";
        }

        const CopyStmt* copyStmt = SVFUtil::dyn_cast<CopyStmt>(stmt);
        if (copyStmt) {
            srcId = copyStmt->getRHSVarID();
            dstId = copyStmt->getLHSVarID();
            edgeLabel = "Copy";
        }

        const GepStmt* gepStmt = SVFUtil::dyn_cast<GepStmt>(stmt);
        if (gepStmt) {
            srcId = gepStmt->getRHSVarID();
            dstId = gepStmt->getLHSVarID();
            edgeLabel = "GEP";
            color = "purple";
        }

        const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt);
        if (addrStmt) {
            srcId = addrStmt->getRHSVarID();
            dstId = addrStmt->getLHSVarID();
            edgeLabel = "Addr";
            color = "green";
        }

        const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt);
        if (callPE) {
            // CallPE is a MultiOpndStmt (see above).
            srcId = callPE->getOpVarNum() > 0 ? callPE->getOpVarID(0) : callPE->getResID();
            dstId = callPE->getResID();
            edgeLabel = "CallPE";
            color = "orange";
        }

        const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt);
        if (retPE) {
            srcId = retPE->getRHSVarID();
            dstId = retPE->getLHSVarID();
            edgeLabel = "RetPE";
            color = "cyan";
        }

        if (srcId != 0 && dstId != 0 && keptNodeIds.count(srcId) && keptNodeIds.count(dstId)) {
            std::string srcIdStr = "PAG" + std::to_string(srcId);
            std::string dstIdStr = "PAG" + std::to_string(dstId);
            out << "  " << srcIdStr << " -> " << dstIdStr
                << " [label=\"" << edgeLabel << "\", color=" << color << "];\n";
        }
    }

    SlicedViewAdapter::finishDotGraph(out, filename, "SlicedPAGView", "PAG");
}

//===----------------------------------------------------------------------===//
// SlicedThreadCallGraphView
//===----------------------------------------------------------------------===//

SlicedThreadCallGraphView::SlicedThreadCallGraphView(ThreadCallGraph* tcg,
                                                      const std::set<const FunObjVar*>& keptFunctions,
                                                      const std::set<const ICFGNode*>& extendedKeptNodes)
    : tcg(tcg)
{
    for (const FunObjVar* fun : keptFunctions) {
        keptFunctionsSet.insert(fun);
    }
    this->extendedKeptNodes = extendedKeptNodes;
    buildKeptNodes();
    // extendedKeptNodes already includes all necessary nodes from performSpatialSlicing;
    // buildCallGraphSets builds the kept edges (filtering pruned call sites when it is non-empty).
    buildCallGraphSets();
}

void SlicedThreadCallGraphView::buildKeptNodes() {
    for (CallGraph::iterator it = tcg->begin(), eit = tcg->end(); it != eit; ++it) {
        const CallGraphNode* node = it->second;
        if (node && node->getFunction() && keptFunctionsSet.count(node->getFunction())) {
            keptNodes.insert(node);
        }
    }
}

void SlicedThreadCallGraphView::getOutEdgesOf(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const {
    out.clear();
    if (!isKeptNode(node)) {
        return;
    }

    for (const CallGraphEdge* edge : node->getOutEdges()) {
        const CallGraphNode* dstNode = edge ? edge->getDstNode() : nullptr;
        if (dstNode && isKeptNode(dstNode)) {
            out.push_back(edge);
        }
    }
}

void SlicedThreadCallGraphView::getInEdgesOf(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const {
    out.clear();
    if (!isKeptNode(node)) {
        return;
    }

    for (const CallGraphEdge* edge : node->getInEdges()) {
        const CallGraphNode* srcNode = edge ? edge->getSrcNode() : nullptr;
        if (srcNode && isKeptNode(srcNode)) {
            out.push_back(edge);
        }
    }
}

bool SlicedThreadCallGraphView::isKeptNode(const CallGraphNode* node) const {
    return keptNodes.count(node) > 0;
}

void SlicedThreadCallGraphView::buildCallGraphSets() {
    // rebuild kept edges, accounting for whether the call site is kept
    keptEdges.clear();
    indirectSitesWithEmptyTargets.clear();

    // CallGraph edge: src/dst both in kept functions and the call site still in the kept ICFG node set
    for (const CallGraphNode* srcNode : keptNodes) {
        for (const CallGraphEdge* edge : srcNode->getOutEdges()) {
            const CallGraphNode* dstNode = edge ? edge->getDstNode() : nullptr;
            if (!dstNode || !keptNodes.count(dstNode)) {
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

            if (!extendedKeptNodes.empty() && callSite && !extendedKeptNodes.count(callSite)) {
                // call site was pruned, skip this edge
                continue;
            }

            // indirect call: record if all targets were filtered out
            if (callSite && edge->getEdgeKind() == CallGraphEdge::CallRetEdge &&
                tcg->hasIndCSCallees(callSite)) {
                const CallGraph::FunctionSet& targets = tcg->getIndCSCallees(callSite);
                bool hasKeptTarget = false;
                for (const FunObjVar* tgt : targets) {
                    if (keptFunctionsSet.count(tgt)) {
                        hasKeptTarget = true;
                        break;
                    }
                }
                if (!hasKeptTarget) {
                    indirectSitesWithEmptyTargets.insert(callSite);
                }
            }

            keptEdges.insert(const_cast<CallGraphEdge*>(edge));
        }
    }
}

void SlicedThreadCallGraphView::dump(const std::string& filename) const {
    std::ofstream out;
    if (!SlicedViewAdapter::openDotGraph(out, filename, "ThreadCallGraph", "TB", "shape=box, style=rounded"))
        return;

    for (const CallGraphNode* node : keptNodes)
        SlicedViewAdapter::emitDotNode(out, "CGN", node->getId(), node->getName());

    out << "\n";

    for (const CallGraphEdge* edge : keptEdges) {
        const CallGraphNode* src = edge->getSrcNode();
        const CallGraphNode* dst = edge->getDstNode();
        if (!isKeptNode(src) || !isKeptNode(dst)) {
            continue;
        }

        std::string srcId = "CGN" + std::to_string(src->getId());
        std::string dstId = "CGN" + std::to_string(dst->getId());
        std::string color = "black";

        if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge) {
            color = "green";
        } else if (edge->getEdgeKind() == CallGraphEdge::TDJoinEdge) {
            color = "orange";
        } else if (edge->getEdgeKind() == CallGraphEdge::CallRetEdge) {
            color = "blue";
        }

        out << "  " << srcId << " -> " << dstId << " [color=" << color << "];\n";
    }

    SlicedViewAdapter::finishDotGraph(out, filename, "SlicedThreadCallGraphView", "ThreadCallGraph");
}

//===----------------------------------------------------------------------===//
// SlicedSVFIRView
//===----------------------------------------------------------------------===//

SlicedSVFIRView::SlicedSVFIRView(SVFIR* svfIr,
                                 CallGraph* cg,
                                 ICFG* icfg,
                                 const std::set<const ICFGNode*>& keepNodes)
    : svfIr(svfIr)
{
    // Derive keptFunctions from keepNodes
    std::set<const FunObjVar*> keptFunctions;
    for (const ICFGNode* node : keepNodes) {
        if (node != nullptr && node->getFun() != nullptr) {
            keptFunctions.insert(node->getFun());
        }
    }

    // The MTA pipeline always slices over a ThreadCallGraph.
    ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(cg);
    assert(tcg != nullptr && "SlicedSVFIRView expects a ThreadCallGraph");
    tcgView = std::make_unique<SlicedThreadCallGraphView>(tcg, keptFunctions, keepNodes);

    // Create ICFG view (based on keepNodes and keptFunctions)
    icfgView = std::make_unique<SlicedICFGView>(icfg, cg, keepNodes, keptFunctions);

    // Create PAG view (extract statements from keepNodes)
    std::set<const SVFStmt*> keptStmts;
    for (const ICFGNode* node : keepNodes) {
        const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
        keptStmts.insert(stmts.begin(), stmts.end());
    }
    pagView = std::make_unique<SlicedPAGView>(svfIr, keptStmts);
}

void SlicedSVFIRView::dumpAll(const std::string& prefix) const {
    icfgView->dump(prefix + "_icfg");
    tcgView->dump(prefix + "_threadcallgraph");
    pagView->dump(prefix + "_pag");
}

void SlicedSVFIRView::dumpStats(const std::string& prefix) const {
    std::string label = prefix.empty() ? "[SlicedSVFIRView]" : "[" + prefix + "]";
    SVFUtil::outs() << label << " Statistics:\n";
    SVFUtil::outs() << "  ICFG nodes: " << icfgView->getKeptNodes().size() << "\n";
    SVFUtil::outs() << "  Functions: " << getKeptFunctions().size() << "\n";
    SVFUtil::outs() << "  PAG statements: " << getKeptStatements().size() << "\n";
    if (!getIndirectSitesWithEmptyTargets().empty()) {
        SVFUtil::outs() << "  Indirect callsites that lost all targets: "
                  << getIndirectSitesWithEmptyTargets().size() << "\n";
    }
}

//===----------------------------------------------------------------------===//
// SlicedViewAdapter
//===----------------------------------------------------------------------===//

namespace SlicedViewAdapter
{

std::string escapeDotLabel(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char uc : s)
    {
        const char c = static_cast<char>(uc);
        if (uc == 0x01)
            continue;
        if (std::iscntrl(uc) && c != '\n' && c != '\t')
            continue;
        if (c == '"' || c == '\\')
            out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

bool openDotGraph(std::ofstream& out, const std::string& filename,
                  const char* graphName, const char* rankdir, const char* nodeAttr)
{
    out.open(filename + ".dot");
    if (!out.is_open())
    {
        SVFUtil::errs() << "Failed to open file: " << filename << ".dot" << "\n";
        return false;
    }
    out << "digraph " << graphName << " {\n";
    out << "  rankdir=" << rankdir << ";\n";
    out << "  node [" << nodeAttr << "];\n\n";
    return true;
}

void emitDotNode(std::ofstream& out, const char* idPrefix, NodeID id, const std::string& label)
{
    out << "  " << idPrefix << id << " [label=\"" << escapeDotLabel(label) << "\"];\n";
}

void finishDotGraph(std::ofstream& out, const std::string& filename, const char* tag, const char* what)
{
    out << "}\n";
    out.close();
    SVFUtil::outs() << "[" << tag << "] " << what << " dumped to " << filename << ".dot\n";
}

const ICFGNode* getFunEntry(const SlicedICFGView* icfgView, const FunObjVar* fun)
{
    // Use sliced view if available, otherwise fall back to original entry
    if (icfgView != nullptr)
    {
        // Prefer the kept FunEntryICFGNode: MTASlicer::expandCallDependence keeps it
        // for every kept function and buildBridgedEdges links it to the kept body, so
        // the MHP interleaving fixpoint can flow from it to every kept statement.
        // The entry basic block's first instruction, by contrast, may be sliced out;
        // returning it (a removed node) would strand the root/thread seed there --
        // getSuccNodes() yields nothing for a non-kept node -- so the function body
        // would never receive the thread's interleaving (a soundness bug).
        if (ICFG* icfg = icfgView->getOriginalICFG())
        {
            if (const ICFGNode* fe = icfg->getFunEntryICFGNode(fun))
            {
                if (icfgView->isKeptNode(fe))
                    return fe;
            }
        }
        // Otherwise: first kept node in the entry block, or the original entry.
        const ICFGNode* entry = fun->getEntryBlock()->front();
        if (icfgView->isKeptNode(entry))
            return entry;
        for (const ICFGNode* node : fun->getEntryBlock()->getICFGNodeList())
        {
            if (icfgView->isKeptNode(node))
                return node;
        }
        return entry;
    }
    return fun->getEntryBlock()->front();
}

void getFunICFGNodes(const SlicedICFGView* icfgView, const FunObjVar* fun,
                     std::vector<const ICFGNode*>& out)
{
    out.clear();
    for (auto it : *fun)
    {
        const SVFBasicBlock* svfbb = it.second;
        for (const ICFGNode* node : svfbb->getICFGNodeList())
        {
            // No view: keep all nodes; with a view: keep only kept nodes.
            if (icfgView == nullptr || icfgView->isKeptNode(node))
            {
                out.push_back(node);
            }
        }
    }
}

void getSuccNodes(const SlicedICFGView* icfgView, const ICFGNode* node,
                  std::vector<const ICFGNode*>& out)
{
    if (icfgView != nullptr)
    {
        icfgView->getSuccNodes(node, out);
    }
    else
    {
        // Fall back to full ICFG
        out.clear();
        for (const ICFGEdge* e : node->getOutEdges())
            out.push_back(e->getDstNode());
    }
}

void projectSeedToKept(const SlicedICFGView* icfgView, const ICFGNode* node,
                       std::vector<const ICFGNode*>& out)
{
    out.clear();
    // No view, or the seed is already kept: use it directly.
    if (icfgView == nullptr || icfgView->isKeptNode(node))
    {
        out.push_back(node);
        return;
    }
    // The seed was sliced out. Walk forward over the full ICFG (staying in the
    // same function, like the interleaving propagation does) to the first kept
    // node(s) on each path -- those are where the slice resumes, so seeding them
    // lets the interleaving continue instead of stranding on the removed node.
    const FunObjVar* fun = node->getFun();
    std::unordered_set<const ICFGNode*> visited;
    std::deque<const ICFGNode*> worklist;
    visited.insert(node);
    worklist.push_back(node);
    while (!worklist.empty())
    {
        const ICFGNode* cur = worklist.front();
        worklist.pop_front();
        for (const ICFGEdge* e : cur->getOutEdges())
        {
            const ICFGNode* dst = e->getDstNode();
            if (dst->getFun() != fun || !visited.insert(dst).second)
                continue;
            if (icfgView->isKeptNode(dst))
                out.push_back(dst);   // first kept node on this path; stop descending
            else
                worklist.push_back(dst);
        }
    }
}

void getPredNodes(const SlicedICFGView* icfgView, const ICFGNode* node,
                  std::vector<const ICFGNode*>& out)
{
    if (icfgView != nullptr)
    {
        icfgView->getPredNodes(node, out);
    }
    else
    {
        // Fall back to full ICFG
        out.clear();
        for (const ICFGEdge* e : node->getInEdges())
            out.push_back(e->getSrcNode());
    }
}

bool acceptsNode(const SlicedICFGView* icfgView, const ICFGNode* node)
{
    // Accept all nodes when there is no sliced view.
    return icfgView == nullptr ? true : icfgView->isKeptNode(node);
}

void getInEdgesOfCallGraphNode(const SlicedSVFIRView* slicedView, const CallGraphNode* node,
                               std::vector<const CallGraphEdge*>& out)
{
    out.clear();

    // Use the sliced ThreadCallGraph view if available.
    if (slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr)
    {
        slicedView->getThreadCallGraph()->getInEdgesOf(node, out);
    }
    else
    {
        // Fall back to the original CallGraphNode in-edges.
        for (CallGraphEdge* edge : node->getInEdges())
        {
            out.push_back(edge);
        }
    }
}

const CallGraph* getAnalysisCallGraph(const SlicedSVFIRView* slicedView)
{
    if (slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr)
        return slicedView->getThreadCallGraph()->getOriginalCallGraph();
    return PAG::getPAG()->getCallGraph();
}

} // namespace SlicedViewAdapter

//===----------------------------------------------------------------------===//
// SlicedTCT - TCT rebuilt over the sliced ThreadCallGraph view.
//===----------------------------------------------------------------------===//

SlicedTCT::SlicedTCT(PointerAnalysis* p, const SlicedSVFIRView* slicedView, u32_t maxContextLen)
    : TCT(p),
      tcgView(slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr ? slicedView->getThreadCallGraph() : nullptr),
      maxContextLen(maxContextLen)
{
    // Base class TCT(p) constructor called build() before tcgView was initialized.
    // Now that tcgView is initialized, rebuild using sliced view.
    if (tcgView != nullptr)
    {
        build();
    }
}

void SlicedTCT::build()
{
    if (tcgView == nullptr)
    {
        TCT::build();
        return;
    }

    // The base TCT(p) constructor already built the whole-program TCT. Reset it to
    // a clean graph so the slice rebuilds from scratch (addGNode reissues ids from
    // 0; stale nodes left behind would later fail the MHP getCxtOfCxtThread lookup).
    reset();

    // Call base class build() which will call our overridden methods
    // (markRelProcs, collectLoopInfoForJoin, collectEntryFunInCallGraph)
    // that filter using sliced view, then build the TCT tree
    TCT::build();
}

void SlicedTCT::markRelProcs()
{
    if (tcgView == nullptr)
    {
        TCT::markRelProcs();
        return;
    }

    // Get kept fork sites from sliced view
    std::vector<const ICFGNode*> keptForkSites;
    getKeptForkSites(keptForkSites);

    for (const ICFGNode* forkSite : keptForkSites)
    {
        // Get function from fork site
        const FunObjVar* svfun = forkSite->getFun();
        TCT::markRelProcs(svfun);

        // Get fork edges from sliced view (use tcgView to get out edges)
        const CallICFGNode* callNode = SVFUtil::cast<CallICFGNode>(forkSite);
        const FunObjVar* callerFun = callNode->getFun();
        CallGraphNode* callerNode = tcg->getCallGraphNode(callerFun);

        if (!isKeptNode(callerNode))
            continue;

        // Use sliced view to get out edges (which filters by kept edges)
        std::vector<const CallGraphEdge*> outEdges;
        tcgView->getOutEdgesOf(callerNode, outEdges);

        for (const CallGraphEdge* edge : outEdges)
        {
            // Check if this is a fork edge
            if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge)
            {
                const CallGraphNode* forkeeNode = edge->getDstNode();
                if (isKeptNode(forkeeNode))
                {
                    // Check if this edge corresponds to the fork site
                    const CallGraphEdge::CallInstSet& directCalls = edge->getDirectCalls();
                    const CallGraphEdge::CallInstSet& indirectCalls = edge->getIndirectCalls();

                    if (directCalls.count(callNode) > 0 || indirectCalls.count(callNode) > 0)
                    {
                        const FunObjVar* forkeeFun = forkeeNode->getFunction();
                        candidateFuncSet.insert(forkeeFun);
                    }
                }
            }
        }
    }

    // Get kept join sites from sliced view
    std::vector<const ICFGNode*> keptJoinSites;
    getKeptJoinSites(keptJoinSites);

    for (const ICFGNode* joinSite : keptJoinSites)
    {
        const FunObjVar* svfun = joinSite->getFun();
        TCT::markRelProcs(svfun);
    }

    if(getMakredProcs().empty())
        SVFUtil::writeWrnMsg("We didn't recognize any fork site, this is single thread program?");
}

void SlicedTCT::markRelProcs(const FunObjVar* fun)
{
    // Call base class implementation
    TCT::markRelProcs(fun);
}

void SlicedTCT::collectLoopInfoForJoin()
{
    if (tcgView == nullptr)
    {
        TCT::collectLoopInfoForJoin();
        return;
    }

    // Get kept join sites from sliced view
    std::vector<const ICFGNode*> keptJoinSites;
    getKeptJoinSites(keptJoinSites);

    for(const ICFGNode* join : keptJoinSites)
    {
        const FunObjVar* svffun = join->getFun();
        const SVFBasicBlock* svfbb = join->getBB();

        if(svffun->hasLoopInfo(svfbb))
        {
            const LoopBBs& lp = svffun->getLoopInfo(svfbb);
            if(!lp.empty() && isJoinMustExecutedInLoop(lp,join))
            {
                joinSiteToLoopMap[join] = lp;
            }
        }

        if(isInRecursion(join))
        {
            inRecurJoinSites.insert(join);
        }
    }
}

void SlicedTCT::collectEntryFunInCallGraph()
{
    if (tcgView == nullptr)
    {
        TCT::collectEntryFunInCallGraph();
        return;
    }

    // Use sliced CallGraph view to collect entry functions
    // Only collect functions that are kept in sliced view
    const std::set<const CallGraphNode*>& keptNodes = tcgView->getKeptNodes();

    for (const CallGraphNode* node : keptNodes)
    {
        const FunObjVar* fun = node->getFunction();
        if (SVFUtil::isExtCall(fun))
            continue;

        // Check if this node has no incoming edges in the sliced view
        std::vector<const CallGraphEdge*> inEdges;
        tcgView->getInEdgesOf(node, inEdges);
        if (inEdges.empty())
        {
            entryFuncSet.insert(fun);
        }
    }

    assert(!getEntryProcs().empty() && "Can't find any function in module!");
}

void SlicedTCT::handleCallRelation(CxtThreadProc& ctp, const CallGraphEdge* cgEdge, const CallICFGNode* cs)
{
    // Check if the call site and callee are kept in sliced view
    if (!isKeptEdge(cgEdge))
        return;

    const CallGraphNode* dstNode = cgEdge->getDstNode();
    if (!isKeptNode(dstNode))
        return;

    // Call base class implementation
    TCT::handleCallRelation(ctp, cgEdge, cs);
}

void SlicedTCT::pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    const FunObjVar* caller = call->getFun();
    CallSiteID csId = tcg->getCallSiteID(call, callee);

    if(inSameCallGraphSCC(tcg->getCallGraphNode(caller), tcg->getCallGraphNode(callee)) == false)
    {
        cxt.push_back(csId);
        // Use custom maxContextLen if set, otherwise use Options::MaxContextLen()
        u32_t maxLen = (maxContextLen > 0) ? maxContextLen : Options::MaxContextLen();
        if (cxt.size() > maxLen)
            cxt.erase(cxt.begin());
        if (cxt.size() > MaxCxtSize)
            MaxCxtSize = cxt.size();
        DBOUT(DMTA, dumpCxt(cxt));
    }
}

bool SlicedTCT::isKeptNode(const CallGraphNode* node) const
{
    if (tcgView == nullptr)
        return true;
    return tcgView->isKeptNode(node);
}

bool SlicedTCT::isKeptEdge(const CallGraphEdge* edge) const
{
    if (tcgView == nullptr)
        return true;
    const CallGraph::CallGraphEdgeSet& keptEdges = tcgView->getKeptEdges();
    return keptEdges.find(const_cast<CallGraphEdge*>(edge)) != keptEdges.end();
}

void SlicedTCT::getKeptForkSites(std::vector<const ICFGNode*>& out) const
{
    out.clear();
    if (tcgView == nullptr)
    {
        // Fall back to original
        for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
        {
            out.push_back(*it);
        }
        return;
    }

    // Get all fork sites from original ThreadCallGraph, but filter by:
    // 1. The function containing the fork site is kept
    // 2. There exists a kept fork edge from this fork site
    for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
    {
        const ICFGNode* forkSite = *it;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(forkSite);
        if (callNode == nullptr)
            continue;

        // Check if the function containing the fork site is kept
        const FunObjVar* fun = forkSite->getFun();
        CallGraphNode* cgNode = tcg->getCallGraphNode(fun);
        if (!isKeptNode(cgNode))
            continue;

        // Check if there's a kept fork edge from this fork site
        // Use sliced view to get out edges
        std::vector<const CallGraphEdge*> outEdges;
        tcgView->getOutEdgesOf(cgNode, outEdges);

        bool hasKeptForkEdge = false;
        for (const CallGraphEdge* edge : outEdges)
        {
            if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge)
            {
                // Check if this edge corresponds to the fork site
                const CallGraphEdge::CallInstSet& directCalls = edge->getDirectCalls();
                const CallGraphEdge::CallInstSet& indirectCalls = edge->getIndirectCalls();

                if (directCalls.count(callNode) > 0 || indirectCalls.count(callNode) > 0)
                {
                    // Check if the destination is kept
                    const CallGraphNode* dstNode = edge->getDstNode();
                    if (isKeptNode(dstNode))
                    {
                        hasKeptForkEdge = true;
                        break;
                    }
                }
            }
        }

        if (hasKeptForkEdge)
        {
            out.push_back(forkSite);
        }
    }
}

void SlicedTCT::getKeptJoinSites(std::vector<const ICFGNode*>& out) const
{
    out.clear();
    if (tcgView == nullptr)
    {
        // Fall back to original
        for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->joinsitesBegin(), eit = tcg->joinsitesEnd(); it != eit; ++it)
        {
            out.push_back(*it);
        }
        return;
    }

    // Get all join sites from original ThreadCallGraph, but filter by:
    // 1. The function containing the join site is kept
    // 2. There exists a kept join edge to this join site
    for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->joinsitesBegin(), eit = tcg->joinsitesEnd(); it != eit; ++it)
    {
        const ICFGNode* joinSite = *it;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(joinSite);
        if (callNode == nullptr)
            continue;

        // Check if the function containing the join site is kept
        const FunObjVar* fun = joinSite->getFun();
        CallGraphNode* cgNode = tcg->getCallGraphNode(fun);
        if (!isKeptNode(cgNode))
            continue;

        // Check if there's a kept join edge to this join site
        // Use sliced view to get in edges
        std::vector<const CallGraphEdge*> inEdges;
        tcgView->getInEdgesOf(cgNode, inEdges);

        bool hasKeptJoinEdge = false;
        for (const CallGraphEdge* edge : inEdges)
        {
            if (edge->getEdgeKind() == CallGraphEdge::TDJoinEdge)
            {
                // Check if this edge corresponds to the join site
                const CallGraphEdge::CallInstSet& directCalls = edge->getDirectCalls();
                const CallGraphEdge::CallInstSet& indirectCalls = edge->getIndirectCalls();

                if (directCalls.count(callNode) > 0 || indirectCalls.count(callNode) > 0)
                {
                    // Check if the source is kept
                    const CallGraphNode* srcNode = edge->getSrcNode();
                    if (isKeptNode(srcNode))
                    {
                        hasKeptJoinEdge = true;
                        break;
                    }
                }
            }
        }

        if (hasKeptJoinEdge)
        {
            out.push_back(joinSite);
        }
    }
}

//===----------------------------------------------------------------------===//
// SlicerBase
//===----------------------------------------------------------------------===//

SlicerBase::SlicerBase(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                       LockAnalysis* lockAnalysis, SVF::SVFG* vfg)
    : svfIr(svfIr), pta(pta), mhp(mhp), lockAnalysis(lockAnalysis), vfg(vfg) {
    callGraph = pta->getCallGraph();
}

SlicerBase::~SlicerBase() {
    // No dynamic memory to clean up - all pointers are managed externally
}

// Helper: Get lock set for an ICFG node
std::set<const ICFGNode*> SlicerBase::getLockSet(const ICFGNode* node) {
    std::set<const ICFGNode*> allLockSites;

    // Get intra-procedural locks
    if (lockAnalysis->isInsideIntraLock(node) &&
        !lockAnalysis->isInsideCondIntraLock(node)) {
        const LockAnalysis::InstSet& intraLocks = lockAnalysis->getIntraLockSet(node);
        for (const ICFGNode* lockSite : intraLocks) {
            allLockSites.insert(lockSite);
        }
    }

    // Get context-sensitive locks
    if (lockAnalysis->hasCxtStmtFromInst(node)) {
        const LockAnalysis::CxtStmtSet& cxtStmts = lockAnalysis->getCxtStmtsFromInst(node);
        for (const CxtStmt& cxtStmt : cxtStmts) {
            if (lockAnalysis->hasCxtLockfromCxtStmt(cxtStmt)) {
                const LockAnalysis::CxtLockSet& cxtLocks =
                    lockAnalysis->getCxtLockfromCxtStmt(cxtStmt);
                for (const LockAnalysis::CxtLock& cxtLock : cxtLocks) {
                    allLockSites.insert(cxtLock.getStmt());
                }
            }
        }
    }

    return allLockSites;
}

// Helper: Get TCTNode set from ICFGNode
std::set<const TCTNode*> SlicerBase::getTCTNodeSetFromNode(const ICFGNode* node) {
    std::set<const TCTNode*> tctNodeSet;

    if (mhp->hasThreadStmtSet(node)) {
        for (const CxtThreadStmt& cts : mhp->getThreadStmtSet(node)) {
            if (mhp->getTCT()->hasGNode(cts.getTid())) {
                tctNodeSet.insert(mhp->getTCT()->getTCTNode(cts.getTid()));
            }
        }
    }

    return tctNodeSet;
}

// Helper: Get dependent thread create statements
std::set<const SVFStmt*> SlicerBase::getDependentThreadCreate(const SVFStmt* stmt) {
    std::set<const SVFStmt*> forkSiteStmts;

    const ICFGNode* icfgNode = stmt->getICFGNode();
    std::set<const TCTNode*> tctNodeSet = getTCTNodeSetFromNode(icfgNode);

    for (const TCTNode* tctNode : tctNodeSet) {
        const CxtThread& cxtThread = tctNode->getCxtThread();
        const ICFGNode* forkSite = cxtThread.getThread();
        if (forkSite != nullptr) {
            const ICFGNode::SVFStmtList& svfStmts = forkSite->getSVFStmts();
            if (!svfStmts.empty()) {
                forkSiteStmts.insert(svfStmts.front());
            }
        }
    }

    return forkSiteStmts;
}

// Data-dependence slice over the thread-aware SVFG (VFG_pre), at SVFG-node
// granularity: the value-flow nodes reachable backward from the seeds. The
// value-flow edges already capture direct (top-level), indirect (address-taken
// / MemSSA), and thread-aware (interference) data dependence.
std::set<const SVFGNode*> SlicerBase::computeDataDependenceSVFGNodes(
    const std::set<const SVFStmt*>& seeds, SVF::SVFG* vfg) {

    assert(vfg != nullptr && "data-dependence slice requires the thread-aware VFG_pre");

    std::set<const SVFGNode*> visited;
    std::deque<const SVFGNode*> worklist;
    auto seed = [&](const SVFGNode* n) {
        if (n != nullptr && visited.insert(n).second)
            worklist.push_back(n);
    };

    // Seed from the value-flow nodes of the given (e.g. race target) statements.
    // VFG_pre is a pointer-only SVFG, so a load/store of a NON-pointer value (the
    // usual case -- a race on an int/float field) has no statement node. For those
    // we must still preserve the points-to of the dereferenced address pointer, or
    // the sliced flow-sensitive solve sees an empty slice for it, computes empty
    // points-to, and drops the race (a soundness bug). So additionally seed from the
    // definition of each load/store's address pointer (always a pointer, hence
    // always in the pointer-only SVFG); its backward closure keeps the pointer's
    // def chain regardless of the value type.
    for (const SVFStmt* stmt : seeds)
    {
        if (vfg->hasStmtVFGNode(stmt))
            seed(vfg->getStmtVFGNode(stmt));

        NodeID addrPtr = 0;
        if (const LoadStmt* ld = SVFUtil::dyn_cast<LoadStmt>(stmt))
            addrPtr = ld->getRHSVarID();
        else if (const StoreStmt* st = SVFUtil::dyn_cast<StoreStmt>(stmt))
            addrPtr = st->getLHSVarID();
        if (addrPtr != 0)
        {
            // getDefSVFGNode takes a ValVar (the address pointer is a top-level
            // value variable).
            const ValVar* ptrNode = SVFUtil::dyn_cast<ValVar>(svfIr->getGNode(addrPtr));
            if (ptrNode != nullptr && vfg->hasDefSVFGNode(ptrNode))
                seed(vfg->getDefSVFGNode(ptrNode));
        }
    }

    // Backward over every value-flow edge.
    while (!worklist.empty()) {
        const SVFGNode* n = worklist.front();
        worklist.pop_front();
        for (const VFGEdge* e : n->getInEdges())
            seed(e->getSrcNode());
    }

    return visited;
}

// Project retained VFG nodes (plus the seeds) onto their ICFG nodes.
std::set<const ICFGNode*> SlicerBase::svfgNodesToICFGNodes(
    const std::set<const SVFGNode*>& nodes, const std::set<const SVFStmt*>& seeds) {
    std::set<const ICFGNode*> result;
    for (const SVFGNode* n : nodes)
        if (const StmtVFGNode* s = SVFUtil::dyn_cast<StmtVFGNode>(n))
            if (s->getICFGNode() != nullptr)
                result.insert(s->getICFGNode());
    for (const SVFStmt* stmt : seeds)
        result.insert(stmt->getICFGNode());
    return result;
}

std::set<const ICFGNode*> SlicerBase::sliceDataDependenceOverVFG(
    const std::set<const SVFStmt*>& seeds, SVF::SVFG* vfg) {
    return svfgNodesToICFGNodes(computeDataDependenceSVFGNodes(seeds, vfg), seeds);
}

// Helper: Collect pthread-related statements (create and join)
std::set<const CallICFGNode*> SlicerBase::collectPthreadStatements(
    const std::set<const SVFStmt*>& vulnerableStmts) {
    std::set<const CallICFGNode*> pthreadCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map pthread_create nodes to their corresponding pthread_join nodes
    std::set<const SVFStmt*> pthreadCreateStmts;

    // First pass: collect all pthread_create nodes
    for (const SVFStmt* stmt : vulnerableStmts) {
        std::set<const SVFStmt*> forkSiteStmts = getDependentThreadCreate(stmt);

        for (const SVFStmt* forkSiteStmt : forkSiteStmts) {
            const ICFGNode* forkSiteNode = forkSiteStmt->getICFGNode();
            const CallICFGNode* forkCallNode = SVFUtil::dyn_cast<CallICFGNode>(forkSiteNode);
            if (forkCallNode != nullptr && threadAPI->isTDFork(forkCallNode)) {
                pthreadCallNodes.insert(forkCallNode);
                pthreadCreateStmts.insert(forkSiteStmt);
            }
        }
    }

    // Second pass: find corresponding pthread_join nodes
    ICFG* icfg = svfIr->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDJoin(callNode)) {
            const SVFVar* joinThread = threadAPI->getJoinedThread(callNode);
            if (joinThread != nullptr) {
                for (auto& createStmt : pthreadCreateStmts) {
                    const ICFGNode* createNode = createStmt->getICFGNode();
                    const CallICFGNode* createCallNode = SVFUtil::dyn_cast<CallICFGNode>(createNode);
                    if (createCallNode != nullptr) {
                        const SVFVar* forkedThread = threadAPI->getForkedThread(createCallNode);
                        if (forkedThread != nullptr && threadAPI->isAliasedForkJoin(pta, forkedThread, joinThread)) {
                                pthreadCallNodes.insert(callNode);
                        }
                    }
                }
            }
        }
    }

    return pthreadCallNodes;
}

// Helper: Collect mutex-related statements (lock and unlock)
std::set<const CallICFGNode*> SlicerBase::collectMutexStatements(
    const std::set<const SVFStmt*>& vulnerableStmts) {
    std::set<const CallICFGNode*> mutexCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map mutex_lock nodes to their corresponding mutex_unlock nodes
    std::set<const CallICFGNode*> mutexLockCallNodes;

    // First pass: collect all mutex_lock nodes from lock sets
    for (const SVFStmt* stmt : vulnerableStmts) {
        std::set<const ICFGNode*> lockSet = getLockSet(stmt->getICFGNode());
        for (const ICFGNode* lockNode : lockSet) {
            const CallICFGNode* lockCallNode = SVFUtil::dyn_cast<CallICFGNode>(lockNode);
            if (lockCallNode != nullptr && threadAPI->isTDAcquire(lockCallNode)) {
                mutexCallNodes.insert(lockCallNode);
                mutexLockCallNodes.insert(lockCallNode);
            }
        }
    }

    // Second pass: find corresponding mutex_unlock nodes
    ICFG* icfg = svfIr->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDRelease(callNode)) {
            const SVFVar* unlockVar = threadAPI->getLockVal(callNode);
            if (unlockVar != nullptr) {
                for (auto lockCallNode : mutexLockCallNodes) {
                    if (lockCallNode != nullptr) {
                        const SVFVar* lockVar = threadAPI->getLockVal(lockCallNode);
                        if (lockVar != nullptr && pta->alias(unlockVar->getId(), lockVar->getId())) {
                            mutexCallNodes.insert(callNode);
                        }
                    }
                }
            }
        }
    }

    return mutexCallNodes;
}

// Helper: Collect common pthread and mutex statements (shared by PTA and MTA slicing)
std::pair<std::set<const CallICFGNode*>, std::set<const CallICFGNode*>>
SlicerBase::collectCommonThreadStatements(const std::set<const SVFStmt*>& vulnerableStatements) {
    // Step 1: Collect pthread-related statements, i.e., pthread_create and pthread_join
    std::set<const CallICFGNode*> pthreadCallNodes = collectPthreadStatements(vulnerableStatements);

    // Step 2: Collect mutex-related statements
    std::set<const CallICFGNode*> mutexCallNodes = collectMutexStatements(vulnerableStatements);

    return std::make_pair(pthreadCallNodes, mutexCallNodes);
}

// Keep the control-flow marker nodes the (sliced) lock analysis depends on:
// every lock/unlock-bearing function's entry and exit nodes.
//
// LockAnalysis classifies an intra lock as *partial* (conditional) by reaching
// the function entry node from the unlock along a lock-free backward path
// (intraBackwardTraverse: `entryInst == I` -> return false), and bails the
// forward span at the function exit node (`exitInst == I`). These checks are
// node-identity tests against the entry/exit markers. The data-dependence slice
// does not otherwise retain those markers, so on the sliced view the backward
// walk can never match the entry node and the lock is mis-classified as total --
// which makes isProtectedByCommonCILock report a spurious common lock and drops
// a real race (a query-preservation violation). Bridging preserves reachability
// *to* the kept markers, so once they are retained the sliced lock analysis
// reproduces the whole-program classification. Markers used: entry block
// front (cxt-lock start) and back (intra backward marker), and exit block back
// (intra forward marker).
static void collectLockFunctionMarkers(
    const std::set<const CallICFGNode*>& mutexCallNodes,
    std::set<const ICFGNode*>& sliceResult) {
    for (const CallICFGNode* mutexCallNode : mutexCallNodes) {
        const FunObjVar* fun = mutexCallNode->getFun();
        if (fun == nullptr)
            continue;
        if (const SVFBasicBlock* entry = fun->getEntryBlock()) {
            sliceResult.insert(entry->front());
            sliceResult.insert(entry->back());
        }
        if (const SVFBasicBlock* exit = fun->getExitBB())
            sliceResult.insert(exit->back());
    }
}

// Build backward ICFG node set from vulnerable nodes
std::set<const ICFGNode*> SlicerBase::buildBackwardICFGNodeSet(
    const std::set<const ICFGNode*>& vulnerableNodes) {
    std::set<const ICFGNode*> backwardICFGNodeSet;
    std::deque<const ICFGNode*> worklist;

    // Initialize with vulnerable nodes
    for (const ICFGNode* node : vulnerableNodes) {
        backwardICFGNodeSet.insert(node);
        worklist.push_back(node);
    }

    // Traverse backward along ICFG edges
    while (!worklist.empty()) {
        const ICFGNode* currICFGNode = worklist.front();
        worklist.pop_front();

        for (const ICFGEdge* inEdge : currICFGNode->getInEdges()) {
            const ICFGNode* srcNode = inEdge->getSrcNode();
            if (backwardICFGNodeSet.find(srcNode) == backwardICFGNodeSet.end()) {
                backwardICFGNodeSet.insert(srcNode);
                worklist.push_back(srcNode);
            }
        }
    }

    return backwardICFGNodeSet;
}

// Perform dual slicing (temporal slicing): filter statements based on control flow and parallel execution
std::set<const ICFGNode*> SlicerBase::runDualSlicing(
    const std::set<const ICFGNode*>& slicedNodes) {
    std::set<const ICFGNode*> dualSlicedNodes;

    // Build backward ICFG node set
    std::set<const ICFGNode*> backwardICFGNodeSet = buildBackwardICFGNodeSet(slicedNodes);

    // Perform control slicing
    for (const ICFGNode* stmtICFGNode : slicedNodes) {
        // Check if the ICFG node is in backward_icfg_node_set
        if (backwardICFGNodeSet.find(stmtICFGNode) != backwardICFGNodeSet.end()) {
            dualSlicedNodes.insert(stmtICFGNode);
        } else {
            // Check if it may happen in parallel with any vulnerable node
            for (const ICFGNode* bugICFGNode : slicedNodes) {
                if (mhp->mayHappenInParallelCache(stmtICFGNode, bugICFGNode)) {
                    dualSlicedNodes.insert(stmtICFGNode);
                    break;
                }
            }
        }
    }

    return dualSlicedNodes;
}

// Call-dependence expansion (used by MTASlicer).
std::set<const ICFGNode*> SlicerBase::expandCallDependence(
    const std::set<const ICFGNode*>& nodes) {

    // Determine keptFunctions from the given nodes
    std::set<const FunObjVar*> keptFunctions;
    for (const ICFGNode* node : nodes) {
        if (node != nullptr && node->getFun() != nullptr) {
            keptFunctions.insert(node->getFun());
        }
    }

    // Build ancestor closure (upward traversal in call graph)
    std::queue<const FunObjVar*> worklistFuncs;
    for (const FunObjVar* fun : keptFunctions) {
        worklistFuncs.push(fun);
    }

    std::unordered_map<const FunObjVar*, const CallGraphNode*> fun2Node;
    for (auto it = callGraph->begin(), eit = callGraph->end(); it != eit; ++it) {
        const CallGraphNode* node = it->second;
        if (node && node->getFunction()) {
            fun2Node[node->getFunction()] = node;
        }
    }

    std::set<const FunObjVar*> visitedFuncs = keptFunctions;
    while (!worklistFuncs.empty()) {
        const FunObjVar* target = worklistFuncs.front();
        worklistFuncs.pop();
        auto nodeIt = fun2Node.find(target);
        if (nodeIt == fun2Node.end()) continue;

        const CallGraphNode* node = nodeIt->second;
        for (const CallGraphEdge* inEdge : node->getInEdges()) {
            if (inEdge == nullptr) continue;
            const CallGraphNode* callerNode = inEdge->getSrcNode();
            if (callerNode && callerNode->getFunction()) {
                const FunObjVar* callerFun = callerNode->getFunction();
                if (visitedFuncs.find(callerFun) == visitedFuncs.end()) {
                    keptFunctions.insert(callerFun);
                    visitedFuncs.insert(callerFun);
                    worklistFuncs.push(callerFun);
                }
            }
        }
    }

    // For each keptFunction, add call/ret nodes and entry/exit nodes
    ICFG* icfg = svfIr->getICFG();
    std::set<const ICFGNode*> expandedNodes = nodes;
    for (const FunObjVar* fun : keptFunctions) {
        if (!fun) continue;

        // Add function entry/exit nodes
        if (fun->hasBasicBlock()) {
            if (FunEntryICFGNode* entry = icfg->getFunEntryICFGNode(fun)) {
                expandedNodes.insert(entry);
            }
            if (FunExitICFGNode* exit = icfg->getFunExitICFGNode(fun)) {
                expandedNodes.insert(exit);
            }
        }

        // Find all call/ret nodes that call this function
        auto funNodeIt = fun2Node.find(fun);
        if (funNodeIt != fun2Node.end()) {
            const CallGraphNode* calleeNode = funNodeIt->second;

            // Traverse all edges that call this function
            for (const CallGraphEdge* inEdge : calleeNode->getInEdges()) {
                if (inEdge == nullptr) continue;

                const CallGraphEdge::CallInstSet &directCalls = inEdge->getDirectCalls();
                const CallGraphEdge::CallInstSet &indirectCalls = inEdge->getIndirectCalls();

                for (const CallICFGNode* callNode : directCalls) {
                    if (callNode != nullptr) {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr) {
                            expandedNodes.insert(retNode);
                        }
                    }
                }

                for (const CallICFGNode* callNode : indirectCalls) {
                    if (callNode != nullptr) {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr) {
                            expandedNodes.insert(retNode);
                        }
                    }
                }
            }
        }
    }

    return expandedNodes;
}

//===----------------------------------------------------------------------===//
// MTASlicer
//===----------------------------------------------------------------------===//

MTASlicer::MTASlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                     LockAnalysis* lockAnalysis)
    : SlicerBase(svfIr, pta, mhp, lockAnalysis) {
}

// Perform slicing for MTA (includes function expansion for IRView)
std::set<const ICFGNode*> MTASlicer::runSlicing(
    const std::set<const SVFStmt*>& vulnerableStatements,
    const std::set<const ICFGNode*>& threadVFSources) {

    // Step 1: Collect common pthread and mutex statements
    auto commonStmts = collectCommonThreadStatements(vulnerableStatements);
    const std::set<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const std::set<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Step 2: Form initial slice result (before function expansion). The ILA
    // slicing sources are the [INIT] race statements plus the [THREAD-VF]
    // sources (MSli §4.2) extracted while building VFG_pre: the endpoints and
    // in-span lock witnesses queried during main-phase value-flow construction.
    std::set<const ICFGNode*> initialSliceResult;
    initialSliceResult.insert(pthreadCallNodes.begin(), pthreadCallNodes.end());
    for (const CallICFGNode* pthreadCallNode : pthreadCallNodes) {
        initialSliceResult.insert(pthreadCallNode->getRetICFGNode());
    }
    initialSliceResult.insert(mutexCallNodes.begin(), mutexCallNodes.end());
    for (const CallICFGNode* mutexCallNode : mutexCallNodes) {
        initialSliceResult.insert(mutexCallNode->getRetICFGNode());
    }
    collectLockFunctionMarkers(mutexCallNodes, initialSliceResult);
    for (const SVFStmt* stmt: vulnerableStatements) {
        initialSliceResult.insert(stmt->getICFGNode());
    }
    initialSliceResult.insert(threadVFSources.begin(), threadVFSources.end());

    // Step 3: Perform dual slicing (temporal slicing)
    std::set<const ICFGNode*> dualSlicedNodes = runDualSlicing(initialSliceResult);

    // Step 4: Expand keptNodes to include call/ret nodes and function entry/exit
    // nodes (call dependence).
    return expandCallDependence(dualSlicedNodes);
}

//===----------------------------------------------------------------------===//
// PTASlicer
//===----------------------------------------------------------------------===//

PTASlicer::PTASlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                     LockAnalysis* lockAnalysis, SVF::SVFG* vfg)
    : SlicerBase(svfIr, pta, mhp, lockAnalysis, vfg) {
}

// The FSPTA data-dependence slice at SVFG-node granularity (memoised so the
// backward closure over VFG_pre is computed once and shared with ILA slicing).
const std::set<const SVFGNode*>& PTASlicer::getRetainedSVFGNodes(
    const std::set<const SVFStmt*>& vulnerableStatements) {
    if (!retainedComputed) {
        retainedSVFGNodes = computeDataDependenceSVFGNodes(vulnerableStatements, vfg);
        retainedComputed = true;
    }
    return retainedSVFGNodes;
}

// Perform slicing for pointer analysis (returns only node set, no IRView needed)
std::set<const ICFGNode*> PTASlicer::runSlicing(
    const std::set<const SVFStmt*>& vulnerableStatements) {

    // Step 1: paper-faithful (§4.3) data-dependence slice over the thread-aware
    // SVFG built once in pre-analysis (reusing the memoised SVFG-node closure).
    std::set<const ICFGNode*> initialSliceResult =
        svfgNodesToICFGNodes(getRetainedSVFGNodes(vulnerableStatements), vulnerableStatements);

    // Retain the lock/unlock-function control-flow markers the sliced lock
    // analysis depends on (see collectLockFunctionMarkers); the data-dependence
    // SVFG slice does not otherwise keep them.
    collectLockFunctionMarkers(collectMutexStatements(vulnerableStatements), initialSliceResult);

    // Step 2: Perform dual slicing (temporal slicing)
    std::set<const ICFGNode*> dualSlicedNodes = runDualSlicing(initialSliceResult);

    return dualSlicedNodes;
}

//===----------------------------------------------------------------------===//
// SingleSlicer
//===----------------------------------------------------------------------===//

SingleSlicer::SingleSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                           LockAnalysis* lockAnalysis, SVF::SVFG* vfg)
    : SlicerBase(svfIr, pta, mhp, lockAnalysis, vfg) {
}

// Single-pass slice (the baseline of MSli §3/§5.4): the transitive closure of
// the target statements under the COMBINED dependence graph -- synchronization,
// data, and call dependence -- yielding one slice shared by ILA and FSPTA.
std::set<const ICFGNode*> SingleSlicer::runSlicing(
    const std::set<const SVFStmt*>& vulnerableStatements) {

    // Step 1: Synchronization dependence -- the relevant pthread (fork/join) and
    // mutex (lock/unlock) statements for the targets.
    auto commonStmts = collectCommonThreadStatements(vulnerableStatements);
    const std::set<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const std::set<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Step 2: Seed the working set with the synchronization statements (and their
    // return nodes) and the target statements themselves.
    std::set<const ICFGNode*> currentNodes;
    currentNodes.insert(pthreadCallNodes.begin(), pthreadCallNodes.end());
    for (const CallICFGNode* pthreadCallNode : pthreadCallNodes)
        currentNodes.insert(pthreadCallNode->getRetICFGNode());
    currentNodes.insert(mutexCallNodes.begin(), mutexCallNodes.end());
    for (const CallICFGNode* mutexCallNode : mutexCallNodes)
        currentNodes.insert(mutexCallNode->getRetICFGNode());
    collectLockFunctionMarkers(mutexCallNodes, currentNodes);
    for (const SVFStmt* stmt : vulnerableStatements)
        currentNodes.insert(stmt->getICFGNode());

    // Step 3: Close over data dependence (the thread-aware VFG_pre value flow --
    // direct + indirect + interference, the same model PTASlicer uses) and call
    // dependence (function expansion), alternately, until the node set converges.
    bool changed = true;
    int iteration = 0;
    const int maxIterations = 100; // safety bound against non-convergence
    while (changed && iteration < maxIterations) {
        iteration++;
        std::set<const ICFGNode*> previousNodes = currentNodes;

        std::set<const SVFStmt*> currentStatements;
        for (const ICFGNode* node : currentNodes) {
            const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
            currentStatements.insert(stmts.begin(), stmts.end());
        }

        std::set<const ICFGNode*> dataDepNodes =
            sliceDataDependenceOverVFG(currentStatements, vfg);
        currentNodes.insert(dataDepNodes.begin(), dataDepNodes.end());

        currentNodes = expandCallDependence(currentNodes);

        changed = (currentNodes != previousNodes);
    }

    if (iteration >= maxIterations)
        SVFUtil::writeWrnMsg("SingleSlicer reached max iterations, may not have converged");

    // Step 4: One dual-slicing (temporal) pass at the end.
    return runDualSlicing(currentNodes);
}

} // namespace SVF
