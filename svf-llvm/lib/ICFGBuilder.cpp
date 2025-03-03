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
#include "SVF-LLVM/CppUtil.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/LLVMUtil.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * Create ICFG nodes and edges
 */
ICFG* ICFGBuilder::build()
{
    icfg = new ICFG();
    DBOUT(DGENERAL, outs() << pasMsg("\t Building ICFG ...\n"));
    // Add the unique global ICFGNode at the entry of a program (before the main method).
    addGlobalICFGNode();

    // Add function entry and exit
    for (Module &M : llvmModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            const Function *fun = &*F;
            if (fun->isDeclaration())
                continue;
            addFunEntryBlock(fun);
            addFunExitBlock(fun);
        }

    }

    for (Module &M : llvmModuleSet()->getLLVMModules())
    {
        for (Module::const_iterator F = M.begin(), E = M.end(); F != E; ++F)
        {
            const Function *fun = &*F;
            if (fun->isDeclaration())
                continue;
            WorkList worklist;
            processFunEntry(fun,worklist);
            processUnreachableFromEntry(fun, worklist);
            processFunBody(worklist);
            processFunExit(fun);

            checkICFGNodesVisited(fun);
        }

    }
    connectGlobalToProgEntry();
    return icfg;
}

void ICFGBuilder::checkICFGNodesVisited(const Function* fun)
{
    for (const auto& bb: *fun)
    {
        for (const auto& inst: bb)
        {
            if(LLVMUtil::isIntrinsicInst(&inst))
                continue;
            assert(visited.count(&inst) && "inst never visited");
            assert(hasICFGNode(&inst) && "icfgnode not created");
        }
    }
}
/*!
 * function entry
 */
void ICFGBuilder::processFunEntry(const Function*  fun, WorkList& worklist)
{
    FunEntryICFGNode* FunEntryICFGNode = getFunEntryICFGNode(fun);
    const Instruction* entryInst = &((fun->getEntryBlock()).front());

    InstVec insts;
    if (LLVMUtil::isIntrinsicInst(entryInst))
        LLVMUtil::getNextInsts(entryInst, insts);
    else
        insts.push_back(entryInst);
    for (InstVec::const_iterator nit = insts.begin(), enit = insts.end();
            nit != enit; ++nit)
    {
        visited.insert(*nit);
        ICFGNode* instNode = addBlockICFGNode(*nit);           //add interprocedural edge
        worklist.push(*nit);
        icfg->addIntraEdge(FunEntryICFGNode, instNode);
    }



}

/*!
 * bbs unreachable from function entry
 */
