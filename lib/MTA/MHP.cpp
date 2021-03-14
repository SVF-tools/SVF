/*
 * MTA.cpp
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTA/MHP.h"
#include "MTA/MTA.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTAResultValidator.h"
#include "Util/SVFUtil.h"


using namespace SVF;
using namespace SVFUtil;


/*!
 * Get the base pointer from any GEP.
 */
static const Value *getBasePtr(const Value *v)
{
    const GetElementPtrInst *GEP = SVFUtil::dyn_cast<GetElementPtrInst>(v);
    while (GEP)
    {
        v = GEP->getOperand(0);
        GEP = SVFUtil::dyn_cast<GetElementPtrInst>(v);
    }
    return v;
}


/*!
 * Compute a SCEV that represents the subtraction of two given SCEVs.
 */
static const SCEV *getSCEVMinusExpr(const SCEV *s1,const SCEV *s2, ScalarEvolution *SE)
{
    if (SE->getCouldNotCompute() == s1 || SE->getCouldNotCompute() == s2)
        return SE->getCouldNotCompute();

    Type *t1 = SE->getEffectiveSCEVType(s1->getType());
    Type *t2 = SE->getEffectiveSCEVType(s2->getType());
    if (t1 != t2)
    {
        if (SE->getTypeSizeInBits(t1) < SE->getTypeSizeInBits(t2))
            s1 = SE->getSignExtendExpr(s1, t2);
        else
            s2 = SE->getSignExtendExpr(s2, t1);
    }

    return SE->getMinusSCEV(s1, s2);
}

namespace SVF
{

// Subclassing RCResultValidator to define the abstract methods.
class MHPValidator : public RaceResultValidator
{
public:
    MHPValidator(MHP *mhp) :mhp(mhp)
    {
    }
    bool mayHappenInParallel(const Instruction *I1, const Instruction *I2)
    {
        return mhp->mayHappenInParallel(I1, I2);
    }
private:
    MHP *mhp;
};

} // End namespace SVF

/*!
 * Constructor
 */
