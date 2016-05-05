//===- SVFG.cpp -- Sparse value-flow graph-----------------------------------//
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
 * SVFG.cpp
 *
 *  Created on: Oct 28, 2013
 *      Author: Yulei Sui
 */

#include "MSSA/SVFG.h"
#include "MSSA/SVFGOPT.h"
#include "MSSA/SVFGStat.h"
#include "Util/GraphUtil.h"
#include "Util/AnalysisUtil.h"

using namespace llvm;
using namespace analysisUtil;


static cl::opt<bool> DumpVFG("dump-svfg", cl::init(false),
                             cl::desc("Dump dot graph of SVFG"));

/*!
 * Constructor
 */
SVFG::SVFG(PTACallGraph* cg, SVFGK k): totalSVFGNode(0), kind(k),mssa(NULL), ptaCallGraph(cg)  {
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
void SVFG::buildSVFG(MemSSA* m) {
    mssa = m;

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Top Level Node\n"));

    stat->TLVFNodeStart();
    addSVFGNodesForTopLevelPtrs();
    stat->TLVFNodeEnd();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Addr-taken Node\n"));

    stat->ATVFNodeStart();
    addSVFGNodesForAddrTakenVars();
    stat->ATVFNodeEnd();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Direct Edge\n"));

    stat->dirVFEdgeStart();
    connectDirectSVFGEdges();
    stat->dirVFEdgeEnd();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate SVFG Indirect Edge\n"));

    stat->indVFEdgeStart();
    connectIndirectSVFGEdges();
    stat->indVFEdgeEnd();

}


/*!
 * Create SVFG nodes for top level pointers
 */
void SVFG::addSVFGNodesForTopLevelPtrs() {

    PAG* pag = mssa->getPAG();
    // initialize dummy definition  null pointers in order to uniform the construction
    // to be noted for black hole pointer it has already has address edge connected,
    // and its definition will be set when processing addr PAG edge.
    addNullPtrSVFGNode(pag->getPAGNode(pag->getNullPtr()));

    // initialize address nodes
    PAGEdge::PAGEdgeSetTy& addrs = pag->getEdgeSet(PAGEdge::Addr);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter) {
        addAddrSVFGNode(cast<AddrPE>(*iter));
    }

    // initialize copy nodes
    PAGEdge::PAGEdgeSetTy& copys = pag->getEdgeSet(PAGEdge::Copy);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter) {
        CopyPE* copy = cast<CopyPE>(*iter);
        if(!isPhiCopyEdge(copy))
            addCopySVFGNode(copy);
    }

    // initialize gep nodes
    PAGEdge::PAGEdgeSetTy& ngeps = pag->getEdgeSet(PAGEdge::NormalGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter) {
        addGepSVFGNode(cast<NormalGepPE>(*iter));
    }

    PAGEdge::PAGEdgeSetTy& vgeps = pag->getEdgeSet(PAGEdge::VariantGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = vgeps.begin(), eiter =
                vgeps.end(); iter != eiter; ++iter) {
        addGepSVFGNode(cast<VariantGepPE>(*iter));
    }

    // initialize load nodes
    PAGEdge::PAGEdgeSetTy& loads = pag->getEdgeSet(PAGEdge::Load);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter) {
        addLoadSVFGNode(cast<LoadPE>(*iter));
    }

    // initialize store nodes
    PAGEdge::PAGEdgeSetTy& stores = pag->getEdgeSet(PAGEdge::Store);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter) {
        addStoreSVFGNode(cast<StorePE>(*iter));
    }

    // initialize actual parameter nodes
    for(PAG::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(), eit = pag->getCallSiteArgsMap().end(); it !=eit; ++it) {
        const Function* fun = getCallee(it->first);
        /// for external function we do not create acutalParm SVFGNode
        /// because we do not have a formal parameter to connect this actualParm
        if(isExtCall(fun))
            continue;

        for(PAG::PAGNodeList::iterator pit = it->second.begin(), epit = it->second.end(); pit!=epit; ++pit) {
            const PAGNode* pagNode = *pit;
            if (pagNode->isPointer())
                addActualParmSVFGNode(pagNode,it->first);
        }
    }

    // initialize actual return nodes (callsite return)
    for(PAG::CSToRetMap::iterator it = pag->getCallSiteRets().begin(), eit = pag->getCallSiteRets().end(); it !=eit; ++it) {
        /// for external function we do not create acutalRet SVFGNode
        /// they are in the formal of AddrSVFGNode if the external function returns an allocated memory
        /// if fun has body, it may also exist in isExtCall, e.g., xmalloc() in bzip2, spec2000.
        if(it->second->isPointer() == false || hasDef(it->second))
            continue;

        addActualRetSVFGNode(it->second,it->first);
    }

    // initialize formal parameter nodes
    for(PAG::FunToArgsListMap::iterator it = pag->getFunArgsMap().begin(), eit = pag->getFunArgsMap().end(); it !=eit; ++it) {
        const llvm::Function* func = it->first;
        for(PAG::PAGNodeList::iterator pit = it->second.begin(), epit = it->second.end(); pit!=epit; ++pit) {
            const PAGNode* param = *pit;
            if (param->isPointer() == false || hasBlackHoleConstObjAddrAsDef(param))
                continue;

            CallPESet callPEs;
            if(param->hasIncomingEdges(PAGEdge::Call)) {
                for(PAGEdge::PAGEdgeSetTy::const_iterator cit = param->getIncomingEdgesBegin(PAGEdge::Call),
                        ecit = param->getIncomingEdgesEnd(PAGEdge::Call); cit!=ecit; ++cit) {
                    callPEs.insert(cast<CallPE>(*cit));
                }
            }
            addFormalParmSVFGNode(param,func,callPEs);
        }

        if (func->getFunctionType()->isVarArg()) {
            const PAGNode* varParam = pag->getPAGNode(pag->getVarargNode(func));
            if (varParam->isPointer() && hasBlackHoleConstObjAddrAsDef(varParam) == false) {
                CallPESet callPEs;
                if (varParam->hasIncomingEdges(PAGEdge::Call)) {
                    for(PAGEdge::PAGEdgeSetTy::const_iterator cit = varParam->getIncomingEdgesBegin(PAGEdge::Call),
                            ecit = varParam->getIncomingEdgesEnd(PAGEdge::Call); cit!=ecit; ++cit) {
                        callPEs.insert(cast<CallPE>(*cit));
                    }
                }
                addFormalParmSVFGNode(varParam,func,callPEs);
            }
        }
    }

    // initialize formal return nodes (callee return)
    for(PAG::FunToRetMap::iterator it = pag->getFunRets().begin(), eit = pag->getFunRets().end(); it !=eit; ++it) {
        const PAGNode* retNode = it->second;
        if (retNode->isPointer() == false)
            continue;

        RetPESet retPEs;
        if(retNode->hasOutgoingEdges(PAGEdge::Ret)) {
            for(PAGEdge::PAGEdgeSetTy::const_iterator cit = retNode->getOutgoingEdgesBegin(PAGEdge::Ret),
                    ecit = retNode->getOutgoingEdgesEnd(PAGEdge::Ret); cit!=ecit; ++cit) {
                retPEs.insert(cast<RetPE>(*cit));
            }
        }
        addFormalRetSVFGNode(retNode,it->first,retPEs);
    }

    // initialize llvm phi nodes (phi of top level pointers)
    PAG::PHINodeMap& phiNodeMap = pag->getPhiNodeMap();
    for(PAG::PHINodeMap::iterator pit = phiNodeMap.begin(), epit = phiNodeMap.end(); pit!=epit; ++pit) {
        addIntraPHISVFGNode(pit->first,pit->second);
    }

}

