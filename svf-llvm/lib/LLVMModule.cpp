//===----- SVFModule.cpp  Base class of pointer analyses ---------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * LLVMModule.cpp
 *
 *  Created on: Aug 4, 2017
 *      Author: Yulei Sui
 *  Refactored on: Jan 25, 2024
 *      Author: Xiao Cheng, Yulei Sui
 */

#include <queue>
#include <algorithm>
#include "Util/SVFUtil.h"
#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/CppUtil.h"
#include "SVF-LLVM/BreakConstantExpr.h"
#include "SVF-LLVM/SymbolTableBuilder.h"
#include "MSSA/SVFGBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "SVF-LLVM/ObjTypeInference.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "SVF-LLVM/ICFGBuilder.h"
#include "Graphs/CallGraph.h"
#include "Util/CallGraphBuilder.h"

using namespace std;
using namespace SVF;

/*
  svf.main() is used to model the real entry point of a C++ program, which
  initializes all global C++ objects and then call main().
  LLVM may generate two global arrays @llvm.global_ctors and @llvm.global_dtors
  that contain constructor and destructor functions for global variables. They
  are not called explicitly, so we have to add them in the svf.main function.
  The order to call these constructor and destructor functions are also
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

LLVMModuleSet* LLVMModuleSet::llvmModuleSet = nullptr;
bool LLVMModuleSet::preProcessed = false;

LLVMModuleSet::LLVMModuleSet()
    : svfir(PAG::getPAG()), typeInference(new ObjTypeInference())
{
}

LLVMModuleSet::~LLVMModuleSet()
{

    delete typeInference;
    typeInference = nullptr;

}

ObjTypeInference* LLVMModuleSet::getTypeInference()
{
    return typeInference;
}

DominatorTree& LLVMModuleSet::getDomTree(const SVF::Function* fun)
{
    auto it = FunToDominatorTree.find(fun);
    if(it != FunToDominatorTree.end()) return it->second;
    DominatorTree& dt = FunToDominatorTree[fun];
    dt.recalculate(const_cast<Function&>(*fun));
    return dt;
}

void LLVMModuleSet::buildSVFModule(Module &mod)
{
    LLVMModuleSet* mset = getLLVMModuleSet();

    double startSVFModuleTime = SVFStat::getClk(true);
    PAG::getPAG()->setModuleIdentifier(mod.getModuleIdentifier());
    mset->modules.emplace_back(mod);    // Populates `modules`; can get context via `this->getContext()`
    mset->loadExtAPIModules();          // Uses context from module through `this->getContext()`
    mset->build();
    double endSVFModuleTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingLLVMModule = (endSVFModuleTime - startSVFModuleTime)/TIMEINTERVAL;

    mset->buildSymbolTable();
}

void LLVMModuleSet::buildSVFModule(const std::vector<std::string> &moduleNameVec)
{
    double startSVFModuleTime = SVFStat::getClk(true);

    LLVMModuleSet* mset = getLLVMModuleSet();

    mset->loadModules(moduleNameVec);   // Populates `modules`; can get context via `this->getContext()`
    mset->loadExtAPIModules();          // Uses context from first module through `this->getContext()`

    if (!moduleNameVec.empty())
    {
        PAG::getPAG()->setModuleIdentifier(moduleNameVec.front());
    }

    mset->build();

    double endSVFModuleTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingLLVMModule =
        (endSVFModuleTime - startSVFModuleTime) / TIMEINTERVAL;

    mset->buildSymbolTable();
}

void LLVMModuleSet::buildSymbolTable() const
{
    double startSymInfoTime = SVFStat::getClk(true);
    if (!SVFIR::pagReadFromTXT())
    {
        /// building symbol table
        DBOUT(DGENERAL, SVFUtil::outs() << SVFUtil::pasMsg("Building Symbol table ...\n"));
        SymbolTableBuilder builder(svfir);
        builder.buildMemModel();
    }
    double endSymInfoTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingSymbolTable =
        (endSymInfoTime - startSymInfoTime) / TIMEINTERVAL;
}

void LLVMModuleSet::build()
{
    if(preProcessed==false)
        prePassSchedule();

    buildFunToFunMap();
    buildGlobalDefToRepMap();

    if (Options::SVFMain())
        addSVFMain();

    createSVFDataStructure();

}

void LLVMModuleSet::createSVFDataStructure()
{
    SVFType::svfI8Ty = getSVFType(getTypeInference()->int8Type());
    SVFType::svfPtrTy = getSVFType(getTypeInference()->ptrType());
    // Functions need to be retrieved in the order of insertion
    // candidateDefs is the vector for all used defined functions
    // candidateDecls is the vector for all used declared functions
    std::vector<const Function*> candidateDefs, candidateDecls;

    for (Module& mod : modules)
    {
        /// Function
        for (Function& func : mod.functions())
        {
            if (func.isDeclaration())
            {
                candidateDecls.push_back(&func);
            }
            else
            {
                candidateDefs.push_back(&func);
            }
        }
    }

    for (const Function* func: candidateDefs)
    {
        addFunctionSet(func);
    }
    for (const Function* func: candidateDecls)
    {
        addFunctionSet(func);
    }

    // set function exit block
    for (const auto& func: funSet)
    {
        for (Function::const_iterator bit = func->begin(), ebit = func->end(); bit != ebit; ++bit)
        {
            const BasicBlock* bb = &*bit;
            /// set exit block: exit basic block must have no successors and have a return instruction
            if (succ_size(bb) == 0)
            {
                if (LLVMUtil::basicBlockHasRetInst(bb))
                {
                    assert((LLVMUtil::functionDoesNotRet(func) ||
                            SVFUtil::isa<ReturnInst>(bb->back())) &&
                           "last inst must be return inst");
                    setFunExitBB(func, bb);
                }
            }
        }
        // For no return functions, we set the last block as exit BB
        // This ensures that each function that has definition must have an exit BB
        if (func->size() != 0 && !getFunExitBB(func))
        {
            assert((LLVMUtil::functionDoesNotRet(func) ||
                    SVFUtil::isa<ReturnInst>(&func->back().back())) &&
                   "last inst must be return inst");
            setFunExitBB(func, &func->back());
        }
    }

    // Store annotations of functions in extapi.bc
    for (const auto& pair : ExtFun2Annotations)
    {
        const Function* fun = getFunction(pair.first);
        setExtFuncAnnotations(fun, pair.second);
    }

}

/*!
 * Invoke llvm passes to modify module
 */
