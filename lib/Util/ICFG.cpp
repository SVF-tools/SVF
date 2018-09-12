//===- ICFG.cpp -- Sparse value-flow graph-----------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFG.cpp
 *
 *  Created on: Sep 11, 2018
 *      Author: Yulei Sui
 */

#include "Util/ICFG.h"
#include "Util/ICFGStat.h"
#include "Util/GraphUtil.h"
#include "Util/AnalysisUtil.h"
#include "Util/SVFModule.h"

using namespace llvm;
using namespace analysisUtil;

static cl::opt<bool> DumpICFG("dump-icfg", cl::init(false),
                             cl::desc("Dump dot graph of ICFG"));

/*!
 * Constructor
 */
ICFG::ICFG(): totalICFGNode(0),pta(NULL) {
	stat = new ICFGStat();
}

/*!
 * Memory has been cleaned up at GenericGraph
 */
void ICFG::destroy() {
    delete stat;
    stat = NULL;
    pta = NULL;
}

/*!
 * Build ICFG
 * 1) build ICFG nodes
 *    a) statements for top level pointers (PAGEdges)
 *    b) operators of address-taken variables (MSSAPHI and MSSACHI)
 * 2) connect ICFG edges
 *    a) between two statements (PAGEdges)
 *    b) between two memory SSA operators (MSSAPHI MSSAMU and MSSACHI)
 */
void ICFG::buildICFG(MemSSA* m) {
    pta = m->getPTA();
    stat->startClk();
    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG Top Level Node\n"));

    addICFGNodesForTopLevelPtrs();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG Addr-taken Node\n"));

    addICFGNodesForAddrTakenVars();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG Direct Edge\n"));

    connectDirectICFGEdges();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG Indirect Edge\n"));

    connectIndirectICFGEdges();
}


/*!
 * Create ICFG nodes for top level pointers
 */
void ICFG::addICFGNodesForTopLevelPtrs() {

    PAG* pag = PAG::getPAG();
    // initialize dummy definition  null pointers in order to uniform the construction
    // to be noted for black hole pointer it has already has address edge connected,
    // and its definition will be set when processing addr PAG edge.
    addNullPtrICFGNode(pag->getPAGNode(pag->getNullPtr()));

    // initialize address nodes
    PAGEdge::PAGEdgeSetTy& addrs = pag->getEdgeSet(PAGEdge::Addr);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = addrs.begin(), eiter =
                addrs.end(); iter != eiter; ++iter) {
        addAddrICFGNode(cast<AddrPE>(*iter));
    }

    // initialize copy nodes
    PAGEdge::PAGEdgeSetTy& copys = pag->getEdgeSet(PAGEdge::Copy);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = copys.begin(), eiter =
                copys.end(); iter != eiter; ++iter) {
        CopyPE* copy = cast<CopyPE>(*iter);
        if(!isPhiCopyEdge(copy))
            addCopyICFGNode(copy);
    }

    // initialize gep nodes
    PAGEdge::PAGEdgeSetTy& ngeps = pag->getEdgeSet(PAGEdge::NormalGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = ngeps.begin(), eiter =
                ngeps.end(); iter != eiter; ++iter) {
        addGepICFGNode(cast<NormalGepPE>(*iter));
    }

    PAGEdge::PAGEdgeSetTy& vgeps = pag->getEdgeSet(PAGEdge::VariantGep);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = vgeps.begin(), eiter =
                vgeps.end(); iter != eiter; ++iter) {
        addGepICFGNode(cast<VariantGepPE>(*iter));
    }

    // initialize load nodes
    PAGEdge::PAGEdgeSetTy& loads = pag->getEdgeSet(PAGEdge::Load);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = loads.begin(), eiter =
                loads.end(); iter != eiter; ++iter) {
        addLoadICFGNode(cast<LoadPE>(*iter));
    }

    // initialize store nodes
    PAGEdge::PAGEdgeSetTy& stores = pag->getEdgeSet(PAGEdge::Store);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = stores.begin(), eiter =
                stores.end(); iter != eiter; ++iter) {
        addStoreICFGNode(cast<StorePE>(*iter));
    }

    // initialize actual parameter nodes
    for(PAG::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(), eit = pag->getCallSiteArgsMap().end(); it !=eit; ++it) {
        const Function* fun = getCallee(it->first);
        fun = getDefFunForMultipleModule(fun);
        /// for external function we do not create acutalParm ICFGNode
        /// because we do not have a formal parameter to connect this actualParm
        if(isExtCall(fun))
            continue;

        for(PAG::PAGNodeList::iterator pit = it->second.begin(), epit = it->second.end(); pit!=epit; ++pit) {
            const PAGNode* pagNode = *pit;
            if (pagNode->isPointer())
                addActualParmICFGNode(pagNode,it->first);
        }
    }

    // initialize actual return nodes (callsite return)
    for(PAG::CSToRetMap::iterator it = pag->getCallSiteRets().begin(), eit = pag->getCallSiteRets().end(); it !=eit; ++it) {
        /// for external function we do not create acutalRet ICFGNode
        /// they are in the formal of AddrICFGNode if the external function returns an allocated memory
        /// if fun has body, it may also exist in isExtCall, e.g., xmalloc() in bzip2, spec2000.
        if(it->second->isPointer() == false || hasDef(it->second))
            continue;

        addActualRetICFGNode(it->second,it->first);
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
            addFormalParmICFGNode(param,func,callPEs);
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
                addFormalParmICFGNode(varParam,func,callPEs);
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
        addFormalRetICFGNode(retNode,it->first,retPEs);
    }

    // initialize llvm phi nodes (phi of top level pointers)
    PAG::PHINodeMap& phiNodeMap = pag->getPhiNodeMap();
    for(PAG::PHINodeMap::iterator pit = phiNodeMap.begin(), epit = phiNodeMap.end(); pit!=epit; ++pit) {
        addIntraPHIICFGNode(pit->first,pit->second);
    }
}

