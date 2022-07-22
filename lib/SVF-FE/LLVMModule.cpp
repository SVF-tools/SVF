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
#include "SVF-FE/BasicTypes.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/BreakConstantExpr.h"

using namespace std;
using namespace SVF;

/*
  svf.main() is used to model the real entry point of a C++ program, which
  initializes all global C++ objects and then call main().

  LLVM may generate two global arrays @llvm.global_ctors and @llvm.global_dtors
  that contain constructor and destructor functions for global variables. They
  are not called explicitly, so we have to add them in the svf.main function.
  The order to call these constructor and desctructor functions are also
  specified in the global arrays.
  
  Related part in LLVM language reference:
  https://llvm.org/docs/LangRef.html#the-llvm-global-ctors-global-variable

  For example, given a "int main(int argc, char * argv[])", the corresponding
  svf.main will be generated as follows:
    define void @svf.main(i32, i8**, i8**) {
      entry:
        call void @ctor1()
        call void @ctor2()
        %3 = call i32 @main(i32 %0, i8** %1)
        call void @dtor1()
        call void @dtor2()
        ret void
    }
*/

#define SVF_MAIN_FUNC_NAME           "svf.main"
#define SVF_GLOBAL_CTORS             "llvm.global_ctors"
#define SVF_GLOBAL_DTORS             "llvm.global_dtors"

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

    loadModules(moduleNameVec);

    if(!moduleNameVec.empty())
        svfModule = new SVFModule(*moduleNameVec.begin());
    else
        svfModule = new SVFModule();

    build();

    return svfModule;
}

void LLVMModuleSet::preProcessBCs(std::vector<std::string> &moduleNameVec)
{
    loadModules(moduleNameVec);
    prePassSchedule();

    std::string preProcessSuffix = ".pre.bc";
    // Get the existing module names, remove old extention, add preProcessSuffix
    for (u32_t i = 0; i < moduleNameVec.size(); i++)
    {
        u32_t lastIndex = moduleNameVec[i].find_last_of(".");
        std::string rawName = moduleNameVec[i].substr(0, lastIndex);
        moduleNameVec[i] = (rawName + preProcessSuffix);
    }

    dumpModulesToFile(preProcessSuffix);
    preProcessed = true;

    releaseLLVMModuleSet();
}



void LLVMModuleSet::build()
{
    initialize();
    buildFunToFunMap();
    buildGlobalDefToRepMap();
    if(preProcessed==false)
        prePassSchedule();
}

/*!
 * Invoke llvm passes to modify module
 */
void LLVMModuleSet::prePassSchedule()
{
    /// BreakConstantGEPs Pass
    std::unique_ptr<BreakConstantGEPs> p1 = std::make_unique<BreakConstantGEPs>();
    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i)
    {
        Module *module = LLVMModuleSet::getLLVMModuleSet()->getModule(i);
        p1->runOnModule(*module);
    }

    /// MergeFunctionRets Pass
    std::unique_ptr<UnifyFunctionExitNodes> p2 =
        std::make_unique<UnifyFunctionExitNodes>();
    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i)
    {
        Module *module = LLVMModuleSet::getLLVMModuleSet()->getModule(i);
        for (auto F = module->begin(), E = module->end(); F != E; ++F)
        {
            Function &fun = *F;
            if (fun.isDeclaration())
                continue;
            p2->runOnFunction(fun);
        }
    }
}


