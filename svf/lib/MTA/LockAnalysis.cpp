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
#include "MTA/MTASlicer.h"
#include "Util/SVFUtil.h"
#include "Util/PTAStat.h"
#include "Graphs/ThreadCallGraph.h"
#include "SVFIR/SVFIR.h"
#include "Graphs/SlicedGraphs.h"

using namespace SVF;
using namespace SVFUtil;



template<class ICFGGraph, class CGGraph>
void LockAnalysis::analyze(ICFGGraph icfg, CGGraph cg)
{
    collectLockUnlockSites(icfg, cg);
    buildCandidateFuncSetForLock(cg);

    DOTIMESTAT(double lockStart = PTAStat::getClk(true));

    DBOUT(DGENERAL, outs() << "\tIntra-procedural LockAnalysis\n");
    DBOUT(DMTA, outs() << "\tIntra-procedural LockAnalysis\n");
    analyzeIntraProceduralLock(icfg);

    DBOUT(DGENERAL, outs() << "\tCollect context-sensitive locks\n");
    DBOUT(DMTA, outs() << "\tCollect context-sensitive locks\n");
    collectCxtLock(icfg, cg);

    DBOUT(DGENERAL, outs() << "\tInter-procedural LockAnalysis\n");
    DBOUT(DMTA, outs() << "\tInter-procedural LockAnalysis\n");
    analyzeLockSpanCxtStmt(icfg, cg);

    DOTIMESTAT(double lockEnd = PTAStat::getClk(true));
    DOTIMESTAT(lockTime += (lockEnd - lockStart) / TIMEINTERVAL);
}


/*!
 * Collect lock/unlock sites
 */
template<class ICFGGraph, class CGGraph>
void LockAnalysis::collectLockUnlockSites(ICFGGraph icfg, CGGraph cg)
{
    ThreadCallGraph* tcg=tct->getThreadCallGraph();

    using CGTraits = GenericGraphTraits<CGGraph>;
    for (auto nodeIt = CGTraits::nodes_begin(cg),
              nodeEnd = CGTraits::nodes_end(cg); nodeIt != nodeEnd; ++nodeIt)
    {
        const FunObjVar* F = CGTraits::getRawNode(*nodeIt)->getFunction();
        for (auto it : *F)
        {
            const SVFBasicBlock* bb = it.second;
            for (const ICFGNode* icfgNode : bb->getICFGNodeList())
            {
                if (!GenericGraphTraits<ICFGGraph>::containsNode(icfg, icfgNode))
                    continue;
                if (isa<CallICFGNode>(icfgNode) && tcg->getThreadAPI()->isTDRelease(cast<CallICFGNode>(icfgNode)))
                {
                    unlockSites.insert(icfgNode);
                }
                if (isa<CallICFGNode>(icfgNode) && tcg->getThreadAPI()->isTDAcquire(cast<CallICFGNode>(icfgNode)))
                {
                    lockSites.insert(icfgNode);
                }
            }
        }
    }
}

/*!
 * Collect candidate functions for context-sensitive lock analysis
 */