/*
 * Create ICFG nodes for address-taken variables
 */
void ICFG::addICFGNodesForAddrTakenVars() {

}

/*!
 * Connect def-use chains for direct value-flow, (value-flow of top level pointers)
 */
void ICFG::connectDirectICFGEdges() {

}

/*
 * Connect def-use chains for indirect value-flow, (value-flow of address-taken variables)
 */
void ICFG::connectIndirectICFGEdges() {

    connectFromGlobalToProgEntry();
}


/*!
 * Connect indirect ICFG edges from global initializers (store) to main function entry
 */
void ICFG::connectFromGlobalToProgEntry()
{

}


/*!
 * Whether we has an intra ICFG edge
 */
ICFGEdge* ICFG::hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}


/*!
 * Whether we has an thread ICFG edge
 */
ICFGEdge* ICFG::hasThreadICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {
    ICFGEdge edge(src,dst,kind);
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * Whether we has an inter ICFG edge
 */
ICFGEdge* ICFG::hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind,CallSiteID csId) {
    ICFGEdge edge(src,dst,ICFGEdge::makeEdgeFlagWithInvokeID(kind,csId));
    ICFGEdge* outEdge = src->hasOutgoingEdge(&edge);
    ICFGEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge) {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return NULL;
}

/*!
 * Return the corresponding ICFGEdge
 */
ICFGEdge* ICFG::getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind) {

    ICFGEdge * edge = NULL;
    Size_t counter = 0;
    for (ICFGEdge::ICFGEdgeSetTy::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter) {
        if ((*iter)->getDstID() == dst->getId() && (*iter)->getEdgeKind() == kind) {
            counter++;
            edge = (*iter);
        }
    }
    assert(counter <= 1 && "there's more than one edge between two ICFG nodes");
    return edge;

}

/*!
 * Add def-use edges for top level pointers
 */
ICFGEdge* ICFG::addIntraDirectVFEdge(NodeID srcId, NodeID dstId) {
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    checkIntraVFEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::VFIntraDirect)) {
        assert(edge->isDirectVFGEdge() && "this should be a direct value flow edge!");
        return NULL;
    }
    else {
        IntraDirVFEdge* directEdge = new IntraDirVFEdge(srcNode,dstNode);
        return (addICFGEdge(directEdge) ? directEdge : NULL);
    }
}

/*!
 * Add def-use call edges for top level pointers
 */
ICFGEdge* ICFG::addCallDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId) {
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::VFDirCall,csId)) {
        assert(edge->isCallDirectVFGEdge() && "this should be a direct value flow edge!");
        return NULL;
    }
    else {
        CallDirVFEdge* callEdge = new CallDirVFEdge(srcNode,dstNode,csId);
        return (addICFGEdge(callEdge) ? callEdge : NULL);
    }
}

/*!
 * Add def-use return edges for top level pointers
 */
