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

#include <queue>
#include <llvm/Support/CommandLine.h>
#include "Util/SVFModule.h"
#include <llvm/IR/LLVMContext.h>		// for llvm LLVMContext
#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriter.h>		// for WriteBitcodeToFile
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Support/FileSystem.h>	// for sys::fs::F_None

#include <llvm/IR/IRBuilder.h>

using namespace std;
using namespace llvm;

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

static cl::opt<std::string> Graphtxt("graphtxt", cl::value_desc("filename"),
                                     cl::desc("graph txt file to build PAG"));
static cl::opt<bool> SVFMain("svfmain", cl::init(false), cl::desc("add svf.main()"));

LLVMModuleSet *SVFModule::llvmModuleSet = NULL;
std::string SVFModule::pagReadFromTxt = "";

LLVMModuleSet::LLVMModuleSet(llvm::Module *mod) {
    moduleNum = 1;
    cxts = &(mod->getContext());
    modules = new unique_ptr<Module>[moduleNum];
    modules[0] = std::unique_ptr<llvm::Module>(mod);

    initialize();
    buildFunToFunMap();
    buildGlobalDefToRepMap();
}

LLVMModuleSet::LLVMModuleSet(llvm::Module &mod) {
    moduleNum = 1;
    cxts = &(mod.getContext());
    modules = new unique_ptr<Module>[moduleNum];
    modules[0] = std::unique_ptr<llvm::Module>(&mod);

    initialize();
    buildFunToFunMap();
    buildGlobalDefToRepMap();
}

LLVMModuleSet::LLVMModuleSet(const vector<string> &moduleNameVec) {
	// We read PAG from LLVM IR
	if(Graphtxt.getValue().empty())
		assert(!moduleNameVec.empty() && "no module is found from LLVM bc file!");
	// We read PAG from a user-defined txt instead of parsing PAG from LLVM IR
	else
		SVFModule::setPagFromTXT(Graphtxt.getValue());
    build(moduleNameVec);
}

void LLVMModuleSet::build(const vector<string> &moduleNameVec) {
    loadModules(moduleNameVec);
    initialize();
    buildFunToFunMap();
    buildGlobalDefToRepMap();
}

void LLVMModuleSet::loadModules(const std::vector<std::string> &moduleNameVec) {
    moduleNum = moduleNameVec.size();
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
    cxts = new LLVMContext[1];
    modules = new unique_ptr<Module>[moduleNum];

    u32_t i = 0;
    for (vector<string>::const_iterator it = moduleNameVec.begin(),
            eit = moduleNameVec.end(); it != eit; ++it, ++i) {
        const string moduleName = *it;
        SMDiagnostic Err;
        modules[i] = parseIRFile(moduleName, Err, cxts[0]);
        if (!modules[i]) {
            errs() << "load module: " << moduleName << "failed\n";
            continue;
        }
    }
}

void LLVMModuleSet::initialize() {
   if (SVFMain)
	addSVFMain();

    for (u32_t i = 0; i < moduleNum; ++i) {
        Module *mod = modules[i].get();

        /// Function
        for (Module::iterator it = mod->begin(), eit = mod->end();
                it != eit; ++it) {
            Function *func = &*it;
            FunctionSet.push_back(func);
        }

        /// GlobalVariable
        for (Module::global_iterator it = mod->global_begin(),
                eit = mod->global_end(); it != eit; ++it) {
            GlobalVariable *global = &*it;
            GlobalSet.push_back(global);
        }

        /// GlobalAlias
        for (Module::alias_iterator it = mod->alias_begin(),
                eit = mod->alias_end(); it != eit; ++it) {
            GlobalAlias *alias = &*it;
            AliasSet.push_back(alias);
        }
    }
}

