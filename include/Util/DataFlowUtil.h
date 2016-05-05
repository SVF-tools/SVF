//===- DataFlowUtil.h -- Helper functions for data-flow analysis--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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

#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

/*!
 * Wrapper for SCEV collected from function pass ScalarEvolution
 */
class PTASCEV {

public:
    PTASCEV():scev(NULL), start(NULL), step(NULL),ptr(NULL),inloop(false),tripcount(0) {}

    /// Constructor
    PTASCEV(const llvm::Value* p, const llvm::SCEV* s, llvm::ScalarEvolution* SE): scev(s),start(NULL), step(NULL), ptr(p) {
        if(const llvm::SCEVAddRecExpr* ar = llvm::dyn_cast<llvm::SCEVAddRecExpr>(s)) {
            if (const llvm::SCEVConstant *startExpr = llvm::dyn_cast<llvm::SCEVConstant>(ar->getStart()))
                start = startExpr->getValue();
            if (const llvm::SCEVConstant *stepExpr = llvm::dyn_cast<llvm::SCEVConstant>(ar->getStepRecurrence(*SE)))
                step = stepExpr->getValue();
            tripcount = SE->getSmallConstantTripCount(const_cast<llvm::Loop*>(ar->getLoop()));
            inloop = true;
        }
    }
    /// Copy Constructor
    PTASCEV(const PTASCEV& ptase): scev(ptase.scev), start(ptase.start), step(ptase.step), ptr(ptase.ptr), inloop(ptase.inloop),tripcount(ptase.tripcount) {

    }

    /// Destructor
    virtual ~PTASCEV() {
    }

    const llvm::SCEV *scev;
    const llvm::Value* start;
    const llvm::Value* step;
    const llvm::Value *ptr;
    bool inloop;
    unsigned tripcount;

    /// Enable compare operator to avoid duplicated item insertion in map or set
    /// to be noted that two vectors can also overload operator()
    inline bool operator< (const PTASCEV& rhs) const {
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
    inline PTASCEV& operator= (const PTASCEV& rhs) {
        if(*this!=rhs) {
            start = rhs.start;
            step = rhs.step;
            ptr = rhs.ptr;
            tripcount = rhs.tripcount;
            inloop = rhs.inloop;
        }
        return *this;
    }
    /// Overloading operator==
    inline bool operator== (const PTASCEV& rhs) const {
        return (start == rhs.start && step == rhs.step && ptr == rhs.ptr && tripcount == rhs.tripcount && inloop == rhs.inloop);
    }
    /// Overloading operator==
    inline bool operator!= (const PTASCEV& rhs) const {
        return !(*this==rhs);
    }
};


/*!
 * LoopInfo used in PTA
 */
class PTALoopInfo : public llvm::LoopInfo {
public:

    PTALoopInfo() : llvm::LoopInfo()
    {}

    bool runOnLI(llvm::Function& fun) {
        releaseMemory();
        llvm::DominatorTree dt;
        dt.recalculate(fun);
        analyze(dt);
        return false;
    }
};

/*!
 * PTA control flow info builder
 * (1) Loop Info
 * (2) Dominator/PostDominator
 * (3) SCEV
 */
class PTACFInfoBuilder {

public:
    typedef std::map<const llvm::Function*, llvm::DominatorTree*> FunToDTMap;  ///< map a function to its dominator tree
    typedef std::map<const llvm::Function*, llvm::PostDominatorTree*> FunToPostDTMap;  ///< map a function to its post dominator tree
    typedef std::map<const llvm::Function*, PTALoopInfo*> FunToLoopInfoMap;  ///< map a function to its loop info

    /// Constructor
    PTACFInfoBuilder() {

    }
    /// Destructor
    ~PTACFInfoBuilder() {
        for(FunToLoopInfoMap::iterator it = funToLoopInfoMap.begin(), eit = funToLoopInfoMap.end(); it!=eit; ++it) {
            delete it->second;
        }
        for(FunToDTMap::iterator it = funToDTMap.begin(), eit = funToDTMap.end(); it!=eit; ++it) {
            delete it->second;
        }
        for(FunToPostDTMap::iterator it = funToPDTMap.begin(), eit = funToPDTMap.end(); it!=eit; ++it) {
            delete it->second;
        }
    }

    /// Get loop info of a function
    PTALoopInfo* getLoopInfo(const llvm::Function* f) {
        llvm::Function* fun = const_cast<llvm::Function*>(f);
        FunToLoopInfoMap::iterator it = funToLoopInfoMap.find(fun);
        if(it==funToLoopInfoMap.end()) {
            PTALoopInfo* loopInfo = new PTALoopInfo();
            loopInfo->runOnLI(*fun);
            funToLoopInfoMap[fun] = loopInfo;
            return loopInfo;
        }
        else
            return it->second;
    }

    /// Get post dominator tree of a function
    llvm::PostDominatorTree* getPostDT(const llvm::Function* f) {
        llvm::Function* fun = const_cast<llvm::Function*>(f);
        FunToPostDTMap::iterator it = funToPDTMap.find(fun);
        if(it==funToPDTMap.end()) {
            llvm::PostDominatorTree* postDT = new llvm::PostDominatorTree();
            postDT->runOnFunction(*fun);
            funToPDTMap[fun] = postDT;
            return postDT;
        }
        else
            return it->second;
    }

    /// Get dominator tree of a function
    llvm::DominatorTree* getDT(const llvm::Function* f) {
        llvm::Function* fun = const_cast<llvm::Function*>(f);
        FunToDTMap::iterator it = funToDTMap.find(fun);
        if(it==funToDTMap.end()) {
            llvm::DominatorTree* dt = new llvm::DominatorTree();
            dt->recalculate(*fun);
            funToDTMap[fun] = dt;
            return dt;
        }
        else
            return it->second;
    }

private:
    FunToLoopInfoMap funToLoopInfoMap;		///< map a function to its loop info
    FunToDTMap funToDTMap;					///< map a function to its dominator tree
    FunToPostDTMap funToPDTMap;				///< map a function to its post dominator tree
};

/*!
 * Iterated dominance frontier
 */
class IteratedDominanceFrontier: public llvm::DominanceFrontierBase<llvm::BasicBlock> {

private:
    const llvm::DominanceFrontier *DF;

    void calculate(llvm::BasicBlock *, const llvm::DominanceFrontier &DF);

public:
    static char ID;

    IteratedDominanceFrontier() :
        llvm::DominanceFrontierBase<llvm::BasicBlock>(false), DF(NULL) {
    }

    virtual ~IteratedDominanceFrontier() {
    }

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
        AU.setPreservesAll();
        AU.addRequired<llvm::DominanceFrontier>();
    }

//	virtual bool runOnFunction(llvm::Function &m) {
//		Frontiers.clear();
//		DF = &getAnalysis<llvm::DominanceFrontier>();
//		return false;
//	}

    iterator getIDFSet(llvm::BasicBlock *B) {
        if (Frontiers.find(B) == Frontiers.end())
            calculate(B, *DF);
        return Frontiers.find(B);
    }
};

#endif /* DATAFLOWUTIL_H_ */
