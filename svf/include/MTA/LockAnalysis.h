//===- LockAnalysis.h -- Analysis of locksets-------------//
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
 * LockAnalysis.h
 *
 *  Created on: 26 Aug 2015
 *      Author: pengd
 */

#ifndef INCLUDE_MTA_LockAnalysis_H_
#define INCLUDE_MTA_LockAnalysis_H_

/*!
 *
 */
#include "MTA/TCT.h"

namespace SVF
{

/*!
 * Lock analysis
 */
class LockAnalysis
{

public:
    /// semilattice  Empty==>TDUnlocked==>TDLocked
    enum ValDomain
    {
        Empty,  // initial(dummy) state
        TDLocked,  // stmt is locked
        TDUnlocked,  //  stmt is unlocked
    };

    typedef CxtStmt CxtLock;
    typedef CxtProc CxtLockProc;

    typedef NodeBS LockSet;
    typedef TCT::InstVec InstVec;
    typedef Set<const SVFInstruction*> InstSet;
    typedef InstSet CISpan;
    typedef Map<const SVFInstruction*, CISpan>CILockToSpan;
    typedef Set<const SVFFunction*> FunSet;
    typedef Map<const SVFInstruction*, InstSet> InstToInstSetMap;
    typedef Map<CxtStmt, ValDomain> CxtStmtToLockFlagMap;
    typedef FIFOWorkList<CxtStmt> CxtStmtWorkList;
    typedef Set<CxtStmt> LockSpan;
    typedef Set<CxtStmt> CxtStmtSet;
    typedef Set<CxtLock> CxtLockSet;

    typedef Map<CxtLock, LockSpan> CxtLockToSpan;
    typedef Map<CxtLock, NodeBS> CxtLockToLockSet;
    typedef Map<const SVFInstruction*, NodeBS> LockSiteToLockSet;
    typedef Map<const SVFInstruction*, LockSpan> InstToCxtStmtSet;
    typedef Map<CxtStmt, CxtLockSet> CxtStmtToCxtLockSet;
    typedef FIFOWorkList<CxtLockProc> CxtLockProcVec;
    typedef Set<CxtLockProc> CxtLockProcSet;

    typedef Map<const SVFInstruction*, CxtStmtSet> InstToCxtStmt;

    LockAnalysis(TCT* t) : tct(t), lockTime(0),numOfTotalQueries(0), numOfLockedQueries(0), lockQueriesTime(0)
    {
    }

    /// context-sensitive forward traversal from each lock site. Generate following results
    /// (1) context-sensitive lock site,
    /// (2) maps a context-sensitive lock site to its corresponding lock span.
    void analyze();
    void analyzeIntraProcedualLock();
    bool intraForwardTraverse(const SVFInstruction* lock, InstSet& unlockset, InstSet& forwardInsts);
    bool intraBackwardTraverse(const InstSet& unlockset, InstSet& backwardInsts);

    void collectCxtLock();
    void analyzeLockSpanCxtStmt();

    void collectLockUnlocksites();
    void buildCandidateFuncSetforLock();

    /// Intraprocedural locks
    //@{
    /// Return true if the lock is an intra-procedural lock
    inline bool isIntraLock(const SVFInstruction* lock) const
    {
        assert(locksites.find(lock)!=locksites.end() && "not a lock site?");
        return ciLocktoSpan.find(lock)!=ciLocktoSpan.end();
    }

    /// Add intra-procedural lock
    inline void addIntraLock(const SVFInstruction* lockSite, const InstSet& stmts)
    {
        for(InstSet::const_iterator it = stmts.begin(), eit = stmts.end(); it!=eit; ++it)
        {
            instCILocksMap[*it].insert(lockSite);
            ciLocktoSpan[lockSite].insert(*it);
        }
    }

    /// Add intra-procedural lock
    inline void addCondIntraLock(const SVFInstruction* lockSite, const InstSet& stmts)
    {
        for(InstSet::const_iterator it = stmts.begin(), eit = stmts.end(); it!=eit; ++it)
        {
            instTocondCILocksMap[*it].insert(lockSite);
        }
    }

    /// Return true if a statement is inside an intra-procedural lock
    inline bool isInsideIntraLock(const SVFInstruction* stmt) const
    {
        return instCILocksMap.find(stmt)!=instCILocksMap.end() || isInsideCondIntraLock(stmt);
    }