void ICFGBuilder::processUnreachableFromEntry(const Function* fun, WorkList& worklist)
{
    SVFLoopAndDomInfo* pInfo =
        llvmModuleSet()->getFunObjVar(fun)->getLoopAndDomInfo();
    for (const auto& bb : *fun)
    {
        if (pInfo->isUnreachable(llvmModuleSet()->getSVFBasicBlock(&bb)) &&
                !visited.count(&bb.front()))
        {
            visited.insert(&bb.front());
            (void)addBlockICFGNode(&bb.front());
            worklist.push(&bb.front());
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
        ICFGNode* srcNode = getICFGNode(inst);
        if (SVFUtil::isa<ReturnInst>(inst))
        {
            FunExitICFGNode* FunExitICFGNode = getFunExitICFGNode(inst->getFunction());
            icfg->addIntraEdge(srcNode, FunExitICFGNode);
        }
        InstVec nextInsts;
        LLVMUtil::getNextInsts(inst, nextInsts);
        s64_t branchID = 0;
        for (InstVec::const_iterator nit = nextInsts.begin(), enit =
                    nextInsts.end(); nit != enit; ++nit)
        {
            const Instruction* succ = *nit;
            ICFGNode* dstNode;
            if (visited.find(succ) != visited.end())
            {
                dstNode = getICFGNode(succ);
            }
            else
            {
                visited.insert(succ);
                dstNode = addBlockICFGNode(succ);
                worklist.push(succ);
            }


            if (LLVMUtil::isNonInstricCallSite(inst))
            {
                RetICFGNode* retICFGNode = getRetICFGNode(inst);
                srcNode = retICFGNode;
            }

            if (const BranchInst* br = SVFUtil::dyn_cast<BranchInst>(inst))
            {
                assert(branchID <= 1 && "if/else has more than two branches?");
                if(br->isConditional())
                    icfg->addConditionalIntraEdge(srcNode, dstNode, 1 - branchID);
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
                    val = LLVMUtil::getIntegerValue(condVal).first;
                icfg->addConditionalIntraEdge(srcNode, dstNode,val);
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
    FunExitICFGNode* FunExitICFGNode = getFunExitICFGNode(f);

    for (const auto& bb : *f)
    {
        for (const auto& inst : bb)
        {
            if (SVFUtil::isa<ReturnInst>(&inst))
            {
                icfg->addIntraEdge(getICFGNode(&inst), FunExitICFGNode);
            }
        }
    }
}




/*!
 * (1) Add and get CallBlockICFGNode
 * (2) Handle call instruction by creating interprocedural edges
 */
InterICFGNode* ICFGBuilder::addInterBlockICFGNode(const Instruction* inst)
{
    assert(LLVMUtil::isCallSite(inst) && "not a call instruction?");
    assert(LLVMUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    assert(llvmModuleSet()->getCallBlock(inst)==nullptr && "duplicate CallICFGNode");
    const CallBase* cb = SVFUtil::dyn_cast<CallBase>(inst);
    bool isvcall = cppUtil::isVirtualCallSite(cb);
    const FunObjVar* calledFunc = nullptr;
    auto called_llvmval = cb->getCalledOperand()->stripPointerCasts();
    if (const Function* called_llvmfunc = SVFUtil::dyn_cast<Function>(called_llvmval))
    {
        calledFunc = llvmModuleSet()->getFunObjVar(called_llvmfunc);
    }
    else
    {
        assert(SVFUtil::dyn_cast<Function>(called_llvmval) == nullptr && "must be nullptr");
    }

    SVFBasicBlock* bb = llvmModuleSet()->getSVFBasicBlock(inst->getParent());

    CallICFGNode* callICFGNode = icfg->addCallICFGNode(
                                     bb, llvmModuleSet()->getSVFType(inst->getType()),
                                     calledFunc, cb->getFunctionType()->isVarArg(), isvcall,
                                     isvcall ? cppUtil::getVCallIdx(cb) : 0,
                                     isvcall ? cppUtil::getFunNameOfVCallSite(cb) : "");
    llvmModuleSet()->addInstructionMap(inst, callICFGNode);

    assert(llvmModuleSet()->getRetBlock(inst)==nullptr && "duplicate RetICFGNode");
    RetICFGNode* retICFGNode = icfg->addRetICFGNode(callICFGNode);
    llvmModuleSet()->addInstructionMap(inst, retICFGNode);

    addICFGInterEdges(inst, LLVMUtil::getCallee(SVFUtil::cast<CallBase>(inst)));    //creating interprocedural edges
    return callICFGNode;
}

/*!
 * Create edges between ICFG nodes across functions
 */
void ICFGBuilder::addICFGInterEdges(const Instruction* cs, const Function* callee)
{

    CallICFGNode* callICFGNode = getCallICFGNode(cs);
    RetICFGNode* retBlockNode = getRetICFGNode(cs);

    /// direct call
    if(callee)
    {
        const FunObjVar* svfFun =
            llvmModuleSet()->getFunObjVar(callee);
        /// if this is an external function (no function body)
        if (SVFUtil::isExtCall(svfFun))
        {
            icfg->addIntraEdge(callICFGNode, retBlockNode);
        }
        /// otherwise connect interprocedural edges
        else
        {
            FunEntryICFGNode* calleeEntryNode = getFunEntryICFGNode(callee);
            FunExitICFGNode* calleeExitNode = getFunExitICFGNode(callee);
            icfg->addCallEdge(callICFGNode, calleeEntryNode);
            icfg->addRetEdge(calleeExitNode, retBlockNode);
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
void ICFGBuilder::connectGlobalToProgEntry()
{
    for (Module &M : llvmModuleSet()->getLLVMModules())
    {
        if (const Function* mainFunc = LLVMUtil::getProgEntryFunction(M))
        {
            // main function
            FunEntryICFGNode* entryNode = getFunEntryICFGNode(mainFunc);
            GlobalICFGNode* globalNode = getGlobalICFGNode();
            IntraCFGEdge* intraEdge = new IntraCFGEdge(globalNode, entryNode);
            icfg->addICFGEdge(intraEdge);
        }
        else
        {
            // not main function
        }
    }
}

inline ICFGNode* ICFGBuilder::addBlockICFGNode(const Instruction* inst)
{
    ICFGNode* node;
    if(LLVMUtil::isNonInstricCallSite(inst))
        node = addInterBlockICFGNode(inst);
    else
        node = addIntraBlockICFGNode(inst);
    const_cast<SVFBasicBlock*>(
        llvmModuleSet()->getSVFBasicBlock(inst->getParent()))
    ->addICFGNode(node);
    return node;
}

IntraICFGNode* ICFGBuilder::addIntraBlockICFGNode(const Instruction* inst)
{
    IntraICFGNode* node = llvmModuleSet()->getIntraBlock(inst);
    assert (node==nullptr && "no IntraICFGNode for this instruction?");
    IntraICFGNode* sNode = icfg->addIntraICFGNode(
                               llvmModuleSet()->getSVFBasicBlock(inst->getParent()), SVFUtil::isa<ReturnInst>(inst));
    llvmModuleSet()->addInstructionMap(inst, sNode);
    return sNode;
}

FunEntryICFGNode* ICFGBuilder::addFunEntryBlock(const Function* fun)
{
    return llvmModuleSet()->FunToFunEntryNodeMap[fun] =
               icfg->addFunEntryICFGNode(llvmModuleSet()->getFunObjVar(fun));
}

inline FunExitICFGNode* ICFGBuilder::addFunExitBlock(const Function* fun)
{
    return llvmModuleSet()->FunToFunExitNodeMap[fun] =
               icfg->addFunExitICFGNode(llvmModuleSet()->getFunObjVar(fun));
}