/*
 * Create SVFG nodes for address-taken variables
 */
void SVFG::addSVFGNodesForAddrTakenVars() {

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
            addFormalINSVFGNode(cast<ENTRYCHI>(*pi));
    }
    /// initialize memory SSA return mu nodes
    for(MemSSA::FunToReturnMuSetMap::iterator it = mssa->getFunToRetMuSetMap().begin(),
            eit = mssa->getFunToRetMuSetMap().end(); it!=eit; ++it) {
        for(MUSet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addFormalOUTSVFGNode(cast<RETMU>(*pi));
    }
    /// initialize memory SSA callsite mu nodes
    for(MemSSA::CallSiteToMUSetMap::iterator it = mssa->getCallSiteToMuSetMap().begin(),
            eit = mssa->getCallSiteToMuSetMap().end();
            it!=eit; ++it) {
        for(MUSet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addActualINSVFGNode(cast<CALLMU>(*pi));
    }
    /// initialize memory SSA callsite chi nodes
    for(MemSSA::CallSiteToCHISetMap::iterator it = mssa->getCallSiteToChiSetMap().begin(),
            eit = mssa->getCallSiteToChiSetMap().end();
            it!=eit; ++it) {
        for(CHISet::iterator pi = it->second.begin(), epi = it->second.end(); pi!=epi; ++pi)
            addActualOUTSVFGNode(cast<CALLCHI>(*pi));
    }
}

/*!
 * Connect def-use chains for direct value-flow, (value-flow of top level pointers)
 */
void SVFG::connectDirectSVFGEdges() {

    for(iterator it = begin(), eit = end(); it!=eit; ++it) {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;

        if(const StmtSVFGNode* stmtNode = dyn_cast<StmtSVFGNode>(node)) {
            /// do not handle AddrSVFG node, as it is already the source of a definition
            if(isa<AddrSVFGNode>(stmtNode))
                continue;
            /// for all other cases, like copy/gep/load/ret, connect the RHS pointer to its def
            addIntraDirectVFEdge(getDef(stmtNode->getPAGSrcNode()),nodeId);

            /// for store, connect the RHS/LHS pointer to its def
            if(isa<StoreSVFGNode>(stmtNode)) {
                addIntraDirectVFEdge(getDef(stmtNode->getPAGDstNode()),nodeId);
            }

        }
        else if(const PHISVFGNode* phiNode = dyn_cast<PHISVFGNode>(node)) {
            for (PHISVFGNode::OPVers::const_iterator it = phiNode->opVerBegin(), eit = phiNode->opVerEnd();
                    it != eit; it++) {
                addIntraDirectVFEdge(getDef(it->second),nodeId);
            }
        }
        else if(const ActualParmSVFGNode* actualParm = dyn_cast<ActualParmSVFGNode>(node)) {
            addIntraDirectVFEdge(getDef(actualParm->getParam()),nodeId);
        }
        else if(const FormalParmSVFGNode* formalParm = dyn_cast<FormalParmSVFGNode>(node)) {
            for(CallPESet::const_iterator it = formalParm->callPEBegin(), eit = formalParm->callPEEnd();
                    it!=eit; ++it) {
                const Instruction* callInst = (*it)->getCallInst();
                CallSite cs = analysisUtil::getLLVMCallSite(callInst);
                const ActualParmSVFGNode* acutalParm = getActualParmSVFGNode((*it)->getSrcNode(),cs);
                addInterVFEdgeFromAPToFP(acutalParm,formalParm,getCallSiteID((*it)->getCallSite(), formalParm->getFun()));
            }
        }
        else if(const FormalRetSVFGNode* calleeRet = dyn_cast<FormalRetSVFGNode>(node)) {
            /// connect formal ret to its definition node
            addIntraDirectVFEdge(getDef(calleeRet->getRet()), nodeId);

            /// connect formal ret to actual ret
            for(RetPESet::const_iterator it = calleeRet->retPEBegin(), eit = calleeRet->retPEEnd();
                    it!=eit; ++it) {
                const ActualRetSVFGNode* callsiteRev = getActualRetSVFGNode((*it)->getDstNode());
                addInterVFEdgeFromFRToAR(calleeRet,callsiteRev, getCallSiteID((*it)->getCallSite(), calleeRet->getFun()));
            }
        }
        /// Do not process FormalRetSVFGNode, as they are connected by copy within callee
        /// We assume one procedure only has unique return
    }

    /// connect direct value-flow edges (parameter passing) for thread fork/join
    /// add fork edge
    PAGEdge::PAGEdgeSetTy& forks = getPAG()->getEdgeSet(PAGEdge::ThreadFork);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = forks.begin(), eiter =
                forks.end(); iter != eiter; ++iter) {
        TDForkPE* forkedge = cast<TDForkPE>(*iter);
        addActualParmSVFGNode(forkedge->getSrcNode(),forkedge->getCallSite());
        const ActualParmSVFGNode* acutalParm = getActualParmSVFGNode(forkedge->getSrcNode(),forkedge->getCallSite());
        const FormalParmSVFGNode* formalParm = getFormalParmSVFGNode(forkedge->getDstNode());
        addInterVFEdgeFromAPToFP(acutalParm,formalParm,getCallSiteID(forkedge->getCallSite(), formalParm->getFun()));
    }
    /// add join edge
    PAGEdge::PAGEdgeSetTy& joins = getPAG()->getEdgeSet(PAGEdge::ThreadJoin);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = joins.begin(), eiter =
                joins.end(); iter != eiter; ++iter) {
        TDJoinPE* joinedge = cast<TDJoinPE>(*iter);
        NodeID callsiteRev = getDef(joinedge->getDstNode());
        const FormalRetSVFGNode* calleeRet = getFormalRetSVFGNode(joinedge->getSrcNode());
        addRetDirectVFEdge(calleeRet->getId(),callsiteRev, getCallSiteID(joinedge->getCallSite(), calleeRet->getFun()));
    }
}

