//===- FSMPTA.cpp -- Flow-sensitive multithreaded pointer analysis (FSAM) ===//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
//===----------------------------------------------------------------------===//

#include "MTA/FSMPTA.h"
#include "WPA/Andersen.h"
#include "WPA/WPAStat.h"
#include "Util/Options.h"
#include <cstdlib>
#include <deque>

using namespace SVF;

FSMPTA::FSMPTA(AndersenWaveDiff& pre, SVFG& graph,
               const SlicedSVFGView& view)
    : FlowSensitive(pre.getPAG()), preAnalysis(&pre), backingGraph(&graph),
      solveView(&view)
{
}

bool FSMPTA::supportsCurrentConfiguration()
{
    return !Options::ClusterAnder() && !Options::ClusterFs() &&
           !Options::PlainMappingFs();
}

void FSMPTA::enqueueSVFGNode(const SVFGNode* node, NodeBS& retained,
                            std::deque<NodeID>& nodeWorklist)
{
    if (node != nullptr && !retained.test(node->getId()))
    {
        retained.set(node->getId());
        nodeWorklist.push_back(node->getId());
    }
}

void FSMPTA::demandTopLevelPointer(const SVFVar* var, SVFG* graph,
                                  NodeBS& demandedVars, NodeBS& retained,
                                  std::deque<NodeID>& nodeWorklist)
{
    const ValVar* val = SVFUtil::dyn_cast<ValVar>(var);
    if (val == nullptr || !val->isPointer() || demandedVars.test(val->getId()))
        return;

    demandedVars.set(val->getId());
    if (graph->hasDefSVFGNode(val))
        enqueueSVFGNode(graph->getDefSVFGNode(val), retained, nodeWorklist);
}

/// Register every solver-global top-level points-to read performed while a
/// node is processed. MemorySSA IN/OUT state is carried by explicit indirect
/// SVFG predecessors and therefore needs no separate variable root here.
void FSMPTA::collectNodeInputDependencies(
    const SVFGNode* node, SVFG* graph, NodeBS& demandedVars,
    NodeBS& retained, std::deque<NodeID>& nodeWorklist)
{
    if (const CopySVFGNode* copy = SVFUtil::dyn_cast<CopySVFGNode>(node))
        demandTopLevelPointer(copy->getSrcNode(), graph, demandedVars,
                              retained, nodeWorklist);
    else if (const GepSVFGNode* gep = SVFUtil::dyn_cast<GepSVFGNode>(node))
        demandTopLevelPointer(gep->getSrcNode(), graph, demandedVars,
                              retained, nodeWorklist);
    else if (const PHISVFGNode* phi = SVFUtil::dyn_cast<PHISVFGNode>(node))
        for (auto it = phi->opVerBegin(), eit = phi->opVerEnd(); it != eit; ++it)
            demandTopLevelPointer(it->second, graph, demandedVars,
                                  retained, nodeWorklist);
    else if (const LoadSVFGNode* load = SVFUtil::dyn_cast<LoadSVFGNode>(node))
        demandTopLevelPointer(load->getSrcNode(), graph, demandedVars,
                              retained, nodeWorklist);
    else if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
    {
        demandTopLevelPointer(store->getDstNode(), graph, demandedVars,
                              retained, nodeWorklist);
        demandTopLevelPointer(store->getSrcNode(), graph, demandedVars,
                              retained, nodeWorklist);
    }
    else if (const ActualParmSVFGNode* actual =
                 SVFUtil::dyn_cast<ActualParmSVFGNode>(node))
        demandTopLevelPointer(actual->getParam(), graph, demandedVars,
                              retained, nodeWorklist);
    else if (const FormalRetSVFGNode* formal =
                 SVFUtil::dyn_cast<FormalRetSVFGNode>(node))
        demandTopLevelPointer(formal->getRet(), graph, demandedVars,
                              retained, nodeWorklist);
}

