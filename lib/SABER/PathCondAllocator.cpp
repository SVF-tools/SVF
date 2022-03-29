//===- PathAllocator.cpp -- Path condition analysis---------------------------//
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
 * PathAllocator.cpp
 *
 *  Created on: Apr 3, 2014
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SVF-FE/LLVMUtil.h"
#include "SABER/PathCondAllocator.h"
#include "Util/DPItem.h"
#include "Graphs/SVFG.h"
#include <climits>

using namespace SVF;
using namespace SVFUtil;

u64_t DPItem::maximumBudget = ULONG_MAX - 1;
u32_t ContextCond::maximumCxtLen = 0;
u32_t ContextCond::maximumCxt = 0;
u32_t ContextCond::maximumPathLen = 0;
u32_t ContextCond::maximumPath = 0;


PathCondAllocator::PathCondAllocator() : condMgr(CondManager::getCondMgr(Options::ConditionType)) {

}
/*!
 * Allocate path condition for each branch
 */
void PathCondAllocator::allocate(const SVFModule* M)
{
    DBOUT(DGENERAL,outs() << pasMsg("path condition allocation starts\n"));

    for (const auto& func : *M)
    {
        if (!SVFUtil::isExtCall(func))
        {
            // Allocate conditions for a program.
            for (Function::const_iterator bit = func->getLLVMFun()->begin(), ebit = func->getLLVMFun()->end(); bit != ebit; ++bit)
            {
                const BasicBlock & bb = *bit;
                collectBBCallingProgExit(bb);
                allocateForBB(bb);
            }
        }
    }

    if(Options::PrintPathCond)
        printPathCond();

    DBOUT(DGENERAL,outs() << pasMsg("path condition allocation ends\n"));
}

/*!
 * Allocate conditions for a basic block and propagate its condition to its successors.
 */
void PathCondAllocator::allocateForBB(const BasicBlock & bb)
{

    u32_t succ_number = getBBSuccessorNum(&bb);

    // if successor number greater than 1, allocate new decision variable for successors
    if(succ_number > 1)
    {

        //allocate log2(num_succ) decision variables
        double num = log(succ_number)/log(2);
        u32_t bit_num = (u32_t)ceil(num);
        u32_t succ_index = 0;
        std::vector<Condition*> condVec;
        for(u32_t i = 0 ; i < bit_num; i++)
        {
            condVec.push_back(newCond(bb.getTerminator()));
        }

        // iterate each successor
        for (succ_const_iterator succ_it = succ_begin(&bb);
             succ_it != succ_end(&bb);
             succ_it++, succ_index++)
        {

            const BasicBlock* succ = *succ_it;

            Condition* path_cond = getTrueCond();

            ///TODO: handle BranchInst and SwitchInst individually here!!

            // for each successor decide its bit representation
            // decide whether each bit of succ_index is 1 or 0, if (three successor) succ_index is 000 then use C1^C2^C3
            // if 001 use C1^C2^negC3
            for(u32_t j = 0 ; j < bit_num; j++)
            {
                //test each bit of this successor's index (binary representation)
                u32_t tool = 0x01 << j;
                if(tool & succ_index)
                {
                    path_cond = condAnd(path_cond, (condNeg(condVec.at(j))));
                }
                else
                {
                    path_cond = condAnd(path_cond, condVec.at(j));
                }
            }
            setBranchCond(&bb,succ,path_cond);
        }

    }
}

/*!
 * Get a branch condition
 */
PathCondAllocator::Condition* PathCondAllocator::getBranchCond(const BasicBlock * bb, const BasicBlock *succ) const
{
    u32_t pos = getBBSuccessorPos(bb,succ);
    if(getBBSuccessorNum(bb) == 1)
        return getTrueCond();
    else
    {
        BBCondMap::const_iterator it = bbConds.find(bb);
        assert(it!=bbConds.end() && "basic block does not have branch and conditions??");
        CondPosMap::const_iterator cit = it->second.find(pos);
        assert(cit!=it->second.end() && "no condition on the branch??");
        return cit->second;
    }
}

PathCondAllocator::Condition* PathCondAllocator::getEvalBrCond(const BasicBlock * bb, const BasicBlock *succ)
{
    if(getCurEvalSVFGNode() && getCurEvalSVFGNode()->getValue())
        return evaluateBranchCond(bb, succ);
    else
        return getBranchCond(bb,succ);
}

/*!
 * Set a branch condition
 */
void PathCondAllocator::setBranchCond(const BasicBlock *bb, const BasicBlock *succ, Condition* cond)
{
    /// we only care about basic blocks have more than one successor
    assert(getBBSuccessorNum(bb) > 1 && "not more than one successor??");
    u32_t pos = getBBSuccessorPos(bb,succ);
    CondPosMap& condPosMap = bbConds[bb];

    /// FIXME: llvm getNumSuccessors allows duplicated block in the successors, it makes this assertion fail
    /// In this case we may waste a condition allocation, because the overwrite of the previous cond
    //assert(condPosMap.find(pos) == condPosMap.end() && "this branch has already been set ");

    condPosMap[pos] = cond;
}

