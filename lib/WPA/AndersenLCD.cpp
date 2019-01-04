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
#include "Util/SVFUtil.h"

using namespace SVFUtil;

AndersenLCD* AndersenLCD::lcdAndersen = nullptr;


void AndersenLCD::solveWorklist() {
	while (!isWorklistEmpty()) {
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
 * Solve constraints of each node
 */
void AndersenLCD::processNode(NodeID nodeId) {
	ConstraintNode* node = consCG->getConstraintNode(nodeId);

    // hand load and store
    double insertStart = stat->getClk();
    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter = getPts(
			nodeId).end(); piter != epiter; ++piter) {
		NodeID ptd = *piter;
		// handle load
		for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(),
				eit = node->outgoingLoadsEnd(); it != eit; ++it) {
			if (processLoad(ptd, *it))
				pushIntoWorklist(ptd);
		}
		// handle store
		for (ConstraintNode::const_iterator it = node->incomingStoresBegin(),
				eit = node->incomingStoresEnd(); it != eit; ++it) {
			if (processStore(ptd, *it))
				pushIntoWorklist((*it)->getSrcID());
		}
	}
    double insertEnd = stat->getClk();
    timeOfProcessLoadStore += (insertEnd - insertStart) / TIMEINTERVAL;

    // handle copy, call, return, gep
    double propStart = stat->getClk();
    for (ConstraintNode::const_iterator it = node->directOutEdgeBegin(), eit =
			node->directOutEdgeEnd(); it != eit; ++it) {
		if (GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(*it))
			processGep(nodeId, gepEdge);
		else {
			NodeID dstNodeId = (*it)->getDstID();
			PointsTo& srcPts = getPts(nodeId);
			PointsTo& dstPts = getPts(dstNodeId);

			// In one edge, if the pts of src node equals to that of dst node, and the edge
			// is never met, push it into 'metEdges' and push the dst node into 'lcdCandidates'
			if (!srcPts.empty() && srcPts == dstPts && !isMetEdge(*it)) {
				addMetEdge(*it);
				addLCDCandidate((*it)->getDstID());
			}
			processCopy(nodeId, *it);
		}
	}
    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
}

/*!
 * Collapse nodes and fields based on 'lcdCandidates'
 */
void AndersenLCD::mergeSCC() {
    if (hasLCDCandidate()) {
        NodeStack &topoRepStack = SCCDetect(lcdCandidates);
        while (!topoRepStack.empty()) {
            NodeID node = topoRepStack.top();
            topoRepStack.pop();
            pushIntoWorklist(node);
        }
        cleanLCDCandidate();
    }
}

/*!
 * AndersenLCD specified SCC detector, need to input a nodeStack 'lcdCandidate'
 */
NodeStack& AndersenLCD::SCCDetect(NodeSet& lcdCandidate) {
    numOfSCCDetection++;

	double sccStart = stat->getClk();
	/// Detect SCC cycles
	WPAConstraintSolver::SCCDetect(lcdCandidate);
	double sccEnd = stat->getClk();
	timeOfSCCDetection += (sccEnd - sccStart) / TIMEINTERVAL;

	double mergeStart = stat->getClk();
	/// Merge SCC cycles
	mergeSccCycle();
	double mergeEnd = stat->getClk();
	timeOfSCCMerges += (mergeEnd - mergeStart) / TIMEINTERVAL;

	return getSCCDetector()->topoNodeStack();
}