void LLVMModuleSet::addSVFMain(){
    std::vector<Function *> init_funcs;
    Function * orgMain = 0;
    u32_t k = 0;
    for (u32_t i = 0; i < moduleNum; ++i) {
        Module *mod = modules[i].get();
        for (auto &func: *mod) {
            if(func.getName().startswith(SVF_GLOBAL_SUB_I_XXX))
                init_funcs.push_back(&func);
            if(func.getName().equals(SVF_MAIN_FUNC_NAME))
                assert(false && SVF_MAIN_FUNC_NAME " already defined");
            if(func.getName().equals("main")){
                orgMain = &func;
                k = i;
            }
        }
    }
    if(orgMain && moduleNum > 0 && init_funcs.size() > 0){
        Module & M = *(modules[k].get());
        // char **
        Type * i8ptr2 = PointerType::getInt8PtrTy(M.getContext())->getPointerTo();
        Type * i32 = IntegerType::getInt32Ty(M.getContext());
        // define void @svf.main(i32, i8**, i8**)
        Function *svfmain = (Function*)M.getOrInsertFunction(
            SVF_MAIN_FUNC_NAME,
            Type::getVoidTy(M.getContext()),
            i32,i8ptr2,i8ptr2
        );
        svfmain->setCallingConv(CallingConv::C);
        BasicBlock* block = BasicBlock::Create(M.getContext(), "entry", svfmain);
        IRBuilder<> Builder(block);
        // emit "call void @_GLOBAL__sub_I_XXX()"
        for(auto & init: init_funcs){
            Function *target = (Function*)M.getOrInsertFunction(
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
        Builder.CreateCall(orgMain, ArrayRef<Value*>(args,args + cnt));
        // return;
        Builder.CreateRetVoid();
    }
}


void LLVMModuleSet::buildFunToFunMap() {
    std::set<Function*> funDecls, funDefs;
    std::set<string> declNames, defNames, intersectNames;
    typedef std::map<string, Function*> NameToFunDefMapTy;
    typedef std::map<string, std::set<Function*>> NameToFunDeclsMapTy;

    for (FunctionSetType::iterator it = FunctionSet.begin(),
            eit = FunctionSet.end(); it != eit; ++it) {
        Function *fun = *it;
        if (fun->isDeclaration()) {
            funDecls.insert(fun);
            declNames.insert(fun->getName().str());
        } else {
            funDefs.insert(fun);
            defNames.insert(fun->getName().str());
        }
    }
    // Find the intersectNames
    std::set<string>::iterator declIter, defIter;
    declIter = declNames.begin();
    defIter = defNames.begin();
    while (declIter != declNames.end() && defIter != defNames.end()) {
        if (*declIter < *defIter) {
            declIter++;
        } else {
            if (!(*defIter < *declIter)) {
                intersectNames.insert(*declIter);
                declIter++;
            }
            defIter++;
        }
    }

    ///// name to def map
    NameToFunDefMapTy nameToFunDefMap;
    for (std::set<Function*>::iterator it = funDefs.begin(),
            eit = funDefs.end(); it != eit; ++it) {
        Function *fdef = *it;
        string funName = fdef->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        nameToFunDefMap[funName] = fdef;
    }

    ///// name to decls map
    NameToFunDeclsMapTy nameToFunDeclsMap;
    for (std::set<Function*>::iterator it = funDecls.begin(),
            eit = funDecls.end(); it != eit; ++it) {
        Function *fdecl = *it;
        string funName = fdecl->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDeclsMapTy::iterator mit = nameToFunDeclsMap.find(funName);
        if (mit == nameToFunDeclsMap.end()) {
            std::set<Function*> decls;
            decls.insert(fdecl);
            nameToFunDeclsMap[funName] = decls;
        } else {
            std::set<Function*> &decls = mit->second;
            decls.insert(fdecl);
        }
    }

    /// Fun decl --> def
    for (std::set<Function*>::iterator it = funDecls.begin(),
            eit = funDecls.end(); it != eit; ++it) {
        Function *fdecl = *it;
        string funName = fdecl->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDefMapTy::iterator mit = nameToFunDefMap.find(funName);
        if (mit == nameToFunDefMap.end())
            continue;
        FunDeclToDefMap[fdecl] = mit->second;
    }

    /// Fun def --> decls
    for (std::set<Function*>::iterator it = funDefs.begin(),
            eit = funDefs.end(); it != eit; ++it) {
        Function *fdef = *it;
        string funName = fdef->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDeclsMapTy::iterator mit = nameToFunDeclsMap.find(funName);
        if (mit == nameToFunDeclsMap.end())
            continue;
        FunctionSetType decls;
        for (std::set<Function*>::iterator sit = mit->second.begin(),
                seit = mit->second.end(); sit != seit; ++sit) {
            decls.push_back(*sit);
        }
        FunDefToDeclsMap[fdef] = decls;
    }
}

void LLVMModuleSet::buildGlobalDefToRepMap() {
    typedef std::map<string, std::set<GlobalVariable*>> NameToGlobalsMapTy;
    NameToGlobalsMapTy nameToGlobalsMap;
    for (GlobalSetType::iterator it = GlobalSet.begin(),
            eit = GlobalSet.end(); it != eit; ++it) {
        GlobalVariable *global = *it;
        if (global->hasPrivateLinkage())
            continue;
        string name = global->getName().str();
        NameToGlobalsMapTy::iterator mit = nameToGlobalsMap.find(name);
        if (mit == nameToGlobalsMap.end()) {
            std::set<GlobalVariable*> globals;
            globals.insert(global);
            nameToGlobalsMap[name] = globals;
        } else {
            std::set<GlobalVariable*> &globals = mit->second;
            globals.insert(global);
        }
    }

    for (NameToGlobalsMapTy::iterator it = nameToGlobalsMap.begin(),
            eit = nameToGlobalsMap.end(); it != eit; ++it) {
        std::set<GlobalVariable*> &globals = it->second;
        GlobalVariable *rep = *(globals.begin());
        std::set<GlobalVariable*>::iterator repit = globals.begin();
        while (repit != globals.end()) {
            GlobalVariable *cur = *repit;
            if (cur->hasInitializer()) {
                rep = cur;
                break;
            }
            repit++;
        }
        for (std::set<GlobalVariable*>::iterator sit = globals.begin(),
                seit = globals.end(); sit != seit; ++sit) {
            GlobalVariable *cur = *sit;
            GlobalDefToRepMap[cur] = rep;
        }
    }
}

// Dump modules to files
void LLVMModuleSet::dumpModulesToFile(const std::string suffix) {
    for (u32_t i = 0; i < moduleNum; ++i) {
        Module *mod = getModule(i);
        std::string moduleName = mod->getName().str();
        std::string OutputFilename;
        std::size_t pos = moduleName.rfind('.');
        if (pos != std::string::npos)
            OutputFilename = moduleName.substr(0, pos) + suffix;
        else
            OutputFilename = moduleName + suffix;

        std::error_code EC;
        llvm::raw_fd_ostream OS(OutputFilename.c_str(), EC, llvm::sys::fs::F_None);
        WriteBitcodeToFile(*mod, OS);
        OS.flush();
    }
}
