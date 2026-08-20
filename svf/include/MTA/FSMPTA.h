//===- FSMPTA.h -- Flow-sensitive multithreaded pointer analysis (FSAM) -===//
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

#ifndef INCLUDE_MTA_FSMPTA_H_
#define INCLUDE_MTA_FSMPTA_H_

#include "WPA/FlowSensitive.h"
#include "Graphs/SlicedGraphs.h"

#include <deque>

namespace SVF
{

class AndersenBase;
class AndersenWaveDiff;

/// Flow-sensitive solver over an already-built thread-aware SVFG. The graph is
/// owned by MTASVFGBuilder; this class owns only the analysis state and the SCC
/// detector for the exact final slice.
class FSMPTA final : public FlowSensitive
{
public:
    FSMPTA(AndersenWaveDiff& preAnalysis, SVFG& backingGraph,
           const SlicedSVFGView& solveView);
    ~FSMPTA() override = default;

    void initialize() override;
    void finalize() override;

    static bool supportsCurrentConfiguration();

    /// Close a backward value-flow slice over FlowSensitive's execution
    /// dependencies. Besides explicit SVFG predecessors, this follows the
    /// definition of every top-level pointer read through the solver-global
    /// points-to map, closes future indirect-call boundary edges, and retains
    /// variant-GEP transfers whose field-sensitivity side effect is global.
    static NodeBS buildExecutionDependencyClosure(
        SVFG* graph, AndersenBase* preAnalysis, NodeBS dependencyNodes);

protected:
    NodeStack& SCCDetect() override;
    void processNode(NodeID nodeId) override;
    void updateConnectedNodes(const SVFGEdgeSetTy& edges) override;

private:
    static void enqueueSVFGNode(const SVFGNode* node, NodeBS& retained,
                                std::deque<NodeID>& worklist);
    static void demandTopLevelPointer(const SVFVar* var, SVFG* graph,
                                      NodeBS& demandedVars, NodeBS& retained,
                                      std::deque<NodeID>& worklist);
    static void collectNodeInputDependencies(
        const SVFGNode* node, SVFG* graph, NodeBS& demandedVars,
        NodeBS& retained, std::deque<NodeID>& worklist);
    void buildRetainedAdjacency();
    void cacheRetainedEdge(SVFGEdge* edge);

    AndersenWaveDiff* preAnalysis;
    SVFG* backingGraph;
    const SlicedSVFGView* solveView;
    std::unique_ptr<SCCDetection<const SlicedSVFGView*>> slicedSCC;
    NodeStack slicedNodeStack;
    bool useRetainedAdjacency = false;
    Map<NodeID, std::vector<SVFGEdge*>> retainedOutEdges;
    Set<const SVFGEdge*> retainedEdgeSet;
};

} // End namespace SVF

#endif /* INCLUDE_MTA_FSMPTA_H_ */
