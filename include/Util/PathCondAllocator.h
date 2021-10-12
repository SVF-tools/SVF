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
#include "Graphs/ICFGNode.h"
#include "Graphs/PAG.h"

namespace SVF
{

/**
 * PathCondAllocator allocates conditions for each basic block of a certain CFG.
 */
class PathCondAllocator
{

public:
    static u32_t totalCondNum;

//    typedef DdNode Condition;     /// bdd condition
    typedef CondExpr Condition;   /// z3 condition

    typedef Map<u32_t,Condition*> CondPosMap;		///< map a branch to its Condition
    typedef Map<const BasicBlock*, CondPosMap > BBCondMap;	// map bb to a Condition
    typedef Map<const ICFGNode*, CondPosMap> ICFGNodeCondMap; // map ICFG node to a Condition
    typedef Map<const Condition*, const Instruction* > CondToTermInstMap;	// map a condition to its branch instruction
    typedef Set<const BasicBlock*> BasicBlockSet;
    typedef Map<const Function*,  BasicBlockSet> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef Map<const ICFGNode*, Condition*> ICFGNodeToCondMap;	///< map a basic block to its condition during control-flow guard computation
    typedef FIFOWorkList<const ICFGNode*> CFWorkList;	///< worklist for control-flow guard computation
    typedef Set<const ICFGNode*> ICFGNodeSet;
    typedef Map<const BasicBlock*, ICFGNodeSet> BBToICFGNodeSet;
//    typedef Map<u32_t,Condition*> IndexToConditionMap;

    /// Constructor
    PathCondAllocator(): condMgr(CondManager::getCondMgr())
    {
        pag = PAG::getPAG();
        icfg = pag->getICFG();
    }
    /// Destructor
    virtual ~PathCondAllocator()
    {
        CondManager::releaseCondMgr();
    }

    /// Statistics
    //@{
    inline std::string getMemUsage()
    {
        return condMgr->getMemUsage();
    }
    inline u32_t getCondNum()
    {
        return condMgr->getCondNumber();
    }
    //@}

    /// Condition operations
    //@{
    inline Condition* condAnd(Condition* lhs, Condition* rhs)
    {
        return condMgr->AND(lhs,rhs);
    }
    inline Condition* condOr(Condition* lhs, Condition* rhs)
    {
        return condMgr->OR(lhs,rhs);
    }
    inline Condition* condNeg(Condition* cond)
    {
        return condMgr->NEG(cond);
    }
    inline Condition* getTrueCond() const
    {
        return condMgr->getTrueCond();
    }
    inline Condition* getFalseCond() const
    {
        return condMgr->getFalseCond();
    }
        /// Iterator every element of the bdd
    inline NodeBS exactCondElem(Condition* cond)
    {
        NodeBS elems;
        condMgr->extractSubConds(cond->getExpr(),elems);
        return elems;
    }

    inline std::string dumpCond(Condition* cond) const
    {
        return condMgr->dumpStr(cond);
    }
    /// Given an z3 expr id, get its condition
    inline Condition* getCond(u32_t i) const
    {
        return condMgr->getCond(i);
    }
    /// Create new z3 condition
    inline Condition* createNewCond(u32_t i)
    {
        return condMgr->createCondForBranch(i);
    }
    /// Allocate a new condition
    inline Condition* newCond(const Instruction* inst)
    {
        Condition* cond = createNewCond(totalCondNum++);
        assert(condToInstMap.find(cond)==condToInstMap.end() && "this should be a fresh condition");
        condToInstMap[cond] = inst;
        return cond;
    }
    //@}

    /// Perform path allocation
    void allocate(const SVFModule* module);

