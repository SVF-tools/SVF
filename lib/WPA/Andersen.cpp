//===- Andersen.cpp -- Field-sensitive Andersen's analysis-------------------//
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
 * Andersen.cpp
 *
 *  Created on: Nov 12, 2013
 *      Author: Yulei Sui
 */

#include "WPA/Andersen.h"
#include "Util/SVFUtil.h"

using namespace SVFUtil;


Size_t Andersen::numOfProcessedAddr = 0;
Size_t Andersen::numOfProcessedCopy = 0;
Size_t Andersen::numOfProcessedGep = 0;
Size_t Andersen::numOfProcessedLoad = 0;
Size_t Andersen::numOfProcessedStore = 0;
Size_t Andersen::numOfSfrs = 0;
Size_t Andersen::numOfFieldExpand = 0;

Size_t Andersen::numOfSCCDetection = 0;
double Andersen::timeOfSCCDetection = 0;
double Andersen::timeOfSCCMerges = 0;
double Andersen::timeOfCollapse = 0;

Size_t Andersen::AveragePointsToSetSize = 0;
Size_t Andersen::MaxPointsToSetSize = 0;
double Andersen::timeOfProcessCopyGep = 0;
double Andersen::timeOfProcessLoadStore = 0;
double Andersen::timeOfUpdateCallGraph = 0;


static llvm::cl::opt<string> WriteAnder("write-ander",  llvm::cl::init(""),
                                  llvm::cl::desc("Write Andersen's analysis results to a file"));
static llvm::cl::opt<string> ReadAnder("read-ander",  llvm::cl::init(""),
                                 llvm::cl::desc("Read Andersen's analysis results from a file"));
static llvm::cl::opt<bool> PtsDiff("diff",  llvm::cl::init(true),
                                    llvm::cl::desc("Disable diff pts propagation"));
static llvm::cl::opt<bool> MergePWC("merge-pwc",  llvm::cl::init(true),
                                        llvm::cl::desc("Enable PWC in graph solving"));


/*!
 * Andersen analysis
 */
void Andersen::analyze(SVFModule svfModule) {
    /// Initialization for the Solver
    initialize(svfModule);
    
    bool readResultsFromFile = false;
    if(!ReadAnder.empty())
        readResultsFromFile = this->readFromFile(ReadAnder);

	if(!readResultsFromFile) {
		// Start solving constraints
		DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Start Solving Constraints\n"));
		solve();
		DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Finish Solving Constraints\n"));

		// Finalize the analysis
		finalize();
	}

	if (!WriteAnder.empty())
		this->writeToFile(WriteAnder);
}

/*!
 * Initilize analysis
 */
void Andersen::initialize(SVFModule svfModule) {
    resetData();
    setDiffOpt(PtsDiff);
    setPWCOpt(MergePWC);
    /// Build PAG
    PointerAnalysis::initialize(svfModule);
    /// Build Constraint Graph
    consCG = new ConstraintGraph(pag);
    setGraph(consCG);
    /// Create statistic class
    stat = new AndersenStat(this);
    consCG->dump("consCG_initial");
    /// Initialize worklist
    processAllAddr();
}

/*!
 * Start constraint solving
 */
void Andersen::processNode(NodeID nodeId) {
    // sub nodes do not need to be processed
    if (sccRepNode(nodeId) != nodeId)
        return;

    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    double insertStart = stat->getClk();
    handleLoadStore(node);
    double insertEnd = stat->getClk();
    timeOfProcessLoadStore += (insertEnd - insertStart) / TIMEINTERVAL;

    double propStart = stat->getClk();
    handleCopyGep(node);
    double propEnd = stat->getClk();
    timeOfProcessCopyGep += (propEnd - propStart) / TIMEINTERVAL;
}

/*!
 * Process copy and gep edges
 */
void Andersen::handleCopyGep(ConstraintNode* node) {
    NodeID nodeId = node->getId();
    computeDiffPts(nodeId);

    if (!getDiffPts(nodeId).empty()) {
        for (ConstraintEdge* edge : node->getCopyOutEdges())
            processCopy(nodeId, edge);
        for (ConstraintEdge* edge : node->getGepOutEdges()) {
            if (GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(edge))
                processGep(nodeId, gepEdge);
        }
    }
}

/*!
 * Process load and store edges
 */
