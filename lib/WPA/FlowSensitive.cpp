//===- FlowSensitive.cpp -- Sparse flow-sensitive pointer analysis------------//
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
 * FlowSensitive.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SVF-FE/DCHG.h"
#include "Util/SVFModule.h"
#include "Util/TypeBasedHeapCloning.h"
#include "WPA/WPAStat.h"
#include "WPA/FlowSensitive.h"
#include "WPA/Andersen.h"
#include "MemoryModel/PointsTo.h"


using namespace SVF;
using namespace SVFUtil;

FlowSensitive* FlowSensitive::fspta = nullptr;

/*!
 * Initialize analysis
 */
void FlowSensitive::initialize()
{
    PointerAnalysis::initialize();

    stat = new FlowSensitiveStat(this);

    // TODO: support clustered aux. Andersen's.
    assert(!Options::ClusterAnder && "FlowSensitive::initialize: clustering auxiliary Andersen's unsupported.");
    ander = AndersenWaveDiff::createAndersenWaveDiff(getPAG());

    // If cluster option is not set, it will give us a no-mapping points-to set.
    assert(!(Options::ClusterFs && Options::PlainMappingFs)
           && "FS::init: plain-mapping and cluster-fs are mutually exclusive.");
    if (Options::ClusterFs)
    {
        cluster();
        // Reset the points-to cache although empty so the new mapping could
        // be applied to the inserted empty set.
        getPtCache().reset();
    }
    else if (Options::PlainMappingFs)
    {
        plainMap();
        // As above.
        getPtCache().reset();
    }

    // When evaluating ctir aliases, we want the whole SVFG.
    if(Options::OPTSVFG)
        svfg = Options::CTirAliasEval ? memSSA.buildFullSVFG(ander) : memSSA.buildPTROnlySVFG(ander);
    else
        svfg = memSSA.buildPTROnlySVFGWithoutOPT(ander);

    setGraph(svfg);
    //AndersenWaveDiff::releaseAndersenWaveDiff();
}

/*!
 * Start analysis
 */
void FlowSensitive::analyze()
{
    bool limitTimerSet = SVFUtil::startAnalysisLimitTimer(Options::FsTimeLimit);

    /// Initialization for the Solver
    initialize();

    double start = stat->getClk(true);
    /// Start solving constraints
    DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Start Solving Constraints\n"));

    do
    {
        numOfIteration++;

        if(0 == numOfIteration % OnTheFlyIterBudgetForStat)
            dumpStat();

        callGraphSCC->find();

        initWorklist();
        solveWorklist();
    }
    while (updateCallGraph(getIndirectCallsites()));

    DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Finish Solving Constraints\n"));

    // Reset the time-up alarm; analysis is done.
    SVFUtil::stopAnalysisLimitTimer(limitTimerSet);

    double end = stat->getClk(true);
    solveTime += (end - start) / TIMEINTERVAL;

    if (Options::CTirAliasEval)
    {
        printCTirAliasStats();
    }

    /// finalize the analysis
    finalize();
}

/*!
 * Finalize analysis
 */
void FlowSensitive::finalize()
{
	if(Options::DumpVFG)
		svfg->dump("fs_solved", true);

    NodeStack& nodeStack = WPASolver<SVFG*>::SCCDetect();
    while (nodeStack.empty() == false)
    {
        NodeID rep = nodeStack.top();
        nodeStack.pop();
        const NodeBS& subNodes = getSCCDetector()->subNodes(rep);
        if (subNodes.count() > maxSCCSize)
            maxSCCSize = subNodes.count();
        if (subNodes.count() > 1)
        {
            numOfNodesInSCC += subNodes.count();
            numOfSCC++;
        }
    }

    // TODO: check -stat too.
    if (Options::ClusterFs)
    {
        Map<std::string, std::string> stats;
        const PTDataTy *ptd = getPTDataTy();
        // TODO: should we use liveOnly?
        Map<PointsTo, unsigned> allPts = ptd->getAllPts(true);
        // TODO: parameterise final arg.
        NodeIDAllocator::Clusterer::evaluate(*PointsTo::getCurrentBestNodeMapping(), allPts, stats, true);
        NodeIDAllocator::Clusterer::printStats("post-main: best", stats);

        // Do the same for the candidates. TODO: probably temporary for eval. purposes.
        for (std::pair<hclust_fast_methods, std::vector<NodeID>> &candidate : candidateMappings)
        {
            // Can reuse stats, since we're always filling it with `evaluate`, it will always be overwritten.
            NodeIDAllocator::Clusterer::evaluate(candidate.second, allPts, stats, true);
            NodeIDAllocator::Clusterer::printStats("post-main: candidate " + SVFUtil::hclustMethodToString(candidate.first), stats);
        }
    }

    PointerAnalysis::finalize();
}

