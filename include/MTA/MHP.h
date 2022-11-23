//===- MHP.h -- May-happen-in-parallel analysis-------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * MHP.h
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui, Peng Di
 */

#ifndef MHP_H_
#define MHP_H_

#include "MTA/TCT.h"
#include "Util/SVFUtil.h"
namespace SVF
{

class ForkJoinAnalysis;
class LockAnalysis;

/*!
 * This class serves as a base may-happen in parallel analysis for multithreaded program
 * Given a statement under an abstract thread, it tells which abstract threads may be alive at the same time (May-happen-in-parallel).
 */
class MHP
{

public:
    typedef Set<const SVFFunction*> FunSet;
    typedef Set<const SVFInstruction*> InstSet;
    typedef TCT::InstVec InstVec;
    typedef FIFOWorkList<CxtThreadStmt> CxtThreadStmtWorkList;
    typedef Set<CxtThreadStmt> CxtThreadStmtSet;
    typedef Map<CxtThreadStmt,NodeBS> ThreadStmtToThreadInterleav;
    typedef Map<const SVFInstruction*,CxtThreadStmtSet> InstToThreadStmtSetMap;
    typedef SVFLoopAndDomInfo::LoopBBs LoopBBs;

    typedef Set<CxtStmt> LockSpan;

    typedef std::pair<const SVFFunction*,const SVFFunction*> FuncPair;
    typedef Map<FuncPair, bool> FuncPairToBool;

    /// Constructor
    MHP(TCT* t);

    /// Destructor
    virtual ~MHP();

    /// Start analysis here
    void analyze();

    /// Analyze thread interleaving
    void analyzeInterleaving();

    /// Get ThreadCallGraph
    inline ThreadCallGraph* getThreadCallGraph() const
    {
        return tcg;
    }

    /// Get Thread Creation Tree
    inline TCT* getTCT() const
    {
        return tct;
    }

    // Get CallICFGNode
    inline CallICFGNode* getCBN(const SVFInstruction* inst)
    {
        return tct->getCallICFGNode(inst);
    }

    /// Whether the function is connected from main function in thread call graph
    bool isConnectedfromMain(const SVFFunction* fun);

//    /// Interface to query whether two instructions are protected by common locks
//    virtual bool isProtectedByACommonLock(const SVFInstruction* i1, const SVFInstruction* i2);
//    virtual bool isAllCxtInSameLockSpan(const SVFInstruction *I1, const SVFInstruction *I2);
//    virtual bool isOneCxtInSameLockSpan(const SVFInstruction *I1, const SVFInstruction *I2);
//
//    bool hasOneCxtInLockSpan(const SVFInstruction *I, LockSpan lspan);
//    bool hasAllCxtInLockSpan(const SVFInstruction *I, LockSpan lspan);
//
//
//    LockSpan getSpanfromCxtLock(NodeID l);
    /// Interface to query whether two instructions may happen-in-parallel
    virtual bool mayHappenInParallel(const SVFInstruction* i1, const SVFInstruction* i2);
    virtual bool mayHappenInParallelCache(const SVFInstruction* i1, const SVFInstruction* i2);
    virtual bool mayHappenInParallelInst(const SVFInstruction* i1, const SVFInstruction* i2);
    virtual bool executedByTheSameThread(const SVFInstruction* i1, const SVFInstruction* i2);

    /// Get interleaving thread for statement inst
    //@{
    inline const NodeBS& getInterleavingThreads(const CxtThreadStmt& cts)
    {
        return threadStmtToTheadInterLeav[cts];
    }
    inline bool hasInterleavingThreads(const CxtThreadStmt& cts) const
    {
        return threadStmtToTheadInterLeav.find(cts)!=threadStmtToTheadInterLeav.end();
    }
    //@}

    /// Get/has ThreadStmt
    //@{
    inline const CxtThreadStmtSet& getThreadStmtSet(const SVFInstruction* inst) const
    {
        InstToThreadStmtSetMap::const_iterator it = instToTSMap.find(inst);
        assert(it!=instToTSMap.end() && "no thread access the instruction?");
        return it->second;
    }
    inline bool hasThreadStmtSet(const SVFInstruction* inst) const
    {
        return instToTSMap.find(inst)!=instToTSMap.end();
    }
    //@}

