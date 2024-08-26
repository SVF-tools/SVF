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

#include "SVF-LLVM/ICFGBuilder.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/LLVMUtil.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Create ICFG nodes and edges
 */
void ICFGBuilder::build(SVFModule* svfModule)
{
    DBOUT(DGENERAL, outs() << pasMsg("\t Building ICFG ...\n"));
    // Add the unique global ICFGNode at the entry of a program (before the main method).
    icfg->addGlobalICFGNode();

    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            const Function *fun = &*F;
            const SVFFunction* svffun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
            if (svffun->isDeclaration())
                continue;
            WorkList worklist;
            processFunEntry(fun,worklist);
            processNoPrecessorBasicBlocks(fun, worklist);
            processFunBody(worklist);
            processFunExit(fun);

            checkICFGNodesVisited(fun);
        }

    }
    connectGlobalToProgEntry(svfModule);
}

void ICFGBuilder::checkICFGNodesVisited(const Function* fun)
{
    for (const auto& bb: *fun)
    {
        for (const auto& inst: bb)
        {
            SVFInstruction* pInstruction =
                LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(
                    &inst);
            if(isIntrinsicInst(pInstruction))
                continue;
            assert(visited.count(&inst) && "inst never visited");
            assert(icfg->hasICFGNode(pInstruction) && "icfgnode not created");
        }
    }
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
        LLVMUtil::getNextInsts(entryInst, insts);
    else
        insts.push_back(entryInst);
    for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
            nit != enit; ++nit)
    {
        visited.insert(*nit);
        ICFGNode* instNode = addBlockICFGNode(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(*nit));           //add interprocedural edge
        worklist.push(*nit);
        icfg->addIntraEdge(FunEntryICFGNode, instNode);
    }



}

/*!
 * bbs with no predecessors
 */
void ICFGBuilder::processNoPrecessorBasicBlocks(const Function* fun, WorkList& worklist)
{
    for (const auto& bb: *fun)
    {
        for (const auto& inst: bb)
        {
            if (LLVMUtil::isNoPrecessorBasicBlock(inst.getParent()) &&
                !visited.count(&inst))
            {
                visited.insert(&inst);
                (void)addBlockICFGNode(LLVMModuleSet::getLLVMModuleSet()
                                           ->getSVFInstruction(&inst));
                worklist.push(&inst);
            }
        }
    }
}

/*!
 * function body
 */
void ICFGBuilder::processFunBody(WorkList& worklist)
{
    /// function body
    while (!worklist.empty())
    {
        const Instruction* inst = worklist.pop();
        const SVFInstruction* svfinst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(inst);
        ICFGNode* srcNode = icfg->getICFGNode(svfinst);
        if (svfinst->isRetInst())
        {
            const SVFFunction* svfFun = svfinst->getFunction();
            FunExitICFGNode* FunExitICFGNode = icfg->getFunExitICFGNode(svfFun);
            icfg->addIntraEdge(srcNode, FunExitICFGNode);
        }
        InstVec nextInsts;
        LLVMUtil::getNextInsts(inst, nextInsts);
        u32_t branchID = 0;
        for (InstVec::const_iterator nit = nextInsts.begin(), enit =
                    nextInsts.end(); nit != enit; ++nit)
        {
            const Instruction* succ = *nit;
            const SVFInstruction* svfsucc = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(succ);
            ICFGNode* dstNode;
            if (visited.find(succ) != visited.end()) {
                dstNode = icfg->getICFGNode(svfsucc);
            }
            else
            {
                visited.insert(succ);
                dstNode = addBlockICFGNode(svfsucc);
                worklist.push(succ);
            }


            if (isNonInstricCallSite(svfinst))
            {
                RetICFGNode* retICFGNode = getRetICFGNode(svfinst);
                srcNode = retICFGNode;
            }

            if (const BranchInst* br = SVFUtil::dyn_cast<BranchInst>(inst))
            {
                assert(branchID <= 1 && "if/else has more than two branches?");
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
            branchID++;
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
        for (const ICFGNode* inst : svfbb->getICFGNodeList())
        {
            if(isRetInstNode(inst))
            {
                icfg->addIntraEdge(const_cast<ICFGNode*>(inst), FunExitICFGNode);
            }
        }
    }
}




/*!
 * (1) Add and get CallBlockICFGNode
 * (2) Handle call instruction by creating interprocedural edges
 */
InterICFGNode* ICFGBuilder::addInterBlockICFGNode(const SVFInstruction* inst)
{
    assert(SVFUtil::isCallSite(inst) && "not a call instruction?");
    assert(SVFUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    CallICFGNode* callICFGNode = icfg->addCallICFGNode(inst);
    (void) icfg->addRetICFGNode(inst);
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