/*!
 * SCC detection
 */
NodeStack& FlowSensitive::SCCDetect()
{
    double start = stat->getClk();
    NodeStack& nodeStack = WPASVFGFSSolver::SCCDetect();
    double end = stat->getClk();
    sccTime += (end - start) / TIMEINTERVAL;
    return nodeStack;
}

/*!
 * Process each SVFG node
 */
void FlowSensitive::processNode(NodeID nodeId)
{
    SVFGNode* node = svfg->getSVFGNode(nodeId);
    if (processSVFGNode(node))
        propagate(&node);

    clearAllDFOutVarFlag(node);
}

/*!
 * Process each SVFG node
 */
bool FlowSensitive::processSVFGNode(SVFGNode* node)
{
    double start = stat->getClk();
    bool changed = false;
    if(AddrSVFGNode* addr = SVFUtil::dyn_cast<AddrSVFGNode>(node))
    {
        numOfProcessedAddr++;
        if(processAddr(addr))
            changed = true;
    }
    else if(CopySVFGNode* copy = SVFUtil::dyn_cast<CopySVFGNode>(node))
    {
        numOfProcessedCopy++;
        if(processCopy(copy))
            changed = true;
    }
    else if(GepSVFGNode* gep = SVFUtil::dyn_cast<GepSVFGNode>(node))
    {
        numOfProcessedGep++;
        if(processGep(gep))
            changed = true;
    }
    else if(LoadSVFGNode* load = SVFUtil::dyn_cast<LoadSVFGNode>(node))
    {
        numOfProcessedLoad++;
        if(processLoad(load))
            changed = true;
    }
    else if(StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
    {
        numOfProcessedStore++;
        if(processStore(store))
            changed = true;
    }
    else if(PHISVFGNode* phi = SVFUtil::dyn_cast<PHISVFGNode>(node))
    {
        numOfProcessedPhi++;
        if (processPhi(phi))
            changed = true;
    }
    else if(SVFUtil::isa<MSSAPHISVFGNode>(node) || SVFUtil::isa<FormalINSVFGNode>(node)
            || SVFUtil::isa<FormalOUTSVFGNode>(node) || SVFUtil::isa<ActualINSVFGNode>(node)
            || SVFUtil::isa<ActualOUTSVFGNode>(node))
    {
        numOfProcessedMSSANode++;
        changed = true;
    }
    else if(SVFUtil::isa<ActualParmSVFGNode>(node) || SVFUtil::isa<FormalParmSVFGNode>(node)
            || SVFUtil::isa<ActualRetSVFGNode>(node) || SVFUtil::isa<FormalRetSVFGNode>(node)
            || SVFUtil::isa<NullPtrSVFGNode>(node))
    {
        changed = true;
    }
    else if(SVFUtil::isa<CmpVFGNode>(node) || SVFUtil::isa<BinaryOPVFGNode>(node) || SVFUtil::dyn_cast<UnaryOPVFGNode>(node))
    {

    }
    else
        assert(false && "unexpected kind of SVFG nodes");

    double end = stat->getClk();
    processTime += (end - start) / TIMEINTERVAL;

    return changed;
}

/*!
 * Propagate points-to information from source to destination node
 * Union dfOutput of src to dfInput of dst.
 * Only propagate points-to set of node which exists on the SVFG edge.
 * 1. propagation along direct edge will always return TRUE.
 * 2. propagation along indirect edge will return TRUE if destination node's
 *    IN set has been updated.
 */
bool FlowSensitive::propFromSrcToDst(SVFGEdge* edge)
{
    double start = stat->getClk();
    bool changed = false;

    if (DirectSVFGEdge* dirEdge = SVFUtil::dyn_cast<DirectSVFGEdge>(edge))
        changed = propAlongDirectEdge(dirEdge);
    else if (IndirectSVFGEdge* indEdge = SVFUtil::dyn_cast<IndirectSVFGEdge>(edge))
        changed = propAlongIndirectEdge(indEdge);
    else
        assert(false && "new kind of svfg edge?");

    double end = stat->getClk();
    propagationTime += (end - start) /TIMEINTERVAL;
    return changed;
}

/*!
 * Propagate points-to information along DIRECT SVFG edge.
 */
bool FlowSensitive::propAlongDirectEdge(const DirectSVFGEdge* edge)
{
    double start = stat->getClk();
    bool changed = false;

    SVFGNode* src = edge->getSrcNode();
    SVFGNode* dst = edge->getDstNode();
    // If this is an actual-param or formal-ret, top-level pointer's pts must be
    // propagated from src to dst.
    if (ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(src))
        changed = propagateFromAPToFP(ap, dst);
    else if (FormalRetSVFGNode* fp = SVFUtil::dyn_cast<FormalRetSVFGNode>(src))
        changed = propagateFromFRToAR(fp, dst);
    else
    {
        // Direct SVFG edge links between def and use of a top-level pointer.
        // There's no points-to information propagated along direct edge.
        // Since the top-level pointer's value has been changed at src node,
        // return TRUE to put dst node into the work list.
        changed = true;
    }

    double end = stat->getClk();
    directPropaTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Propagate points-to information from actual-param to formal-param.
 *  Not necessary if SVFGOPT is used instead of original SVFG.
 */
bool FlowSensitive::propagateFromAPToFP(const ActualParmSVFGNode* ap, const SVFGNode* dst)
{
    const FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(dst);
    assert(fp && "expecting a formal param node");

    NodeID pagDst = fp->getParam()->getId();
    const PointsTo &srcCPts = getPts(ap->getParam()->getId());
    bool changed = unionPts(pagDst, srcCPts);

    return changed;
}

/*!
 * Propagate points-to information from formal-ret to actual-ret.
 * Not necessary if SVFGOPT is used instead of original SVFG.
 */
bool FlowSensitive::propagateFromFRToAR(const FormalRetSVFGNode* fr, const SVFGNode* dst)
{
    const ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(dst);
    assert(ar && "expecting an actual return node");

    NodeID pagDst = ar->getRev()->getId();
    const PointsTo & srcCPts = getPts(fr->getRet()->getId());
    bool changed = unionPts(pagDst, srcCPts);

    return changed;
}

/*!
 * Propagate points-to information along INDIRECT SVFG edge.
 */
bool FlowSensitive::propAlongIndirectEdge(const IndirectSVFGEdge* edge)
{
    double start = stat->getClk();

    SVFGNode* src = edge->getSrcNode();
    SVFGNode* dst = edge->getDstNode();

    bool changed = false;

    // Get points-to targets may be used by next SVFG node.
    // Propagate points-to set for node used in dst.
    const NodeBS& pts = edge->getPointsTo();
    for (NodeBS::iterator ptdIt = pts.begin(), ptdEit = pts.end(); ptdIt != ptdEit; ++ptdIt)
    {
        NodeID ptd = *ptdIt;

        if (propVarPtsFromSrcToDst(ptd, src, dst))
            changed = true;

        if (isFieldInsensitive(ptd))
        {
            /// If this is a field-insensitive obj, propagate all field node's pts
            const NodeBS& allFields = getAllFieldsObjNode(ptd);
            for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                    fieldIt != fieldEit; ++fieldIt)
            {
                if (propVarPtsFromSrcToDst(*fieldIt, src, dst))
                    changed = true;
            }
        }
    }

    double end = stat->getClk();
    indirectPropaTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Propagate points-to information of a certain variable from src to dst.
 */
bool FlowSensitive::propVarPtsFromSrcToDst(NodeID var, const SVFGNode* src, const SVFGNode* dst)
{
    bool changed = false;
    if (SVFUtil::isa<StoreSVFGNode>(src))
    {
        if (updateInFromOut(src, var, dst, var))
            changed = true;
    }
    else
    {
        if (updateInFromIn(src, var, dst, var))
            changed = true;
    }
    return changed;
}

/*!
 * Process address node
 */
bool FlowSensitive::processAddr(const AddrSVFGNode* addr)
{
    double start = stat->getClk();
    NodeID srcID = addr->getPAGSrcNodeID();
    /// TODO: If this object has been set as field-insensitive, just
    ///       add the insensitive object node into dst pointer's pts.
    if (isFieldInsensitive(srcID))
        srcID = getFIObjNode(srcID);
    bool changed = addPts(addr->getPAGDstNodeID(), srcID);
    double end = stat->getClk();
    addrTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Process copy node
 */
bool FlowSensitive::processCopy(const CopySVFGNode* copy)
{
    double start = stat->getClk();
    bool changed = unionPts(copy->getPAGDstNodeID(), copy->getPAGSrcNodeID());
    double end = stat->getClk();
    copyTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Process mssa phi node
 */
bool FlowSensitive::processPhi(const PHISVFGNode* phi)
{
    double start = stat->getClk();
    bool changed = false;
    NodeID pagDst = phi->getRes()->getId();
    for (PHISVFGNode::OPVers::const_iterator it = phi->opVerBegin(), eit = phi->opVerEnd();	it != eit; ++it)
    {
        NodeID src = it->second->getId();
        const PointsTo& srcPts = getPts(src);
        if (unionPts(pagDst, srcPts))
            changed = true;
    }

    double end = stat->getClk();
    phiTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Process gep node
 */
bool FlowSensitive::processGep(const GepSVFGNode* edge)
{
    double start = stat->getClk();
    bool changed = false;
    const PointsTo& srcPts = getPts(edge->getPAGSrcNodeID());

    PointsTo tmpDstPts;
    if (SVFUtil::isa<VariantGepPE>(edge->getPAGEdge()))
    {
        for (NodeID o : srcPts)
        {
            if (isBlkObjOrConstantObj(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            setObjFieldInsensitive(o);
            tmpDstPts.set(getFIObjNode(o));
        }
    }
    else if (const NormalGepPE* normalGep = SVFUtil::dyn_cast<NormalGepPE>(edge->getPAGEdge()))
    {
        for (NodeID o : srcPts)
        {
            if (isBlkObjOrConstantObj(o))
            {
                tmpDstPts.set(o);
                continue;
            }

            NodeID fieldSrcPtdNode = getGepObjNode(o, normalGep->getLocationSet());
            tmpDstPts.set(fieldSrcPtdNode);
        }
    }
    else
    {
        assert(false && "FlowSensitive::processGep: New type GEP edge type?");
    }

    if (unionPts(edge->getPAGDstNodeID(), tmpDstPts))
        changed = true;

    double end = stat->getClk();
    gepTime += (end - start) / TIMEINTERVAL;
    return changed;
}


/*!
 * Process load node
 *
 * Foreach node \in src
 * pts(dst) = union pts(node)
 */
bool FlowSensitive::processLoad(const LoadSVFGNode* load)
{
    double start = stat->getClk();
    bool changed = false;

    NodeID dstVar = load->getPAGDstNodeID();

    const PointsTo& srcPts = getPts(load->getPAGSrcNodeID());
    for (PointsTo::iterator ptdIt = srcPts.begin(); ptdIt != srcPts.end(); ++ptdIt)
    {
        NodeID ptd = *ptdIt;

        if (pag->isConstantObj(ptd) || pag->isNonPointerObj(ptd))
            continue;

        if (unionPtsFromIn(load, ptd, dstVar))
            changed = true;

        if (isFieldInsensitive(ptd))
        {
            /// If the ptd is a field-insensitive node, we should also get all field nodes'
            /// points-to sets and pass them to pagDst.
            const NodeBS& allFields = getAllFieldsObjNode(ptd);
            for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                    fieldIt != fieldEit; ++fieldIt)
            {
                if (unionPtsFromIn(load, *fieldIt, dstVar))
                    changed = true;
            }
        }
    }

    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Process store node
 *
 * foreach node \in dst
 * pts(node) = union pts(src)
 */
bool FlowSensitive::processStore(const StoreSVFGNode* store)
{

    const PointsTo & dstPts = getPts(store->getPAGDstNodeID());

    /// STORE statement can only be processed if the pointer on the LHS
    /// points to something. If we handle STORE with an empty points-to
    /// set, the OUT set will be updated from IN set. Then if LHS pointer
    /// points-to one target and it has been identified as a strong
    /// update, we can't remove those points-to information computed
    /// before this strong update from the OUT set.
    if (dstPts.empty())
        return false;

    double start = stat->getClk();
    bool changed = false;

    if(getPts(store->getPAGSrcNodeID()).empty() == false)
    {
        for (PointsTo::iterator it = dstPts.begin(), eit = dstPts.end(); it != eit; ++it)
        {
            NodeID ptd = *it;

            if (pag->isConstantObj(ptd) || pag->isNonPointerObj(ptd))
                continue;

            if (unionPtsFromTop(store, store->getPAGSrcNodeID(), ptd))
                changed = true;
        }
    }

    double end = stat->getClk();
    storeTime += (end - start) / TIMEINTERVAL;

    double updateStart = stat->getClk();
    // also merge the DFInSet to DFOutSet.
    /// check if this is a strong updates store
    NodeID singleton;
    bool isSU = isStrongUpdate(store, singleton);
    if (isSU)
    {
        svfgHasSU.set(store->getId());
        if (strongUpdateOutFromIn(store, singleton))
            changed = true;
    }
    else
    {
        svfgHasSU.reset(store->getId());
        if (weakUpdateOutFromIn(store))
            changed = true;
    }
    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    return changed;
}

/*!
 * Return TRUE if this is a strong update STORE statement.
 */
bool FlowSensitive::isStrongUpdate(const SVFGNode* node, NodeID& singleton)
{
    bool isSU = false;
    if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(node))
    {
        const PointsTo& dstCPSet = getPts(store->getPAGDstNodeID());
        if (dstCPSet.count() == 1)
        {
            /// Find the unique element in cpts
            PointsTo::iterator it = dstCPSet.begin();
            singleton = *it;

            // Strong update can be made if this points-to target is not heap, array or field-insensitive.
            if (!isHeapMemObj(singleton) && !isArrayMemObj(singleton)
                    && pag->getBaseObj(singleton)->isFieldInsensitive() == false
                    && !isLocalVarInRecursiveFun(singleton))
            {
                isSU = true;
            }
        }
    }
    return isSU;
}

/*!
 * Update call graph
 */
bool FlowSensitive::updateCallGraph(const CallSiteToFunPtrMap& callsites)
{
    double start = stat->getClk();
    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites, newEdges);

    // Bound the new edges by the Andersen's call graph.
    // TODO: we want this to be an assertion eventually.
    const CallEdgeMap &andersCallEdgeMap = ander->getIndCallMap();
    for (typename CallEdgeMap::value_type &csfs : newEdges)
    {
        const CallBlockNode *potentialCallSite = csfs.first;
        FunctionSet &potentialFunctionSet = csfs.second;

        // Check this callsite even calls anything per Andersen's.
        typename CallEdgeMap::const_iterator andersFunctionSetIt
            = andersCallEdgeMap.find(potentialCallSite);
        if (andersFunctionSetIt == andersCallEdgeMap.end())
        {
            potentialFunctionSet.clear();
        }

        const FunctionSet &andersFunctionSet = andersFunctionSetIt->second;
        for (FunctionSet::iterator potentialFunctionIt = potentialFunctionSet.begin();
             potentialFunctionIt != potentialFunctionSet.end(); )
        {
            const SVFFunction *potentialFunction = *potentialFunctionIt;
            if (andersFunctionSet.find(potentialFunction) == andersFunctionSet.end())
            {
                // potentialFunction is not in the Andersen's call graph -- remove it.
                potentialFunctionIt = potentialFunctionSet.erase(potentialFunctionIt);
            }
            else
            {
                // potentialFunction is in the Andersen's call graph -- keep it..
                ++potentialFunctionIt;
            }
        }
    }

    SVFGEdgeSetTy svfgEdges;
    connectCallerAndCallee(newEdges, svfgEdges);

    updateConnectedNodes(svfgEdges);

    double end = stat->getClk();
    updateCallGraphTime += (end - start) / TIMEINTERVAL;
    return (!newEdges.empty());
}

/*!
 *  Handle parameter passing in SVFG
 */
void FlowSensitive::connectCallerAndCallee(const CallEdgeMap& newEdges, SVFGEdgeSetTy& edges)
{
    CallEdgeMap::const_iterator iter = newEdges.begin();
    CallEdgeMap::const_iterator eiter = newEdges.end();
    for (; iter != eiter; iter++)
    {
        const CallBlockNode* cs = iter->first;
        const FunctionSet & functions = iter->second;
        for (FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const SVFFunction*  func = *func_iter;
            svfg->connectCallerAndCallee(cs, func, edges);
        }
    }
}

/*!
 * Push nodes connected during update call graph into worklist so they will be
 * solved during next iteration.
 */
void FlowSensitive::updateConnectedNodes(const SVFGEdgeSetTy& edges)
{
    for (SVFGEdgeSetTy::const_iterator it = edges.begin(), eit = edges.end();
            it != eit; ++it)
    {
        const SVFGEdge* edge = *it;
        SVFGNode* dstNode = edge->getDstNode();
        if (SVFUtil::isa<PHISVFGNode>(dstNode))
        {
            /// If this is a formal-param or actual-ret node, we need to solve this phi
            /// node in next iteration
            pushIntoWorklist(dstNode->getId());
        }
        else if (SVFUtil::isa<FormalINSVFGNode>(dstNode) || SVFUtil::isa<ActualOUTSVFGNode>(dstNode))
        {
            /// If this is a formal-in or actual-out node, we need to propagate points-to
            /// information from its predecessor node.
            bool changed = false;

            SVFGNode* srcNode = edge->getSrcNode();

            const NodeBS& pts = SVFUtil::cast<IndirectSVFGEdge>(edge)->getPointsTo();
            for (NodeBS::iterator ptdIt = pts.begin(), ptdEit = pts.end(); ptdIt != ptdEit; ++ptdIt)
            {
                NodeID ptd = *ptdIt;

                if (propVarPtsAfterCGUpdated(ptd, srcNode, dstNode))
                    changed = true;

                if (isFieldInsensitive(ptd))
                {
                    /// If this is a field-insensitive obj, propagate all field node's pts
                    const NodeBS& allFields = getAllFieldsObjNode(ptd);
                    for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                            fieldIt != fieldEit; ++fieldIt)
                    {
                        if (propVarPtsAfterCGUpdated(*fieldIt, srcNode, dstNode))
                            changed = true;
                    }
                }
            }

            if (changed)
                pushIntoWorklist(dstNode->getId());
        }
    }
}


/*!
 * Propagate points-to information of a certain variable from src to dst.
 */
bool FlowSensitive::propVarPtsAfterCGUpdated(NodeID var, const SVFGNode* src, const SVFGNode* dst)
{
    if (SVFUtil::isa<StoreSVFGNode>(src))
    {
        if (propDFOutToIn(src, var, dst, var))
            return true;
    }
    else
    {
        if (propDFInToIn(src, var, dst, var))
            return true;
    }
    return false;
}

void FlowSensitive::cluster(void)
{
    std::vector<std::pair<unsigned, unsigned>> keys;
    for (PAG::iterator pit = pag->begin(); pit != pag->end(); ++pit) keys.push_back(std::make_pair(pit->first, 1));

    PointsTo::MappingPtr nodeMapping =
        std::make_shared<std::vector<NodeID>>(NodeIDAllocator::Clusterer::cluster(ander, keys, candidateMappings, "aux-ander"));
    PointsTo::MappingPtr reverseNodeMapping =
        std::make_shared<std::vector<NodeID>>(NodeIDAllocator::Clusterer::getReverseNodeMapping(*nodeMapping));

    PointsTo::setCurrentBestNodeMapping(nodeMapping, reverseNodeMapping);
}

void FlowSensitive::plainMap(void) const
{
    assert(Options::NodeAllocStrat == NodeIDAllocator::Strategy::DENSE
           && "FS::cluster: plain mapping requires dense allocation strategy.");

    const size_t numObjects = NodeIDAllocator::get()->getNumObjects();
    PointsTo::MappingPtr plainMapping = std::make_shared<std::vector<NodeID>>(numObjects);
    PointsTo::MappingPtr reversePlainMapping = std::make_shared<std::vector<NodeID>>(numObjects);
    for (NodeID i = 0; i < plainMapping->size(); ++i)
    {
        plainMapping->at(i) = i;
        reversePlainMapping->at(i) = i;
    }

    PointsTo::setCurrentBestNodeMapping(plainMapping, reversePlainMapping);
}

void FlowSensitive::printCTirAliasStats(void)
{
    DCHGraph *dchg = SVFUtil::dyn_cast<DCHGraph>(chgraph);
    assert(dchg && "eval-ctir-aliases needs DCHG.");

    // < SVFG node ID (loc), PAG node of interest (top-level pointer) >.
    Set<std::pair<NodeID, NodeID>> cmpLocs;
    for (SVFG::iterator npair = svfg->begin(); npair != svfg->end(); ++npair)
    {
        NodeID loc = npair->first;
        SVFGNode *node = npair->second;

        // Only care about loads, stores, and GEPs.
        if (StmtSVFGNode *stmt = SVFUtil::dyn_cast<StmtSVFGNode>(node))
        {
            if (!SVFUtil::isa<LoadSVFGNode>(stmt) && !SVFUtil::isa<StoreSVFGNode>(stmt)
                    && !SVFUtil::isa<GepSVFGNode>(stmt))
            {
                continue;
            }

            if (!TypeBasedHeapCloning::getRawCTirMetadata(stmt->getInst() ? stmt->getInst() : stmt->getPAGEdge()->getValue()))
            {
                continue;
            }

            NodeID p = 0;
            if (SVFUtil::isa<LoadSVFGNode>(stmt))
            {
                p = stmt->getPAGSrcNodeID();
            }
            else if (SVFUtil::isa<StoreSVFGNode>(stmt))
            {
                p = stmt->getPAGDstNodeID();
            }
            else if (SVFUtil::isa<GepSVFGNode>(stmt))
            {
                p = stmt->getPAGSrcNodeID();
            }
            else
            {
                // Not interested.
                continue;
            }

            cmpLocs.insert(std::make_pair(loc, p));
        }
    }

    unsigned mayAliases = 0, noAliases = 0;
    countAliases(cmpLocs, &mayAliases, &noAliases);

    unsigned total = mayAliases + noAliases;
    llvm::outs() << "eval-ctir-aliases "
                 << total << " "
                 << mayAliases << " "
                 << noAliases << " "
                 << "\n";
    llvm::outs() << "  " << "TOTAL : " << total << "\n"
                 << "  " << "MAY   : " << mayAliases << "\n"
                 << "  " << "MAY % : " << 100 * ((double)mayAliases/(double)(total)) << "\n"
                 << "  " << "NO    : " << noAliases << "\n"
                 << "  " << "NO  % : " << 100 * ((double)noAliases/(double)(total)) << "\n";
}

void FlowSensitive::countAliases(Set<std::pair<NodeID, NodeID>> cmp, unsigned *mayAliases, unsigned *noAliases)
{
    for (std::pair<NodeID, NodeID> locPA : cmp)
    {
        // loc doesn't make a difference for FSPTA.
        NodeID p = locPA.second;
        for (std::pair<NodeID, NodeID> locPB : cmp)
        {
            if (locPB == locPA) continue;

            NodeID q = locPB.second;

            switch (alias(p, q))
            {
            case llvm::AliasResult::NoAlias:
                ++(*noAliases);
                break;
            case llvm::AliasResult::MayAlias:
                ++(*mayAliases);
                break;
            default:
                assert("Not May/NoAlias?");
            }
        }
    }

}
