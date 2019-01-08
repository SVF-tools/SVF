//===- AndersenWave.cpp-- Wave propagation based Andersen's analysis-----------//
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
 * AndersenWave.cpp
 *
 *  Created on: Oct 17, 2013
 *      Author: Yulei Sui
 */

#include "WPA/Andersen.h"
#include "Util/SVFUtil.h"

using namespace SVFUtil;

AndersenWave* AndersenWave::waveAndersen = NULL;


void AndersenWave::solveWorklist() {
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
void AndersenWave::processNode(NodeID nodeId) {
	// This node may be merged during collapseNodePts() which means it is no longer a rep node
	// in the graph. Only rep node needs to be handled.
	if (sccRepNode(nodeId) != nodeId)
		return;

	ConstraintNode* node = consCG->getConstraintNode(nodeId);
	handleCopyGep(node);
}

/*!
 * Post process node
 */
void AndersenWave::postProcessNode(NodeID nodeId)
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
void AndersenWave::handleCopyGep(ConstraintNode* node)
{
    double propStart = stat->getClk();

    NodeID nodeId = node->getId();
    for (ConstraintNode::const_iterator it = node->directOutEdgeBegin(), eit = node->directOutEdgeEnd(); it != eit;
            ++it) {
        if(CopyCGEdge* copyEdge = SVFUtil::dyn_cast<CopyCGEdge>(*it))
            reanalyze = (processCopy(nodeId,copyEdge));
        else if(GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(*it))
            reanalyze = processGep(nodeId,gepEdge);
    }

    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
}

/*!
 * Handle load
 */
bool AndersenWave::handleLoad(NodeID nodeId, const ConstraintEdge* edge)
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
bool AndersenWave::handleStore(NodeID nodeId, const ConstraintEdge* edge)
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
