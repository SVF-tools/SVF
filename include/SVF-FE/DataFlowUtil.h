//===- DataFlowUtil.h -- Helper functions for data-flow analysis--------------//
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
 * DataFlowUtil.h
 *
 *  Created on: Jun 4, 2013
 *      Author: Yulei Sui
 */

#ifndef DATAFLOWUTIL_H_
#define DATAFLOWUTIL_H_

#include "Util/BasicTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "SVF-FE/BasicTypes.h"

namespace SVF
{

/*!
 * Wrapper for SCEV collected from function pass ScalarEvolution
 */
class PTASCEV
{

public:
    PTASCEV():scev(nullptr), start(nullptr), step(nullptr),ptr(nullptr),inloop(false),tripcount(0) {}

    /// Constructor
    PTASCEV(const Value* p, const SCEV* s, ScalarEvolution* SE): scev(s),start(nullptr), step(nullptr), ptr(p), inloop(false), tripcount(0)
    {
        /*if(const SCEVAddRecExpr* ar = SVFUtil::dyn_cast<SCEVAddRecExpr>(s))
        {
            if (const SCEVConstant *startExpr = SVFUtil::dyn_cast<SCEVConstant>(ar->getStart()))
                start = startExpr->getValue();
            if (const SCEVConstant *stepExpr = SVFUtil::dyn_cast<SCEVConstant>(ar->getStepRecurrence(*SE)))
                step = stepExpr->getValue();
            tripcount = SE->getSmallConstantTripCount(const_cast<Loop*>(ar->getLoop()));
            inloop = true;
        }*/
        inloop = false;
    }
    /// Copy Constructor
    PTASCEV(const PTASCEV& ptase): scev(ptase.scev), start(ptase.start), step(ptase.step), ptr(ptase.ptr), inloop(ptase.inloop),tripcount(ptase.tripcount)
    {

    }

    /// Destructor
    virtual ~PTASCEV()
    {
    }

    const SCEV *scev;
    const Value* start;
    const Value* step;
    const Value *ptr;
    bool inloop;
    unsigned tripcount;

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const PTASCEV& rhs) const
    {
        if(start!=rhs.start)
            return start < rhs.start;
        else if(step!=rhs.step)
            return step < rhs.step;
        else if(ptr!=rhs.ptr)
            return ptr < rhs.ptr;
        else if(tripcount!=rhs.tripcount)
            return tripcount < rhs.tripcount;
        else
            return inloop < rhs.inloop;
    }
    /// Overloading operator=
    inline PTASCEV& operator= (const PTASCEV& rhs)
    {
        if(*this!=rhs)
        {
            start = rhs.start;
            step = rhs.step;
            ptr = rhs.ptr;
            tripcount = rhs.tripcount;
            inloop = rhs.inloop;
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const PTASCEV& rhs) const
    {
        return (start == rhs.start && step == rhs.step && ptr == rhs.ptr && tripcount == rhs.tripcount && inloop == rhs.inloop);
    }
    /// Overloading operator==
    inline bool operator!= (const PTASCEV& rhs) const
    {
        return !(*this==rhs);
    }
};


class PTACFInfoBuilder
{
public:
    typedef Map<const Function*, DominatorTree*> FunToDTMap;  ///< map a function to its dominator tree
    typedef Map<const Function*, PostDominatorTree*> FunToPostDTMap;  ///< map a function to its post dominator tree
    typedef Map<const Function*, LoopInfo*> FunToLoopInfoMap;  ///< map a function to its loop info

    /// Constructor
    PTACFInfoBuilder();

    ~PTACFInfoBuilder();

    /// Get loop info of a function
    LoopInfo* getLoopInfo(const Function* f);

    /// Get post dominator tree of a function
    PostDominatorTree* getPostDT(const Function* f);

    /// Get dominator tree of a function
    DominatorTree* getDT(const Function* f);

private:
    FunToLoopInfoMap funToLoopInfoMap;      ///< map a function to its loop info
    FunToDTMap funToDTMap;                  ///< map a function to its dominator tree
    FunToPostDTMap funToPDTMap;             ///< map a function to its post dominator tree
};


/*!
 * Iterated dominance frontier
 */
class IteratedDominanceFrontier: public llvm::DominanceFrontierBase<BasicBlock, false>
{

private:
    const DominanceFrontier *DF;

    void calculate(BasicBlock *, const DominanceFrontier &DF);

public:
    static char ID;

    IteratedDominanceFrontier() :
        DominanceFrontierBase(), DF(nullptr)
    {
    }

    virtual ~IteratedDominanceFrontier()
    {
    }

//	virtual bool runOnFunction(Function &m) {
//		Frontiers.clear();
//		DF = &getAnalysis<DominanceFrontier>();
//		return false;
//	}

    iterator getIDFSet(BasicBlock *B)
    {
        if (Frontiers.find(B) == Frontiers.end())
            calculate(B, *DF);
        return Frontiers.find(B);
    }
};

} // End namespace SVF

#endif /* DATAFLOWUTIL_H_ */
