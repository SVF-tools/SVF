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
 */

#include "Graphs/SlicedGraphs.h"
#include "Graphs/SVFGNode.h"
#include "Graphs/GraphPrinter.h"
#include "Graphs/DOTGraphTraits.h"
#include "Util/SVFUtil.h"
#include "Util/ThreadAPI.h"
#include "SVFIR/SVFIR.h"
#include <cassert>
#include <cstdlib>

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
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple)
    {
    }

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
        if (SVFUtil::isa<FunEntryICFGNode>(node))
            str += ",color=yellow";
        else if (SVFUtil::isa<FunExitICFGNode>(node))
            str += ",color=green";
        else if (SVFUtil::isa<CallICFGNode>(node))
            str += ",color=red";
        else if (SVFUtil::isa<RetICFGNode>(node))
            str += ",color=blue";
        else if (SVFUtil::isa<GlobalICFGNode>(node))
            str += ",color=purple";
        else
            str += ",color=black";
        return str;
    }

    // Bridged edges are drawn dashed; kept original call/ret edges keep their colour.
    template <class EdgeIter>
    static std::string getEdgeAttributes(SlicedICFGNodeRef, EdgeIter EI, const SlicedICFGView*)
    {
        const SlicedICFGEdgeRef& e = EI.currentEdge();
        if (e.bridged)
            return "style=dashed,color=gray";
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
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple)
    {
    }

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
        if (e != nullptr && e->getEdgeKind() == CallGraphEdge::TDForkEdge)
            return "color=green";
        if (e != nullptr && e->getEdgeKind() == CallGraphEdge::CallRetEdge)
            return "color=blue";
        return "color=black";
    }
};

//===----------------------------------------------------------------------===//
// DOTGraphTraits for the sliced PAG view.
//===----------------------------------------------------------------------===//
template <>
struct DOTGraphTraits<const SlicedPAGView*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple)
    {
    }

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
        if (SVFUtil::isa<LoadStmt>(s))
            return "color=blue";
        if (SVFUtil::isa<StoreStmt>(s))
            return "color=red";
        if (SVFUtil::isa<GepStmt>(s))
            return "color=purple";
        if (SVFUtil::isa<AddrStmt>(s))
            return "color=green";
        if (SVFUtil::isa<CallPE>(s))
            return "color=orange";
        if (SVFUtil::isa<RetPE>(s))
            return "color=cyan";
        return "color=black";
    }
};

//===----------------------------------------------------------------------===//
// DOTGraphTraits for the sliced SVFG view.
//===----------------------------------------------------------------------===//
template <>
struct DOTGraphTraits<const SlicedSVFGView*> : public DefaultDOTGraphTraits
{
    DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple)
    {
    }

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
        if (SVFUtil::isa<StoreSVFGNode>(n.raw))
            return "shape=record,color=red";
        if (SVFUtil::isa<LoadSVFGNode>(n.raw))
            return "shape=record,color=blue";
        return "shape=record,color=black";
    }

    template <class EdgeIter>
    static std::string getEdgeAttributes(SlicedSVFGNodeRef, EdgeIter EI, const SlicedSVFGView*)
    {
        const SVFGEdge* e = EI.currentEdge().underlying;
        if (e != nullptr && SVFUtil::isa<IndirectSVFGEdge>(e))
            return "style=dashed";
        return "style=solid";
    }
};

//===----------------------------------------------------------------------===//
// SlicedSVFGView
//===----------------------------------------------------------------------===//

bool SlicedSVFGView::isKeptNode(const SVFGNode* n) const
{
    if (n == nullptr)
        return false;
    return retainedNodeIds.test(n->getId());
}

size_t SlicedSVFGView::getKeptNodeCount() const
{
    return retainedNodeIds.count();
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
                               const OrderedSet<const ICFGNode*>& keepNodes)
    : icfg(icfg)
{
    buildICFGSets(keepNodes);
    buildBridgedEdges();
}

// getSuccNodes/getPredNodes and the GenericGraphTraits iterators must agree on the
// slice topology, so both go through the one traits definition (kept original
// edges + bridged edges). The sliced MHP/Lock analyses reach the slice here.
void SlicedICFGView::getSuccNodes(
    const ICFGNode* node, std::vector<const ICFGNode*>& out) const
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

