//===- AndersenWave.cpp-- Wave propagation based Andersen's analysis-----------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * AndersenWave.cpp
 *
 *  Created on: Oct 17, 2013
 *      Author: Yulei Sui
 */

#include "WPA/Andersen.h"
#include "Util/AnalysisUtil.h"

#include <llvm/Support/CommandLine.h> // for tool output file
using namespace llvm;
using namespace analysisUtil;

AndersenWave* AndersenWave::waveAndersen = NULL;

/*!
 * Process edge PAGNode
 */
void AndersenWave::processNode(NodeID nodeId)
{
    double propStart = stat->getClk();

    // If this is a PWC node, collapse all its points-to targets.
    // collapseNodePts() may change the points-to set of the nodes which have been processed
    // before, in this case, we may need to re-do the analysis.
    if (consCG->isPWCNode(nodeId) && collapseNodePts(nodeId))
        reanalyze = true;

    // This node may be merged during collapseNodePts() which means it is no longer a rep node
    // in the graph. Only rep node needs to be handled.
    if (sccRepNode(nodeId) != nodeId)
        return;

    ConstraintNode* node = consCG->getConstraintNode(nodeId);

    handleCopyGep(node);

    // collapse nodes found during processing variant gep edges.
    while (consCG->hasNodesToBeCollapsed()) {
        NodeID nodeId = consCG->getNextCollapseNode();
        // collapseField() may change the points-to set of the nodes which have been processed
        // before, in this case, we may need to re-do the analysis.
        if (collapseField(nodeId))
            reanalyze = true;
    }
    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
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
    NodeID nodeId = node->getId();
    for (ConstraintNode::const_iterator it = node->directOutEdgeBegin(), eit = node->directOutEdgeEnd(); it != eit;
            ++it) {
        if(CopyCGEdge* copyEdge = dyn_cast<CopyCGEdge>(*it))
            processCopy(nodeId,copyEdge);
        else if(GepCGEdge* gepEdge = dyn_cast<GepCGEdge>(*it))
            processGep(nodeId,gepEdge);
    }
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
