//===- MHP.cpp -- May-happen-in-parallel analysis-------------//
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
 * MHP.cpp
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTA/MHP.h"
#include "MTA/MTA.h"
#include "MTA/LockAnalysis.h"
#include "Util/SVFUtil.h"
#include "Util/PTAStat.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * Constructor
 */
MHP::MHP(TCT* t) : tcg(t->getThreadCallGraph()), tct(t), numOfTotalQueries(0), numOfMHPQueries(0),
    interleavingTime(0), interleavingQueriesTime(0)
{
    fja = new ForkJoinAnalysis(tct);
    fja->analyzeForkJoinPair();
}

/*!
 * Destructor
 */
MHP::~MHP()
{
    delete fja;
}

/*!
 * Start analysis here
 */
void MHP::analyze()
{
    DBOUT(DGENERAL, outs() << pasMsg("MHP interleaving analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP interleaving analysis\n"));
    DOTIMESTAT(double interleavingStart = PTAStat::getClk(true));
    analyzeInterleaving();
    DOTIMESTAT(double interleavingEnd = PTAStat::getClk(true));
    DOTIMESTAT(interleavingTime += (interleavingEnd - interleavingStart) / TIMEINTERVAL);
}

/*!
 * Analyze thread interleaving
 */
void MHP::analyzeInterleaving()
{
    for (const std::pair<const NodeID, TCTNode*>& tpair : *tct)
    {
        const CxtThread& ct = tpair.second->getCxtThread();
        NodeID rootTid = tpair.first;
        const SVFFunction* routine = tct->getStartRoutineOfCxtThread(ct);
        const SVFInstruction* svfInst = routine->getEntryBlock()->front();
        CxtThreadStmt rootcts(rootTid, ct.getContext(), svfInst);

        addInterleavingThread(rootcts, rootTid);
        updateAncestorThreads(rootTid);
        updateSiblingThreads(rootTid);

        while (!cxtStmtList.empty())
        {
            CxtThreadStmt cts = popFromCTSWorkList();
            const SVFInstruction* curInst = cts.getStmt();
            DBOUT(DMTA, outs() << "-----\nMHP analysis root thread: " << rootTid << " ");
            DBOUT(DMTA, cts.dump());
            DBOUT(DMTA, outs() << "current thread interleaving: < ");
            DBOUT(DMTA, dumpSet(getInterleavingThreads(cts)));
            DBOUT(DMTA, outs() << " >\n-----\n");

            /// handle non-candidate function
            if (!tct->isCandidateFun(curInst->getParent()->getParent()))
            {
                handleNonCandidateFun(cts);
            }
            /// handle candidate function
            else
            {
                if (isTDFork(curInst))
                {
                    handleFork(cts, rootTid);
                }
                else if (isTDJoin(curInst))
                {
                    handleJoin(cts, rootTid);
                }
                else if (SVFUtil::isCallSite(curInst) && !isExtCall(curInst))
                {
                    handleCall(cts, rootTid);
                    PTACallGraph::FunctionSet callees;
                    if (!tct->isCandidateFun(getCallee(curInst, callees)))
                        handleIntra(cts);
                }
                else if (curInst->isRetInst())
                {
                    handleRet(cts);
                }
                else
                {
                    handleIntra(cts);
                }
            }
        }
    }

    /// update non-candidate functions' interleaving
    updateNonCandidateFunInterleaving();

    if (Options::PrintInterLev())
        printInterleaving();
}

/*!
 * Update non-candidate functions' interleaving
 */
void MHP::updateNonCandidateFunInterleaving()
{
    SVFModule* module = tct->getSVFModule();
    for (const SVFFunction* fun : module->getFunctionSet())
    {
        if (!tct->isCandidateFun(fun) && !isExtCall(fun))
        {
            const SVFInstruction* entryinst = fun->getEntryBlock()->front();
            if (!hasThreadStmtSet(entryinst))
                continue;

            const CxtThreadStmtSet& tsSet = getThreadStmtSet(entryinst);

            for (const CxtThreadStmt& cts : tsSet)
            {
                const CallStrCxt& curCxt = cts.getContext();

                for (const SVFBasicBlock* svfbb : fun->getBasicBlockList())
                {
                    for (const SVFInstruction* svfInst : svfbb->getInstructionList())
                    {
                        if (svfInst == entryinst)
                            continue;
                        CxtThreadStmt newCts(cts.getTid(), curCxt, svfInst);
                        threadStmtToTheadInterLeav[newCts] |= threadStmtToTheadInterLeav[cts];
                        instToTSMap[svfInst].insert(newCts);
                    }
                }
            }
        }
    }
}

/*!
 * Handle call instruction in the current thread scope (excluding any fork site)
 */
