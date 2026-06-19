//===- SlicedView.cpp -- Sliced views of the SVFIR graphs -----------------===//
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
 * SlicedView.cpp
 *
 *      Author: Jiawei Yang
 *
 * SlicedICFGView + SlicedPAGView + SlicedThreadCallGraphView + SlicedSVFIRView +
 * SlicedViewAdapter.
 */

#include "MTA/SlicedView.h"

#include <Graphs/ICFG.h>
#include <Graphs/ICFGNode.h>
#include <Graphs/ICFGEdge.h>
#include <Graphs/CallGraph.h>
#include <Graphs/ThreadCallGraph.h>
#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include <MTA/TCT.h>
#include <WPA/Andersen.h>  // PointerAnalysis (for SlicedTCT)
#include <Util/SVFUtil.h>
#include <Util/Options.h>
#include <Util/WorkList.h>
#include <fstream>
#include <iostream>
#include <cctype>
#include <unordered_map>

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
    : icfg(icfg), callGraph(cg)
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
    std::ofstream out(filename + ".dot");
    if (!out.is_open()) {
        SVFUtil::errs() << "Failed to open file: " << filename << ".dot" << "\n";
        return;
    }

    out << "digraph ICFG {\n";
    out << "  rankdir=TB;\n";
    out << "  node [shape=box, style=rounded];\n\n";

    for (const ICFGNode* node : keptNodes) {
        std::string nodeId = "N" + std::to_string(node->getId());
        std::string label = node->toString();
        std::string escapedLabel = SlicedViewAdapter::escapeDotLabel(label);
        out << "  " << nodeId << " [label=\"" << escapedLabel << "\"];\n";
    }

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

    out << "}\n";
    out.close();
    SVFUtil::outs() << "[SlicedICFGView] ICFG dumped to " << filename << ".dot\n";
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
    // node contraction: for each removed node n, connect its preds to its succs.
    // keptNodesSet (member) was already populated by buildICFGSets().
    std::unordered_set<const ICFGNode*> deletedNodesSet;

    // temporary edge set: edges produced by node contraction
    std::unordered_map<const ICFGNode*, std::set<const ICFGNode*>> tempOutEdges;
    std::unordered_map<const ICFGNode*, std::set<const ICFGNode*>> tempInEdges;

    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* node = it->second;
        if (node == nullptr) continue;

        // only handle removed nodes (not in the kept set)
        if (keptNodesSet.count(node)) continue;

        // Pred
        std::set<const ICFGNode*> predNodes;
        for (const ICFGEdge* inEdge : node->getInEdges()) {
            if (inEdge == nullptr) continue;
            const ICFGNode* predNode = inEdge->getSrcNode();
            if (predNode == nullptr) continue;
            if (deletedNodesSet.count(predNode)) continue;
            predNodes.insert(predNode);
        }
        for (const ICFGNode* predNode : tempInEdges[node]) {
            if (predNode == nullptr) continue;
            if (deletedNodesSet.count(predNode)) continue;
            predNodes.insert(predNode);
        }

        // Succ
        std::set<const ICFGNode*> succNodes;
        for (const ICFGEdge* outEdge : node->getOutEdges()) {
            if (outEdge == nullptr) continue;
            const ICFGNode* succNode = outEdge->getDstNode();
            if (succNode == nullptr) continue;
            if (deletedNodesSet.count(succNode)) continue;
            succNodes.insert(succNode);
        }
        for (const ICFGNode* succNode : tempOutEdges[node]) {
            if (succNode == nullptr) continue;
            if (deletedNodesSet.count(succNode)) continue;
            succNodes.insert(succNode);
        }

        for (const ICFGNode* predNode : predNodes) {
            for (const ICFGNode* succNode : succNodes) {
                tempOutEdges[predNode].insert(succNode);
                tempInEdges[succNode].insert(predNode);
            }
        }
        deletedNodesSet.insert(node);
    }

    // project the contracted edges onto the kept-node set to form bridgedEdges
    for (const auto& pair : tempOutEdges) {
        const ICFGNode* src = pair.first;
        if (!keptNodesSet.count(src))
            continue;
        for (const ICFGNode* dst : pair.second) {
            if (!keptNodesSet.count(dst))
                continue;
            bridgedEdges[src].insert(dst);
            bridgedPreds[dst].insert(src);
        }
    }

    size_t totalBridgedEdges = 0;
    for (const auto& pair : bridgedEdges) {
        totalBridgedEdges += pair.second.size();
    }
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
            keptNodeIds.insert(callPE->getLHSVarID());
            keptNodeIds.insert(callPE->getRHSVarID());
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
    std::ofstream out(filename + ".dot");
    if (!out.is_open()) {
        SVFUtil::errs() << "Failed to open file: " << filename << ".dot" << "\n";
        return;
    }

    out << "digraph PAG {\n";
    out << "  rankdir=LR;\n";
    out << "  node [shape=box];\n\n";

    // emit nodes
    for (NodeID nodeId : keptNodeIds) {
        const SVFVar* var = pag->getGNode(nodeId);
        if (!var) continue;

        std::string nodeIdStr = "PAG" + std::to_string(nodeId);
        std::string label = var->toString();
        std::string escapedLabel = SlicedViewAdapter::escapeDotLabel(label);
        out << "  " << nodeIdStr << " [label=\"" << escapedLabel << "\"];\n";
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
            srcId = callPE->getRHSVarID();
            dstId = callPE->getLHSVarID();
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

    out << "}\n";
    out.close();
    SVFUtil::outs() << "[SlicedPAGView] PAG dumped to " << filename << ".dot\n";
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
    std::ofstream out(filename + ".dot");
    if (!out.is_open()) {
        SVFUtil::errs() << "Failed to open file: " << filename << ".dot" << "\n";
        return;
    }

    out << "digraph ThreadCallGraph {\n";
    out << "  rankdir=TB;\n";
    out << "  node [shape=box, style=rounded];\n\n";

    for (const CallGraphNode* node : keptNodes) {
        std::string nodeId = "CGN" + std::to_string(node->getId());
        std::string label = node->getName();
        std::string escapedLabel = SlicedViewAdapter::escapeDotLabel(label);
        out << "  " << nodeId << " [label=\"" << escapedLabel << "\"];\n";
    }

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

    out << "}\n";
    out.close();
    SVFUtil::outs() << "[SlicedThreadCallGraphView] ThreadCallGraph dumped to " << filename << ".dot\n";
}

