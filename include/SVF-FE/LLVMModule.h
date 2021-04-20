//===- LLVMModule.h -- LLVM Module class-----------------------------------------//
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
 * LLVMModule.h
 *
 *  Created on: 23 Mar.2020
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVF_FE_LLVMMODULE_H_
#define INCLUDE_SVF_FE_LLVMMODULE_H_

#include "SVF-FE/CPPUtil.h"
#include "Util/BasicTypes.h"
#include "Util/SVFModule.h"

namespace SVF
{

class LLVMModuleSet
{
public:

    typedef std::vector<const SVFFunction*> FunctionSetType;
    typedef Map<const SVFFunction*, const SVFFunction*> FunDeclToDefMapTy;
    typedef Map<const SVFFunction*, FunctionSetType> FunDefToDeclsMapTy;
    typedef Map<const GlobalVariable*, GlobalVariable*> GlobalDefToRepMapTy;

private:
    static LLVMModuleSet *llvmModuleSet;
    SVFModule* svfModule;
    std::unique_ptr<LLVMContext> cxts;
    std::vector<std::unique_ptr<Module>> owned_modules;
    std::vector<std::reference_wrapper<Module>> modules;

    /// Function declaration to function definition map
    FunDeclToDefMapTy FunDeclToDefMap;
    /// Function definition to function declaration map
    FunDefToDeclsMapTy FunDefToDeclsMap;
    /// Global definition to a rep definition map
    GlobalDefToRepMapTy GlobalDefToRepMap;

    /// Constructor
    LLVMModuleSet(): svfModule(nullptr), cxts(nullptr) {}

    void build();

public:
    static inline LLVMModuleSet *getLLVMModuleSet()
    {
        if (llvmModuleSet == nullptr)
            llvmModuleSet = new LLVMModuleSet();
        return llvmModuleSet;
    }

    static void releaseLLVMModuleSet()
    {
        if (llvmModuleSet)
            delete llvmModuleSet;
        llvmModuleSet = nullptr;
    }

    SVFModule* buildSVFModule(Module &mod);
    SVFModule* buildSVFModule(const std::vector<std::string> &moduleNameVec);

	inline SVFModule* getSVFModule() {
		assert(svfModule && "svfModule has not been built yet!");
		return svfModule;
	}

    u32_t getModuleNum() const
    {
        return modules.size();
    }

    Module *getModule(u32_t idx) const
    {
        return &getModuleRef(idx);
    }

    Module &getModuleRef(u32_t idx) const
    {
        assert(idx < getModuleNum() && "Out of range.");
        return modules[idx];
    }

    // Dump modules to files
    void dumpModulesToFile(const std::string suffix);

    const SVFFunction *getSVFFunction(const Function *fun) const
    {
        return svfModule->getSVFFunction(fun);
    }

    /// Fun decl --> def
    bool hasDefinition(const Function *fun) const
    {
        return hasDefinition(svfModule->getSVFFunction(fun));
    }

    bool hasDefinition(const SVFFunction *fun) const
    {
        assert(fun->isDeclaration() && "not a function declaration?");
        FunDeclToDefMapTy::const_iterator it = FunDeclToDefMap.find(fun);
        return it != FunDeclToDefMap.end();
    }

    const SVFFunction *getDefinition(const Function *fun) const
    {
        return getDefinition(svfModule->getSVFFunction(fun));
    }

    const SVFFunction *getDefinition(const SVFFunction *fun) const
    {
        assert(fun->isDeclaration() && "not a function declaration?");
        FunDeclToDefMapTy::const_iterator it = FunDeclToDefMap.find(fun);
        assert(it != FunDeclToDefMap.end() && "has no definition?");
        return it->second;
    }

    /// Fun def --> decl
    bool hasDeclaration(const Function *fun) const
    {
        return hasDeclaration(svfModule->getSVFFunction(fun));
    }

    bool hasDeclaration(const SVFFunction *fun) const
    {
    	if(fun->isDeclaration() && !hasDefinition(fun))
    		return false;

    	const SVFFunction* funDef = fun;
        if(fun->isDeclaration() && hasDefinition(fun))
        	funDef = getDefinition(fun);

    	FunDefToDeclsMapTy::const_iterator it = FunDefToDeclsMap.find(funDef);
    	return it != FunDefToDeclsMap.end();
    }

    const FunctionSetType& getDeclaration(const Function *fun) const
    {
        return getDeclaration(svfModule->getSVFFunction(fun));
    }

    const FunctionSetType& getDeclaration(const SVFFunction *fun) const
    {
    	const SVFFunction* funDef = fun;
        if(fun->isDeclaration() && hasDefinition(fun))
        	funDef = getDefinition(fun);

        FunDefToDeclsMapTy::const_iterator it = FunDefToDeclsMap.find(funDef);
        assert(it != FunDefToDeclsMap.end() && "does not have a function definition (body)?");
        return it->second;
    }

    /// Global to rep
    bool hasGlobalRep(const GlobalVariable *val) const
    {
        GlobalDefToRepMapTy::const_iterator it = GlobalDefToRepMap.find(val);
        return it != GlobalDefToRepMap.end();
    }

    GlobalVariable *getGlobalRep(const GlobalVariable *val) const
    {
        GlobalDefToRepMapTy::const_iterator it = GlobalDefToRepMap.find(val);
        assert(it != GlobalDefToRepMap.end() && "has no rep?");
        return it->second;
    }


    Module *getMainLLVMModule() const
    {
        return getModule(0);
    }

    LLVMContext& getContext() const
    {
        assert(!empty() && "empty LLVM module!!");
        return getMainLLVMModule()->getContext();
    }

    bool empty() const
    {
        return getModuleNum() == 0;
    }

    /// Returns true if all LLVM modules are compiled with ctir.
    bool allCTir(void) const
    {
        // Iterate over all modules. If a single module does not have the correct ctir module flag,
        // short-circuit and return false.
        for (u32_t i = 0; i < getModuleNum(); ++i)
        {
            llvm::Metadata *ctirModuleFlag = llvmModuleSet->getModule(i)->getModuleFlag(cppUtil::ctir::derefMDName);
            if (ctirModuleFlag == nullptr)
            {
                return false;
            }

            llvm::ConstantAsMetadata *flagConstMetadata = SVFUtil::dyn_cast<llvm::ConstantAsMetadata>(ctirModuleFlag);
            ConstantInt *flagConstInt = SVFUtil::dyn_cast<ConstantInt>(flagConstMetadata->getValue());
            if (flagConstInt->getZExtValue() != cppUtil::ctir::moduleFlagValue)
            {
                return false;
            }
        }

        return true;
    }

private:
    void loadModules(const std::vector<std::string> &moduleNameVec);
    void addSVFMain();
    void initialize();
    void buildFunToFunMap();
    void buildGlobalDefToRepMap();
};

} // End namespace SVF

#endif /* INCLUDE_SVF_FE_LLVMMODULE_H_ */