ICFGEdge* ICFG::addRetDirectVFEdge(NodeID srcId, NodeID dstId, CallSiteID csId) {
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::VFDirRet,csId)) {
        assert(edge->isRetDirectVFGEdge() && "this should be a direct value flow edge!");
        return NULL;
    }
    else {
        RetDirVFEdge* retEdge = new RetDirVFEdge(srcNode,dstNode,csId);
        return (addICFGEdge(retEdge) ? retEdge : NULL);
    }
}

/*
 *  Add def-use edges of a memory region between two statements
 */
ICFGEdge* ICFG::addIntraIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts)
{
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    checkIntraVFEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode,ICFGEdge::VFIntraIndirect)) {
        assert(isa<IndirectVFEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<IndirectVFEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        IntraIndVFEdge* indirectEdge = new IntraIndVFEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(cpts);
        return (addICFGEdge(indirectEdge) ? indirectEdge : NULL);
    }
}

/*!
 * Add def-use edges of a memory region between two may-happen-in-parallel statements for multithreaded program
 */
ICFGEdge* ICFG::addThreadMHPIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts) {
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    if(ICFGEdge* edge = hasThreadICFGEdge(srcNode,dstNode,ICFGEdge::VFTheadMHPIndirect)) {
        assert(isa<IndirectVFEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<IndirectVFEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        ThreadMHPIndVFEdge* indirectEdge = new ThreadMHPIndVFEdge(srcNode,dstNode);
        indirectEdge->addPointsTo(cpts);
        return (addICFGEdge(indirectEdge) ? indirectEdge : NULL);
    }
}

/*
 *  Add def-use call edges of a memory region between two statements
 */
ICFGEdge* ICFG::addCallIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId)
{
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode,ICFGEdge::VFIndCall,csId)) {
        assert(isa<CallIndVFEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<CallIndVFEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        CallIndVFEdge* callEdge = new CallIndVFEdge(srcNode,dstNode,csId);
        callEdge->addPointsTo(cpts);
        return (addICFGEdge(callEdge) ? callEdge : NULL);
    }
}

/*
 *  Add def-use return edges of a memory region between two statements
 */
ICFGEdge* ICFG::addRetIndirectVFEdge(NodeID srcId, NodeID dstId, const PointsTo& cpts,CallSiteID csId)
{
    ICFGNode* srcNode = getICFGNode(srcId);
    ICFGNode* dstNode = getICFGNode(dstId);
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode,ICFGEdge::VFIndRet,csId)) {
        assert(isa<RetIndVFEdge>(edge) && "this should be a indirect value flow edge!");
        return (cast<RetIndVFEdge>(edge)->addPointsTo(cpts) ? edge : NULL);
    }
    else {
        RetIndVFEdge* retEdge = new RetIndVFEdge(srcNode,dstNode,csId);
        retEdge->addPointsTo(cpts);
        return (addICFGEdge(retEdge) ? retEdge : NULL);
    }
}

/*!
 *
 */
ICFGEdge* ICFG::addInterIndirectVFCallEdge(const ActualINICFGNode* src, const FormalINICFGNode* dst,CallSiteID csId) {
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
ICFGEdge* ICFG::addInterIndirectVFRetEdge(const FormalOUTICFGNode* src, const ActualOUTICFGNode* dst,CallSiteID csId) {

    PointsTo cpts1 = src->getPointsTo();
    PointsTo cpts2 = dst->getPointsTo();
    if(cpts1.intersects(cpts2)) {
        cpts1 &= cpts2;
        return addRetIndirectVFEdge(src->getId(),dst->getId(),cpts1,csId);
    }
    return NULL;
}

/*!
 * Dump ICFG
 */
void ICFG::dump(const std::string& file, bool simple) {
    if(DumpICFG)
        GraphPrinter::WriteGraphToFile(llvm::outs(), file, this, simple);
}

/**
 * Connect actual params/return to formal params/return for top-level variables.
 * Also connect indirect actual in/out and formal in/out.
 */
void ICFG::connectCallerAndCallee(CallSite cs, const llvm::Function* callee, ICFGEdgeSetTy& edges)
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
}

/*!
 * Given a ICFG node, return its left hand side top level pointer
 */