void LLVMModuleSet::loadModules(const std::vector<std::string> &moduleNameVec)
{

    // We read SVFIR from LLVM IR
    if(Options::Graphtxt.getValue().empty())
    {
        if(moduleNameVec.empty())
        {
            SVFUtil::outs() << "no LLVM bc file is found!\n";
            exit(0);
        }
        //assert(!moduleNameVec.empty() && "no LLVM bc file is found!");
    }
    // We read SVFIR from a user-defined txt instead of parsing SVFIR from LLVM IR
    else
        SVFModule::setPagFromTXT(Options::Graphtxt.getValue());

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

    for (const std::string& moduleName : moduleNameVec)
    {
        SMDiagnostic Err;
        std::unique_ptr<Module> mod = parseIRFile(moduleName, Err, *cxts);
        if (mod == nullptr)
        {
            SVFUtil::errs() << "load module: " << moduleName << "failed!!\n\n";
            Err.print("SVFModuleLoader", llvm::errs());
            abort();
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

std::vector<const Function *> LLVMModuleSet::getLLVMGlobalFunctions(
    const GlobalVariable *global)
{
    // This function is used to extract constructor and destructor functions
    // sorted by their priority from @llvm.global_ctors or @llvm.global_dtors.
    // For example, given following @llvm.global_ctors, the returning sorted
    // function list should be [ctor3, ctor1, ctor2].
    // ------------------------------------------------------------------
    //    ; Each struct in the array is {priority, function, associated data}
    //
    //    @llvm.global_ctors = appending global [2 x { i32, void ()*, i8* }]
    //    [{ i32, void ()*, i8* } { i32 1234, void ()* @ctor1.cpp, i8* null },
    //    { i32, void ()*, i8* } { i32 2345, void ()* @ctor2.cpp, i8* null },
    //    { i32, void ()*, i8* } { i32 345, void ()* @ctor3.cpp, i8* null }]
    // ------------------------------------------------------------------
    // TODO: According to LLVM language reference, if the third field is
    // non-null, and points to a global variable or function, the initializer
    // function will only run if the associated data from the current module is
    // not discarded. However the associated data is currently ignored.


    // This class is used for the priority queue that sorts the functions by
    // their priority. Each object of this class stands for an item in the
    // function array.
    class LLVMGlobalFunction {
    public:
        u32_t priority;
        const Function *func;
        LLVMGlobalFunction(){};
        LLVMGlobalFunction(u32_t _priority, const Function *_func)
              : priority(_priority), func(_func){};
        bool operator>(const LLVMGlobalFunction &other) const
        {
            if (priority != other.priority) {
                return priority > other.priority;
            } else {
                return func > other.func;
            }
        }
    };

    std::priority_queue<LLVMGlobalFunction, std::vector<LLVMGlobalFunction>,
                        greater<LLVMGlobalFunction>>
            queue;
    std::vector<const Function *> result;

    // The @llvm.global_ctors/dtors global variable is an array of struct. Each
    // struct has three fields: {i32 priority, void ()* @ctor/dtor, i8* @data}.
    // First get the array here.
    if(const ConstantArray *globalFuncArray =
            SVFUtil::dyn_cast<ConstantArray>(global->getInitializer())){
        // Get each struct in the array.
        for (unsigned int i = 0; i < globalFuncArray->getNumOperands(); ++i) {
            if (
            const ConstantStruct *globalFuncItem =
                    SVFUtil::dyn_cast<ConstantStruct>(
                            globalFuncArray->getOperand(i))){

                // Extract priority and function from the struct
                const ConstantInt *priority = SVFUtil::dyn_cast<ConstantInt>(
                        globalFuncItem->getOperand(0));
                const Function *func = SVFUtil::dyn_cast<Function>(
                        globalFuncItem->getOperand(1));

                if (priority && func) {
                    queue.push(LLVMGlobalFunction(priority
                                                          ->getZExtValue(),
                                                  func));
                }
            }
        }
    }

    // Generate a sorted vector of functions from the priority queue.
    while (!queue.empty()) {
        result.push_back(queue.top().func);
        queue.pop();
    }
    return result;
}

void LLVMModuleSet::addSVFMain()
{
    std::vector<const Function *> ctor_funcs;
    std::vector<const Function *> dtor_funcs;
    Function * orgMain = 0;
    Module* mainMod = nullptr;

    for (Module &mod : modules) {
        // Collect ctor and dtor functions
        for (Module::global_iterator it = mod.global_begin(),
                                     eit = mod.global_end();
             it != eit; ++it) {
            const GlobalVariable *global = &*it;

            if (global->getName().equals(SVF_GLOBAL_CTORS) &&
                global->hasInitializer()) {
                ctor_funcs = getLLVMGlobalFunctions(global);
            } else if (global->getName().equals(SVF_GLOBAL_DTORS) &&
                       global->hasInitializer()) {
                dtor_funcs = getLLVMGlobalFunctions(global);
            }
        }

        // Find main function
        for (auto &func : mod) {
            if (func.getName().equals(SVF_MAIN_FUNC_NAME))
                assert(false && SVF_MAIN_FUNC_NAME " already defined");
            if(func.getName().equals("main"))
            {
                orgMain = &func;
                mainMod = &mod;
            }
        }
    }

    // Only create svf.main when the original main function is found, and also
    // there are global contructor or destructor functions.
    if (orgMain && getModuleNum() > 0 &&
        (ctor_funcs.size() > 0 || dtor_funcs.size() > 0)) {
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
        // emit "call void @ctor()". ctor_funcs is sorted so the functions are
        // emitted in the order of priority
        for(auto & ctor: ctor_funcs) {
            auto target = M.getOrInsertFunction(
                              ctor->getName(),
                              Type::getVoidTy(M.getContext())
                          );
            Builder.CreateCall(target);
        }
        // main() should be called after all ctor functions and before dtor
        // functions.
        Function::arg_iterator arg_it = svfmain->arg_begin();
        Value * args[] = {arg_it, arg_it + 1, arg_it + 2 };
        size_t cnt = orgMain->arg_size();
        assert(cnt <= 3 && "Too many arguments for main()");
        Builder.CreateCall(orgMain, llvm::ArrayRef<Value*>(args,args + cnt));
        // emit "call void @dtor()". dtor_funcs is sorted so the functions are
        // emitted in the order of priority
        for (auto &dtor : dtor_funcs) {
            auto target = M.getOrInsertFunction(
                              dtor->getName(),
                              Type::getVoidTy(M.getContext())
                          );
            Builder.CreateCall(target);
        }
        // return;
        Builder.CreateRetVoid();
    }
}


void LLVMModuleSet::buildFunToFunMap()
{
    Set<Function*> funDecls, funDefs;
    OrderedSet<string> declNames, defNames, intersectNames;
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
    OrderedSet<string>::iterator declIter, defIter;
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
        raw_fd_ostream OS(OutputFilename.c_str(), EC, llvm::sys::fs::OF_None);

#if (LLVM_VERSION_MAJOR >= 7)
        WriteBitcodeToFile(mod, OS);
#else
        WriteBitcodeToFile(&mod, OS);
#endif

        OS.flush();
    }
}
