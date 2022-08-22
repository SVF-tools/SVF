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

#include "Util/BasicTypes.h"

namespace SVF
{

//
// Pass: BreakConstantGEPs
//
// Description:
//  This pass modifies a function so that it uses GEP instructions instead of
//  GEP constant expressions.
//
class BreakConstantGEPs : public ModulePass
{
private:
    // Private methods

    // Private variables

public:
    static char ID;
    BreakConstantGEPs() : ModulePass(ID) {}
    llvm::StringRef getPassName() const
    {
        return "Remove Constant GEP Expressions";
    }
    virtual bool runOnModule (Module & M);
};


//
// Pass: MergeFunctionRets
//
// Description:
//  This pass modifies a function so that each function only have one unified exit basic block
//
class MergeFunctionRets : public ModulePass
{
private:
    // Private methods

    // Private variables

public:
    static char ID;
    MergeFunctionRets() : ModulePass(ID) {}
    llvm::StringRef getPassName() const
    {
        return "unify function exit into one dummy exit basic block";
    }
    virtual bool runOnModule (Module & M)
    {
        UnifyFunctionExit(M);
        return true;
    }
    inline void UnifyFunctionExit(Module& module)
    {
        for (Module::const_iterator iter = module.begin(), eiter = module.end();
                iter != eiter; ++iter)
        {
            const Function& fun = *iter;
            if(fun.isDeclaration())
                continue;
            getUnifyExit(fun)->runOnFunction(const_cast<Function&>(fun));
        }
    }
    /// Get Unified Exit basic block node
    inline UnifyFunctionExitNodes* getUnifyExit(const Function& fn)
    {
        assert(!fn.isDeclaration() && "external function does not have DF");
        return &getAnalysis<UnifyFunctionExitNodes>(const_cast<Function&>(fn));
    }
};

} // End namespace SVF

#endif