const PAGNode* ICFG::getLHSTopLevPtr(const ICFGNode* node) const {

    if(const AddrICFGNode* addr = dyn_cast<AddrICFGNode>(node))
        return addr->getPAGDstNode();
    else if(const CopyICFGNode* copy = dyn_cast<CopyICFGNode>(node))
        return copy->getPAGDstNode();
    else if(const GepICFGNode* gep = dyn_cast<GepICFGNode>(node))
        return gep->getPAGDstNode();
    else if(const LoadICFGNode* load = dyn_cast<LoadICFGNode>(node))
        return load->getPAGDstNode();
    else if(const PHIICFGNode* phi = dyn_cast<PHIICFGNode>(node))
        return phi->getRes();
    else if(const ActualParmICFGNode* ap = dyn_cast<ActualParmICFGNode>(node))
        return ap->getParam();
    else if(const FormalParmICFGNode*fp = dyn_cast<FormalParmICFGNode>(node))
        return fp->getParam();
    else if(const ActualRetICFGNode* ar = dyn_cast<ActualRetICFGNode>(node))
        return ar->getRev();
    else if(const FormalRetICFGNode* fr = dyn_cast<FormalRetICFGNode>(node))
        return fr->getRet();
    else if(const NullPtrICFGNode* nullICFG = dyn_cast<NullPtrICFGNode>(node))
        return nullICFG->getPAGNode();
    else
        assert(false && "unexpected node kind!");
    return NULL;
}

/*!
 * Whether this is an function entry ICFGNode (formal parameter, formal In)
 */
const llvm::Function* ICFG::isFunEntryICFGNode(const ICFGNode* node) const {
    if(const FormalParmICFGNode* fp = dyn_cast<FormalParmICFGNode>(node)) {
        return fp->getFun();
    }
    else if(const InterPHIICFGNode* phi = dyn_cast<InterPHIICFGNode>(node)) {
        if(phi->isFormalParmPHI())
            return phi->getFun();
    }
    else if(const FormalINICFGNode* fi = dyn_cast<FormalINICFGNode>(node)) {
        return fi->getFun();
    }
    else if(const InterMSSAPHIICFGNode* mphi = dyn_cast<InterMSSAPHIICFGNode>(node)) {
        if(mphi->isFormalINPHI())
            return phi->getFun();
    }
    return NULL;
}

/*!
 * Whether this is an callsite return ICFGNode (actual return, actual out)
 */
llvm::Instruction* ICFG::isCallSiteRetICFGNode(const ICFGNode* node) const {
    if(const ActualRetICFGNode* ar = dyn_cast<ActualRetICFGNode>(node)) {
        return ar->getCallSite().getInstruction();
    }
    else if(const InterPHIICFGNode* phi = dyn_cast<InterPHIICFGNode>(node)) {
        if(phi->isActualRetPHI())
            return phi->getCallSite().getInstruction();
    }
    else if(const ActualOUTICFGNode* ao = dyn_cast<ActualOUTICFGNode>(node)) {
        return ao->getCallSite().getInstruction();
    }
    else if(const InterMSSAPHIICFGNode* mphi = dyn_cast<InterMSSAPHIICFGNode>(node)) {
        if(mphi->isActualOUTPHI())
            return mphi->getCallSite().getInstruction();
    }
    return NULL;
}

/*!
 * Perform Statistics
 */
void ICFG::performStat()
{
    stat->performStat();
}

/*!
 * GraphTraits specialization
 */
namespace llvm {
template<>
struct DOTGraphTraits<ICFG*> : public DOTGraphTraits<PAG*> {

    typedef ICFGNode NodeType;
    DOTGraphTraits(bool isSimple = false) :
        DOTGraphTraits<PAG*>(isSimple) {
    }

    /// Return name of the graph
    static std::string getGraphName(ICFG *graph) {
        return "ICFG";
    }

    std::string getNodeLabel(NodeType *node, ICFG *graph) {
        if (isSimple())
            return getSimpleNodeLabel(node, graph);
        else
            return getCompleteNodeLabel(node, graph);
    }

