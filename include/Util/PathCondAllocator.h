//===- PathCondAllocator.h -- Path condition manipulation---------------------//
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
 * PathAllocator.h
 *
 *  Created on: Apr 3, 2014
 *      Author: Yulei Sui
 */

#ifndef PATHALLOCATOR_H_
#define PATHALLOCATOR_H_

#include "Util/AnalysisUtil.h"
#include "Util/Conditions.h"
#include "Util/WorkList.h"
#include "Util/DataFlowUtil.h"

/**
 * PathCondAllocator allocates conditions for each basic block of a certain CFG.
 */
class PathCondAllocator {

public:
    static u32_t totalCondNum;

    typedef DdNode Condition;
    typedef std::map<u32_t,Condition*> CondPosMap;		///< map a branch to its Condition
    typedef std::map<const llvm::BasicBlock*, CondPosMap > BBCondMap;	// map bb to a Condition
    typedef std::map<const Condition*, const llvm::TerminatorInst* > CondToTermInstMap;	// map a condition to its branch instruction
    typedef std::set<const llvm::BasicBlock*> BasicBlockSet;
    typedef std::map<const llvm::Function*,  BasicBlockSet> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef std::map<const llvm::BasicBlock*, Condition*> BBToCondMap;	///< map a basic block to its condition during control-flow guard computation
    typedef FIFOWorkList<const llvm::BasicBlock*> CFWorkList;	///< worklist for control-flow guard computation

    /// Constructor
    PathCondAllocator() {
        getBddCondManager();
    }
    /// Destructor
    virtual ~PathCondAllocator() {
        destroy();
    }
    static inline Condition* trueCond() {
        return getBddCondManager()->getTrueCond();
    }

    static inline Condition* falseCond() {
        return getBddCondManager()->getFalseCond();
    }

    /// Statistics
    //@{
    static inline u32_t getMemUsage() {
        return getBddCondManager()->getBDDMemUsage();
    }
    static inline u32_t getCondNum() {
        return getBddCondManager()->getCondNumber();
    }
    static inline u32_t getMaxLiveCondNumber() {
        return getBddCondManager()->getMaxLiveCondNumber();
    }
    //@}

    /// Perform path allocation
    void allocate(const SVFModule module);

    /// Get llvm conditional expression
    inline const llvm::TerminatorInst* getCondInst(const Condition* cond) const {
        CondToTermInstMap::const_iterator it = condToInstMap.find(cond);
        assert(it!=condToInstMap.end() && "this should be a fresh condition");
        return it->second;
    }

    /// Get dominators
    inline llvm::DominatorTree* getDT(const llvm::Function* fun) {
        return cfInfoBuilder.getDT(fun);
    }
    /// Get Postdominators
    inline llvm::PostDominatorTree* getPostDT(const llvm::Function* fun) {
        return cfInfoBuilder.getPostDT(fun);
    }
    /// Get LoopInfo
    PTALoopInfo* getLoopInfo(const llvm::Function* f) {
        return cfInfoBuilder.getLoopInfo(f);
    }

    /// Condition operations
    //@{
    inline Condition* condAnd(Condition* lhs, Condition* rhs) {
        return bddCondMgr->AND(lhs,rhs);
    }
    inline Condition* condOr(Condition* lhs, Condition* rhs) {
        return bddCondMgr->OR(lhs,rhs);
    }
    inline Condition* condNeg(Condition* cond) {
        return bddCondMgr->NEG(cond);
    }
    inline Condition* getTrueCond() const {
        return bddCondMgr->getTrueCond();
    }
    inline Condition* getFalseCond() const {
        return bddCondMgr->getFalseCond();
    }
    /// Given an index, get its condition
    inline Condition* getCond(u32_t i) const {
        return bddCondMgr->getCond(i);
    }
    /// Iterator every element of the bdd
    inline NodeBS exactCondElem(Condition* cond) {
        NodeBS elems;
        bddCondMgr->BddSupport(cond,elems);
        return elems;
    }
    /// Decrease reference counting for the bdd
    inline void markForRelease(Condition* cond) {
        bddCondMgr->markForRelease(cond);
    }
    /// Print debug information for this condition
    inline void printDbg(Condition* cond) {
        bddCondMgr->printDbg(cond);
    }
    inline std::string dumpCond(Condition* cond) const {
        return bddCondMgr->dumpStr(cond);
    }
    //@}

    /// Guard Computation for a value-flow (between two basic blocks)
    //@{
    virtual Condition* ComputeIntraVFGGuard(const llvm::BasicBlock* src, const llvm::BasicBlock* dst);
    virtual Condition* ComputeInterCallVFGGuard(const llvm::BasicBlock* src, const llvm::BasicBlock* dst, const llvm::BasicBlock* callBB);
    virtual Condition* ComputeInterRetVFGGuard(const llvm::BasicBlock* src, const llvm::BasicBlock* dst, const llvm::BasicBlock* retBB);