    /// Return true if a statement is inside a partial lock/unlock pair (conditional lock with unconditional unlock)
    inline bool isInsideCondIntraLock(const SVFInstruction* stmt) const
    {
        return instTocondCILocksMap.find(stmt)!=instTocondCILocksMap.end();
    }

    inline const InstSet& getIntraLockSet(const SVFInstruction* stmt) const
    {
        InstToInstSetMap::const_iterator it = instCILocksMap.find(stmt);
        assert(it!=instCILocksMap.end() && "intralock not found!");
        return it->second;
    }
    //@}

    /// Context-sensitive locks
    //@{
    /// Add inter-procedural context-sensitive lock
    inline void addCxtLock(const CallStrCxt& cxt,const SVFInstruction* inst)
    {
        CxtLock cxtlock(cxt,inst);
        cxtLockset.insert(cxtlock);
        DBOUT(DMTA, SVFUtil::outs() << "LockAnalysis Process new lock "; cxtlock.dump());
    }

    /// Get context-sensitive lock
    inline bool hasCxtLock(const CxtLock& cxtLock) const
    {
        return cxtLockset.find(cxtLock)!=cxtLockset.end();
    }

    /// Return true if the intersection of two locksets is not empty
    inline bool intersects(const CxtLockSet& lockset1,const CxtLockSet& lockset2) const
    {
        for(CxtLockSet::const_iterator it = lockset1.begin(), eit = lockset1.end(); it!=eit; ++it)
        {
            const CxtLock& lock = *it;
            for(CxtLockSet::const_iterator lit = lockset2.begin(), elit = lockset2.end(); lit!=elit; ++lit)
            {
                if(lock==*lit)
                    return true;
            }
        }
        return false;
    }
    /// Return true if two locksets has at least one alias lock
    inline bool alias(const CxtLockSet& lockset1,const CxtLockSet& lockset2)
    {
        for(CxtLockSet::const_iterator it = lockset1.begin(), eit = lockset1.end(); it!=eit; ++it)
        {
            const CxtLock& lock = *it;
            for(CxtLockSet::const_iterator lit = lockset2.begin(), elit = lockset2.end(); lit!=elit; ++lit)
            {
                if(isAliasedLocks(lock,*lit))
                    return true;
            }
        }
        return false;
    }
    //@}

    /// Return true if it is a candidate function
    inline bool isLockCandidateFun(const SVFFunction* fun) const
    {
        return lockcandidateFuncSet.find(fun)!=lockcandidateFuncSet.end();
    }

    /// Context-sensitive statement and lock spans
    //@{
    /// Get LockSet and LockSpan
    inline bool hasCxtStmtfromInst(const SVFInstruction* inst) const
    {
        InstToCxtStmtSet::const_iterator it = instToCxtStmtSet.find(inst);
        return (it != instToCxtStmtSet.end());
    }
    inline const CxtStmtSet& getCxtStmtfromInst(const SVFInstruction* inst) const
    {
        InstToCxtStmtSet::const_iterator it = instToCxtStmtSet.find(inst);
        assert(it != instToCxtStmtSet.end());
        return it->second;
    }
    inline bool hasCxtLockfromCxtStmt(const CxtStmt& cts) const
    {
        CxtStmtToCxtLockSet::const_iterator it = cxtStmtToCxtLockSet.find(cts);
        return (it != cxtStmtToCxtLockSet.end());
    }
    inline const CxtLockSet& getCxtLockfromCxtStmt(const CxtStmt& cts) const
    {
        CxtStmtToCxtLockSet::const_iterator it = cxtStmtToCxtLockSet.find(cts);
        assert(it != cxtStmtToCxtLockSet.end());
        return it->second;
    }
    inline CxtLockSet& getCxtLockfromCxtStmt(const CxtStmt& cts)
    {
        CxtStmtToCxtLockSet::iterator it = cxtStmtToCxtLockSet.find(cts);
        assert(it != cxtStmtToCxtLockSet.end());
        return it->second;
    }
    /// Add context-sensitive statement
    inline bool addCxtStmtToSpan(const CxtStmt& cts, const CxtLock& cl)
    {
        cxtLocktoSpan[cl].insert(cts);
        return cxtStmtToCxtLockSet[cts].insert(cl).second;
    }
    /// Add context-sensitive statement
    inline bool removeCxtStmtToSpan(CxtStmt& cts, const CxtLock& cl)
    {
        bool find = cxtStmtToCxtLockSet[cts].find(cl)!=cxtStmtToCxtLockSet[cts].end();
        if(find)
        {
            cxtStmtToCxtLockSet[cts].erase(cl);
            cxtLocktoSpan[cl].erase(cts);
        }
        return find;
    }

