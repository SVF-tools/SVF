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

#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/ICFGBuilder.h"
#include "Graphs/PAG.h"

using namespace SVFUtil;

/*!
 * Create ICFG nodes and edges
 */
void ICFGBuilder::build(SVFModule* svfModule){
    for (SVFModule::const_iterator iter = svfModule->begin(), eiter = svfModule->end(); iter != eiter; ++iter) {
        const Function *fun = *iter;
        if (SVFUtil::isExtCall(fun))
            continue;
        WorkList worklist;
        processFunEntry(fun,worklist);
        processFunBody(worklist);
        processFunExit(fun);

    }
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


