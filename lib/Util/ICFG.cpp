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
#include "Util/SVFUtil.h"
#include "Util/SVFModule.h"
#include "Util/VFG.h"

using namespace SVFUtil;


static llvm::cl::opt<bool> DumpICFG("dump-icfg", llvm::cl::init(false),
                             llvm::cl::desc("Dump dot graph of ICFG"));

static llvm::cl::opt<bool> DumpLLVMInst("dump-inst", llvm::cl::init(false),
                             llvm::cl::desc("Dump LLVM instruction for each ICFG Node"));

/*!
 * Constructor
 *  * Build ICFG
 * 1) build ICFG nodes
 *    statements for top level pointers (PAGEdges)
 * 2) connect ICFG edges
 *    between two statements (PAGEdges)
 */
ICFG::ICFG(PTACallGraph* cg): totalICFGNode(0), callgraph(cg), pag(PAG::getPAG()) {
	stat = new ICFGStat(this);
	vfg = new VFG(cg);
    DBOUT(DGENERAL, outs() << pasMsg("\tCreate ICFG ...\n"));
	build();
	addVFGToICFG();
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
		RetBlockNode* retBlockNode = getRetICFGNode(cs);
		FunExitBlockNode* calleeExitNode = getFunExitICFGNode(callee);
		addRetEdge(calleeExitNode, retBlockNode, getCallSiteID(cs, callee));
	}
}

/*!
 * Add and get IntraBlock ICFGNode
 */
IntraBlockNode* ICFG::getIntraBlockICFGNode(const Instruction* inst) {
	InstToBlockNodeMapTy::const_iterator it = InstToBlockNodeMap.find(inst);
	if (it == InstToBlockNodeMap.end()) {
		IntraBlockNode* sNode = new IntraBlockNode(totalICFGNode++,inst);
		addICFGNode(sNode);
		InstToBlockNodeMap[inst] = sNode;
		return sNode;
	}
	return it->second;
}


/*!
 * (1) Add and get CallBlockICFGNode
 * (2) Handle call instruction by creating interprocedural edges
 */
InterBlockNode* ICFG::getInterBlockICFGNode(const Instruction* inst){
	CallSite cs = getLLVMCallSite(inst);
	CallBlockNode* callICFGNode = getCallICFGNode(cs);
	RetBlockNode* retICFGNode = getRetICFGNode(cs);
	if (const Function* callee = getCallee(inst))
		addICFGInterEdges(cs, callee);                       //creating interprocedural edges
	return callICFGNode;
}

/*!
 * function entry
 */
void ICFG::processFunEntry(const Function* fun, WorkList& worklist){
	FunEntryBlockNode* FunEntryBlockNode = getFunEntryICFGNode(fun);
	const Instruction* entryInst = &((fun->getEntryBlock()).front());
	InstVec insts;
	if (isInstrinsicDbgInst(entryInst))
		getNextInsts(entryInst, insts);
	else
		insts.push_back(entryInst);
	for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
			nit != enit; ++nit) {
		ICFGNode* instNode = getBlockICFGNode(*nit);           //add interprocedure edge
		addIntraEdge(FunEntryBlockNode, instNode);
		worklist.push(*nit);
	}
}

/*!
 * function body
 */
void ICFG::processFunBody(WorkList& worklist){
	BBSet visited;
	/// function body
	while (!worklist.empty()) {
		const Instruction* inst = worklist.pop();
        if (visited.find(inst) == visited.end()) {
            visited.insert(inst);
            ICFGNode* srcNode = getBlockICFGNode(inst);
            if (isReturn(inst)) {
                FunExitBlockNode* FunExitBlockNode = getFunExitICFGNode(
                        inst->getFunction());
                addIntraEdge(srcNode, FunExitBlockNode);
            }
            InstVec nextInsts;
            getNextInsts(inst, nextInsts);
            for (InstVec::const_iterator nit = nextInsts.begin(), enit =
                    nextInsts.end(); nit != enit; ++nit) {
                const Instruction* succ = *nit;
                ICFGNode* dstNode = getBlockICFGNode(succ);
                if (isNonInstricCallSite(inst)) {
                    RetBlockNode* retICFGNode = getRetICFGNode(
                            getLLVMCallSite(inst));
                    addIntraEdge(srcNode, retICFGNode);
                    srcNode = retICFGNode;
                }
                addIntraEdge(srcNode, dstNode);
                worklist.push(succ);
            }
        }
	}
}

