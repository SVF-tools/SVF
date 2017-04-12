//===- Andersen.cpp -- Field-sensitive Andersen's analysis-------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * Andersen.cpp
 *
 *  Created on: Nov 12, 2013
 *      Author: Yulei Sui
 */

#include "MemoryModel/PAG.h"
#include "WPA/Andersen.h"
#include "Util/AnalysisUtil.h"

#include <llvm/Support/CommandLine.h> // for tool output file

using namespace llvm;
using namespace analysisUtil;


Size_t Andersen::numOfProcessedAddr = 0;
Size_t Andersen::numOfProcessedCopy = 0;
Size_t Andersen::numOfProcessedGep = 0;
Size_t Andersen::numOfProcessedLoad = 0;
Size_t Andersen::numOfProcessedStore = 0;

Size_t Andersen::numOfSCCDetection = 0;
double Andersen::timeOfSCCDetection = 0;
double Andersen::timeOfSCCMerges = 0;
double Andersen::timeOfCollapse = 0;

Size_t Andersen::AveragePointsToSetSize = 0;
Size_t Andersen::MaxPointsToSetSize = 0;
double Andersen::timeOfProcessCopyGep = 0;
double Andersen::timeOfProcessLoadStore = 0;
double Andersen::timeOfUpdateCallGraph = 0;


static cl::opt<string> WriteAnder("write-ander",  cl::init(""),
                                  cl::desc("Write Andersen's analysis results to a file"));
static cl::opt<string> ReadAnder("read-ander",  cl::init(""),
                                 cl::desc("Read Andersen's analysis results from a file"));


/*!
 * We start from here
 */
bool Andersen::runOnModule(llvm::Module& module) {

    /// start analysis
    analyze(module);
    return false;
}

/*!
 * Andersen analysis
 */
void Andersen::analyze(llvm::Module& module) {
    /// Initialization for the Solver
    initialize(module);

    bool readResultsFromFile = false;
    if(!ReadAnder.empty())
        readResultsFromFile = this->readFromFile(ReadAnder);

    if(!readResultsFromFile) {
        DBOUT(DGENERAL, llvm::outs() << analysisUtil::pasMsg("Start Solving Constraints\n"));

        processAllAddr();

        do {
            numOfIteration++;

            if(0 == numOfIteration % OnTheFlyIterBudgetForStat) {
                dumpStat();
            }

            reanalyze = false;

            /// Start solving constraints
            solve();

            double cgUpdateStart = stat->getClk();
            if (updateCallGraph(getIndirectCallsites()))
                reanalyze = true;
            double cgUpdateEnd = stat->getClk();
            timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

        } while (reanalyze);

        DBOUT(DGENERAL, llvm::outs() << analysisUtil::pasMsg("Finish Solving Constraints\n"));

        /// finalize the analysis
        finalize();
    }

    if(!WriteAnder.empty())
        this->writeToFile(WriteAnder);
}



/*!
 * Start constraint solving
 */
void Andersen::processNode(NodeID nodeId) {

    numOfIteration++;
    if (0 == numOfIteration % OnTheFlyIterBudgetForStat) {
        dumpStat();
    }

    ConstraintNode* node = consCG->getConstraintNode(nodeId);

    for (ConstraintNode::const_iterator it = node->outgoingAddrsBegin(), eit =
                node->outgoingAddrsEnd(); it != eit; ++it) {
        processAddr(cast<AddrCGEdge>(*it));
    }

    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter =
                getPts(nodeId).end(); piter != epiter; ++piter) {
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

    // handle copy, call, return, gep
    for (ConstraintNode::const_iterator it = node->directOutEdgeBegin(), eit =
                node->directOutEdgeEnd(); it != eit; ++it) {
        if (GepCGEdge* gepEdge = llvm::dyn_cast<GepCGEdge>(*it))
            processGep(nodeId, gepEdge);
        else
            processCopy(nodeId, *it);
    }
}

/*!
 * Process address edges
 */
