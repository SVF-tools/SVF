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
AndersenDSCD* AndersenDSCD::scdDiff = NULL;


//==================== AndersenSCD ==================//

/*!
 *
 */
void AndersenSCD::solveWorklist() {
    // Initialize the nodeStack via a whole SCC detection
    // Nodes in nodeStack are in topological order by default.
    NodeStack& nodeStack = SCCDetect();

    // propagate point-to sets
    while (!nodeStack.empty()) {
        NodeID nodeId = nodeStack.top();
        nodeStack.pop();

        // This node may be merged during collapseNodePts() which means it is no longer a rep node
        // in the graph. Only rep node needs to be handled.
        if (sccRepNode(nodeId) == nodeId && isInWorklist(nodeId)) {
            collapsePWCNode(nodeId);

            ConstraintNode *node = consCG->getConstraintNode(nodeId);
            handleCopyGep(node);

            processPWC(nodeId);
            collapseFields();
        }
    }

    // New nodes will be inserted into workList during processing.
    while (!isWorklistEmpty()) {
        NodeID nodeId = popFromWorklist();
        ConstraintNode* node = consCG->getConstraintNode(nodeId);
        // add copy edges via processing load or store edges
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

    for (NodeID nId : sccCandidates)
        pushIntoWorklist(nId);
    sccCandidates.clear();

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
    addPts(dst,src);
    addSccCandidate(dst);
}


/*!
 * If one copy edge is successful added, the src node should be added into SCC detection
 */
bool AndersenSCD::addCopyEdge(NodeID src, NodeID dst) {
    if (consCG->addCopyCGEdge(src, dst)) {
        addSccCandidate(src);
        return true;
    }
    return false;
}


/*!
 * Update call graph for the input indirect callsites
 */
bool AndersenSCD::updateCallGraph(const PointerAnalysis::CallSiteToFunPtrMap &callsites) {
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

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}



//==================== AndersenDSCD ===================//

/*!
 * Compute diff points-to set before propagation
 */
void AndersenDSCD::handleCopyGep(ConstraintNode* node) {
    computeDiffPts(node->getId());
    if (!getDiffPts(node->getId()).empty())
        Andersen::handleCopyGep(node);
}


/*!
 * Propagate diff points-to set from src to dst
 */
bool AndersenDSCD::processCopy(NodeID node, const ConstraintEdge* edge) {
    numOfProcessedCopy++;

    assert((SVFUtil::isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcDiffPts = getDiffPts(node);

    bool changed = unionPts(dst, srcDiffPts);
    if (changed)
        pushIntoWorklist(dst);
    return changed;
}


/*!
 * Propagate diff points-to set from src to dst
 */
bool AndersenDSCD::processGep(NodeID node, const GepCGEdge* edge) {
    PointsTo& srcDiffPts = getDiffPts(edge->getSrcID());
    return processGepPts(srcDiffPts, edge);
}


/*!
 * If one copy edge is successful added, the src node should be added into SCC detection
 */
bool AndersenDSCD::addCopyEdge(NodeID src, NodeID dst) {
    if (consCG->addCopyCGEdge(src, dst)) {
        updatePropaPts(src, dst);
        addSccCandidate(src);
        return true;
    }
    return false;
}


/*!
 * merge nodeId to newRepId. Return true if the newRepId is a PWC node
 */
bool AndersenDSCD::mergeSrcToTgt(NodeID nodeId, NodeID newRepId){

    if(nodeId==newRepId)
        return false;

    /// union pts of node to rep
    updatePropaPts(newRepId, nodeId);
    unionPts(newRepId,nodeId);

    /// move the edges from node to rep, and remove the node
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool gepInsideScc = consCG->moveEdgesToRepNode(node, consCG->getConstraintNode(newRepId));

    /// set rep and sub relations
    updateNodeRepAndSubs(node->getId(),newRepId);

    consCG->removeConstraintNode(node);

    return gepInsideScc;
}

