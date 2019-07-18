//===- AndersenSCD.cpp -- SCD based field-sensitive Andersen's analysis-------//
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
//===----------------------------------------------------------------------===//

/*
 * AndersenSCD.cpp
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#include "WPA/AndersenSFR.h"
#include "Util/SVFUtil.h"

using namespace SVFUtil;

AndersenSCD* AndersenSCD::scdAndersen = NULL;


/*!
 *
 */
void AndersenSCD::solveWorklist() {
    // Initialize the nodeStack via a whole SCC detection
    // Nodes in nodeStack are in topological order by default.
    NodeStack& nodeStack = SCCDetect();
    for (NodeID nId : sccCandidates)
        pushIntoWorklist(nId);
    sccCandidates.clear();

    NodeSet copyProcessed;
    copyProcessed.clear();

    // propagate point-to sets
    while (!nodeStack.empty()) {
        NodeID nodeId = nodeStack.top();
        nodeStack.pop();

        if (copyProcessed.find(nodeId) != copyProcessed.end())
            assert(false && "copy processed!!!");

        collapsePWCNode(nodeId);

        // This node may be merged during collapseNodePts() which means it is no longer a rep node
        // in the graph. Only rep node needs to be handled.
        if (sccRepNode(nodeId) == nodeId && isInWorklist(nodeId)) {
            ConstraintNode *node = consCG->getConstraintNode(nodeId);
            handleCopyGep(node);
            processPWC(nodeId);
        }

        copyProcessed.insert(nodeId);
    }
    collapseFields();

    // New nodes will be inserted into workList during processing.
    while (!isWorklistEmpty()) {
        NodeID nodeId = popFromWorklist();
        ConstraintNode* node = consCG->getConstraintNode(nodeId);
        // process nodes in worklist
        handleLoadStore(node);
    }
}


/*!
 * SCC detection for SCD
 */
NodeStack& AndersenSCD::SCCDetect() {
    numOfSCCDetection++;

    double sccStart = stat->getClk();
    getSCCDetector()->find(sccCandidates);
    double sccEnd = stat->getClk();
    timeOfSCCDetection +=  (sccEnd - sccStart)/TIMEINTERVAL;

    double mergeStart = stat->getClk();
    mergeSccCycle();
    double mergeEnd = stat->getClk();
    timeOfSCCMerges +=  (mergeEnd - mergeStart)/TIMEINTERVAL;

    return getSCCDetector()->topoNodeStack();
}


/*!
 * Source nodes of new added edges are pushed into sccCandidates.
 * Source nodes of new added edges whose pts differ from those of dst nodes are pushed into worklist.
 */
void AndersenSCD::handleLoadStore(ConstraintNode *node) {
    double insertStart = stat->getClk();

    NodeID nodeId = node->getId();

    // handle load
    for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(),
                 eit = node->outgoingLoadsEnd(); it != eit; ++it)
        for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter =
                getPts(nodeId).end(); piter != epiter; ++piter) {
            NodeID ptd = *piter;
            if (processLoad(ptd, *it)) {
                reanalyze = true;
            }
        }

    // handle store
    for (ConstraintNode::const_iterator it = node->incomingStoresBegin(),
                 eit = node->incomingStoresEnd(); it != eit; ++it)
        for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter =
                getPts(nodeId).end(); piter != epiter; ++piter) {
            NodeID ptd = *piter;
            if (processStore(ptd, *it)) {
                reanalyze = true;
            }
        }

    double insertEnd = stat->getClk();
    timeOfProcessLoadStore += (insertEnd - insertStart) / TIMEINTERVAL;
}


/*!
 * Initialize worklist via processing addrs
 */
void AndersenSCD::processAddr(const AddrCGEdge *addr) {
    numOfProcessedAddr++;

    NodeID dst = addr->getDstID();
    NodeID src = addr->getSrcID();
    if(addPts(dst,src)) {
        addSccCandidate(dst);
    }
}


/*!
 *
 */
bool AndersenSCD::addCopyEdge(NodeID src, NodeID dst) {
    if (consCG->addCopyCGEdge(src, dst)) {
        addSccCandidate(src);
        return true;
    }
    return false;
}


/*!
 *
 */
bool AndersenSCD::updateCallGraph(const CallSiteToFunPtrMap& callsites) {

    double cgUpdateStart = stat->getClk();

    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites,newEdges);
    NodePairSet cpySrcNodes;	/// nodes as a src of a generated new copy edge
    for(CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end(); it!=eit; ++it ) {
        CallSite cs = it->first;
        for(FunctionSet::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit) {
            consCG->connectCaller2CalleeParams(cs,*cit,cpySrcNodes);
        }
    }
    for(NodePairSet::iterator it = cpySrcNodes.begin(), eit = cpySrcNodes.end(); it!=eit; ++it)
        addSccCandidate(it->first);

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}