/*!
 * Evaluate null like expression for source-sink related bug detection in SABER
 */
PathCondAllocator::Condition* PathCondAllocator::evaluateTestNullLikeExpr(const BranchStmt* branchStmt, const BasicBlock *succ)
{

    const BasicBlock* succ1 = branchStmt->getSuccessor(0)->getBB();

    if(isTestNullExpr(branchStmt->getCondition()->getValue()))
    {
        // succ is then branch
        if(succ1 == succ)
            return getFalseCond();
            // succ is else branch
        else
            return getTrueCond();
    }
    if(isTestNotNullExpr(branchStmt->getCondition()->getValue()))
    {
        // succ is then branch
        if(succ1 == succ)
            return getTrueCond();
            // succ is else branch
        else
            return getFalseCond();
    }

    return nullptr;
}

/*!
 * Evaluate condition for program exit (e.g., exit(0))
 */
PathCondAllocator::Condition* PathCondAllocator::evaluateProgExit(const BranchStmt* branchStmt, const BasicBlock *succ)
{
    const BasicBlock *succ1 = branchStmt->getSuccessor(0)->getBB();
    const BasicBlock* succ2 = branchStmt->getSuccessor(1)->getBB();

    bool branch1 = isBBCallsProgExit(succ1);
    bool branch2 = isBBCallsProgExit(succ2);

    /// then branch calls program exit
    if(branch1 && !branch2)
    {
        // succ is then branch
        if(succ1 == succ)
            return getFalseCond();
            // succ is else branch
        else
            return getTrueCond();
    }
        /// else branch calls program exit
    else if(!branch1 && branch2)
    {
        // succ is else branch
        if(succ2 == succ)
            return getFalseCond();
            // succ is then branch
        else
            return getTrueCond();
    }
        // two branches both call program exit
    else if(branch1 && branch2)
    {
        return getFalseCond();
    }
        /// no branch call program exit
    else
        return nullptr;

}

/*!
 * Evaluate loop exit branch to be true if
 * bb is loop header and succ is the only exit basic block outside the loop (excluding exit bbs which call program exit)
 * for all other case, we conservatively evaluate false for now
 */
PathCondAllocator::Condition* PathCondAllocator::evaluateLoopExitBranch(const BasicBlock * bb, const BasicBlock *dst)
{
    const Function* fun = bb->getParent();
    assert(fun==dst->getParent() && "two basic blocks should be in the same function");

    const LoopInfo* loopInfo = getLoopInfo(fun);
    if(loopInfo->isLoopHeader(const_cast<BasicBlock*>(bb)))
    {
        const Loop *loop = loopInfo->getLoopFor(bb);
        SmallBBVector exitbbs;
        Set<BasicBlock*> filteredbbs;
        loop->getExitBlocks(exitbbs);
        /// exclude exit bb which calls program exit
        while(!exitbbs.empty())
        {
            BasicBlock* eb = exitbbs.pop_back_val();
            if(!isBBCallsProgExit(eb))
                filteredbbs.insert(eb);
        }

        /// if the dst dominate all other loop exit bbs, then dst can certainly be reached
        bool allPDT = true;
        PostDominatorTree* pdt = getPostDT(fun);
        for(const auto& filteredbb : filteredbbs)
        {
            if(!pdt->dominates(dst,filteredbb))
                allPDT =false;
        }

        if(allPDT)
            return getTrueCond();
    }
    return nullptr;
}

/*!
 *  (1) Evaluate a branch when it reaches a program exit
 *  (2) Evaluate a branch when it is loop exit branch
 *  (3) Evaluate a branch when it is a test null like condition
 */
PathCondAllocator::Condition* PathCondAllocator::evaluateBranchCond(const BasicBlock * bb, const BasicBlock *succ)
{
    if(getBBSuccessorNum(bb) == 1)
    {
        assert(bb->getTerminator()->getSuccessor(0) == succ && "not the unique successor?");
        return getTrueCond();
    }

    if (ICFGNode *icfgNode = getICFG()->getICFGNode(bb->getTerminator())) {
        for (const auto &svfStmt: icfgNode->getSVFStmts()) {
            if (const BranchStmt *branchStmt = SVFUtil::dyn_cast<BranchStmt>(svfStmt)) {
                if(branchStmt->getNumSuccessors() == 2) {
                    const BasicBlock* succ1 = branchStmt->getSuccessor(0)->getBB();
                    const BasicBlock* succ2 = branchStmt->getSuccessor(1)->getBB();
                    assert((succ1 == succ || succ2 == succ) && "not a successor??");

                    Condition* evalLoopExit = evaluateLoopExitBranch(bb,succ);
                    if(evalLoopExit)
                        return evalLoopExit;

                    Condition* evalProgExit = evaluateProgExit(branchStmt,succ);
                    if(evalProgExit)
                        return evalProgExit;

                    Condition* evalTestNullLike = evaluateTestNullLikeExpr(branchStmt,succ);
                    if(evalTestNullLike)
                        return evalTestNullLike;
                    break;
                }
            }
        }
    }

    return getBranchCond(bb, succ);
}

