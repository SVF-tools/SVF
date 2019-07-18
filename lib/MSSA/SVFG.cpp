//===- SVFG.cpp -- Sparse value-flow graph-----------------------------------//
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
 * SVFG.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#include "MSSA/SVFG.h"
#include "MSSA/SVFGOPT.h"
#include "MSSA/SVFGStat.h"
#include "Util/SVFUtil.h"
#include "Util/SVFModule.h"

using namespace SVFUtil;


static llvm::cl::opt<bool> DumpVFG("dump-svfg", llvm::cl::init(false),
                             llvm::cl::desc("Dump dot graph of SVFG"));

/*!
 * Constructor
 */
SVFG::SVFG(MemSSA* _mssa, VFGK k): VFG(_mssa->getPTA()->getPTACallGraph(),k),mssa(_mssa), pta(mssa->getPTA()) {
    stat = new SVFGStat(this);
}

/*!
 * Memory has been cleaned up at GenericGraph
 */
void SVFG::destroy() {
    delete stat;
    stat = NULL;
    mssa = NULL;
}

/*!
 * Build SVFG
 * 1) build SVFG nodes
 *    a) statements for top level pointers (PAGEdges)
 *    b) operators of address-taken variables (MSSAPHI and MSSACHI)
 * 2) connect SVFG edges
 *    a) between two statements (PAGEdges)
 *    b) between two memory SSA operators (MSSAPHI MSSAMU and MSSACHI)
 */
void SVFG::buildSVFG() {
    stat->startClk();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Addr-taken Node\n"));

    stat->ATVFNodeStart();
    addSVFGNodesForAddrTakenVars();
    stat->ATVFNodeEnd();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Indirect Edge\n"));

    stat->indVFEdgeStart();
    connectIndirectSVFGEdges();
    stat->indVFEdgeEnd();

}

/*
 * Create SVFG nodes for address-taken variables
 */