void SlicedICFGView::getPredNodes(
    const ICFGNode* node, std::vector<const ICFGNode*>& out) const
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

void SlicedICFGView::buildICFGSets(
    const OrderedSet<const ICFGNode*>& keepNodes)
{
    keptNodes.clear();
    keptNodes.insert(keepNodes.begin(), keepNodes.end());

    // Build keptNodesSet for fast lookup
    keptNodesSet.clear();
    keptNodesSet.insert(keptNodes.begin(), keptNodes.end());
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
    Map<const ICFGNode*, int> removedNodeIndex;
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end();
         it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        if (node == nullptr || keptNodesSet.count(node))
            continue;
        removedNodeIndex[node] = static_cast<int>(removed.size());
        removed.push_back(node);
    }
    const int removedNodeCount = static_cast<int>(removed.size());

    // call_i -> ret_i summary for call sites with an omitted callee (some resolved
    // callee entry not retained), for every call site so paths can compose through
    // removed ones. ret_i is in the same caller, so the seed stays intra-procedural.
    Map<const ICFGNode*, const ICFGNode*> seedRet;
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end();
         it != eit; ++it)
    {
        const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(it->second);
        if (call == nullptr || call->getRetICFGNode() == nullptr)
            continue;
        for (const ICFGEdge* edge : call->getOutEdges())
            if (edge != nullptr && SVFUtil::isa<CallCFGEdge>(edge) &&
                keptNodesSet.count(edge->getDstNode()) == 0)
            {
                seedRet[call] = call->getRetICFGNode();
                break;
            }
    }
    // Local successors = intra edges + matched call->ret seeds; the only edges
    // contraction may traverse. Original call/ret edges are excluded.
    std::vector<std::vector<int>> removedSuccessors(removedNodeCount);
    std::vector<std::vector<const ICFGNode*>>
        keptSuccessors(removedNodeCount);
    std::vector<const ICFGNode*> successors;
    for (int nodeIndex = 0; nodeIndex < removedNodeCount; ++nodeIndex)
    {
        getLocalSuccessors(removed[nodeIndex], seedRet, successors);
        for (const ICFGNode* successor : successors)
        {
            if (keptNodesSet.count(successor))
                keptSuccessors[nodeIndex].push_back(successor);
            else
            {
                const auto found = removedNodeIndex.find(successor);
                if (found != removedNodeIndex.end())
                    removedSuccessors[nodeIndex].push_back(found->second);
            }
        }
    }

    // Iterative Tarjan SCC over the removed subgraph. Components are produced in
    // reverse-topological order, so comp ids of a node's successors are < its own.
    std::vector<int> discoveryIndex(removedNodeCount, -1);
    std::vector<int> lowLink(removedNodeCount, 0);
    std::vector<int> component(removedNodeCount, -1);
    std::vector<char> onStack(removedNodeCount, 0);
    std::vector<int> tarjanStack;
    int nextDiscoveryIndex = 0;
    int componentCount = 0;
    for (int root = 0; root < removedNodeCount; ++root)
    {
        if (discoveryIndex[root] != -1)
            continue;
        std::vector<std::pair<int, size_t>> work;
        work.emplace_back(root, 0);
        while (!work.empty())
        {
            const int nodeIndex = work.back().first;
            size_t& successorPosition = work.back().second;
            if (successorPosition == 0)
            {
                discoveryIndex[nodeIndex] =
                    lowLink[nodeIndex] = nextDiscoveryIndex++;
                tarjanStack.push_back(nodeIndex);
                onStack[nodeIndex] = 1;
            }
            bool descend = false;
            while (successorPosition < removedSuccessors[nodeIndex].size())
            {
                const int successorIndex =
                    removedSuccessors[nodeIndex][successorPosition++];
                if (discoveryIndex[successorIndex] == -1)
                {
                    work.emplace_back(successorIndex, 0);
                    descend = true;
                    break;
                }
                if (onStack[successorIndex] &&
                    discoveryIndex[successorIndex] < lowLink[nodeIndex])
                    lowLink[nodeIndex] = discoveryIndex[successorIndex];
            }
            if (descend)
                continue;
            if (lowLink[nodeIndex] == discoveryIndex[nodeIndex])
            {
                while (true)
                {
                    const int componentNode = tarjanStack.back();
                    tarjanStack.pop_back();
                    onStack[componentNode] = 0;
                    component[componentNode] = componentCount;
                    if (componentNode == nodeIndex)
                        break;
                }
                ++componentCount;
            }
            work.pop_back();
            if (!work.empty())
            {
                const int parentIndex = work.back().first;
                if (lowLink[nodeIndex] < lowLink[parentIndex])
                    lowLink[parentIndex] = lowLink[nodeIndex];
            }
        }
    }

    // Condensation: base kept-successors and DAG successors per component.
    std::vector<OrderedSet<const ICFGNode*>> baseKept(componentCount);
    std::vector<OrderedSet<int>> componentSuccessors(componentCount);
    for (int nodeIndex = 0; nodeIndex < removedNodeCount; ++nodeIndex)
    {
        const int componentIndex = component[nodeIndex];
        for (const ICFGNode* keptSuccessor : keptSuccessors[nodeIndex])
            baseKept[componentIndex].insert(keptSuccessor);
        for (int successorIndex : removedSuccessors[nodeIndex])
            if (component[successorIndex] != componentIndex)
                componentSuccessors[componentIndex].insert(
                    component[successorIndex]);
    }

    // Propagate reachKept in ascending comp order (successors have smaller ids).
    std::vector<OrderedSet<const ICFGNode*>>
        reachableKeptNodes(componentCount);
    for (int componentIndex = 0; componentIndex < componentCount;
         ++componentIndex)
    {
        OrderedSet<const ICFGNode*>& reachable =
            reachableKeptNodes[componentIndex];
        reachable = baseKept[componentIndex];
        for (int successorComponent : componentSuccessors[componentIndex])
            reachable.insert(
                reachableKeptNodes[successorComponent].begin(),
                reachableKeptNodes[successorComponent].end());
    }

    // bridgedEdges[u] = kept nodes reached from kept u through removed local paths,
    // plus a matched call->ret summary when u is a seeded call site (kept ret).
    for (const ICFGNode* source : keptNodesSet)
    {
        getLocalSuccessors(source, seedRet, successors);
        const auto seedReturn = seedRet.find(source);
        for (const ICFGNode* successor : successors)
        {
            if (keptNodesSet.count(successor))
            {
                // Kept seed target: no real edge exists, so record the bridge; a
                // kept intra target is a real edge handled by getSuccNodes.
                if (seedReturn != seedRet.end() &&
                    seedReturn->second == successor)
                {
                    bridgedEdges[source].insert(successor);
                    bridgedPreds[successor].insert(source);
                }
                continue;
            }
            const auto removedIndex = removedNodeIndex.find(successor);
            if (removedIndex == removedNodeIndex.end())
            {
                SVFUtil::errs()
                    << "[ERROR] Local ICFG successor is neither kept nor indexed\n";
                std::abort();
            }
            for (const ICFGNode* target :
                 reachableKeptNodes[component[removedIndex->second]])
            {
                bridgedEdges[source].insert(target);
                bridgedPreds[target].insert(source);
            }
        }
    }

    size_t totalBridgedEdges = 0;
    for (const auto& pair : bridgedEdges)
        totalBridgedEdges += pair.second.size();
    SVFUtil::outs() << "[SlicedICFGView] Built " << totalBridgedEdges
                    << " bridged edges across " << bridgedEdges.size()
                    << " source nodes\n";
}