bool PathCondAllocator::isEQCmp(const CmpInst* cmp) const
{
    return (cmp->getPredicate() == CmpInst::ICMP_EQ);
}

bool PathCondAllocator::isNECmp(const CmpInst* cmp) const
{
    return (cmp->getPredicate() == CmpInst::ICMP_NE);
}

bool PathCondAllocator::isTestNullExpr(const Value* test) const
{
    if(const CmpInst* cmp = SVFUtil::dyn_cast<CmpInst>(test))
    {
        return isTestContainsNullAndTheValue(cmp) && isEQCmp(cmp);
    }
    return false;
}

bool PathCondAllocator::isTestNotNullExpr(const Value* test) const
{
    if(const CmpInst* cmp = SVFUtil::dyn_cast<CmpInst>(test))
    {
        return isTestContainsNullAndTheValue(cmp) && isNECmp(cmp);
    }
    return false;
}

/*!
 * Return true if:
 * (1) cmp contains a null value
 * (2) there is an indirect edge from cur evaluated SVFG node to cmp operand
 *
 * e.g.,
 *      cur svfg node -> 1. store i32* %0, i32** %p, align 8, !dbg !157
 *      cmp operand   -> 2. %1 = load i32*, i32** %p, align 8, !dbg !159
 *                       3. %tobool = icmp ne i32* %1, null, !dbg !159
 *                       4. br i1 %tobool, label %if.end, label %if.then, !dbg !161
 *     There is an indirect edge 1->2 with value %0
 */
bool PathCondAllocator::isTestContainsNullAndTheValue(const CmpInst* cmp) const
{

    const Value *op0 = cmp->getOperand(0);
    const Value *op1 = cmp->getOperand(1);
    if (SVFUtil::isa<ConstantPointerNull>(op1)) {
        Set<const Value *> inDirVal;
        for (const auto &it: getCurEvalSVFGNode()->getOutEdges()) {
            if (it->isIndirectVFGEdge()) {
                inDirVal.insert(it->getDstNode()->getValue());
            }
        }
        return inDirVal.find(op0) != inDirVal.end();
    } else if (SVFUtil::isa<ConstantPointerNull>(op0)) {
        Set<const Value *> inDirVal;
        for (const auto &it: getCurEvalSVFGNode()->getOutEdges()) {
            if (it->isIndirectVFGEdge()) {
                inDirVal.insert(it->getDstNode()->getValue());
            }
        }
        return inDirVal.find(op1) != inDirVal.end();
    }
    return false;

}

/*!
 * Whether this basic block contains program exit function call
 */
void PathCondAllocator::collectBBCallingProgExit(const BasicBlock & bb)
{

    for(BasicBlock::const_iterator it = bb.begin(), eit = bb.end(); it!=eit; it++)
    {
        const Instruction* inst = &*it;
        if(SVFUtil::isa<CallBase>(inst))
            if(SVFUtil::isProgExitCall(inst))
            {
                funToExitBBsMap[bb.getParent()].insert(&bb);
            }
    }
}

/*!
 * Whether this basic block contains program exit function call
 */
bool PathCondAllocator::isBBCallsProgExit(const BasicBlock* bb)
{
    const Function* fun = bb->getParent();
    FunToExitBBsMap::const_iterator it = funToExitBBsMap.find(fun);
    if(it!=funToExitBBsMap.end())
    {
        PostDominatorTree* pdt = getPostDT(fun);
        for(const auto& bit : it->second)
        {
            if(pdt->dominates(bit,bb))
                return true;
        }
    }
    return false;
}

/*!
 * Get complement phi condition
 * e.g., B0: dstBB; B1:incomingBB; B2:complementBB
 * Assume B0 (phi node) is the successor of both B1 and B2.
 * If B1 dominates B2, and B0 not dominate B2 then condition from B1-->B0 = neg(B1-->B2)^(B1-->B0)
 */