void SVFG::addSVFGNodesForAddrTakenVars() {

    // set defs for address-taken vars defined at store statements
    PAGEdge::PAGEdgeSetTy& stores = getPAGEdgeSet(PAGEdge::Store);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter) {
        StorePE* store = SVFUtil::cast<StorePE>(*iter);
        const StmtSVFGNode* sNode = getStmtVFGNode(store);
        for(CHISet::iterator pi = mssa->getCHISet(store).begin(), epi = mssa->getCHISet(store).end(); pi!=epi; ++pi)
            setDef((*pi)->getResVer(),sNode);
    }

    /// set defs for address-taken vars defined at phi/chi/call
    /// create corresponding def and use nodes for address-taken vars (a.k.a MRVers)
    /// initialize memory SSA phi nodes (phi of address-taken variables)
    for(MemSSA::BBToPhiSetMap::iterator it = mssa->getBBToPhiSetMap().begin(),
            eit = mssa->getBBToPhiSetMap().end(); it!=eit; ++it) {
        for(PHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addIntraMSSAPHISVFGNode(*pi);
    }
    /// initialize memory SSA entry chi nodes
    for(MemSSA::FunToEntryChiSetMap::iterator it = mssa->getFunToEntryChiSetMap().begin(),
            eit = mssa->getFunToEntryChiSetMap().end(); it!=eit; ++it) {
        for(CHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addFormalINSVFGNode(SVFUtil::cast<ENTRYCHI>(*pi));
    }
    /// initialize memory SSA return mu nodes
    for(MemSSA::FunToReturnMuSetMap::iterator it = mssa->getFunToRetMuSetMap().begin(),
            eit = mssa->getFunToRetMuSetMap().end(); it!=eit; ++it) {
        for(MUSet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addFormalOUTSVFGNode(SVFUtil::cast<RETMU>(*pi));
    }
    /// initialize memory SSA callsite mu nodes
    for(MemSSA::CallSiteToMUSetMap::iterator it = mssa->getCallSiteToMuSetMap().begin(),
            eit = mssa->getCallSiteToMuSetMap().end();
            it!=eit; ++it) {
        for(MUSet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addActualINSVFGNode(SVFUtil::cast<CALLMU>(*pi));
    }
    /// initialize memory SSA callsite chi nodes
    for(MemSSA::CallSiteToCHISetMap::iterator it = mssa->getCallSiteToChiSetMap().begin(),
            eit = mssa->getCallSiteToChiSetMap().end();
            it!=eit; ++it) {
        for(CHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addActualOUTSVFGNode(SVFUtil::cast<CALLCHI>(*pi));
    }
}

/*
 * Connect def-use chains for indirect value-flow, (value-flow of address-taken variables)
 */
void SVFG::connectIndirectSVFGEdges() {

    for(iterator it = begin(), eit = end(); it!=eit; ++it) {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;
        if(const LoadSVFGNode* loadNode = SVFUtil::dyn_cast<LoadSVFGNode>(node)) {
            MUSet& muSet = mssa->getMUSet(SVFUtil::cast<LoadPE>(loadNode->getPAGEdge()));
            for(MUSet::iterator it = muSet.begin(), eit = muSet.end(); it!=eit; ++it) {
                if(LOADMU* mu = SVFUtil::dyn_cast<LOADMU>(*it)) {
                    NodeID def = getDef(mu->getVer());
                    addIntraIndirectVFEdge(def,nodeId, mu->getVer()->getMR()->getPointsTo());
                }
            }
        }
        else if(const StoreSVFGNode* storeNode = SVFUtil::dyn_cast<StoreSVFGNode>(node)) {
            CHISet& chiSet = mssa->getCHISet(SVFUtil::cast<StorePE>(storeNode->getPAGEdge()));
            for(CHISet::iterator it = chiSet.begin(), eit = chiSet.end(); it!=eit; ++it) {
                if(STORECHI* chi = SVFUtil::dyn_cast<STORECHI>(*it)) {
                    NodeID def = getDef(chi->getOpVer());
                    addIntraIndirectVFEdge(def,nodeId, chi->getOpVer()->getMR()->getPointsTo());
                }
            }
        }
        else if(const FormalINSVFGNode* formalIn = SVFUtil::dyn_cast<FormalINSVFGNode>(node)) {
            PTACallGraphEdge::CallInstSet callInstSet;
            mssa->getPTA()->getPTACallGraph()->getDirCallSitesInvokingCallee(formalIn->getEntryChi()->getFunction(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it) {
                CallSite cs = SVFUtil::getLLVMCallSite(*it);
                if(!mssa->hasMU(cs))
                    continue;
                ActualINSVFGNodeSet& actualIns = getActualINSVFGNodes(cs);
                for(ActualINSVFGNodeSet::iterator ait = actualIns.begin(), aeit = actualIns.end(); ait!=aeit; ++ait) {
                    const ActualINSVFGNode* actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ait));
                    addInterIndirectVFCallEdge(actualIn,formalIn,getCallSiteID(cs, formalIn->getFun()));
                }
            }
        }
        else if(const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node)) {
            PTACallGraphEdge::CallInstSet callInstSet;
            const MemSSA::RETMU* retMu = formalOut->getRetMU();
            mssa->getPTA()->getPTACallGraph()->getDirCallSitesInvokingCallee(retMu->getFunction(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it) {
                CallSite cs = SVFUtil::getLLVMCallSite(*it);
                if(!mssa->hasCHI(cs))
                    continue;
                ActualOUTSVFGNodeSet& actualOuts = getActualOUTSVFGNodes(cs);
                for(ActualOUTSVFGNodeSet::iterator ait = actualOuts.begin(), aeit = actualOuts.end(); ait!=aeit; ++ait) {
                    const ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ait));
                    addInterIndirectVFRetEdge(formalOut,actualOut,getCallSiteID(cs, formalOut->getFun()));
                }
            }
            NodeID def = getDef(retMu->getVer());
            addIntraIndirectVFEdge(def,nodeId, retMu->getVer()->getMR()->getPointsTo());
        }
        else if(const ActualINSVFGNode* actualIn = SVFUtil::dyn_cast<ActualINSVFGNode>(node)) {
            const MRVer* ver = actualIn->getCallMU()->getVer();
            NodeID def = getDef(ver);
            addIntraIndirectVFEdge(def,nodeId, ver->getMR()->getPointsTo());
        }
        else if(SVFUtil::isa<ActualOUTSVFGNode>(node)) {
            /// There's no need to connect actual out node to its definition site in the same function.
        }
        else if(const MSSAPHISVFGNode* phiNode = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node)) {
            for (MemSSA::PHI::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++) {
                const MRVer* op = it->second;
                NodeID def = getDef(op);
                addIntraIndirectVFEdge(def,nodeId, op->getMR()->getPointsTo());
            }
        }
    }


    connectFromGlobalToProgEntry();
}