void Andersen::handleLoadStore(ConstraintNode *node) {
    NodeID nodeId = node->getId();
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
            processAddr(SVFUtil::cast<AddrCGEdge>(*it));
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

    assert((SVFUtil::isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcPts = getDiffPts(node);

    bool changed = unionPts(dst, srcPts);
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
bool Andersen::processGep(NodeID node, const GepCGEdge* edge) {
    PointsTo& srcPts = getDiffPts(edge->getSrcID());
    return processGepPts(srcPts, edge);
}

/*!
 * Compute points-to for gep edges
 */
bool Andersen::processGepPts(PointsTo& pts, const GepCGEdge* edge)
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
            if (SVFUtil::isa<VariantGepCGEdge>(edge)) {
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
            else if (const NormalGepCGEdge* normalGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge)) {
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
    if (unionPts(dstId, tmpDstPts)) {
        pushIntoWorklist(dstId);
        return true;
    }
    return false;
}

/**
 * Detect and collapse PWC nodes produced by processing gep edges, under the constraint of field limit.
 */
inline void Andersen::collapsePWCNode(NodeID nodeId) {
    // If a node is a PWC node, collapse all its points-to tarsget.
    // collapseNodePts() may change the points-to set of the nodes which have been processed
    // before, in this case, we may need to re-do the analysis.
    if (mergePWC() && consCG->isPWCNode(nodeId) && collapseNodePts(nodeId))
        reanalyze = true;
}

inline void Andersen::collapseFields() {
    while (consCG->hasNodesToBeCollapsed()) {
        NodeID node = consCG->getNextCollapseNode();
        // collapseField() may change the points-to set of the nodes which have been processed
        // before, in this case, we may need to re-do the analysis.
        if (collapseField(node))
            reanalyze = true;
    }
}

/*
 * Merge constraint graph nodes based on SCC cycle detected.
 */
void Andersen::mergeSccCycle()
{
    NodeStack revTopoOrder;
    NodeStack & topoOrder = getSCCDetector()->topoNodeStack();
    while (!topoOrder.empty()) {
        NodeID repNodeId = topoOrder.top();
        topoOrder.pop();
        revTopoOrder.push(repNodeId);
        const NodeBS& subNodes = getSCCDetector()->subNodes(repNodeId);
        // merge sub nodes to rep node
        mergeSccNodes(repNodeId, subNodes);
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
void Andersen::mergeSccNodes(NodeID repNodeId, const NodeBS& subNodes)
{
    for (NodeBS::iterator nodeIt = subNodes.begin(); nodeIt != subNodes.end(); nodeIt++) {
        NodeID subNodeId = *nodeIt;
        if (subNodeId != repNodeId) {
            mergeNodeToRep(subNodeId, repNodeId);
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
    if (consCG->isBlkObjOrConstantObj(nodeId))
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
                pushIntoWorklist(*ptdIt);

                changed = true;
            }
            // merge field node into base node, including edges and pts.
            NodeID fieldRepNodeId = consCG->sccRepNode(fieldId);
            if (fieldRepNodeId != baseRepNodeId)
                mergeNodeToRep(fieldRepNodeId, baseRepNodeId);
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

/*!
 * Update call graph for the input indirect callsites
 */
bool Andersen::updateCallGraph(const CallSiteToFunPtrMap& callsites) {

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
        pushIntoWorklist(it->first);
    }

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}

/*!
 * Connect formal and actual parameters for indirect callsites
 */
void Andersen::connectCaller2CalleeParams(CallSite cs, const Function *F, NodePairSet &cpySrcNodes) {
    assert(F);

    DBOUT(DAndersen, outs() << "connect parameters from indirect callsite " << *cs.getInstruction() << " to callee " << *F << "\n");

    if (pag->funHasRet(F) && pag->callsiteHasRet(cs)) {
        const PAGNode* cs_return = pag->getCallSiteRet(cs);
        const PAGNode* fun_return = pag->getFunRet(F);
        if (cs_return->isPointer() && fun_return->isPointer()) {
            NodeID dstrec = sccRepNode(cs_return->getId());
            NodeID srcret = sccRepNode(fun_return->getId());
            if(addCopyEdge(srcret, dstrec)) {
                cpySrcNodes.insert(std::make_pair(srcret,dstrec));
            }
        }
        else {
            DBOUT(DAndersen, outs() << "not a pointer ignored\n");
        }
    }

    if (pag->hasCallSiteArgsMap(cs) && pag->hasFunArgsMap(F)) {

        // connect actual and formal param
        const PAG::PAGNodeList& csArgList = pag->getCallSiteArgsList(cs);
        const PAG::PAGNodeList& funArgList = pag->getFunArgsList(F);
        //Go through the fixed parameters.
        DBOUT(DPAGBuild, outs() << "      args:");
        PAG::PAGNodeList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        PAG::PAGNodeList::const_iterator csArgIt  = csArgList.begin(), csArgEit = csArgList.end();
        for (; funArgIt != funArgEit; ++csArgIt, ++funArgIt) {
            //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
            if (csArgIt  == csArgEit) {
                DBOUT(DAndersen, outs() << " !! not enough args\n");
                break;
            }
            const PAGNode *cs_arg = *csArgIt ;
            const PAGNode *fun_arg = *funArgIt;

            if (cs_arg->isPointer() && fun_arg->isPointer()) {
                DBOUT(DAndersen, outs() << "process actual parm  " << *(cs_arg->getValue()) << " \n");
                NodeID srcAA = sccRepNode(cs_arg->getId());
                NodeID dstFA = sccRepNode(fun_arg->getId());
                if(addCopyEdge(srcAA, dstFA)) {
                    cpySrcNodes.insert(std::make_pair(srcAA,dstFA));
                }
            }
        }

        //Any remaining actual args must be varargs.
        if (F->isVarArg()) {
            NodeID vaF = sccRepNode(pag->getVarargNode(F));
            DBOUT(DPAGBuild, outs() << "\n      varargs:");
            for (; csArgIt != csArgEit; ++csArgIt) {
                const PAGNode *cs_arg = *csArgIt;
                if (cs_arg->isPointer()) {
                    NodeID vnAA = sccRepNode(cs_arg->getId());
                    if (addCopyEdge(vnAA,vaF)) {
                        cpySrcNodes.insert(std::make_pair(vnAA,vaF));
                    }
                }
            }
        }
        if(csArgIt != csArgEit) {
            wrnMsg("too many args to non-vararg func.");
            wrnMsg("(" + getSourceLoc(cs.getInstruction()) + ")");
        }
    }
}

/*!
 * merge nodeId to newRepId. Return true if the newRepId is a PWC node
 */
bool Andersen::mergeSrcToTgt(NodeID nodeId, NodeID newRepId){

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
/*
 * Merge a node to its rep node based on SCC detection
 */
void Andersen::mergeNodeToRep(NodeID nodeId,NodeID newRepId) {

    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool gepInsideScc = mergeSrcToTgt(nodeId,newRepId);
    /// 1. if find gep edges inside SCC cycle, the rep node will become a PWC node and
    /// its pts should be collapsed later.
    /// 2. if the node to be merged is already a PWC node, the rep node will also become
    /// a PWC node as it will have a self-cycle gep edge.
    if (gepInsideScc || node->isPWCNode())
        consCG->setPWCNode(newRepId);
}

/*
 * Updates subnodes of its rep, and rep node of its subs
 */
void Andersen::updateNodeRepAndSubs(NodeID nodeId, NodeID newRepId) {
    consCG->setRep(nodeId,newRepId);
    NodeBS repSubs;
    repSubs.set(nodeId);
    /// update nodeToRepMap, for each subs of current node updates its rep to newRepId
    //  update nodeToSubsMap, union its subs with its rep Subs
    NodeBS& nodeSubs = consCG->sccSubNodes(nodeId);
    for(NodeBS::iterator sit = nodeSubs.begin(), esit = nodeSubs.end(); sit!=esit; ++sit) {
        NodeID subId = *sit;
        consCG->setRep(subId,newRepId);
    }
    repSubs |= nodeSubs;
    consCG->setSubs(newRepId,repSubs);
    consCG->resetSubs(nodeId);
}

/*!
 * Print pag nodes' pts by an ascending order
 */
void Andersen::dumpTopLevelPtsTo() {
    for (NodeSet::iterator nIter = this->getAllValidPtrs().begin();
         nIter != this->getAllValidPtrs().end(); ++nIter) {
        const PAGNode* node = getPAG()->getPAGNode(*nIter);
        if (getPAG()->isValidTopLevelPtr(node)) {
            PointsTo& pts = this->getPts(node->getId());
            outs() << "\nNodeID " << node->getId() << " ";

            if (pts.empty()) {
                outs() << "\t\tPointsTo: {empty}\n\n";
            } else {
                outs() << "\t\tPointsTo: { ";

                multiset<Size_t> line;
                for (PointsTo::iterator it = pts.begin(), eit = pts.end();
                     it != eit; ++it) {
                    line.insert(*it);
                }
                for (multiset<Size_t>::const_iterator it = line.begin(); it != line.end(); ++it)
                    outs() << *it << " ";
                outs() << "}\n\n";
            }
        }
    }
}