    /// Print interleaving results
    void printInterleaving();

private:

    inline const PTACallGraph::FunctionSet& getCallee(const SVFInstruction* inst, PTACallGraph::FunctionSet& callees)
    {
        tcg->getCallees(getCBN(inst), callees);
        return callees;
    }
    /// Update non-candidate functions' interleaving.
    /// Copy interleaving threads of the entry inst to other insts.
    void updateNonCandidateFunInterleaving();

    /// Handle non-candidate function
    void handleNonCandidateFun(const CxtThreadStmt& cts);

    /// Handle fork
    void handleFork(const CxtThreadStmt& cts, NodeID rootTid);

    /// Handle join
    void handleJoin(const CxtThreadStmt& cts, NodeID rootTid);

    /// Handle call
    void handleCall(const CxtThreadStmt& cts, NodeID rootTid);

    /// Handle return
    void handleRet(const CxtThreadStmt& cts);

    /// Handle intra
    void handleIntra(const CxtThreadStmt& cts);

    /// Add/Remove interleaving thread for statement inst
    //@{
    inline void addInterleavingThread(const CxtThreadStmt& tgr, NodeID tid)
    {
        if(threadStmtToTheadInterLeav[tgr].test_and_set(tid))
        {
            instToTSMap[tgr.getStmt()].insert(tgr);
            pushToCTSWorkList(tgr);
        }
    }
    inline void addInterleavingThread(const CxtThreadStmt& tgr, const CxtThreadStmt& src)
    {
        bool changed = threadStmtToTheadInterLeav[tgr] |= threadStmtToTheadInterLeav[src];
        if(changed)
        {
            instToTSMap[tgr.getStmt()].insert(tgr);
            pushToCTSWorkList(tgr);
        }
    }
    inline void rmInterleavingThread(const CxtThreadStmt& tgr, const NodeBS& tids, const SVFInstruction* joinsite)
    {
        NodeBS joinedTids;
        for(NodeBS::iterator it = tids.begin(), eit = tids.end(); it!=eit; ++it)
        {
            if(isMustJoin(tgr.getTid(),joinsite))
                joinedTids.set(*it);
        }
        if(threadStmtToTheadInterLeav[tgr].intersectWithComplement(joinedTids))
        {
            pushToCTSWorkList(tgr);
        }
    }
    //@}

    /// Update Ancestor and sibling threads
    //@{
    void updateAncestorThreads(NodeID tid);
    void updateSiblingThreads(NodeID tid);
    //@}

    /// Thread curTid can be fully joined by parentTid recurively
    bool isRecurFullJoin(NodeID parentTid, NodeID curTid);

    /// Whether a join site must join a thread t
    bool isMustJoin(const NodeID curTid, const SVFInstruction* joinsite);

    /// A thread is a multiForked thread if it is in a loop or recursion
    inline bool isMultiForkedThread(NodeID curTid)
    {
        return tct->getTCTNode(curTid)->isMultiforked();
    }
    /// Push calling context
    inline void pushCxt(CallStrCxt& cxt, const SVFInstruction* call, const SVFFunction* callee)
    {
        tct->pushCxt(cxt,call,callee);
    }
    /// Match context
    inline bool matchCxt(CallStrCxt& cxt, const SVFInstruction* call, const SVFFunction* callee)
    {
        return tct->matchCxt(cxt,call,callee);
    }

    /// WorkList helper functions
    //@{
    inline bool pushToCTSWorkList(const CxtThreadStmt& cs)
    {
        return cxtStmtList.push(cs);
    }
    inline CxtThreadStmt popFromCTSWorkList()
    {
        CxtThreadStmt ctp = cxtStmtList.pop();
        return ctp;
    }