    CxtStmtToCxtLockSet getCSTCLS()
    {
        return cxtStmtToCxtLockSet;
    }
    /// Touch this context statement
    inline void touchCxtStmt(CxtStmt& cts)
    {
        cxtStmtToCxtLockSet[cts];
    }
    inline bool hasSpanfromCxtLock(const CxtLock& cl)
    {
        return cxtLocktoSpan.find(cl) != cxtLocktoSpan.end();
    }
    inline LockSpan& getSpanfromCxtLock(const CxtLock& cl)
    {
        assert(cxtLocktoSpan.find(cl) != cxtLocktoSpan.end());
        return cxtLocktoSpan[cl];
    }
    //@}



    /// Check if one instruction's context stmt is in a lock span
    inline bool hasOneCxtInLockSpan(const SVFInstruction *I, LockSpan lspan) const
    {
        if(!hasCxtStmtfromInst(I))
            return false;
        const LockSpan ctsset = getCxtStmtfromInst(I);
        for (LockSpan::const_iterator cts = ctsset.begin(), ects = ctsset.end(); cts != ects; cts++)
        {
            if(lspan.find(*cts) != lspan.end())
            {
                return true;
            }
        }
        return false;
    }

    inline bool hasAllCxtInLockSpan(const SVFInstruction *I, LockSpan lspan) const
    {
        if(!hasCxtStmtfromInst(I))
            return false;
        const LockSpan ctsset = getCxtStmtfromInst(I);
        for (LockSpan::const_iterator cts = ctsset.begin(), ects = ctsset.end(); cts != ects; cts++)
        {
            if (lspan.find(*cts) == lspan.end())
            {
                return false;
            }
        }
        return true;
    }


    /// Check if two Instructions are protected by common locks
    /// echo inst may have multiple cxt stmt
    /// we check whether every cxt stmt of instructions is protected by a common lock.
    bool isProtectedByCommonLock(const SVFInstruction *i1, const SVFInstruction *i2);
    bool isProtectedByCommonCxtLock(const SVFInstruction *i1, const SVFInstruction *i2);
    bool isProtectedByCommonCxtLock(const CxtStmt& cxtStmt1, const CxtStmt& cxtStmt2);
    bool isProtectedByCommonCILock(const SVFInstruction *i1, const SVFInstruction *i2);

    bool isInSameSpan(const SVFInstruction *I1, const SVFInstruction *I2);
    bool isInSameCSSpan(const SVFInstruction *i1, const SVFInstruction *i2) const;
    bool isInSameCSSpan(const CxtStmt& cxtStmt1, const CxtStmt& cxtStmt2) const;
    bool isInSameCISpan(const SVFInstruction *i1, const SVFInstruction *i2) const;

    inline u32_t getNumOfCxtLocks()
    {
        return cxtLockset.size();
    }
    /// Print locks and spans
    void printLocks(const CxtStmt& cts);

    /// Get tct
    TCT* getTCT()
    {
        return tct;
    }
private:
    /// Handle fork
    void handleFork(const CxtStmt& cts);

    /// Handle call
    void handleCall(const CxtStmt& cts);

    /// Handle return
    void handleRet(const CxtStmt& cts);

    /// Handle intra
    void handleIntra(const CxtStmt& cts);

    /// Handle call relations
    void handleCallRelation(CxtLockProc& clp, const PTACallGraphEdge* cgEdge, CallSite call);

    /// Return true it a lock matches an unlock
    bool isAliasedLocks(const CxtLock& cl1, const CxtLock& cl2)
    {
        return isAliasedLocks(cl1.getStmt(), cl2.getStmt());
    }
    bool isAliasedLocks(const SVFInstruction* i1, const SVFInstruction* i2)
    {
        /// todo: must alias
        return tct->getPTA()->alias(getLockVal(i1), getLockVal(i2));
    }