NodeBS FSMPTA::buildExecutionDependencyClosure(
    SVFG* graph, AndersenBase* preAnalysis, NodeBS dependencyNodes)
{
    if (graph == nullptr || preAnalysis == nullptr)
    {
        SVFUtil::errs() << "[ERROR] FSMPTA execution closure requires a BaseSVFG "
                        << "and Andersen targets\n";
        return NodeBS();
    }

    std::deque<NodeID> nodeWorklist;
    for (NodeID id : dependencyNodes)
        nodeWorklist.push_back(id);

    NodeBS demandedVars;
    // FlowSensitive::solveConstraints invokes updateCallGraph() for the whole
    // program after every iteration. Its function-pointer reads are therefore
    // execution roots even when the call site is not itself in the target slice.
    SVFIR* pag = graph->getPAG();
    for (const auto& callsiteAndPtr : pag->getIndirectCallsites())
    {
        const SVFVar* funPtr = pag->getGNode(callsiteAndPtr.second);
        demandTopLevelPointer(funPtr, graph, demandedVars,
                              dependencyNodes, nodeWorklist);
    }

    Set<const CallICFGNode*> indirectSites;
    Set<const FunObjVar*> indirectTargets;
    for (const auto& callsiteAndPtr : pag->getIndirectCallsites())
        indirectSites.insert(callsiteAndPtr.first);
    for (const auto& callsiteAndTargets : preAnalysis->getIndCallMap())
        for (const FunObjVar* target : callsiteAndTargets.second)
            indirectTargets.insert(target);

    // One backing-SVFG pass collects both kinds of solver-global roots:
    // updateCallGraph boundary nodes and variant-GEP side effects.
    for (SVFG::const_iterator it = graph->begin(), eit = graph->end();
         it != eit; ++it)
    {
        const SVFGNode* node = it->second;
        bool boundary = false;
        if (const ActualParmSVFGNode* actual =
                SVFUtil::dyn_cast<ActualParmSVFGNode>(node))
            boundary = indirectSites.count(actual->getCallSite()) > 0;
        else if (const ActualRetSVFGNode* actual =
                     SVFUtil::dyn_cast<ActualRetSVFGNode>(node))
            boundary = indirectSites.count(actual->getCallSite()) > 0;
        else if (const ActualINSVFGNode* actual =
                     SVFUtil::dyn_cast<ActualINSVFGNode>(node))
            boundary = indirectSites.count(actual->getCallSite()) > 0;
        else if (const ActualOUTSVFGNode* actual =
                     SVFUtil::dyn_cast<ActualOUTSVFGNode>(node))
            boundary = indirectSites.count(actual->getCallSite()) > 0;
        else if (const FormalParmSVFGNode* formal =
                     SVFUtil::dyn_cast<FormalParmSVFGNode>(node))
            boundary = indirectTargets.count(formal->getFun()) > 0;
        else if (const FormalRetSVFGNode* formal =
                     SVFUtil::dyn_cast<FormalRetSVFGNode>(node))
            boundary = indirectTargets.count(formal->getFun()) > 0;
        else if (const FormalINSVFGNode* formal =
                     SVFUtil::dyn_cast<FormalINSVFGNode>(node))
            boundary = indirectTargets.count(
                formal->getFunEntryNode()->getFun()) > 0;
        else if (const FormalOUTSVFGNode* formal =
                     SVFUtil::dyn_cast<FormalOUTSVFGNode>(node))
            boundary = indirectTargets.count(
                formal->getFunExitNode()->getFun()) > 0;
        else if (const InterMSSAPHISVFGNode* phi =
                     SVFUtil::dyn_cast<InterMSSAPHISVFGNode>(node))
            boundary = phi->isFormalINPHI()
                           ? indirectTargets.count(phi->getFun()) > 0
                           : indirectSites.count(phi->getCallSite()) > 0;

        if (boundary)
            enqueueSVFGNode(node, dependencyNodes, nodeWorklist);

        // A variant GEP changes field-sensitivity globally. Later transfers
        // read that state without an explicit SVFG edge.
        if (const GepSVFGNode* gep = SVFUtil::dyn_cast<GepSVFGNode>(node))
        {
            const GepStmt* stmt = SVFUtil::cast<GepStmt>(gep->getSVFStmt());
            if (stmt->isVariantFieldGep())
                enqueueSVFGNode(gep, dependencyNodes, nodeWorklist);
        }
    }

    // Joint fixed point: explicit SVFG predecessors carry direct/MemorySSA
    // dependencies; definition roots cover FlowSensitive's implicit reads from
    // the solver-global top-level points-to relation.
    while (!nodeWorklist.empty())
    {
        const NodeID id = nodeWorklist.front();
        nodeWorklist.pop_front();
        const SVFGNode* node = graph->getSVFGNode(id);

        for (const SVFGEdge* edge : node->getInEdges())
            enqueueSVFGNode(
                edge->getSrcNode(), dependencyNodes, nodeWorklist);

        collectNodeInputDependencies(
            node, graph, demandedVars, dependencyNodes, nodeWorklist);
    }
    return dependencyNodes;
}