/*!
 * function exit e.g., exit(0). In LLVM, it usually manifests as "unreachable" instruction
 * If a function has multiple exit(0), we will only have one "unreachle" instruction
 * after the UnifyFunctionExitNodes pass.
 */
void ICFG::processFunExit(const Function* fun){
	FunExitBlockNode* FunExitBlockNode = getFunExitICFGNode(fun);
	const Instruction* exitInst = &(getFunExitBB(fun)->back());
	InstVec insts;
	if (isInstrinsicDbgInst(exitInst))
		getPrevInsts(exitInst, insts);
	else
		insts.push_back(exitInst);
	for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
			nit != enit; ++nit) {
		ICFGNode* instNode = getBlockICFGNode(*nit);
		addIntraEdge(instNode, FunExitBlockNode);
	}
}


/*!
 * Create ICFG nodes and edges
 */
void ICFG::build(){
    SVFModule svfModule = pag->getModule();
    for (SVFModule::const_iterator iter = svfModule.begin(), eiter = svfModule.end(); iter != eiter; ++iter) {
        const Function *fun = *iter;
        if (SVFUtil::isExtCall(fun))
            continue;
        WorkList worklist;
        processFunEntry(fun,worklist);
        processFunBody(worklist);
        processFunExit(fun);

    }
}

void ICFG::connectGlobalToProgEntry()
{
    const Function* mainFunc = SVFUtil::getProgEntryFunction(pag->getModule());

    /// Return back if the main function is not found, the bc file might be a library only
    if(mainFunc == NULL)
        return;

    FunEntryBlockNode* entryNode = getFunEntryICFGNode(mainFunc);
    for(ICFGEdgeSetTy::const_iterator it = entryNode->getOutEdges().begin(), eit = entryNode->getOutEdges().end(); it!=eit; ++it){
        if(const IntraCFGEdge* intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(*it)){
            if(IntraBlockNode* intra = SVFUtil::dyn_cast<IntraBlockNode>(intraEdge->getDstNode())){
                for (VFG::GlobalVFGNodeSet::const_iterator nodeIt = vfg->getGlobalVFGNodes().begin(), nodeEit = vfg->getGlobalVFGNodes().end(); nodeIt != nodeEit; ++nodeIt)
                    intra->addVFGNode(*nodeIt);
            }
            else
                assert(SVFUtil::isa<CallBlockNode>(intraEdge->getDstNode()) && " the dst node of an intra edge is not an intra block node or a callblocknode?");
        }
        else
            assert(false && "the edge from main's functionEntryBlock is not an intra edge?");
    }
}


/*!
 *
 */
void ICFG::addVFGToICFG(){

    connectGlobalToProgEntry();

	for (const_iterator it = begin(), eit = end(); it!=eit; ++it){
		ICFGNode* node = it->second;
		if (IntraBlockNode* intra = SVFUtil::dyn_cast<IntraBlockNode>(node))
			handleIntraBlock(intra);
		else if (InterBlockNode* inter = SVFUtil::dyn_cast<InterBlockNode>(node))
		    handleInterBlock(inter);
	}
}

/*!
 *  Add VFGStmtNode into IntraBlockNode
 */
void ICFG::handleIntraBlock(IntraBlockNode* intraICFGNode){
	const Instruction* inst = intraICFGNode->getInst();
	if (!SVFUtil::isNonInstricCallSite(inst)) {
		PAG::PAGEdgeList& pagEdgeList = pag->getInstPAGEdgeList(inst);
		for (PAG::PAGEdgeList::const_iterator bit = pagEdgeList.begin(),
				ebit = pagEdgeList.end(); bit != ebit; ++bit) {
			const PAGEdge* edge = *bit;
			if (isPhiCopyEdge(edge)) {
				IntraPHIVFGNode* phi = vfg->getIntraPHIVFGNode(edge->getDstNode());
				intraICFGNode->addVFGNode(phi);
			}
			else if (isBinaryEdge(edge)){
                BinaryOPVFGNode* binary = vfg->getBinaryOPVFGNode(edge->getDstNode());
                intraICFGNode->addVFGNode(binary);
			}
            else if (isCmpEdge(edge)){
                CmpVFGNode* cmp = vfg->getCmpVFGNode(edge->getDstNode());
                intraICFGNode->addVFGNode(cmp);
            }
			else{
				StmtVFGNode* stmt = vfg->getStmtVFGNode(edge);
				intraICFGNode->addVFGNode(stmt);
			}
		}
	}
}

