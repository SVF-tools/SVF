//===- LockAnalysis.cpp -- Analysis of locksets-------------//
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
 * LocksetAnalysis.cpp
 *
 *  Created on: 26 Aug 2015
 *      Author: pengd
 */

#include "Util/Options.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTA.h"
#include "Util/SVFUtil.h"
#include "Util/PTAStat.h"


using namespace SVF;
using namespace SVFUtil;


void LockAnalysis::analyze()
{

    collectLockUnlocksites();
    buildCandidateFuncSetforLock();

    DOTIMESTAT(double lockStart = PTAStat::getClk(true));

    DBOUT(DGENERAL, outs() << "\tIntra-procedural LockAnalysis\n");
    DBOUT(DMTA, outs() << "\tIntra-procedural LockAnalysis\n");
    analyzeIntraProcedualLock();

    DBOUT(DGENERAL, outs() << "\tCollect context-sensitive locks\n");
    DBOUT(DMTA, outs() << "\tCollect context-sensitive locks\n");
    collectCxtLock();

    DBOUT(DGENERAL, outs() << "\tInter-procedural LockAnalysis\n");
    DBOUT(DMTA, outs() << "\tInter-procedural LockAnalysis\n");
    analyzeLockSpanCxtStmt();

    DOTIMESTAT(double lockEnd = PTAStat::getClk(true));
    DOTIMESTAT(lockTime += (lockEnd - lockStart) / TIMEINTERVAL);
}


/*!
 * Collect lock/unlock sites
 */
void LockAnalysis::collectLockUnlocksites()
{
    ThreadCallGraph* tcg=tct->getThreadCallGraph();

    for (const auto& item : *PAG::getPAG()->getCallGraph())
    {
        const FunObjVar* F = item.second->getFunction();
        for (auto it : *F)
        {
            const SVFBasicBlock* bb = it.second;
            for (const ICFGNode* icfgNode : bb->getICFGNodeList())
            {
                if (isa<CallICFGNode>(icfgNode) && tcg->getThreadAPI()->isTDRelease(cast<CallICFGNode>(icfgNode)))
                {
                    unlocksites.insert(icfgNode);
                }
                if (isa<CallICFGNode>(icfgNode) && tcg->getThreadAPI()->isTDAcquire(cast<CallICFGNode>(icfgNode)))
                {
                    locksites.insert(icfgNode);
                }
            }
        }
    }
}

/*!
 * Collect candidate functions for context-sensitive lock analysis
 */
void LockAnalysis::buildCandidateFuncSetforLock()
{

    ThreadCallGraph* tcg=tct->getThreadCallGraph();

    TCT::PTACGNodeSet visited;
    FIFOWorkList<const CallGraphNode*> worklist;

    for (InstSet::iterator it = locksites.begin(), eit = locksites.end(); it != eit; ++it)
    {
        const FunObjVar* fun=(*it)->getFun();
        CallGraphNode* cgnode = tcg->getCallGraphNode(fun);
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }
    for (InstSet::iterator it = unlocksites.begin(), eit = unlocksites.end(); it != eit; ++it)
    {
        const FunObjVar* fun = (*it)->getFun();
        CallGraphNode* cgnode = tcg->getCallGraphNode(fun);
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }
    while (!worklist.empty())
    {
        const CallGraphNode* node = worklist.pop();
        lockcandidateFuncSet.insert(node->getFunction());
        for (CallGraphNode::const_iterator nit = node->InEdgeBegin(), neit = node->InEdgeEnd(); nit != neit; nit++)
        {
            const CallGraphNode* srcNode = (*nit)->getSrcNode();
            if (visited.find(srcNode) == visited.end())
            {
                visited.insert(srcNode);
                worklist.push(srcNode);
            }
        }
    }
}

/*!
 * Analyze intraprocedural locks
 * A lock is intraprocedural if its lock span is within a procedural
 */
