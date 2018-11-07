/*
 * RaceDetector.h
 *
 *  Created on: May 14, 2014
 *      Author: Yulei Sui, Peng Di
 */

#ifndef MTA_H_
#define MTA_H_

#include <llvm/Pass.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>
#include <set>
#include <vector>

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
class MTA: public llvm::ModulePass {

public:
    typedef std::set<const llvm::LoadInst*> LoadSet;
    typedef std::set<const llvm::StoreInst*> StoreSet;
    typedef std::map<const llvm::Function*, llvm::ScalarEvolution*> FunToSEMap;
    typedef std::map<const llvm::Function*, llvm::LoopInfo*> FunToLoopInfoMap;

    /// Pass ID
    static char ID;

    static llvm::ModulePass* modulePass;

    /// Constructor
    MTA();

    /// Destructor
    virtual ~MTA();


    /// We start the pass here
    virtual bool runOnModule(llvm::Module& module);
    /// We start the pass here
    virtual bool runOnModule(SVFModule module);
    /// Compute MHP
    virtual MHP* computeMHP(SVFModule module);
    /// Compute locksets
    virtual LockAnalysis* computeLocksets(TCT* tct);
    /// Perform detection
    virtual void detect(SVFModule module);

    /// Pass name
    virtual llvm::StringRef getPassName() const {
        return "Multi threaded program analysis pass";
    }

    /// Get analysis usage
    inline virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const {
        /// do not intend to change the IR in this pass,
        au.setPreservesAll();
        au.addRequired<llvm::ScalarEvolutionWrapperPass>();
    }

    // Get ScalarEvolution for Function F.
    static inline llvm::ScalarEvolution* getSE(const llvm::Function *F) {
        FunToSEMap::iterator it = func2ScevMap.find(F);
        if (it != func2ScevMap.end())
            return it->second;
        llvm::ScalarEvolutionWrapperPass *scev = &modulePass->getAnalysis<llvm::ScalarEvolutionWrapperPass>(*const_cast<llvm::Function*>(F));
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

#endif /* MTA_H_ */
