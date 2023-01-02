//===- PathAllocator.cpp -- Path condition analysis---------------------------//
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
 * PathAllocator.cpp
 *
 *  Created on: Apr 3, 2014
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SABER/SaberCondAllocator.h"
#include "Util/DPItem.h"
#include "Graphs/SVFG.h"
#include <climits>
#include <cmath>
#include "SVFIR/SVFStatements.h"

using namespace SVF;
using namespace SVFUtil;

u64_t DPItem::maximumBudget = ULONG_MAX - 1;
u32_t ContextCond::maximumCxtLen = 0;
u32_t ContextCond::maximumCxt = 0;
u32_t ContextCond::maximumPathLen = 0;
u32_t ContextCond::maximumPath = 0;
u32_t SaberCondAllocator::totalCondNum = 0;


SaberCondAllocator::SaberCondAllocator()
{

}

/*!
 * Allocate path condition for each branch
 */
void SaberCondAllocator::allocate(const SVFModule *M)
{
    DBOUT(DGENERAL, outs() << pasMsg("path condition allocation starts\n"));

    for (const auto &func: *M)
    {
        if (!SVFUtil::isExtCall(func))
        {
            // Allocate conditions for a program.
            for (SVFFunction::const_iterator bit = func->begin(), ebit = func->end();
                    bit != ebit; ++bit)
            {
                const SVFBasicBlock* bb = *bit;
                collectBBCallingProgExit(*bb);
                allocateForBB(*bb);
            }
        }
    }

    if (Options::PrintPathCond())
        printPathCond();

    DBOUT(DGENERAL, outs() << pasMsg("path condition allocation ends\n"));
}

/*!
 * Allocate conditions for a basic block and propagate its condition to its successors.
 */
void SaberCondAllocator::allocateForBB(const SVFBasicBlock &bb)
{

    u32_t succ_number = bb.getNumSuccessors();

    // if successor number greater than 1, allocate new decision variable for successors
    if (succ_number > 1)
    {

        //allocate log2(num_succ) decision variables
        double num = log(succ_number) / log(2);
        u32_t bit_num = (u32_t) ceil(num);
        u32_t succ_index = 0;
        std::vector<Condition> condVec;
        for (u32_t i = 0; i < bit_num; i++)
        {
            const SVFInstruction* svfInst = bb.getTerminator();
            condVec.push_back(newCond(svfInst));
        }

        // iterate each successor
        for (const SVFBasicBlock* svf_succ_bb : bb.getSuccessors())
        {
            Condition path_cond = getTrueCond();

            ///TODO: handle BranchInst and SwitchInst individually here!!

            // for each successor decide its bit representation
            // decide whether each bit of succ_index is 1 or 0, if (three successor) succ_index is 000 then use C1^C2^C3
            // if 001 use C1^C2^negC3
            for (u32_t j = 0; j < bit_num; j++)
            {
                //test each bit of this successor's index (binary representation)
                u32_t tool = 0x01 << j;
                if (tool & succ_index)
                {
                    path_cond = condAnd(path_cond, (condNeg(condVec.at(j))));
                }
                else
                {
                    path_cond = condAnd(path_cond, condVec.at(j));
                }
            }
            setBranchCond(&bb, svf_succ_bb, path_cond);

            succ_index++;
        }

    }
}

/*!
 * Get a branch condition
 */
SaberCondAllocator::Condition SaberCondAllocator::getBranchCond(const SVFBasicBlock* bb, const SVFBasicBlock* succ) const
{
    u32_t pos = bb->getBBSuccessorPos(succ);
    if(bb->getNumSuccessors() == 1)
        return getTrueCond();
    else
    {
        BBCondMap::const_iterator it = bbConds.find(bb);
        assert(it != bbConds.end() && "basic block does not have branch and conditions??");
        CondPosMap::const_iterator cit = it->second.find(pos);
        assert(cit != it->second.end() && "no condition on the branch??");
        return cit->second;
    }
}

SaberCondAllocator::Condition SaberCondAllocator::getEvalBrCond(const SVFBasicBlock* bb, const SVFBasicBlock* succ)
{
    if (getCurEvalSVFGNode() && getCurEvalSVFGNode()->getValue())
        return evaluateBranchCond(bb, succ);
    else
        return getBranchCond(bb, succ);
}

/*!
 * Set a branch condition
 */
