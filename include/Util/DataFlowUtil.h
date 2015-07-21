/* SVF - Static Value-Flow Analysis Framework
Copyright (C) 2015 Yulei Sui
Copyright (C) 2015 Jingling Xue

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

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
        getBase().Analyze(dt);
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

    /// Check whether two inloop scevs have same start and step
    static bool sameStartAndStep(llvm::ScalarEvolution* SE1, const llvm::SCEV *se1, llvm::ScalarEvolution* SE2,const llvm::SCEV *se2) {
        // We only handle AddRec here
        const llvm::SCEVAddRecExpr *addRec1 = llvm::dyn_cast<llvm::SCEVAddRecExpr>(se1);
        const llvm::SCEVAddRecExpr *addRec2 = llvm::dyn_cast<llvm::SCEVAddRecExpr>(se2);
        if (!addRec1 || !addRec2)   return false;

        // Compare the starts
        bool sameStart = (addRec1->getStart() == addRec2->getStart());
        if (!sameStart)     return false;

        // Compare the steps
        bool sameStep = (addRec1->getStepRecurrence(*SE1)
                         == addRec2->getStepRecurrence(*SE2));
        if (!sameStep)      return false;

        return true;
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