template<class CGGraph>
void LockAnalysis::buildCandidateFuncSetForLock(CGGraph cg)
{

    ThreadCallGraph* tcg=tct->getThreadCallGraph();

    TCT::PTACGNodeSet visited;
    FIFOWorkList<const CallGraphNode*> worklist;

    for (InstSet::iterator it = lockSites.begin(), eit = lockSites.end(); it != eit; ++it)
    {
        const FunObjVar* fun=(*it)->getFun();
        CallGraphNode* cgnode = tcg->getCallGraphNode(fun);
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }
    for (InstSet::iterator it = unlockSites.begin(), eit = unlockSites.end(); it != eit; ++it)
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
        lockCandidateFuncSet.insert(node->getFunction());
        std::vector<const CallGraphEdge*> inEdges;
        GenericGraphTraits<CGGraph>::getInEdges(cg, node, inEdges);
        for (const CallGraphEdge* edge : inEdges)
        {
            const CallGraphNode* srcNode = edge->getSrcNode();
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
template<class ICFGGraph>
void LockAnalysis::analyzeIntraProceduralLock(ICFGGraph icfg)
{

    // Identify the protected Instructions.
    for (InstSet::const_iterator it = lockSites.begin(), ie = lockSites.end(); it != ie; ++it)
    {
        const ICFGNode* lockSite = *it;
        assert(isCallSite(lockSite) && "Lock acquire instruction must be a CallSite");

        // Perform forward traversal
        InstSet forwardInsts;
        InstSet backwardInsts;
        InstSet unlockSet;

        bool forward = intraForwardTraverse(
                           icfg, lockSite, unlockSet, forwardInsts);
        bool backward = intraBackwardTraverse(
                            icfg, unlockSet, backwardInsts);

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
template<class ICFGGraph>
bool LockAnalysis::intraForwardTraverse(
    ICFGGraph icfg, const ICFGNode* lockSite, InstSet& unlockSet,
    InstSet& forwardInsts)
{

    const FunObjVar* svfFun = lockSite->getFun();

    InstVec worklist;
    worklist.push_back(lockSite);
    while (!worklist.empty())
    {
        const ICFGNode *I = worklist.back();
        worklist.pop_back();
        const ICFGNode* exitInst =
            GenericGraphTraits<ICFGGraph>::getFunExit(icfg, svfFun);
        if (exitInst == nullptr)
            return false;
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

        std::vector<const ICFGNode*> succ;
        GenericGraphTraits<ICFGGraph>::getSuccNodes(icfg, I, succ);
        for (const ICFGNode* dst : succ)
        {
            if(dst->getFun() == I->getFun())
            {
                worklist.push_back(dst);
            }
        }
    }

    return true;
}


/*!
 * Intra-procedural backward traversal
 */
template<class ICFGGraph>
bool LockAnalysis::intraBackwardTraverse(
    ICFGGraph icfg, const InstSet& unlockSet, InstSet& backwardInsts)
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

            std::vector<const ICFGNode*> pred;
            GenericGraphTraits<ICFGGraph>::getPredNodes(icfg, I, pred);
            for (const ICFGNode* src : pred)
            {
                if(src->getFun() == I->getFun())
                {
                    worklist.push_back(src);
                }
            }
        }
    }

    return true;
}


template<class ICFGGraph, class CGGraph>
void LockAnalysis::collectCxtLock(ICFGGraph icfg, CGGraph cg)
{
    const TCT::FunSet& entryFuncSet = tct->getEntryProcs();
    for (TCT::FunSet::const_iterator it = entryFuncSet.begin(), eit = entryFuncSet.end(); it != eit; ++it)
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
        if (!isLockCandidateFun(cgNode->getFunction()))
            continue;

        std::vector<const CallGraphEdge*> outEdges;
        GenericGraphTraits<CGGraph>::getOutEdges(cg, cgNode, outEdges);
        for (const CallGraphEdge* cgEdge : outEdges)
        {
            std::vector<const CallICFGNode*> directCalls;
            GenericGraphTraits<CGGraph>::getDirectCalls(
                cg, cgEdge, directCalls);
            for (const CallICFGNode* callSite : directCalls)
            {
                DBOUT(DMTA,
                      outs() << "\nCollecting CxtLocks: handling direct call:" << *callSite << "\t" << cgEdge->getSrcNode()->getFunction()->getName()
                      << "-->" << cgEdge->getDstNode()->getFunction()->getName() << "\n");
                handleCallRelation(icfg, cg, clp, cgEdge, callSite);
            }
            std::vector<const CallICFGNode*> indirectCalls;
            GenericGraphTraits<CGGraph>::getIndirectCalls(
                cg, cgEdge, indirectCalls);
            for (const CallICFGNode* callSite : indirectCalls)
            {
                DBOUT(DMTA,
                      outs() << "\nCollecting CxtLocks: handling indirect call:" << *callSite << "\t"
                      << cgEdge->getSrcNode()->getFunction()->getName() << "-->" << cgEdge->getDstNode()->getFunction()->getName()
                      << "\n");
                handleCallRelation(icfg, cg, clp, cgEdge, callSite);
            }
        }
    }
}


/*!
 * Handling call relations when collecting context-sensitive locks
 */
