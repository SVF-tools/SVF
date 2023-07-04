//===- ICFGBuilder.cpp ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//


/*
 * ICFGBuilder.cpp
 *
 *  Created on:
 *      Author: yulei
 */

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/ICFGBuilder.h"
#include "SVFIR/SVFIR.h"

using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

/*!
 * Create ICFG nodes and edges
 */
void ICFGBuilder::build(SVFModule* svfModule)
{
    DBOUT(DGENERAL, outs() << pasMsg("\t Building ICFG ...\n"));
    // Add the unqiue global ICFGNode at the entry of a program (before the main method).
    icfg->addGlobalICFGNode();

    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            const Function *fun = &*F;
            const SVFFunction* svffun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
            if (SVFUtil::isExtCall(svffun))
                continue;
            WorkList worklist;
            processFunEntry(fun,worklist);
            processFunBody(worklist);
            processFunExit(fun);
        }
    }
    connectGlobalToProgEntry(svfModule);
}

/*!
 * function entry
 */
void ICFGBuilder::processFunEntry(const Function*  fun, WorkList& worklist)
{
    FunEntryICFGNode* FunEntryICFGNode = icfg->getFunEntryICFGNode(LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun));
    const Instruction* entryInst = &((fun->getEntryBlock()).front());
    const SVFInstruction* svfentryInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(entryInst);

    InstVec insts;
    if (isIntrinsicInst(svfentryInst))
        getNextInsts(entryInst, insts);
    else
        insts.push_back(entryInst);
    for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
            nit != enit; ++nit)
    {
        ICFGNode* instNode = getOrAddBlockICFGNode(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(*nit));           //add interprocedure edge
        icfg->addIntraEdge(FunEntryICFGNode, instNode);
        worklist.push(*nit);
    }
}

/*!
 * function body
 */
void ICFGBuilder::processFunBody(WorkList& worklist)
{
    BBSet visited;
    /// function body
    while (!worklist.empty())
    {
        const Instruction* inst = worklist.pop();
        if (visited.find(inst) == visited.end())
        {
            visited.insert(inst);
            const SVFInstruction* svfinst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(inst);
            ICFGNode* srcNode = getOrAddBlockICFGNode(svfinst);
            if (svfinst->isRetInst())
            {
                const SVFFunction* svfFun = svfinst->getFunction();
                FunExitICFGNode* FunExitICFGNode = icfg->getFunExitICFGNode(svfFun);
                icfg->addIntraEdge(srcNode, FunExitICFGNode);
            }
            InstVec nextInsts;
            getNextInsts(inst, nextInsts);
            u32_t branchID = 0;
            for (InstVec::const_iterator nit = nextInsts.begin(), enit =
                        nextInsts.end(); nit != enit; ++nit)
            {
                const Instruction* succ = *nit;
                const SVFInstruction* svfsucc = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(succ);
                ICFGNode* dstNode = getOrAddBlockICFGNode(svfsucc);
                if (isNonInstricCallSite(svfinst))
                {
                    RetICFGNode* retICFGNode = getRetICFGNode(svfinst);
                    srcNode = retICFGNode;
                }

                if (const BranchInst* br = SVFUtil::dyn_cast<BranchInst>(inst))
                {
                    assert(branchID <= 2 && "if/else has more than two branches?");
                    if(br->isConditional())
                        icfg->addConditionalIntraEdge(srcNode, dstNode, LLVMModuleSet::getLLVMModuleSet()->getSVFValue(br->getCondition()), 1 - branchID);
                    else
                        icfg->addIntraEdge(srcNode, dstNode);
                }
                else if (const SwitchInst* si = SVFUtil::dyn_cast<SwitchInst>(inst))
                {
                    /// branch condition value
                    const ConstantInt* condVal = const_cast<SwitchInst*>(si)->findCaseDest(const_cast<BasicBlock*>(succ->getParent()));
                    /// default case is set to -1;
                    s64_t val = -1;
                    if (condVal && condVal->getBitWidth() <= 64)
                        val = condVal->getSExtValue();
                    icfg->addConditionalIntraEdge(srcNode, dstNode, LLVMModuleSet::getLLVMModuleSet()->getSVFValue(si->getCondition()),val);
                }
                else
                    icfg->addIntraEdge(srcNode, dstNode);

                worklist.push(succ);
                branchID++;
            }
        }
    }
}

/*!
 * function exit e.g., exit(0). In LLVM, it usually manifests as "unreachable" instruction
 * If a function has multiple exit(0), we will only have one "unreachle" instruction
 * after the UnifyFunctionExitNodes pass.
 */
void ICFGBuilder::processFunExit(const Function*  f)
{
    const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(f);
    FunExitICFGNode* FunExitICFGNode = icfg->getFunExitICFGNode(fun);

    for (const SVFBasicBlock* svfbb : fun->getBasicBlockList())
    {
        for (const SVFInstruction* inst : svfbb->getInstructionList())
        {
            if(inst->isRetInst())
            {
                ICFGNode* instNode = getOrAddBlockICFGNode(inst);
                icfg->addIntraEdge(instNode, FunExitICFGNode);
            }
        }
    }
}




/*!
 * (1) Add and get CallBlockICFGNode
 * (2) Handle call instruction by creating interprocedural edges
 */
InterICFGNode* ICFGBuilder::getOrAddInterBlockICFGNode(const SVFInstruction* inst)
{
    assert(SVFUtil::isCallSite(inst) && "not a call instruction?");
    assert(SVFUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    CallICFGNode* callICFGNode = getCallICFGNode(inst);
    addICFGInterEdges(inst, getCallee(inst));                       //creating interprocedural edges
    return callICFGNode;
}

/*!
 * Create edges between ICFG nodes across functions
 */
void ICFGBuilder::addICFGInterEdges(const SVFInstruction* cs, const SVFFunction* callee)
{
    CallICFGNode* callICFGNode = getCallICFGNode(cs);
    RetICFGNode* retBlockNode = getRetICFGNode(cs);

    /// direct call
    if(callee)
    {
        /// if this is an external function (no function body)
        if (isExtCall(callee))
        {
            icfg->addIntraEdge(callICFGNode, retBlockNode);
        }
        /// otherwise connect interprocedural edges
        else
        {
            FunEntryICFGNode* calleeEntryNode = icfg->getFunEntryICFGNode(callee);
            FunExitICFGNode* calleeExitNode = icfg->getFunExitICFGNode(callee);
            icfg->addCallEdge(callICFGNode, calleeEntryNode, cs);
            icfg->addRetEdge(calleeExitNode, retBlockNode, cs);
        }
    }
    /// indirect call (don't know callee)
    else
    {
        icfg->addIntraEdge(callICFGNode, retBlockNode);
    }
}
/*
* Add the global initialization statements immediately after the function entry of main
*/
void ICFGBuilder::connectGlobalToProgEntry(SVFModule* svfModule)
{
    const SVFFunction* mainFunc = SVFUtil::getProgEntryFunction(svfModule);

    /// Return back if the main function is not found, the bc file might be a library only
    if(mainFunc == nullptr)
        return;

    FunEntryICFGNode* entryNode = icfg->getFunEntryICFGNode(mainFunc);
    GlobalICFGNode* globalNode = icfg->getGlobalICFGNode();
    IntraCFGEdge* intraEdge = new IntraCFGEdge(globalNode, entryNode);
    icfg->addICFGEdge(intraEdge);
}