    /// Return label of a VFG node without MemSSA information
    static std::string getSimpleNodeLabel(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if(StmtICFGNode* stmtNode = dyn_cast<StmtICFGNode>(node)) {
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
        else if(PHIICFGNode* tphi = dyn_cast<PHIICFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = PHI(";
            for(PHIICFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                    it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(FormalParmICFGNode* fp = dyn_cast<FormalParmICFGNode>(node)) {
            rawstr << "FPARM(" << fp->getParam()->getId() << ")\n";
            rawstr << "Fun[" << fp->getFun()->getName() << "]";
        }
        else if(ActualParmICFGNode* ap = dyn_cast<ActualParmICFGNode>(node)) {
            rawstr << "APARM(" << ap->getParam()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ap->getCallSite().getInstruction()) << "]";
        }
        else if (ActualRetICFGNode* ar = dyn_cast<ActualRetICFGNode>(node)) {
            rawstr << "ARet(" << ar->getRev()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ar->getCallSite().getInstruction()) << "]";
        }
        else if (FormalRetICFGNode* fr = dyn_cast<FormalRetICFGNode>(node)) {
            rawstr << "FRet(" << fr->getRet()->getId() << ")\n";
            rawstr << "Fun[" << fr->getFun()->getName() << "]";
        }
        else if(FormalINICFGNode* fi = dyn_cast<FormalINICFGNode>(node)) {
            rawstr << "ENCHI\n";
            rawstr << "Fun[" << fi->getFun()->getName() << "]";
        }
        else if(FormalOUTICFGNode* fo = dyn_cast<FormalOUTICFGNode>(node)) {
            rawstr << "RETMU\n";
            rawstr << "Fun[" << fo->getFun()->getName() << "]";
        }
        else if(ActualINICFGNode* ai = dyn_cast<ActualINICFGNode>(node)) {
            rawstr << "CSMU\n";
            rawstr << "CS[" << getSourceLoc(ai->getCallSite().getInstruction())  << "]";
        }
        else if(ActualOUTICFGNode* ao = dyn_cast<ActualOUTICFGNode>(node)) {
            rawstr << "CSCHI\n";
            rawstr << "CS[" << getSourceLoc(ao->getCallSite().getInstruction())  << "]";
        }
        else if(MSSAPHIICFGNode* mphi = dyn_cast<MSSAPHIICFGNode>(node)) {
            rawstr << "MSSAPHI\n";
            rawstr << getSourceLoc(&mphi->getBB()->back());
        }
        else if(isa<NullPtrICFGNode>(node)) {
            rawstr << "NullPtr";
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    /// Return label of a VFG node with MemSSA information
    static std::string getCompleteNodeLabel(NodeType *node, ICFG *graph) {

        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
        if(StmtICFGNode* stmtNode = dyn_cast<StmtICFGNode>(node)) {
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
        else if(MSSAPHIICFGNode* mphi = dyn_cast<MSSAPHIICFGNode>(node)) {
            rawstr << "MR_" << mphi->getRes()->getMR()->getMRID()
                   << "V_" << mphi->getRes()->getResVer()->getSSAVersion() << " = PHI(";
            for (MemSSA::PHI::OPVers::const_iterator it = mphi->opVerBegin(), eit = mphi->opVerEnd();
                    it != eit; it++)
                rawstr << "MR_" << it->second->getMR()->getMRID() << "V_" << it->second->getSSAVersion() << ", ";
            rawstr << ")\n";

            rawstr << mphi->getRes()->getMR()->dumpStr() << "\n";
            rawstr << getSourceLoc(&mphi->getBB()->back());
        }
        else if(PHIICFGNode* tphi = dyn_cast<PHIICFGNode>(node)) {
            rawstr << tphi->getRes()->getId() << " = PHI(";
            for(PHIICFGNode::OPVers::const_iterator it = tphi->opVerBegin(), eit = tphi->opVerEnd();
                    it != eit; it++)
                rawstr << it->second->getId() << ", ";
            rawstr << ")\n";
            rawstr << getSourceLoc(tphi->getRes()->getValue());
        }
        else if(FormalINICFGNode* fi = dyn_cast<FormalINICFGNode>(node)) {
            rawstr	<< fi->getEntryChi()->getMR()->getMRID() << "V_" << fi->getEntryChi()->getResVer()->getSSAVersion() <<
                    " = ENCHI(MR_" << fi->getEntryChi()->getMR()->getMRID() << "V_" << fi->getEntryChi()->getOpVer()->getSSAVersion() << ")\n";
            rawstr << fi->getEntryChi()->getMR()->dumpStr() << "\n";
            rawstr << "Fun[" << fi->getFun()->getName() << "]";
        }
        else if(FormalOUTICFGNode* fo = dyn_cast<FormalOUTICFGNode>(node)) {
            rawstr << "RETMU(" << fo->getRetMU()->getMR()->getMRID() << "V_" << fo->getRetMU()->getVer()->getSSAVersion() << ")\n";
            rawstr  << fo->getRetMU()->getMR()->dumpStr() << "\n";
            rawstr << "Fun[" << fo->getFun()->getName() << "]";
        }
        else if(FormalParmICFGNode* fp = dyn_cast<FormalParmICFGNode>(node)) {
            rawstr	<< "FPARM(" << fp->getParam()->getId() << ")\n";
            rawstr << "Fun[" << fp->getFun()->getName() << "]";
        }
        else if(ActualINICFGNode* ai = dyn_cast<ActualINICFGNode>(node)) {
            rawstr << "CSMU(" << ai->getCallMU()->getMR()->getMRID() << "V_" << ai->getCallMU()->getVer()->getSSAVersion() << ")\n";
            rawstr << ai->getCallMU()->getMR()->dumpStr() << "\n";
            rawstr << "CS[" << getSourceLoc(ai->getCallSite().getInstruction()) << "]";
        }
        else if(ActualOUTICFGNode* ao = dyn_cast<ActualOUTICFGNode>(node)) {
            rawstr <<  ao->getCallCHI()->getMR()->getMRID() << "V_" << ao->getCallCHI()->getResVer()->getSSAVersion() <<
                   " = CSCHI(MR_" << ao->getCallCHI()->getMR()->getMRID() << "V_" << ao->getCallCHI()->getOpVer()->getSSAVersion() << ")\n";
            rawstr << ao->getCallCHI()->getMR()->dumpStr() << "\n";
            rawstr << "CS[" << getSourceLoc(ao->getCallSite().getInstruction()) << "]" ;
        }
        else if(ActualParmICFGNode* ap = dyn_cast<ActualParmICFGNode>(node)) {
            rawstr << "APARM(" << ap->getParam()->getId() << ")\n" ;
            rawstr << "CS[" << getSourceLoc(ap->getCallSite().getInstruction()) << "]";
        }
        else if(isa<NullPtrICFGNode>(node)) {
            rawstr << "NullPtr";
        }
        else if (ActualRetICFGNode* ar = dyn_cast<ActualRetICFGNode>(node)) {
            rawstr << "ARet(" << ar->getRev()->getId() << ")\n";
            rawstr << "CS[" << getSourceLoc(ar->getCallSite().getInstruction()) << "]";
        }
        else if (FormalRetICFGNode* fr = dyn_cast<FormalRetICFGNode>(node)) {
            rawstr << "FRet(" << fr->getRet()->getId() << ")\n";
            rawstr << "Fun[" << fr->getFun()->getName() << "]";
        }
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);

        if(StmtICFGNode* stmtNode = dyn_cast<StmtICFGNode>(node)) {
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
        else if(isa<MSSAPHIICFGNode>(node)) {
            rawstr <<  "color=black";
        }
        else if(isa<PHIICFGNode>(node)) {
            rawstr <<  "color=black";
        }
        else if(isa<NullPtrICFGNode>(node)) {
            rawstr <<  "color=grey";
        }
        else if(isa<FormalINICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<FormalOUTICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<FormalParmICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<ActualINICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<ActualOUTICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if(isa<ActualParmICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (isa<ActualRetICFGNode>(node)) {
            rawstr <<  "color=yellow,style=double";
        }
        else if (isa<FormalRetICFGNode>(node)) {
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
    static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, ICFG *pag) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
        if (isa<DirectVFEdge>(edge)) {
            if (isa<CallDirVFEdge>(edge))
                return "style=solid,color=red";
            else if (isa<RetDirVFEdge>(edge))
                return "style=solid,color=blue";
            else
                return "style=solid";
        } else if (isa<IndirectVFEdge>(edge)) {
            if (isa<CallIndVFEdge>(edge))
                return "style=dashed,color=red";
            else if (isa<RetIndVFEdge>(edge))
                return "style=dashed,color=blue";
            else
                return "style=dashed";
        }
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallDirVFEdge* dirCall = dyn_cast<CallDirVFEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetDirVFEdge* dirRet = dyn_cast<RetDirVFEdge>(edge))
            rawstr << dirRet->getCallSiteId();
        else if (CallIndVFEdge* indCall = dyn_cast<CallIndVFEdge>(edge))
            rawstr << indCall->getCallSiteId();
        else if (RetIndVFEdge* indRet = dyn_cast<RetIndVFEdge>(edge))
            rawstr << indRet->getCallSiteId();

        return rawstr.str();
    }
};
}
