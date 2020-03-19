//===- ICFGBuilder.cpp ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * ICFGBuilder.cpp
 *
 *  Created on:
 *      Author: yulei
 */

#include "Util/ICFGBuilder.h"
using namespace SVFUtil;

/*!
 * Create ICFG nodes and edges
 */
void ICFGBuilder::build(SVFModule svfModule){
    for (SVFModule::const_iterator iter = svfModule.begin(), eiter = svfModule.end(); iter != eiter; ++iter) {
        const Function *fun = *iter;
        if (SVFUtil::isExtCall(fun))
            continue;
        WorkList worklist;
        processFunEntry(fun,worklist);
        processFunBody(worklist);
        processFunExit(fun);

    }

	//addPAGEdgeToICFG();
}

/*!
 * function entry
 */
void ICFGBuilder::processFunEntry(const Function* fun, WorkList& worklist){
	FunEntryBlockNode* FunEntryBlockNode = getOrAddFunEntryICFGNode(fun);
	const Instruction* entryInst = &((fun->getEntryBlock()).front());
	InstVec insts;
	if (isInstrinsicDbgInst(entryInst))
		getNextInsts(entryInst, insts);
	else
		insts.push_back(entryInst);
	for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
			nit != enit; ++nit) {
		ICFGNode* instNode = getOrAddBlockICFGNode(*nit);           //add interprocedure edge
		icfg->addIntraEdge(FunEntryBlockNode, instNode);
		worklist.push(*nit);
	}
}

/*!
 * function body
 */
