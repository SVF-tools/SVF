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

#include "Util/SVFModule.h"
#include "SVF-FE/DataFlowUtil.h"
#include "Util/Conditions.h"
#include "Util/WorkList.h"

namespace SVF
{

/**
 * PathCondAllocator allocates conditions for each basic block of a certain CFG.
 */
class PathCondAllocator
{

public:
    static u32_t totalCondNum;

    typedef DdNode Condition;
    typedef Map<u32_t,Condition*> CondPosMap;		///< map a branch to its Condition
    typedef Map<const BasicBlock*, CondPosMap > BBCondMap;	// map bb to a Condition
    typedef Map<const Condition*, const Instruction* > CondToTermInstMap;	// map a condition to its branch instruction
    typedef Set<const BasicBlock*> BasicBlockSet;
    typedef Map<const Function*,  BasicBlockSet> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef Map<const BasicBlock*, Condition*> BBToCondMap;	///< map a basic block to its condition during control-flow guard computation
    typedef FIFOWorkList<const BasicBlock*> CFWorkList;	///< worklist for control-flow guard computation

    typedef Map<u32_t,Condition*> IndexToConditionMap;

    /// Constructor
    PathCondAllocator()
    {
        getBddCondManager();
    }
    /// Destructor
    virtual ~PathCondAllocator()
    {
        destroy();
    }
    static inline Condition* trueCond()
    {
        return getBddCondManager()->getTrueCond();
    }

    static inline Condition* falseCond()
    {
        return getBddCondManager()->getFalseCond();
    }

    /// Statistics
    //@{
    static inline u32_t getMemUsage()
    {
        return getBddCondManager()->getBDDMemUsage();
    }
    static inline u32_t getCondNum()
    {
        return getBddCondManager()->getCondNumber();
    }
    static inline u32_t getMaxLiveCondNumber()
    {
        return getBddCondManager()->getMaxLiveCondNumber();
    }
    //@}

    /// Perform path allocation
    void allocate(const SVFModule* module);

    /// Get llvm conditional expression
    inline const Instruction* getCondInst(const Condition* cond) const
    {
        CondToTermInstMap::const_iterator it = condToInstMap.find(cond);
        assert(it!=condToInstMap.end() && "this should be a fresh condition");
        return it->second;
    }

    /// Get dominators
    inline DominatorTree* getDT(const Function* fun)
    {
        return cfInfoBuilder.getDT(fun);
    }
    /// Get Postdominators
    inline PostDominatorTree* getPostDT(const Function* fun)
    {
        return cfInfoBuilder.getPostDT(fun);
    }
    /// Get LoopInfo
    inline LoopInfo* getLoopInfo(const Function* f)
    {
        return cfInfoBuilder.getLoopInfo(f);
    }

    /// Condition operations
    //@{
    inline Condition* condAnd(Condition* lhs, Condition* rhs)
    {
        return bddCondMgr->AND(lhs,rhs);
    }
    inline Condition* condOr(Condition* lhs, Condition* rhs)
    {
        return bddCondMgr->OR(lhs,rhs);
    }
    inline Condition* condNeg(Condition* cond)
    {
        return bddCondMgr->NEG(cond);
    }
    inline Condition* getTrueCond() const
    {
        return bddCondMgr->getTrueCond();
    }
    inline Condition* getFalseCond() const
    {
        return bddCondMgr->getFalseCond();
    }
    /// Given an index, get its condition
    inline Condition* getCond(u32_t i) const
    {
        IndexToConditionMap::const_iterator it = indexToDDNodeMap.find(i);
        assert(it!=indexToDDNodeMap.end() && "condition not found!");
        return it->second;
    }
    /// Iterator every element of the bdd
    inline NodeBS exactCondElem(Condition* cond)
    {
        NodeBS elems;
        bddCondMgr->BddSupport(cond,elems);
        return elems;
    }
    /// Decrease reference counting for the bdd
    inline void markForRelease(Condition* cond)
    {
        bddCondMgr->markForRelease(cond);
    }
    /// Print debug information for this condition
    inline void printDbg(Condition* cond)
    {
        bddCondMgr->printDbg(cond);
    }
    inline std::string dumpCond(Condition* cond) const
    {
        return bddCondMgr->dumpStr(cond);
    }
    //@}

    /// Guard Computation for a value-flow (between two basic blocks)
    //@{
    virtual Condition* ComputeIntraVFGGuard(const BasicBlock* src, const BasicBlock* dst);
    virtual Condition* ComputeInterCallVFGGuard(const BasicBlock* src, const BasicBlock* dst, const BasicBlock* callBB);
    virtual Condition* ComputeInterRetVFGGuard(const BasicBlock* src, const BasicBlock* dst, const BasicBlock* retBB);

    /// Get complement condition (from B1 to B0) according to a complementBB (BB2) at a phi
    /// e.g., B0: dstBB; B1:incomingBB; B2:complementBB
    virtual Condition* getPHIComplementCond(const BasicBlock* BB1, const BasicBlock* BB2, const BasicBlock* BB0);