    /// Get llvm conditional expression
    inline const Instruction* getCondInst(const Condition* cond) const
    {
        auto it = condToInstMap.find(cond);
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


    /// Guard Computation for a value-flow (between two basic blocks)
    //@{
    virtual Condition* ComputeIntraVFGGuard(const ICFGNode* src, const ICFGNode* dst);
    virtual Condition* ComputeInterCallVFGGuard(const ICFGNode* src, const ICFGNode* dst, const ICFGNode* callBB);
    virtual Condition* ComputeInterRetVFGGuard(const ICFGNode* src, const ICFGNode* dst, const ICFGNode* retBB);

    /// Get complement condition (from B1 to B0) according to a complementBB (BB2) at a phi
    /// e.g., B0: dstBB; B1:incomingBB; B2:complementBB
    virtual Condition* getPHIComplementCond(const BasicBlock* BB1, const BasicBlock* BB2, const BasicBlock* BB0);

    inline void clearCFCond()
    {
        icfgNodeToCondMap.clear();
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

    /// whether condition is satisfiable
    inline bool isSatisfiable(Condition* condition){
        return condMgr->isSatisfiable(condition);
    }

    /// whether condition is satisfiable for all possible boolean guards
    inline bool isAllSatisfiable(Condition* condition){
        return condMgr->isAllSatisfiable(condition);
    }

private:

    /// Allocate path condition for every ICFG Node
    virtual void allocateForICFGNode(const ICFGNode* bb);

    /// Get/Set a branch condition, and its terminator instruction
    //@{
    /// Set branch condition
    void setBranchCond(const ICFGNode *icfgNode, const BasicBlock *succ, Condition* cond);
    /// Get branch condition
    Condition* getBranchCond(const ICFGNode * icfgNode, const ICFGEdge *icfgEdge) const;
    ///Get a condition, evaluate the value for conditions if necessary (e.g., testNull like express)
    inline Condition* getEvalBrCond(const ICFGNode * icfgNode, const BasicBlock *succ, const ICFGEdge* icfgEdge)
    {
        if(const Value* val = getCurEvalVal())
            return evaluateBranchCond(icfgNode, succ, val, icfgEdge);
        else
            return getBranchCond(icfgNode,icfgEdge);
    }
    //@}
    /// Evaluate branch conditions
    //@{
    /// Evaluate the branch condtion
    Condition* evaluateBranchCond(const ICFGNode * icfgNode, const BasicBlock *succ, const Value* val, const ICFGEdge* icfgEdge) ;
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
    inline bool setCFCond(const ICFGNode* bb, Condition* cond)
    {
        auto it = icfgNodeToCondMap.find(bb);
        if(it != icfgNodeToCondMap.end() && *(it->second) == *cond)
            return false;

        icfgNodeToCondMap[bb] = cond;
        return true;
    }
    inline Condition* getCFCond(const ICFGNode* bb) const
    {
        auto it = icfgNodeToCondMap.find(bb);
        if(it == icfgNodeToCondMap.end())
        {
            return getFalseCond();
        }
        return it->second;
    }
    //@}

    /// Release memory
    void destroy(){
        
    }

    CondToTermInstMap condToInstMap;		///< map a condition to its corresponding llvm instruction
    PTACFInfoBuilder cfInfoBuilder;		    ///< map a function to its loop info
    FunToExitBBsMap funToExitBBsMap;		///< map a function to all its basic blocks calling program exit
    ICFGNodeToCondMap icfgNodeToCondMap;				///< map an ICFG Node to its path condition starting from root
    ICFGNodeCondMap icfgNodeConds;  // map ICFG node to its data and branch conditions
    const Value* curEvalVal{};			///< current llvm value to evaluate branch condition when computing guards

protected:
//    BddCondManager condMgr;		///< bbd manager
    CondManager* condMgr;		///< z3 manager
    BBCondMap bbConds;						///< map basic block to its successors/predecessors branch conditions
    BBToICFGNodeSet bbToICFGNodeSet; ///< map a basic block to its ICFG Nodes
    PAG* pag;
    ICFG* icfg;
//    IndexToConditionMap indexToCondMap;

};

} // End namespace SVF

#endif /* PATHALLOCATOR_H_ */