template<class ICFGGraph, class CGGraph>
void LockAnalysis::handleCallRelation(ICFGGraph icfg, CGGraph cg, CxtLockProc& clp, const CallGraphEdge* cgEdge, const CallICFGNode* cs)
{

    CallStrCxt cxt(clp.getContext());
    const ICFGNode* curNode = cs;
    if (!GenericGraphTraits<ICFGGraph>::containsNode(icfg, curNode))
        return;
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

bool LockAnalysis::isAliasedLocks(const ICFGNode* i1, const ICFGNode* i2)
{
    // Lock matching is conservative: may-alias lock objects are treated as the
    // same lock, consistent with the existing MTA lock semantics.
    return tct->getPTA()->alias(getLockVal(i1)->getId(), getLockVal(i2)->getId());
}

template<class ICFGGraph, class CGGraph>
void LockAnalysis::analyzeLockSpanCxtStmt(ICFGGraph icfg, CGGraph cg)
{

    const TCT::FunSet& entryFuncSet = tct->getEntryProcs();
    for (TCT::FunSet::const_iterator it = entryFuncSet.begin(), eit = entryFuncSet.end(); it != eit; ++it)
    {
        if (!isLockCandidateFun(*it))
            continue;
        CallStrCxt cxt;
        const ICFGNode* frontInst = GenericGraphTraits<ICFGGraph>::getFunEntry(icfg, *it);
        if (!GenericGraphTraits<ICFGGraph>::containsNode(icfg, frontInst))
            continue;
        CxtStmt cxtstmt(cxt, frontInst);
        pushToCTSWorkList(cxtstmt);
    }

    while (!cxtStmtList.empty())
    {
        CxtStmt cts = popFromCTSWorkList();

        touchCxtStmt(cts);
        const ICFGNode* curInst = cts.getStmt();
        if (!GenericGraphTraits<ICFGGraph>::containsNode(icfg, curInst))
            continue;
        const bool firstContextVisit = instToCxtStmtSet[curInst].insert(cts).second;
        if (firstContextVisit && isCallSite(curInst))
            indexCallsiteContext(curInst, cts.getContext());
        DBOUT(DMTA, outs() << "\nVisit cxtStmt: ");
        DBOUT(DMTA, cts.dump());

        DBOUT(DMTA, outs() << "\nIts cxt lock sets: ");
        DBOUT(DMTA, printLocks(cts));

        if (isTDFork(curInst))
        {
            handleFork(icfg, cg, cts);
        }
        else if (isTDAcquire(curInst))
        {
            // Context truncation can merge a path into a lock context that the
            // call-graph pre-collection did not enumerate. The propagation is
            // authoritative: register that reachable lock before constructing
            // its span. Release builds already constructed the same span; this
            // keeps the canonical lock registry consistent as well.
            if (!hasCxtLock(cts))
                addCxtLock(cts.getContext(), curInst);
            if (addCxtStmtToSpan(cts, cts))
                handleIntra(icfg, cg, cts);
        }
        else if (isTDRelease(curInst))
        {
            if(removeCxtStmtToSpan(cts,cts))
                handleIntra(icfg, cg, cts);
        }
        else if (isCallSite(curInst) && !isExtCall(curInst))
        {
            handleCall(icfg, cg, cts);
        }
        else if (SVFUtil::dyn_cast<FunExitICFGNode>(curInst))
        {
            handleRet(icfg, cg, cts);
        }
        else
        {
            handleIntra(icfg, cg, cts);
        }

    }

}


/*!
 * Print context-insensitive and context-sensitive locks
 */
void LockAnalysis::printLocks(const CxtStmt& cts)
{
    const CxtLockSet & lockset = getCxtLockFromCxtStmt(cts);
    outs() << "\nlock sets size = " << lockset.size() << "\n";
    for (CxtLockSet::const_iterator it = lockset.begin(), eit = lockset.end(); it != eit; ++it)
    {
        (*it).dump();
    }
}



/// Handle fork
template<class ICFGGraph, class CGGraph>
void LockAnalysis::handleFork(ICFGGraph icfg, CGGraph cg, const CxtStmt& cts)
{
    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(cts.getStmt());
    const CallGraphNode* callerNode = getTCG()->getCallGraphNode(call->getFun());
    std::vector<const CallGraphEdge*> outEdges;
    GenericGraphTraits<CGGraph>::getOutEdges(cg, callerNode, outEdges);
    for (const CallGraphEdge* edge : outEdges)
    {
        if (!SVFUtil::isa<ThreadForkEdge>(edge) ||
            !GenericGraphTraits<CGGraph>::containsCallSite(cg, edge, call))
            continue;
        const FunObjVar* svfcallee = edge->getDstNode()->getFunction();
        CallStrCxt newCxt = curCxt;
        pushCxt(newCxt, call, svfcallee);
        const ICFGNode* svfInst =
            GenericGraphTraits<ICFGGraph>::getFunEntry(icfg, svfcallee);
        if (svfInst == nullptr)
            continue;
        CxtStmt newCts(newCxt, svfInst);
        markCxtStmtFlag(newCts, cts);
    }
    handleIntra(icfg, cg, cts);
}

/// Handle call
template<class ICFGGraph, class CGGraph>
void LockAnalysis::handleCall(ICFGGraph icfg, CGGraph cg, const CxtStmt& cts)
{

    const CallStrCxt& curCxt = cts.getContext();
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(cts.getStmt());
    const CallGraphNode* callerNode = getTCG()->getCallGraphNode(call->getFun());
    std::vector<const CallGraphEdge*> outEdges;
    GenericGraphTraits<CGGraph>::getOutEdges(cg, callerNode, outEdges);
    for (const CallGraphEdge* edge : outEdges)
    {
        if (edge->getEdgeKind() != CallGraphEdge::CallRetEdge ||
            !GenericGraphTraits<CGGraph>::containsCallSite(cg, edge, call))
            continue;
        const FunObjVar* svfcallee = edge->getDstNode()->getFunction();
        if (SVFUtil::isExtCall(svfcallee))
            continue;
        CallStrCxt newCxt = curCxt;
        pushCxt(newCxt, call, svfcallee);
        const ICFGNode* svfInst =
            GenericGraphTraits<ICFGGraph>::getFunEntry(icfg, svfcallee);
        if (svfInst == nullptr)
            continue;
        CxtStmt newCts(newCxt, svfInst);
        markCxtStmtFlag(newCts, cts);

        // Return-flow rendezvous (see MHP::handleCall): forward an already
        // computed callee-exit lockset to this callsite's return site.
        if (svfcallee->hasBasicBlock())
        {
            const ICFGNode* exitInst =
                GenericGraphTraits<ICFGGraph>::getFunExit(icfg, svfcallee);
            if (exitInst == nullptr)
                continue;
            CxtStmt exitCts(newCxt, exitInst);
            if (hasCxtLockFromCxtStmt(exitCts))
            {
                const ICFGNode* retNode = call->getRetICFGNode();
                if (GenericGraphTraits<ICFGGraph>::containsNode(icfg, retNode))
                {
                    CxtStmt retCts(curCxt, retNode);
                    markCxtStmtFlag(retCts, exitCts);
                }
            }
        }
    }
}

void LockAnalysis::handleReturnAtCallsite(
    const CxtStmt& exitCxtStmt, const FunObjVar* callee,
    const ICFGNode* callsite, const std::vector<const ICFGNode*>& successors)
{
    CallStrCxt callerCxt = exitCxtStmt.getContext();
    const CallICFGNode* call = SVFUtil::cast<CallICFGNode>(callsite);
    if (!matchCxt(callerCxt, call, callee))
        return;

    for (const ICFGNode* successor : successors)
    {
        if (successor->getFun() != callsite->getFun())
            continue;

        const CallStrCxtSet* matchingContexts =
            getCallsiteContextsWithSuffix(callsite, callerCxt);
        if (matchingContexts == nullptr)
            continue;

        for (const CallStrCxt& callsiteCxt : *matchingContexts)
        {
            CxtStmt returnCxtStmt(callsiteCxt, successor);
            markCxtStmtFlag(returnCxtStmt, exitCxtStmt);
        }
    }
}

/// Handle return
template<class ICFGGraph, class CGGraph>
void LockAnalysis::handleRet(ICFGGraph icfg, CGGraph cg, const CxtStmt& cts)
{

    const ICFGNode* curInst = cts.getStmt();
    const FunObjVar* svffun = curInst->getFun();
    CallGraphNode* curFunNode = getTCG()->getCallGraphNode(svffun);

    std::vector<const ICFGNode*> succ;
    GenericGraphTraits<ICFGGraph>::getSuccNodes(icfg, curInst, succ);

    std::vector<const CallGraphEdge*> inEdges;
    GenericGraphTraits<CGGraph>::getInEdges(cg, curFunNode, inEdges);
    for (const CallGraphEdge* edgeConst : inEdges)
    {
        if (SVFUtil::isa<ThreadForkEdge, ThreadJoinEdge>(edgeConst))
            continue;
        std::vector<const CallICFGNode*> directCalls;
        GenericGraphTraits<CGGraph>::getDirectCalls(
            cg, edgeConst, directCalls);
        for (const CallICFGNode* callSite : directCalls)
            handleReturnAtCallsite(
                cts, curFunNode->getFunction(), callSite, succ);

        std::vector<const CallICFGNode*> indirectCalls;
        GenericGraphTraits<CGGraph>::getIndirectCalls(
            cg, edgeConst, indirectCalls);
        for (const CallICFGNode* callSite : indirectCalls)
            handleReturnAtCallsite(
                cts, curFunNode->getFunction(), callSite, succ);
    }
}

/// Handle intra
template<class ICFGGraph, class CGGraph>
void LockAnalysis::handleIntra(ICFGGraph icfg, CGGraph cg, const CxtStmt& cts)
{

    const ICFGNode* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    std::vector<const ICFGNode*> succ;
    GenericGraphTraits<ICFGGraph>::getSuccNodes(icfg, curInst, succ);
    for (const ICFGNode* dst : succ)
    {
        if(dst->getFun() == curInst->getFun())
        {
            CxtStmt newCts(curCxt, dst);
            markCxtStmtFlag(newCts, cts);
        }
    }
}

void LockAnalysis::pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    tct->pushCxt(cxt,call,callee);
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

bool LockAnalysis::isContextSuffix(const CallStrCxt& lhs, const CallStrCxt& call)
{
    return tct->isContextSuffix(lhs,call);
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
    if(!hasCxtLockFromCxtStmt(cxtStmt1) || !hasCxtLockFromCxtStmt(cxtStmt2))
        return false;
    const CxtLockSet& lockset1 = getCxtLockFromCxtStmt(cxtStmt1);
    const CxtLockSet& lockset2 = getCxtLockFromCxtStmt(cxtStmt2);
    return alias(lockset1,lockset2);
}

/*!
 * Protected by at least one common context-sensitive lock under each context
 */
bool LockAnalysis::isProtectedByCommonCxtLock(const ICFGNode *i1, const ICFGNode *i2)
{
    if(!hasCxtStmtFromInst(i1) || !hasCxtStmtFromInst(i2))
        return false;
    const CxtStmtSet& ctsset1 = getCxtStmtsFromInst(i1);
    const CxtStmtSet& ctsset2 = getCxtStmtsFromInst(i2);
    for (CxtStmtSet::const_iterator cts1 = ctsset1.begin(), ects1 = ctsset1.end(); cts1 != ects1; cts1++)
    {
        const CxtStmt& cxtStmt1 = *cts1;
        for (CxtStmtSet::const_iterator cts2 = ctsset2.begin(), ects2 = ctsset2.end(); cts2 != ects2; cts2++)
        {
            const CxtStmt& cxtStmt2 = *cts2;
            if(cxtStmt1==cxtStmt2)
            {
                // i1==i2 under the same context: a self-race between two dynamic
                // instances of one statement (e.g. a thread forked in a loop).
                // This is the ONLY pair the loop produces for such a query, so
                // skipping it would fall through to the vacuous "protected" return
                // below and drop a real race. The two instances are mutually
                // excluded only if this context actually holds a (non-empty) lock.
                if(!hasCxtLockFromCxtStmt(cxtStmt1) || getCxtLockFromCxtStmt(cxtStmt1).empty())
                    return false;
                continue;
            }
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
    if(!hasCxtLockFromCxtStmt(cxtStmt1) || !hasCxtLockFromCxtStmt(cxtStmt2))
        return false;
    const CxtLockSet& lockset1 = getCxtLockFromCxtStmt(cxtStmt1);
    const CxtLockSet& lockset2 = getCxtLockFromCxtStmt(cxtStmt2);
    return intersects(lockset1,lockset2);
}
/*!
 * Return true if two instructions are inside at least one common context-sensitive lock span
 */
bool LockAnalysis::isInSameCSSpan(const ICFGNode *I1, const ICFGNode *I2) const
{
    if(!hasCxtStmtFromInst(I1) || !hasCxtStmtFromInst(I2))
        return false;
    const CxtStmtSet& ctsset1 = getCxtStmtsFromInst(I1);
    const CxtStmtSet& ctsset2 = getCxtStmtsFromInst(I2);

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

// The two graphs the lock analysis runs on; the algorithm above is written once
// and instantiated for both (all internal templates instantiate transitively).
template void LockAnalysis::analyze<ICFG*, CallGraph*>(ICFG*, CallGraph*);
template void LockAnalysis::analyze<const SlicedICFGView*, const SlicedThreadCallGraphView*>(const SlicedICFGView*, const SlicedThreadCallGraphView*);
