//===----- SVFModule.cpp  Base class of pointer analyses ---------------------//
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
 * SVFModule.cpp
 *
 *  Created on: Aug 4, 2017
 *      Author: Xiaokang Fan
 */

#include "Util/Options.h"
#include <queue>
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/SymbolTableInfo.h"

using namespace std;
using namespace SVF;

/*
  svf.main() is used to model the real entry point of a C++ program,
  which initializes all global C++ objects and then call main().
  For example, given a "int main(int argc, char * argv[])", the corresponding
  svf.main will be generated as follows:
    define void @svf.main(i32, i8**, i8**) {
      entry:
        call void @_GLOBAL__sub_I_cast.cpp()
        call void @_GLOBAL__sub_I_1.cpp()
        call void @_GLOBAL__sub_I_2.cpp()
        %3 = call i32 @main(i32 %0, i8** %1)
        ret void
    }
 */
#define SVF_MAIN_FUNC_NAME           "svf.main"
#define SVF_GLOBAL_SUB_I_XXX          "_GLOBAL__sub_I_"

LLVMModuleSet *LLVMModuleSet::llvmModuleSet = nullptr;
std::string SVFModule::pagReadFromTxt = "";

SVFModule* LLVMModuleSet::buildSVFModule(Module &mod)
{
    svfModule = new SVFModule(mod.getModuleIdentifier());
    modules.emplace_back(mod);

    build();

    return svfModule;
}

SVFModule* LLVMModuleSet::buildSVFModule(const std::vector<std::string> &moduleNameVec)
{
    assert(llvmModuleSet && "LLVM Module set needs to be created!");

    // We read PAG from LLVM IR
    if(Options::Graphtxt.getValue().empty())
    {
        if(moduleNameVec.empty())
        {
            SVFUtil::outs() << "no LLVM bc file is found!\n";
            exit(0);
        }
        //assert(!moduleNameVec.empty() && "no LLVM bc file is found!");
    }
    // We read PAG from a user-defined txt instead of parsing PAG from LLVM IR
    else
        SVFModule::setPagFromTXT(Options::Graphtxt.getValue());

    if(!moduleNameVec.empty())
        svfModule = new SVFModule(*moduleNameVec.begin());
    else
        svfModule = new SVFModule();

    loadModules(moduleNameVec);
    build();

    return svfModule;
}

void LLVMModuleSet::build()
{
    initialize();
    buildFunToFunMap();
    buildGlobalDefToRepMap();

    if (!SVFModule::pagReadFromTXT()) {
        /// building symbol table
        DBOUT(DGENERAL,SVFUtil::outs() << SVFUtil::pasMsg("Building Symbol table ...\n"));
        SymbolTableInfo *symInfo = SymbolTableInfo::SymbolInfo();
        symInfo->buildMemModel(svfModule);
    }

}

void LLVMModuleSet::loadModules(const std::vector<std::string> &moduleNameVec)
{
    //
    // To avoid the following type bugs (t1 != t3) when parsing multiple modules,
    // We should use only one LLVMContext object for multiple modules in the same thread.
    // No such problem if only one module is processed by SVF.
    // ------------------------------------------------------------------
    //    LLVMContext ctxa,ctxb;
    //    IntegerType * t1 = IntegerType::get(ctxa,32);
    //    IntegerType * t2 = IntegerType::get(ctxa,32);
    //    assert(t1 == t2);
    //    IntegerType * t3 = IntegerType::get(ctxb,32);
    //    IntegerType * t4 = IntegerType::get(ctxb,32);
    //    assert(t3 == t4);
    //    assert(t1 != t3);
    // ------------------------------------------------------------------
    //
    cxts = std::make_unique<LLVMContext>();

    for (const std::string& moduleName : moduleNameVec) {
        SMDiagnostic Err;
        std::unique_ptr<Module> mod = parseIRFile(moduleName, Err, *cxts);
        if (mod == nullptr)
        {
            SVFUtil::errs() << "load module: " << moduleName << "failed!!\n\n";
            Err.print("SVFModuleLoader", SVFUtil::errs());
            continue;
        }
        modules.emplace_back(*mod);
        owned_modules.emplace_back(std::move(mod));
    }
}