void SaberCondAllocator::setBranchCond(const SVFBasicBlock* bb, const SVFBasicBlock* succ, const Condition &cond)
{
    /// we only care about basic blocks have more than one successor
    assert(bb->getNumSuccessors() > 1 && "not more than one successor??");
    u32_t pos = bb->getBBSuccessorPos(succ);
    CondPosMap& condPosMap = bbConds[bb];

    /// FIXME: llvm getNumSuccessors allows duplicated block in the successors, it makes this assertion fail
    /// In this case we may waste a condition allocation, because the overwrite of the previous cond
    //assert(condPosMap.find(pos) == condPosMap.end() && "this branch has already been set ");

    condPosMap[pos] = cond;
}

/*!
 * Evaluate null like expression for source-sink related bug detection in SABER
 */
SaberCondAllocator::Condition
SaberCondAllocator::evaluateTestNullLikeExpr(const BranchStmt *branchStmt, const SVFBasicBlock* succ)
{

    const SVFBasicBlock* succ1 = branchStmt->getSuccessor(0)->getBB();

    if (isTestNullExpr(branchStmt->getCondition()->getValue()))
    {
        // succ is then branch
        if (succ1 == succ)
            return getFalseCond();
        // succ is else branch
        else
            return getTrueCond();
    }
    if (isTestNotNullExpr(branchStmt->getCondition()->getValue()))
    {
        // succ is then branch
        if (succ1 == succ)
            return getTrueCond();
        // succ is else branch
        else
            return getFalseCond();
    }
    return Condition::nullExpr();
}

/*!
 * Evaluate condition for program exit (e.g., exit(0))
 */
SaberCondAllocator::Condition SaberCondAllocator::evaluateProgExit(const BranchStmt *branchStmt, const SVFBasicBlock* succ)
{
    const SVFBasicBlock* succ1 = branchStmt->getSuccessor(0)->getBB();
    const SVFBasicBlock* succ2 = branchStmt->getSuccessor(1)->getBB();

    bool branch1 = isBBCallsProgExit(succ1);
    bool branch2 = isBBCallsProgExit(succ2);

    /// then branch calls program exit
    if (branch1 && !branch2)
    {
        // succ is then branch
        if (succ1 == succ)
            return getFalseCond();
        // succ is else branch
        else
            return getTrueCond();
    }
    /// else branch calls program exit
    else if (!branch1 && branch2)
    {
        // succ is else branch
        if (succ2 == succ)
            return getFalseCond();
        // succ is then branch
        else
            return getTrueCond();
    }
    // two branches both call program exit
    else if (branch1 && branch2)
    {
        return getFalseCond();
    }
    /// no branch call program exit
    else
        return Condition::nullExpr();

}

/*!
 * Evaluate loop exit branch to be true if
 * bb is loop header and succ is the only exit basic block outside the loop (excluding exit bbs which call program exit)
 * for all other case, we conservatively evaluate false for now
 */
SaberCondAllocator::Condition SaberCondAllocator::evaluateLoopExitBranch(const SVFBasicBlock* bb, const SVFBasicBlock* dst)
{
    const SVFFunction* svffun = bb->getParent();
    assert(svffun == dst->getParent() && "two basic blocks should be in the same function");

    if (svffun->isLoopHeader(bb))
    {
        Set<const SVFBasicBlock* > filteredbbs;
        std::vector<const SVFBasicBlock*> exitbbs;
        svffun->getExitBlocksOfLoop(bb,exitbbs);
        /// exclude exit bb which calls program exit
        for(const SVFBasicBlock* eb : exitbbs)
        {
            if(!isBBCallsProgExit(eb))
                filteredbbs.insert(eb);
        }

        /// if the dst dominate all other loop exit bbs, then dst can certainly be reached
        bool allPDT = true;
        for (const auto &filteredbb: filteredbbs)
        {
            if (!postDominate(dst, filteredbb))
                allPDT = false;
        }

        if (allPDT)
            return getTrueCond();
    }
    return Condition::nullExpr();
}

/*!
 *  (1) Evaluate a branch when it reaches a program exit
 *  (2) Evaluate a branch when it is loop exit branch
 *  (3) Evaluate a branch when it is a test null like condition
 */