/*!
 * Connect indirect SVFG edges from global initializers (store) to main function entry
 */
void SVFG::connectFromGlobalToProgEntry()
{
    SVFModule svfModule = mssa->getPTA()->getModule();
    const Function* mainFunc =
        SVFUtil::getProgEntryFunction(svfModule);
    FormalINSVFGNodeSet& formalIns = getFormalINSVFGNodes(mainFunc);
    if (formalIns.empty())
        return;

    for (GlobalVFGNodeSet::const_iterator storeIt = globalVFGNodes.begin(), storeEit = globalVFGNodes.end();
            storeIt != storeEit; ++storeIt) {
        if (const StoreSVFGNode* store = SVFUtil::dyn_cast<StoreSVFGNode>(*storeIt)) {

            /// connect this store to main function entry
            const PointsTo& storePts = mssa->getPTA()->getPts(
                    store->getPAGDstNodeID());

            for (NodeBS::iterator fiIt = formalIns.begin(), fiEit =
                    formalIns.end(); fiIt != fiEit; ++fiIt) {
                NodeID formalInID = *fiIt;
                PointsTo formalInPts = ((FormalINSVFGNode*) getSVFGNode(formalInID))->getPointsTo();

                formalInPts &= storePts;
                if (formalInPts.empty())
                    continue;

                /// add indirect value flow edge
                addIntraIndirectVFEdge(store->getId(), formalInID, formalInPts);
            }
        }
    }
}

/*
 *  Add def-use edges of a memory region between two statements
 */
SVFGEdge* SVFG::addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    checkIntraEdgeParents(srcNode, dstNode);
    if(SVFGEdge* edge = hasIntraVFGEdge(srcNode,dstNode,SVFGEdge::IntraIndirectVF)) {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        IntraIndSVFGEdge* indirectEdge = new IntraIndSVFGEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(cpts);
        return (addSVFGEdge(indirectEdge) ? indirectEdge : NULL);
    }
}


/*!
 * Add def-use edges of a memory region between two may-happen-in-parallel statements for multithreaded program
 */
SVFGEdge* SVFG::addThreadMHPIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts) {
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasThreadVFGEdge(srcNode,dstNode,SVFGEdge::TheadMHPIndirectVF)) {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        ThreadMHPIndSVFGEdge* indirectEdge = new ThreadMHPIndSVFGEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(cpts);
        return (addSVFGEdge(indirectEdge) ? indirectEdge : NULL);
    }
}

/*
 *  Add def-use call edges of a memory region between two statements
 */
SVFGEdge* SVFG::addCallIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasInterVFGEdge(srcNode,dstNode,SVFGEdge::CallIndVF,csId)) {
        assert(SVFUtil::isa<CallIndSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<CallIndSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        CallIndSVFGEdge* callEdge = new CallIndSVFGEdge(srcNode,dstNode,csId);
        callEdge->addPointsTo(cpts);
        return (addSVFGEdge(callEdge) ? callEdge : NULL);
    }
}

/*
 *  Add def-use return edges of a memory region between two statements
 */