void LockAnalysis::analyzeIntraProcedualLock()
{

    // Identify the protected Instructions.
    for (InstSet::const_iterator it = locksites.begin(), ie = locksites.end(); it != ie; ++it)
    {
        const ICFGNode* lockSite = *it;
        assert(isCallSite(lockSite) && "Lock acquire instruction must be a CallSite");

        // Perform forward traversal
        InstSet forwardInsts;
        InstSet backwardInsts;
        InstSet unlockSet;

        bool forward = intraForwardTraverse(lockSite,unlockSet,forwardInsts);
        bool backward =	intraBackwardTraverse(unlockSet,backwardInsts);

        /// FIXME:Should we intersect forwardInsts and backwardInsts?
        if(forward && backward)
            addIntraLock(lockSite,forwardInsts);
        else if(forward && !backward)
            addCondIntraLock(lockSite,forwardInsts);
    }
}

/*!
 * Intra-procedural forward traversal
 */
bool LockAnalysis::intraForwardTraverse(const ICFGNode* lockSite, InstSet& unlockSet, InstSet& forwardInsts)
{

    const FunObjVar* svfFun = lockSite->getFun();

    InstVec worklist;
    worklist.push_back(lockSite);
    while (!worklist.empty())
    {
        const ICFGNode *I = worklist.back();
        worklist.pop_back();
        const ICFGNode* exitInst = svfFun->getExitBB()->back();
        if(exitInst == I)
            return false;

        // Skip the visited Instructions.
        if (forwardInsts.find(I)!=forwardInsts.end())
            continue;
        forwardInsts.insert(I);

        if (isTDRelease(I) && isAliasedLocks(lockSite, I))
        {
            unlockSet.insert(I);
            DBOUT(DMTA, outs() << "LockAnalysis ci lock   -- " << lockSite->getSourceLoc()<<"\n");
            DBOUT(DMTA, outs() << "LockAnalysis ci unlock -- " << I->getSourceLoc()<<"\n");
            continue;
        }

        for(const ICFGEdge* outEdge : I->getOutEdges())
        {
            if(outEdge->getDstNode()->getFun() == I->getFun())
            {
                worklist.push_back(outEdge->getDstNode());
            }
        }
    }

    return true;
}


/*!
 * Intra-procedural backward traversal
 */
bool LockAnalysis::intraBackwardTraverse(const InstSet& unlockSet, InstSet& backwardInsts)
{

    InstVec worklist;
    for(InstSet::const_iterator it = unlockSet.begin(), eit = unlockSet.end(); it!=eit; ++it)
    {
        const ICFGNode* unlockSite = *it;
        const ICFGNode* entryInst = unlockSite->getFun()->getEntryBlock()->back();
        worklist.push_back(*it);

        while (!worklist.empty())
        {
            const ICFGNode *I = worklist.back();
            worklist.pop_back();

            if(entryInst == I)
                return false;

            // Skip the visited Instructions.
            if (backwardInsts.find(I)!=backwardInsts.end())
                continue;
            backwardInsts.insert(I);

            if (isTDAcquire(I) && isAliasedLocks(unlockSite, I))
            {
                DBOUT(DMTA, outs() << "LockAnalysis ci lock   -- " << I->getSourceLoc()<<"\n");
                DBOUT(DMTA, outs() << "LockAnalysis ci unlock -- " << unlockSite->getSourceLoc()<<"\n");
                continue;
            }

            for(const ICFGEdge* inEdge : I->getInEdges())
            {
                if(inEdge->getSrcNode()->getFun() == I->getFun())
                {
                    worklist.push_back(inEdge->getSrcNode());
                }
            }
        }
    }

    return true;
}