void LLVMModuleSet::prePassSchedule()
{
    /// BreakConstantGEPs Pass
    std::unique_ptr<BreakConstantGEPs> p1 = std::make_unique<BreakConstantGEPs>();
    for (Module &M : getLLVMModules())
    {
        p1->runOnModule(M);
    }

    /// MergeFunctionRets Pass
    std::unique_ptr<UnifyFunctionExitNodes> p2 =
        std::make_unique<UnifyFunctionExitNodes>();
    for (Module &M : getLLVMModules())
    {
        for (auto F = M.begin(), E = M.end(); F != E; ++F)
        {
            Function &fun = *F;
            if (fun.isDeclaration())
                continue;
            p2->runOnFunction(fun);
        }
    }
}

void LLVMModuleSet::preProcessBCs(std::vector<std::string> &moduleNameVec)
{
    LLVMModuleSet* mset = getLLVMModuleSet();
    mset->loadModules(moduleNameVec);
    mset->loadExtAPIModules();
    mset->prePassSchedule();

    std::string preProcessSuffix = ".pre.bc";
    // Get the existing module names, remove old extension, add preProcessSuffix
    for (u32_t i = 0; i < moduleNameVec.size(); i++)
    {
        u32_t lastIndex = moduleNameVec[i].find_last_of(".");
        std::string rawName = moduleNameVec[i].substr(0, lastIndex);
        moduleNameVec[i] = (rawName + preProcessSuffix);
    }

    mset->dumpModulesToFile(preProcessSuffix);
    preProcessed = true;
    releaseLLVMModuleSet();
}

