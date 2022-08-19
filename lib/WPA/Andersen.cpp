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

#include "Util/Options.h"
#include "Graphs/CHG.h"
#include "Util/SVFUtil.h"
#include "MemoryModel/PointsTo.h"
#include "WPA/Andersen.h"
#include "WPA/Steensgaard.h"

using namespace SVF;
using namespace SVFUtil;


u32_t AndersenBase::numOfProcessedAddr = 0;
u32_t AndersenBase::numOfProcessedCopy = 0;
u32_t AndersenBase::numOfProcessedGep = 0;
u32_t AndersenBase::numOfProcessedLoad = 0;
u32_t AndersenBase::numOfProcessedStore = 0;
u32_t AndersenBase::numOfSfrs = 0;
u32_t AndersenBase::numOfFieldExpand = 0;

u32_t AndersenBase::numOfSCCDetection = 0;
double AndersenBase::timeOfSCCDetection = 0;
double AndersenBase::timeOfSCCMerges = 0;
double AndersenBase::timeOfCollapse = 0;

u32_t AndersenBase::AveragePointsToSetSize = 0;
u32_t AndersenBase::MaxPointsToSetSize = 0;
double AndersenBase::timeOfProcessCopyGep = 0;
double AndersenBase::timeOfProcessLoadStore = 0;
double AndersenBase::timeOfUpdateCallGraph = 0;

/*!
 * Destructor
 */
AndersenBase::~AndersenBase()
{
    delete consCG;
    consCG = nullptr;
}

/*!
 * Initilize analysis
 */
void AndersenBase::initialize()
{
    /// Build SVFIR
    PointerAnalysis::initialize();
    /// Build Constraint Graph
    consCG = new ConstraintGraph(pag);
    setGraph(consCG);
    /// Create statistic class
    stat = new AndersenStat(this);
    if (Options::ConsCGDotGraph)
        consCG->dump("consCG_initial");
}

/*!
 * Finalize analysis
 */
void AndersenBase::finalize()
{
    /// dump constraint graph if PAGDotGraph flag is enabled
    if (Options::ConsCGDotGraph)
        consCG->dump("consCG_final");

    if (Options::PrintCGGraph)
        consCG->print();
    BVDataPTAImpl::finalize();
}

/*!
 * Andersen analysis
 */
void AndersenBase::analyze()
{
    /// Initialization for the Solver
    initialize();

    bool readResultsFromFile = false;
    if(!Options::ReadAnder.empty())
    {
        readResultsFromFile = this->readFromFile(Options::ReadAnder);
        // Finalize the analysis
        PointerAnalysis::finalize();
    }

    if(!readResultsFromFile)
    {
        // Start solving constraints
        DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Start Solving Constraints\n"));

        bool limitTimerSet = SVFUtil::startAnalysisLimitTimer(Options::AnderTimeLimit);

        initWorklist();
        do
        {
            numOfIteration++;
            if (0 == numOfIteration % iterationForPrintStat)
                printStat();

            reanalyze = false;

            solveWorklist();

            if (updateCallGraph(getIndirectCallsites()))
                reanalyze = true;

        }
        while (reanalyze);

        // Analysis is finished, reset the alarm if we set it.
        SVFUtil::stopAnalysisLimitTimer(limitTimerSet);

        DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Finish Solving Constraints\n"));

        // Finalize the analysis
        finalize();
    }

    if (!Options::WriteAnder.empty())
        this->writeToFile(Options::WriteAnder);
}

void AndersenBase::cleanConsCG(NodeID id)
{
    consCG->resetSubs(consCG->getRep(id));
    for (NodeID sub: consCG->getSubs(id))
        consCG->resetRep(sub);
    consCG->resetSubs(id);
    consCG->resetRep(id);
    assert(!consCG->hasGNode(id) && "this is either a rep nodeid or a sub nodeid should have already been merged to its field-insensitive base! ");
}

