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
 *  * Build ICFG
 * 1) build ICFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect ICFG edges
 *    between two statements (PAGEdges)
 */
ICFG::ICFG(PTACallGraph* cg): totalICFGNode(0), callgraph(cg), pag(PAG::getPAG()) {
	stat = new ICFGStat();

    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG Top Level Node\n"));
	addICFGNodes();
	addICFGEdges();
}

/*!
 * Memory has been cleaned up at GenericGraph
 */
void ICFG::destroy() {
    delete stat;
    stat = NULL;
    pag = NULL;
}

/*!
 * Create edges between ICFG nodes across functions
 */
void ICFG::addICFGInterEdges(CallSite cs, const Function* callee){
	const Function* caller = cs.getCaller();

	CallBlockNode* CallBlockNode = getCallICFGNode(cs);
	FunEntryBlockNode* calleeEntryNode = getFunEntryICFGNode(callee);
	addCallEdge(CallBlockNode, calleeEntryNode, getCallSiteID(cs, callee));

	if (!isExtCall(callee)) {
		RetBlockNode* RetBlockNode = getRetICFGNode(cs);
		FunExitBlockNode* calleeExitNode = getFunExitICFGNode(callee);
		addRetEdge(calleeExitNode, RetBlockNode, getCallSiteID(cs, callee));
	}
}

/*!
 * Add statements into IntraBlockNode
 */
void ICFG::addStmtsToIntraBlockICFGNode(IntraBlockNode* instICFGNode, const llvm::Instruction* inst){
	if (isCallSite(inst) && getCallee(inst)) {
		CallSite cs = getLLVMCallSite(inst);
		addICFGInterEdges(cs, getCallee(inst));
		addIntraEdge(instICFGNode, getCallICFGNode(cs));
		addIntraEdge(getRetICFGNode(cs), instICFGNode);
		InstVec nextInsts;
		getNextInsts(inst,nextInsts);
	    for (InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit != enit; ++nit) {
			addIntraEdge(getRetICFGNode(cs), getIntraBlockICFGNode(*nit));
	    }
	} else {
		PAG::PAGEdgeList& pagEdgeList = pag->getInstPAGEdgeList(inst);
		for (PAG::PAGEdgeList::const_iterator bit = pagEdgeList.begin(), ebit =
				pagEdgeList.end(); bit != ebit; ++bit) {
			if (!isPhiCopyEdge(*bit)) {
				StmtICFGNode* stmt = getStmtICFGNode(*bit);
				instICFGNode->addStmtICFGNode(stmt);
			}
		}
	}
}

/*
 *
 */
IntraBlockNode* ICFG::getLastInstFromBasicBlock(const llvm::BasicBlock* bb){
	const Instruction* curInst = &(*bb->begin());
	IntraBlockNode* curNode = getIntraBlockICFGNode(curInst);

	for(BasicBlock::const_iterator it = bb->begin(), eit = bb->end(); it!=eit; ++it){
		const Instruction* nextInst = &(*it);
		if (curInst != nextInst) {
			curNode = getIntraBlockICFGNode(curInst);
			IntraBlockNode* nextNode = getIntraBlockICFGNode(nextInst);
			addIntraEdge(curNode, nextNode);
			curInst = nextInst;
			curNode = nextNode;
		}
	}
	return curNode;
}
/*!
 * Create ICFG edges
 */
void ICFG::addICFGEdges(){

    SVFModule svfModule = pag->getModule();
    for (SVFModule::const_iterator iter = svfModule.begin(), eiter = svfModule.end(); iter != eiter; ++iter) {
        const Function *fun = *iter;
        if (analysisUtil::isExtCall(fun))
            continue;

        /// function entry
        FunEntryBlockNode* FunEntryBlockNode = getFunEntryICFGNode(fun);
        const BasicBlock* entryBB = &(fun->getEntryBlock());
        IntraBlockNode* entryBBNode = getFirstInstFromBasicBlock(entryBB);
        addIntraEdge(FunEntryBlockNode, entryBBNode);

        BBSet visited;
        WorkList worklist;
        /// function body
        worklist.push(entryBB);
		while (!worklist.empty()) {
            const llvm::BasicBlock* bb = worklist.pop();
			if (visited.find(bb) == visited.end()) {
				visited.insert(bb);
				IntraBlockNode* srcNode = getLastInstFromBasicBlock(bb);
	            for (succ_const_iterator sit = succ_begin(bb), esit = succ_end(bb); sit != esit; ++sit) {
	                const BasicBlock* succ = *sit;
	                IntraBlockNode* dstNode = getFirstInstFromBasicBlock(succ);
	                addIntraEdge(srcNode, dstNode);
	                worklist.push(succ);
	            }

			}
		}

		/// function exit
		FunExitBlockNode* FunExitBlockNode = getFunExitICFGNode(fun);
		const BasicBlock* exitBB = getFunExitBB(fun);
        IntraBlockNode* exitInstNode = getLastInstFromBasicBlock(exitBB);
        addIntraEdge(exitInstNode,FunExitBlockNode);
    }
}