void SlicedICFGView::getLocalSuccessors(
    const ICFGNode* node,
    const Map<const ICFGNode*, const ICFGNode*>& callsiteReturnNodes,
    std::vector<const ICFGNode*>& successors)
{
    successors.clear();
    for (const ICFGEdge* edge : node->getOutEdges())
        if (edge != nullptr && SVFUtil::isa<IntraCFGEdge>(edge) &&
            edge->getDstNode() != nullptr)
            successors.push_back(edge->getDstNode());

    const auto returnNode = callsiteReturnNodes.find(node);
    if (returnNode != callsiteReturnNodes.end())
        successors.push_back(returnNode->second);
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
        // Handle the two SVF statement abstractions directly so new concrete
        // statement subclasses cannot silently disappear from the PAG view.
        if (const AssignStmt* assignStmt = SVFUtil::dyn_cast<AssignStmt>(stmt))
        {
            keptNodeIds.insert(assignStmt->getLHSVarID());
            keptNodeIds.insert(assignStmt->getRHSVarID());
            continue;
        }
        if (const MultiOpndStmt* multi =
                SVFUtil::dyn_cast<MultiOpndStmt>(stmt))
        {
            keptNodeIds.insert(multi->getResID());
            for (u32_t i = 0; i < multi->getOpVarNum(); ++i)
                keptNodeIds.insert(multi->getOpVarID(i));
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
    // The input already contains the slicing targets and control-flow anchors;
    // buildCallGraphSets filters edges whose callsites were pruned.
    buildCallGraphSets();
}

void SlicedThreadCallGraphView::buildKeptNodes()
{
    for (CallGraph::iterator it = tcg->begin(), eit = tcg->end();
         it != eit; ++it)
    {
        const CallGraphNode* node = it->second;
        if (node != nullptr && node->getFunction() != nullptr &&
            keptFunctionsSet.count(node->getFunction()))
        {
            keptNodes.insert(node);
        }
    }
}

void SlicedThreadCallGraphView::getOutEdgesOf(
    const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
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

void SlicedThreadCallGraphView::getInEdgesOf(
    const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
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

void SlicedThreadCallGraphView::getDirectCallsOf(
    const CallGraphEdge* edge, std::vector<const CallICFGNode*>& out) const
{
    out.clear();
    const auto found = keptDirectCalls.find(edge);
    if (found != keptDirectCalls.end())
        out.insert(out.end(), found->second.begin(), found->second.end());
}

void SlicedThreadCallGraphView::getIndirectCallsOf(
    const CallGraphEdge* edge, std::vector<const CallICFGNode*>& out) const
{
    out.clear();
    const auto found = keptIndirectCalls.find(edge);
    if (found != keptIndirectCalls.end())
        out.insert(out.end(), found->second.begin(), found->second.end());
}

bool SlicedThreadCallGraphView::containsCallSite(
    const CallGraphEdge* edge, const CallICFGNode* callSite) const
{
    const auto direct = keptDirectCalls.find(edge);
    if (direct != keptDirectCalls.end() && direct->second.count(callSite))
        return true;
    const auto indirect = keptIndirectCalls.find(edge);
    return indirect != keptIndirectCalls.end() &&
           indirect->second.count(callSite);
}

void SlicedThreadCallGraphView::getCalleesOf(
    const CallICFGNode* callSite, CallGraph::FunctionSet& callees) const
{
    callees.clear();
    const CallGraphNode* caller = tcg->getCallGraphNode(callSite->getFun());
    if (!isKeptNode(caller))
        return;
    std::vector<const CallGraphEdge*> outEdges;
    getOutEdgesOf(caller, outEdges);
    for (const CallGraphEdge* edge : outEdges)
        if (containsCallSite(edge, callSite))
            callees.insert(edge->getDstNode()->getFunction());
}

void SlicedThreadCallGraphView::getForkEdgesOf(
    const CallICFGNode* callSite,
    std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    if (!extendedKeptNodes.count(callSite) ||
        !tcg->hasThreadForkEdge(callSite))
        return;
    for (auto it = tcg->getForkEdgeBegin(callSite),
              end = tcg->getForkEdgeEnd(callSite); it != end; ++it)
    {
        const CallGraphEdge* edge = *it;
        if (isKeptNode(edge->getSrcNode()) &&
            isKeptNode(edge->getDstNode()) &&
            containsCallSite(edge, callSite))
            out.push_back(edge);
    }
}

void SlicedThreadCallGraphView::getJoinEdgesOf(
    const CallICFGNode* callSite,
    std::vector<const CallGraphEdge*>& out) const
{
    out.clear();
    if (!extendedKeptNodes.count(callSite) ||
        !tcg->hasThreadJoinEdge(callSite))
        return;
    for (auto it = tcg->getJoinEdgeBegin(callSite),
              end = tcg->getJoinEdgeEnd(callSite); it != end; ++it)
    {
        const CallGraphEdge* edge = *it;
        if (isKeptNode(edge->getSrcNode()) &&
            isKeptNode(edge->getDstNode()))
            out.push_back(edge);
    }
}

bool SlicedThreadCallGraphView::isKeptNode(const CallGraphNode* node) const
{
    return keptNodes.count(node) > 0;
}

void SlicedThreadCallGraphView::buildCallGraphSets()
{
    // rebuild kept edges, accounting for whether the call site is kept
    keptEdges.clear();
    keptDirectCalls.clear();
    keptIndirectCalls.clear();
    indirectSitesWithEmptyTargets.clear();

    // CallGraph edge: src/dst both in kept functions and the call site still in the kept ICFG node set
    for (const CallGraphNode* srcNode : keptNodes)
    {
        for (const CallGraphEdge* edge : srcNode->getOutEdges())
        {
            const CallGraphNode* dstNode = edge ? edge->getDstNode() : nullptr;
            if (dstNode == nullptr || !keptNodes.count(dstNode))
            {
                continue;
            }

            CallGraphEdge::CallInstSet& retainedDirect = keptDirectCalls[edge];
            for (const CallICFGNode* callSite : edge->getDirectCalls())
                if (extendedKeptNodes.count(callSite))
                    retainedDirect.insert(callSite);

            CallGraphEdge::CallInstSet& retainedIndirect = keptIndirectCalls[edge];
            for (const CallICFGNode* callSite : edge->getIndirectCalls())
                if (extendedKeptNodes.count(callSite))
                    retainedIndirect.insert(callSite);

            const bool hasOriginalCallSites =
                !edge->getDirectCalls().empty() || !edge->getIndirectCalls().empty();
            if (hasOriginalCallSites && retainedDirect.empty() && retainedIndirect.empty())
            {
                keptDirectCalls.erase(edge);
                keptIndirectCalls.erase(edge);
                continue;
            }

            keptEdges.insert(edge);
        }
    }

    // An indirect callsite is empty only when none of its original targets is
    // represented by a retained callsite-to-callee relation.
    for (const ICFGNode* node : extendedKeptNodes)
    {
        const CallICFGNode* callSite = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callSite == nullptr || !tcg->hasIndCSCallees(callSite))
            continue;
        CallGraph::FunctionSet callees;
        getCalleesOf(callSite, callees);
        if (callees.empty())
            indirectSitesWithEmptyTargets.insert(callSite);
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

SlicedSVFIRView::SlicedSVFIRView(SVFIR* svfir,
                                 ThreadCallGraph& callGraph,
                                 ICFG* icfg,
                                 const OrderedSet<const ICFGNode*>& keepNodes)
    : svfir(svfir)
{
    // A retained function is represented by explicit synthetic entry/exit
    // nodes. Graph-generic MHP and lock propagation can therefore never seed or
    // rendezvous at a node outside the view.
    OrderedSet<const ICFGNode*> extendedKeepNodes = keepNodes;

    // Derive keptFunctions from keepNodes.
    OrderedSet<const FunObjVar*> keptFunctions;
    for (const ICFGNode* node : keepNodes)
    {
        if (node != nullptr && node->getFun() != nullptr)
        {
            keptFunctions.insert(node->getFun());
        }
    }
    for (const FunObjVar* fun : keptFunctions)
    {
        extendedKeepNodes.insert(icfg->getFunEntryICFGNode(fun));
        extendedKeepNodes.insert(icfg->getFunExitICFGNode(fun));
    }

    tcgView = std::make_unique<SlicedThreadCallGraphView>(
                  &callGraph, keptFunctions, extendedKeepNodes);

    // Create ICFG view (based on keepNodes and keptFunctions)
    icfgView = std::make_unique<SlicedICFGView>(
                   icfg, extendedKeepNodes);

    // Create PAG view (extract statements from keepNodes)
    OrderedSet<const SVFStmt*> keptStmts;
    for (const ICFGNode* node : extendedKeepNodes)
    {
        const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
        keptStmts.insert(stmts.begin(), stmts.end());
    }
    pagView = std::make_unique<SlicedPAGView>(svfir, keptStmts);
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
    // A view constructor always retains the synthetic entry. Keep the fallback
    // for defensive compatibility with directly constructed SlicedICFGViews,
    // but never return a node outside this view.
    const ICFGNode* entry = fun->getEntryBlock()->front();
    if (isKeptNode(entry))
        return entry;
    for (const ICFGNode* node : fun->getEntryBlock()->getICFGNodeList())
    {
        if (isKeptNode(node))
            return node;
    }
    return nullptr;
}

const ICFGNode* SlicedICFGView::getFunExit(const FunObjVar* fun) const
{
    const ICFGNode* exit = icfg->getFunExitICFGNode(fun);
    return isKeptNode(exit) ? exit : nullptr;
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

} // End namespace SVF