void LLVMModuleSet::loadModules(const std::vector<std::string> &moduleNameVec)
{

    // We read SVFIR from LLVM IR
    if(Options::Graphtxt().empty())
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
    {
        SVFIR::setPagFromTXT(Options::Graphtxt());
    }
    //
    // LLVMContext objects separate global LLVM settings (from which e.g. types are
    // derived); multiple LLVMContext objects can coexist and each context can "own"
    // multiple modules (modules can only have one context). Mixing contexts can lead
    // to unintended inequalities, such as the following:
    //
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
    // When loading bytecode files, SVF will use the same LLVMContext object for all
    // modules (i.e. the context owns all loaded modules). This applies to ExtAPI as
    // well, which *must* be loaded using the same LLVMContext object. Hence, when
    // loading modules from bitcode files, a new LLVMContext is created (using a
    // `std::unique_ptr<LLVMContext>` type to ensure automatic garbage collection).
    //
    // This garbage collection should be avoided when building an SVF module from an LLVM
    // module instance; see the comment(s) in `buildSVFModule` and `loadExtAPIModules()`

    owned_ctx = std::make_unique<LLVMContext>();
    for (const std::string& moduleName : moduleNameVec)
    {
        if (!LLVMUtil::isIRFile(moduleName))
        {
            SVFUtil::errs() << "not an IR file: " << moduleName << std::endl;
            abort();
        }

        SMDiagnostic Err;
        std::unique_ptr<Module> mod = parseIRFile(moduleName, Err, *owned_ctx);
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

void LLVMModuleSet::loadExtAPIModules()
{
    // This function loads the ExtAPI bitcode file as an LLVM module. Note that it is important that
    // the same LLVMContext object is used to load this bitcode file as is used by the other modules
    // being analysed.
    // When the modules are loaded from bitcode files (i.e. passing filenames to files containing
    // LLVM IR to `buildSVFModule({file1.bc, file2.bc, ...})) the context is created while loading
    // the modules in `loadModules()`, which populates this->modules and this->owned_modules.
    // If, however, an LLVM Module object is passed to `buildSVFModule` (e.g. from an LLVM pass),
    // the context should be retrieved from the module itself (note that the garbage collection from
    // `std::unique_ptr<LLVMContext> LLVMModuleSet::owned_ctx` should be avoided in this case). This
    // function populates only this->modules.
    // In both cases, fetching the context from the main LLVM module (through `getContext`) works
    assert(!empty() && "LLVMModuleSet contains no modules; cannot load ExtAPI module without LLVMContext!");

    // Load external API module (extapi.bc)
    if (!ExtAPI::getExtAPI()->getExtBcPath().empty())
    {
        std::string extModuleName = ExtAPI::getExtAPI()->getExtBcPath();
        if (!LLVMUtil::isIRFile(extModuleName))
        {
            SVFUtil::errs() << "not an external IR file: " << extModuleName << std::endl;
            abort();
        }
        SMDiagnostic Err;
        std::unique_ptr<Module> mod = parseIRFile(extModuleName, Err, getContext());
        if (mod == nullptr)
        {
            SVFUtil::errs() << "load external module: " << extModuleName << "failed!!\n\n";
            Err.print("SVFModuleLoader", llvm::errs());
            abort();
        }
        // The module of extapi.bc needs to be inserted before applications modules, like std::vector<std::reference_wrapper<Module>> modules{extapi_module, app_module}.
        // Otherwise, when overwriting the app function with SVF extern function, the corresponding SVFFunction of the extern function will not be found.
        modules.insert(modules.begin(), *mod);
        owned_modules.insert(owned_modules.begin(),std::move(mod));
    }
}

std::vector<const Function* > LLVMModuleSet::getLLVMGlobalFunctions(const GlobalVariable *global)
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
    class LLVMGlobalFunction
    {
    public:
        u32_t priority;
        const Function* func;
        LLVMGlobalFunction() {};
        LLVMGlobalFunction(u32_t _priority, const Function* _func)
            : priority(_priority), func(_func) {};
        bool operator>(const LLVMGlobalFunction &other) const
        {
            if (priority != other.priority)
            {
                return priority > other.priority;
            }
            else
            {
                return func > other.func;
            }
        }
    };

    std::priority_queue<LLVMGlobalFunction, std::vector<LLVMGlobalFunction>,
        greater<LLVMGlobalFunction>>
        queue;
    std::vector<const Function* > result;

    // The @llvm.global_ctors/dtors global variable is an array of struct. Each
    // struct has three fields: {i32 priority, void ()* @ctor/dtor, i8* @data}.
    // First get the array here.
    if(const ConstantArray *globalFuncArray =
                SVFUtil::dyn_cast<ConstantArray>(global->getInitializer()))
    {
        // Get each struct in the array.
        for (unsigned int i = 0; i < globalFuncArray->getNumOperands(); ++i)
        {
            if (
                const ConstantStruct *globalFuncItem =
                    SVFUtil::dyn_cast<ConstantStruct>(
                        globalFuncArray->getOperand(i)))
            {

                // Extract priority and function from the struct
                const ConstantInt* priority = SVFUtil::dyn_cast<ConstantInt>(
                                                  globalFuncItem->getOperand(0));
                const Function* func = SVFUtil::dyn_cast<Function>(
                                           globalFuncItem->getOperand(1));

                if (priority && func)
                {
                    queue.push(LLVMGlobalFunction(LLVMUtil::getIntegerValue(priority).second,
                                                  func));
                }
            }
        }
    }

    // Generate a sorted vector of functions from the priority queue.
    while (!queue.empty())
    {
        result.push_back(queue.top().func);
        queue.pop();
    }
    return result;
}

void LLVMModuleSet::addSVFMain()
{
    std::vector<const Function*> ctor_funcs;
    std::vector<const Function*> dtor_funcs;
    Function* orgMain = 0;
    Module* mainMod = nullptr;

    for (Module &mod : modules)
    {
        // Collect ctor and dtor functions
        for (const GlobalVariable& global : mod.globals())
        {
            if (global.getName().equals(SVF_GLOBAL_CTORS) && global.hasInitializer())
            {
                ctor_funcs = getLLVMGlobalFunctions(&global);
            }
            else if (global.getName().equals(SVF_GLOBAL_DTORS) && global.hasInitializer())
            {
                dtor_funcs = getLLVMGlobalFunctions(&global);
            }
        }

        // Find main function
        for (auto &func : mod)
        {
            auto funName = func.getName();

            assert(!funName.equals(SVF_MAIN_FUNC_NAME) && SVF_MAIN_FUNC_NAME " already defined");

            if (funName.equals("main"))
            {
                orgMain = &func;
                mainMod = &mod;
            }
        }
    }

    // Only create svf.main when the original main function is found, and also
    // there are global constructor or destructor functions.
    if (orgMain && getModuleNum() > 0 &&
            (ctor_funcs.size() > 0 || dtor_funcs.size() > 0))
    {
        assert(mainMod && "Module with main function not found.");
        Module& M = *mainMod;
        // char **
        Type* ptr = PointerType::getUnqual(M.getContext());
        Type* i32 = IntegerType::getInt32Ty(M.getContext());
        // define void @svf.main(i32, i8**, i8**)
#if (LLVM_VERSION_MAJOR >= 9)
        FunctionCallee svfmainFn = M.getOrInsertFunction(
                                       SVF_MAIN_FUNC_NAME,
                                       Type::getVoidTy(M.getContext()),
                                       i32,ptr,ptr
                                   );
        Function* svfmain = SVFUtil::dyn_cast<Function>(svfmainFn.getCallee());
#else
        Function* svfmain = SVFUtil::dyn_cast<Function>(M.getOrInsertFunction(
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
        for (auto& ctor : ctor_funcs)
        {
            auto target = M.getOrInsertFunction(
                              ctor->getName(),
                              Type::getVoidTy(M.getContext())
                          );
            Builder.CreateCall(target);
        }
        // main() should be called after all ctor functions and before dtor
        // functions.
        Function::arg_iterator arg_it = svfmain->arg_begin();
        Value* args[] = {arg_it, arg_it + 1, arg_it + 2};
        size_t cnt = orgMain->arg_size();
        assert(cnt <= 3 && "Too many arguments for main()");
        Builder.CreateCall(orgMain, llvm::ArrayRef<Value*>(args, args + cnt));
        // emit "call void @dtor()". dtor_funcs is sorted so the functions are
        // emitted in the order of priority
        for (auto& dtor : dtor_funcs)
        {
            auto target = M.getOrInsertFunction(dtor->getName(), Type::getVoidTy(M.getContext()));
            Builder.CreateCall(target);
        }
        // return;
        Builder.CreateRetVoid();
    }
}

void LLVMModuleSet::collectExtFunAnnotations(const Module* mod)
{
    GlobalVariable *glob = mod->getGlobalVariable("llvm.global.annotations");
    if (glob == nullptr || !glob->hasInitializer())
        return;

    ConstantArray *ca = SVFUtil::dyn_cast<ConstantArray>(glob->getInitializer());
    if (ca == nullptr)
        return;

    for (unsigned i = 0; i < ca->getNumOperands(); ++i)
    {
        ConstantStruct *structAn = SVFUtil::dyn_cast<ConstantStruct>(ca->getOperand(i));
        if (structAn == nullptr || structAn->getNumOperands() == 0)
            continue;

        // Check if the annotation is for a function
        Function* fun = nullptr;
        GlobalVariable *annotateStr = nullptr;
        /// Non-opaque pointer
        if (ConstantExpr *expr = SVFUtil::dyn_cast<ConstantExpr>(structAn->getOperand(0)))
        {
            if (expr->getOpcode() == Instruction::BitCast && SVFUtil::isa<Function>(expr->getOperand(0)))
                fun = SVFUtil::cast<Function>(expr->getOperand(0));

            ConstantExpr *note = SVFUtil::cast<ConstantExpr>(structAn->getOperand(1));
            if (note->getOpcode() != Instruction::GetElementPtr)
                continue;

            annotateStr = SVFUtil::dyn_cast<GlobalVariable>(note->getOperand(0));
        }
        /// Opaque pointer
        else
        {
            fun = SVFUtil::dyn_cast<Function>(structAn->getOperand(0));
            annotateStr = SVFUtil::dyn_cast<GlobalVariable>(structAn->getOperand(1));
        }

        if (!fun || annotateStr == nullptr || !annotateStr->hasInitializer())
            continue;;

        ConstantDataSequential *data = SVFUtil::dyn_cast<ConstantDataSequential>(annotateStr->getInitializer());
        if (data && data->isString())
        {
            std::string annotation = data->getAsString().str();
            if (!annotation.empty())
                ExtFun2Annotations[fun->getName().str()].push_back(annotation);
        }
    }
}

/*
    There are three types of functions(definitions) in extapi.c:
    1. (Fun_Overwrite): Functions with "OVERWRITE" annotion:
        These functions are used to replace the corresponding function definitions in the application.
    2. (Fun_Annotation): Functions with annotation(s) but without "OVERWRITE" annotation:
        These functions are used to tell SVF to do special processing, like malloc().
    3. (Fun_Noraml): Functions without any annotation:
        These functions are used to replace the corresponding function declarations in the application.


    We will iterate over declarations (appFunDecl) and definitons (appFunDef) of functions in the application and extapi.c to do the following clone or replace operations:
    1. appFuncDecl --> Fun_Normal:     Clone the Fun_Overwrite and replace the appFuncDecl in application.
    2. appFuncDecl --> Fun_Annotation: Move the annotions on Fun_Annotation to appFuncDecl in application.

    3. appFunDef --> Fun_Overwrite:    Clone the Fun_Overwrite and overwrite the appFunDef in application.
    4. appFunDef --> Fun_Annotation:   Replace the appFunDef with appFunDecl and move the annotions to appFunDecl in application
*/
void LLVMModuleSet::buildFunToFunMap()
{
    Set<const Function*> appFunDecls, appFunDefs, extFuncs, clonedFuncs;
    OrderedSet<string> appFuncDeclNames, appFuncDefNames, extFunDefNames, intersectNames;
    Map<const Function*, const Function*> extFuncs2ClonedFuncs;
    Module* appModule = nullptr;
    Module* extModule = nullptr;

    for (Module& mod : modules)
    {
        // extapi.bc functions
        if (mod.getName().str() == ExtAPI::getExtAPI()->getExtBcPath())
        {
            collectExtFunAnnotations(&mod);
            extModule = &mod;
            for (const Function& fun : mod.functions())
            {
                // there is main declaration in ext bc, it should be mapped to
                // main definition in app bc.
                if (fun.getName().str() == "main")
                {
                    appFunDecls.insert(&fun);
                    appFuncDeclNames.insert(fun.getName().str());
                }
                /// Keep svf_main() function and all the functions called in svf_main()
                else if (fun.getName().str() == "svf__main")
                {
                    ExtFuncsVec.push_back(&fun);
                }
                else
                {
                    extFuncs.insert(&fun);
                    extFunDefNames.insert(fun.getName().str());
                }
            }
        }
        else
        {
            appModule = &mod;
            /// app functions
            for (const Function& fun : mod.functions())
            {
                if (fun.isDeclaration())
                {
                    appFunDecls.insert(&fun);
                    appFuncDeclNames.insert(fun.getName().str());
                }
                else
                {
                    appFunDefs.insert(&fun);
                    appFuncDefNames.insert(fun.getName().str());
                }
            }
        }
    }

    // Find the intersectNames between appFuncDefNames and externalFunDefNames
    std::set_intersection(
        appFuncDefNames.begin(), appFuncDefNames.end(), extFunDefNames.begin(), extFunDefNames.end(),
        std::inserter(intersectNames, intersectNames.end()));

    auto cloneAndReplaceFunction = [&](const Function* extFunToClone, Function* appFunToReplace, Module* appModule, bool cloneBody) -> Function*
    {
        assert(!(appFunToReplace == NULL && appModule == NULL) && "appFunToReplace and appModule cannot both be NULL");

        if (appFunToReplace)
        {
            appModule = appFunToReplace->getParent();
        }
        // Create a new function with the same signature as extFunToClone
        Function *clonedFunction = Function::Create(extFunToClone->getFunctionType(), Function::ExternalLinkage, extFunToClone->getName(), appModule);
        // Map the arguments of the new function to the arguments of extFunToClone
        llvm::ValueToValueMapTy valueMap;
        Function::arg_iterator destArg = clonedFunction->arg_begin();
        for (Function::const_arg_iterator srcArg = extFunToClone->arg_begin(); srcArg != extFunToClone->arg_end(); ++srcArg)
        {
            destArg->setName(srcArg->getName()); // Copy the name of the original argument
            valueMap[&*srcArg] = &*destArg++; // Add a mapping from the old arg to the new arg
        }
        if (cloneBody)
        {
            // Collect global variables referenced by extFunToClone
            // This step identifies all global variables used within the function to be cloned
            std::set<GlobalVariable*> referencedGlobals;
            for (const BasicBlock& BB : *extFunToClone)
            {
                for (const Instruction& I : BB)
                {
                    for (const Value* operand : I.operands())
                    {
                        // Check if the operand is a global variable
                        if (const GlobalVariable* GV = SVFUtil::dyn_cast<GlobalVariable>(operand))
                        {
                            referencedGlobals.insert(const_cast<GlobalVariable*>(GV));
                        }
                    }
                }
            }

            // Copy global variables to target module and update valueMap
            // When cloning a function, we need to ensure all global variables it references are available in the target module
            for (GlobalVariable* GV : referencedGlobals)
            {
                // Check if the global variable already exists in the target module
                GlobalVariable* existingGV = appModule->getGlobalVariable(GV->getName());
                if (existingGV)
                {
                    // If the global variable already exists, ensure type consistency
                    assert(existingGV->getType() == GV->getType() && "Global variable type mismatch in client module!");
                    // Map the original global to the existing one in the target module
                    valueMap[GV] = existingGV; // Map to existing global variable
                }
                else
                {
                    // If the global variable doesn't exist in the target module, create a new one with the same properties
                    GlobalVariable* newGV = new GlobalVariable(
                        *appModule,                   // Target module
                        GV->getValueType(),           // Type of the global variable
                        GV->isConstant(),             // Whether it's constant
                        GV->getLinkage(),             // Linkage type
                        nullptr,                      // No initializer yet
                        GV->getName(),                // Same name
                        nullptr,                      // No insert before instruction
                        GV->getThreadLocalMode(),     // Thread local mode
                        GV->getAddressSpace()         // Address space
                    );

                    // Copy initializer if present to maintain the global's value
                    if (GV->hasInitializer())
                    {
                        Constant* init = GV->getInitializer();
                        newGV->setInitializer(init); // Simple case: direct copy
                    }

                    // Copy other attributes like alignment to ensure identical behavior
                    newGV->copyAttributesFrom(GV);

                    // Add mapping from original global to the new one for use during function cloning
                    valueMap[GV] = newGV;
                }
            }

            // Clone function body with updated valueMap
            llvm::SmallVector<ReturnInst*, 8> ignoredReturns;
            CloneFunctionInto(clonedFunction, extFunToClone, valueMap, llvm::CloneFunctionChangeType::LocalChangesOnly, ignoredReturns, "", nullptr);
        }
        if (appFunToReplace)
        {
            // Replace all uses of appFunToReplace with clonedFunction
            appFunToReplace->replaceAllUsesWith(clonedFunction);
            std::string oldFunctionName = appFunToReplace->getName().str();
            // Delete the old function
            appFunToReplace->eraseFromParent();
            clonedFunction->setName(oldFunctionName);
        }
        return clonedFunction;
    };

    /// App Func decl -> SVF extern Func def
    for (const Function* appFunDecl : appFunDecls)
    {
        std::string appFunDeclName = LLVMUtil::restoreFuncName(appFunDecl->getName().str());
        for (const Function* extFun : extFuncs)
        {
            if (extFun->getName().str().compare(appFunDeclName) == 0)
            {
                auto it = ExtFun2Annotations.find(extFun->getName().str());
                // Without annotations, this function is normal function with useful function body
                if (it == ExtFun2Annotations.end())
                {
                    Function* clonedFunction = cloneAndReplaceFunction(const_cast<Function*>(extFun), const_cast<Function*>(appFunDecl), nullptr, true);
                    extFuncs2ClonedFuncs[extFun] = clonedFunction;
                    clonedFuncs.insert(clonedFunction);
                }
                else
                {
                    ExtFuncsVec.push_back(appFunDecl);
                }
                break;
            }
        }
    }

    /// Overwrite
    /// App Func def -> SVF extern Func def
    for (string sameFuncDef: intersectNames)
    {
        Function* appFuncDef = appModule->getFunction(sameFuncDef);
        Function* extFuncDef = extModule->getFunction(sameFuncDef);
        if (appFuncDef == nullptr || extFuncDef == nullptr)
            continue;

        FunctionType *appFuncDefType = appFuncDef->getFunctionType();
        FunctionType *extFuncDefType = extFuncDef->getFunctionType();
        if (appFuncDefType != extFuncDefType)
            continue;

        auto it = ExtFun2Annotations.find(sameFuncDef);
        if (it != ExtFun2Annotations.end())
        {
            std::vector<std::string> annotations = it->second;
            if (annotations.size() == 1 && annotations[0].find("OVERWRITE") != std::string::npos)
            {
                Function* clonedFunction = cloneAndReplaceFunction(const_cast<Function*>(extFuncDef), const_cast<Function*>(appFuncDef), nullptr, true);
                extFuncs2ClonedFuncs[extFuncDef] = clonedFunction;
                clonedFuncs.insert(clonedFunction);
            }
            else
            {
                if (annotations.size() >= 2)
                {
                    for (const auto& annotation : annotations)
                    {
                        if(annotation.find("OVERWRITE") != std::string::npos)
                        {
                            assert(false && "overwrite and other annotations cannot co-exist");
                        }
                    }
                }
            }
        }
    }

    auto linkFunctions = [&](Function* caller, Function* callee)
    {
        for (inst_iterator I = inst_begin(caller), E = inst_end(caller); I != E; ++I)
        {
            Instruction *inst = &*I;

            if (CallInst *callInst = SVFUtil::dyn_cast<CallInst>(inst))
            {
                Function *calledFunc = callInst->getCalledFunction();

                if (calledFunc && calledFunc->getName() == callee->getName())
                {
                    callInst->setCalledFunction(callee);
                }
            }
        }
    };

    std::function<void(const Function*, Function*)> cloneAndLinkFunction;
    cloneAndLinkFunction = [&](const Function* extFunToClone, Function* appClonedFun)
    {
        if (clonedFuncs.find(extFunToClone) != clonedFuncs.end())
            return;

        Module* appModule = appClonedFun->getParent();
        // Check if the function already exists in the parent module
        if (appModule->getFunction(extFunToClone->getName()))
        {
            // The function already exists, no need to clone, but need to link it with the caller
            Function*  func = appModule->getFunction(extFunToClone->getName());
            linkFunctions(appClonedFun, func);
            return;
        }
        // Decide whether to clone the function body based on ExtFun2Annotations
        bool cloneBody = true;
        auto it = ExtFun2Annotations.find(extFunToClone->getName().str());
        if (it != ExtFun2Annotations.end())
        {
            std::vector<std::string> annotations = it->second;
            if (!(annotations.size() == 1 && annotations[0].find("OVERWRITE") != std::string::npos))
            {
                cloneBody = false;
            }
        }

        Function* clonedFunction = cloneAndReplaceFunction(extFunToClone, nullptr, appModule, cloneBody);

        clonedFuncs.insert(clonedFunction);
        // Add the cloned function to ExtFuncsVec for further processing
        ExtFuncsVec.push_back(clonedFunction);

        linkFunctions(appClonedFun, clonedFunction);

        std::vector<const Function*> calledFunctions = LLVMUtil::getCalledFunctions(extFunToClone);

        for (const auto& calledFunction : calledFunctions)
        {
            cloneAndLinkFunction(calledFunction, clonedFunction);
        }
    };

    // Recursive clone called functions
    for (const auto& pair : extFuncs2ClonedFuncs)
    {
        Function* extFun = const_cast<Function*>(pair.first);
        Function* clonedExtFun = const_cast<Function*>(pair.second);
        std::vector<const Function*> extCalledFuns = LLVMUtil::getCalledFunctions(extFun);

        for (const auto& extCalledFun : extCalledFuns)
        {
            cloneAndLinkFunction(extCalledFun, clonedExtFun);
        }
    }

    // Remove unused annotations in ExtFun2Annotations according to the functions in ExtFuncsVec
    Fun2AnnoMap newFun2AnnoMap;
    for (const Function* extFun : ExtFuncsVec)
    {
        std::string name = LLVMUtil::restoreFuncName(extFun->getName().str());
        auto it = ExtFun2Annotations.find(name);
        if (it != ExtFun2Annotations.end())
        {
            std::string newKey = name;
            if (name != extFun->getName().str())
            {
                newKey = extFun->getName().str();
            }
            newFun2AnnoMap.insert({newKey, it->second});
        }
    }
    ExtFun2Annotations.swap(newFun2AnnoMap);

    // Remove ExtAPI module from modules
    auto it = std::find_if(modules.begin(), modules.end(),
                           [&extModule](const std::reference_wrapper<llvm::Module>& moduleRef)
    {
        return &moduleRef.get() == extModule;
    });

    if (it != modules.end())
    {
        size_t index = std::distance(modules.begin(), it);
        modules.erase(it);
        owned_modules.erase(owned_modules.begin() + index);
    }
}

void LLVMModuleSet::buildGlobalDefToRepMap()
{
    typedef Map<string, Set<GlobalVariable*>> NameToGlobalsMapTy;
    NameToGlobalsMapTy nameToGlobalsMap;
    for (Module &mod : modules)
    {
        // Collect ctor and dtor functions
        for (GlobalVariable& global : mod.globals())
        {
            if (global.hasPrivateLinkage())
                continue;
            string name = global.getName().str();
            if (name.empty())
                continue;
            nameToGlobalsMap[std::move(name)].insert(&global);
        }
    }

    for (const auto& pair : nameToGlobalsMap)
    {
        const Set<GlobalVariable*> &globals = pair.second;

        const auto repIt =
            std::find_if(globals.begin(), globals.end(),
                         [](GlobalVariable* g)
        {
            return g->hasInitializer();
        });
        GlobalVariable* rep =
            repIt != globals.end()
            ? *repIt
            // When there is no initializer, just pick the first one.
            : (assert(!globals.empty() && "Empty global set"),
               *globals.begin());

        for (const GlobalVariable* cur : globals)
        {
            GlobalDefToRepMap[cur] = rep;
        }
    }
}

// Dump modules to files
void LLVMModuleSet::dumpModulesToFile(const std::string& suffix)
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

NodeID LLVMModuleSet::getValueNode(const Value *llvm_value)
{
    if (SVFUtil::isa<ConstantPointerNull>(llvm_value))
        return svfir->nullPtrSymID();
    else if (SVFUtil::isa<UndefValue>(llvm_value))
        return svfir->blkPtrSymID();
    else
    {
        ValueToIDMapTy::const_iterator iter = valSymMap.find(llvm_value);
        assert(iter!=valSymMap.end() &&"value sym not found");
        return iter->second;
    }
}
bool LLVMModuleSet::hasValueNode(const Value *val)
{
    if (SVFUtil::isa<ConstantPointerNull, UndefValue>(val))
        return true;
    else
        return (valSymMap.find(val) != valSymMap.end());
}

NodeID LLVMModuleSet::getObjectNode(const Value *llvm_value)
{
    if (const GlobalVariable* glob = SVFUtil::dyn_cast<GlobalVariable>(llvm_value))
        llvm_value = LLVMUtil::getGlobalRep(glob);
    ValueToIDMapTy::const_iterator iter = objSymMap.find(llvm_value);
    assert(iter!=objSymMap.end() && "obj sym not found");
    return iter->second;
}


void LLVMModuleSet::dumpSymTable()
{
    OrderedMap<NodeID, const Value*> idmap;
    for (ValueToIDMapTy::iterator iter = valSymMap.begin(); iter != valSymMap.end();
            ++iter)
    {
        const NodeID i = iter->second;
        idmap[i] = iter->first;
    }
    for (ValueToIDMapTy::iterator iter = objSymMap.begin(); iter != objSymMap.end();
            ++iter)
    {
        const NodeID i = iter->second;
        idmap[i] = iter->first;
    }
    for (FunToIDMapTy::iterator iter = retSyms().begin(); iter != retSyms().end();
            ++iter)
    {
        const NodeID i = iter->second;
        idmap[i] = iter->first;
    }
    for (FunToIDMapTy::iterator iter = varargSyms().begin(); iter != varargSyms().end();
            ++iter)
    {
        const NodeID i = iter->second;
        idmap[i] = iter->first;
    }
    SVFUtil::outs() << "{SymbolTableInfo \n";




    for (auto iter : idmap)
    {
        std::string str;
        llvm::raw_string_ostream rawstr(str);
        auto llvmVal = iter.second;
        if (llvmVal)
            rawstr << " " << *llvmVal << " ";
        else
            rawstr << " No llvmVal found";
        rawstr << LLVMUtil::getSourceLoc(llvmVal);
        SVFUtil::outs() << iter.first << " " << rawstr.str() << "\n";
    }
    SVFUtil::outs() << "}\n";
}

void LLVMModuleSet::addToSVFVar2LLVMValueMap(const Value* val,
        SVFValue* svfBaseNode)
{
    SVFBaseNode2LLVMValue[svfBaseNode] = val;
    svfBaseNode->setSourceLoc(LLVMUtil::getSourceLoc(val));
    svfBaseNode->setName(val->getName().str());
}

const FunObjVar* LLVMModuleSet::getFunObjVar(const std::string& name)
{
    Function* fun = nullptr;

    for (u32_t i = 0; i < llvmModuleSet->getModuleNum(); ++i)
    {
        Module* mod = llvmModuleSet->getModule(i);
        fun = mod->getFunction(name);
        if (fun)
        {
            return llvmModuleSet->getFunObjVar(fun);
        }
    }
    return nullptr;
}


const Type* LLVMModuleSet::getLLVMType(const SVFType* T) const
{
    for(LLVMType2SVFTypeMap::const_iterator it = LLVMType2SVFType.begin(), eit = LLVMType2SVFType.end(); it!=eit; ++it)
    {
        if (it->second == T)
            return it->first;
    }
    assert(false && "can't find the corresponding LLVM Type");
    abort();
}

/*!
 * Get or create SVFType and typeinfo
 */
SVFType* LLVMModuleSet::getSVFType(const Type* T)
{
    assert(T && "SVFType should not be null");
    LLVMType2SVFTypeMap::const_iterator it = LLVMType2SVFType.find(T);
    if (it != LLVMType2SVFType.end())
        return it->second;

    SVFType* svfType = addSVFTypeInfo(T);
    StInfo* stinfo = collectTypeInfo(T);
    svfType->setTypeInfo(stinfo);
    return svfType;
}


/// Get a basic block ICFGNode
ICFGNode* LLVMModuleSet::getICFGNode(const Instruction* inst)
{
    ICFGNode* node;
    if(LLVMUtil::isNonInstricCallSite(inst))
        node = getCallICFGNode(inst);
    else if(LLVMUtil::isIntrinsicInst(inst))
        node = getIntraICFGNode(inst);
    else
        node = getIntraICFGNode(inst);

    assert (node!=nullptr && "no ICFGNode for this instruction?");
    return node;
}

bool LLVMModuleSet::hasICFGNode(const Instruction* inst)
{
    ICFGNode* node;
    if(LLVMUtil::isNonInstricCallSite(inst))
        node = getCallBlock(inst);
    else if(LLVMUtil::isIntrinsicInst(inst))
        node = getIntraBlock(inst);
    else
        node = getIntraBlock(inst);

    return node != nullptr;
}

CallICFGNode* LLVMModuleSet::getCallICFGNode(const Instruction* inst)
{
    assert(LLVMUtil::isCallSite(inst) && "not a call instruction?");
    assert(LLVMUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    CallICFGNode* node = getCallBlock(inst);
    assert (node!=nullptr && "no CallICFGNode for this instruction?");
    return node;
}

RetICFGNode* LLVMModuleSet::getRetICFGNode(const Instruction* inst)
{
    assert(LLVMUtil::isCallSite(inst) && "not a call instruction?");
    assert(LLVMUtil::isNonInstricCallSite(inst) && "associating an intrinsic debug instruction with an ICFGNode!");
    RetICFGNode* node = getRetBlock(inst);
    assert (node!=nullptr && "no RetICFGNode for this instruction?");
    return node;
}

IntraICFGNode* LLVMModuleSet::getIntraICFGNode(const Instruction* inst)
{
    IntraICFGNode* node = getIntraBlock(inst);
    assert (node!=nullptr && "no IntraICFGNode for this instruction?");
    return node;
}

StInfo* LLVMModuleSet::collectTypeInfo(const Type* T)
{
    Type2TypeInfoMap::iterator tit = Type2TypeInfo.find(T);
    if (tit != Type2TypeInfo.end())
    {
        return tit->second;
    }
    // No such StInfo for T, create it now.
    StInfo* stInfo;
    if (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(T))
    {
        stInfo = collectArrayInfo(aty);
    }
    else if (const StructType* sty = SVFUtil::dyn_cast<StructType>(T))
    {
        u32_t nf;
        stInfo = collectStructInfo(sty, nf);
        if (nf > svfir->maxStSize)
        {
            svfir->maxStruct = getSVFType(sty);
            svfir->maxStSize = nf;
        }
    }
    else
    {
        stInfo = collectSimpleTypeInfo(T);
    }
    Type2TypeInfo.emplace(T, stInfo);
    svfir->addStInfo(stInfo);
    return stInfo;
}

SVFType* LLVMModuleSet::addSVFTypeInfo(const Type* T)
{
    assert(LLVMType2SVFType.find(T) == LLVMType2SVFType.end() &&
           "SVFType has been added before");

    // add SVFType's LLVM byte size iff T isSized(), otherwise byteSize is 1 (default value)
    u32_t byteSize = 1;
    if (T->isSized())
    {
        const llvm::DataLayout &DL = LLVMModuleSet::getLLVMModuleSet()->
                                     getMainLLVMModule()->getDataLayout();
        Type *mut_T = const_cast<Type *>(T);
        byteSize = DL.getTypeAllocSize(mut_T);
    }

    SVFType* svftype;

    u32_t id = NodeIDAllocator::get()->allocateTypeId();
    if (SVFUtil::isa<PointerType>(T))
    {
        svftype = new SVFPointerType(id, byteSize);
    }
    else if (const IntegerType* intT = SVFUtil::dyn_cast<IntegerType>(T))
    {
        auto svfIntT = new SVFIntegerType(id, byteSize);
        unsigned signWidth = intT->getBitWidth();
        assert(signWidth < INT16_MAX && "Integer width too big");
        svfIntT->setSignAndWidth(intT->getSignBit() ? -signWidth : signWidth);
        svftype = svfIntT;
    }
    else if (const FunctionType* ft = SVFUtil::dyn_cast<FunctionType>(T))
    {
        std::vector<const SVFType*> paramTypes;
        for (const auto& t: ft->params())
        {
            paramTypes.push_back(getSVFType(t));
        }
        svftype = new SVFFunctionType(id, getSVFType(ft->getReturnType()), paramTypes, ft->isVarArg());
    }
    else if (const StructType* st = SVFUtil::dyn_cast<StructType>(T))
    {
        std::vector<const SVFType*> fieldTypes;

        for (const auto& t: st->elements())
        {
            fieldTypes.push_back(getSVFType(t));
        }
        auto svfst = new SVFStructType(id, fieldTypes, byteSize);
        if (st->hasName())
            svfst->setName(st->getName().str());
        svftype = svfst;
    }
    else if (const auto at = SVFUtil::dyn_cast<ArrayType>(T))
    {
        auto svfat = new SVFArrayType(id, byteSize);
        svfat->setNumOfElement(at->getNumElements());
        svfat->setTypeOfElement(getSVFType(at->getElementType()));
        svftype = svfat;
    }
    else
    {
        std::string buffer;
        auto ot = new SVFOtherType(id, T->isSingleValueType(), byteSize);
        llvm::raw_string_ostream(buffer) << *T;
        ot->setRepr(std::move(buffer));
        svftype = ot;
    }

    svfir->addTypeInfo(svftype);
    LLVMType2SVFType[T] = svftype;

    return svftype;
}

/*!
 * Fill in StInfo for an array type.
 */
StInfo* LLVMModuleSet::collectArrayInfo(const ArrayType* ty)
{
    u64_t totalElemNum = ty->getNumElements();
    const Type* elemTy = ty->getElementType();
    while (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        totalElemNum *= aty->getNumElements();
        elemTy = aty->getElementType();
    }

    StInfo* stInfo = new StInfo(totalElemNum);
    const SVFType* elemSvfType = getSVFType(elemTy);

    /// array without any element (this is not true in C/C++ arrays) we assume there is an empty dummy element
    if (totalElemNum == 0)
    {
        stInfo->addFldWithType(0, elemSvfType, 0);
        stInfo->setNumOfFieldsAndElems(1, 1);
        stInfo->getFlattenFieldTypes().push_back(elemSvfType);
        stInfo->getFlattenElementTypes().push_back(elemSvfType);
        return stInfo;
    }

    /// Array's flatten field infor is the same as its element's
    /// flatten infor.
    StInfo* elemStInfo = collectTypeInfo(elemTy);
    u32_t nfF = elemStInfo->getNumOfFlattenFields();
    u32_t nfE = elemStInfo->getNumOfFlattenElements();
    for (u32_t j = 0; j < nfF; j++)
    {
        const SVFType* fieldTy = elemStInfo->getFlattenFieldTypes()[j];
        stInfo->getFlattenFieldTypes().push_back(fieldTy);
    }

    /// Flatten arrays, map each array element index `i` to flattened index `(i * nfE * totalElemNum)/outArrayElemNum`
    /// nfE>1 if the array element is a struct with more than one field.
    u32_t outArrayElemNum = ty->getNumElements();
    for (u32_t i = 0; i < outArrayElemNum; ++i)
    {
        auto idx = (i * nfE * totalElemNum) / outArrayElemNum;
        stInfo->addFldWithType(0, elemSvfType, idx);
    }

    for (u32_t i = 0; i < totalElemNum; ++i)
    {
        for (u32_t j = 0; j < nfE; ++j)
        {
            const SVFType* et = elemStInfo->getFlattenElementTypes()[j];
            stInfo->getFlattenElementTypes().push_back(et);
        }
    }

    assert(stInfo->getFlattenElementTypes().size() == nfE * totalElemNum &&
           "typeForArray size incorrect!!!");
    stInfo->setNumOfFieldsAndElems(nfF, nfE * totalElemNum);

    return stInfo;
}

/*!
 * Fill in struct_info for T.
 * Given a Struct type, we recursively extend and record its fields and types.
 */
StInfo* LLVMModuleSet::collectStructInfo(const StructType* structTy,
        u32_t& numFields)
{
    /// The struct info should not be processed before
    StInfo* stInfo = new StInfo(1);

    // Number of fields after flattening the struct
    numFields = 0;
    // The offset when considering array stride info
    u32_t strideOffset = 0;
    for (const Type* elemTy : structTy->elements())
    {
        const SVFType* elemSvfTy = getSVFType(elemTy);
        // offset with int_32 (s32_t) is large enough and won't overflow
        stInfo->addFldWithType(numFields, elemSvfTy, strideOffset);

        if (SVFUtil::isa<StructType, ArrayType>(elemTy))
        {
            StInfo* subStInfo = collectTypeInfo(elemTy);
            u32_t nfF = subStInfo->getNumOfFlattenFields();
            u32_t nfE = subStInfo->getNumOfFlattenElements();
            // Copy ST's info, whose element 0 is the size of ST itself.
            for (u32_t j = 0; j < nfF; ++j)
            {
                const SVFType* elemTy = subStInfo->getFlattenFieldTypes()[j];
                stInfo->getFlattenFieldTypes().push_back(elemTy);
            }
            numFields += nfF;
            strideOffset += nfE;
            for (u32_t tpj = 0; tpj < nfE; ++tpj)
            {
                const SVFType* ty = subStInfo->getFlattenElementTypes()[tpj];
                stInfo->getFlattenElementTypes().push_back(ty);
            }

        }
        else
        {
            // Simple type
            numFields += 1;
            strideOffset += 1;
            stInfo->getFlattenFieldTypes().push_back(elemSvfTy);
            stInfo->getFlattenElementTypes().push_back(elemSvfTy);
        }
    }

    assert(stInfo->getFlattenElementTypes().size() == strideOffset &&
           "typeForStruct size incorrect!");
    stInfo->setNumOfFieldsAndElems(numFields,strideOffset);

    return stInfo;
}


/*!
 * Collect simple type (non-aggregate) info
 */
StInfo* LLVMModuleSet::collectSimpleTypeInfo(const Type* ty)
{
    /// Only one field
    StInfo* stInfo = new StInfo(1);
    SVFType* svfType = getSVFType(ty);
    stInfo->addFldWithType(0, svfType, 0);

    stInfo->getFlattenFieldTypes().push_back(svfType);
    stInfo->getFlattenElementTypes().push_back(svfType);
    stInfo->setNumOfFieldsAndElems(1,1);

    return stInfo;
}

void LLVMModuleSet::setExtFuncAnnotations(const Function* fun, const std::vector<std::string>& funcAnnotations)
{
    assert(fun && "Null SVFFunction* pointer");
    func2Annotations[fun] = funcAnnotations;
}

bool LLVMModuleSet::hasExtFuncAnnotation(const Function* fun, const std::string& funcAnnotation)
{
    assert(fun && "Null SVFFunction* pointer");
    auto it = func2Annotations.find(fun);
    if (it != func2Annotations.end())
    {
        for (const std::string& annotation : it->second)
            if (annotation.find(funcAnnotation) != std::string::npos)
                return true;
    }
    return false;
}

std::string LLVMModuleSet::getExtFuncAnnotation(const Function* fun, const std::string& funcAnnotation)
{
    assert(fun && "Null Function* pointer");
    auto it = func2Annotations.find(fun);
    if (it != func2Annotations.end())
    {
        for (const std::string& annotation : it->second)
            if (annotation.find(funcAnnotation) != std::string::npos)
                return annotation;
    }
    return "";
}

const std::vector<std::string>& LLVMModuleSet::getExtFuncAnnotations(const Function* fun)
{
    assert(fun && "Null Function* pointer");
    auto it = func2Annotations.find(fun);
    if (it != func2Annotations.end())
        return it->second;
    return func2Annotations[fun];
}

bool LLVMModuleSet::is_memcpy(const Function *F)
{
    return F &&
           (hasExtFuncAnnotation(F, "MEMCPY") ||  hasExtFuncAnnotation(F, "STRCPY")
            || hasExtFuncAnnotation(F, "STRCAT"));
}

bool LLVMModuleSet::is_memset(const Function *F)
{
    return F && hasExtFuncAnnotation(F, "MEMSET");
}

bool LLVMModuleSet::is_alloc(const Function* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_HEAP_RET");
}

// Does (F) allocate a new object and assign it to one of its arguments?
bool LLVMModuleSet::is_arg_alloc(const Function* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_HEAP_ARG");
}

bool LLVMModuleSet::is_alloc_stack_ret(const Function* F)
{
    return F && hasExtFuncAnnotation(F, "ALLOC_STACK_RET");
}

// Get the position of argument which holds the new object
s32_t LLVMModuleSet::get_alloc_arg_pos(const Function* F)
{
    std::string allocArg = getExtFuncAnnotation(F, "ALLOC_HEAP_ARG");
    assert(!allocArg.empty() && "Not an alloc call via argument or incorrect extern function annotation!");

    std::string number;
    for (char c : allocArg)
    {
        if (isdigit(c))
            number.push_back(c);
    }
    assert(!number.empty() && "Incorrect naming convention for svf external functions(ALLOC_HEAP_ARG + number)?");
    return std::stoi(number);
}

// Does (F) reallocate a new object?
bool LLVMModuleSet::is_realloc(const Function* F)
{
    return F && hasExtFuncAnnotation(F, "REALLOC_HEAP_RET");
}


// Should (F) be considered "external" (either not defined in the program
//   or a user-defined version of a known alloc or no-op)?
bool LLVMModuleSet::is_ext(const Function* F)
{
    assert(F && "Null SVFFunction* pointer");
    if (F->isDeclaration() || F->isIntrinsic())
        return true;
    else if (hasExtFuncAnnotation(F, "OVERWRITE") && getExtFuncAnnotations(F).size() == 1)
        return false;
    else
        return !getExtFuncAnnotations(F).empty();
}
