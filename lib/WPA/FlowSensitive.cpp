//===- FlowSensitive.cpp -- Sparse flow-sensitive pointer analysis------------//
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
 * FlowSensitive.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#include "WPA/WPAStat.h"
#include "WPA/FlowSensitive.h"
#include "WPA/Andersen.h"
#include <llvm/Support/Debug.h>		// DEBUG TYPE

using namespace llvm;


FlowSensitive* FlowSensitive::fspta = NULL;

/*!
 * Initialize analysis
 */
void FlowSensitive::initialize(llvm::Module& module) {
    PointerAnalysis::initialize(module);

    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(module);
    svfg = memSSA.buildSVFG(ander);
    setGraph(svfg);
    //AndersenWaveDiff::releaseAndersenWaveDiff();

    stat = new FlowSensitiveStat(this);
}

/*!
 * Start analysis
 */
void FlowSensitive::analyze(llvm::Module& module) {
    /// Initialization for the Solver
    initialize(module);

    double start = stat->getClk();
    /// Start solving constraints
    DBOUT(DGENERAL, llvm::outs() << analysisUtil::pasMsg("Start Solving Constraints\n"));

    callGraphSCC = new CallGraphSCC(getPTACallGraph());

    do {
        numOfIteration++;

        if(0 == numOfIteration % OnTheFlyIterBudgetForStat)
            dumpStat();

        callGraphSCC->find();

        solve();

    } while (updateCallGraph(getIndirectCallsites()));

    DBOUT(DGENERAL, llvm::outs() << analysisUtil::pasMsg("Finish Solving Constraints\n"));

    double end = stat->getClk();
    solveTime += (end - start) / TIMEINTERVAL;

    /// finalize the analysis
    finalize();
}

/*!
 * Finalize analysis
 */