void MHP::handleNonCandidateFun(const CxtThreadStmt& cts)
{
    const SVFInstruction* curInst = cts.getStmt();
    const SVFFunction* curfun = curInst->getParent()->getParent();
    assert((curInst == curfun->getEntryBlock()->front()) && "curInst is not the entry of non candidate function.");
    const CallStrCxt& curCxt = cts.getContext();
    PTACallGraphNode* node = tcg->getCallGraphNode(curfun);
    for (PTACallGraphNode::const_iterator nit = node->OutEdgeBegin(), neit = node->OutEdgeEnd(); nit != neit; nit++)
    {
        const SVFFunction* callee = (*nit)->getDstNode()->getFunction();
        if (!isExtCall(callee))
        {
            const SVFInstruction* calleeInst = callee->getEntryBlock()->front();
            CxtThreadStmt newCts(cts.getTid(), curCxt, calleeInst);
            addInterleavingThread(newCts, cts);
        }
    }
}

/*!
 * Handle fork
 */
void MHP::handleFork(const CxtThreadStmt& cts, NodeID rootTid)
{

    const SVFInstruction* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDFork(call));
    CallICFGNode* cbn = getCBN(call);
    if (tct->getThreadCallGraph()->hasCallGraphEdge(cbn))
    {

        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = tcg->getForkEdgeBegin(cbn),
                ecgIt = tcg->getForkEdgeEnd(cbn);
                cgIt != ecgIt; ++cgIt)
        {
            const SVFFunction* svfroutine = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, svfroutine);
            const SVFInstruction* stmt = svfroutine->getEntryBlock()->front();
            CxtThread ct(newCxt, call);
            CxtThreadStmt newcts(tct->getTCTNode(ct)->getId(), ct.getContext(), stmt);
            addInterleavingThread(newcts, cts);
        }
    }
    handleIntra(cts);
}

/*!
 * Handle join
 */
void MHP::handleJoin(const CxtThreadStmt& cts, NodeID rootTid)
{

    const SVFInstruction* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDJoin(call));

    NodeBS joinedTids = getDirAndIndJoinedTid(curCxt, call);
    if (!joinedTids.empty())
    {
        if (fja->hasJoinLoop(call))
        {
            std::vector<const SVFBasicBlock*> exitbbs;
            call->getFunction()->getExitBlocksOfLoop(call->getParent(), exitbbs);
            while (!exitbbs.empty())
            {
                const SVFBasicBlock* eb = exitbbs.back();
                exitbbs.pop_back();
                const SVFInstruction* svfEntryInst = eb->front();
                CxtThreadStmt newCts(cts.getTid(), curCxt, svfEntryInst);
                addInterleavingThread(newCts, cts);
                if (hasJoinInSymmetricLoop(curCxt, call))
                    rmInterleavingThread(newCts, joinedTids, call);
            }
        }
        else
        {
            rmInterleavingThread(cts, joinedTids, call);
            DBOUT(DMTA, outs() << "\n\t match join site " << call->toString() << " for thread " << rootTid << "\n");
        }
    }
    /// for the join site in a loop loop which does not join the current thread
    /// we process the loop exit
    else
    {
        if (fja->hasJoinLoop(call))
        {
            std::vector<const SVFBasicBlock*> exitbbs;
            call->getFunction()->getExitBlocksOfLoop(call->getParent(), exitbbs);
            while (!exitbbs.empty())
            {
                const SVFBasicBlock* eb = exitbbs.back();
                exitbbs.pop_back();
                const SVFInstruction* svfEntryInst = eb->front();
                CxtThreadStmt newCts(cts.getTid(), cts.getContext(), svfEntryInst);
                addInterleavingThread(newCts, cts);
            }
        }
    }
    handleIntra(cts);
}

/*!
 * Handle call instruction in the current thread scope (excluding any fork site)
 */