/*
 * Connect def-use chains for indirect value-flow, (value-flow of address-taken variables)
 */
void SVFG::connectIndirectSVFGEdges() {

    for(iterator it = begin(), eit = end(); it!=eit; ++it) {
        NodeID nodeId = it->first;
        const SVFGNode* node = it->second;
        if(const LoadSVFGNode* loadNode = dyn_cast<LoadSVFGNode>(node)) {
            MUSet& muSet = mssa->getMUSet(cast<LoadPE>(loadNode->getPAGEdge()));
            for(MUSet::iterator it = muSet.begin(), eit = muSet.end(); it!=eit; ++it) {
                if(LOADMU* mu = dyn_cast<LOADMU>(*it)) {
                    NodeID def = getDef(mu->getVer());
                    addIntraIndirectVFEdge(def,nodeId, mu->getVer()->getMR()->getPointsTo());
                }
            }
        }
        else if(const StoreSVFGNode* storeNode = dyn_cast<StoreSVFGNode>(node)) {
            CHISet& chiSet = mssa->getCHISet(cast<StorePE>(storeNode->getPAGEdge()));
            for(CHISet::iterator it = chiSet.begin(), eit = chiSet.end(); it!=eit; ++it) {
                if(STORECHI* chi = dyn_cast<STORECHI>(*it)) {
                    NodeID def = getDef(chi->getOpVer());
                    addIntraIndirectVFEdge(def,nodeId, chi->getOpVer()->getMR()->getPointsTo());
                }
            }
        }
        else if(const FormalINSVFGNode* formalIn = dyn_cast<FormalINSVFGNode>(node)) {
            PTACallGraphEdge::CallInstSet callInstSet;
            getPTACallGraph()->getDirCallSitesInvokingCallee(formalIn->getEntryChi()->getFunction(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it) {
                CallSite cs = analysisUtil::getLLVMCallSite(*it);
                if(!mssa->hasMU(cs))
                    continue;
                ActualINSVFGNodeSet& actualIns = getActualINSVFGNodes(cs);
                for(ActualINSVFGNodeSet::iterator ait = actualIns.begin(), aeit = actualIns.end(); ait!=aeit; ++ait) {
                    const ActualINSVFGNode* actualIn = llvm::cast<ActualINSVFGNode>(getSVFGNode(*ait));
                    addInterIndirectVFCallEdge(actualIn,formalIn,getCallSiteID(cs, formalIn->getFun()));
                }
            }
        }
        else if(const FormalOUTSVFGNode* formalOut = dyn_cast<FormalOUTSVFGNode>(node)) {
            PTACallGraphEdge::CallInstSet callInstSet;
            const MemSSA::RETMU* retMu = formalOut->getRetMU();
            getPTACallGraph()->getDirCallSitesInvokingCallee(retMu->getFunction(),callInstSet);
            for(PTACallGraphEdge::CallInstSet::iterator it = callInstSet.begin(), eit = callInstSet.end(); it!=eit; ++it) {
                CallSite cs = analysisUtil::getLLVMCallSite(*it);
                if(!mssa->hasCHI(cs))
                    continue;
                ActualOUTSVFGNodeSet& actualOuts = getActualOUTSVFGNodes(cs);
                for(ActualOUTSVFGNodeSet::iterator ait = actualOuts.begin(), aeit = actualOuts.end(); ait!=aeit; ++ait) {
                    const ActualOUTSVFGNode* actualOut = llvm::cast<ActualOUTSVFGNode>(getSVFGNode(*ait));
                    addInterIndirectVFRetEdge(formalOut,actualOut,getCallSiteID(cs, formalOut->getFun()));
                }
            }
            NodeID def = getDef(retMu->getVer());
            addIntraIndirectVFEdge(def,nodeId, retMu->getVer()->getMR()->getPointsTo());
        }
        else if(const ActualINSVFGNode* actualIn = dyn_cast<ActualINSVFGNode>(node)) {
            const MRVer* ver = actualIn->getCallMU()->getVer();
            NodeID def = getDef(ver);
            addIntraIndirectVFEdge(def,nodeId, ver->getMR()->getPointsTo());
        }
        else if(isa<ActualOUTSVFGNode>(node)) {
            /// There's no need to connect actual out node to its definition site in the same function.
        }
        else if(const MSSAPHISVFGNode* phiNode = dyn_cast<MSSAPHISVFGNode>(node)) {
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
    llvm::Module* mod = getPTACallGraph()->getModule();
    const llvm::Function* mainFunc = analysisUtil::getProgEntryFunction(mod);
    FormalINSVFGNodeSet& formalIns = getFormalINSVFGNodes(mainFunc);
    if (formalIns.empty())
        return;

    for (StoreNodeSet::const_iterator storeIt = globalStore.begin(), storeEit = globalStore.end();
            storeIt != storeEit; ++storeIt) {
        const StoreSVFGNode* store = *storeIt;

        /// connect this store to main function entry
        const PointsTo& storePts = mssa->getPTA()->getPts(store->getPAGDstNodeID());

        for (NodeBS::iterator fiIt = formalIns.begin(), fiEit = formalIns.end();
                fiIt != fiEit; ++fiIt) {
            NodeID formalInID = *fiIt;
            PointsTo formalInPts = ((FormalINSVFGNode*)getSVFGNode(formalInID))->getPointsTo();

            formalInPts &= storePts;
            if (formalInPts.empty())
                continue;

            /// add indirect value flow edge
            addIntraIndirectVFEdge(store->getId(), formalInID, formalInPts);
        }
    }
}


/*!
 * Whether we has an intra SVFG edge
 */
SVFGEdge* SVFG::hasIntraSVFGEdge(SVFGNode* src, SVFGNode* dst, SVFGEdge::SVFGEdgeK kind) {
    SVFGEdge edge(src,dst,kind);
    SVFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    SVFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}


/*!
 * Whether we has an thread SVFG edge
 */
SVFGEdge* SVFG::hasThreadSVFGEdge(SVFGNode* src, SVFGNode* dst, SVFGEdge::SVFGEdgeK kind) {
    SVFGEdge edge(src,dst,kind);
    SVFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    SVFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * Whether we has an inter SVFG edge
 */
SVFGEdge* SVFG::hasInterSVFGEdge(SVFGNode* src, SVFGNode* dst, SVFGEdge::SVFGEdgeK kind,CallSiteID csId) {
    SVFGEdge edge(src,dst,SVFGEdge::makeEdgeFlagWithInvokeID(kind,csId));
    SVFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    SVFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * Return the corresponding SVFGEdge
 */
SVFGEdge* SVFG::getSVFGEdge(const SVFGNode* src, const SVFGNode* dst, SVFGEdge::SVFGEdgeK kind) {

    SVFGEdge * edge = NULL;
    Size_t counter = 0;
    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter) {
        if ((*iter)->getDstID() == dst->getId() && (*iter)->getEdgeKind() == kind) {
            counter++;
            edge = (*iter);
        }
    }
    assert(counter <= 1 && "there's more than one edge between two SVFG nodes");
    return edge;

}

/*!
 * Add def-use edges for top level pointers
 */
SVFGEdge* SVFG::addIntraDirectVFEdge(NodeID srcId, NodeID dstId) {
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasIntraSVFGEdge(srcNode,dstNode, SVFGEdge::IntraDirect)) {
        assert(edge->isDirectVFGEdge() && "this should be a direct value flow edge!");
        return NULL;
    }
    else {
        IntraDirSVFGEdge* directEdge = new IntraDirSVFGEdge(srcNode,dstNode);
        return (addSVFGEdge(directEdge) ? directEdge : NULL);
    }
}

/*!
 * Add def-use call edges for top level pointers
 */
SVFGEdge* SVFG::addCallDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId) {
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasInterSVFGEdge(srcNode,dstNode, SVFGEdge::DirCall,csId)) {
        assert(edge->isCallDirectVFGEdge() && "this should be a direct value flow edge!");
        return NULL;
    }
    else {
        CallDirSVFGEdge* callEdge = new CallDirSVFGEdge(srcNode,dstNode,csId);
        return (addSVFGEdge(callEdge) ? callEdge : NULL);
    }
}

/*!
 * Add def-use return edges for top level pointers
 */
SVFGEdge* SVFG::addRetDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId) {
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasInterSVFGEdge(srcNode,dstNode, SVFGEdge::DirRet,csId)) {
        assert(edge->isRetDirectVFGEdge() && "this should be a direct value flow edge!");
        return NULL;
    }
    else {
        RetDirSVFGEdge* retEdge = new RetDirSVFGEdge(srcNode,dstNode,csId);
        return (addSVFGEdge(retEdge) ? retEdge : NULL);
    }
}