void FSMPTA::initialize()
{
    PointerAnalysis::initialize();
    stat = new FlowSensitiveStat(this);
    // SlicedMTA reports the deployment-facing result summary. Avoid the generic
    // FlowSensitive statistics pass because it recomputes SCCs on the full SVFG.
    disablePrintStat();

    if (!supportsCurrentConfiguration())
    {
        SVFUtil::errs() << "[ERROR] FSMPTA does not support clustered Andersen, "
                        << "clustered FS, or plain FS mappings\n";
        std::abort();
    }

    // Reuse both the Andersen result and the already-built base SVFG. The main
    // ILA overlay has been attached by SlicedMTA before analysis starts.
    ander = preAnalysis;
    svfg = backingGraph;
    // Retain the stock graph handle for FlowSensitive's dynamic-call support;
    // SCC/worklist topology is supplied exclusively by slicedSCC below.
    setGraph(svfg);
    slicedSCC = std::make_unique<SCCDetection<const SlicedSVFGView*>>(solveView);
    useRetainedAdjacency = !solveView->keepsAllNodes();
    if (useRetainedAdjacency)
        buildRetainedAdjacency();
}

void FSMPTA::finalize()
{
    if (Options::DumpVFG())
        svfg->dump("fs_solved", true);
    BVDataPTAImpl::finalize();
}

void FSMPTA::cacheRetainedEdge(SVFGEdge* edge)
{
    if (solveView->isKeptEdge(edge) && retainedEdgeSet.insert(edge).second)
        retainedOutEdges[edge->getSrcID()].push_back(edge);
}

void FSMPTA::buildRetainedAdjacency()
{
    for (SVFG::iterator it = svfg->begin(), eit = svfg->end(); it != eit; ++it)
    {
        SVFGNode* node = it->second;
        if (!solveView->isKeptNode(node))
            continue;
        for (SVFGEdge* edge : node->getOutEdges())
            cacheRetainedEdge(edge);
    }
}

NodeStack& FSMPTA::SCCDetect()
{
    const double start = stat->getClk();
    slicedSCC->find();
    assert(slicedNodeStack.empty() && "sliced SCC stack was not fully consumed");

    FIFOWorkList<NodeID> revTopo = slicedSCC->revTopoNodeStack();
    while (!revTopo.empty())
    {
        const NodeID rep = revTopo.front();
        revTopo.pop();
        const NodeBS& subNodes = slicedSCC->subNodes(rep);
        for (NodeID id : subNodes)
            slicedNodeStack.push(id);
    }

    assert(slicedNodeStack.size() == solveView->getKeptNodeCount() &&
           "FSMPTA SCC topology must contain exactly the solve view");

    const double end = stat->getClk();
    sccTime += (end - start) / TIMEINTERVAL;
    return slicedNodeStack;
}

void FSMPTA::processNode(NodeID nodeId)
{
    SVFGNode* node = svfg->getSVFGNode(nodeId);
    assert(solveView->isKeptNode(node) &&
           "FSMPTA worklist must never contain a node outside the solve view");

    if (processSVFGNode(node))
    {
        if (useRetainedAdjacency)
        {
            const auto found = retainedOutEdges.find(nodeId);
            if (found != retainedOutEdges.end())
            {
                for (SVFGEdge* edge : found->second)
                {
                    if (propFromSrcToDst(edge))
                    {
                        pushIntoWorklist(edge->getDstID());
                    }
                }
            }
        }
        else
        {
            for (SVFGEdge* edge : node->getOutEdges())
            {
                if (propFromSrcToDst(edge))
                {
                    pushIntoWorklist(edge->getDstID());
                }
            }
        }
    }
    clearAllDFOutVarFlag(node);
}

void FSMPTA::updateConnectedNodes(const SVFGEdgeSetTy& edges)
{
    SVFGEdgeSetTy keptEdges;
    for (SVFGEdge* edge : edges)
        if (solveView->isKeptEdge(edge))
        {
            keptEdges.insert(edge);
            if (useRetainedAdjacency)
                cacheRetainedEdge(edge);
        }
    FlowSensitive::updateConnectedNodes(keptEdges);
}