void MHP::handleCall(const CxtThreadStmt& cts, NodeID rootTid)
{

    const SVFInstruction* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
    CallICFGNode* cbn = getCBN(call);
    if (tct->getThreadCallGraph()->hasCallGraphEdge(cbn))
    {
        for (PTACallGraph::CallGraphEdgeSet::const_iterator cgIt = tcg->getCallEdgeBegin(cbn),
                ecgIt = tcg->getCallEdgeEnd(cbn);
                cgIt != ecgIt; ++cgIt)
        {

            const SVFFunction* svfcallee = (*cgIt)->getDstNode()->getFunction();
            if (isExtCall(svfcallee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, svfcallee);
            const SVFInstruction* svfEntryInst = svfcallee->getEntryBlock()->front();
            CxtThreadStmt newCts(cts.getTid(), newCxt, svfEntryInst);
            addInterleavingThread(newCts, cts);
        }
    }
}

/*!
 * Handle return instruction in the current thread scope (excluding any join site)
 */
void MHP::handleRet(const CxtThreadStmt& cts)
{
    PTACallGraphNode* curFunNode = tcg->getCallGraphNode(cts.getStmt()->getParent()->getParent());
    for (PTACallGraphEdge* edge : curFunNode->getInEdges())
    {
        if (SVFUtil::isa<ThreadForkEdge, ThreadJoinEdge>(edge))
            continue;
        for (PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->directCallsBegin(),
                ecit = (edge)->directCallsEnd();
                cit != ecit; ++cit)
        {
            CallStrCxt newCxt = cts.getContext();
            if (matchCxt(newCxt, (*cit)->getCallSite(), curFunNode->getFunction()))
            {
                const InstVec& nextInsts = (*cit)->getCallSite()->getSuccInstructions();
                for (const auto& ni : nextInsts)
                {
                    CxtThreadStmt newCts(cts.getTid(), newCxt, ni);
                    addInterleavingThread(newCts, cts);
                }
            }
        }
        for (PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->indirectCallsBegin(),
                ecit = (edge)->indirectCallsEnd();
                cit != ecit; ++cit)
        {
            CallStrCxt newCxt = cts.getContext();
            if (matchCxt(newCxt, (*cit)->getCallSite(), curFunNode->getFunction()))
            {
                const InstVec& nextInsts = (*cit)->getCallSite()->getSuccInstructions();
                for (const auto& ni: nextInsts)
                {
                    CxtThreadStmt newCts(cts.getTid(), newCxt, ni);
                    addInterleavingThread(newCts, cts);
                }
            }
        }
    }
}

/*!
 * Handling intraprocedural statements (successive statements on the CFG )
 */
void MHP::handleIntra(const CxtThreadStmt& cts)
{

    const InstVec& nextInsts = cts.getStmt()->getSuccInstructions();
    for (const auto& ni: nextInsts)
    {
        CxtThreadStmt newCts(cts.getTid(), cts.getContext(), ni);
        addInterleavingThread(newCts, cts);
    }
}

/*!
 * Update interleavings of ancestor threads according to TCT
 */
void MHP::updateAncestorThreads(NodeID curTid)
{
    NodeBS tds = tct->getAncestorThread(curTid);
    DBOUT(DMTA, outs() << "##Ancestor thread of " << curTid << " is : ");
    DBOUT(DMTA, dumpSet(tds));
    DBOUT(DMTA, outs() << "\n");
    tds.set(curTid);

    for (const unsigned i : tds)
    {
        const CxtThread& ct = tct->getTCTNode(i)->getCxtThread();
        if (const SVFInstruction* forkInst = ct.getThread())
        {
            CallStrCxt forkSiteCxt = tct->getCxtOfCxtThread(ct);
            const InstVec& nextInsts = forkInst->getSuccInstructions();
            for (const auto& ni: nextInsts)
            {
                CxtThreadStmt cts(tct->getParentThread(i), forkSiteCxt, ni);
                addInterleavingThread(cts, curTid);
            }
        }
    }
}

/*!
 * Update interleavings of sibling threads according to TCT
 *
 * Exclude sibling thread that never happen in parallel based on ForkJoinAnalysis
 *
 * The interleaving of a thread t is not unnecessary to be updated if
 * (1) t HB Sibling and t fully joins curTid recusively
 * or
 * (2) Sibling HB t
 */
void MHP::updateSiblingThreads(NodeID curTid)
{
    NodeBS tds = tct->getAncestorThread(curTid);
    tds.set(curTid);
    for (const unsigned tid : tds)
    {
        NodeBS siblingTds = tct->getSiblingThread(tid);
        for (const unsigned stid : siblingTds)
        {
            if ((isHBPair(tid, stid) && isRecurFullJoin(tid, curTid)) || isHBPair(stid, tid))
                continue;

            const CxtThread& ct = tct->getTCTNode(stid)->getCxtThread();
            const SVFFunction* routine = tct->getStartRoutineOfCxtThread(ct);
            const SVFInstruction* stmt = routine->getEntryBlock()->front();
            CxtThreadStmt cts(stid, ct.getContext(), stmt);
            addInterleavingThread(cts, curTid);
        }

        DBOUT(DMTA, outs() << "##Sibling thread of " << curTid << " is : ");
        DBOUT(DMTA, dumpSet(siblingTds));
        DBOUT(DMTA, outs() << "\n");
    }
}

/*!
 * Whether curTid can be fully joined by parentTid recursively
 */
bool MHP::isRecurFullJoin(NodeID parentTid, NodeID curTid)
{
    if (parentTid == curTid)
        return true;

    const TCTNode* curNode = tct->getTCTNode(curTid);
    FIFOWorkList<const TCTNode*> worklist;
    worklist.push(curNode);
    while (!worklist.empty())
    {
        const TCTNode* node = worklist.pop();
        for (TCTEdge* edge : node->getInEdges())
        {
            NodeID srcID = edge->getSrcID();
            if (fja->isFullJoin(srcID, node->getId()))
            {
                if (srcID == parentTid)
                    return true;
                else
                    worklist.push(edge->getSrcNode());
            }
            else
            {
                return false;
            }
        }
    }
    return false;
}

/*!
 * A join site must join t if
 * (1) t is not a multiforked thread
 * (2) the join site of t is not in recursion
 */
bool MHP::isMustJoin(NodeID curTid, const SVFInstruction* joinsite)
{
    assert(isTDJoin(joinsite) && "not a join site!");
    return !isMultiForkedThread(curTid) && !tct->isJoinSiteInRecursion(joinsite);
}

/*!
 * Return thread id(s) which are directly or indirectly joined at this join site
 */
NodeBS MHP::getDirAndIndJoinedTid(const CallStrCxt& cxt, const SVFInstruction* call)
{
    CxtStmt cs(cxt, call);
    return fja->getDirAndIndJoinedTid(cs);
}

/*!
 *  Whether a context-sensitive join satisfies symmetric loop pattern
 */
bool MHP::hasJoinInSymmetricLoop(const CallStrCxt& cxt, const SVFInstruction* call) const
{
    CxtStmt cs(cxt, call);
    return fja->hasJoinInSymmetricLoop(cs);
}

/// Whether a context-sensitive join satisfies symmetric loop pattern
const MHP::LoopBBs& MHP::getJoinInSymmetricLoop(const CallStrCxt& cxt, const SVFInstruction* call) const
{
    CxtStmt cs(cxt, call);
    return fja->getJoinInSymmetricLoop(cs);
}

/*!
 * Whether two thread t1 happens-fore t2
 */
bool MHP::isHBPair(NodeID tid1, NodeID tid2)
{
    return fja->isHBPair(tid1, tid2);
}

bool MHP::isConnectedfromMain(const SVFFunction* fun)
{
    PTACallGraphNode* cgnode = tcg->getCallGraphNode(fun);
    FIFOWorkList<const PTACallGraphNode*> worklist;
    TCT::PTACGNodeSet visited;
    worklist.push(cgnode);
    visited.insert(cgnode);
    while (!worklist.empty())
    {
        const PTACallGraphNode* node = worklist.pop();
        if ("main" == node->getFunction()->getName())
            return true;
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
    return false;
}

/*!
 * Answer MHP queries
 * For a pair of ThreadStmts
 * (t1,s1) = <l1>
 * (t2,s2) = <l2>
 * They may happen in parallel if
 * (1) t1 == t2 and t1 inloop/incycle
 * (2) t1!=t2 and t1 \in l2 and t2 \in l1
 */

bool MHP::mayHappenInParallelInst(const SVFInstruction* i1, const SVFInstruction* i2)
{

    /// TODO: Any instruction in dead function is assumed no MHP with others
    if (!hasThreadStmtSet(i1) || !hasThreadStmtSet(i2))
        return false;

    const CxtThreadStmtSet& tsSet1 = getThreadStmtSet(i1);
    const CxtThreadStmtSet& tsSet2 = getThreadStmtSet(i2);
    for (const CxtThreadStmt& ts1 : tsSet1)
    {
        NodeBS l1 = getInterleavingThreads(ts1);
        for (const CxtThreadStmt& ts2 : tsSet2)
        {
            NodeBS l2 = getInterleavingThreads(ts2);
            if (ts1.getTid() != ts2.getTid())
            {
                if (l1.test(ts2.getTid()) && l2.test(ts1.getTid()))
                {
                    numOfMHPQueries++;
                    return true;
                }
            }
            else
            {
                if (isMultiForkedThread(ts1.getTid()))
                {
                    numOfMHPQueries++;
                    return true;
                }
            }
        }
    }
    return false;
}

bool MHP::mayHappenInParallelCache(const SVFInstruction* i1, const SVFInstruction* i2)
{
    if (!tct->isCandidateFun(i1->getParent()->getParent()) && !tct->isCandidateFun(i2->getParent()->getParent()))
    {
        FuncPair funpair = std::make_pair(i1->getFunction(), i2->getFunction());
        FuncPairToBool::const_iterator it = nonCandidateFuncMHPRelMap.find(funpair);
        if (it == nonCandidateFuncMHPRelMap.end())
        {
            bool mhp = mayHappenInParallelInst(i1, i2);
            nonCandidateFuncMHPRelMap[funpair] = mhp;
            return mhp;
        }
        else
        {
            if (it->second)
                numOfMHPQueries++;
            return it->second;
        }
    }
    return mayHappenInParallelInst(i1, i2);
}

bool MHP::mayHappenInParallel(const SVFInstruction* i1, const SVFInstruction* i2)
{
    numOfTotalQueries++;

    DOTIMESTAT(double queryStart = PTAStat::getClk(true));
    bool mhp = mayHappenInParallelCache(i1, i2);
    DOTIMESTAT(double queryEnd = PTAStat::getClk(true));
    DOTIMESTAT(interleavingQueriesTime += (queryEnd - queryStart) / TIMEINTERVAL);

    return mhp;
}

bool MHP::executedByTheSameThread(const SVFInstruction* i1, const SVFInstruction* i2)
{
    if (!hasThreadStmtSet(i1) || !hasThreadStmtSet(i2))
        return true;

    const CxtThreadStmtSet& tsSet1 = getThreadStmtSet(i1);
    const CxtThreadStmtSet& tsSet2 = getThreadStmtSet(i2);
    for (const CxtThreadStmt&ts1 : tsSet1)
    {
        for (const CxtThreadStmt& ts2 : tsSet2)
        {
            if (ts1.getTid() != ts2.getTid() || isMultiForkedThread(ts1.getTid()))
                return false;
        }
    }
    return true;
}

/*!
 * Print interleaving results
 */
void MHP::printInterleaving()
{
    for (const auto& pair : threadStmtToTheadInterLeav)
    {
        outs() << "( t" << pair.first.getTid()
               << " , $" << pair.first.getStmt()->getSourceLoc()
               << "$" << pair.first.getStmt()->toString() << " ) ==> [";
        for (unsigned i : pair.second)
        {
            outs() << " " << i << " ";
        }
        outs() << "]\n";
    }
}

/*!
 * Collect SCEV pass information for pointers at fork/join sites
 * Because ScalarEvolution is a function pass, previous knowledge of a function
 * may be overwritten when analyzing a new function. We use a
 * internal wrapper class PTASCEV to record all the necessary information for determining symmetric fork/join inside loops
 */
void ForkJoinAnalysis::collectSCEVInfo()
{
    typedef Set<const SVFInstruction*> CallInstSet;
    typedef Map<const SVFFunction*, CallInstSet> FunToFJSites;
    FunToFJSites funToFJSites;

    for (ThreadCallGraph::CallSiteSet::const_iterator it = tct->getThreadCallGraph()->forksitesBegin(),
            eit = tct->getThreadCallGraph()->forksitesEnd();
            it != eit; ++it)
    {
        const SVFInstruction* fork = (*it)->getCallSite();
        funToFJSites[fork->getFunction()].insert(fork);
    }

    for (ThreadCallGraph::CallSiteSet::const_iterator it = tct->getThreadCallGraph()->joinsitesBegin(),
            eit = tct->getThreadCallGraph()->joinsitesEnd();
            it != eit; ++it)
    {
        const SVFInstruction* join = (*it)->getCallSite();
        funToFJSites[join->getFunction()].insert(join);
    }

    // for(FunToFJSites::const_iterator it = funToFJSites.begin(), eit = funToFJSites.end(); it!=eit; ++it)
    // {
    //     // ScalarEvolution* SE = MTA::getSE(it->first);
    //     for(CallInstSet::const_iterator sit = it->second.begin(), esit = it->second.end(); sit!=esit; ++sit)
    //     {
    //         const SVFInstruction* callInst =  *sit;
    //         if(tct->getThreadCallGraph()->isForksite(getCBN(callInst)))
    //         {
    //             // const SVFValue* forkSiteTidPtr = getForkedThread(callInst);
    //             // const SCEV *forkSiteTidPtrSCEV = SE->getSCEV(const_cast<Value*>(forkSiteTidPtr));
    //             // const SCEV *baseForkTidPtrSCEV = SE->getSCEV(const_cast<Value*>(getBasePtr(forkSiteTidPtr)));
    //             // forkSiteTidPtrSCEV = getSCEVMinusExpr(forkSiteTidPtrSCEV, baseForkTidPtrSCEV, SE);
    //             // PTASCEV scev(forkSiteTidPtr,nullptr,nullptr);
    //             // fkjnToPTASCEVMap.insert(std::make_pair(callInst,scev));
    //         }
    //         else
    //         {
    //             // const SVFValue* joinSiteTidPtr = getJoinedThread(callInst);
    //             //const SCEV *joinSiteTidPtrSCEV = SE->getSCEV(const_cast<Value*>(joinSiteTidPtr));
    //             //const SCEV *baseJoinTidPtrSCEV = SE->getSCEV(const_cast<Value*>(getBasePtr(joinSiteTidPtr)));
    //             //joinSiteTidPtrSCEV = getSCEVMinusExpr(joinSiteTidPtrSCEV, baseJoinTidPtrSCEV, SE);

    //             // PTASCEV scev(joinSiteTidPtr,nullptr,nullptr);
    //             // fkjnToPTASCEVMap.insert(std::make_pair(callInst,scev));
    //         }
    //     }
    // }
}

/*!
 * Context-sensitive forward traversal from each fork site
 */
void ForkJoinAnalysis::analyzeForkJoinPair()
{
    for (const std::pair<const NodeID, TCTNode*>& tpair : *tct)
    {
        const CxtThread& ct = tpair.second->getCxtThread();
        const NodeID rootTid = tpair.first;
        clearFlagMap();
        if (const SVFInstruction* forkInst = ct.getThread())
        {
            CallStrCxt forkSiteCxt = tct->getCxtOfCxtThread(ct);
            const SVFInstruction* exitInst = getExitInstOfParentRoutineFun(rootTid);

            const InstVec& nextInsts = forkInst->getSuccInstructions();
            for (const SVFInstruction* ni : nextInsts)
            {
                CxtStmt cs(forkSiteCxt, ni);
                markCxtStmtFlag(cs, TDAlive);
            }

            while (!cxtStmtList.empty())
            {
                CxtStmt cts = popFromCTSWorkList();
                const SVFInstruction* curInst = cts.getStmt();
                DBOUT(DMTA, outs() << "-----\nForkJoinAnalysis root thread: " << tpair.first << " ");
                DBOUT(DMTA, cts.dump());
                DBOUT(DMTA, outs() << "-----\n");
                PTACallGraph::FunctionSet callees;
                if (isTDFork(curInst))
                {
                    handleFork(cts, rootTid);
                }
                else if (isTDJoin(curInst))
                {
                    handleJoin(cts, rootTid);
                }
                else if (SVFUtil::isCallSite(curInst) && tct->isCandidateFun(getCallee(curInst, callees)))
                {

                    handleCall(cts, rootTid);
                }
                else if (curInst->isRetInst())
                {
                    handleRet(cts);
                }
                else
                {
                    handleIntra(cts);
                }

                if (curInst == exitInst)
                {
                    if (getMarkedFlag(cts) != TDAlive)
                        addToFullJoin(tct->getParentThread(rootTid), rootTid);
                    else
                        addToPartial(tct->getParentThread(rootTid), rootTid);
                }
            }
        }
    }
}

/// Handle fork
void ForkJoinAnalysis::handleFork(const CxtStmt& cts, NodeID rootTid)
{
    const SVFInstruction* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDFork(call));
    CallICFGNode* cbn = getCBN(call);
    if (getTCG()->hasThreadForkEdge(cbn))
    {
        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = getTCG()->getForkEdgeBegin(cbn),
                ecgIt = getTCG()->getForkEdgeEnd(cbn);
                cgIt != ecgIt; ++cgIt)
        {
            const SVFFunction* callee = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, callee);
            CxtThread ct(newCxt, call);
            if (getMarkedFlag(cts) != TDAlive)
                addToHBPair(rootTid, tct->getTCTNode(ct)->getId());
            else
                addToHPPair(rootTid, tct->getTCTNode(ct)->getId());
        }
    }
    handleIntra(cts);
}