void LockAnalysis::collectCxtLock()
{
    FunSet entryFuncSet = tct->getEntryProcs();
    for (FunSet::const_iterator it = entryFuncSet.begin(), eit = entryFuncSet.end(); it != eit; ++it)
    {
        if (!isLockCandidateFun(*it))
            continue;
        CallStrCxt cxt;
        CxtLockProc t(cxt, *it);
        pushToCTPWorkList(t);
    }

    while (!clpList.empty())
    {
        CxtLockProc clp = popFromCTPWorkList();
        CallGraphNode* cgNode = getTCG()->getCallGraphNode(clp.getProc());
        // lzh TODO.
        if (!isLockCandidateFun(cgNode->getFunction()))
            continue;

        for (CallGraphNode::const_iterator nit = cgNode->OutEdgeBegin(), neit = cgNode->OutEdgeEnd(); nit != neit; nit++)
        {
            const CallGraphEdge* cgEdge = (*nit);

            for (CallGraphEdge::CallInstSet::const_iterator cit = cgEdge->directCallsBegin(), ecit = cgEdge->directCallsEnd();
                    cit != ecit; ++cit)
            {
                DBOUT(DMTA,
                      outs() << "\nCollecting CxtLocks: handling direct call:" << **cit << "\t" << cgEdge->getSrcNode()->getFunction()->getName()
                      << "-->" << cgEdge->getDstNode()->getFunction()->getName() << "\n");
                handleCallRelation(clp, cgEdge, *cit);
            }
            for (CallGraphEdge::CallInstSet::const_iterator ind = cgEdge->indirectCallsBegin(), eind = cgEdge->indirectCallsEnd();
                    ind != eind; ++ind)
            {
                DBOUT(DMTA,
                      outs() << "\nCollecting CxtLocks: handling indirect call:" << **ind << "\t"
                      << cgEdge->getSrcNode()->getFunction()->getName() << "-->" << cgEdge->getDstNode()->getFunction()->getName()
                      << "\n");
                handleCallRelation(clp, cgEdge, *ind);
            }
        }
    }
}


/*!
 * Handling call relations when collecting context-sensitive locks
 */
void LockAnalysis::handleCallRelation(CxtLockProc& clp, const CallGraphEdge* cgEdge, const CallICFGNode* cs)
{

    CallStrCxt cxt(clp.getContext());
    const ICFGNode* curNode = cs;
    if (isTDAcquire(curNode))
    {
        addCxtLock(cxt,curNode);
        return;
    }
    const FunObjVar* svfcallee = cgEdge->getDstNode()->getFunction();
    pushCxt(cxt, SVFUtil::cast<CallICFGNode>(curNode), svfcallee);

    CxtLockProc newclp(cxt, svfcallee);
    if (pushToCTPWorkList(newclp))
    {
        DBOUT(DMTA, outs() << "LockAnalysis Process CallRet old clp --"; clp.dump());
        DBOUT(DMTA, outs() << "LockAnalysis Process CallRet new clp --"; newclp.dump());
    }

}

void LockAnalysis::analyzeLockSpanCxtStmt()
{

    FunSet entryFuncSet = tct->getEntryProcs();
    for (FunSet::const_iterator it = entryFuncSet.begin(), eit = entryFuncSet.end(); it != eit; ++it)
    {
        if (!isLockCandidateFun(*it))
            continue;
        CallStrCxt cxt;
        const ICFGNode* frontInst = (*it)->getEntryBlock()->front();
        CxtStmt cxtstmt(cxt, frontInst);
        pushToCTSWorkList(cxtstmt);
    }

    while (!cxtStmtList.empty())
    {
        CxtStmt cts = popFromCTSWorkList();

        touchCxtStmt(cts);
        const ICFGNode* curInst = cts.getStmt();
        instToCxtStmtSet[curInst].insert(cts);

        DBOUT(DMTA, outs() << "\nVisit cxtStmt: ");
        DBOUT(DMTA, cts.dump());

        DBOUT(DMTA, outs() << "\nIts cxt lock sets: ");
        DBOUT(DMTA, printLocks(cts));

        if (isTDFork(curInst))
        {
            handleFork(cts);
        }
        else if (isTDAcquire(curInst))
        {
            assert(hasCxtLock(cts) && "context-sensitive lock not found!!");
            if(addCxtStmtToSpan(cts,cts))
                handleIntra(cts);
        }
        else if (isTDRelease(curInst))
        {
            if(removeCxtStmtToSpan(cts,cts))
                handleIntra(cts);
        }
        else if (isCallSite(curInst) && !isExtCall(curInst))
        {
            handleCall(cts);
        }
        else if (isRetInstNode(curInst))
        {
            handleRet(cts);
        }
        else
        {
            handleIntra(cts);
        }

    }

}


