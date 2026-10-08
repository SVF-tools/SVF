//===- AndersenWaveDiff.cpp -- Wave propagation based Andersen's analysis with caching--//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
//===--------------------------------------------------------------------------------===//

/*
 * AndersenWaveDiff.cpp
 *
 *  Created on: 23/11/2013
 *      Author: yesen
 */

#include "WPA/Andersen.h"
#include "MemoryModel/PointsTo.h"
#include "Util/PTAStat.h"
#include "Util/GeneralType.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;

AndersenWaveDiff* AndersenWaveDiff::diffWave = nullptr;

/*!
 * Initialize
 */
void AndersenWaveDiff::initialize()
{
    handledPts.clear();
    handledEdges.clear();
    Andersen::initialize();
    setDetectPWC(true);   // Standard wave propagation always collapses PWCs
}

/*!
 * solve worklist
 */
void AndersenWaveDiff::solveWorklist()
{
    // Initialize the nodeStack via a whole SCC detection
    // Nodes in nodeStack are in topological order by default.
    NodeStack& nodeStack = SCCDetect();

    // Process nodeStack and put the changed nodes into workList.
    while (!nodeStack.empty())
    {
        NodeID nodeId = nodeStack.top();
        nodeStack.pop();
        collapsePWCNode(nodeId);
        // process nodes in nodeStack
        processNode(nodeId);
        collapseFields();
    }

    // New nodes will be inserted into workList during processing.
    while (!isWorklistEmpty())
    {
        NodeID nodeId = popFromWorklist();
        // process nodes in worklist
        postProcessNode(nodeId);
    }
}

/*!
 * Process edge PAGNode
 */
void AndersenWaveDiff::processNode(NodeID nodeId)
{
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
 * Post process node: add the copy edges of its loads and stores.
 * A load or store edge already handled at this node needs only the objects added to the
 * node's points-to set since then; an edge new at the node (e.g. moved here by an SCC merge)
 * needs all of them.
 */
void AndersenWaveDiff::postProcessNode(NodeID nodeId)
{
    double insertStart = stat->getClk();

    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    if (node->getLoadOutEdges().empty() && node->getStoreInEdges().empty())
        return;
    const PointsTo all = getPts(nodeId);
    PointsTo added = all;
    added -= handledPts[nodeId];
    Set<EdgeID>& handled = handledEdges[nodeId];

    // handle load
    for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(), eit = node->outgoingLoadsEnd();
            it != eit; ++it)
    {
        if (handleLoad(handled.insert((*it)->getEdgeID()).second ? all : added, *it))
            reanalyze = true;
    }
    // handle store
    for (ConstraintNode::const_iterator it = node->incomingStoresBegin(), eit =  node->incomingStoresEnd();
            it != eit; ++it)
    {
        if (handleStore(handled.insert((*it)->getEdgeID()).second ? all : added, *it))
            reanalyze = true;
    }
    handledPts[nodeId] = all;

    double insertEnd = stat->getClk();
    timeOfProcessLoadStore += (insertEnd - insertStart) / TIMEINTERVAL;
}

/*!
 * Handle load: add a copy edge from each given object to the load's destination
 */
bool AndersenWaveDiff::handleLoad(const PointsTo& objs, const ConstraintEdge* edge)
{
    bool changed = false;
    for (PointsTo::iterator piter = objs.begin(), epiter = objs.end();
            piter != epiter; ++piter)
    {
        if (processLoad(*piter, edge))
        {
            changed = true;
        }
    }
    return changed;
}

/*!
 * Handle store: add a copy edge from the store's source to each given object
 */
bool AndersenWaveDiff::handleStore(const PointsTo& objs, const ConstraintEdge* edge)
{
    bool changed = false;
    for (PointsTo::iterator piter = objs.begin(), epiter = objs.end();
            piter != epiter; ++piter)
    {
        if (processStore(*piter, edge))
        {
            changed = true;
        }
    }
    return changed;
}
