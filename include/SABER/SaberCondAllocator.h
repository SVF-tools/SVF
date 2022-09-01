//===- SaberCondAllocator.h -- Path condition manipulation---------------------//
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
#include "SVF-FE/BasicTypes.h"
#include "Util/WorkList.h"
#include "Graphs/SVFG.h"
#include "Util/Z3Expr.h"


namespace SVF
{

/**
 * SaberCondAllocator allocates conditions for each basic block of a certain CFG.
 */
class SaberCondAllocator
{

public:

    typedef Z3Expr Condition;   /// z3 condition
    typedef Map<u32_t, const Instruction *> IndexToTermInstMap; /// id to instruction map for z3
    typedef Map<u32_t,Condition> CondPosMap;		///< map a branch to its Condition
    typedef Map<const BasicBlock*, CondPosMap > BBCondMap;	/// map bb to a Condition
    typedef Set<const BasicBlock*> BasicBlockSet;
    typedef Map<const Function*,  BasicBlockSet> FunToExitBBsMap;  ///< map a function to all its basic blocks calling program exit
    typedef Map<const BasicBlock*, Condition> BBToCondMap;	///< map a basic block to its condition during control-flow guard computation
    typedef FIFOWorkList<const BasicBlock*> CFWorkList;	///< worklist for control-flow guard computation


    /// Constructor
    SaberCondAllocator();

    /// Destructor
    virtual ~SaberCondAllocator()
    {
    }
    /// Statistics
    //@{
    inline std::string getMemUsage()
    {
        u32_t vmrss, vmsize;
        if (SVFUtil::getMemoryUsageKB(&vmrss, &vmsize))
            return std::to_string(vmsize) + "KB";
        else
            return "cannot read memory usage";
    }
    inline u32_t getCondNum()
    {
        return totalCondNum;
    }
    //@}

    /// Condition operations
    //@{
    inline Condition condAnd(const Condition& lhs, const Condition& rhs)
    {
        return Condition::AND(lhs,rhs);
    }
    inline Condition condOr(const Condition& lhs, const Condition& rhs)
    {
        return Condition::OR(lhs,rhs);
    }
    inline Condition condNeg(const Condition& cond)
    {
        return Condition::NEG(cond);
    }
    inline Condition getTrueCond() const
    {
        return Condition::getTrueCond();
    }
    inline Condition getFalseCond() const
    {
        return Condition::getFalseCond();
    }
    /// Iterator every element of the condition
    inline NodeBS exactCondElem(const Condition& cond)
    {
        NodeBS elems;
        extractSubConds(cond, elems);
        return elems;
    }

    inline std::string dumpCond(const Condition& cond) const
    {
        return Condition::dumpStr(cond);
    }

    /// Allocate a new condition
    Condition newCond(const Instruction* inst);

    /// Perform path allocation
    void allocate(const SVFModule* module);

    /// Get/Set instruction based on Z3 expression id
    //{@
    inline const Instruction *getCondInst(u32_t id) const
    {
        IndexToTermInstMap::const_iterator it = idToTermInstMap.find(id);
        assert(it != idToTermInstMap.end() && "this should be a fresh condition");
        return it->second;
    }

    inline void setCondInst(const Condition &condition, const Instruction *inst)
    {
        assert(idToTermInstMap.find(condition.id()) == idToTermInstMap.end() && "this should be a fresh condition");
        idToTermInstMap[condition.id()] = inst;
    }
    //@}

    bool isNegCond(u32_t id) const
    {
        return negConds.test(id);
    }

    inline const bool pdtDominates(const BasicBlock* bbKey, const BasicBlock* bbValue)
    {
        return dominates(bbKey,bbValue,SVFFunction::DtK::PostDominatorTree);
    }

    inline const bool dtDominates(const BasicBlock* bbKey, const BasicBlock* bbValue)
    {
        return dominates(bbKey,bbValue,SVFFunction::DtK::DominatorTree);;
    }

