/*
 * RaceDetector.h
 *
 *  Created on: May 14, 2014
 *      Author: Yulei Sui, Peng Di
 */

#ifndef MTA_H_
#define MTA_H_

#include <set>
#include <vector>
#include "Util/BasicTypes.h"

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
    virtual StringRef getPassName() const
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