void AndersenBase::normalizePointsTo()
{
    SVFIR::MemObjToFieldsMap &memToFieldsMap = pag->getMemToFieldsMap();
    SVFIR::NodeLocationSetMap &GepObjVarMap = pag->getGepObjNodeMap();

    // clear GepObjVarMap/memToFieldsMap/nodeToSubsMap/nodeToRepMap
    // for redundant gepnodes and remove those nodes from pag
    for (NodeID n: redundantGepNodes)
    {
        NodeID base = pag->getBaseObjVar(n);
        GepObjVar *gepNode = SVFUtil::dyn_cast<GepObjVar>(pag->getGNode(n));
        assert(gepNode && "Not a gep node in redundantGepNodes set");
        const LocationSet ls = gepNode->getLocationSet();
        GepObjVarMap.erase(std::make_pair(base, ls));
        memToFieldsMap[base].reset(n);
        cleanConsCG(n);

        pag->removeGNode(gepNode);
    }
}

/*!
 * Initilize analysis
 */
void Andersen::initialize()
{
    resetData();
    setDiffOpt(Options::PtsDiff);
    setPWCOpt(Options::MergePWC);
    AndersenBase::initialize();

    if (Options::ClusterAnder) cluster();

    /// Initialize worklist
    processAllAddr();
}

/*!
 * Finalize analysis
 */
void Andersen::finalize()
{
    // TODO: check -stat too.
    // TODO: broken
    if (Options::ClusterAnder)
    {
        Map<std::string, std::string> stats;
        const PTDataTy *ptd = getPTDataTy();
        // TODO: should we use liveOnly?
        // TODO: parameterise final arg.
        NodeIDAllocator::Clusterer::evaluate(*PointsTo::getCurrentBestNodeMapping(), ptd->getAllPts(true), stats, true);
        NodeIDAllocator::Clusterer::printStats("post-main", stats);
    }

    /// sanitize field insensitive obj
    /// TODO: Fields has been collapsed during Andersen::collapseField().
    //	sanitizePts();
    AndersenBase::finalize();
}

/*!
 * Start constraint solving
 */