/// Handle join
void ForkJoinAnalysis::handleJoin(const CxtStmt& cts, NodeID rootTid)
{
    const SVFInstruction* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDJoin(call));
    CallICFGNode* cbn = getCBN(call);
    if (getTCG()->hasCallGraphEdge(cbn))
    {
        const SVFInstruction* forkSite = tct->getTCTNode(rootTid)->getCxtThread().getThread();
        const SVFInstruction* joinSite = cts.getStmt();

        if (isAliasedForkJoin(forkSite, joinSite))
        {
            if (hasJoinLoop(joinSite))
            {
                LoopBBs& joinLoop = getJoinLoop(joinSite);
                std::vector<const SVFBasicBlock *> exitbbs;
                joinSite->getFunction()->getExitBlocksOfLoop(joinSite->getParent(), exitbbs);
                while (!exitbbs.empty())
                {
                    const SVFBasicBlock* eb = exitbbs.back();
                    exitbbs.pop_back();
                    const SVFInstruction* svfEntryInst = eb->front();
                    CxtStmt newCts(curCxt, svfEntryInst);
                    addDirectlyJoinTID(cts, rootTid);
                    if (isSameSCEV(forkSite, joinSite))
                    {
                        markCxtStmtFlag(newCts, TDDead);
                        addSymmetricLoopJoin(cts, joinLoop);
                    }
                    else
                        markCxtStmtFlag(cts, TDAlive);
                }
            }
            else
            {
                markCxtStmtFlag(cts, TDDead);
                addDirectlyJoinTID(cts, rootTid);
                DBOUT(DMTA, outs() << "\n\t match join site " << call->toString() << "for thread " << rootTid << "\n");
            }
        }
        /// for the join site in a loop loop which does not join the current thread
        /// we process the loop exit
        else
        {
            if (hasJoinLoop(joinSite))
            {
                std::vector<const SVFBasicBlock*> exitbbs;
                joinSite->getFunction()->getExitBlocksOfLoop(joinSite->getParent(), exitbbs);
                while (!exitbbs.empty())
                {
                    const SVFBasicBlock* eb = exitbbs.back();
                    exitbbs.pop_back();
                    const SVFInstruction* svfEntryInst = eb->front();
                    CxtStmt newCts(curCxt, svfEntryInst);
                    markCxtStmtFlag(newCts, cts);
                }
            }
        }
    }
    handleIntra(cts);
}