MHP::MHP(TCT* t) :tcg(t->getThreadCallGraph()),tct(t),numOfTotalQueries(0),numOfMHPQueries(0),
    interleavingTime(0),interleavingQueriesTime(0)
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
    for(TCT::const_iterator it = tct->begin(), eit = tct->end(); it!=eit; ++it)
    {
        const CxtThread& ct = it->second->getCxtThread();
        NodeID rootTid = it->first;
        const Function* routine = tct->getStartRoutineOfCxtThread(ct);
        CxtThreadStmt rootcts(rootTid,ct.getContext(),&(routine->getEntryBlock().front()));

        addInterleavingThread(rootcts,rootTid);
        updateAncestorThreads(rootTid);
        updateSiblingThreads(rootTid);

        while(!cxtStmtList.empty())
        {
            CxtThreadStmt cts = popFromCTSWorkList();
            const Instruction* curInst = cts.getStmt();
            DBOUT(DMTA,outs() << "-----\nMHP analysis root thread: " << rootTid << " ");
            DBOUT(DMTA,cts.dump());
            DBOUT(DMTA,outs() << "current thread interleaving: < ");
            DBOUT(DMTA,dumpSet(getInterleavingThreads(cts)));
            DBOUT(DMTA,outs() << " >\n-----\n");

            /// handle non-candidate function
            if (!tct->isCandidateFun(curInst->getParent()->getParent()))
            {
                handleNonCandidateFun(cts);
            }
            /// handle candidate function
            else
            {
                if(isTDFork(curInst))
                {
                    handleFork(cts,rootTid);
                }
                else if(isTDJoin(curInst))
                {
                    handleJoin(cts,rootTid);
                }
                else if(SVFUtil::isa<CallInst>(curInst) && !isExtCall(curInst))
                {
                    handleCall(cts,rootTid);
                    if(!tct->isCandidateFun(getCallee(curInst)))
                        handleIntra(cts);
                }
                else if(SVFUtil::isa<ReturnInst>(curInst))
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


    if(Options::PrintInterLev)
        printInterleaving();

    validateResults();
}

/*!
 * Update non-candidate functions' interleaving
 */
void MHP::updateNonCandidateFunInterleaving()
{
    SVFModule* module = tcg->getModule();
    for (SVFModule::iterator F = module->begin(), E = module->end(); F != E; ++F)
    {
        const Function* fun = *F;
        if (!tct->isCandidateFun(fun) && !isExtCall(fun))
        {
            const Instruction *entryinst = &(fun->getEntryBlock().front());
            if (!hasThreadStmtSet(entryinst))
                continue;

            const CxtThreadStmtSet& tsSet = getThreadStmtSet(entryinst);

            for (CxtThreadStmtSet::const_iterator it1 = tsSet.begin(), eit1 = tsSet.end(); it1 != eit1; ++it1)
            {
                const CxtThreadStmt& cts = *it1;
                const CallStrCxt& curCxt = cts.getContext();

                for (const_inst_iterator II = inst_begin(fun), EE = inst_end(fun); II != EE; ++II)
                {
                    const Instruction *inst = &*II;
                    if (inst == entryinst)
                        continue;
                    CxtThreadStmt newCts(cts.getTid(), curCxt, inst);
                    threadStmtToTheadInterLeav[newCts] |= threadStmtToTheadInterLeav[cts];
                    instToTSMap[inst].insert(newCts);
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
    const Instruction* curInst = cts.getStmt();
    const Function* curfun = curInst->getParent()->getParent();
    assert(curInst == &(curfun->getEntryBlock().front()) && "curInst is not the entry of non candidate function.");
    const CallStrCxt& curCxt = cts.getContext();
    PTACallGraphNode* node = tcg->getCallGraphNode(curfun);
    for (PTACallGraphNode::const_iterator nit = node->OutEdgeBegin(), neit = node->OutEdgeEnd(); nit != neit; nit++)
    {
        const Function* callee = (*nit)->getDstNode()->getFunction();
        if (!isExtCall(callee))
        {
            CxtThreadStmt newCts(cts.getTid(), curCxt, &(callee->getEntryBlock().front()));
            addInterleavingThread(newCts, cts);
        }
    }
}

/*!
 * Handle fork
 */
void MHP::handleFork(const CxtThreadStmt& cts, NodeID rootTid)
{

    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDFork(call));
    if(tct->getThreadCallGraph()->hasCallGraphEdge(call))
    {
        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = tcg->getForkEdgeBegin(call),
                ecgIt = tcg->getForkEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const Function* routine = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt,call,routine);
            const Instruction* stmt = &(routine->getEntryBlock().front());
            CxtThread ct(newCxt,call);
            CxtThreadStmt newcts(tct->getTCTNode(ct)->getId(),ct.getContext(),stmt);
            addInterleavingThread(newcts,cts);
        }
    }
    handleIntra(cts);
}

/*!
 * Handle join
 */
void MHP::handleJoin(const CxtThreadStmt& cts, NodeID rootTid)
{

    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDJoin(call));

    NodeBS joinedTids = getDirAndIndJoinedTid(curCxt,call);
    if(!joinedTids.empty())
    {
        if(const Loop* joinLoop = fja->getJoinLoop(call))
        {
            SmallBBVector exitbbs;
            joinLoop->getExitBlocks(exitbbs);
            while(!exitbbs.empty())
            {
                BasicBlock* eb = exitbbs.pop_back_val();
                CxtThreadStmt newCts(cts.getTid(),curCxt,&(eb->front()));
                addInterleavingThread(newCts,cts);
                if(isJoinInSymmetricLoop(curCxt,call))
                    rmInterleavingThread(newCts,joinedTids,call);
            }
        }
        else
        {
            rmInterleavingThread(cts,joinedTids,call);
            DBOUT(DMTA,outs() << "\n\t match join site " << *call <<  " for thread " << rootTid << "\n");
        }
    }
    /// for the join site in a loop loop which does not join the current thread
    /// we process the loop exit
    else
    {
        if(const Loop* joinLoop = fja->getJoinLoop(call))
        {
            SmallBBVector exitbbs;
            joinLoop->getExitBlocks(exitbbs);
            while(!exitbbs.empty())
            {
                BasicBlock* eb = exitbbs.pop_back_val();
                CxtThreadStmt newCts(cts.getTid(),cts.getContext(),&(eb->front()));
                addInterleavingThread(newCts,cts);
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

    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();

    if(tct->getThreadCallGraph()->hasCallGraphEdge(call))
    {
        for (PTACallGraph::CallGraphEdgeSet::const_iterator cgIt = tcg->getCallEdgeBegin(call),
                ecgIt = tcg->getCallEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const Function* callee = (*cgIt)->getDstNode()->getFunction();
            if (isExtCall(callee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt,call,callee);
            CxtThreadStmt newCts(cts.getTid(),newCxt,&(callee->getEntryBlock().front()));
            addInterleavingThread(newCts,cts);
        }
    }
}

/*!
 * Handle return instruction in the current thread scope (excluding any join site)
 */
void MHP::handleRet(const CxtThreadStmt& cts)
{

    PTACallGraphNode* curFunNode = tcg->getCallGraphNode(cts.getStmt()->getParent()->getParent());
    for(PTACallGraphNode::const_iterator it = curFunNode->getInEdges().begin(), eit = curFunNode->getInEdges().end(); it!=eit; ++it)
    {
        PTACallGraphEdge* edge = *it;
        if(SVFUtil::isa<ThreadForkEdge>(edge) || SVFUtil::isa<ThreadJoinEdge>(edge))
            continue;
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->directCallsBegin(),
                ecit = (edge)->directCallsEnd(); cit!=ecit; ++cit)
        {
            CallStrCxt newCxt = cts.getContext();
            if(matchCxt(newCxt,*cit,curFunNode->getFunction()))
            {
                InstVec nextInsts;
                getNextInsts(*cit,nextInsts);
                for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
                {
                    CxtThreadStmt newCts(cts.getTid(),newCxt,*nit);
                    addInterleavingThread(newCts,cts);
                }
            }
        }
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->indirectCallsBegin(),
                ecit = (edge)->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            CallStrCxt newCxt = cts.getContext();
            if(matchCxt(newCxt,*cit,curFunNode->getFunction()))
            {
                InstVec nextInsts;
                getNextInsts(*cit,nextInsts);
                for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
                {
                    CxtThreadStmt newCts(cts.getTid(),newCxt,*nit);
                    addInterleavingThread(newCts,cts);
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

    InstVec nextInsts;
    getNextInsts(cts.getStmt(),nextInsts);
    for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
    {
        CxtThreadStmt newCts(cts.getTid(),cts.getContext(),*nit);
        addInterleavingThread(newCts,cts);
    }
}


/*!
 * Update interleavings of ancestor threads according to TCT
 */
void MHP::updateAncestorThreads(NodeID curTid)
{
    NodeBS tds = tct->getAncestorThread(curTid);
    DBOUT(DMTA,outs() << "##Ancestor thread of " << curTid << " is : ");
    DBOUT(DMTA,dumpSet(tds));
    DBOUT(DMTA,outs() << "\n");
    tds.set(curTid);

    for(NodeBS::iterator it = tds.begin(), eit = tds.end(); it!=eit; ++it)
    {
        const CxtThread& ct = tct->getTCTNode(*it)->getCxtThread();
        if(const CallInst* forkInst = ct.getThread())
        {
            CallStrCxt forkSiteCxt = tct->getCxtOfCxtThread(ct);
            InstVec nextInsts;
            getNextInsts(forkInst,nextInsts);
            for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
            {
                CxtThreadStmt cts(tct->getParentThread(*it),forkSiteCxt,*nit);
                addInterleavingThread(cts,curTid);
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
    for(NodeBS::iterator cit = tds.begin(), ecit = tds.end(); cit!=ecit; ++cit)
    {
        NodeBS  siblingTds = tct->getSiblingThread(*cit);
        for(NodeBS::iterator it = siblingTds.begin(), eit = siblingTds.end(); it!=eit; ++it)
        {

            if((isHBPair(*cit,*it) && isRecurFullJoin(*cit,curTid)) || isHBPair(*it,*cit) )
                continue;

            const CxtThread& ct = tct->getTCTNode(*it)->getCxtThread();
            const Function* routine = tct->getStartRoutineOfCxtThread(ct);
            const Instruction* stmt = &(routine->getEntryBlock().front());
            CxtThreadStmt cts(*it,ct.getContext(),stmt);
            addInterleavingThread(cts,curTid);
        }

        DBOUT(DMTA,outs() << "##Sibling thread of " << curTid << " is : ");
        DBOUT(DMTA,dumpSet(siblingTds));
        DBOUT(DMTA,outs() << "\n");
    }
}

/*!
 * Whether curTid can be fully joined by parentTid recursively
 */
bool MHP::isRecurFullJoin(NodeID parentTid, NodeID curTid)
{
    if(parentTid==curTid)
        return true;

    const TCTNode* curNode = tct->getTCTNode(curTid);
    FIFOWorkList<const TCTNode*> worklist;
    worklist.push(curNode);
    while(!worklist.empty())
    {
        const TCTNode* node = worklist.pop();
        for(TCT::ThreadCreateEdgeSet::const_iterator it = node->getInEdges().begin(), eit = node->getInEdges().end(); it!=eit; ++it)
        {
            NodeID srcID = (*it)->getSrcID();
            if(fja->isFullJoin(srcID,node->getId()))
            {
                if(srcID == parentTid)
                    return true;
                else
                    worklist.push((*it)->getSrcNode());
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
bool MHP::isMustJoin(NodeID curTid, const Instruction* joinsite)
{
    assert(isTDJoin(joinsite) && "not a join site!");
    return !isMultiForkedThread(curTid) && !tct->isJoinSiteInRecursion(joinsite);
}

/*!
 * Return thread id(s) which are directly or indirectly joined at this join site
 */
NodeBS MHP::getDirAndIndJoinedTid(const CallStrCxt& cxt, const Instruction* call)
{
    CxtStmt cs(cxt,call);
    return fja->getDirAndIndJoinedTid(cs);
}

/*!
 *  Whether a context-sensitive join satisfies symmetric loop pattern
 */
const Loop* MHP::isJoinInSymmetricLoop(const CallStrCxt& cxt, const Instruction* call) const
{
    CxtStmt cs(cxt,call);
    return fja->isJoinInSymmetricLoop(cs);
}

/*!
 * Whether two thread t1 happens-fore t2
 */
bool MHP::isHBPair(NodeID tid1, NodeID tid2)
{
    return fja->isHBPair(tid1,tid2);
}



bool MHP::isConnectedfromMain(const Function* fun)
{
    PTACallGraphNode* cgnode = tcg->getCallGraphNode(fun);
    FIFOWorkList<const PTACallGraphNode*> worklist;
    TCT::PTACGNodeSet visited;
    worklist.push(cgnode);
    visited.insert(cgnode);
    while (!worklist.empty())
    {
        const PTACallGraphNode* node = worklist.pop();
        if("main" == node->getFunction()->getName())
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

bool MHP::mayHappenInParallelInst(const Instruction* i1, const Instruction* i2)
{

    /// TODO: Any instruction in dead function is assumed no MHP with others
    if(!hasThreadStmtSet(i1) || !hasThreadStmtSet(i2))
        return false;

    const CxtThreadStmtSet& tsSet1 = getThreadStmtSet(i1);
    const CxtThreadStmtSet& tsSet2 = getThreadStmtSet(i2);
    for(CxtThreadStmtSet::const_iterator it1 = tsSet1.begin(), eit1 = tsSet1.end(); it1!=eit1; ++it1)
    {
        const CxtThreadStmt& ts1 = *it1;
        NodeBS l1 = getInterleavingThreads(ts1);
        for(CxtThreadStmtSet::const_iterator it2 = tsSet2.begin(), eit2 = tsSet2.end(); it2!=eit2; ++it2)
        {
            const CxtThreadStmt& ts2 = *it2;
            NodeBS l2 = getInterleavingThreads(ts2);
            if(ts1.getTid()!=ts2.getTid())
            {
                if(l1.test(ts2.getTid()) && l2.test(ts1.getTid()))
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

bool MHP::mayHappenInParallelCache(const Instruction* i1, const Instruction* i2)
{
    if(!tct->isCandidateFun(i1->getParent()->getParent()) &&!tct->isCandidateFun(i2->getParent()->getParent()))
    {
        FuncPair funpair = std::make_pair(i1->getParent()->getParent(), i2->getParent()->getParent());
        FuncPairToBool::const_iterator it = nonCandidateFuncMHPRelMap.find(funpair);
        if (it==nonCandidateFuncMHPRelMap.end())
        {
            bool mhp = mayHappenInParallelInst(i1, i2);
            nonCandidateFuncMHPRelMap[funpair] = mhp;
            return mhp;
        }
        else
        {
            if(it->second)
                numOfMHPQueries++;
            return it->second;
        }
    }
    return mayHappenInParallelInst(i1,i2);
}

bool MHP::mayHappenInParallel(const Instruction* i1, const Instruction* i2)
{
    numOfTotalQueries++;

    DOTIMESTAT(double queryStart = PTAStat::getClk());
    bool mhp=mayHappenInParallelCache(i1,i2);
    DOTIMESTAT(double queryEnd = PTAStat::getClk());
    DOTIMESTAT(interleavingQueriesTime += (queryEnd - queryStart) / TIMEINTERVAL);

    return mhp;
}

bool MHP::executedByTheSameThread(const Instruction* i1, const Instruction* i2)
{
    if(!hasThreadStmtSet(i1) || !hasThreadStmtSet(i2))
        return true;

    const CxtThreadStmtSet& tsSet1 = getThreadStmtSet(i1);
    const CxtThreadStmtSet& tsSet2 = getThreadStmtSet(i2);
    for(CxtThreadStmtSet::const_iterator it1 = tsSet1.begin(), eit1 = tsSet1.end(); it1!=eit1; ++it1)
    {
        const CxtThreadStmt& ts1 = *it1;
        for(CxtThreadStmtSet::const_iterator it2 = tsSet2.begin(), eit2 = tsSet2.end(); it2!=eit2; ++it2)
        {
            const CxtThreadStmt& ts2 = *it2;
            if(ts1.getTid()!=ts2.getTid())
                return false;
            else if (isMultiForkedThread(ts1.getTid()))
                return false;
        }
    }
    return true;
}

void MHP::validateResults()
{

    // Initialize the validator and perform validation.
    MHPValidator validator(this);
    validator.init(this->getThreadCallGraph()->getModule());
    validator.analyze();

    MTAResultValidator MTAValidator(this);
    MTAValidator.analyze();
}

/*!
 * Print interleaving results
 */
void MHP::printInterleaving()
{
    for(ThreadStmtToThreadInterleav::const_iterator it = threadStmtToTheadInterLeav.begin(), eit = threadStmtToTheadInterLeav.end(); it!=eit; ++it)
    {
        outs() << "( t" << it->first.getTid() << " , $" << SVFUtil::getSourceLoc(it->first.getStmt()) << "$" << *(it->first.getStmt()) << " ) ==> [";
        for (NodeBS::iterator ii = it->second.begin(), ie = it->second.end();
                ii != ie; ii++)
        {
            outs() << " " << *ii << " ";
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
    typedef Set<const Instruction*> CallInstSet;
    typedef Map<const Function*, CallInstSet > FunToFJSites;
    FunToFJSites funToFJSites;

    for(ThreadCallGraph::CallSiteSet::iterator it = tct->getThreadCallGraph()->forksitesBegin(),
            eit = tct->getThreadCallGraph()->forksitesEnd(); it!=eit; ++it)
    {
        const Instruction* fork = *it;
        const Function* fun = fork->getParent()->getParent();
        funToFJSites[fun].insert(fork);
    }

    for(ThreadCallGraph::CallSiteSet::iterator it = tct->getThreadCallGraph()->joinsitesBegin(),
            eit = tct->getThreadCallGraph()->joinsitesEnd(); it!=eit; ++it)
    {
        const Instruction* join = *it;
        funToFJSites[join->getParent()->getParent()].insert(join);
    }

    for(FunToFJSites::iterator it = funToFJSites.begin(), eit = funToFJSites.end(); it!=eit; ++it)
    {
        ScalarEvolution* SE = MTA::getSE(it->first);
        for(CallInstSet::iterator sit = it->second.begin(), esit = it->second.end(); sit!=esit; ++sit)
        {
            const Instruction* callInst =  *sit;
            if(tct->getThreadCallGraph()->isForksite(callInst))
            {
                const Value *forkSiteTidPtr = getForkedThread(callInst);
                const SCEV *forkSiteTidPtrSCEV = SE->getSCEV(const_cast<Value*>(forkSiteTidPtr));
                const SCEV *baseForkTidPtrSCEV = SE->getSCEV(const_cast<Value*>(getBasePtr(forkSiteTidPtr)));
                forkSiteTidPtrSCEV = getSCEVMinusExpr(forkSiteTidPtrSCEV, baseForkTidPtrSCEV, SE);
                PTASCEV scev(forkSiteTidPtr,forkSiteTidPtrSCEV,SE);
                fkjnToPTASCEVMap.insert(std::make_pair(callInst,scev));
            }
            else
            {
                const Value *joinSiteTidPtr = getJoinedThread(callInst);
                const SCEV *joinSiteTidPtrSCEV = SE->getSCEV(const_cast<Value*>(joinSiteTidPtr));
                const SCEV *baseJoinTidPtrSCEV = SE->getSCEV(const_cast<Value*>(getBasePtr(joinSiteTidPtr)));
                joinSiteTidPtrSCEV = getSCEVMinusExpr(joinSiteTidPtrSCEV, baseJoinTidPtrSCEV, SE);
                PTASCEV scev(joinSiteTidPtr,joinSiteTidPtrSCEV,SE);
                fkjnToPTASCEVMap.insert(std::make_pair(callInst,scev));
            }
        }

    }
}

/*!
 * Context-sensitive forward traversal from each fork site
 */
void ForkJoinAnalysis::analyzeForkJoinPair()
{
    for(TCT::const_iterator it = tct->begin(), eit = tct->end(); it!=eit; ++it)
    {
        const CxtThread& ct = it->second->getCxtThread();
        const NodeID rootTid = it->first;
        clearFlagMap();
        if(const CallInst* forkInst = ct.getThread())
        {
            CallStrCxt forkSiteCxt = tct->getCxtOfCxtThread(ct);
            const Instruction* exitInst = getExitInstOfParentRoutineFun(rootTid);

            InstVec nextInsts;
            getNextInsts(forkInst,nextInsts);
            for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
            {
                CxtStmt cs(forkSiteCxt,*nit);
                markCxtStmtFlag(cs,TDAlive);
            }

            while(!cxtStmtList.empty())
            {
                CxtStmt cts = popFromCTSWorkList();
                const Instruction* curInst = cts.getStmt();
                DBOUT(DMTA,outs() << "-----\nForkJoinAnalysis root thread: " << it->first << " ");
                DBOUT(DMTA,cts.dump());
                DBOUT(DMTA,outs() << "-----\n");

                if(isTDFork(curInst))
                {
                    handleFork(cts,rootTid);
                }
                else if(isTDJoin(curInst))
                {
                    handleJoin(cts,rootTid);
                }
                else if(SVFUtil::isa<CallInst>(curInst) && tct->isCandidateFun(SVFUtil::getCallee(curInst)))
                {
                    handleCall(cts,rootTid);
                }
                else if(SVFUtil::isa<ReturnInst>(curInst))
                {
                    handleRet(cts);
                }
                else
                {
                    handleIntra(cts);
                }

                if(curInst==exitInst)
                {
                    if(getMarkedFlag(cts)!=TDAlive)
                        addToFullJoin(tct->getParentThread(rootTid),rootTid);
                    else
                        addToPartial(tct->getParentThread(rootTid),rootTid);
                }
            }

        }
    }
}

/// Handle fork
void ForkJoinAnalysis::handleFork(const CxtStmt& cts, NodeID rootTid)
{
    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDFork(call));

    if(getTCG()->hasThreadForkEdge(call))
    {
        for (ThreadCallGraph::ForkEdgeSet::const_iterator cgIt = getTCG()->getForkEdgeBegin(call),
                ecgIt = getTCG()->getForkEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const Function* callee = (*cgIt)->getDstNode()->getFunction();
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt,call,callee);
            CxtThread ct(newCxt,call);
            if(getMarkedFlag(cts)!=TDAlive)
                addToHBPair(rootTid,tct->getTCTNode(ct)->getId());
            else
                addToHPPair(rootTid,tct->getTCTNode(ct)->getId());
        }
    }
    handleIntra(cts);
}

/// Handle join
void ForkJoinAnalysis::handleJoin(const CxtStmt& cts, NodeID rootTid)
{
    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();

    assert(isTDJoin(call));

    if(getTCG()->hasCallGraphEdge(call))
    {
        const Instruction* forkSite = tct->getTCTNode(rootTid)->getCxtThread().getThread();
        const Instruction* joinSite = cts.getStmt();

        if(isAliasedForkJoin(forkSite, joinSite))
        {
            if(const Loop* joinLoop = getJoinLoop(joinSite))
            {
                SmallBBVector exitbbs;
                joinLoop->getExitBlocks(exitbbs);
                while(!exitbbs.empty())
                {
                    BasicBlock* eb = exitbbs.pop_back_val();
                    CxtStmt newCts(curCxt,&(eb->front()));
                    addDirectlyJoinTID(cts,rootTid);
                    if(isSameSCEV(forkSite,joinSite))
                    {
                        markCxtStmtFlag(newCts,TDDead);
                        addSymmetricLoopJoin(cts,joinLoop);
                    }
                    else
                        markCxtStmtFlag(cts,TDAlive);
                }
            }
            else
            {
                markCxtStmtFlag(cts,TDDead);
                addDirectlyJoinTID(cts,rootTid);
                DBOUT(DMTA,outs() << "\n\t match join site " << *call <<  "for thread " << rootTid << "\n");
            }
        }
        /// for the join site in a loop loop which does not join the current thread
        /// we process the loop exit
        else
        {
            if(const Loop* joinLoop = getJoinLoop(joinSite))
            {
                SmallBBVector exitbbs;
                joinLoop->getExitBlocks(exitbbs);
                while(!exitbbs.empty())
                {
                    BasicBlock* eb = exitbbs.pop_back_val();
                    CxtStmt newCts(curCxt,&(eb->front()));
                    markCxtStmtFlag(newCts,cts);
                }
            }
        }
    }
    handleIntra(cts);
}

/// Handle call
void ForkJoinAnalysis::handleCall(const CxtStmt& cts, NodeID rootTid)
{

    const CallInst* call = SVFUtil::cast<CallInst>(cts.getStmt());
    const CallStrCxt& curCxt = cts.getContext();

    if(getTCG()->hasCallGraphEdge(call))
    {
        for (PTACallGraph::CallGraphEdgeSet::const_iterator cgIt = getTCG()->getCallEdgeBegin(call),
                ecgIt = getTCG()->getCallEdgeEnd(call); cgIt != ecgIt; ++cgIt)
        {
            const Function* callee = (*cgIt)->getDstNode()->getFunction();
            if (isExtCall(callee))
                continue;
            CallStrCxt newCxt = curCxt;
            pushCxt(newCxt,call,callee);
            CxtStmt newCts(newCxt,&(callee->getEntryBlock().front()));
            markCxtStmtFlag(newCts,cts);
        }
    }
}

/// Handle return
void ForkJoinAnalysis::handleRet(const CxtStmt& cts)
{

    const Instruction* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    PTACallGraphNode* curFunNode = getTCG()->getCallGraphNode(curInst->getParent()->getParent());
    for(PTACallGraphNode::const_iterator it = curFunNode->getInEdges().begin(), eit = curFunNode->getInEdges().end(); it!=eit; ++it)
    {
        PTACallGraphEdge* edge = *it;
        if(SVFUtil::isa<ThreadForkEdge>(edge) || SVFUtil::isa<ThreadJoinEdge>(edge))
            continue;
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->directCallsBegin(),
                ecit = (edge)->directCallsEnd(); cit!=ecit; ++cit)
        {
            CallStrCxt newCxt = curCxt;
            if(matchCxt(newCxt,*cit,curFunNode->getFunction()))
            {
                InstVec nextInsts;
                getNextInsts(*cit,nextInsts);
                for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
                {
                    CxtStmt newCts(newCxt,*nit);
                    markCxtStmtFlag(newCts,cts);
                }
            }
        }
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (edge)->indirectCallsBegin(),
                ecit = (edge)->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            CallStrCxt newCxt = curCxt;
            if(matchCxt(newCxt,*cit,curFunNode->getFunction()))
            {
                InstVec nextInsts;
                getNextInsts(*cit,nextInsts);
                for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
                {
                    CxtStmt newCts(newCxt,*nit);
                    markCxtStmtFlag(newCts,cts);
                }
            }
        }
    }
}

/// Handle intra
void ForkJoinAnalysis::handleIntra(const CxtStmt& cts)
{

    const Instruction* curInst = cts.getStmt();
    const CallStrCxt& curCxt = cts.getContext();

    InstVec nextInsts;
    getNextInsts(curInst,nextInsts);
    for(InstVec::const_iterator nit = nextInsts.begin(), enit = nextInsts.end(); nit!=enit; ++nit)
    {
        CxtStmt newCts(curCxt,*nit);
        markCxtStmtFlag(newCts,cts);
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
    if(it!=dirAndIndJoinMap.end())
        return it->second;

    const NodeBS& directJoinTids =  getDirectlyJoinedTid(cs);
    NodeBS allJoinTids = directJoinTids;

    FIFOWorkList<NodeID> worklist;
    for(NodeBS::iterator it = directJoinTids.begin(), eit = directJoinTids.end(); it!=eit; ++it)
    {
        worklist.push(*it);
    }

    while(!worklist.empty())
    {
        NodeID tid = worklist.pop();
        TCTNode* node = tct->getTCTNode(tid);
        for(TCT::ThreadCreateEdgeSet::const_iterator it = tct->getChildrenBegin(node), eit = tct->getChildrenEnd(node); it!=eit; ++it)
        {
            NodeID childTid = (*it)->getDstID();
            if(isFullJoin(tid,childTid))
            {
                allJoinTids.set(childTid);
                worklist.push(childTid);
            }
        }
    }

    dirAndIndJoinMap[cs] = allJoinTids;

    return allJoinTids;
}

static bool accessSameArrayIndex(const GetElementPtrInst* ptr1, const GetElementPtrInst* ptr2)
{

    std::vector<u32_t> ptr1vec;
    for (gep_type_iterator gi = gep_type_begin(*ptr1), ge = gep_type_end(*ptr1);
            gi != ge; ++gi)
    {
        if(ConstantInt* ci = SVFUtil::dyn_cast<ConstantInt>(gi.getOperand()))
        {
            Size_t idx = ci->getSExtValue();
            ptr1vec.push_back(idx);
        }
        else
            return false;
    }

    std::vector<u32_t> ptr2vec;
    for (gep_type_iterator gi = gep_type_begin(*ptr2), ge = gep_type_end(*ptr2);
            gi != ge; ++gi)
    {
        if(ConstantInt* ci = SVFUtil::dyn_cast<ConstantInt>(gi.getOperand()))
        {
            Size_t idx = ci->getSExtValue();
            ptr2vec.push_back(idx);
        }
        else
            return false;
    }

    return ptr1vec==ptr2vec;
}

/*!
 * We assume a pair of fork and join sites are must-alias if they have same PTASCEV
 * (1) SCEV not inside loop
 * (2) SCEV inside two symmetric loops, then
 *  pointers of fork thread and join thread should have same scev start and step.
 *  and should have same loop trip count
 */
bool ForkJoinAnalysis::isSameSCEV(const Instruction* forkSite, const Instruction* joinSite)
{

    const PTASCEV& forkse = fkjnToPTASCEVMap[forkSite];
    const PTASCEV& joinse = fkjnToPTASCEVMap[joinSite];

    //if(sameLoopTripCount(forkSite,joinSite) == false)
    //  return false;

    if(forkse.inloop && joinse.inloop)
        return forkse.start==joinse.start && forkse.step == joinse.step && forkse.tripcount <= joinse.tripcount;
    else if(SVFUtil::isa<GetElementPtrInst>(forkse.ptr) && SVFUtil::isa<GetElementPtrInst>(joinse.ptr))
        return accessSameArrayIndex(SVFUtil::cast<GetElementPtrInst>(forkse.ptr),SVFUtil::cast<GetElementPtrInst>(joinse.ptr));
    else if(SVFUtil::isa<GetElementPtrInst>(forkse.ptr) || SVFUtil::isa<GetElementPtrInst>(joinse.ptr))
        return false;
    else
        return true;
}

/*!
 * The fork and join have same loop trip count
 */
bool ForkJoinAnalysis::sameLoopTripCount(const Instruction* forkSite, const Instruction* joinSite)
{

    ScalarEvolution* forkSE = getSE(forkSite);
    ScalarEvolution* joinSE = getSE(joinSite);

    // Get loops
    const Loop *forkSiteLoop = tct->getLoop(forkSite);
    const Loop *joinSiteLoop = tct->getLoop(joinSite);

    if(forkSiteLoop == nullptr || joinSiteLoop == nullptr)
        return false;

    const SCEV* forkLoopCountScev = forkSE->getMaxBackedgeTakenCount(forkSiteLoop);
    const SCEV* joinLoopCountScev = joinSE->getMaxBackedgeTakenCount(joinSiteLoop);

    if(forkLoopCountScev!=forkSE->getCouldNotCompute())
    {
        if(forkLoopCountScev==joinLoopCountScev)
        {
            return true;
        }
    }
    return false;
}