SVFGEdge* SVFG::addRetIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasInterVFGEdge(srcNode,dstNode,SVFGEdge::RetIndVF,csId)) {
        assert(SVFUtil::isa<RetIndSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (SVFUtil::cast<RetIndSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        RetIndSVFGEdge* retEdge = new RetIndSVFGEdge(srcNode,dstNode,csId);
        retEdge->addPointsTo(cpts);
        return (addSVFGEdge(retEdge) ? retEdge : NULL);
    }
}

/*!
 *
 */
SVFGEdge* SVFG::addInterIndirectVFCallEdge(const ActualINSVFGNode* src, const FormalINSVFGNode* dst,CallSiteID csId) {
    PointsTo cpts1 = src->getPointsTo();
    PointsTo cpts2 = dst->getPointsTo();
    if(cpts1.intersects(cpts2)) {
        cpts1 &= cpts2;
        return addCallIndirectVFEdge(src->getId(),dst->getId(),cpts1,csId);
    }
    return NULL;
}

/*!
 * Add inter VF edge from function exit mu to callsite chi
 */
SVFGEdge* SVFG::addInterIndirectVFRetEdge(const FormalOUTSVFGNode* src, const ActualOUTSVFGNode* dst,CallSiteID csId) {

    PointsTo cpts1 = src->getPointsTo();
    PointsTo cpts2 = dst->getPointsTo();
    if(cpts1.intersects(cpts2)) {
        cpts1 &= cpts2;
        return addRetIndirectVFEdge(src->getId(),dst->getId(),cpts1,csId);
    }
    return NULL;
}

/*!
 * Dump SVFG
 */
void SVFG::dump(const std::string& file, bool simple) {
    if(DumpVFG)
        GraphPrinter::WriteGraphToFile(outs(), file, this, simple);
}

/**
 * Get all inter value flow edges at this indirect call site, including call and return edges.
 */
void SVFG::getInterVFEdgesForIndirectCallSite(const CallSite cs, const Function* callee, SVFGEdgeSetTy& edges)
{
    CallSiteID csId = getCallSiteID(cs, callee);
    // Find inter direct call edges between actual param and formal param.
    if (pag->hasCallSiteArgsMap(cs) && pag->hasFunArgsMap(callee)) {
        const PAG::PAGNodeList& csArgList = pag->getCallSiteArgsList(cs);
        const PAG::PAGNodeList& funArgList = pag->getFunArgsList(callee);
        PAG::PAGNodeList::const_iterator csArgIt = csArgList.begin(), csArgEit = csArgList.end();
        PAG::PAGNodeList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        for (; funArgIt != funArgEit && csArgIt != csArgEit; funArgIt++, csArgIt++) {
            const PAGNode *cs_arg = *csArgIt;
            const PAGNode *fun_arg = *funArgIt;
            if (fun_arg->isPointer() && cs_arg->isPointer())
                getInterVFEdgeAtIndCSFromAPToFP(cs_arg, fun_arg, cs, csId, edges);
        }
        assert(funArgIt == funArgEit && "function has more arguments than call site");
        if (callee->isVarArg()) {
            NodeID varFunArg = pag->getVarargNode(callee);
            const PAGNode* varFunArgNode = pag->getPAGNode(varFunArg);
            if (varFunArgNode->isPointer()) {
                for (; csArgIt != csArgEit; csArgIt++) {
                    const PAGNode *cs_arg = *csArgIt;
                    if (cs_arg->isPointer())
                        getInterVFEdgeAtIndCSFromAPToFP(cs_arg, varFunArgNode, cs, csId, edges);
                }
            }
        }
    }

    // Find inter direct return edges between actual return and formal return.
    if (pag->funHasRet(callee) && pag->callsiteHasRet(cs)) {
        const PAGNode* cs_return = pag->getCallSiteRet(cs);
        const PAGNode* fun_return = pag->getFunRet(callee);
        if (cs_return->isPointer() && fun_return->isPointer())
            getInterVFEdgeAtIndCSFromFRToAR(fun_return, cs_return, csId, edges);
    }

    // Find inter indirect call edges between actual-in and formal-in svfg nodes.
    if (hasFuncEntryChi(callee) && hasCallSiteMu(cs)) {
        SVFG::ActualINSVFGNodeSet& actualInNodes = getActualINSVFGNodes(cs);
        for(SVFG::ActualINSVFGNodeSet::iterator ai_it = actualInNodes.begin(),
                ai_eit = actualInNodes.end(); ai_it!=ai_eit; ++ai_it) {
            ActualINSVFGNode * actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ai_it));
            getInterVFEdgeAtIndCSFromAInToFIn(actualIn, callee, edges);
        }
    }

    // Find inter indirect return edges between actual-out and formal-out svfg nodes.
    if (hasFuncRetMu(callee) && hasCallSiteChi(cs)) {
        SVFG::ActualOUTSVFGNodeSet& actualOutNodes = getActualOUTSVFGNodes(cs);
        for(SVFG::ActualOUTSVFGNodeSet::iterator ao_it = actualOutNodes.begin(),
                ao_eit = actualOutNodes.end(); ao_it!=ao_eit; ++ao_it) {
            ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ao_it));
            getInterVFEdgeAtIndCSFromFOutToAOut(actualOut, callee, edges);
        }
    }
}