void FlowSensitive::finalize()
{
    svfg->dump("fs_solved", true);

    NodeStack& nodeStack = WPASolver<SVFG*>::SCCDetect();
    while (nodeStack.empty() == false) {
        NodeID rep = nodeStack.top();
        nodeStack.pop();
        const NodeBS& subNodes = getSCCDetector()->subNodes(rep);
        if (subNodes.count() > maxSCCSize)
            maxSCCSize = subNodes.count();
        if (subNodes.count() > 1) {
            numOfNodesInSCC += subNodes.count();
            numOfSCC++;
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
void FlowSensitive::processNode(NodeID nodeId) {
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
    if(AddrSVFGNode* addr = dyn_cast<AddrSVFGNode>(node)) {
        numOfProcessedAddr++;
        if(processAddr(addr))
            changed = true;
    }
    else if(CopySVFGNode* copy = dyn_cast<CopySVFGNode>(node)) {
        numOfProcessedCopy++;
        if(processCopy(copy))
            changed = true;
    }
    else if(GepSVFGNode* gep = dyn_cast<GepSVFGNode>(node)) {
        numOfProcessedGep++;
        if(processGep(gep))
            changed = true;
    }
    else if(LoadSVFGNode* load = dyn_cast<LoadSVFGNode>(node)) {
        numOfProcessedLoad++;
        if(processLoad(load))
            changed = true;
    }
    else if(StoreSVFGNode* store = dyn_cast<StoreSVFGNode>(node)) {
        numOfProcessedStore++;
        if(processStore(store))
            changed = true;
    }
    else if(PHISVFGNode* phi = dyn_cast<PHISVFGNode>(node)) {
        numOfProcessedPhi++;
        if (processPhi(phi))
            changed = true;
    }
    else if(isa<MSSAPHISVFGNode>(node) || isa<FormalINSVFGNode>(node)
            || isa<FormalOUTSVFGNode>(node) || isa<ActualINSVFGNode>(node)
            || isa<ActualOUTSVFGNode>(node)) {
        numOfProcessedMSSANode++;
        changed = true;
    }
    else if(isa<ActualParmSVFGNode>(node) || isa<FormalParmSVFGNode>(node)
            || isa<ActualRetSVFGNode>(node) || isa<FormalRetSVFGNode>(node)
            || isa<NullPtrSVFGNode>(node)) {
        changed = true;
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
bool FlowSensitive::propFromSrcToDst(SVFGEdge* edge) {
    double start = stat->getClk();
    bool changed = false;

    if (DirectSVFGEdge* dirEdge = dyn_cast<DirectSVFGEdge>(edge))
        changed = propAlongDirectEdge(dirEdge);
    else if (IndirectSVFGEdge* indEdge = dyn_cast<IndirectSVFGEdge>(edge))
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
    if (ActualParmSVFGNode* ap = dyn_cast<ActualParmSVFGNode>(src))
        changed = propagateFromAPToFP(ap, dst);
    else if (FormalRetSVFGNode* fp = dyn_cast<FormalRetSVFGNode>(src))
        changed = propagateFromFRToAR(fp, dst);
    else {
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
bool FlowSensitive::propagateFromAPToFP(const ActualParmSVFGNode* ap, const SVFGNode* dst) {
    if (const FormalParmSVFGNode* fp = dyn_cast<FormalParmSVFGNode>(dst)) {
        NodeID pagDst = fp->getParam()->getId();
        const PointsTo & srcCPts = getPts(ap->getParam()->getId());
        return unionPts(pagDst, srcCPts);
    }
    else {
        assert(false && "expecting a formal param node");
        return false;
    }
}

/*!
 * Propagate points-to information from formal-ret to actual-ret.
 * Not necessary if SVFGOPT is used instead of original SVFG.
 */
bool FlowSensitive::propagateFromFRToAR(const FormalRetSVFGNode* fr, const SVFGNode* dst) {
    if (const ActualRetSVFGNode* ar = dyn_cast<ActualRetSVFGNode>(dst)) {
        NodeID pagDst = ar->getRev()->getId();
        const PointsTo & srcCPts = getPts(fr->getRet()->getId());
        return unionPts(pagDst, srcCPts);
    }
    else {
        assert(false && "expecting a actual return node");
        return false;
    }
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
    const PointsTo& pts = edge->getPointsTo();
    for (PointsTo::iterator ptdIt = pts.begin(), ptdEit = pts.end(); ptdIt != ptdEit; ++ptdIt) {
        NodeID ptd = *ptdIt;

        if (propVarPtsFromSrcToDst(ptd, src, dst))
            changed = true;

        if (isFIObjNode(ptd)) {
            /// If this is a field-insensitive obj, propagate all field node's pts
            const NodeBS& allFields = getAllFieldsObjNode(ptd);
            for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                    fieldIt != fieldEit; ++fieldIt) {
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
    if (isa<StoreSVFGNode>(src)) {
        if (updateInFromOut(src, var, dst, var))
            changed = true;
    }
    else {
        if (updateInFromIn(src, var, dst, var))
            changed = true;
    }
    return changed;
}

/*!
 * Process address node
 */
bool FlowSensitive::processAddr(const AddrSVFGNode* addr) {
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
bool FlowSensitive::processCopy(const CopySVFGNode* copy) {
    double start = stat->getClk();
    bool changed = unionPts(copy->getPAGDstNodeID(), copy->getPAGSrcNodeID());
    double end = stat->getClk();
    copyGepTime += (end - start) / TIMEINTERVAL;
    return changed;
}

/*!
 * Process mssa phi node
 */
bool FlowSensitive::processPhi(const PHISVFGNode* phi) {
    bool changed = false;
    NodeID pagDst = phi->getRes()->getId();
    for (PHISVFGNode::OPVers::const_iterator it = phi->opVerBegin(), eit = phi->opVerEnd();	it != eit; ++it) {
        NodeID src = it->second->getId();
        const PointsTo& srcPts = getPts(src);
        if (unionPts(pagDst, srcPts))
            changed = true;
    }
    return changed;
}

/*!
 * Process gep node
 */
bool FlowSensitive::processGep(const GepSVFGNode* edge) {
    double start = stat->getClk();
    bool changed = false;
    const PointsTo& srcPts = getPts(edge->getPAGSrcNodeID());

    PointsTo tmpDstPts;
    for (PointsTo::iterator piter = srcPts.begin(); piter != srcPts.end(); ++piter) {
        NodeID ptd = *piter;
        if (isBlkObjOrConstantObj(ptd))
            tmpDstPts.set(ptd);
        else {
            if (isa<VariantGepPE>(edge->getPAGEdge())) {
                setObjFieldInsensitive(ptd);
                tmpDstPts.set(getFIObjNode(ptd));
            }
            else if (const NormalGepPE* normalGep = dyn_cast<NormalGepPE>(edge->getPAGEdge())) {
                NodeID fieldSrcPtdNode = getGepObjNode(ptd,	normalGep->getLocationSet());
                tmpDstPts.set(fieldSrcPtdNode);
            }
            else
                assert(false && "new gep edge?");
        }
    }

    if (unionPts(edge->getPAGDstNodeID(), tmpDstPts))
        changed = true;

    double end = stat->getClk();
    copyGepTime += (end - start) / TIMEINTERVAL;
    return changed;
}


/*!
 * Process load node
 *
 * Foreach node \in src
 * pts(dst) = union pts(node)
 */
bool FlowSensitive::processLoad(const LoadSVFGNode* load) {
    double start = stat->getClk();
    bool changed = false;

    NodeID dstVar = load->getPAGDstNodeID();

    const PointsTo& srcPts = getPts(load->getPAGSrcNodeID());
    for (PointsTo::iterator ptdIt = srcPts.begin(); ptdIt != srcPts.end(); ++ptdIt) {
        NodeID ptd = *ptdIt;

        if (pag->isConstantObj(ptd) || pag->isNonPointerObj(ptd))
            continue;

        if (unionPtsFromIn(load, ptd, dstVar))
            changed = true;

        if (isFIObjNode(ptd)) {
            /// If the ptd is a field-insensitive node, we should also get all field nodes'
            /// points-to sets and pass them to pagDst.
            const NodeBS& allFields = getAllFieldsObjNode(ptd);
            for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                    fieldIt != fieldEit; ++fieldIt) {
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
bool FlowSensitive::processStore(const StoreSVFGNode* store) {

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

    if(getPts(store->getPAGSrcNodeID()).empty() == false) {
        for (PointsTo::iterator it = dstPts.begin(), eit = dstPts.end(); it != eit; ++it) {
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
    if (isSU) {
        svfgHasSU.set(store->getId());
        if (strongUpdateOutFromIn(store, singleton))
            changed = true;
    }
    else {
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
bool FlowSensitive::isStrongUpdate(const SVFGNode* node, NodeID& singleton) {
    bool isSU = false;
    if (const StoreSVFGNode* store = dyn_cast<StoreSVFGNode>(node)) {
        const PointsTo& dstCPSet = getPts(store->getPAGDstNodeID());
        if (dstCPSet.count() == 1) {
            /// Find the unique element in cpts
            PointsTo::iterator it = dstCPSet.begin();
            singleton = *it;

            // Strong update can be made if this points-to target is not heap, array or field-insensitive.
            if (!isHeapMemObj(singleton) && !isArrayMemObj(singleton)
                    && pag->getBaseObj(singleton)->isFieldInsensitive() == false
                    && !isLocalVarInRecursiveFun(singleton)) {
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
void FlowSensitive::connectCallerAndCallee(const CallEdgeMap& newEdges, SVFGEdgeSetTy& edges) {
    CallEdgeMap::const_iterator iter = newEdges.begin();
    CallEdgeMap::const_iterator eiter = newEdges.end();
    for (; iter != eiter; iter++) {
        CallSite cs = iter->first;
        const FunctionSet & functions = iter->second;
        for (FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++) {
            const llvm::Function * func = *func_iter;
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
            it != eit; ++it) {
        const SVFGEdge* edge = *it;
        SVFGNode* dstNode = edge->getDstNode();
        if (isa<PHISVFGNode>(dstNode)) {
            /// If this is a formal-param or actual-ret node, we need to solve this phi
            /// node in next iteration
            pushIntoWorklist(dstNode->getId());
        }
        else if (isa<FormalINSVFGNode>(dstNode) || isa<ActualOUTSVFGNode>(dstNode)) {
            /// If this is a formal-in or actual-out node, we need to propagate points-to
            /// information from its predecessor node.
            bool changed = false;

            SVFGNode* srcNode = edge->getSrcNode();

            const PointsTo& pts = cast<IndirectSVFGEdge>(edge)->getPointsTo();
            for (PointsTo::iterator ptdIt = pts.begin(), ptdEit = pts.end(); ptdIt != ptdEit; ++ptdIt) {
                NodeID ptd = *ptdIt;

                if (propVarPtsAfterCGUpdated(ptd, srcNode, dstNode))
                    changed = true;

                if (isFIObjNode(ptd)) {
                    /// If this is a field-insensitive obj, propagate all field node's pts
                    const NodeBS& allFields = getAllFieldsObjNode(ptd);
                    for (NodeBS::iterator fieldIt = allFields.begin(), fieldEit = allFields.end();
                            fieldIt != fieldEit; ++fieldIt) {
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
    if (isa<StoreSVFGNode>(src)) {
        if (propDFOutToIn(src, var, dst, var))
            return true;
    }
    else {
        if (propDFInToIn(src, var, dst, var))
            return true;
    }
    return false;
}
