//===- AndersenWaveDiff.cpp -- Wave propagation based Andersen's analysis with caching--//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------------------===//

/*
 * AndersenWaveDiff.cpp
 *
 *  Created on: 23/11/2013
 *      Author: yesen
 */

#include "WPA/Andersen.h"
#include <llvm/Support/CommandLine.h> // for tool output file

using namespace llvm;
using namespace analysisUtil;

AndersenWaveDiff* AndersenWaveDiff::diffWave = NULL;


/*!
 * Compute diff points-to set before propagation
 */
void AndersenWaveDiff::handleCopyGep(ConstraintNode* node)
{
    computeDiffPts(node->getId());

    AndersenWave::handleCopyGep(node);
}

/*!
 * Propagate diff points-to set from src to dst
 */
bool AndersenWaveDiff::processCopy(NodeID node, const ConstraintEdge* edge) {
    numOfProcessedCopy++;

    bool changed = false;
    assert((isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcDiffPts = getDiffPts(node);
    if(unionPts(dst,srcDiffPts)) {
        changed = true;
        pushIntoWorklist(dst);
    }

    return changed;
}

/*!
 * Propagate diff points-to set from src to dst
 */
void AndersenWaveDiff::processGep(NodeID node, const GepCGEdge* edge) {
    PointsTo& srcDiffPts = getDiffPts(edge->getSrcID());
    processGepPts(srcDiffPts, edge);
}

/*!
 * Add copy edges which haven't been added before
 */
bool AndersenWaveDiff::handleLoad(NodeID node, const ConstraintEdge* edge)
{
    /// calculate diff pts.
    PointsTo & cache = getCachePts(edge);
    PointsTo & pts = getPts(node);
    PointsTo newPts;
    newPts.intersectWithComplement(pts, cache);
    cache |= newPts;

    bool changed = false;
    for (PointsTo::iterator piter = newPts.begin(), epiter = newPts.end(); piter != epiter; ++piter) {
        NodeID ptdId = *piter;
        if (processLoad(ptdId, edge)) {
            changed = true;
        }
    }

    return changed;
}

/*!
 * Add copy edges which haven't been added before
 */
bool AndersenWaveDiff::handleStore(NodeID node, const ConstraintEdge* edge)
{
    /// calculate diff pts.
    PointsTo & cache = getCachePts(edge);
    PointsTo & pts = getPts(node);
    PointsTo newPts;
    newPts.intersectWithComplement(pts, cache);
    cache |= newPts;

    bool changed = false;
    for (PointsTo::iterator piter = newPts.begin(), epiter = newPts.end(); piter != epiter; ++piter) {
        NodeID ptdId = *piter;
        if (processStore(ptdId, edge)) {
            changed = true;
        }
    }

    return changed;
}

/*!
 * Update call graph for the input indirect callsites
 */
bool AndersenWaveDiff::updateCallGraph(const CallSiteToFunPtrMap& callsites) {
    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites,newEdges);
    NodePairSet cpySrcNodes;	/// nodes as a src of a generated new copy edge
    for(CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end(); it!=eit; ++it ) {
        llvm::CallSite cs = it->first;
        for(FunctionSet::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit) {
            consCG->connectCaller2CalleeParams(cs,*cit,cpySrcNodes);
        }
    }
    for(NodePairSet::iterator it = cpySrcNodes.begin(), eit = cpySrcNodes.end(); it!=eit; ++it) {
        NodeID src = sccRepNode(it->first);
        NodeID dst = sccRepNode(it->second);
        unionPts(dst, src);
        pushIntoWorklist(dst);
    }

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