void ICFGBuilder::processFunBody(WorkList& worklist){
	BBSet visited;
	/// function body
	while (!worklist.empty()) {
		const Instruction* inst = worklist.pop();
        if (visited.find(inst) == visited.end()) {
            visited.insert(inst);
            ICFGNode* srcNode = getOrAddBlockICFGNode(inst);
            if (isReturn(inst)) {
                FunExitBlockNode* FunExitBlockNode = getOrAddFunExitICFGNode(
                        inst->getFunction());
                icfg->addIntraEdge(srcNode, FunExitBlockNode);
            }
            InstVec nextInsts;
            getNextInsts(inst, nextInsts);
            for (InstVec::const_iterator nit = nextInsts.begin(), enit =
                    nextInsts.end(); nit != enit; ++nit) {
                const Instruction* succ = *nit;
                ICFGNode* dstNode = getOrAddBlockICFGNode(succ);
                if (isNonInstricCallSite(inst)) {
                    RetBlockNode* retICFGNode = getOrAddRetICFGNode(
                            getLLVMCallSite(inst));
                    icfg->addIntraEdge(srcNode, retICFGNode);
                    srcNode = retICFGNode;
                }
                icfg->addIntraEdge(srcNode, dstNode);
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
void ICFGBuilder::processFunExit(const Function* fun){
	FunExitBlockNode* FunExitBlockNode = getOrAddFunExitICFGNode(fun);
	const Instruction* exitInst = &(getFunExitBB(fun)->back());
	InstVec insts;
	if (isInstrinsicDbgInst(exitInst))
		getPrevInsts(exitInst, insts);
	else
		insts.push_back(exitInst);
	for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
			nit != enit; ++nit) {
		ICFGNode* instNode = getOrAddBlockICFGNode(*nit);
		icfg->addIntraEdge(instNode, FunExitBlockNode);
	}
}




/*!
 * (1) Add and get CallBlockICFGNode
 * (2) Handle call instruction by creating interprocedural edges
 */
InterBlockNode* ICFGBuilder::getOrAddInterBlockICFGNode(const Instruction* inst){
	CallSite cs = getLLVMCallSite(inst);
	CallBlockNode* callICFGNode = getOrAddCallICFGNode(cs);
	RetBlockNode* retICFGNode = getOrAddRetICFGNode(cs);
	if (const Function* callee = getCallee(inst))
		addICFGInterEdges(cs, callee);                       //creating interprocedural edges
	return callICFGNode;
}

/*!
 * Create edges between ICFG nodes across functions
 */
void ICFGBuilder::addICFGInterEdges(CallSite cs, const Function* callee){
	const Function* caller = cs.getCaller();

	CallBlockNode* CallBlockNode = getOrAddCallICFGNode(cs);
	FunEntryBlockNode* calleeEntryNode = getOrAddFunEntryICFGNode(callee);
	icfg->addCallEdge(CallBlockNode, calleeEntryNode, cs);

	if (!isExtCall(callee)) {
		RetBlockNode* retBlockNode = getOrAddRetICFGNode(cs);
		FunExitBlockNode* calleeExitNode = getOrAddFunExitICFGNode(callee);
		icfg->addRetEdge(calleeExitNode, retBlockNode, cs);
	}
}


/*!
 *
 */
void ICFGBuilder::addPAGEdgeToICFG(){

    connectGlobalToProgEntry();

	for (ICFG::const_iterator it = icfg->begin(), eit = icfg->end(); it!=eit; ++it){
		ICFGNode* node = it->second;
		if (IntraBlockNode* intra = SVFUtil::dyn_cast<IntraBlockNode>(node))
			handleIntraBlock(intra);
		else if (InterBlockNode* inter = SVFUtil::dyn_cast<InterBlockNode>(node))
		    handleInterBlock(inter);
	}
}


void ICFGBuilder::connectGlobalToProgEntry()
{
    const Function* mainFunc = SVFUtil::getProgEntryFunction(PAG::getPAG()->getModule());

    /// Return back if the main function is not found, the bc file might be a library only
    if(mainFunc == NULL)
        return;

    FunEntryBlockNode* entryNode = getOrAddFunEntryICFGNode(mainFunc);
    for(ICFG::ICFGEdgeSetTy::const_iterator it = entryNode->getOutEdges().begin(), eit = entryNode->getOutEdges().end(); it!=eit; ++it){
        if(const IntraCFGEdge* intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(*it)){
            if(IntraBlockNode* intra = SVFUtil::dyn_cast<IntraBlockNode>(intraEdge->getDstNode())){
                const PAG::PAGEdgeSet& globaledges = PAG::getPAG()->getGlobalPAGEdgeSet();
                for (PAG::PAGEdgeSet::const_iterator edgeIt = globaledges.begin(), edgeEit = globaledges.end(); edgeIt != edgeEit; ++edgeIt)
                    intra->addPAGEdge(*edgeIt);
            }
            else
                assert(SVFUtil::isa<CallBlockNode>(intraEdge->getDstNode()) && " the dst node of an intra edge is not an intra block node or a callblocknode?");
        }
        else
            assert(false && "the edge from main's functionEntryBlock is not an intra edge?");
    }
}


/*!
 *  Add VFGStmtNode into IntraBlockNode
 */
void ICFGBuilder::handleIntraBlock(IntraBlockNode* intraICFGNode){
	const Instruction* inst = intraICFGNode->getInst();
	if (!SVFUtil::isNonInstricCallSite(inst)) {
		PAG::PAGEdgeList& pagEdgeList = PAG::getPAG()->getInstPAGEdgeList(inst);
		for (PAG::PAGEdgeList::const_iterator bit = pagEdgeList.begin(),
				ebit = pagEdgeList.end(); bit != ebit; ++bit) {
			const PAGEdge* edge = *bit;
			intraICFGNode->addPAGEdge(edge);
		}
	}
}

/*!
 * Add ArgumentVFGNode into InterBlockNode
 */
void ICFGBuilder::handleInterBlock(InterBlockNode* interICFGNode){

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
void ICFGBuilder::handleFormalParm(FunEntryBlockNode* funEntryBlockNode){
    const Function* func = funEntryBlockNode->getFun();
    if (PAG::getPAG()->hasFunArgsMap(func)){
        const PAG::PAGNodeList& pagNodeList =  PAG::getPAG()->getFunArgsList(func);
        for(PAG::PAGNodeList::const_iterator it = pagNodeList.begin(),
                    eit = pagNodeList.end(); it != eit; ++it){
			const PAGNode* param = *it;
			funEntryBlockNode->addFormalParms(param);
		}
    }
}

/// Add FormalRetNode(VFG) to FunExitBlockNode (ICFG)
void ICFGBuilder::handleFormalRet(FunExitBlockNode* funExitBlockNode){
    const Function* func = funExitBlockNode->getFun();
    if (PAG::getPAG()->funHasRet(func)){
        const PAGNode* retNode =  PAG::getPAG()->getFunRet(func);
        funExitBlockNode->addFormalRet(retNode);
    }
}

/// Add ActualParmVFGNode(VFG) to CallBlockNode (ICFG)
void ICFGBuilder::handleActualParm(CallBlockNode* callBlockNode){
    CallSite cs = callBlockNode->getCallSite();
	if (PAG::getPAG()->hasCallSiteArgsMap(cs)) {
		const PAG::PAGNodeList& pagNodeList = PAG::getPAG()->getCallSiteArgsList(cs);
		for (PAG::PAGNodeList::const_iterator it = pagNodeList.begin(), eit =
				pagNodeList.end(); it != eit; ++it) {
			callBlockNode->addActualParms(*it);
		}
	}
}

/// Add ActualRetVFGNode(VFG) to RetBlockNode (ICFG)
void ICFGBuilder::handleActualRet(RetBlockNode* retBlockNode){
    CallSite cs = retBlockNode->getCallSite();
    if (PAG::getPAG()->callsiteHasRet(cs)){
        const PAGNode* retNode =  PAG::getPAG()->getCallSiteRet(cs);
        /// if this retNode is returned from a malloc-like instruction (e.g., ret=malloc()),
        /// we have already created an address node
        /// if this is a copy edge from an external call (e.g., ret = fgets(i8* %arraydecay);
        /// we have handled it as a copy from %arraydecay to ret.
        if(!retNode->hasIncomingEdges(PAGEdge::Addr) && !retNode->hasIncomingEdges(PAGEdge::Copy)){
			retBlockNode->addActualRet(retNode);
        }
    }
}