void Andersen::processNode(NodeID nodeId)
{
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
void Andersen::handleCopyGep(ConstraintNode* node)
{
    NodeID nodeId = node->getId();
    computeDiffPts(nodeId);

    if (!getDiffPts(nodeId).empty())
    {
        for (ConstraintEdge* edge : node->getCopyOutEdges())
            processCopy(nodeId, edge);
        for (ConstraintEdge* edge : node->getGepOutEdges())
        {
            if (GepCGEdge* gepEdge = SVFUtil::dyn_cast<GepCGEdge>(edge))
                processGep(nodeId, gepEdge);
        }
    }
}

/*!
 * Process load and store edges
 */
void Andersen::handleLoadStore(ConstraintNode *node)
{
    NodeID nodeId = node->getId();
    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter =
                getPts(nodeId).end(); piter != epiter; ++piter)
    {
        NodeID ptd = *piter;
        // handle load
        for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(),
                eit = node->outgoingLoadsEnd(); it != eit; ++it)
        {
            if (processLoad(ptd, *it))
                pushIntoWorklist(ptd);
        }

        // handle store
        for (ConstraintNode::const_iterator it = node->incomingStoresBegin(),
                eit = node->incomingStoresEnd(); it != eit; ++it)
        {
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
    for (ConstraintGraph::const_iterator nodeIt = consCG->begin(), nodeEit = consCG->end(); nodeIt != nodeEit; nodeIt++)
    {
        ConstraintNode * cgNode = nodeIt->second;
        for (ConstraintNode::const_iterator it = cgNode->incomingAddrsBegin(), eit = cgNode->incomingAddrsEnd();
                it != eit; ++it)
            processAddr(SVFUtil::cast<AddrCGEdge>(*it));
    }
}

/*!
 * Process address edges
 */
void Andersen::processAddr(const AddrCGEdge* addr)
{
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
bool Andersen::processLoad(NodeID node, const ConstraintEdge* load)
{
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
bool Andersen::processStore(NodeID node, const ConstraintEdge* store)
{
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
bool Andersen::processCopy(NodeID node, const ConstraintEdge* edge)
{
    numOfProcessedCopy++;

    assert((SVFUtil::isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    const PointsTo& srcPts = getDiffPts(node);

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
bool Andersen::processGep(NodeID, const GepCGEdge* edge)
{
    const PointsTo& srcPts = getDiffPts(edge->getSrcID());
    return processGepPts(srcPts, edge);
}

/*!
 * Compute points-to for gep edges
 */
bool Andersen::processGepPts(const PointsTo& pts, const GepCGEdge* edge)
{
    numOfProcessedGep++;

    PointsTo tmpDstPts;
    if (SVFUtil::isa<VariantGepCGEdge>(edge))
    {
        // If a pointer is connected by a variant gep edge,
        // then set this memory object to be field insensitive,
        // unless the object is a black hole/constant.
        for (NodeID o : pts)
        {
            if (consCG->isBlkObjOrConstantObj(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            if (!isFieldInsensitive(o))
            {
                setObjFieldInsensitive(o);
                consCG->addNodeToBeCollapsed(consCG->getBaseObjVar(o));
            }

            // Add the field-insensitive node into pts.
            NodeID baseId = consCG->getFIObjVar(o);
            tmpDstPts.set(baseId);
        }
    }
    else if (const NormalGepCGEdge* normalGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge))
    {
        // TODO: after the node is set to field insensitive, handling invariant
        // gep edge may lose precision because offsets here are ignored, and the
        // base object is always returned.
        for (NodeID o : pts)
        {
            if (consCG->isBlkObjOrConstantObj(o) || isFieldInsensitive(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            if (!matchType(edge->getSrcID(), o, normalGepEdge)) continue;

            NodeID fieldSrcPtdNode = consCG->getGepObjVar(o, normalGepEdge->getLocationSet());
            tmpDstPts.set(fieldSrcPtdNode);
            addTypeForGepObjNode(fieldSrcPtdNode, normalGepEdge);
        }
    }
    else
    {
        assert(false && "Andersen::processGepPts: New type GEP edge type?");
    }

    NodeID dstId = edge->getDstID();
    if (unionPts(dstId, tmpDstPts))
    {
        pushIntoWorklist(dstId);
        return true;
    }

    return false;
}

/**
 * Detect and collapse PWC nodes produced by processing gep edges, under the constraint of field limit.
 */
inline void Andersen::collapsePWCNode(NodeID nodeId)
{
    // If a node is a PWC node, collapse all its points-to tarsget.
    // collapseNodePts() may change the points-to set of the nodes which have been processed
    // before, in this case, we may need to re-do the analysis.
    if (mergePWC() && consCG->isPWCNode(nodeId) && collapseNodePts(nodeId))
        reanalyze = true;
}

inline void Andersen::collapseFields()
{
    while (consCG->hasNodesToBeCollapsed())
    {
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
    while (!topoOrder.empty())
    {
        NodeID repNodeId = topoOrder.top();
        topoOrder.pop();
        revTopoOrder.push(repNodeId);
        const NodeBS& subNodes = getSCCDetector()->subNodes(repNodeId);
        // merge sub nodes to rep node
        mergeSccNodes(repNodeId, subNodes);
    }

    // restore the topological order for later solving.
    while (!revTopoOrder.empty())
    {
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
    for (NodeBS::iterator nodeIt = subNodes.begin(); nodeIt != subNodes.end(); nodeIt++)
    {
        NodeID subNodeId = *nodeIt;
        if (subNodeId != repNodeId)
        {
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
    const PointsTo& nodePts = getPts(nodeId);
    /// Points to set may be changed during collapse, so use a clone instead.
    PointsTo ptsClone = nodePts;
    for (PointsTo::iterator ptsIt = ptsClone.begin(), ptsEit = ptsClone.end(); ptsIt != ptsEit; ptsIt++)
    {
        if (isFieldInsensitive(*ptsIt))
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
    setObjFieldInsensitive(nodeId);

    // replace all occurrences of each field with the field-insensitive node
    NodeID baseId = consCG->getFIObjVar(nodeId);
    NodeID baseRepNodeId = consCG->sccRepNode(baseId);
    NodeBS & allFields = consCG->getAllFieldsObjVars(baseId);
    for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end(); fieldIt != fieldEit; fieldIt++)
    {
        NodeID fieldId = *fieldIt;
        if (fieldId != baseId)
        {
            // use the reverse pts of this field node to find all pointers point to it
            const NodeSet revPts = getRevPts(fieldId);
            for (const NodeID o : revPts)
            {
                // change the points-to target from field to base node
                clearPts(o, fieldId);
                addPts(o, baseId);
                pushIntoWorklist(o);

                changed = true;
            }
            // merge field node into base node, including edges and pts.
            NodeID fieldRepNodeId = consCG->sccRepNode(fieldId);
            mergeNodeToRep(fieldRepNodeId, baseRepNodeId);
            if (fieldId != baseRepNodeId)
            {
                // gep node fieldId becomes redundant if it is merged to its base node who is set as field-insensitive
                // two node IDs should be different otherwise this field is actually the base and should not be removed.
                redundantGepNodes.set(fieldId);
            }
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
NodeStack& Andersen::SCCDetect()
{
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
bool Andersen::updateCallGraph(const CallSiteToFunPtrMap& callsites)
{

    double cgUpdateStart = stat->getClk();

    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites,newEdges);
    NodePairSet cpySrcNodes;	/// nodes as a src of a generated new copy edge
    for(CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end(); it!=eit; ++it )
    {
        CallSite cs = SVFUtil::getLLVMCallSite(it->first->getCallSite());
        for(FunctionSet::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit)
        {
            connectCaller2CalleeParams(cs,*cit,cpySrcNodes);
        }
    }
    for(NodePairSet::iterator it = cpySrcNodes.begin(), eit = cpySrcNodes.end(); it!=eit; ++it)
    {
        pushIntoWorklist(it->first);
    }

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}

void Andersen::heapAllocatorViaIndCall(CallSite cs, NodePairSet &cpySrcNodes)
{
    assert(SVFUtil::getCallee(cs) == nullptr && "not an indirect callsite?");
    RetICFGNode* retBlockNode = pag->getICFG()->getRetICFGNode(cs.getInstruction());
    const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
    NodeID srcret;
    CallSite2DummyValPN::const_iterator it = callsite2DummyValPN.find(cs);
    if(it != callsite2DummyValPN.end())
    {
        srcret = sccRepNode(it->second);
    }
    else
    {
        NodeID valNode = pag->addDummyValNode();
        NodeID objNode = pag->addDummyObjNode(cs.getType());
        addPts(valNode,objNode);
        callsite2DummyValPN.insert(std::make_pair(cs,valNode));
        consCG->addConstraintNode(new ConstraintNode(valNode),valNode);
        consCG->addConstraintNode(new ConstraintNode(objNode),objNode);
        srcret = valNode;
    }

    NodeID dstrec = sccRepNode(cs_return->getId());
    if(addCopyEdge(srcret, dstrec))
        cpySrcNodes.insert(std::make_pair(srcret,dstrec));
}

/*!
 * Connect formal and actual parameters for indirect callsites
 */
void Andersen::connectCaller2CalleeParams(CallSite cs, const SVFFunction* F, NodePairSet &cpySrcNodes)
{
    assert(F);

    DBOUT(DAndersen, outs() << "connect parameters from indirect callsite " << SVFUtil::value2String(cs.getInstruction()) << " to callee " << *F << "\n");

    CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(cs.getInstruction());
    RetICFGNode* retBlockNode = pag->getICFG()->getRetICFGNode(cs.getInstruction());

    if(SVFUtil::isHeapAllocExtFunViaRet(F) && pag->callsiteHasRet(retBlockNode))
    {
        heapAllocatorViaIndCall(cs,cpySrcNodes);
    }

    if (pag->funHasRet(F) && pag->callsiteHasRet(retBlockNode))
    {
        const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
        const PAGNode* fun_return = pag->getFunRet(F);
        if (cs_return->isPointer() && fun_return->isPointer())
        {
            NodeID dstrec = sccRepNode(cs_return->getId());
            NodeID srcret = sccRepNode(fun_return->getId());
            if(addCopyEdge(srcret, dstrec))
            {
                cpySrcNodes.insert(std::make_pair(srcret,dstrec));
            }
        }
        else
        {
            DBOUT(DAndersen, outs() << "not a pointer ignored\n");
        }
    }

    if (pag->hasCallSiteArgsMap(callBlockNode) && pag->hasFunArgsList(F))
    {

        // connect actual and formal param
        const SVFIR::SVFVarList& csArgList = pag->getCallSiteArgsList(callBlockNode);
        const SVFIR::SVFVarList& funArgList = pag->getFunArgsList(F);
        //Go through the fixed parameters.
        DBOUT(DPAGBuild, outs() << "      args:");
        SVFIR::SVFVarList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        SVFIR::SVFVarList::const_iterator csArgIt  = csArgList.begin(), csArgEit = csArgList.end();
        for (; funArgIt != funArgEit; ++csArgIt, ++funArgIt)
        {
            //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
            if (csArgIt  == csArgEit)
            {
                DBOUT(DAndersen, outs() << " !! not enough args\n");
                break;
            }
            const PAGNode *cs_arg = *csArgIt ;
            const PAGNode *fun_arg = *funArgIt;

            if (cs_arg->isPointer() && fun_arg->isPointer())
            {
                DBOUT(DAndersen, outs() << "process actual parm  " << cs_arg->toString() << " \n");
                NodeID srcAA = sccRepNode(cs_arg->getId());
                NodeID dstFA = sccRepNode(fun_arg->getId());
                if(addCopyEdge(srcAA, dstFA))
                {
                    cpySrcNodes.insert(std::make_pair(srcAA,dstFA));
                }
            }
        }

        //Any remaining actual args must be varargs.
        if (F->isVarArg())
        {
            NodeID vaF = sccRepNode(pag->getVarargNode(F));
            DBOUT(DPAGBuild, outs() << "\n      varargs:");
            for (; csArgIt != csArgEit; ++csArgIt)
            {
                const PAGNode *cs_arg = *csArgIt;
                if (cs_arg->isPointer())
                {
                    NodeID vnAA = sccRepNode(cs_arg->getId());
                    if (addCopyEdge(vnAA,vaF))
                    {
                        cpySrcNodes.insert(std::make_pair(vnAA,vaF));
                    }
                }
            }
        }
        if(csArgIt != csArgEit)
        {
            writeWrnMsg("too many args to non-vararg func.");
            writeWrnMsg("(" + getSourceLoc(cs.getInstruction()) + ")");
        }
    }
}

/*!
 * merge nodeId to newRepId. Return true if the newRepId is a PWC node
 */
bool Andersen::mergeSrcToTgt(NodeID nodeId, NodeID newRepId)
{

    if(nodeId==newRepId)
        return false;

    /// union pts of node to rep
    updatePropaPts(newRepId, nodeId);
    unionPts(newRepId,nodeId);

    /// move the edges from node to rep, and remove the node
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    bool pwc = consCG->moveEdgesToRepNode(node, consCG->getConstraintNode(newRepId));

    /// 1. if find gep edges inside SCC cycle, the rep node will become a PWC node and
    /// its pts should be collapsed later.
    /// 2. if the node to be merged is already a PWC node, the rep node will also become
    /// a PWC node as it will have a self-cycle gep edge.
    if(node->isPWCNode())
        pwc = true;

    /// set rep and sub relations
    updateNodeRepAndSubs(node->getId(),newRepId);

    consCG->removeConstraintNode(node);

    return pwc;
}
/*
 * Merge a node to its rep node based on SCC detection
 */
void Andersen::mergeNodeToRep(NodeID nodeId,NodeID newRepId)
{

    if (mergeSrcToTgt(nodeId,newRepId))
        consCG->setPWCNode(newRepId);
}

/*
 * Updates subnodes of its rep, and rep node of its subs
 */
void Andersen::updateNodeRepAndSubs(NodeID nodeId, NodeID newRepId)
{
    consCG->setRep(nodeId,newRepId);
    NodeBS repSubs;
    repSubs.set(nodeId);
    /// update nodeToRepMap, for each subs of current node updates its rep to newRepId
    //  update nodeToSubsMap, union its subs with its rep Subs
    NodeBS& nodeSubs = consCG->sccSubNodes(nodeId);
    for(NodeBS::iterator sit = nodeSubs.begin(), esit = nodeSubs.end(); sit!=esit; ++sit)
    {
        NodeID subId = *sit;
        consCG->setRep(subId,newRepId);
    }
    repSubs |= nodeSubs;
    consCG->setSubs(newRepId,repSubs);
    consCG->resetSubs(nodeId);
}

void Andersen::cluster(void) const
{
    assert(Options::MaxFieldLimit == 0 && "Andersen::cluster: clustering for Andersen's is currently only supported in field-insesnsitive analysis");
    Steensgaard *steens = Steensgaard::createSteensgaard(pag);
    std::vector<std::pair<unsigned, unsigned>> keys;
    for (SVFIR::iterator pit = pag->begin(); pit != pag->end(); ++pit)
    {
        keys.push_back(std::make_pair(pit->first, 1));
    }

    std::vector<std::pair<hclust_fast_methods, std::vector<NodeID>>> candidates;
    PointsTo::MappingPtr nodeMapping =
        std::make_shared<std::vector<NodeID>>(NodeIDAllocator::Clusterer::cluster(steens, keys, candidates, "aux-steens"));
    PointsTo::MappingPtr reverseNodeMapping =
        std::make_shared<std::vector<NodeID>>(NodeIDAllocator::Clusterer::getReverseNodeMapping(*nodeMapping));

    PointsTo::setCurrentBestNodeMapping(nodeMapping, reverseNodeMapping);
}

/*!
 * Print pag nodes' pts by an ascending order
 */
void Andersen::dumpTopLevelPtsTo()
{
    for (OrderedNodeSet::iterator nIter = this->getAllValidPtrs().begin();
            nIter != this->getAllValidPtrs().end(); ++nIter)
    {
        const PAGNode* node = getPAG()->getGNode(*nIter);
        if (getPAG()->isValidTopLevelPtr(node))
        {
            const PointsTo& pts = this->getPts(node->getId());
            outs() << "\nNodeID " << node->getId() << " ";

            if (pts.empty())
            {
                outs() << "\t\tPointsTo: {empty}\n\n";
            }
            else
            {
                outs() << "\t\tPointsTo: { ";

                multiset<u32_t> line;
                for (PointsTo::iterator it = pts.begin(), eit = pts.end();
                        it != eit; ++it)
                {
                    line.insert(*it);
                }
                for (multiset<u32_t>::const_iterator it = line.begin(); it != line.end(); ++it)
                    outs() << *it << " ";
                outs() << "}\n\n";
            }
        }
    }

    outs().flush();
}