/**
 * Connect actual params/return to formal params/return for top-level variables.
 * Also connect indirect actual in/out and formal in/out.
 */
void SVFG::connectCallerAndCallee(CallSite cs, const Function* callee, SVFGEdgeSetTy& edges)
{
    VFG::connectCallerAndCallee(cs,callee,edges);

    CallSiteID csId = getCallSiteID(cs, callee);

    // connect actual in and formal in
    if (hasFuncEntryChi(callee) && hasCallSiteMu(cs)) {
        SVFG::ActualINSVFGNodeSet& actualInNodes = getActualINSVFGNodes(cs);
        const SVFG::FormalINSVFGNodeSet& formalInNodes = getFormalINSVFGNodes(callee);
        for(SVFG::ActualINSVFGNodeSet::iterator ai_it = actualInNodes.begin(),
                ai_eit = actualInNodes.end(); ai_it!=ai_eit; ++ai_it) {
            const ActualINSVFGNode * actualIn = SVFUtil::cast<ActualINSVFGNode>(getSVFGNode(*ai_it));
            for(SVFG::FormalINSVFGNodeSet::iterator fi_it = formalInNodes.begin(),
                    fi_eit = formalInNodes.end(); fi_it!=fi_eit; ++fi_it) {
                const FormalINSVFGNode* formalIn = SVFUtil::cast<FormalINSVFGNode>(getSVFGNode(*fi_it));
                connectAInAndFIn(actualIn, formalIn, csId, edges);
            }
        }
    }

    // connect actual out and formal out
    if (hasFuncRetMu(callee) && hasCallSiteChi(cs)) {
        // connect formal out and actual out
        const SVFG::FormalOUTSVFGNodeSet& formalOutNodes = getFormalOUTSVFGNodes(callee);
        SVFG::ActualOUTSVFGNodeSet& actualOutNodes = getActualOUTSVFGNodes(cs);
        for(SVFG::FormalOUTSVFGNodeSet::iterator fo_it = formalOutNodes.begin(),
                fo_eit = formalOutNodes.end(); fo_it!=fo_eit; ++fo_it) {
            const FormalOUTSVFGNode * formalOut = SVFUtil::cast<FormalOUTSVFGNode>(getSVFGNode(*fo_it));
            for(SVFG::ActualOUTSVFGNodeSet::iterator ao_it = actualOutNodes.begin(),
                    ao_eit = actualOutNodes.end(); ao_it!=ao_eit; ++ao_it) {
                const ActualOUTSVFGNode* actualOut = SVFUtil::cast<ActualOUTSVFGNode>(getSVFGNode(*ao_it));
                connectFOutAndAOut(formalOut, actualOut, csId, edges);
            }
        }
    }
}


/*!
 * Whether this is an function entry SVFGNode (formal parameter, formal In)
 */
const Function* SVFG::isFunEntrySVFGNode(const SVFGNode* node) const {
    if(const FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node)) {
        return fp->getFun();
    }
    else if(const InterPHISVFGNode* phi = SVFUtil::dyn_cast<InterPHISVFGNode>(node)) {
        if(phi->isFormalParmPHI())
            return phi->getFun();
    }
    else if(const FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node)) {
        return fi->getFun();
    }
    else if(const InterMSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<InterMSSAPHISVFGNode>(node)) {
        if(mphi->isFormalINPHI())
            return phi->getFun();
    }
    return NULL;
}

/*!
 * Whether this is an callsite return SVFGNode (actual return, actual out)
 */