SaberCondAllocator::Condition SaberCondAllocator::evaluateBranchCond(const SVFBasicBlock* bb, const SVFBasicBlock* succ)
{
    if(bb->getNumSuccessors() == 1)
    {
        return getTrueCond();
    }

    const SVFInstruction* svfInst = bb->getTerminator();
    if (ICFGNode *icfgNode = getICFG()->getICFGNode(svfInst))
    {
        for (const auto &svfStmt: icfgNode->getSVFStmts())
        {
            if (const BranchStmt *branchStmt = SVFUtil::dyn_cast<BranchStmt>(svfStmt))
            {
                if (branchStmt->getNumSuccessors() == 2)
                {
                    const SVFBasicBlock* succ1 = branchStmt->getSuccessor(0)->getBB();
                    const SVFBasicBlock* succ2 = branchStmt->getSuccessor(1)->getBB();
                    bool is_succ = (succ1 == succ || succ2 == succ);
                    (void)is_succ; // Suppress warning of unused variable under release build
                    assert(is_succ && "not a successor??");
                    Condition evalLoopExit = evaluateLoopExitBranch(bb, succ);
                    if (!eq(evalLoopExit, Condition::nullExpr()))
                        return evalLoopExit;

                    Condition evalProgExit = evaluateProgExit(branchStmt, succ);
                    if (!eq(evalProgExit, Condition::nullExpr()))
                        return evalProgExit;

                    Condition evalTestNullLike = evaluateTestNullLikeExpr(branchStmt, succ);
                    if (!eq(evalTestNullLike, Condition::nullExpr()))
                        return evalTestNullLike;
                    break;
                }
            }
        }
    }

    return getBranchCond(bb, succ);
}

bool SaberCondAllocator::isEQCmp(const CmpStmt *cmp) const
{
    return (cmp->getPredicate() == CmpStmt::ICMP_EQ);
}

bool SaberCondAllocator::isNECmp(const CmpStmt *cmp) const
{
    return (cmp->getPredicate() == CmpStmt::ICMP_NE);
}

