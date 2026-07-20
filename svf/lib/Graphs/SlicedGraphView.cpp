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
 * SlicedGraphView.cpp -- implementations of the general sliced graph views
 * and their shared adapter (moved out of MTA/Slicer.cpp).
 */

#include "Graphs/SlicedGraphView.h"
#include <Util/SVFUtil.h>
#include <Util/ThreadAPI.h>
#include <SVFIR/SVFIR.h>
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
// SlicedICFGView
//===----------------------------------------------------------------------===//

SlicedICFGView::SlicedICFGView(ICFG* icfg,
                               CallGraph* cg,
                               const std::set<const ICFGNode*>& keepNodes,
                               const std::set<const FunObjVar*>& keptFunctions,
                               bool buildBridged)
    : icfg(icfg)
{
    buildICFGSets(keepNodes, keptFunctions);
    if (buildBridged)
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
                                 const std::set<const ICFGNode*>& keepNodes,
                                 bool buildBridged)
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
    icfgView = std::make_unique<SlicedICFGView>(icfg, cg, keepNodes, keptFunctions, buildBridged);

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

} // namespace SVF