void Andersen::processAllAddr()
{
    for (ConstraintGraph::const_iterator nodeIt = consCG->begin(), nodeEit = consCG->end(); nodeIt != nodeEit; nodeIt++) {
        ConstraintNode * cgNode = nodeIt->second;
        for (ConstraintNode::const_iterator it = cgNode->incomingAddrsBegin(), eit = cgNode->incomingAddrsEnd();
                it != eit; ++it)
            processAddr(cast<AddrCGEdge>(*it));
    }
}

/*!
 * Process address edges
 */
void Andersen::processAddr(const AddrCGEdge* addr) {
    numOfProcessedAddr++;

    NodeID dst = addr->getDstID();
    NodeID src = addr->getSrcID();
    if(addPts(dst,src))
        pushIntoWorklist(dst);
}

/*!
 * Process load edges
 *	src --load--> dst,
 *	node \in pts(src) ==>  node--copy-->dst
 */
bool Andersen::processLoad(NodeID node, const ConstraintEdge* load) {
    /// TODO: New copy edges are also added for black hole obj node to
    ///       make gcc in spec 2000 pass the flow-sensitive analysis.
    ///       Try to handle black hole obj in an appropiate way.
//	if (pag->isBlkObjOrConstantObj(node) || isNonPointerObj(node))
    if (pag->isConstantObj(node) || isNonPointerObj(node))
        return false;

    numOfProcessedLoad++;

    NodeID dst = load->getDstID();
    return addCopyEdge(node, dst);
}

/*!
 * Process store edges
 *	src --store--> dst,
 *	node \in pts(dst) ==>  src--copy-->node
 */
bool Andersen::processStore(NodeID node, const ConstraintEdge* store) {
    /// TODO: New copy edges are also added for black hole obj node to
    ///       make gcc in spec 2000 pass the flow-sensitive analysis.
    ///       Try to handle black hole obj in an appropiate way
//	if (pag->isBlkObjOrConstantObj(node) || isNonPointerObj(node))
    if (pag->isConstantObj(node) || isNonPointerObj(node))
        return false;

    numOfProcessedStore++;

    NodeID src = store->getSrcID();
    return addCopyEdge(src, node);
}

/*!
 * Process copy edges
 *	src --copy--> dst,
 *	union pts(dst) with pts(src)
 */