/*!
 * Add ArgumentVFGNode into InterBlockNode
 */
void ICFG::handleInterBlock(InterBlockNode* interICFGNode){

	if (FunEntryBlockNode* entry = dyn_cast<FunEntryBlockNode>(interICFGNode)){
        handleFormalParm(entry);
	}
	else if (FunExitBlockNode* exit = dyn_cast<FunExitBlockNode>(interICFGNode)){
        handleFormalRet(exit);
	}
	else if (CallBlockNode* call = dyn_cast<CallBlockNode>(interICFGNode)){
        handleActualParm(call);
	}
	else if (RetBlockNode* ret = dyn_cast<RetBlockNode>(interICFGNode)){
        handleActualRet(ret);
	}

}

/// Add FormalParmVFGNode(VFG) to FunEntryBlockNode (ICFG)
void ICFG::handleFormalParm(FunEntryBlockNode* funEntryBlockNode){
    const Function* func = funEntryBlockNode->getFun();
    if (pag->hasFunArgsMap(func)){
        const PAG::PAGNodeList& pagNodeList =  pag->getFunArgsList(func);
        for(PAG::PAGNodeList::const_iterator it = pagNodeList.begin(),
                    eit = pagNodeList.end(); it != eit; ++it){
			const PAGNode* param = *it;
			if (vfg->hasBlackHoleConstObjAddrAsDef(param) == false) {
				FormalParmVFGNode* formalParmNode = vfg->getFormalParmVFGNode(param);
				funEntryBlockNode->addFormalParms(formalParmNode);
			}
		}
    }
}

/// Add FormalRetNode(VFG) to FunExitBlockNode (ICFG)
void ICFG::handleFormalRet(FunExitBlockNode* funExitBlockNode){
    const Function* func = funExitBlockNode->getFun();
    if (pag->funHasRet(func)){
        const PAGNode* retNode =  pag->getFunRet(func);
        FormalRetVFGNode* formalRetNode = vfg->getFormalRetVFGNode(retNode);
        funExitBlockNode->addFormalRet(formalRetNode);
    }
}

/// Add ActualParmVFGNode(VFG) to CallBlockNode (ICFG)
void ICFG::handleActualParm(CallBlockNode* callBlockNode){
    CallSite cs = callBlockNode->getCallSite();
	if (pag->hasCallSiteArgsMap(cs)) {
		const PAG::PAGNodeList& pagNodeList = pag->getCallSiteArgsList(cs);
		for (PAG::PAGNodeList::const_iterator it = pagNodeList.begin(), eit =
				pagNodeList.end(); it != eit; ++it) {
			ActualParmVFGNode* actualParmNode = vfg->getActualParmVFGNode(*it, cs);
			callBlockNode->addActualParms(actualParmNode);
		}
	}
}