/*!
 * Print context-insensitive and context-sensitive locks
 */
void LockAnalysis::printLocks(const CxtStmt& cts)
{
    const CxtLockSet & lockset = getCxtLockfromCxtStmt(cts);
    outs() << "\nlock sets size = " << lockset.size() << "\n";
    for (CxtLockSet::const_iterator it = lockset.begin(), eit = lockset.end(); it != eit; ++it)
    {
        (*it).dump();
    }
}



/// Handle fork
void LockAnalysis::handleFork(const CxtStmt& cts)
{
    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(cts.getStmt());
    if(getTCG()->hasThreadForkEdge(call))
    {
        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = getTCG()->getForkEdgeBegin(call),
                ecgIt = getTCG()->getForkEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const FunObjVar* svfcallee = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt,call,svfcallee);
            const ICFGNode* svfInst = svfcallee->getEntryBlock()->front();
            CxtStmt newCts(newCxt, svfInst);
            markCxtStmtFlag(newCts, cts);
        }
    }
    handleIntra(cts);
}

/// Handle call
void LockAnalysis::handleCall(const CxtStmt& cts)
{

    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(cts.getStmt());
    if (getTCG()->hasCallGraphEdge(call))
    {
        for (CallGraph::CallGraphEdgeSet::const_iterator cgIt = getTCG()->getCallEdgeBegin(call), ecgIt = getTCG()->getCallEdgeEnd(call);
                cgIt != ecgIt; ++cgIt)
        {
            const FunObjVar* svfcallee = (*cgIt)->getDstNode()->getFunction();
            if (SVFUtil::isExtCall(svfcallee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, svfcallee);
            const ICFGNode* svfInst = svfcallee->getEntryBlock()->front();
            CxtStmt newCts(newCxt, svfInst);
            markCxtStmtFlag(newCts, cts);
        }
    }
}

/// Handle return
void LockAnalysis::handleRet(const CxtStmt& cts)
{

    const ICFGNode* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
    const FunObjVar* svffun = curInst->getFun();
    CallGraphNode* curFunNode = getTCG()->getCallGraphNode(svffun);

    for (CallGraphNode::const_iterator it = curFunNode->getInEdges().begin(), eit = curFunNode->getInEdges().end(); it != eit; ++it)
    {
        CallGraphEdge* edge = *it;
        if (SVFUtil::isa<ThreadForkEdge, ThreadJoinEdge>(edge))
            continue;
        for (CallGraphEdge::CallInstSet::const_iterator cit = (edge)->directCallsBegin(), ecit = (edge)->directCallsEnd(); cit != ecit;
                ++cit)
        {
            CallStrCxt newCxt = curCxt;
            const ICFGNode* inst = *cit;
            if (matchCxt(newCxt, SVFUtil::cast<CallICFGNode>(inst), curFunNode->getFunction()))
            {
                for(const ICFGEdge* outEdge : curInst->getOutEdges())
                {
                    if(outEdge->getDstNode()->getFun() == curInst->getFun())
                    {
                        CxtStmt newCts(newCxt, outEdge->getDstNode());
                        markCxtStmtFlag(newCts, cts);
                    }
                }
            }
        }
        for (CallGraphEdge::CallInstSet::const_iterator cit = (edge)->indirectCallsBegin(), ecit = (edge)->indirectCallsEnd();
                cit != ecit; ++cit)
        {
            CallStrCxt newCxt = curCxt;
            const ICFGNode* inst = *cit;
            if (matchCxt(newCxt, SVFUtil::cast<CallICFGNode>(inst), curFunNode->getFunction()))
            {
                for(const ICFGEdge* outEdge : curInst->getOutEdges())
                {
                    if(outEdge->getDstNode()->getFun() == curInst->getFun())
                    {
                        CxtStmt newCts(newCxt, outEdge->getDstNode());
                        markCxtStmtFlag(newCts, cts);
                    }
                }
            }
        }
    }
}

/// Handle intra
void LockAnalysis::handleIntra(const CxtStmt& cts)
{

    const ICFGNode* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    for(const ICFGEdge* outEdge : curInst->getOutEdges())
    {
        if(outEdge->getDstNode()->getFun() == curInst->getFun())
        {
            CxtStmt newCts(curCxt, outEdge->getDstNode());
            markCxtStmtFlag(newCts, cts);
        }
    }
}


void LockAnalysis::pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    const FunObjVar* svfcaller = call->getFun();
    CallSiteID csId = getTCG()->getCallSiteID(call, callee);

//    /// handle calling context for candidate functions only
//    if (isLockCandidateFun(caller) == false)
//        return;

    if (tct->inSameCallGraphSCC(getTCG()->getCallGraphNode(svfcaller), getTCG()->getCallGraphNode(callee)) == false)
    {
        tct->pushCxt(cxt,csId);
        DBOUT(DMTA, tct->dumpCxt(cxt));
    }
}