    inline void clearCFCond()
    {
        bbToCondMap.clear();
    }
    /// Set current value for branch condition evaluation
    inline void setCurEvalVal(const Value* val)
    {
        curEvalVal = val;
    }
    /// Get current value for branch condition evaluation
    inline const Value* getCurEvalVal() const
    {
        return curEvalVal;
    }
    //@}

    /// Print out the path condition information
    void printPathCond();

private:

    /// Allocate path condition for every basic block
    virtual void allocateForBB(const BasicBlock& bb);

    /// Get/Set a branch condition, and its terminator instruction
    //@{
    /// Set branch condition
    void setBranchCond(const BasicBlock *bb, const BasicBlock *succ, Condition* cond);
    /// Get branch condition
    Condition* getBranchCond(const BasicBlock * bb, const BasicBlock *succ) const;
    ///Get a condition, evaluate the value for conditions if necessary (e.g., testNull like express)
    inline Condition* getEvalBrCond(const BasicBlock * bb, const BasicBlock *succ)
    {
        if(const Value* val = getCurEvalVal())
            return evaluateBranchCond(bb, succ, val);
        else
            return getBranchCond(bb,succ);
    }
    //@}
    /// Evaluate branch conditions
    //@{
    /// Evaluate the branch condtion
    Condition* evaluateBranchCond(const BasicBlock * bb, const BasicBlock *succ, const Value* val) ;
    /// Evaluate loop exit branch
    Condition* evaluateLoopExitBranch(const BasicBlock * bb, const BasicBlock *succ);
    /// Return branch condition after evaluating test null like expression
    Condition* evaluateTestNullLikeExpr(const BranchInst* brInst, const BasicBlock *succ, const Value* val);
    /// Return condition when there is a branch calls program exit
    Condition* evaluateProgExit(const BranchInst* brInst, const BasicBlock *succ);
    /// Collect basic block contains program exit function call
    void collectBBCallingProgExit(const BasicBlock& bb);
    bool isBBCallsProgExit(const BasicBlock* bb);
    //@}

    /// Evaluate test null/not null like expressions
    //@{
    /// Return true if the predicate of this compare instruction is equal
    bool isEQCmp(const CmpInst* cmp) const;
    /// Return true if the predicate of this compare instruction is not equal
    bool isNECmp(const CmpInst* cmp) const;
    /// Return true if this is a test null expression
    bool isTestNullExpr(const Value* test,const Value* val) const;
    /// Return true if this is a test not null expression
    bool isTestNotNullExpr(const Value* test,const Value* val) const;
    /// Return true if two values on the predicate are what we want
    bool isTestContainsNullAndTheValue(const CmpInst* cmp, const Value* val) const;
    //@}


    /// Get/Set control-flow conditions
    //@{
    inline bool setCFCond(const BasicBlock* bb, Condition* cond)
    {
        BBToCondMap::iterator it = bbToCondMap.find(bb);
        if(it!=bbToCondMap.end() && it->second == cond)
            return false;

        bbToCondMap[bb] = cond;
        return true;
    }
    inline Condition* getCFCond(const BasicBlock* bb) const
    {
        BBToCondMap::const_iterator it = bbToCondMap.find(bb);
        if(it==bbToCondMap.end())
        {
            return getFalseCond();
        }
        return it->second;
    }
    //@}

    /// Create new BDD condition
    inline Condition* createNewCond(u32_t i)
    {
        assert(indexToDDNodeMap.find(i)==indexToDDNodeMap.end() && "This should be fresh index to create new BDD");
        Condition* d = bddCondMgr->Cudd_bdd(i);
        indexToDDNodeMap[i] = d;
        return d;
    }
    /// Allocate a new condition
    inline Condition* newCond(const Instruction* inst)
    {
        Condition* cond = createNewCond(totalCondNum++);
        assert(condToInstMap.find(cond)==condToInstMap.end() && "this should be a fresh condition");
        condToInstMap[cond] = inst;
        return cond;
    }

    /// Used internally, not supposed to be exposed to other classes
    static BddCondManager* getBddCondManager()
    {
        if(bddCondMgr==nullptr)
            bddCondMgr = new BddCondManager();
        return bddCondMgr;
    }

    /// Release memory
    void destroy();

    CondToTermInstMap condToInstMap;		///< map a condition to its corresponding llvm instruction
    PTACFInfoBuilder cfInfoBuilder;		    ///< map a function to its loop info
    FunToExitBBsMap funToExitBBsMap;		///< map a function to all its basic blocks calling program exit
    BBToCondMap bbToCondMap;				///< map a basic block to its path condition starting from root
    const Value* curEvalVal;			///< current llvm value to evaluate branch condition when computing guards

protected:
    static BddCondManager* bddCondMgr;		///< bbd manager
    BBCondMap bbConds;						///< map basic block to its successors/predecessors branch conditions
    IndexToConditionMap indexToDDNodeMap;

};

} // End namespace SVF

#endif /* PATHALLOCATOR_H_ */