bool Andersen::processCopy(NodeID node, const ConstraintEdge* edge) {
    numOfProcessedCopy++;

    assert((isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcPts = getPts(node);
    bool changed = unionPts(dst,srcPts);
    if (changed)
        pushIntoWorklist(dst);

    return changed;
}

/*!
 * Process gep edges
 *	src --gep--> dst,
 *	for each srcPtdNode \in pts(src) ==> add fieldSrcPtdNode into tmpDstPts
 *		union pts(dst) with tmpDstPts
 */
void Andersen::processGep(NodeID node, const GepCGEdge* edge) {

    PointsTo& srcPts = getPts(edge->getSrcID());
    processGepPts(srcPts, edge);
}

/*!
 * Compute points-to for gep edges
 */
void Andersen::processGepPts(PointsTo& pts, const GepCGEdge* edge)
{
    numOfProcessedGep++;

    PointsTo tmpDstPts;
    for (PointsTo::iterator piter = pts.begin(), epiter = pts.end(); piter != epiter; ++piter) {
        /// get the object
        NodeID ptd = *piter;
        /// handle blackhole and constant
        if (consCG->isBlkObjOrConstantObj(ptd)) {
            tmpDstPts.set(*piter);
        } else {
            /// handle variant gep edge
            /// If a pointer connected by a variant gep edge,
            /// then set this memory object to be field insensitive
            if (isa<VariantGepCGEdge>(edge)) {
                if (consCG->isFieldInsensitiveObj(ptd) == false) {
                    consCG->setObjFieldInsensitive(ptd);
                    consCG->addNodeToBeCollapsed(consCG->getBaseObjNode(ptd));
                }
                // add the field-insensitive node into pts.
                NodeID baseId = consCG->getFIObjNode(ptd);
                tmpDstPts.set(baseId);
            }
            /// Otherwise process invariant (normal) gep
            // TODO: after the node is set to field insensitive, handling invaraint gep edge may lose precision
            // because offset here are ignored, and it always return the base obj
            else if (const NormalGepCGEdge* normalGepEdge = dyn_cast<NormalGepCGEdge>(edge)) {
                if (!matchType(edge->getSrcID(), ptd, normalGepEdge))
                    continue;
                NodeID fieldSrcPtdNode = consCG->getGepObjNode(ptd,	normalGepEdge->getLocationSet());
                tmpDstPts.set(fieldSrcPtdNode);
                addTypeForGepObjNode(fieldSrcPtdNode, normalGepEdge);
            }
            else {
                assert(false && "new gep edge?");
            }
        }
    }

    NodeID dstId = edge->getDstID();
    if (unionPts(dstId, tmpDstPts))
        pushIntoWorklist(dstId);
}

/*
 * Merge constraint graph nodes based on SCC cycle detected.
 */
void Andersen::mergeSccCycle()
{
    NodeBS changedRepNodes;

    NodeStack revTopoOrder;
    NodeStack & topoOrder = getSCCDetector()->topoNodeStack();
    while (!topoOrder.empty()) {
        NodeID repNodeId = topoOrder.top();
        topoOrder.pop();
        revTopoOrder.push(repNodeId);

        // merge sub nodes to rep node
        mergeSccNodes(repNodeId, changedRepNodes);
    }

    // update rep/sub relation in the constraint graph.
    // each node will have a rep node
    for(NodeBS::iterator it = changedRepNodes.begin(), eit = changedRepNodes.end(); it!=eit; ++it) {
        updateNodeRepAndSubs(*it);
    }

    // restore the topological order for later solving.
    while (!revTopoOrder.empty()) {
        NodeID nodeId = revTopoOrder.top();
        revTopoOrder.pop();
        topoOrder.push(nodeId);
    }
}


/**
 * Union points-to of subscc nodes into its rep nodes
 * Move incoming/outgoing direct edges of sub node to rep node
 */
void Andersen::mergeSccNodes(NodeID repNodeId, NodeBS & chanegdRepNodes)
{
    const NodeBS& subNodes = getSCCDetector()->subNodes(repNodeId);
    for (NodeBS::iterator nodeIt = subNodes.begin(); nodeIt != subNodes.end(); nodeIt++) {
        NodeID subNodeId = *nodeIt;
        if (subNodeId != repNodeId) {
            mergeNodeToRep(subNodeId, repNodeId);
            chanegdRepNodes.set(subNodeId);
        }
    }
}

/**
 * Collapse node's points-to set. Change all points-to elements into field-insensitive.
 */
bool Andersen::collapseNodePts(NodeID nodeId)
{
    bool changed = false;
    PointsTo& nodePts = getPts(nodeId);
    /// Points to set may be changed during collapse, so use a clone instead.
    PointsTo ptsClone = nodePts;
    for (PointsTo::iterator ptsIt = ptsClone.begin(), ptsEit = ptsClone.end(); ptsIt != ptsEit; ptsIt++) {
        if (consCG->isFieldInsensitiveObj(*ptsIt))
            continue;

        if (collapseField(*ptsIt))
            changed = true;
    }
    return changed;
}

/**
 * Collapse field. make struct with the same base as nodeId become field-insensitive.
 */
bool Andersen::collapseField(NodeID nodeId)
{
    /// Black hole doesn't have structures, no collapse is needed.
    /// In later versions, instead of using base node to represent the struct,
    /// we'll create new field-insensitive node. To avoid creating a new "black hole"
    /// node, do not collapse field for black hole node.
    if (consCG->isBlkObjOrConstantObj(nodeId) || consCG->isSingleFieldObj(nodeId))
        return false;

    bool changed = false;

    double start = stat->getClk();

    // set base node field-insensitive.
    consCG->setObjFieldInsensitive(nodeId);

    // replace all occurrences of each field with the field-insensitive node
    NodeID baseId = consCG->getFIObjNode(nodeId);
    NodeID baseRepNodeId = consCG->sccRepNode(baseId);
    NodeBS & allFields = consCG->getAllFieldsObjNode(baseId);
    for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end(); fieldIt != fieldEit; fieldIt++) {
        NodeID fieldId = *fieldIt;
        if (fieldId != baseId) {
            // use the reverse pts of this field node to find all pointers point to it
            PointsTo & revPts = getRevPts(fieldId);
            for (PointsTo::iterator ptdIt = revPts.begin(), ptdEit = revPts.end();
                    ptdIt != ptdEit; ptdIt++) {
                // change the points-to target from field to base node
                PointsTo & pts = getPts(*ptdIt);
                pts.reset(fieldId);
                pts.set(baseId);

                changed = true;
            }
            // merge field node into base node, including edges and pts.
            NodeID fieldRepNodeId = consCG->sccRepNode(fieldId);
            if (fieldRepNodeId != baseRepNodeId)
                mergeNodeToRep(fieldRepNodeId, baseRepNodeId);

            // field's rep node FR has got new rep node BR during mergeNodeToRep(),
            // update all FR's sub nodes' rep node to BR.
            updateNodeRepAndSubs(fieldRepNodeId);
        }
    }

    if (consCG->isPWCNode(baseRepNodeId))
        if (collapseNodePts(baseRepNodeId))
            changed = true;

    double end = stat->getClk();
    timeOfCollapse += (end - start) / TIMEINTERVAL;

    return changed;
}

/*!
 * SCC detection on constraint graph
 */
NodeStack& Andersen::SCCDetect() {
    numOfSCCDetection++;

    double sccStart = stat->getClk();
    WPAConstraintSolver::SCCDetect();
    double sccEnd = stat->getClk();

    timeOfSCCDetection +=  (sccEnd - sccStart)/TIMEINTERVAL;

    double mergeStart = stat->getClk();

    mergeSccCycle();

    double mergeEnd = stat->getClk();

    timeOfSCCMerges +=  (mergeEnd - mergeStart)/TIMEINTERVAL;

    return getSCCDetector()->topoNodeStack();
}

/// Update call graph for the input indirect callsites
bool Andersen::updateCallGraph(const CallSiteToFunPtrMap& callsites) {
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
        pushIntoWorklist(it->first);
    }

    if(!newEdges.empty())
        return true;
    return false;
}