    /// Whether it is a fork site
    inline bool isTDFork(const SVFInstruction* call)
    {
        return tcg->getThreadAPI()->isTDFork(call);
    }
    /// Whether it is a join site
    inline bool isTDJoin(const SVFInstruction* call)
    {
        return tcg->getThreadAPI()->isTDJoin(call);
    }

    /// Return thread id(s) which are directly or indirectly joined at this join site
    NodeBS getDirAndIndJoinedTid(const CallStrCxt& cxt, const SVFInstruction* call);

    /// Whether a context-sensitive join satisfies symmetric loop pattern
    bool hasJoinInSymmetricLoop(const CallStrCxt& cxt, const SVFInstruction* call) const;

    /// Whether a context-sensitive join satisfies symmetric loop pattern
    const LoopBBs& getJoinInSymmetricLoop(const CallStrCxt& cxt, const SVFInstruction* call) const;

    /// Whether thread t1 happens before t2 based on ForkJoin Analysis
    bool isHBPair(NodeID tid1, NodeID tid2);

    ThreadCallGraph* tcg;				///< TCG
    TCT* tct;							///< TCT
    ForkJoinAnalysis* fja;				///< ForJoin Analysis
    CxtThreadStmtWorkList cxtStmtList;	///< CxtThreadStmt worklist
    ThreadStmtToThreadInterleav threadStmtToTheadInterLeav; /// Map a statement to its thread interleavings
    InstToThreadStmtSetMap instToTSMap; ///< Map an instruction to its ThreadStmtSet
    FuncPairToBool nonCandidateFuncMHPRelMap;


public:
    u32_t numOfTotalQueries;		///< Total number of queries
    u32_t numOfMHPQueries;			///< Number of queries are answered as may-happen-in-parallel
    double interleavingTime;
    double interleavingQueriesTime;
};



/*!
 *
 */
class ForkJoinAnalysis
{

public:
    /// semilattice  Empty==>TDDead==>TDAlive
    enum ValDomain
    {
        Empty,  // initial(dummy) state
        TDAlive,  // thread is alive
        TDDead,  //  thread is dead
    };

    typedef SVFLoopAndDomInfo::LoopBBs LoopBBs;
    typedef TCT::InstVec InstVec;
    typedef Map<CxtStmt,ValDomain> CxtStmtToAliveFlagMap;
    typedef Map<CxtStmt,NodeBS> CxtStmtToTIDMap;
    typedef Set<NodePair> ThreadPairSet;
    typedef Map<CxtStmt, LoopBBs> CxtStmtToLoopMap;
    typedef FIFOWorkList<CxtStmt> CxtStmtWorkList;

    ForkJoinAnalysis(TCT* t) : tct(t)
    {
        collectSCEVInfo();
    }
    /// functions
    void collectSCEVInfo();

    /// context-sensitive forward traversal from each fork site. Generate following results
    /// (1) fork join pair, maps a context-sensitive join site to its corresponding thread ids
    /// (2) never happen-in-parallel thread pairs
    void analyzeForkJoinPair();

    /// Get directly joined threadIDs based on a context-sensitive join site
    inline NodeBS& getDirectlyJoinedTid(const CxtStmt& cs)
    {
        return directJoinMap[cs];
    }
    /// Get directly and indirectly joined threadIDs based on a context-sensitive join site
    NodeBS getDirAndIndJoinedTid(const CxtStmt& cs);

    /// Whether a context-sensitive join satisfies symmetric loop pattern
    inline const LoopBBs& getJoinInSymmetricLoop(const CxtStmt& cs) const
    {
        CxtStmtToLoopMap::const_iterator it = cxtJoinInLoop.find(cs);
        assert(it!=cxtJoinInLoop.end() && "does not have the loop");
        return it->second;
    }
    inline bool hasJoinInSymmetricLoop(const CxtStmt& cs) const
    {
        CxtStmtToLoopMap::const_iterator it = cxtJoinInLoop.find(cs);
        return it!=cxtJoinInLoop.end();
    }
    /// Whether thread t1 happens-before thread t2
    inline bool isHBPair(NodeID tid1, NodeID tid2)
    {
        bool nonhp = HBPair.find(std::make_pair(tid1,tid2))!=HBPair.end();
        bool hp = HPPair.find(std::make_pair(tid1,tid2))!=HPPair.end();
        if(nonhp && !hp)
            return true;
        return false;
    }
    /// Whether t1 fully joins t2
    inline bool isFullJoin(NodeID tid1, NodeID tid2)
    {
        bool full = fullJoin.find(std::make_pair(tid1,tid2))!=fullJoin.end();
        bool partial = partialJoin.find(std::make_pair(tid1,tid2))!=partialJoin.end();
        if(full && !partial)
            return true;
        return false;
    }

