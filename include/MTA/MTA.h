//===- MTA.h -- Analysis of multithreaded programs-------------//
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
 * MTA.h
 *
 *  Created on: May 14, 2014
 *      Author: Yulei Sui, Peng Di
 * 
 * The implementation is based on 
 * Yulei Sui, Peng Di, and Jingling Xue. "Sparse Flow-Sensitive Pointer Analysis for Multithreaded Programs". 
 * 2016 International Symposium on Code Generation and Optimization (CGO'16)
 */

#ifndef MTA_H_
#define MTA_H_

#include <set>
#include <vector>
#include "Util/BasicTypes.h"
#include "SVF-FE/BasicTypes.h"

namespace SVF
{

class PointerAnalysis;
class AndersenWaveDiff;
class ThreadCallGraph;
class MTAStat;
class TCT;
class MHP;
class LockAnalysis;
class SVFModule;

/*!
 * Base data race detector
 */
class MTA: public ModulePass
{

public:
    typedef Set<const LoadInst*> LoadSet;
    typedef Set<const StoreInst*> StoreSet;
    typedef Map<const Function*, ScalarEvolution*> FunToSEMap;
    typedef Map<const Function*, LoopInfo*> FunToLoopInfoMap;

    /// Pass ID
    static char ID;

    static ModulePass* modulePass;

    /// Constructor
    MTA();

    /// Destructor
    virtual ~MTA();


    /// We start the pass here

    virtual bool runOnModule(Module& module);
    /// We start the pass here
    virtual bool runOnModule(SVFModule* module);
    /// Compute MHP
    virtual MHP* computeMHP(SVFModule* module);
    /// Compute locksets
    virtual LockAnalysis* computeLocksets(TCT* tct);
    /// Perform detection
    virtual void detect(SVFModule* module);

    /// Pass name
    virtual llvm::StringRef getPassName() const
    {
        return "Multi threaded program analysis pass";
    }

    void dump(Module &module, MHP *mhp, LockAnalysis *lsa);

    // Get ScalarEvolution for Function F.
    static inline ScalarEvolution* getSE(const Function *F)
    {
        FunToSEMap::iterator it = func2ScevMap.find(F);
        if (it != func2ScevMap.end())
            return it->second;
        ScalarEvolutionWrapperPass *scev = &modulePass->getAnalysis<ScalarEvolutionWrapperPass>(*const_cast<Function*>(F));
        func2ScevMap[F] = &scev->getSE();
        return &scev->getSE();
    }

private:
    ThreadCallGraph* tcg;
    TCT* tct;
    MTAStat* stat;
    static FunToSEMap func2ScevMap;
    static FunToLoopInfoMap func2LoopInfoMap;
};

} // End namespace SVF

#endif /* MTA_H_ */