void LLVMModuleSet::initialize()
{
    if (Options::SVFMain)
        addSVFMain();

    for (Module& mod : modules)
    {
        /// Function
        for (Module::iterator it = mod.begin(), eit = mod.end();
                it != eit; ++it)
        {
            Function *func = &*it;
            svfModule->addFunctionSet(func);
        }

        /// GlobalVariable
        for (Module::global_iterator it = mod.global_begin(),
                eit = mod.global_end(); it != eit; ++it)
        {
            GlobalVariable *global = &*it;
            svfModule->addGlobalSet(global);
        }

        /// GlobalAlias
        for (Module::alias_iterator it = mod.alias_begin(),
                eit = mod.alias_end(); it != eit; ++it)
        {
            GlobalAlias *alias = &*it;
            svfModule->addAliasSet(alias);
        }
    }
}

void LLVMModuleSet::addSVFMain()
{
    std::vector<Function *> init_funcs;
    Function * orgMain = 0;
    Module* mainMod = nullptr;
    for (Module& mod : modules)
    {
        for (auto &func: mod)
        {
            if(func.getName().startswith(SVF_GLOBAL_SUB_I_XXX))
                init_funcs.push_back(&func);
            if(func.getName().equals(SVF_MAIN_FUNC_NAME))
                assert(false && SVF_MAIN_FUNC_NAME " already defined");
            if(func.getName().equals("main"))
            {
                orgMain = &func;
                mainMod = &mod;
            }
        }
    }
    if(orgMain && getModuleNum() > 0 && init_funcs.size() > 0)
    {
        assert(mainMod && "Module with main function not found.");
        Module & M = *mainMod;
        // char **
        Type * i8ptr2 = PointerType::getInt8PtrTy(M.getContext())->getPointerTo();
        Type * i32 = IntegerType::getInt32Ty(M.getContext());
        // define void @svf.main(i32, i8**, i8**)
#if (LLVM_VERSION_MAJOR >= 9)
        FunctionCallee svfmainFn = M.getOrInsertFunction(
                                       SVF_MAIN_FUNC_NAME,
                                       Type::getVoidTy(M.getContext()),
                                       i32,i8ptr2,i8ptr2
                                   );
        Function *svfmain = SVFUtil::dyn_cast<Function>(svfmainFn.getCallee());
#else
        Function *svfmain = SVFUtil::dyn_cast<Function>(M.getOrInsertFunction(
                                SVF_MAIN_FUNC_NAME,
                                Type::getVoidTy(M.getContext()),
                                i32,i8ptr2,i8ptr2
                            ));
#endif
        svfmain->setCallingConv(llvm::CallingConv::C);
        BasicBlock* block = BasicBlock::Create(M.getContext(), "entry", svfmain);
        IRBuilder Builder(block);
        // emit "call void @_GLOBAL__sub_I_XXX()"
        for(auto & init: init_funcs)
        {
            auto target = M.getOrInsertFunction(
                              init->getName(),
                              Type::getVoidTy(M.getContext())
                          );
            Builder.CreateCall(target);
        }
        // main() should be called after all _GLOBAL__sub_I_XXX functions.
        Function::arg_iterator arg_it = svfmain->arg_begin();
        Value * args[] = {arg_it, arg_it + 1, arg_it + 2 };
        size_t cnt = orgMain->arg_size();
        assert(cnt <= 3 && "Too many arguments for main()");
        Builder.CreateCall(orgMain, llvm::ArrayRef<Value*>(args,args + cnt));
        // return;
        Builder.CreateRetVoid();
    }
}


