//===- AndersenWaveDiff.cpp -- Wave propagation based Andersen's analysis with caching--//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===--------------------------------------------------------------------------------===//

/*
 * AndersenWaveDiff.cpp
 *
 *  Created on: 23/11/2013
 *      Author: yesen
 */

#include "WPA/Andersen.h"

using namespace SVFUtil;

AndersenWaveDiff* AndersenWaveDiff::diffWave = NULL;

/*!
 * solve worklist
 */
void AndersenWaveDiff::solveWorklist() {
    // Initialize the nodeStack via a whole SCC detection
    // Nodes in nodeStack are in topological order by default.
    NodeStack& nodeStack = SCCDetect();

    // Process nodeStack and put the changed nodes into workList.
    while (!nodeStack.empty()) {
        NodeID nodeId = nodeStack.top();
        nodeStack.pop();
        collapsePWCNode(nodeId);
        // process nodes in nodeStack
        processNode(nodeId);
        collapseFields();
    }

    // This modification is to make WAVE feasible to handle PWC analysis
    if (!mergePWC()) {
        NodeStack tmpWorklist;
        while (!isWorklistEmpty()) {
            NodeID nodeId = popFromWorklist();
            collapsePWCNode(nodeId);
            // process nodes in nodeStack
            processNode(nodeId);
            collapseFields();
            tmpWorklist.push(nodeId);
        }
        while (!tmpWorklist.empty()) {
            NodeID nodeId = tmpWorklist.top();
            tmpWorklist.pop();
            pushIntoWorklist(nodeId);
        }
    }

    // New nodes will be inserted into workList during processing.
    while (!isWorklistEmpty()) {
        NodeID nodeId = popFromWorklist();
        // process nodes in worklist
        postProcessNode(nodeId);
    }
}

/*!
 * Process edge PAGNode
 */
void AndersenWaveDiff::processNode(NodeID nodeId) {
    // This node may be merged during collapseNodePts() which means it is no longer a rep node
    // in the graph. Only rep node needs to be handled.
    if (sccRepNode(nodeId) != nodeId)
        return;

    double propStart = stat->getClk();
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    handleCopyGep(node);
    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
}

/*!
 * Post process node
 */
void AndersenWaveDiff::postProcessNode(NodeID nodeId)
{
    double insertStart = stat->getClk();

    ConstraintNode* node = consCG->getConstraintNode(nodeId);

    // handle load
    for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(), eit = node->outgoingLoadsEnd();
         it != eit; ++it) {
        if (handleLoad(nodeId, *it))
            reanalyze = true;
    }
    // handle store
    for (ConstraintNode::const_iterator it = node->incomingStoresBegin(), eit =  node->incomingStoresEnd();
         it != eit; ++it) {
        if (handleStore(nodeId, *it))
            reanalyze = true;
    }

    double insertEnd = stat->getClk();
    timeOfProcessLoadStore += (insertEnd - insertStart) / TIMEINTERVAL;
}

/*!
 * Handle copy gep
 */
void AndersenWaveDiff::handleCopyGep(ConstraintNode* node) {
    NodeID nodeId = node->getId();
    computeDiffPts(nodeId);

    if (!getDiffPts(nodeId).empty()) {
        for (ConstraintEdge* edge : node->getCopyOutEdges())
            if (CopyCGEdge* copyEdge = SVFUtil::dyn_cast<CopyCGEdge>(edge))
                processCopy(nodeId, copyEdge);
        for (ConstraintEdge* edge : node->getGepOutEdges())
            if (GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(edge))
                processGep(nodeId, gepEdge);
    }
}

/*!
 * Handle load
 */
bool AndersenWaveDiff::handleLoad(NodeID nodeId, const ConstraintEdge* edge)
{
    bool changed = false;
    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter = getPts(nodeId).end();
         piter != epiter; ++piter) {
        if (processLoad(*piter, edge)) {
            changed = true;
        }
    }
    return changed;
}

/*!
 * Handle store
 */
bool AndersenWaveDiff::handleStore(NodeID nodeId, const ConstraintEdge* edge)
{
    bool changed = false;
    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter = getPts(nodeId).end();
         piter != epiter; ++piter) {
        if (processStore(*piter, edge)) {
            changed = true;
        }
    }
    return changed;
}

/*!
 * Propagate diff points-to set from src to dst
 */
bool AndersenWaveDiff::processCopy(NodeID node, const ConstraintEdge* edge) {
    numOfProcessedCopy++;

    bool changed = false;
    assert((SVFUtil::isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcDiffPts = getDiffPts(node);
    processCast(edge);
    if(unionPts(dst,srcDiffPts)) {
        changed = true;
        pushIntoWorklist(dst);
    }

    return changed;
}

/*!
 * Update call graph for the input indirect callsites
 */
bool AndersenWaveDiff::updateCallGraph(const CallSiteToFunPtrMap& callsites) {

    double cgUpdateStart = stat->getClk();

    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites,newEdges);
    NodePairSet cpySrcNodes;	/// nodes as a src of a generated new copy edge
    for(CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end(); it!=eit; ++it ) {
        CallSite cs = it->first;
        for(FunctionSet::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit) {
            connectCaller2CalleeParams(cs,*cit,cpySrcNodes);
        }
    }
    for(NodePairSet::iterator it = cpySrcNodes.begin(), eit = cpySrcNodes.end(); it!=eit; ++it) {
        NodeID src = sccRepNode(it->first);
        NodeID dst = sccRepNode(it->second);
        unionPts(dst, src);
        pushIntoWorklist(dst);
    }

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}

/*
 * Merge a node to its rep node
 */
void AndersenWaveDiff::mergeNodeToRep(NodeID nodeId,NodeID newRepId) {
    if(nodeId==newRepId)
        return;

    /// update rep's propagated points-to set
    updatePropaPts(newRepId, nodeId);

    Andersen::mergeNodeToRep(nodeId, newRepId);
}