/// Add ActualRetVFGNode(VFG) to RetBlockNode (ICFG)
void ICFG::handleActualRet(RetBlockNode* retBlockNode){
    CallSite cs = retBlockNode->getCallSite();
    if (pag->callsiteHasRet(cs)){
        const PAGNode* retNode =  pag->getCallSiteRet(cs);
        /// if this retNode is returned from a malloc-like instruction (e.g., ret=malloc()),
        /// we have already created an address node
        /// if this is a copy edge from an external call (e.g., ret = fgets(i8* %arraydecay);
        /// we have handled it as a copy from %arraydecay to ret.
        if(!retNode->hasIncomingEdges(PAGEdge::Addr) && !retNode->hasIncomingEdges(PAGEdge::Copy)){
        		ActualRetVFGNode* actualRetNode = vfg->getActualRetVFGNode(retNode);
        		retBlockNode->addActualRet(actualRetNode);
        }
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

void ICFG::updateCallgraph(PointerAnalysis* pta) {

	PointerAnalysis::CallEdgeMap::const_iterator iter = pta->getIndCallMap().begin();
	PointerAnalysis::CallEdgeMap::const_iterator eiter = pta->getIndCallMap().end();
	for (; iter != eiter; iter++) {
		CallSite cs = iter->first;
		const PointerAnalysis::FunctionSet & functions = iter->second;
		for (PointerAnalysis::FunctionSet::const_iterator func_iter =
				functions.begin(); func_iter != functions.end(); func_iter++) {
			const Function * callee = *func_iter;
			addICFGInterEdges(cs, callee);
		}
	}
}


/*!
 * Dump ICFG
 */
void ICFG::dump(const std::string& file, bool simple) {
    if(DumpICFG)
        GraphPrinter::WriteGraphToFile(outs(), file, this, simple);
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

    /// Return the label of an ICFG node
    static std::string getSimpleNodeLabel(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);
        rawstr << "NodeID: " << node->getId() << "\n";
		if (IntraBlockNode* bNode = SVFUtil::dyn_cast<IntraBlockNode>(node)) {
			rawstr << getSourceLoc(bNode->getInst()) << "\n";

			IntraBlockNode::StmtOrPHIVFGNodeVec& nodes = bNode->getVFGNodes();
			for (IntraBlockNode::StmtOrPHIVFGNodeVec::iterator it = nodes.begin(), eit = nodes.end(); it != eit; ++it){
			    const VFGNode* node = *it;
			    if(const StmtVFGNode* stmtNode = SVFUtil::dyn_cast<StmtVFGNode>(node)){
			        NodeID src = stmtNode->getPAGSrcNodeID();
			        NodeID dst = stmtNode->getPAGDstNodeID();
			        rawstr << dst << "<--" << src << "\n";
			        std::string srcValueName = stmtNode->getPAGSrcNode()->getValueName();
			        std::string dstValueName = stmtNode->getPAGDstNode()->getValueName();
			        rawstr << dstValueName << "<--" << srcValueName << "\n";
			    }
			}

			if(DumpLLVMInst)
				rawstr << *(bNode->getInst()) << "\n";
		} else if (FunEntryBlockNode* entry = SVFUtil::dyn_cast<FunEntryBlockNode>(node)) {
			if (isExtCall(entry->getFun()))
				rawstr << "Entry(" << ")\n";
			else
				rawstr << "Entry(" << getSourceLoc(entry->getFun()) << ")\n";
			rawstr << "Fun[" << entry->getFun()->getName() << "]";
		} else if (FunExitBlockNode* exit = SVFUtil::dyn_cast<FunExitBlockNode>(node)) {
			if (isExtCall(exit->getFun()))
				rawstr << "Exit(" << ")\n";
			else
				rawstr << "Exit(" << getSourceLoc(&(exit->getBB()->back()))
						<< ")\n";
			rawstr << "Fun[" << exit->getFun()->getName() << "]";
		} else if (CallBlockNode* call = SVFUtil::dyn_cast<CallBlockNode>(node)) {
			rawstr << "Call("
					<< getSourceLoc(call->getCallSite().getInstruction())
					<< ")\n";
		} else if (RetBlockNode* ret = SVFUtil::dyn_cast<RetBlockNode>(node)) {
			rawstr << "Ret("
					<< getSourceLoc(ret->getCallSite().getInstruction())
					<< ")\n";
		}
        else
            assert(false && "what else kinds of nodes do we have??");

        return rawstr.str();
    }

    static std::string getNodeAttributes(NodeType *node, ICFG *graph) {
        std::string str;
        raw_string_ostream rawstr(str);

        if(SVFUtil::isa<IntraBlockNode>(node)) {
        		rawstr <<  "color=black";
        }
        else if(SVFUtil::isa<FunEntryBlockNode>(node)) {
            rawstr <<  "color=yellow";
        }
        else if(SVFUtil::isa<FunExitBlockNode>(node)) {
            rawstr <<  "color=green";
        }
        else if(SVFUtil::isa<CallBlockNode>(node)) {
            rawstr <<  "color=red";
        }
        else if(SVFUtil::isa<RetBlockNode>(node)) {
            rawstr <<  "color=blue";
        }
        else
            assert(false && "no such kind of node!!");

        rawstr <<  "";

        return rawstr.str();
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(NodeType *node, EdgeIter EI, ICFG *pag) {
        ICFGEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");
		if (SVFUtil::isa<CallCFGEdge>(edge))
			return "style=solid,color=red";
		else if (SVFUtil::isa<RetCFGEdge>(edge))
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
        if (CallCFGEdge* dirCall = SVFUtil::dyn_cast<CallCFGEdge>(edge))
            rawstr << dirCall->getCallSiteId();
        else if (RetCFGEdge* dirRet = SVFUtil::dyn_cast<RetCFGEdge>(edge))
            rawstr << dirRet->getCallSiteId();

        return rawstr.str();
    }
};
}