//===----------------------------------------------------------------------===//
// SlicedSVFIRView
//===----------------------------------------------------------------------===//

SlicedSVFIRView::SlicedSVFIRView(SVFIR* svfIr,
                                 CallGraph* cg,
                                 ICFG* icfg,
                                 const std::set<const ICFGNode*>& keepNodes)
    : svfIr(svfIr), callGraph(cg)
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
        if (ICFG* icfg = icfgView->getICFG())
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

} // namespace SlicedViewAdapter

//===----------------------------------------------------------------------===//
// SlicedTCT - TCT rebuilt over the sliced ThreadCallGraph view.
//===----------------------------------------------------------------------===//

SlicedTCT::SlicedTCT(PointerAnalysis* p, const SlicedSVFIRView* sv, u32_t maxCxtLen)
    : TCT(p),
      slicedView(sv),
      tcgView(sv != nullptr && sv->getThreadCallGraph() != nullptr ? sv->getThreadCallGraph() : nullptr),
      maxContextLen(maxCxtLen)
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

    // Clear all TCT data structures to rebuild with sliced view
    ctpToNodeMap.clear();
    ctToForkCxtsMap.clear();
    ctToRoutineFunMap.clear();
    candidateFuncSet.clear();
    entryFuncSet.clear();
    joinSiteToLoopMap.clear();
    inRecurJoinSites.clear();
    ctpList.clear();
    visitedCTPs.clear();
    TCTNodeNum = 0;
    TCTEdgeNum = 0;
    MaxCxtSize = 0;

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

} // namespace SVF