    inline const bool dominates(const BasicBlock* bbKey, const BasicBlock* bbValue, const SVFFunction::DtK dtType)
    {
        if (bbKey == bbValue)
            return true;
        SVFModule* svfModule = PAG::getPAG()->getModule();
        if (svfModule != nullptr)
        {
            const SVFFunction*  func = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(bbKey->getParent());
            Map<const BasicBlock*,Set<const BasicBlock*>> dtBBsMap;
            if (dtType == SVFFunction::DtK::DominatorTree)
            {
                dtBBsMap = func->getDtBBsMap();
            } 
            else if (dtType == SVFFunction::DtK::PostDominatorTree)
            {
                dtBBsMap = func->getPostDtBBsMap();
            }
            Map<const BasicBlock*,Set<const BasicBlock*>>::const_iterator mapIter = dtBBsMap.find(bbKey);
            if (mapIter != dtBBsMap.end())
            {
                Set<const BasicBlock*> dtBBs = mapIter->second;
                Set<const BasicBlock*>::const_iterator setIter = dtBBs.find(bbValue);
                if (setIter != dtBBs.end())
                {
                    return true;
                }
            } 
        }
        return false;
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
    virtual Condition ComputeIntraVFGGuard(const BasicBlock* src, const BasicBlock* dst);
    virtual Condition ComputeInterCallVFGGuard(const BasicBlock* src, const BasicBlock* dst, const BasicBlock* callBB);
    virtual Condition ComputeInterRetVFGGuard(const BasicBlock* src, const BasicBlock* dst, const BasicBlock* retBB);

    /// Get complement condition (from B1 to B0) according to a complementBB (BB2) at a phi
    /// e.g., B0: dstBB; B1:incomingBB; B2:complementBB
    virtual Condition getPHIComplementCond(const BasicBlock* BB1, const BasicBlock* BB2, const BasicBlock* BB0);

    inline void clearCFCond()
    {
        bbToCondMap.clear();
    }
    /// Set current value for branch condition evaluation
    inline void setCurEvalSVFGNode(const SVFGNode* node)
    {
        curEvalSVFGNode = node;
    }
    /// Get current value for branch condition evaluation
    inline const SVFGNode* getCurEvalSVFGNode() const
    {
        return curEvalSVFGNode;
    }
    //@}

    /// Print out the path condition information
    void printPathCond();

    /// whether condition is satisfiable
    bool isSatisfiable(const Condition& condition);

    /// whether condition is satisfiable for all possible boolean guards
    inline bool isAllPathReachable(Condition& condition)
    {
        return isEquivalentBranchCond(condition, Condition::getTrueCond());
    }

    /// Whether lhs and rhs are equivalent branch conditions
    bool isEquivalentBranchCond(const Condition &lhs, const Condition &rhs) const;

    inline ICFG* getICFG() const
    {
        return PAG::getPAG()->getICFG();
    }

    /// Get/Set control-flow conditions
    //@{
    inline bool setCFCond(const BasicBlock* bb, const Condition& cond)
    {
        BBToCondMap::iterator it = bbToCondMap.find(bb);
        // until a fixed-point is reached (condition is not changed)
        if(it!=bbToCondMap.end() && isEquivalentBranchCond(it->second, cond))
            return false;

        bbToCondMap[bb] = cond;
        return true;
    }
    inline Condition getCFCond(const BasicBlock* bb) const
    {
        BBToCondMap::const_iterator it = bbToCondMap.find(bb);
        if(it==bbToCondMap.end())
        {
            return getFalseCond();
        }
        return it->second;
    }
    //@}


    /// mark neg Z3 expression
    inline void setNegCondInst(const Condition &condition, const Instruction *inst)
    {
        setCondInst(condition, inst);
        negConds.set(condition.id());
    }
private:

    /// Allocate path condition for every basic block
    virtual void allocateForBB(const BasicBlock& bb);

    /// Get/Set a branch condition, and its terminator instruction
    //@{
    /// Set branch condition
    void setBranchCond(const BasicBlock *bb, const BasicBlock *succ, const Condition& cond);
    /// Get branch condition
    Condition getBranchCond(const BasicBlock * bb, const BasicBlock *succ) const;
    ///Get a condition, evaluate the value for conditions if necessary (e.g., testNull like express)
    Condition getEvalBrCond(const BasicBlock * bb, const BasicBlock *succ);
    //@}
    /// Evaluate branch conditions
    //@{
    /// Evaluate the branch condtion
    Condition evaluateBranchCond(const BasicBlock * bb, const BasicBlock *succ) ;
    /// Evaluate loop exit branch
    Condition evaluateLoopExitBranch(const BasicBlock * bb, const BasicBlock *succ);
    /// Return branch condition after evaluating test null like expression
    Condition evaluateTestNullLikeExpr(const BranchStmt* branchStmt, const BasicBlock *succ);
    /// Return condition when there is a branch calls program exit
    Condition evaluateProgExit(const BranchStmt* branchStmt, const BasicBlock *succ);
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
    bool isTestNullExpr(const Value* test) const;
    /// Return true if this is a test not null expression
    bool isTestNotNullExpr(const Value* test) const;
    /// Return true if two values on the predicate are what we want
    bool isTestContainsNullAndTheValue(const CmpInst* cmp) const;
    //@}

    /// Release memory
    void destroy()
    {

    }

    /// extract subexpression from a Z3 expression
    void extractSubConds(const Condition &condition, NodeBS &support) const;


    PTACFInfoBuilder cfInfoBuilder;		    ///< map a function to its loop info
    FunToExitBBsMap funToExitBBsMap;		///< map a function to all its basic blocks calling program exit
    BBToCondMap bbToCondMap;				///< map a basic block to its path condition starting from root
    const SVFGNode* curEvalSVFGNode{};			///< current llvm value to evaluate branch condition when computing guards
    IndexToTermInstMap idToTermInstMap;     ///key: z3 expression id, value: instruction
    NodeBS negConds;                        ///bit vector for distinguish neg
    std::vector<Condition> conditionVec;          /// vector storing z3expression
    static u32_t totalCondNum; /// a counter for fresh condition

protected:
    BBCondMap bbConds;						///< map basic block to its successors/predecessors branch conditions

};

} // End namespace SVF

#endif /* PATHALLOCATOR_H_ */