void LLVMModuleSet::buildFunToFunMap()
{
    Set<Function*> funDecls, funDefs;
    Set<string> declNames, defNames, intersectNames;
    typedef Map<string, Function*> NameToFunDefMapTy;
    typedef Map<string, Set<Function*>> NameToFunDeclsMapTy;

    for (SVFModule::LLVMFunctionSetType::iterator it = svfModule->llvmFunBegin(),
            eit = svfModule->llvmFunEnd(); it != eit; ++it)
    {
        Function *fun = *it;
        if (fun->isDeclaration())
        {
            funDecls.insert(fun);
            declNames.insert(fun->getName().str());
        }
        else
        {
            funDefs.insert(fun);
            defNames.insert(fun->getName().str());
        }
    }
    // Find the intersectNames
    Set<string>::iterator declIter, defIter;
    declIter = declNames.begin();
    defIter = defNames.begin();
    while (declIter != declNames.end() && defIter != defNames.end())
    {
        if (*declIter < *defIter)
        {
            declIter++;
        }
        else
        {
            if (!(*defIter < *declIter))
            {
                intersectNames.insert(*declIter);
                declIter++;
            }
            defIter++;
        }
    }

    ///// name to def map
    NameToFunDefMapTy nameToFunDefMap;
    for (Set<Function*>::iterator it = funDefs.begin(),
            eit = funDefs.end(); it != eit; ++it)
    {
        Function *fdef = *it;
        string funName = fdef->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        nameToFunDefMap[funName] = fdef;
    }

    ///// name to decls map
    NameToFunDeclsMapTy nameToFunDeclsMap;
    for (Set<Function*>::iterator it = funDecls.begin(),
            eit = funDecls.end(); it != eit; ++it)
    {
        Function *fdecl = *it;
        string funName = fdecl->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDeclsMapTy::iterator mit = nameToFunDeclsMap.find(funName);
        if (mit == nameToFunDeclsMap.end())
        {
            Set<Function*> decls;
            decls.insert(fdecl);
            nameToFunDeclsMap[funName] = decls;
        }
        else
        {
            Set<Function*> &decls = mit->second;
            decls.insert(fdecl);
        }
    }

    /// Fun decl --> def
    for (Set<Function*>::iterator it = funDecls.begin(),
            eit = funDecls.end(); it != eit; ++it)
    {
        const Function *fdecl = *it;
        string funName = fdecl->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDefMapTy::iterator mit = nameToFunDefMap.find(funName);
        if (mit == nameToFunDefMap.end())
            continue;
        FunDeclToDefMap[svfModule->getSVFFunction(fdecl)] = svfModule->getSVFFunction(mit->second);
    }

    /// Fun def --> decls
    for (Set<Function*>::iterator it = funDefs.begin(),
            eit = funDefs.end(); it != eit; ++it)
    {
        const Function *fdef = *it;
        string funName = fdef->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDeclsMapTy::iterator mit = nameToFunDeclsMap.find(funName);
        if (mit == nameToFunDeclsMap.end())
            continue;
        std::vector<const SVFFunction*>& decls = FunDefToDeclsMap[svfModule->getSVFFunction(fdef)];
        for (Set<Function*>::iterator sit = mit->second.begin(),
                seit = mit->second.end(); sit != seit; ++sit)
        {
            decls.push_back(svfModule->getSVFFunction(*sit));
        }
    }
}

void LLVMModuleSet::buildGlobalDefToRepMap()
{
    typedef Map<string, Set<GlobalVariable*>> NameToGlobalsMapTy;
    NameToGlobalsMapTy nameToGlobalsMap;
    for (SVFModule::global_iterator it = svfModule->global_begin(),
            eit = svfModule->global_end(); it != eit; ++it)
    {
        GlobalVariable *global = *it;
        if (global->hasPrivateLinkage())
            continue;
        string name = global->getName().str();
        NameToGlobalsMapTy::iterator mit = nameToGlobalsMap.find(name);
        if (mit == nameToGlobalsMap.end())
        {
            Set<GlobalVariable*> globals;
            globals.insert(global);
            nameToGlobalsMap[name] = globals;
        }
        else
        {
            Set<GlobalVariable*> &globals = mit->second;
            globals.insert(global);
        }
    }

    for (NameToGlobalsMapTy::iterator it = nameToGlobalsMap.begin(),
            eit = nameToGlobalsMap.end(); it != eit; ++it)
    {
        Set<GlobalVariable*> &globals = it->second;
        GlobalVariable *rep = *(globals.begin());
        Set<GlobalVariable*>::iterator repit = globals.begin();
        while (repit != globals.end())
        {
            GlobalVariable *cur = *repit;
            if (cur->hasInitializer())
            {
                rep = cur;
                break;
            }
            repit++;
        }
        for (Set<GlobalVariable*>::iterator sit = globals.begin(),
                seit = globals.end(); sit != seit; ++sit)
        {
            GlobalVariable *cur = *sit;
            GlobalDefToRepMap[cur] = rep;
        }
    }
}

// Dump modules to files
void LLVMModuleSet::dumpModulesToFile(const std::string suffix)
{
    for (Module& mod : modules)
    {
        std::string moduleName = mod.getName().str();
        std::string OutputFilename;
        std::size_t pos = moduleName.rfind('.');
        if (pos != std::string::npos)
            OutputFilename = moduleName.substr(0, pos) + suffix;
        else
            OutputFilename = moduleName + suffix;

        std::error_code EC;
        raw_fd_ostream OS(OutputFilename.c_str(), EC, llvm::sys::fs::F_None);

#if (LLVM_VERSION_MAJOR >= 7)
        WriteBitcodeToFile(mod, OS);
#else
        WriteBitcodeToFile(&mod, OS);
#endif

        OS.flush();
    }
}