bool LockAnalysis::matchCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    const FunObjVar* svfcaller = call->getFun();
    CallSiteID csId = getTCG()->getCallSiteID(call, callee);

//    /// handle calling context for candidate functions only
//    if (isLockCandidateFun(caller) == false)
//        return true;

    /// partial match
    if (cxt.empty())
        return true;

    if (tct->inSameCallGraphSCC(getTCG()->getCallGraphNode(svfcaller), getTCG()->getCallGraphNode(callee)) == false)
    {
        if (cxt.back() == csId)
            cxt.pop_back();
        else
            return false;
        DBOUT(DMTA, tct->dumpCxt(cxt));
    }
    return true;
}


/*!
 * Protected by at least one common lock under every context
 */
bool LockAnalysis::isProtectedByCommonLock(const ICFGNode *i1, const ICFGNode *i2)
{
    numOfTotalQueries++;
    bool commonlock = false;
    DOTIMESTAT(double queryStart = PTAStat::getClk(true));
    if (isInsideIntraLock(i1) && isInsideIntraLock(i2))
        commonlock = isProtectedByCommonCILock(i1,i2) ;
    else
        commonlock = isProtectedByCommonCxtLock(i1,i2);
    DOTIMESTAT(double queryEnd = PTAStat::getClk(true));
    DOTIMESTAT(lockQueriesTime += (queryEnd - queryStart) / TIMEINTERVAL);
    return commonlock;
}

/*!
 * Protected by at least one common context-insensitive lock
 */
bool LockAnalysis::isProtectedByCommonCILock(const ICFGNode *i1, const ICFGNode *i2)
{

    if(!isInsideCondIntraLock(i1) && !isInsideCondIntraLock(i2))
    {
        const InstSet& lockset1 = getIntraLockSet(i1);
        const InstSet& lockset2 = getIntraLockSet(i2);
        for (InstSet::const_iterator cil1 = lockset1.begin(), ecil1 = lockset1.end(); cil1!=ecil1; ++cil1)
        {
            for (InstSet::const_iterator cil2=lockset2.begin(), ecil2=lockset2.end(); cil2!=ecil2; ++cil2)
            {
                if (isAliasedLocks(*cil1, *cil2))
                    return true;
            }
        }
    }
    return false;
}

/*!
 * Protected by at least one common context-sensitive lock
 */
bool LockAnalysis::isProtectedByCommonCxtLock(const CxtStmt& cxtStmt1, const CxtStmt& cxtStmt2)
{
    if(!hasCxtLockfromCxtStmt(cxtStmt1) || !hasCxtLockfromCxtStmt(cxtStmt2))
        return true;
    const CxtLockSet& lockset1 = getCxtLockfromCxtStmt(cxtStmt1);
    const CxtLockSet& lockset2 = getCxtLockfromCxtStmt(cxtStmt2);
    return alias(lockset1,lockset2);
}

/*!
 * Protected by at least one common context-sensitive lock under each context
 */