Instruction* SVFG::isCallSiteRetSVFGNode(const SVFGNode* node) const {
    if(const ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node)) {
        return ar->getCallSite().getInstruction();
    }
    else if(const InterPHISVFGNode* phi = SVFUtil::dyn_cast<InterPHISVFGNode>(node)) {
        if(phi->isActualRetPHI())
            return phi->getCallSite().getInstruction();
    }
    else if(const ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node)) {
        return ao->getCallSite().getInstruction();
    }
    else if(const InterMSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<InterMSSAPHISVFGNode>(node)) {
        if(mphi->isActualOUTPHI())
            return mphi->getCallSite().getInstruction();
    }
    return NULL;
}

/*!
 * Perform Statistics
 */
void SVFG::performStat()
{
    stat->performStat();
}

/*!
 * GraphTraits specialization
 */
namespace llvm {
template<>
struct DOTGraphTraits<SVFG*> : public DOTGraphTraits<PAG*> {

    typedef SVFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<PAG*>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(SVFG *graph) {
        return "SVFG";
    }

    std::string getNodeLabel(NodeType *node, SVFG *graph) {
        if (isSimple())
            return getSimpleNodeLabel(node, graph);
        else
            return getCompleteNodeLabel(node, graph);
    }

    /// Return label of a VFG node without MemSSA information
    static std::string getSimpleNodeLabel(NodeType *node, SVFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if(StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node)) {
            NodeID src = stmtNode->getPAGSrcNodeID();
            NodeID dst = stmtNode->getPAGDstNodeID();
            rawstr << dst << "<--" << src << "\n";
            if(stmtNode->getInst()) {
                rawstr << getSourceLoc(stmtNode->getInst());
            }
            else if(stmtNode->getPAGDstNode()->hasValue()) {
                rawstr << getSourceLoc(stmtNode->getPAGDstNode()->getValue());
            }
        }
        else if(PHISVFGNode* tphi = SVFUtil::dyn_cast<PHISVFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = PHI(";
            for(PHISVFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                    it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node)) {
            rawstr << "FPARM(" << fp->getParam()->getId() << ")\n";
            rawstr << "Fun[" << fp->getFun()->getName() << "]";
        }
        else if(ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(node)) {
            rawstr << "APARM(" << ap->getParam()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ap->getCallSite().getInstruction()) << "]";
        }
        else if (ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node)) {
            rawstr << "ARet(" << ar->getRev()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ar->getCallSite().getInstruction()) << "]";
        }
        else if (FormalRetSVFGNode* fr = SVFUtil::dyn_cast<FormalRetSVFGNode>(node)) {
            rawstr << "FRet(" << fr->getRet()->getId() << ")\n";
            rawstr << "Fun[" << fr->getFun()->getName() << "]";
        }
        else if(FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node)) {
            rawstr << "ENCHI\n";
            rawstr << "Fun[" << fi->getFun()->getName() << "]";
        }
        else if(FormalOUTSVFGNode* fo = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node)) {
            rawstr << "RETMU\n";
            rawstr << "Fun[" << fo->getFun()->getName() << "]";
        }
        else if(ActualINSVFGNode* ai = SVFUtil::dyn_cast<ActualINSVFGNode>(node)) {
            rawstr << "CSMU\n";
            rawstr << "CS[" << getSourceLoc(ai->getCallSite().getInstruction())  << "]";
        }
        else if(ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node)) {
            rawstr << "CSCHI\n";
            rawstr << "CS[" << getSourceLoc(ao->getCallSite().getInstruction())  << "]";
        }
        else if(MSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node)) {
            rawstr << "MSSAPHI\n";
            rawstr << getSourceLoc(&mphi->getBB()->back());
        }
        else if(SVFUtil::isa<NullPtrSVFGNode>(node)) {
            rawstr << "NullPtr";
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    /// Return label of a VFG node with MemSSA information
    static std::string getCompleteNodeLabel(NodeType *node, SVFG *graph) {

        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if(StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node)) {
            NodeID src = stmtNode->getPAGSrcNodeID();
            NodeID dst = stmtNode->getPAGDstNodeID();
            rawstr << dst << "<--" << src << "\n";
            if(stmtNode->getInst()) {
                rawstr << getSourceLoc(stmtNode->getInst());
            }
            else if(stmtNode->getPAGDstNode()->hasValue()) {
                rawstr << getSourceLoc(stmtNode->getPAGDstNode()->getValue());
            }
        }
        else if(BinaryOPVFGNode* tphi = SVFUtil::dyn_cast<BinaryOPVFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = Binary(";
            for(BinaryOPVFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(CmpVFGNode* tphi = SVFUtil::dyn_cast<CmpVFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = cmp(";
            for(CmpVFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(MSSAPHISVFGNode* mphi = SVFUtil::dyn_cast<MSSAPHISVFGNode>(node)) {
            rawstr << "MR_" << mphi->getRes()->getMR()->getMRID()
                   << "V_" << mphi->getRes()->getResVer()->getSSAVersion() << " = PHI(";
            for (MemSSA::PHI::OPVers::const_iterator it = mphi->opVerBegin(), eit = mphi->opVerEnd();
                    it != eit; it++)
                rawstr << "MR_" << it->second->getMR()->getMRID() << "V_" << it->second->getSSAVersion() << ", ";
            rawstr << ")\n";

            rawstr << mphi->getRes()->getMR()->dumpStr() << "\n";
            rawstr << getSourceLoc(&mphi->getBB()->back());
        }
        else if(PHISVFGNode* tphi = SVFUtil::dyn_cast<PHISVFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = PHI(";
            for(PHISVFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                    it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(FormalINSVFGNode* fi = SVFUtil::dyn_cast<FormalINSVFGNode>(node)) {
            rawstr	<< fi->getEntryChi()->getMR()->getMRID() << "V_" << fi->getEntryChi()->getResVer()->getSSAVersion() <<
                    " = ENCHI(MR_" << fi->getEntryChi()->getMR()->getMRID() << "V_" << fi->getEntryChi()->getOpVer()->getSSAVersion() << ")\n";
            rawstr << fi->getEntryChi()->getMR()->dumpStr() << "\n";
            rawstr << "Fun[" << fi->getFun()->getName() << "]";
        }
        else if(FormalOUTSVFGNode* fo = SVFUtil::dyn_cast<FormalOUTSVFGNode>(node)) {
            rawstr << "RETMU(" << fo->getRetMU()->getMR()->getMRID() << "V_" << fo->getRetMU()->getVer()->getSSAVersion() << ")\n";
            rawstr  << fo->getRetMU()->getMR()->dumpStr() << "\n";
            rawstr << "Fun[" << fo->getFun()->getName() << "]";
        }
        else if(FormalParmSVFGNode* fp = SVFUtil::dyn_cast<FormalParmSVFGNode>(node)) {
            rawstr	<< "FPARM(" << fp->getParam()->getId() << ")\n";
            rawstr << "Fun[" << fp->getFun()->getName() << "]";
        }
        else if(ActualINSVFGNode* ai = SVFUtil::dyn_cast<ActualINSVFGNode>(node)) {
            rawstr << "CSMU(" << ai->getCallMU()->getMR()->getMRID() << "V_" << ai->getCallMU()->getVer()->getSSAVersion() << ")\n";
            rawstr << ai->getCallMU()->getMR()->dumpStr() << "\n";
            rawstr << "CS[" << getSourceLoc(ai->getCallSite().getInstruction()) << "]";
        }
        else if(ActualOUTSVFGNode* ao = SVFUtil::dyn_cast<ActualOUTSVFGNode>(node)) {
            rawstr <<  ao->getCallCHI()->getMR()->getMRID() << "V_" << ao->getCallCHI()->getResVer()->getSSAVersion() <<
                   " = CSCHI(MR_" << ao->getCallCHI()->getMR()->getMRID() << "V_" << ao->getCallCHI()->getOpVer()->getSSAVersion() << ")\n";
            rawstr << ao->getCallCHI()->getMR()->dumpStr() << "\n";
            rawstr << "CS[" << getSourceLoc(ao->getCallSite().getInstruction()) << "]" ;
        }
        else if(ActualParmSVFGNode* ap = SVFUtil::dyn_cast<ActualParmSVFGNode>(node)) {
            rawstr << "APARM(" << ap->getParam()->getId() << ")\n" ;
            rawstr << "CS[" << getSourceLoc(ap->getCallSite().getInstruction()) << "]";
        }
        else if(SVFUtil::isa<NullPtrSVFGNode>(node)) {
            rawstr << "NullPtr";
        }
        else if (ActualRetSVFGNode* ar = SVFUtil::dyn_cast<ActualRetSVFGNode>(node)) {
            rawstr << "ARet(" << ar->getRev()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ar->getCallSite().getInstruction()) << "]";
        }
        else if (FormalRetSVFGNode* fr = SVFUtil::dyn_cast<FormalRetSVFGNode>(node)) {
            rawstr << "FRet(" << fr->getRet()->getId() << ")\n";
            rawstr << "Fun[" << fr->getFun()->getName() << "]";
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, SVFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);

        if(StmtSVFGNode* stmtNode = SVFUtil::dyn_cast<StmtSVFGNode>(node)) {
            const PAGEdge* edge = stmtNode->getPAGEdge();
            if (SVFUtil::isa<AddrPE>(edge)) {
                rawstr <<  "color=green";
            } else if (SVFUtil::isa<CopyPE>(edge)) {
                rawstr <<  "color=black";
            } else if (SVFUtil::isa<RetPE>(edge)) {
                rawstr <<  "color=black,style=dotted";
            } else if (SVFUtil::isa<GepPE>(edge)) {
                rawstr <<  "color=purple";
            } else if (SVFUtil::isa<StorePE>(edge)) {
                rawstr <<  "color=blue";
            } else if (SVFUtil::isa<LoadPE>(edge)) {
                rawstr <<  "color=red";
            } else {
                assert(0 && "No such kind edge!!");
            }
            rawstr <<  "";
        }
        else if(SVFUtil::isa<MSSAPHISVFGNode>(node)) {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<PHISVFGNode>(node)) {
            rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<NullPtrSVFGNode>(node)) {
            rawstr <<  "color=grey";
        }
        else if(SVFUtil::isa<FormalINSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(SVFUtil::isa<FormalOUTSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(SVFUtil::isa<FormalParmSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(SVFUtil::isa<ActualINSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(SVFUtil::isa<ActualOUTSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(SVFUtil::isa<ActualParmSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (SVFUtil::isa<ActualRetSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (SVFUtil::isa<FormalRetSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (SVFUtil::isa<BinaryOPVFGNode>(node)) {
            rawstr <<  "color=black,style=double";
        }
        else if (SVFUtil::isa<CmpVFGNode>(node)) {
            rawstr <<  "color=black,style=double";
        }
        else
            assert(false && "no such kind of node!!");

        /// dump slice information
        if(graph->getStat()->isSource(node)) {
            rawstr << ",style=filled, fillcolor=red";
        }
        else if(graph->getStat()->isSink(node)) {
            rawstr << ",style=filled, fillcolor=blue";
        }
        else if(graph->getStat()->inBackwardSlice(node)) {
            rawstr << ",style=filled, fillcolor=yellow";
        }
        else if(graph->getStat()->inForwardSlice(node))
            rawstr << ",style=filled, fillcolor=gray";

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, SVFG *pag) {
        SVFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (SVFUtil::isa<DirectSVFGEdge>(edge)) {
            if (SVFUtil::isa<CallDirSVFGEdge>(edge))
                return "style=solid,color=red";
            else if (SVFUtil::isa<RetDirSVFGEdge>(edge))
                return "style=solid,color=blue";
            else
                return "style=solid";
        } else if (SVFUtil::isa<IndirectSVFGEdge>(edge)) {
            if (SVFUtil::isa<CallIndSVFGEdge>(edge))
                return "style=dashed,color=red";
            else if (SVFUtil::isa<RetIndSVFGEdge>(edge))
                return "style=dashed,color=blue";
            else
                return "style=dashed";
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
        SVFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallDirSVFGEdge* dirCall = SVFUtil::dyn_cast<CallDirSVFGEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetDirSVFGEdge* dirRet = SVFUtil::dyn_cast<RetDirSVFGEdge>(edge))
            rawstr << dirRet->getCallSiteId();
        else if (CallIndSVFGEdge* indCall = SVFUtil::dyn_cast<CallIndSVFGEdge>(edge))
            rawstr << indCall->getCallSiteId();
        else if (RetIndSVFGEdge* indRet = SVFUtil::dyn_cast<RetIndSVFGEdge>(edge))
            rawstr << indRet->getCallSiteId();

        return rawstr.str();
    }
};
}