    /// Mark thread flags for cxtStmt
    //@{
    /// Transfer function for marking context-sensitive statement
    void markCxtStmtFlag(const CxtStmt& tgr, const CxtStmt& src)
    {
        const CxtLockSet& srclockset = getCxtLockfromCxtStmt(src);
        if(hasCxtLockfromCxtStmt(tgr)== false)
        {
            for(CxtLockSet::const_iterator it = srclockset.begin(), eit = srclockset.end(); it!=eit; ++it)
            {
                addCxtStmtToSpan(tgr,*it);
            }
            pushToCTSWorkList(tgr);
        }
        else
        {
            if(intersect(getCxtLockfromCxtStmt(tgr),srclockset))
                pushToCTSWorkList(tgr);
        }
    }
    bool intersect(CxtLockSet& tgrlockset, const CxtLockSet& srclockset)
    {
        CxtLockSet toBeDeleted;
        for(CxtLockSet::const_iterator it = tgrlockset.begin(), eit = tgrlockset.end(); it!=eit; ++it)
        {
            if(srclockset.find(*it)==srclockset.end())
                toBeDeleted.insert(*it);
        }
        for(CxtLockSet::const_iterator it = toBeDeleted.begin(), eit = toBeDeleted.end(); it!=eit; ++it)
        {
            tgrlockset.erase(*it);
        }
        return !toBeDeleted.empty();
    }

    /// Clear flags
    inline void clearFlagMap()
    {
        cxtStmtList.clear();
    }
    //@}

    /// WorkList helper functions
    //@{
    inline bool pushToCTPWorkList(const CxtLockProc& clp)
    {
        if (isVisitedCTPs(clp) == false)
        {
            visitedCTPs.insert(clp);
            return clpList.push(clp);
        }
        return false;
    }
    inline CxtLockProc popFromCTPWorkList()
    {
        CxtLockProc clp = clpList.pop();
        return clp;
    }
    inline bool isVisitedCTPs(const CxtLockProc& clp) const
    {
        return visitedCTPs.find(clp) != visitedCTPs.end();
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
        CxtStmt clp = cxtStmtList.pop();
        return clp;
    }
    //@}

    /// Push calling context
    void pushCxt(CallStrCxt& cxt, const SVFInstruction* call, const SVFFunction* callee);
    /// Match context
    bool matchCxt(CallStrCxt& cxt, const SVFInstruction* call, const SVFFunction* callee);

    /// Whether it is a lock site
    inline bool isTDFork(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->isTDFork(call);
    }
    /// Whether it is a lock site
    inline bool isTDAcquire(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->isTDAcquire(call);
    }
    /// Whether it is a unlock site
    inline bool isTDRelease(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->isTDRelease(call);
    }
    /// Get lock value
    inline const SVFValue* getLockVal(const SVFInstruction* call)
    {
        return getTCG()->getThreadAPI()->getLockVal(call);
    }
    /// ThreadCallGraph
    inline ThreadCallGraph* getTCG() const
    {
        return tct->getThreadCallGraph();
    }

    /// TCT
    TCT* tct;

    /// context-sensitive statement worklist
    CxtStmtWorkList cxtStmtList;

    /// Map a statement to all its context-sensitive statements
    InstToCxtStmtSet instToCxtStmtSet;


    /// Context-sensitive locks
    CxtLockSet cxtLockset;

    /// Map a context-sensitive lock to its lock span statements
    /// Map a context-sensitive statement to its context-sensitive lock
    //@{
    CxtLockToSpan cxtLocktoSpan;
    CxtStmtToCxtLockSet cxtStmtToCxtLockSet;
    //@}

    /// Following data structures are used for collecting context-sensitive locks
    //@{
    CxtLockProcVec clpList;	/// CxtLockProc List
    CxtLockProcSet visitedCTPs; /// Record all visited clps
    //@}

    /// Collecting lock/unlock sites
    //@{
    InstSet locksites;
    InstSet unlocksites;
    //@}

    /// Candidate functions which relevant to locks/unlocks
    //@{
    FunSet lockcandidateFuncSet;
    //@}

    /// Used for context-insensitive intra-procedural locks
    //@{
    CILockToSpan ciLocktoSpan;
    InstToInstSetMap instCILocksMap;
    InstToInstSetMap instTocondCILocksMap;
    //@}


public:
    double lockTime;
    u32_t numOfTotalQueries;
    u32_t numOfLockedQueries;
    double lockQueriesTime;
};

} // End namespace SVF

#endif /* INCLUDE_MTA_LockAnalysis_H_ */