PathCondAllocator::Condition* PathCondAllocator::getPHIComplementCond(const BasicBlock* BB1, const BasicBlock* BB2, const BasicBlock* BB0)
{
    assert(BB1 && BB2 && "expect nullptr BB here!");

    DominatorTree* dt = getDT(BB1->getParent());
    /// avoid both BB0 and BB1 dominate BB2 (e.g., while loop), then BB2 is not necessaryly a complement BB
    if(dt->dominates(BB1,BB2) && !dt->dominates(BB0,BB2))
    {
        Condition* cond =  ComputeIntraVFGGuard(BB1,BB2);
        return condNeg(cond);
    }

    return getTrueCond();
}

/*!
 * Compute calling inter-procedural guards between two SVFGNodes (from caller to callee)
 * src --c1--> callBB --true--> funEntryBB --c2--> dst
 * the InterCallVFGGuard is c1 ^ c2
 */
PathCondAllocator::Condition* PathCondAllocator::ComputeInterCallVFGGuard(const BasicBlock* srcBB, const BasicBlock* dstBB, const BasicBlock* callBB)
{
    const BasicBlock* funEntryBB = &dstBB->getParent()->getEntryBlock();

    Condition* c1 = ComputeIntraVFGGuard(srcBB,callBB);
    setCFCond(funEntryBB,condOr(getCFCond(funEntryBB),getCFCond(callBB)));
    Condition* c2 = ComputeIntraVFGGuard(funEntryBB,dstBB);
    return condAnd(c1,c2);
}

/*!
 * Compute return inter-procedural guards between two SVFGNodes (from callee to caller)
 * src --c1--> funExitBB --true--> retBB --c2--> dst
 * the InterRetVFGGuard is c1 ^ c2
 */
PathCondAllocator::Condition* PathCondAllocator::ComputeInterRetVFGGuard(const BasicBlock*  srcBB, const BasicBlock*  dstBB, const BasicBlock* retBB)
{
    const BasicBlock* funExitBB = getFunExitBB(srcBB->getParent());

    Condition* c1 = ComputeIntraVFGGuard(srcBB,funExitBB);
    setCFCond(retBB,condOr(getCFCond(retBB),getCFCond(funExitBB)));
    Condition* c2 = ComputeIntraVFGGuard(retBB,dstBB);
    return condAnd(c1,c2);
}

/*!
 * Compute intra-procedural guards between two SVFGNodes (inside same function)
 */
PathCondAllocator::Condition* PathCondAllocator::ComputeIntraVFGGuard(const BasicBlock* srcBB, const BasicBlock* dstBB)
{

    assert(srcBB->getParent() == dstBB->getParent() && "two basic blocks are not in the same function??");

    PostDominatorTree* postDT = getPostDT(srcBB->getParent());
    if(postDT->dominates(dstBB,srcBB))
        return getTrueCond();

    CFWorkList worklist;
    worklist.push(srcBB);
    setCFCond(srcBB,getTrueCond());

    while(!worklist.empty())
    {
        const BasicBlock* bb = worklist.pop();
        Condition* cond = getCFCond(bb);

        /// if the dstBB is the eligible loop exit of the current basic block
        /// we can early terminate the computation
        if(Condition* loopExitCond = evaluateLoopExitBranch(bb,dstBB))
            return condAnd(cond, loopExitCond);


        for (succ_const_iterator succ_it = succ_begin(bb);
             succ_it != succ_end(bb); succ_it++)
        {
            const BasicBlock* succ = *succ_it;
            /// calculate the branch condition
            /// if succ post dominate bb, then we get brCond quicker by using postDT
            /// note that we assume loop exit always post dominate loop bodys
            /// which means loops are approximated only once.
            Condition* brCond;
            if(postDT->dominates(succ,bb))
                brCond = getTrueCond();
            else
                brCond = getEvalBrCond(bb, succ);

            DBOUT(DSaber, outs() << " bb (" << bb->getName().str() <<
                                 ") --> " << "succ_bb (" << succ->getName().str() << ") condition: " << brCond << "\n");
            Condition* succPathCond = condAnd(cond, brCond);
            if(setCFCond(succ, condOr(getCFCond(succ), succPathCond)))
                worklist.push(succ);
        }
    }

    DBOUT(DSaber, outs() << " src_bb (" << srcBB->getName().str() <<
                         ") --> " << "dst_bb (" << dstBB->getName().str() << ") condition: " << getCFCond(dstBB) << "\n");

    return getCFCond(dstBB);
}


/*!
 * Print path conditions
 */
void PathCondAllocator::printPathCond()
{

    outs() << "print path condition\n";

    for(const auto & bbCond : bbConds)
    {
        const BasicBlock* bb = bbCond.first;
        for(const auto& cit : bbCond.second)
        {
            u32_t i=0;
            for (const BasicBlock *succ: successors(bb))
            {
                if (i == cit.first)
                {
                    Condition* cond = cit.second;
                    outs() << bb->getName().str() << "-->" << succ->getName().str() << ":";
                    outs() << dumpCond(cond) << "\n";
                    break;
                }
                i++;
            }
        }
    }
}