bool SaberCondAllocator::isTestNullExpr(const SVFValue* test) const
{
    if(const SVFInstruction* svfInst = SVFUtil::dyn_cast<SVFInstruction>(test))
    {
        for(const SVFStmt* stmt : PAG::getPAG()->getSVFStmtList(getICFG()->getICFGNode(svfInst)))
        {
            if(const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
            {
                return isTestContainsNullAndTheValue(cmp) && isEQCmp(cmp);
            }
        }
    }
    return false;
}

bool SaberCondAllocator::isTestNotNullExpr(const SVFValue* test) const
{
    if(const SVFInstruction* svfInst = SVFUtil::dyn_cast<SVFInstruction>(test))
    {
        for(const SVFStmt* stmt : PAG::getPAG()->getSVFStmtList(getICFG()->getICFGNode(svfInst)))
        {
            if(const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
            {
                return isTestContainsNullAndTheValue(cmp) && isNECmp(cmp);
            }
        }
    }
    return false;
}

/*!
 * Return true if:
 * (1) cmp contains a null value
 * (2) there is an indirect/direct edge from cur evaluated SVFG node to cmp operand
 *
 * e.g.,
 * indirect edge:
 *      cur svfg node -> 1. store i32* %0, i32** %p, align 8, !dbg !157
 *      cmp operand   -> 2. %1 = load i32*, i32** %p, align 8, !dbg !159
 *                       3. %tobool = icmp ne i32* %1, null, !dbg !159
 *                       4. br i1 %tobool, label %if.end, label %if.then, !dbg !161
 *     There is an indirect edge 1->2 with value %0
 *
 * direct edge:
 *      cur svfg node -> 1. %3 = tail call i8* @malloc(i64 16), !dbg !22
 *      (cmp operand)    2. %4 = icmp eq i8* %3, null, !dbg !28
 *                       3. br i1 %4, label %7, label %5, !dbg !30
 *     There is an direct edge 1->2 with value %3
 *
 */
bool SaberCondAllocator::isTestContainsNullAndTheValue(const CmpStmt *cmp) const
{

    const SVFValue* op0 = cmp->getOpVar(0)->getValue();
    const SVFValue* op1 = cmp->getOpVar(1)->getValue();
    if (SVFUtil::isa<SVFConstantNullPtr>(op1))
    {
        Set<const SVFValue* > inDirVal;
        inDirVal.insert(getCurEvalSVFGNode()->getValue());
        for (const auto &it: getCurEvalSVFGNode()->getOutEdges())
        {
            inDirVal.insert(it->getDstNode()->getValue());
        }
        return inDirVal.find(op0) != inDirVal.end();
    }
    else if (SVFUtil::isa<SVFConstantNullPtr>(op0))
    {
        Set<const SVFValue* > inDirVal;
        inDirVal.insert(getCurEvalSVFGNode()->getValue());
        for (const auto &it: getCurEvalSVFGNode()->getOutEdges())
        {
            inDirVal.insert(it->getDstNode()->getValue());
        }
        return inDirVal.find(op1) != inDirVal.end();
    }
    return false;
}

/*!
 * Whether this basic block contains program exit function call
 */
void SaberCondAllocator::collectBBCallingProgExit(const SVFBasicBlock &bb)
{

    for (SVFBasicBlock::const_iterator it = bb.begin(), eit = bb.end(); it != eit; it++)
    {
        const SVFInstruction* svfInst = *it;
        if (SVFUtil::isCallSite(svfInst))
            if (SVFUtil::isProgExitCall(svfInst))
            {
                const SVFFunction* svfun = bb.getParent();
                funToExitBBsMap[svfun].insert(&bb);
            }
    }
}

/*!
 * Whether this basic block contains program exit function call
 */
bool SaberCondAllocator::isBBCallsProgExit(const SVFBasicBlock* bb)
{
    const SVFFunction* svfun = bb->getParent();
    FunToExitBBsMap::const_iterator it = funToExitBBsMap.find(svfun);
    if (it != funToExitBBsMap.end())
    {
        for (const auto &bit: it->second)
        {
            if (postDominate(bit, bb))
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
SaberCondAllocator::Condition
SaberCondAllocator::getPHIComplementCond(const SVFBasicBlock* BB1, const SVFBasicBlock* BB2, const SVFBasicBlock* BB0)
{
    assert(BB1 && BB2 && "expect nullptr BB here!");

    /// avoid both BB0 and BB1 dominate BB2 (e.g., while loop), then BB2 is not necessaryly a complement BB
    if (dominate(BB1, BB2) && ! dominate(BB0, BB2))
    {
        Condition cond = ComputeIntraVFGGuard(BB1, BB2);
        return condNeg(cond);
    }

    return getTrueCond();
}

/*!
 * Compute calling inter-procedural guards between two SVFGNodes (from caller to callee)
 * src --c1--> callBB --true--> funEntryBB --c2--> dst
 * the InterCallVFGGuard is c1 ^ c2
 */
SaberCondAllocator::Condition
SaberCondAllocator::ComputeInterCallVFGGuard(const SVFBasicBlock* srcBB, const SVFBasicBlock* dstBB,
        const SVFBasicBlock* callBB)
{
    const SVFBasicBlock* funEntryBB = dstBB->getParent()->getEntryBlock();

    Condition c1 = ComputeIntraVFGGuard(srcBB, callBB);
    setCFCond(funEntryBB, condOr(getCFCond(funEntryBB), getCFCond(callBB)));
    Condition c2 = ComputeIntraVFGGuard(funEntryBB, dstBB);
    return condAnd(c1, c2);
}

/*!
 * Compute return inter-procedural guards between two SVFGNodes (from callee to caller)
 * src --c1--> funExitBB --true--> retBB --c2--> dst
 * the InterRetVFGGuard is c1 ^ c2
 */
SaberCondAllocator::Condition
SaberCondAllocator::ComputeInterRetVFGGuard(const SVFBasicBlock* srcBB, const SVFBasicBlock* dstBB, const SVFBasicBlock* retBB)
{
    const SVFFunction* parent = srcBB->getParent();
    const SVFBasicBlock* funExitBB = parent->getExitBB();

    Condition c1 = ComputeIntraVFGGuard(srcBB, funExitBB);
    setCFCond(retBB, condOr(getCFCond(retBB), getCFCond(funExitBB)));
    Condition c2 = ComputeIntraVFGGuard(retBB, dstBB);
    return condAnd(c1, c2);
}

/*!
 * Compute intra-procedural guards between two SVFGNodes (inside same function)
 */
SaberCondAllocator::Condition SaberCondAllocator::ComputeIntraVFGGuard(const SVFBasicBlock* srcBB, const SVFBasicBlock* dstBB)
{

    assert(srcBB->getParent() == dstBB->getParent() && "two basic blocks are not in the same function??");

    if (postDominate(dstBB, srcBB))
        return getTrueCond();

    CFWorkList worklist;
    worklist.push(srcBB);
    setCFCond(srcBB, getTrueCond());

    while (!worklist.empty())
    {
        const SVFBasicBlock* bb = worklist.pop();
        Condition cond = getCFCond(bb);

        /// if the dstBB is the eligible loop exit of the current basic block
        /// we can early terminate the computation
        Condition loopExitCond = evaluateLoopExitBranch(bb, dstBB);
        if (!eq(loopExitCond, Condition::nullExpr()))
            return condAnd(cond, loopExitCond);

        for (const SVFBasicBlock* succ : bb->getSuccessors())
        {
            /// calculate the branch condition
            /// if succ post dominate bb, then we get brCond quicker by using postDT
            /// note that we assume loop exit always post dominate loop bodys
            /// which means loops are approximated only once.
            Condition brCond;
            if (postDominate(succ, bb))
                brCond = getTrueCond();
            else
                brCond = getEvalBrCond(bb, succ);

            DBOUT(DSaber, outs() << " bb (" << bb->getName() <<
                  ") --> " << "succ_bb (" << succ->getName() << ") condition: " << brCond << "\n");
            Condition succPathCond = condAnd(cond, brCond);
            if (setCFCond(succ, condOr(getCFCond(succ), succPathCond)))
                worklist.push(succ);
        }
    }

    DBOUT(DSaber, outs() << " src_bb (" << srcBB->getName() <<
          ") --> " << "dst_bb (" << dstBB->getName() << ") condition: " << getCFCond(dstBB)
          << "\n");

    return getCFCond(dstBB);
}


/*!
 * Print path conditions
 */
void SaberCondAllocator::printPathCond()
{

    outs() << "print path condition\n";

    for (const auto &bbCond: bbConds)
    {
        const SVFBasicBlock* bb = bbCond.first;
        for (const auto &cit: bbCond.second)
        {
            u32_t i = 0;
            for (const SVFBasicBlock* succ: bb->getSuccessors())
            {
                if (i == cit.first)
                {
                    Condition cond = cit.second;
                    outs() << bb->getName() << "-->" << succ->getName() << ":";
                    outs() << dumpCond(cond) << "\n";
                    break;
                }
                i++;
            }
        }
    }
}

/// Allocate a new condition
SaberCondAllocator::Condition SaberCondAllocator::newCond(const SVFInstruction* inst)
{
    u32_t condCountIdx = totalCondNum++;
    Condition expr = Condition::getContext().bool_const(("c" + std::to_string(condCountIdx)).c_str());
    Condition negCond = Condition::NEG(expr);
    setCondInst(expr, inst);
    setNegCondInst(negCond, inst);
    conditionVec.push_back(expr);
    conditionVec.push_back(negCond);
    return expr;
}

/// Whether lhs and rhs are equivalent branch conditions
bool SaberCondAllocator::isEquivalentBranchCond(const Condition &lhs,
        const Condition &rhs) const
{
    Condition::getSolver().push();
    Condition::getSolver().add(lhs.getExpr() != rhs.getExpr()); /// check equal using z3 solver
    z3::check_result res = Condition::getSolver().check();
    Condition::getSolver().pop();
    return res == z3::unsat;
}

/// whether condition is satisfiable
bool SaberCondAllocator::isSatisfiable(const Condition &condition)
{
    Condition::getSolver().add(condition.getExpr());
    z3::check_result result = Condition::getSolver().check();
    Condition::getSolver().pop();
    if (result == z3::sat || result == z3::unknown)
        return true;
    else
        return false;
}

/// extract subexpression from a Z3 expression
void SaberCondAllocator::extractSubConds(const Condition &condition, NodeBS &support) const
{
    if (condition.getExpr().num_args() == 1 && isNegCond(condition.id()))
    {
        support.set(condition.getExpr().id());
        return;
    }
    if (condition.getExpr().num_args() == 0)
        if (!condition.getExpr().is_true() && !condition.getExpr().is_false())
            support.set(condition.getExpr().id());
    for (u32_t i = 0; i < condition.getExpr().num_args(); ++i)
    {
        Condition expr = condition.getExpr().arg(i);
        extractSubConds(expr, support);
    }

}
