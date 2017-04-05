//===- BreakConstantGEPs.h - Change constant GEPs into GEP instructions --- --//
//
//                          The SAFECode Compiler
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass changes all GEP constant expressions into GEP instructions.  This
// permits the rest of SAFECode to put run-time checks on them if necessary.
//
//===----------------------------------------------------------------------===//

#ifndef BREAKCONSTANTGEPS_H
#define BREAKCONSTANTGEPS_H

#include <llvm/IR/Dominators.h>
#include <llvm/IR/Module.h>
#include <llvm/Pass.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>

//
// Pass: BreakConstantGEPs
//
// Description:
//  This pass modifies a function so that it uses GEP instructions instead of
//  GEP constant expressions.
//
class BreakConstantGEPs : public llvm::ModulePass {
private:
    // Private methods

    // Private variables

public:
    static char ID;
    BreakConstantGEPs() : ModulePass(ID) {}
    llvm::StringRef getPassName() const {
        return "Remove Constant GEP Expressions";
    }
    virtual bool runOnModule (llvm::Module & M);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
        // This pass does not modify the control-flow graph of the function
        AU.setPreservesCFG();
    }
};


//
// Pass: MergeFunctionRets
//
// Description:
//  This pass modifies a function so that each function only have one unified exit basic block
//
class MergeFunctionRets : public llvm::ModulePass {
private:
    // Private methods

    // Private variables

public:
    static char ID;
    MergeFunctionRets() : ModulePass(ID) {}
    llvm::StringRef getPassName() const {
        return "unify function exit into one dummy exit basic block";
    }
    virtual bool runOnModule (llvm::Module & M) {
        UnifyFunctionExit(M);
        return true;
    }
    inline void UnifyFunctionExit(llvm::Module& module) {
        for (llvm::Module::const_iterator iter = module.begin(), eiter = module.end();
                iter != eiter; ++iter) {
            const llvm::Function& fun = *iter;
            if(fun.isDeclaration())
                continue;
            getUnifyExit(fun)->runOnFunction(const_cast<llvm::Function&>(fun));
        }
    }
    /// Get Unified Exit basic block node
    inline llvm::UnifyFunctionExitNodes* getUnifyExit(const llvm::Function& fn) {
        assert(!fn.isDeclaration() && "external function does not have DF");
        return &getAnalysis<llvm::UnifyFunctionExitNodes>(const_cast<llvm::Function&>(fn));
    }
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const {
        // This pass does not modify the control-flow graph of the function
        AU.addRequired<llvm::UnifyFunctionExitNodes>();
        AU.addPreserved<BreakConstantGEPs>();
    }
};


#endif