    /// Get exit instruction of the start routine function of tid's parent thread
    inline const SVFInstruction* getExitInstOfParentRoutineFun(NodeID tid) const
    {
        NodeID parentTid = tct->getParentThread(tid);
        const CxtThread& parentct = tct->getTCTNode(parentTid)->getCxtThread();
        const SVFFunction* parentRoutine = tct->getStartRoutineOfCxtThread(parentct);
        const SVFInstruction* inst = parentRoutine->getExitBB()->back();
        return inst;
    }

    /// Get loop for join site
    inline LoopBBs& getJoinLoop(const SVFInstruction* inst)
    {
        return tct->getJoinLoop(inst);
    }
    inline bool hasJoinLoop(const SVFInstruction* inst)
    {
        return tct->hasJoinLoop(inst);
    }
private:

    /// Handle fork
    void handleFork(const CxtStmt& cts,NodeID rootTid);

    /// Handle join
    void handleJoin(const CxtStmt& cts,NodeID rootTid);

    /// Handle call
    void handleCall(const CxtStmt& cts,NodeID rootTid);

    /// Handle return
    void handleRet(const CxtStmt& cts);

    /// Handle intra
    void handleIntra(const CxtStmt& cts);

    /// Return true if the fork and join have the same SCEV
    bool isSameSCEV(const SVFInstruction* forkSite, const SVFInstruction* joinSite);

    /// Same loop trip count
    bool sameLoopTripCount(const SVFInstruction* forkSite, const SVFInstruction* joinSite);

    /// Whether it is a matched fork join pair
    bool isAliasedForkJoin(const SVFInstruction* forkSite, const SVFInstruction* joinSite)
    {
        return tct->getPTA()->alias(getForkedThread(forkSite), getJoinedThread(joinSite)) && isSameSCEV(forkSite,joinSite);
    }
    // Get CallICFGNode
    inline CallICFGNode* getCBN(const SVFInstruction* inst)
    {
        return tct->getCallICFGNode(inst);
    }
    /// Mark thread flags for cxtStmt
    //@{
    /// Get the flag for a cxtStmt
    inline ValDomain getMarkedFlag(const CxtStmt& cs)
    {
        CxtStmtToAliveFlagMap::const_iterator it = cxtStmtToAliveFlagMap.find(cs);
        if(it==cxtStmtToAliveFlagMap.end())
        {
            cxtStmtToAliveFlagMap[cs] = Empty;
            return Empty;
        }
        else
            return it->second;
    }
    /// Initialize TDAlive and TDDead flags
    void markCxtStmtFlag(const CxtStmt& tgr, ValDomain flag)
    {
        ValDomain flag_tgr = getMarkedFlag(tgr);
        cxtStmtToAliveFlagMap[tgr] = flag;
        if(flag_tgr!=getMarkedFlag(tgr))
            pushToCTSWorkList(tgr);
    }
    /// Transfer function for marking context-sensitive statement
    void markCxtStmtFlag(const CxtStmt& tgr, const CxtStmt& src)
    {
        ValDomain flag_tgr = getMarkedFlag(tgr);
        ValDomain flag_src = getMarkedFlag(src);
        if(flag_tgr == Empty)
        {
            cxtStmtToAliveFlagMap[tgr] = flag_src;
        }
        else if(flag_tgr == TDDead)
        {
            if(flag_src==TDAlive)
                cxtStmtToAliveFlagMap[tgr] = TDAlive;
        }
        else
        {
            /// alive is at the bottom of the semilattice, nothing needs to be done here
        }
        if(flag_tgr!=getMarkedFlag(tgr))
        {
            pushToCTSWorkList(tgr);
        }
    }
    /// Clear flags
    inline void clearFlagMap()
    {
        cxtStmtToAliveFlagMap.clear();
        cxtStmtList.clear();
    }
    //@}