    /// Get complement condition (from B1 to B0) according to a complementBB (BB2) at a phi
    /// e.g., B0: dstBB; B1:incomingBB; B2:complementBB
    virtual Condition* getPHIComplementCond(const llvm::BasicBlock* BB1, const llvm::BasicBlock* BB2, const llvm::BasicBlock* BB0);

    inline void clearCFCond() {
        bbToCondMap.clear();
    }
    /// Set current value for branch condition evaluation
    inline void setCurEvalVal(const llvm::Value* val) {
        curEvalVal = val;
    }
    /// Get current value for branch condition evaluation
    inline const llvm::Value* getCurEvalVal() const {
        return curEvalVal;
    }
    //@}

    /// Print out the path condition information
    void printPathCond();

private:

    /// Allocate path condition for every basic block
    virtual void allocateForBB(const llvm::BasicBlock& bb);

    /// Get/Set a branch condition, and its terminator instruction
    //@{
    /// Set branch condition
    void setBranchCond(const llvm::BasicBlock *bb, const llvm::BasicBlock *succ, Condition* cond);
    /// Get branch condition
    Condition* getBranchCond(const llvm::BasicBlock * bb, const llvm::BasicBlock *succ) const;
    ///Get a condition, evaluate the value for conditions if necessary (e.g., testNull like express)
    inline Condition* getEvalBrCond(const llvm::BasicBlock * bb, const llvm::BasicBlock *succ) {
        if(const llvm::Value* val = getCurEvalVal())
            return evaluateBranchCond(bb, succ, val);
        else
            return getBranchCond(bb,succ);
    }
    //@}
    /// Evaluate branch conditions
    //@{
    /// Evaluate the branch condtion
    Condition* evaluateBranchCond(const llvm::BasicBlock * bb, const llvm::BasicBlock *succ, const llvm::Value* val) ;
    /// Evaluate loop exit branch
    Condition* evaluateLoopExitBranch(const llvm::BasicBlock * bb, const llvm::BasicBlock *succ);
    /// Return branch condition after evaluating test null like expression
    Condition* evaluateTestNullLikeExpr(const llvm::BranchInst* brInst, const llvm::BasicBlock *succ, const llvm::Value* val);
    /// Return condition when there is a branch calls program exit
    Condition* evaluateProgExit(const llvm::BranchInst* brInst, const llvm::BasicBlock *succ);
    /// Collect basic block contains program exit function call
    void collectBBCallingProgExit(const llvm::BasicBlock& bb);
    bool isBBCallsProgExit(const llvm::BasicBlock* bb);
    //@}

    /// Evaluate test null/not null like expressions
    //@{
    /// Return true if the predicate of this compare instruction is equal
    bool isEQCmp(const llvm::CmpInst* cmp) const;
    /// Return true if the predicate of this compare instruction is not equal
    bool isNECmp(const llvm::CmpInst* cmp) const;
    /// Return true if this is a test null expression
    bool isTestNullExpr(const llvm::Value* test,const llvm::Value* val) const;
    /// Return true if this is a test not null expression
    bool isTestNotNullExpr(const llvm::Value* test,const llvm::Value* val) const;
    /// Return true if two values on the predicate are what we want
    bool isTestContainsNullAndTheValue(const llvm::CmpInst* cmp, const llvm::Value* val) const;
    //@}


    /// Get/Set control-flow conditions
    //@{
    inline bool setCFCond(const llvm::BasicBlock* bb, Condition* cond) {
        BBToCondMap::iterator it = bbToCondMap.find(bb);
        if(it!=bbToCondMap.end() && it->second == cond)
            return false;

        bbToCondMap[bb] = cond;
        return true;
    }
    inline Condition* getCFCond(const llvm::BasicBlock* bb) const {
        BBToCondMap::const_iterator it = bbToCondMap.find(bb);
        if(it==bbToCondMap.end()) {
            return getFalseCond();
        }
        return it->second;
    }
    //@}

    /// Allocate a new condition
    inline Condition* newCond(const llvm::TerminatorInst* inst) {
        Condition* cond = bddCondMgr->createNewCond(totalCondNum++);
        assert(condToInstMap.find(cond)==condToInstMap.end() && "this should be a fresh condition");
        condToInstMap[cond] = inst;
        return cond;
    }
    /// Used internally, not supposed to be exposed to other classes
    static BddCondManager* getBddCondManager() {
        if(bddCondMgr==NULL)
            bddCondMgr = new BddCondManager();
        return bddCondMgr;
    }

    /// Release memory
    void destroy();

    CondToTermInstMap condToInstMap;		///< map a condition to its corresponding llvm instruction
    PTACFInfoBuilder cfInfoBuilder;		    ///< map a function to its loop info
    FunToExitBBsMap funToExitBBsMap;		///< map a function to all its basic blocks calling program exit
    BBToCondMap bbToCondMap;				///< map a basic block to its path condition starting from root
    const llvm::Value* curEvalVal;			///< current llvm value to evaluate branch condition when computing guards

protected:
    static BddCondManager* bddCondMgr;		///< bbd manager
    BBCondMap bbConds;						///< map basic block to its successors/predecessors branch conditions

};

#endif /* PATHALLOCATOR_H_ */