/*
 * Merge a node to its rep node
 */
void Andersen::mergeNodeToRep(NodeID nodeId,NodeID newRepId) {
    if(nodeId==newRepId)
        return;

    /// union pts of node to rep
    unionPts(newRepId,nodeId);

    /// move the edges from node to rep, and remove the node
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool gepInsideScc = consCG->moveEdgesToRepNode(node, consCG->getConstraintNode(newRepId));
    /// 1. if find gep edges inside SCC cycle, the rep node will become a PWC node and
    /// its pts should be collapsed later.
    /// 2. if the node to be merged is already a PWC node, the rep node will also become
    /// a PWC node as it will have a self-cycle gep edge.
    if (gepInsideScc || node->isPWCNode())
        consCG->setPWCNode(newRepId);

    consCG->removeConstraintNode(node);

    /// set rep and sub relations
    consCG->setRep(node->getId(),newRepId);
    NodeBS& newSubs = consCG->sccSubNodes(newRepId);
    newSubs.set(node->getId());
}

/*
 * Updates subnodes of its rep, and rep node of its subs
 */
void Andersen::updateNodeRepAndSubs(NodeID nodeId) {
    NodeID repId = consCG->sccRepNode(nodeId);
    NodeBS repSubs;
    /// update nodeToRepMap, for each subs of current node updates its rep to newRepId
    //  update nodeToSubsMap, union its subs with its rep Subs
    NodeBS& nodeSubs = consCG->sccSubNodes(nodeId);
    for(NodeBS::iterator sit = nodeSubs.begin(), esit = nodeSubs.end(); sit!=esit; ++sit) {
        NodeID subId = *sit;
        consCG->setRep(subId,repId);
    }
    repSubs |= nodeSubs;
    consCG->setSubs(repId,repSubs);
}