    /// Worklist operations
    //@{
    inline bool pushToCTSWorkList(const CxtStmt& cs)
    {
        return cxtStmtList.push(cs);
    }
    inline CxtStmt popFromCTSWorkList()
    {
        CxtStmt ctp = cxtStmtList.pop();
        return ctp;
    }
    //@}

    /// Push calling context
    inline void pushCxt(CallStrCxt& cxt, const SVFInstruction* call, const SVFFunction* callee)
    {
        tct->pushCxt(cxt,call,callee);
    }
    /// Match context
    inline bool matchCxt(CallStrCxt& cxt, const SVFInstruction* call, const SVFFunction* callee)
    {
        return tct->matchCxt(cxt,call,callee);
    }

    /// Whether it is a fork site
    inline bool isTDFork(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->isTDFork(call);
    }
    /// Whether it is a join site
    inline bool isTDJoin(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->isTDJoin(call);
    }
    /// Get forked thread
    inline const SVFValue* getForkedThread(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->getForkedThread(call);
    }
    /// Get joined thread
    inline const SVFValue* getJoinedThread(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->getJoinedThread(call);
    }
    inline const PTACallGraph::FunctionSet& getCallee(const SVFInstruction* inst, PTACallGraph::FunctionSet& callees)
    {
        getTCG()->getCallees(getCBN(inst), callees);
        return callees;
    }
    /// ThreadCallGraph
    inline ThreadCallGraph* getTCG() const
    {
        return tct->getThreadCallGraph();
    }
    ///maps a context-sensitive join site to a thread id
    inline void addDirectlyJoinTID(const CxtStmt& cs, NodeID tid)
    {
        directJoinMap[cs].set(tid);
    }

    /// happen-in-parallel pair
    /// happens-before pair
    //@{
    inline void addToHPPair(NodeID tid1, NodeID tid2)
    {
        HPPair.insert(std::make_pair(tid1,tid2));
        HPPair.insert(std::make_pair(tid2,tid1));
    }
    inline void addToHBPair(NodeID tid1, NodeID tid2)
    {
        HBPair.insert(std::make_pair(tid1,tid2));
    }
    //@}

    /// full join and partial join
    //@{
    inline void addToFullJoin(NodeID tid1, NodeID tid2)
    {
        fullJoin.insert(std::make_pair(tid1,tid2));
    }
    inline void addToPartial(NodeID tid1, NodeID tid2)
    {
        partialJoin.insert(std::make_pair(tid1,tid2));
    }
    //@}

    /// Add inloop join
    inline void addSymmetricLoopJoin(const CxtStmt& cs, LoopBBs& lp)
    {
        cxtJoinInLoop[cs] = lp;
    }
    TCT* tct;
    CxtStmtToAliveFlagMap cxtStmtToAliveFlagMap;	///< flags for context-sensitive statements
    CxtStmtWorkList cxtStmtList;	 ///< context-sensitive statement worklist
    CxtStmtToTIDMap directJoinMap; ///< maps a context-sensitive join site to directly joined thread ids
    CxtStmtToTIDMap dirAndIndJoinMap; ///< maps a context-sensitive join site to directly and indirectly joined thread ids
    CxtStmtToLoopMap cxtJoinInLoop;		///< a set of context-sensitive join inside loop
    ThreadPairSet HBPair;		///< thread happens-before pair
    ThreadPairSet HPPair;		///< threads happen-in-parallel
    ThreadPairSet fullJoin;		///< t1 fully joins t2 along all program path
    ThreadPairSet partialJoin;		///< t1 partially joins t2 along some program path(s)
};

} // End namespace SVF

#endif /* MHP_H_ */