bool LockAnalysis::isProtectedByCommonCxtLock(const ICFGNode *i1, const ICFGNode *i2)
{
    if(!hasCxtStmtfromInst(i1) || !hasCxtStmtfromInst(i2))
        return false;
    const CxtStmtSet& ctsset1 = getCxtStmtfromInst(i1);
    const CxtStmtSet& ctsset2 = getCxtStmtfromInst(i2);
    for (CxtStmtSet::const_iterator cts1 = ctsset1.begin(), ects1 = ctsset1.end(); cts1 != ects1; cts1++)
    {
        const CxtStmt& cxtStmt1 = *cts1;
        for (CxtStmtSet::const_iterator cts2 = ctsset2.begin(), ects2 = ctsset2.end(); cts2 != ects2; cts2++)
        {
            const CxtStmt& cxtStmt2 = *cts2;
            if(cxtStmt1==cxtStmt2) continue;
            if(isProtectedByCommonCxtLock(cxtStmt1,cxtStmt2)==false)
                return false;
        }
    }
    return true;
}


/*!
 * Return true if two instructions are inside at least one common lock span
 */
bool LockAnalysis::isInSameSpan(const ICFGNode *i1, const ICFGNode *i2)
{
    DOTIMESTAT(double queryStart = PTAStat::getClk(true));

    bool sameSpan = false;
    if (isInsideIntraLock(i1) && isInsideIntraLock(i2))
        sameSpan = isInSameCISpan(i1, i2);
    else
        sameSpan = isInSameCSSpan(i1, i2);

    DOTIMESTAT(double queryEnd = PTAStat::getClk(true));
    DOTIMESTAT(lockQueriesTime += (queryEnd - queryStart) / TIMEINTERVAL);
    return sameSpan;
}

/*!
 * Return true if two instructions are inside same context-insensitive lock span
 */
bool LockAnalysis::isInSameCISpan(const ICFGNode *i1, const ICFGNode *i2) const
{
    if(!isInsideCondIntraLock(i1) && !isInsideCondIntraLock(i2))
    {
        const InstSet& lockset1 = getIntraLockSet(i1);
        const InstSet& lockset2 = getIntraLockSet(i2);
        for (InstSet::const_iterator cil1 = lockset1.begin(), ecil1 = lockset1.end(); cil1!=ecil1; ++cil1)
        {
            for (InstSet::const_iterator cil2=lockset2.begin(), ecil2=lockset2.end(); cil2!=ecil2; ++cil2)
            {
                if (*cil1==*cil2)
                    return true;
            }
        }
    }
    return false;
}

/*!
 * Return true if two context-sensitive instructions are inside same context-insensitive lock spa
 */
bool LockAnalysis::isInSameCSSpan(const CxtStmt& cxtStmt1, const CxtStmt& cxtStmt2) const
{
    if(!hasCxtLockfromCxtStmt(cxtStmt1) || !hasCxtLockfromCxtStmt(cxtStmt2))
        return true;
    const CxtLockSet& lockset1 = getCxtLockfromCxtStmt(cxtStmt1);
    const CxtLockSet& lockset2 = getCxtLockfromCxtStmt(cxtStmt2);
    return intersects(lockset1,lockset2);
}
/*!
 * Return true if two instructions are inside at least one common context-sensitive lock span
 */
bool LockAnalysis::isInSameCSSpan(const ICFGNode *I1, const ICFGNode *I2) const
{
    if(!hasCxtStmtfromInst(I1) || !hasCxtStmtfromInst(I2))
        return false;
    const CxtStmtSet& ctsset1 = getCxtStmtfromInst(I1);
    const CxtStmtSet& ctsset2 = getCxtStmtfromInst(I2);

    for (CxtStmtSet::const_iterator cts1 = ctsset1.begin(), ects1 = ctsset1.end(); cts1 != ects1; cts1++)
    {
        const CxtStmt& cxtStmt1 = *cts1;
        for (CxtStmtSet::const_iterator cts2 = ctsset2.begin(), ects2 = ctsset2.end(); cts2 != ects2; cts2++)
        {
            const CxtStmt& cxtStmt2 = *cts2;
            if(cxtStmt1==cxtStmt2) continue;
            if(isInSameCSSpan(cxtStmt1,cxtStmt2)==false)
                return false;
        }
    }
    return true;
}