/*!
 * Create ICFG nodes for top level pointers
 */
void ICFG::addICFGNodes() {

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

    PAGEdge::PAGEdgeSetTy& forks = getPAG()->getEdgeSet(PAGEdge::ThreadFork);
    for (PAGEdge::PAGEdgeSetTy::iterator iter = forks.begin(), eiter =
                forks.end(); iter != eiter; ++iter) {
        TDForkPE* forkedge = cast<TDForkPE>(*iter);
        addActualParmICFGNode(forkedge->getSrcNode(),forkedge->getCallSite());
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
		const llvm::Function* func = it->first;

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
        addFormalRetICFGNode(retNode,func,retPEs);
    }

    // initialize llvm phi nodes (phi of top level pointers)
    PAG::PHINodeMap& phiNodeMap = pag->getPhiNodeMap();
    for(PAG::PHINodeMap::iterator pit = phiNodeMap.begin(), epit = phiNodeMap.end(); pit!=epit; ++pit) {
        addIntraPHIICFGNode(pit->first,pit->second);
    }
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
 * Add intraprocedural edges between two nodes
 */
ICFGEdge* ICFG::addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode){
    checkIntraEdgeParents(srcNode, dstNode);
    if(ICFGEdge* edge = hasIntraICFGEdge(srcNode,dstNode, ICFGEdge::IntraCF)) {
        assert(edge->isIntraCFGEdge() && "this should be an intra CFG edge!");
        return NULL;
    }
    else {
        IntraCFGEdge* intraEdge = new IntraCFGEdge(srcNode,dstNode);
        return (addICFGEdge(intraEdge) ? intraEdge : NULL);
    }
}

/*!
 * Add interprocedural call edges between two nodes
 */
ICFGEdge* ICFG::addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, CallSiteID csId) {
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::CallCF,csId)) {
        assert(edge->isCallCFGEdge() && "this should be a call CFG edge!");
        return NULL;
    }
    else {
        CallCFGEdge* callEdge = new CallCFGEdge(srcNode,dstNode,csId);
        return (addICFGEdge(callEdge) ? callEdge : NULL);
    }
}

/*!
 * Add interprocedural return edges between two nodes
 */
ICFGEdge* ICFG::addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, CallSiteID csId) {
    if(ICFGEdge* edge = hasInterICFGEdge(srcNode,dstNode, ICFGEdge::RetCF,csId)) {
        assert(edge->isRetCFGEdge() && "this should be a return CFG edge!");
        return NULL;
    }
    else {
        RetCFGEdge* retEdge = new RetCFGEdge(srcNode,dstNode,csId);
        return (addICFGEdge(retEdge) ? retEdge : NULL);
    }
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
    return NULL;
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
        return getSimpleNodeLabel(node, graph);
    }

    /// Return label of a VFG node without MemSSA information
    static std::string getSimpleNodeLabel(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
		if (IntraBlockNode* bNode = dyn_cast<IntraBlockNode>(node)) {
			rawstr << getSourceLoc(bNode->getInst());
		}
        else if(FunEntryBlockNode* entry = dyn_cast<FunEntryBlockNode>(node)) {
            rawstr << "Entry(" << entry->getId() << ")\n";
            rawstr << "Fun[" << entry->getFun()->getName() << "]";
        }
        else if(FunExitBlockNode* exit = dyn_cast<FunExitBlockNode>(node)) {
            rawstr << "Exit(" << exit->getId() << ")\n";
            rawstr << "Fun[" << exit->getFun()->getName() << "]";
        }
        else if(CallBlockNode* call = dyn_cast<CallBlockNode>(node)) {
            rawstr << "Call(" << call->getId() << ")\n";
        }
        else if(RetBlockNode* ret = dyn_cast<RetBlockNode>(node)) {
            rawstr << "Ret(" << ret->getId() << ")\n";
        }
//        else
//            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);

        if(isa<IntraBlockNode>(node)) {
        		rawstr <<  "color=black";
        }
        else if(isa<FunEntryBlockNode>(node)) {
            rawstr <<  "color=yellow";
        }
        else if(isa<FunExitBlockNode>(node)) {
            rawstr <<  "color=yellow";
        }
        else if(isa<CallBlockNode>(node)) {
            rawstr <<  "color=red";
        }
        else if(isa<RetBlockNode>(node)) {
            rawstr <<  "color=blue";
        }
//        else
//            assert(false && "no such kind of node!!");

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, ICFG *pag) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
		if (isa<CallCFGEdge>(edge))
			return "style=solid,color=red";
		else if (isa<RetCFGEdge>(edge))
			return "style=solid,color=blue";
		else
			return "style=solid";
        return "";
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType *node, EdgeIter EI) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        raw_string_ostream rawstr(str);
        if (CallCFGEdge* dirCall = dyn_cast<CallCFGEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetCFGEdge* dirRet = dyn_cast<RetCFGEdge>(edge))
            rawstr << dirRet->getCallSiteId();

        return rawstr.str();
    }
};
}