/// Handle call
void ForkJoinAnalysis::handleCall(const CxtStmt& cts, NodeID rootTid)
{

    const SVFInstruction* call = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();
    CallICFGNode* cbn = getCBN(call);
    if (getTCG()->hasCallGraphEdge(cbn))
    {
        for (PTACallGraph::CallGraphEdgeSet::const_iterator cgIt = getTCG()->getCallEdgeBegin(cbn),
                ecgIt = getTCG()->getCallEdgeEnd(cbn);
                cgIt != ecgIt; ++cgIt)
        {
            const SVFFunction* svfcallee = (*cgIt)->getDstNode()->getFunction();
            if (isExtCall(svfcallee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt, call, svfcallee);
            const SVFInstruction* svfEntryInst = svfcallee->getEntryBlock()->front();
            CxtStmt newCts(newCxt, svfEntryInst);
            markCxtStmtFlag(newCts, cts);
        }
    }
}

/// Handle return
void ForkJoinAnalysis::handleRet(const CxtStmt& cts)
{

    const SVFInstruction* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    PTACallGraphNode* curFunNode = getTCG()->getCallGraphNode(curInst->getFunction());
    for (PTACallGraphEdge* edge : curFunNode->getInEdges())
    {
        if (SVFUtil::isa<ThreadForkEdge, ThreadJoinEdge>(edge))
            continue;
        for (PTACallGraphEdge::CallInstSet::const_iterator cit = edge->directCallsBegin(),
                ecit = edge->directCallsEnd();
                cit != ecit; ++cit)
        {
            CallStrCxt newCxt = curCxt;
            if (matchCxt(newCxt, (*cit)->getCallSite(), curFunNode->getFunction()))
            {
                const InstVec& nextInsts = (*cit)->getCallSite()->getSuccInstructions();
                for (const auto& ni : nextInsts)
                {
                    CxtStmt newCts(newCxt, ni);
                    markCxtStmtFlag(newCts, cts);
                }
            }
        }
        for (PTACallGraphEdge::CallInstSet::const_iterator cit = edge->indirectCallsBegin(),
                ecit = edge->indirectCallsEnd();
                cit != ecit; ++cit)
        {
            CallStrCxt newCxt = curCxt;
            if (matchCxt(newCxt, (*cit)->getCallSite(), curFunNode->getFunction()))
            {
                const InstVec& nextInsts = (*cit)->getCallSite()->getSuccInstructions();
                for (const auto& ni : nextInsts)
                {
                    CxtStmt newCts(newCxt, ni);
                    markCxtStmtFlag(newCts, cts);
                }
            }
        }
    }
}

/// Handle intra
void ForkJoinAnalysis::handleIntra(const CxtStmt& cts)
{

    const SVFInstruction* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    const InstVec& nextInsts = curInst->getSuccInstructions();
    for (const auto& ni: nextInsts)
    {
        CxtStmt newCts(curCxt, ni);
        markCxtStmtFlag(newCts, cts);
    }
}

/*!
 * Return thread id(s) which are joined at this join site
 * (1) thread t1 directly joins thread t2
 * (2) thread t1 indirectly joins thread t3 via directly joining t2 (t2 fully joins its child thread t3)
 */
NodeBS ForkJoinAnalysis::getDirAndIndJoinedTid(const CxtStmt& cs)
{

    CxtStmtToTIDMap::const_iterator it = dirAndIndJoinMap.find(cs);
    if (it != dirAndIndJoinMap.end())
        return it->second;

    const NodeBS& directJoinTids = getDirectlyJoinedTid(cs);
    NodeBS allJoinTids = directJoinTids;

    FIFOWorkList<NodeID> worklist;
    for (unsigned id : directJoinTids)
    {
        worklist.push(id);
    }

    while (!worklist.empty())
    {
        NodeID tid = worklist.pop();
        TCTNode* node = tct->getTCTNode(tid);
        for (TCT::ThreadCreateEdgeSet::const_iterator it = tct->getChildrenBegin(node), eit = tct->getChildrenEnd(node); it != eit; ++it)
        {
            NodeID childTid = (*it)->getDstID();
            if (isFullJoin(tid, childTid))
            {
                allJoinTids.set(childTid);
                worklist.push(childTid);
            }
        }
    }

    dirAndIndJoinMap[cs] = allJoinTids;

    return allJoinTids;
}

// static bool accessSameArrayIndex(const GetElementPtrInst* ptr1, const GetElementPtrInst* ptr2)
// {

//     std::vector<u32_t> ptr1vec;
//     for (gep_type_iterator gi = gep_type_begin(*ptr1), ge = gep_type_end(*ptr1);
//             gi != ge; ++gi)
//     {
//         if(SVFConstantInt* ci = SVFUtil::dyn_cast<SVFConstantInt>(LLVMModuleSet::getLLVMModuleSet()->getSVFValue(gi.getOperand())))
//         {
//             s32_t idx = ci->getSExtValue();
//             ptr1vec.push_back(idx);
//         }
//         else
//             return false;
//     }

//     std::vector<u32_t> ptr2vec;
//     for (gep_type_iterator gi = gep_type_begin(*ptr2), ge = gep_type_end(*ptr2);
//             gi != ge; ++gi)
//     {
//         if(SVFConstantInt* ci = SVFUtil::dyn_cast<SVFConstantInt>(LLVMModuleSet::getLLVMModuleSet()->getSVFValue(gi.getOperand())))
//         {
//             s32_t idx = ci->getSExtValue();
//             ptr2vec.push_back(idx);
//         }
//         else
//             return false;
//     }

//     return ptr1vec==ptr2vec;
// }

/*!
 * We assume a pair of fork and join sites are must-alias if they have same PTASCEV
 * (1) SCEV not inside loop
 * (2) SCEV inside two symmetric loops, then
 *  pointers of fork thread and join thread should have same scev start and step.
 *  and should have same loop trip count
 */
bool ForkJoinAnalysis::isSameSCEV(const SVFInstruction* forkSite, const SVFInstruction* joinSite)
{

    // const PTASCEV& forkse = fkjnToPTASCEVMap[forkSite];
    // const PTASCEV& joinse = fkjnToPTASCEVMap[joinSite];

    // //if(sameLoopTripCount(forkSite,joinSite) == false)
    // //  return false;

    // if(forkse.inloop && joinse.inloop)
    //     return forkse.start==joinse.start && forkse.step == joinse.step && forkse.tripcount <= joinse.tripcount;
    // else if(SVFUtil::isa<GetElementPtrInst>(forkse.ptr) && SVFUtil::isa<GetElementPtrInst>(joinse.ptr))
    //     return accessSameArrayIndex(SVFUtil::cast<GetElementPtrInst>(forkse.ptr),SVFUtil::cast<GetElementPtrInst>(joinse.ptr));
    // else if(SVFUtil::isa<GetElementPtrInst, GetElementPtrInst>(joinse.ptr))
    //     return false;
    // else
    //     return true;

    return false;
}

/*!
 * The fork and join have same loop trip count
 */
bool ForkJoinAnalysis::sameLoopTripCount(const SVFInstruction* forkSite, const SVFInstruction* joinSite)
{

    // ScalarEvolution* forkSE = getSE(forkSite);
    // ScalarEvolution* joinSE = getSE(joinSite);

    // if(tct->hasLoop(forkSite) == false || tct->hasLoop(joinSite) == false)
    //     return false;

    // // Get loops
    // const LoopBBs& forkSiteLoop = tct->getLoop(forkSite);
    // const LoopBBs& joinSiteLoop = tct->getLoop(joinSite);

    // const SCEV* forkLoopCountScev = forkSE->getBackedgeTakenCount(forkSiteLoop);
    // const SCEV* joinLoopCountScev = joinSE->getBackedgeTakenCount(joinSiteLoop);

    // if(forkLoopCountScev!=forkSE->getCouldNotCompute())
    // {
    //     if(forkLoopCountScev==joinLoopCountScev)
    //     {
    //         return true;
    //     }
    // }
    return false;
}
