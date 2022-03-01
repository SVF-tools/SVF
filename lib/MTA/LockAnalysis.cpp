/*
 * LocksetAnalysis.cpp
 *
 *  Created on: 26 Aug 2015
 *      Author: pengd
 */

#include "Util/Options.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTA.h"
#include "MTA/MTAResultValidator.h"
#include "Util/SVFUtil.h"
#include "MemoryModel/PTAStat.h"
#include "MTA/LockResultValidator.h"


using namespace SVF;
using namespace SVFUtil;


namespace SVF
{

// Subclassing RCResultValidator to define the abstract methods.
class RaceValidator : public RaceResultValidator
{
public:
    RaceValidator(LockAnalysis* ls) :lsa(ls)
    {
    }
    bool protectedByCommonLocks(const Instruction *I1, const Instruction *I2)
    {
        return lsa->isProtectedByCommonLock(I1, I2);
    }
private:
    LockAnalysis *lsa;
};

} // End namespace SVF

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
	if(Options::LockValid)
    	validateResults();
}


/*!
 * Collect lock/unlock sites
 */
void LockAnalysis::collectLockUnlocksites()
{
    ThreadCallGraph* tcg=tct->getThreadCallGraph();
    for (SVFModule::iterator F = tct->getSVFModule()->begin(), E = tct->getSVFModule()->end(); F != E; ++F)
    {
        for (inst_iterator II = inst_begin((*F)->getLLVMFun()), EE = inst_end((*F)->getLLVMFun()); II != EE; ++II)
        {
            const Instruction *inst = &*II;
            if (tcg->getThreadAPI()->isTDRelease(inst))
            {
                unlocksites.insert(inst);
            }
            if (tcg->getThreadAPI()->isTDAcquire(inst))
            {
                locksites.insert(inst);
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
    FIFOWorkList<const PTACallGraphNode*> worklist;

    for (InstSet::iterator it = locksites.begin(), eit = locksites.end(); it != eit; ++it)
    {
        const Function* fun=(*it)->getParent()->getParent();
        PTACallGraphNode* cgnode = tcg->getCallGraphNode(tct->getSVFFun(fun));
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }
    for (InstSet::iterator it = unlocksites.begin(), eit = unlocksites.end(); it != eit; ++it)
    {
        const Function* fun = (*it)->getParent()->getParent();
        PTACallGraphNode* cgnode = tcg->getCallGraphNode(tct->getSVFFun(fun));
        if (visited.find(cgnode) == visited.end())
        {
            worklist.push(cgnode);
            visited.insert(cgnode);
        }
    }
    while (!worklist.empty())
    {
        const PTACallGraphNode* node = worklist.pop();
        lockcandidateFuncSet.insert(node->getFunction()->getLLVMFun());
        for (PTACallGraphNode::const_iterator nit = node->InEdgeBegin(), neit = node->InEdgeEnd(); nit != neit; nit++)
        {
            const PTACallGraphNode* srcNode = (*nit)->getSrcNode();
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
        const CallInst *lockSite = SVFUtil::dyn_cast<CallInst>(*it);
        assert(lockSite && "Lock acquire instruction must be CallInst");

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
bool LockAnalysis::intraForwardTraverse(const Instruction* lockSite, InstSet& unlockSet, InstSet& forwardInsts)
{

    const Function* fun = lockSite->getParent()->getParent();

    InstVec worklist;
    worklist.push_back(lockSite);
    while (!worklist.empty())
    {
        const Instruction *I = worklist.back();
        worklist.pop_back();

        if(&(getFunExitBB(fun)->back()) == I)
            return false;

        // Skip the visited Instructions.
        if (forwardInsts.find(I)!=forwardInsts.end())
            continue;
        forwardInsts.insert(I);

        if (isTDRelease(I) && isAliasedLocks(lockSite, I))
        {
            unlockSet.insert(I);
            DBOUT(DMTA, outs() << "LockAnalysis ci lock   -- " << SVFUtil::getSourceLoc(lockSite)<<"\n");
            DBOUT(DMTA, outs() << "LockAnalysis ci unlock -- " << SVFUtil::getSourceLoc(I)<<"\n");
            continue;
        }

        InstVec nextInsts;
        getNextInsts(I, nextInsts);
        for (InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit != enit; ++nit)
        {
            worklist.push_back(*nit);
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
        const Instruction* unlockSite = *it;
        const Function* fun = unlockSite->getParent()->getParent();
		const Instruction* entryInst = &(fun->getEntryBlock().back());
        worklist.push_back(*it);

        while (!worklist.empty())
        {
            const Instruction *I = worklist.back();
            worklist.pop_back();

            if(entryInst == I)
                return false;

            // Skip the visited Instructions.
            if (backwardInsts.find(I)!=backwardInsts.end())
                continue;
            backwardInsts.insert(I);

            if (isTDAcquire(I) && isAliasedLocks(unlockSite, I))
            {
                DBOUT(DMTA, outs() << "LockAnalysis ci lock   -- " << SVFUtil::getSourceLoc(I)<<"\n");
                DBOUT(DMTA, outs() << "LockAnalysis ci unlock -- " << SVFUtil::getSourceLoc(unlockSite)<<"\n");
                continue;
            }

            InstVec nextInsts;
            getPrevInsts(I, nextInsts);
            for (InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit != enit; ++nit)
            {
                worklist.push_back(*nit);
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
        CxtLockProc t(cxt, tct->getSVFFun(*it));
        pushToCTPWorkList(t);
    }

    while (!clpList.empty())
    {
        CxtLockProc clp = popFromCTPWorkList();
        PTACallGraphNode* cgNode = getTCG()->getCallGraphNode(clp.getProc());
        // lzh TODO.
        if (!isLockCandidateFun(cgNode->getFunction()->getLLVMFun()))
            continue;

        for (PTACallGraphNode::const_iterator nit = cgNode->OutEdgeBegin(), neit = cgNode->OutEdgeEnd(); nit != neit; nit++)
        {
            const PTACallGraphEdge* cgEdge = (*nit);

            for (PTACallGraphEdge::CallInstSet::const_iterator cit = cgEdge->directCallsBegin(), ecit = cgEdge->directCallsEnd();
                    cit != ecit; ++cit)
            {
                DBOUT(DMTA,
                      outs() << "\nCollecting CxtLocks: handling direct call:" << **cit << "\t" << cgEdge->getSrcNode()->getFunction()->getName()
                      << "-->" << cgEdge->getDstNode()->getFunction()->getName() << "\n");
                handleCallRelation(clp, cgEdge, getLLVMCallSite((*cit)->getCallSite()));
            }
            for (PTACallGraphEdge::CallInstSet::const_iterator ind = cgEdge->indirectCallsBegin(), eind = cgEdge->indirectCallsEnd();
                    ind != eind; ++ind)
            {
                DBOUT(DMTA,
                      outs() << "\nCollecting CxtLocks: handling indirect call:" << **ind << "\t"
                      << cgEdge->getSrcNode()->getFunction()->getName() << "-->" << cgEdge->getDstNode()->getFunction()->getName()
                      << "\n");
                handleCallRelation(clp, cgEdge, getLLVMCallSite((*ind)->getCallSite()));
            }
        }
    }
}


/*!
 * Handling call relations when collecting context-sensitive locks
 */
void LockAnalysis::handleCallRelation(CxtLockProc& clp, const PTACallGraphEdge* cgEdge, CallSite cs)
{

    CallStrCxt cxt(clp.getContext());

    if (isTDAcquire(cs.getInstruction()))
    {
        addCxtLock(cxt,cs.getInstruction());
        return;
    }
	const SVFFunction* svfcallee = cgEdge->getDstNode()->getFunction();
	const Function* callee = svfcallee->getLLVMFun();
    pushCxt(cxt, cs.getInstruction(), callee);

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
        CxtStmt cxtstmt(cxt, &((*it)->front().front()));
        pushToCTSWorkList(cxtstmt);
    }

    while (!cxtStmtList.empty())
    {
        CxtStmt cts = popFromCTSWorkList();

        touchCxtStmt(cts);
        const Instruction* curInst = cts.getStmt();
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
        else if (SVFUtil::isa<CallInst>(curInst) && !isExtCall(curInst))
        {
            handleCall(cts);
        }
        else if (SVFUtil::isa<ReturnInst>(curInst))
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
    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();
	CallICFGNode* cbn = tct->getCallICFGNode(call);
    if(getTCG()->hasThreadForkEdge(cbn))
    {
        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = getTCG()->getForkEdgeBegin(cbn),
                ecgIt = getTCG()->getForkEdgeEnd(cbn); cgIt != ecgIt; ++cgIt)
        {
            const SVFFunction* svfcallee = (*cgIt)->getDstNode()->getFunction();
            const Function* callee = svfcallee->getLLVMFun();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt,call,callee);
            CxtStmt newCts(newCxt, &(callee->getEntryBlock().front()));
            markCxtStmtFlag(newCts, cts);
        }
    }
    handleIntra(cts);
}

/// Handle call
void LockAnalysis::handleCall(const CxtStmt& cts)
{

    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();
	CallICFGNode* cbn = tct->getCallICFGNode(call);
    if (getTCG()->hasCallGraphEdge(cbn))
    {
        for (PTACallGraph::CallGraphEdgeSet::const_iterator cgIt = getTCG()->getCallEdgeBegin(cbn), ecgIt = getTCG()->getCallEdgeEnd(cbn);
                cgIt != ecgIt; ++cgIt)
        {
            const SVFFunction* svfcallee = (*cgIt)->getDstNode()->getFunction();
            const Function* callee = svfcallee->getLLVMFun();
            if (isExtCall(svfcallee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, callee);
            CxtStmt newCts(newCxt, &(callee->getEntryBlock().front()));
            markCxtStmtFlag(newCts, cts);
        }
    }
}

/// Handle return
void LockAnalysis::handleRet(const CxtStmt& cts)
{

    const Instruction* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
	const SVFFunction* svffun = tct->getSVFFun(curInst->getParent()->getParent());
    PTACallGraphNode* curFunNode = getTCG()->getCallGraphNode(svffun);
    
    for (PTACallGraphNode::const_iterator it = curFunNode->getInEdges().begin(), eit = curFunNode->getInEdges().end(); it != eit; ++it)
    {
        PTACallGraphEdge* edge = *it;
        if (SVFUtil::isa<ThreadForkEdge>(edge) || SVFUtil::isa<ThreadJoinEdge>(edge))
            continue;
        for (PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->directCallsBegin(), ecit = (edge)->directCallsEnd(); cit != ecit;
                ++cit)
        {
            CallStrCxt newCxt = curCxt;
            const Instruction* inst = (*cit)->getCallSite();
            if (matchCxt(newCxt, inst, curFunNode->getFunction()->getLLVMFun()))
            {
                InstVec nextInsts;
                getNextInsts(inst, nextInsts);
                for (InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit != enit; ++nit)
                {
                    CxtStmt newCts(newCxt, *nit);
                    markCxtStmtFlag(newCts, cts);
                }
            }
        }
        for (PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->indirectCallsBegin(), ecit = (edge)->indirectCallsEnd();
                cit != ecit; ++cit)
        {
            CallStrCxt newCxt = curCxt;
            const Instruction* inst = (*cit)->getCallSite();
            if (matchCxt(newCxt, inst, curFunNode->getFunction()->getLLVMFun()))
            {
                InstVec nextInsts;
                getNextInsts(inst, nextInsts);
                for (InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit != enit; ++nit)
                {
                    CxtStmt newCts(newCxt, *nit);
                    markCxtStmtFlag(newCts, cts);
                }
            }
        }
    }
}

/// Handle intra
void LockAnalysis::handleIntra(const CxtStmt& cts)
{

    const Instruction* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    InstVec nextInsts;
    getNextInsts(curInst, nextInsts);
    for (InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit != enit; ++nit)
    {
        CxtStmt newCts(curCxt, *nit);
        markCxtStmtFlag(newCts, cts);
    }
}


void LockAnalysis::pushCxt(CallStrCxt& cxt, const Instruction* call, const Function* callee)
{
    const Function* caller = call->getParent()->getParent();
    const SVFFunction* svfcallee = tct->getSVFFun(callee);
    const SVFFunction* svfcaller = tct->getSVFFun(caller);
    CallICFGNode* cbn = tct->getCallICFGNode(call);
    CallSiteID csId = getTCG()->getCallSiteID(cbn, svfcallee);
	
//    /// handle calling context for candidate functions only
//    if (isLockCandidateFun(caller) == false)
//        return;

    if (tct->inSameCallGraphSCC(getTCG()->getCallGraphNode(svfcaller), getTCG()->getCallGraphNode(svfcallee)) == false)
    {
        tct->pushCxt(cxt,csId);
        DBOUT(DMTA, tct->dumpCxt(cxt));
    }
}

bool LockAnalysis::matchCxt(CallStrCxt& cxt, const Instruction* call, const Function* callee)
{
    const Function* caller = call->getParent()->getParent();
    const SVFFunction* svfcallee = tct->getSVFFun(callee);
    const SVFFunction* svfcaller = tct->getSVFFun(caller);
    CallICFGNode* cbn = tct->getCallICFGNode(call);
    CallSiteID csId = getTCG()->getCallSiteID(cbn, svfcallee);

//    /// handle calling context for candidate functions only
//    if (isLockCandidateFun(caller) == false)
//        return true;

    /// partial match
    if (cxt.empty())
        return true;

    if (tct->inSameCallGraphSCC(getTCG()->getCallGraphNode(svfcaller), getTCG()->getCallGraphNode(svfcallee)) == false)
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
bool LockAnalysis::isProtectedByCommonLock(const Instruction *i1, const Instruction *i2)
{
    numOfTotalQueries++;
    bool commonlock = false;
    DOTIMESTAT(double queryStart = PTAStat::getClk());
    if (isInsideIntraLock(i1) && isInsideIntraLock(i2))
        commonlock = isProtectedByCommonCILock(i1,i2) ;
    else
        commonlock = isProtectedByCommonCxtLock(i1,i2);
    DOTIMESTAT(double queryEnd = PTAStat::getClk());
    DOTIMESTAT(lockQueriesTime += (queryEnd - queryStart) / TIMEINTERVAL);
    return commonlock;
}

/*!
 * Protected by at least one common context-insensitive lock
 */
bool LockAnalysis::isProtectedByCommonCILock(const Instruction *i1, const Instruction *i2)
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
    if (alias(lockset1,lockset2))
        return true;

    return false;
}

/*!
 * Protected by at least one common context-sensitive lock under each context
 */
bool LockAnalysis::isProtectedByCommonCxtLock(const Instruction *i1, const Instruction *i2)
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
bool LockAnalysis::isInSameSpan(const Instruction *i1, const Instruction *i2)
{
    DOTIMESTAT(double queryStart = PTAStat::getClk());

    bool sameSpan = false;
    if (isInsideIntraLock(i1) && isInsideIntraLock(i2))
        sameSpan = isInSameCISpan(i1, i2);
    else
        sameSpan = isInSameCSSpan(i1, i2);

    DOTIMESTAT(double queryEnd = PTAStat::getClk());
    DOTIMESTAT(lockQueriesTime += (queryEnd - queryStart) / TIMEINTERVAL);
    return sameSpan;
}

/*!
 * Return true if two instructions are inside same context-insensitive lock span
 */
bool LockAnalysis::isInSameCISpan(const Instruction *i1, const Instruction *i2) const
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
    if (intersects(lockset1,lockset2))
        return true;

    return false;
}
/*!
 * Return true if two instructions are inside at least one common contex-sensitive lock span
 */
bool LockAnalysis::isInSameCSSpan(const Instruction *I1, const Instruction *I2) const
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

void LockAnalysis::validateResults()
{

    // Initialize the validator and perform validation.
    LockResultValidator lockvalidator(this);
    lockvalidator.analyze();
    
    RaceValidator validator(this);
    validator.init(tct->getSVFModule());
    validator.analyze();
}
