//===- AndersenLCD.cpp -- LCD based field-sensitive Andersen's analysis-------//
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
 * AndersenLCD.cpp
 *
 *  Created on: 13 Sep. 2018
 *      Author: Yuxiang Lei
 */

#include "WPA/Andersen.h"
#include "MemoryModel/PointsTo.h"

using namespace SVF;
using namespace SVFUtil;

AndersenLCD* AndersenLCD::lcdAndersen = nullptr;


void AndersenLCD::solveWorklist()
{
    while (!isWorklistEmpty())
    {
        // Merge detected SCC cycles
        mergeSCC();

        NodeID nodeId = popFromWorklist();
        collapsePWCNode(nodeId);
        // Keep solving until workList is empty.
        processNode(nodeId);
        collapseFields();
    }
}

/*!
 * Process copy and gep edges
 */
void AndersenLCD::handleCopyGep(ConstraintNode* node)
{
    double propStart = stat->getClk();

    NodeID nodeId = node->getId();
    computeDiffPts(nodeId);

    for (ConstraintEdge* edge : node->getCopyOutEdges())
    {
        NodeID dstNodeId = edge->getDstID();
        const PointsTo& srcPts = getPts(nodeId);
        const PointsTo& dstPts = getPts(dstNodeId);
        // In one edge, if the pts of src node equals to that of dst node, and the edge
        // is never met, push it into 'metEdges' and push the dst node into 'lcdCandidates'
        if (!srcPts.empty() && srcPts == dstPts && !isMetEdge(edge))
        {
            addMetEdge(edge);
            addLCDCandidate((edge)->getDstID());
        }
        processCopy(nodeId, edge);
    }
    for (ConstraintEdge* edge : node->getGepOutEdges())
    {
        if (GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(edge))
            processGep(nodeId, gepEdge);
    }

    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
}

/*!
 * Collapse nodes and fields based on 'lcdCandidates'
 */
void AndersenLCD::mergeSCC()
{
    if (hasLCDCandidate())
    {
        SCCDetect();
        cleanLCDCandidate();
    }
}

/*!
 * AndersenLCD specified SCC detector, need to input a nodeStack 'lcdCandidate'
 */
NodeStack& AndersenLCD::SCCDetect()
{
    numOfSCCDetection++;

    NodeSet sccCandidates;
    sccCandidates.clear();
    for (NodeSet::iterator it = lcdCandidates.begin(); it != lcdCandidates.end(); ++it)
        if (sccRepNode(*it) == *it)
            sccCandidates.insert(*it);

    double sccStart = stat->getClk();
    /// Detect SCC cycles
    getSCCDetector()->find(sccCandidates);
    double sccEnd = stat->getClk();
    timeOfSCCDetection += (sccEnd - sccStart) / TIMEINTERVAL;

    double mergeStart = stat->getClk();
    /// Merge SCC cycles
    mergeSccCycle();
    double mergeEnd = stat->getClk();
    timeOfSCCMerges += (mergeEnd - mergeStart) / TIMEINTERVAL;

    return getSCCDetector()->topoNodeStack();
}

/*!
 * merge nodeId to newRepId. Return true if the newRepId is a PWC node
 */
bool AndersenLCD::mergeSrcToTgt(NodeID nodeId, NodeID newRepId)
{

    if(nodeId==newRepId)
        return false;

    /// union pts of node to rep
    updatePropaPts(newRepId, nodeId);
    unionPts(newRepId,nodeId);
    pushIntoWorklist(newRepId);

    /// move the edges from node to rep, and remove the node
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool gepInsideScc = consCG->moveEdgesToRepNode(node, consCG->getConstraintNode(newRepId));

    /// set rep and sub relations
    updateNodeRepAndSubs(node->getId(),newRepId);

    consCG->removeConstraintNode(node);

    return gepInsideScc;
}