/*
 *  Add def-use edges of a memory region between two statements
 */
SVFGEdge* SVFG::addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts)
{
    SVFGNode* srcNode = getSVFGNode(srcId);
    SVFGNode* dstNode = getSVFGNode(dstId);
    if(SVFGEdge* edge = hasIntraSVFGEdge(srcNode,dstNode,SVFGEdge::IntraIndirect)) {
        assert(isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<IndirectSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
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
    if(SVFGEdge* edge = hasThreadSVFGEdge(srcNode,dstNode,SVFGEdge::TheadMHPIndirect)) {
        assert(isa<IndirectSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<IndirectSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
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
    if(SVFGEdge* edge = hasInterSVFGEdge(srcNode,dstNode,SVFGEdge::IndCall,csId)) {
        assert(isa<CallIndSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<CallIndSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
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
    if(SVFGEdge* edge = hasInterSVFGEdge(srcNode,dstNode,SVFGEdge::IndRet,csId)) {
        assert(isa<RetIndSVFGEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<RetIndSVFGEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
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
        GraphPrinter::WriteGraphToFile(llvm::outs(), file, this, simple);
}

/**
 * Get all inter value flow edges at this indirect call site, including call and return edges.
 */
void SVFG::getInterVFEdgesForIndirectCallSite(const llvm::CallSite cs, const llvm::Function* callee, SVFGEdgeSetTy& edges)
{
    PAG * pag = PAG::getPAG();
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
            ActualINSVFGNode * actualIn = llvm::cast<ActualINSVFGNode>(getSVFGNode(*ai_it));
            getInterVFEdgeAtIndCSFromAInToFIn(actualIn, callee, edges);
        }
    }

    // Find inter indirect return edges between actual-out and formal-out svfg nodes.
    if (hasFuncRetMu(callee) && hasCallSiteChi(cs)) {
        SVFG::ActualOUTSVFGNodeSet& actualOutNodes = getActualOUTSVFGNodes(cs);
        for(SVFG::ActualOUTSVFGNodeSet::iterator ao_it = actualOutNodes.begin(),
                ao_eit = actualOutNodes.end(); ao_it!=ao_eit; ++ao_it) {
            ActualOUTSVFGNode* actualOut = llvm::cast<ActualOUTSVFGNode>(getSVFGNode(*ao_it));
            getInterVFEdgeAtIndCSFromFOutToAOut(actualOut, callee, edges);
        }
    }
}

/**
 * Connect actual params/return to formal params/return for top-level variables.
 * Also connect indirect actual in/out and formal in/out.
 */
void SVFG::connectCallerAndCallee(CallSite cs, const llvm::Function* callee, SVFGEdgeSetTy& edges)
{
    PAG * pag = PAG::getPAG();
    CallSiteID csId = getCallSiteID(cs, callee);
    // connect actual and formal param
    if (pag->hasCallSiteArgsMap(cs) && pag->hasFunArgsMap(callee)) {
        const PAG::PAGNodeList& csArgList = pag->getCallSiteArgsList(cs);
        const PAG::PAGNodeList& funArgList = pag->getFunArgsList(callee);
        PAG::PAGNodeList::const_iterator csArgIt = csArgList.begin(), csArgEit = csArgList.end();
        PAG::PAGNodeList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        for (; funArgIt != funArgEit && csArgIt != csArgEit; funArgIt++, csArgIt++) {
            const PAGNode *cs_arg = *csArgIt;
            const PAGNode *fun_arg = *funArgIt;
            if (fun_arg->isPointer() && cs_arg->isPointer())
                connectAParamAndFParam(cs_arg, fun_arg, cs, csId, edges);
        }
        assert(funArgIt == funArgEit && "function has more arguments than call site");
        if (callee->isVarArg()) {
            NodeID varFunArg = pag->getVarargNode(callee);
            const PAGNode* varFunArgNode = pag->getPAGNode(varFunArg);
            if (varFunArgNode->isPointer()) {
                for (; csArgIt != csArgEit; csArgIt++) {
                    const PAGNode *cs_arg = *csArgIt;
                    if (cs_arg->isPointer())
                        connectAParamAndFParam(cs_arg, varFunArgNode, cs, csId, edges);
                }
            }
        }
    }

    // connect actual return and formal return
    if (pag->funHasRet(callee) && pag->callsiteHasRet(cs)) {
        const PAGNode* cs_return = pag->getCallSiteRet(cs);
        const PAGNode* fun_return = pag->getFunRet(callee);
        if (cs_return->isPointer() && fun_return->isPointer())
            connectFRetAndARet(fun_return, cs_return, csId, edges);
    }

    // connect actual in and formal in
    if (hasFuncEntryChi(callee) && hasCallSiteMu(cs)) {
        SVFG::ActualINSVFGNodeSet& actualInNodes = getActualINSVFGNodes(cs);
        const SVFG::FormalINSVFGNodeSet& formalInNodes = getFormalINSVFGNodes(callee);
        for(SVFG::ActualINSVFGNodeSet::iterator ai_it = actualInNodes.begin(),
                ai_eit = actualInNodes.end(); ai_it!=ai_eit; ++ai_it) {
            const ActualINSVFGNode * actualIn = llvm::cast<ActualINSVFGNode>(getSVFGNode(*ai_it));
            for(SVFG::FormalINSVFGNodeSet::iterator fi_it = formalInNodes.begin(),
                    fi_eit = formalInNodes.end(); fi_it!=fi_eit; ++fi_it) {
                const FormalINSVFGNode* formalIn = llvm::cast<FormalINSVFGNode>(getSVFGNode(*fi_it));
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
            const FormalOUTSVFGNode * formalOut = llvm::cast<FormalOUTSVFGNode>(getSVFGNode(*fo_it));
            for(SVFG::ActualOUTSVFGNodeSet::iterator ao_it = actualOutNodes.begin(),
                    ao_eit = actualOutNodes.end(); ao_it!=ao_eit; ++ao_it) {
                const ActualOUTSVFGNode* actualOut = llvm::cast<ActualOUTSVFGNode>(getSVFGNode(*ao_it));
                connectFOutAndAOut(formalOut, actualOut, csId, edges);
            }
        }
    }
}

/*!
 * Given a svfg node, return its left hand side top level pointer
 */
const PAGNode* SVFG::getLHSTopLevPtr(const SVFGNode* node) const {

    if(const AddrSVFGNode* addr = dyn_cast<AddrSVFGNode>(node))
        return addr->getPAGDstNode();
    else if(const CopySVFGNode* copy = dyn_cast<CopySVFGNode>(node))
        return copy->getPAGDstNode();
    else if(const GepSVFGNode* gep = dyn_cast<GepSVFGNode>(node))
        return gep->getPAGDstNode();
    else if(const LoadSVFGNode* load = dyn_cast<LoadSVFGNode>(node))
        return load->getPAGDstNode();
    else if(const PHISVFGNode* phi = dyn_cast<PHISVFGNode>(node))
        return phi->getRes();
    else if(const ActualParmSVFGNode* ap = dyn_cast<ActualParmSVFGNode>(node))
        return ap->getParam();
    else if(const FormalParmSVFGNode*fp = dyn_cast<FormalParmSVFGNode>(node))
        return fp->getParam();
    else if(const ActualRetSVFGNode* ar = dyn_cast<ActualRetSVFGNode>(node))
        return ar->getRev();
    else if(const FormalRetSVFGNode* fr = dyn_cast<FormalRetSVFGNode>(node))
        return fr->getRet();
    else if(const NullPtrSVFGNode* nullsvfg = dyn_cast<NullPtrSVFGNode>(node))
        return nullsvfg->getPAGNode();
    else
        assert(false && "unexpected node kind!");
    return NULL;
}

/*!
 * Whether this is an function entry SVFGNode (formal parameter, formal In)
 */
const llvm::Function* SVFG::isFunEntrySVFGNode(const SVFGNode* node) const {
    if(const FormalParmSVFGNode* fp = dyn_cast<FormalParmSVFGNode>(node)) {
        return fp->getFun();
    }
    else if(const InterPHISVFGNode* phi = dyn_cast<InterPHISVFGNode>(node)) {
        if(phi->isFormalParmPHI())
            return phi->getFun();
    }
    else if(const FormalINSVFGNode* fi = dyn_cast<FormalINSVFGNode>(node)) {
        return fi->getFun();
    }
    else if(const InterMSSAPHISVFGNode* mphi = dyn_cast<InterMSSAPHISVFGNode>(node)) {
        if(mphi->isFormalINPHI())
            return phi->getFun();
    }
    return NULL;
}

/*!
 * Whether this is an callsite return SVFGNode (actual return, actual out)
 */
llvm::Instruction* SVFG::isCallSiteRetSVFGNode(const SVFGNode* node) const {
    if(const ActualRetSVFGNode* ar = dyn_cast<ActualRetSVFGNode>(node)) {
        return ar->getCallSite().getInstruction();
    }
    else if(const InterPHISVFGNode* phi = dyn_cast<InterPHISVFGNode>(node)) {
        if(phi->isActualRetPHI())
            return phi->getCallSite().getInstruction();
    }
    else if(const ActualOUTSVFGNode* ao = dyn_cast<ActualOUTSVFGNode>(node)) {
        return ao->getCallSite().getInstruction();
    }
    else if(const InterMSSAPHISVFGNode* mphi = dyn_cast<InterMSSAPHISVFGNode>(node)) {
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
        if(StmtSVFGNode* stmtNode = dyn_cast<StmtSVFGNode>(node)) {
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
        else if(PHISVFGNode* tphi = dyn_cast<PHISVFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = PHI(";
            for(PHISVFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                    it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(FormalParmSVFGNode* fp = dyn_cast<FormalParmSVFGNode>(node)) {
            rawstr << "FPARM(" << fp->getParam()->getId() << ")\n";
            rawstr << "Fun[" << fp->getFun()->getName() << "]";
        }
        else if(ActualParmSVFGNode* ap = dyn_cast<ActualParmSVFGNode>(node)) {
            rawstr << "APARM(" << ap->getParam()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ap->getCallSite().getInstruction()) << "]";
        }
        else if (ActualRetSVFGNode* ar = dyn_cast<ActualRetSVFGNode>(node)) {
            rawstr << "ARet(" << ar->getRev()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ar->getCallSite().getInstruction()) << "]";
        }
        else if (FormalRetSVFGNode* fr = dyn_cast<FormalRetSVFGNode>(node)) {
            rawstr << "FRet(" << fr->getRet()->getId() << ")\n";
            rawstr << "Fun[" << fr->getFun()->getName() << "]";
        }
        else if(FormalINSVFGNode* fi = dyn_cast<FormalINSVFGNode>(node)) {
            rawstr << "ENCHI\n";
            rawstr << "Fun[" << fi->getFun()->getName() << "]";
        }
        else if(FormalOUTSVFGNode* fo = dyn_cast<FormalOUTSVFGNode>(node)) {
            rawstr << "RETMU\n";
            rawstr << "Fun[" << fo->getFun()->getName() << "]";
        }
        else if(ActualINSVFGNode* ai = dyn_cast<ActualINSVFGNode>(node)) {
            rawstr << "CSMU\n";
            rawstr << "CS[" << getSourceLoc(ai->getCallSite().getInstruction())  << "]";
        }
        else if(ActualOUTSVFGNode* ao = dyn_cast<ActualOUTSVFGNode>(node)) {
            rawstr << "CSCHI\n";
            rawstr << "CS[" << getSourceLoc(ao->getCallSite().getInstruction())  << "]";
        }
        else if(MSSAPHISVFGNode* mphi = dyn_cast<MSSAPHISVFGNode>(node)) {
            rawstr << "MSSAPHI\n";
            rawstr << getSourceLoc(&mphi->getBB()->back());
        }
        else if(isa<NullPtrSVFGNode>(node)) {
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
        if(StmtSVFGNode* stmtNode = dyn_cast<StmtSVFGNode>(node)) {
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
        else if(MSSAPHISVFGNode* mphi = dyn_cast<MSSAPHISVFGNode>(node)) {
            rawstr << "MR_" << mphi->getRes()->getMR()->getMRID()
                   << "V_" << mphi->getRes()->getResVer()->getSSAVersion() << " = PHI(";
            for (MemSSA::PHI::OPVers::const_iterator it = mphi->opVerBegin(), eit = mphi->opVerEnd();
                    it != eit; it++)
                rawstr << "MR_" << it->second->getMR()->getMRID() << "V_" << it->second->getSSAVersion() << ", ";
            rawstr << ")\n";

            rawstr << mphi->getRes()->getMR()->dumpStr() << "\n";
            rawstr << getSourceLoc(&mphi->getBB()->back());
        }
        else if(PHISVFGNode* tphi = dyn_cast<PHISVFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = PHI(";
            for(PHISVFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                    it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(FormalINSVFGNode* fi = dyn_cast<FormalINSVFGNode>(node)) {
            rawstr	<< fi->getEntryChi()->getMR()->getMRID() << "V_" << fi->getEntryChi()->getResVer()->getSSAVersion() <<
                    " = ENCHI(MR_" << fi->getEntryChi()->getMR()->getMRID() << "V_" << fi->getEntryChi()->getOpVer()->getSSAVersion() << ")\n";
            rawstr << fi->getEntryChi()->getMR()->dumpStr() << "\n";
            rawstr << "Fun[" << fi->getFun()->getName() << "]";
        }
        else if(FormalOUTSVFGNode* fo = dyn_cast<FormalOUTSVFGNode>(node)) {
            rawstr << "RETMU(" << fo->getRetMU()->getMR()->getMRID() << "V_" << fo->getRetMU()->getVer()->getSSAVersion() << ")\n";
            rawstr  << fo->getRetMU()->getMR()->dumpStr() << "\n";
            rawstr << "Fun[" << fo->getFun()->getName() << "]";
        }
        else if(FormalParmSVFGNode* fp = dyn_cast<FormalParmSVFGNode>(node)) {
            rawstr	<< "FPARM(" << fp->getParam()->getId() << ")\n";
            rawstr << "Fun[" << fp->getFun()->getName() << "]";
        }
        else if(ActualINSVFGNode* ai = dyn_cast<ActualINSVFGNode>(node)) {
            rawstr << "CSMU(" << ai->getCallMU()->getMR()->getMRID() << "V_" << ai->getCallMU()->getVer()->getSSAVersion() << ")\n";
            rawstr << ai->getCallMU()->getMR()->dumpStr() << "\n";
            rawstr << "CS[" << getSourceLoc(ai->getCallSite().getInstruction()) << "]";
        }
        else if(ActualOUTSVFGNode* ao = dyn_cast<ActualOUTSVFGNode>(node)) {
            rawstr <<  ao->getCallCHI()->getMR()->getMRID() << "V_" << ao->getCallCHI()->getResVer()->getSSAVersion() <<
                   " = CSCHI(MR_" << ao->getCallCHI()->getMR()->getMRID() << "V_" << ao->getCallCHI()->getOpVer()->getSSAVersion() << ")\n";
            rawstr << ao->getCallCHI()->getMR()->dumpStr() << "\n";
            rawstr << "CS[" << getSourceLoc(ao->getCallSite().getInstruction()) << "]" ;
        }
        else if(ActualParmSVFGNode* ap = dyn_cast<ActualParmSVFGNode>(node)) {
            rawstr << "APARM(" << ap->getParam()->getId() << ")\n" ;
            rawstr << "CS[" << getSourceLoc(ap->getCallSite().getInstruction()) << "]";
        }
        else if(isa<NullPtrSVFGNode>(node)) {
            rawstr << "NullPtr";
        }
        else if (ActualRetSVFGNode* ar = dyn_cast<ActualRetSVFGNode>(node)) {
            rawstr << "ARet(" << ar->getRev()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ar->getCallSite().getInstruction()) << "]";
        }
        else if (FormalRetSVFGNode* fr = dyn_cast<FormalRetSVFGNode>(node)) {
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

        if(StmtSVFGNode* stmtNode = dyn_cast<StmtSVFGNode>(node)) {
            const PAGEdge* edge = stmtNode->getPAGEdge();
            if (isa<AddrPE>(edge)) {
                rawstr <<  "color=green";
            } else if (isa<CopyPE>(edge)) {
                rawstr <<  "color=black";
            } else if (isa<RetPE>(edge)) {
                rawstr <<  "color=black,style=dotted";
            } else if (isa<GepPE>(edge)) {
                rawstr <<  "color=purple";
            } else if (isa<StorePE>(edge)) {
                rawstr <<  "color=blue";
            } else if (isa<LoadPE>(edge)) {
                rawstr <<  "color=red";
            } else {
                assert(0 && "No such kind edge!!");
            }
            rawstr <<  "";
        }
        else if(isa<MSSAPHISVFGNode>(node)) {
            rawstr <<  "color=black";
        }
        else if(isa<PHISVFGNode>(node)) {
            rawstr <<  "color=black";
        }
        else if(isa<NullPtrSVFGNode>(node)) {
            rawstr <<  "color=grey";
        }
        else if(isa<FormalINSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<FormalOUTSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<FormalParmSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<ActualINSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<ActualOUTSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<ActualParmSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (isa<ActualRetSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (isa<FormalRetSVFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
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
        if (isa<DirectSVFGEdge>(edge)) {
            if (isa<CallDirSVFGEdge>(edge))
                return "style=solid,color=red";
            else if (isa<RetDirSVFGEdge>(edge))
                return "style=solid,color=blue";
            else
                return "style=solid";
        } else if (isa<IndirectSVFGEdge>(edge)) {
            if (isa<CallIndSVFGEdge>(edge))
                return "style=dashed,color=red";
            else if (isa<RetIndSVFGEdge>(edge))
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
        if (CallDirSVFGEdge* dirCall = dyn_cast<CallDirSVFGEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetDirSVFGEdge* dirRet = dyn_cast<RetDirSVFGEdge>(edge))
            rawstr << dirRet->getCallSiteId();
        else if (CallIndSVFGEdge* indCall = dyn_cast<CallIndSVFGEdge>(edge))
            rawstr << indCall->getCallSiteId();
        else if (RetIndSVFGEdge* indRet = dyn_cast<RetIndSVFGEdge>(edge))
            rawstr << indRet->getCallSiteId();

        return rawstr.str();
    }
};
}
