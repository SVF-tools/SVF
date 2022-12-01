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
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/BreakConstantExpr.h"
#include "SVF-LLVM/SymbolTableBuilder.h"
#include "MSSA/SVFGBuilder.h"
#include "llvm/Support/FileSystem.h"

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

LLVMModuleSet::LLVMModuleSet(): svfModule(nullptr), cxts(nullptr), preProcessed(false)
{
    symInfo = SymbolTableInfo::SymbolInfo();
}

LLVMModuleSet::~LLVMModuleSet()
{

}

SVFModule* LLVMModuleSet::buildSVFModule(Module &mod)
{
    svfModule = std::make_unique<SVFModule>(mod.getModuleIdentifier());
    modules.emplace_back(mod);

    build();

    return svfModule.get();
}

SVFModule* LLVMModuleSet::buildSVFModule(const std::vector<std::string> &moduleNameVec)
{
    double startSVFModuleTime = SVFStat::getClk(true);

    assert(llvmModuleSet && "LLVM Module set needs to be created!");

    loadModules(moduleNameVec);

    if(!moduleNameVec.empty())
        svfModule = std::make_unique<SVFModule>(*moduleNameVec.begin());
    else
        svfModule = std::make_unique<SVFModule>();

    build();

    double endSVFModuleTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingLLVMModule = (endSVFModuleTime - startSVFModuleTime)/TIMEINTERVAL;

    double startSymInfoTime = SVFStat::getClk(true);
    if (!SVFModule::pagReadFromTXT())
    {
        /// building symbol table
        DBOUT(DGENERAL, SVFUtil::outs() << SVFUtil::pasMsg("Building Symbol table ...\n"));
        SymbolTableBuilder builder(symInfo);
        builder.buildMemModel(svfModule.get());
    }
    double endSymInfoTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingSymbolTable = (endSymInfoTime - startSymInfoTime)/TIMEINTERVAL;

    return svfModule.get();
}

void LLVMModuleSet::build()
{
    if(preProcessed==false)
        prePassSchedule();

    buildFunToFunMap();
    buildGlobalDefToRepMap();

    if (Options::SVFMain)
        addSVFMain();

    createSVFDataStructure();
    initSVFFunction();
}

void LLVMModuleSet::createSVFDataStructure()
{

    for (Module& mod : modules)
    {
        /// Function
        for (Module::const_iterator it = mod.begin(), eit = mod.end(); it != eit; ++it)
        {
            const Function* func = &*it;
            SVFLoopAndDomInfo* ld = new SVFLoopAndDomInfo();
            SVFFunction* svfFunc = new SVFFunction(func->getName().str(), getSVFType(func->getType()), SVFUtil::cast<SVFFunctionType>(getSVFType(func->getFunctionType())), func->isDeclaration(), LLVMUtil::isIntrinsicFun(func), func->hasAddressTaken(), func->isVarArg(), ld);
            svfModule->addFunctionSet(svfFunc);
            addFunctionMap(func,svfFunc);

            for (Function::const_arg_iterator I = func->arg_begin(), E = func->arg_end(); I != E; ++I)
            {
                const Argument* arg = &*I;
                SVFArgument* svfarg = new SVFArgument(arg->getName().str(), getSVFType(arg->getType()), svfFunc, arg->getArgNo(), LLVMUtil::isArgOfUncalledFunction(arg));
                svfFunc->addArgument(svfarg);
                addArgumentMap(arg,svfarg);
            }

            for (Function::const_iterator bit = func->begin(), ebit = func->end(); bit != ebit; ++bit)
            {
                const BasicBlock* bb = &*bit;
                SVFBasicBlock* svfBB = new SVFBasicBlock(bb->getName().str(), getSVFType(bb->getType()), svfFunc);
                svfFunc->addBasicBlock(svfBB);
                addBasicBlockMap(bb,svfBB);
                for (BasicBlock::const_iterator iit = bb->begin(), eiit = bb->end(); iit != eiit; ++iit)
                {
                    const Instruction* inst = &*iit;
                    SVFInstruction* svfInst = nullptr;
                    if(const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
                    {
                        if(cppUtil::isVirtualCallSite(call))
                            svfInst = new SVFVirtualCallInst(call->getName().str(), getSVFType(call->getType()), svfBB,call->getFunctionType()->isVarArg(),inst->isTerminator());
                        else
                            svfInst = new SVFCallInst(call->getName().str(), getSVFType(call->getType()), svfBB,call->getFunctionType()->isVarArg(),inst->isTerminator());
                    }
                    else
                    {
                        svfInst = new SVFInstruction(inst->getName().str(),getSVFType(inst->getType()), svfBB, inst->isTerminator(), SVFUtil::isa<ReturnInst>(inst));
                    }
                    svfBB->addInstruction(svfInst);
                    addInstructionMap(inst,svfInst);
                }
            }
        }

        /// GlobalVariable
        for (Module::const_global_iterator it = mod.global_begin(),
                eit = mod.global_end(); it != eit; ++it)
        {
            const GlobalVariable* global = &*it;
            SVFGlobalValue* svfglobal = new SVFGlobalValue(global->getName().str(), getSVFType(global->getType()));
            svfModule->addGlobalSet(svfglobal);
            addGlobalValueMap(global,svfglobal);
        }

        /// GlobalAlias
        for (Module::const_alias_iterator it = mod.alias_begin(),
                eit = mod.alias_end(); it != eit; ++it)
        {
            const GlobalAlias *alias = &*it;
            SVFGlobalValue* svfalias = new SVFGlobalValue(alias->getName().str(), getSVFType(alias->getType()));
            svfModule->addAliasSet(svfalias);
            addGlobalValueMap(alias,svfalias);
        }
    }
}

void LLVMModuleSet::initSVFFunction()
{
    for (Module& mod : modules)
    {
        /// Function
        for (Module::iterator it = mod.begin(), eit = mod.end(); it != eit; ++it)
        {
            const Function* f = &*it;
            SVFFunction* svffun = getSVFFunction(f);
            initSVFBasicBlock(f);

            if (SVFUtil::isExtCall(svffun) == false)
            {
                initDomTree(svffun, f);
            }
        }
    }
}

void LLVMModuleSet::initSVFBasicBlock(const Function* func)
{
    for (Function::const_iterator bit = func->begin(), ebit = func->end(); bit != ebit; ++bit)
    {
        const BasicBlock* bb = &*bit;
        SVFBasicBlock* svfbb = getSVFBasicBlock(bb);
        for (succ_const_iterator succ_it = succ_begin(bb); succ_it != succ_end(bb); succ_it++)
        {
            const SVFBasicBlock* svf_scc_bb = getSVFBasicBlock(*succ_it);
            svfbb->addSuccBasicBlock(svf_scc_bb);
        }
        for (const_pred_iterator pred_it = pred_begin(bb); pred_it != pred_end(bb); pred_it++)
        {
            const SVFBasicBlock* svf_pred_bb = getSVFBasicBlock(*pred_it);
            svfbb->addPredBasicBlock(svf_pred_bb);
        }
        for (BasicBlock::const_iterator iit = bb->begin(), eiit = bb->end(); iit != eiit; ++iit)
        {
            const Instruction* inst = &*iit;
            if(const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
            {
                SVFInstruction* svfinst = getSVFInstruction(call);
                SVFCallInst* svfcall = SVFUtil::cast<SVFCallInst>(svfinst);
                SVFValue* callee = getSVFValue(call->getCalledOperand()->stripPointerCasts());
                svfcall->setCalledOperand(callee);
                if(SVFVirtualCallInst* virtualCall = SVFUtil::dyn_cast<SVFVirtualCallInst>(svfcall))
                {
                    virtualCall->setVtablePtr(getSVFValue(cppUtil::getVCallVtblPtr(call)));
                    virtualCall->setFunIdxInVtable(cppUtil::getVCallIdx(call));
                    virtualCall->setFunNameOfVirtualCall(cppUtil::getFunNameOfVCallSite(call));
                }
                for(u32_t i = 0; i < call->arg_size(); i++)
                {
                    SVFValue* svfval = getSVFValue(call->getArgOperand(i));
                    svfcall->addArgument(svfval);
                }
            }
            LLVMUtil::getNextInsts(inst, getSVFInstruction(inst)->getSuccInstructions());
            LLVMUtil::getPrevInsts(inst, getSVFInstruction(inst)->getPredInstructions());
        }
    }
}


void LLVMModuleSet::initDomTree(SVFFunction* svffun, const Function* fun)
{
    //process and stored dt & df
    DominatorTree dt;
    DominanceFrontier df;
    dt.recalculate(const_cast<Function&>(*fun));
    df.analyze(dt);
    LoopInfo loopInfo = LoopInfo(dt);
    PostDominatorTree pdt = PostDominatorTree(const_cast<Function&>(*fun));
    SVFLoopAndDomInfo* ld = svffun->getLoopAndDomInfo();

    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> & dfBBsMap = ld->getDomFrontierMap();
    for (DominanceFrontierBase::const_iterator dfIter = df.begin(), eDfIter = df.end(); dfIter != eDfIter; dfIter++)
    {
        const BasicBlock* keyBB = dfIter->first;
        const std::set<BasicBlock* >& domSet = dfIter->second;
        Set<const SVFBasicBlock*>& valueBasicBlocks = dfBBsMap[getSVFBasicBlock(keyBB)];
        for (const BasicBlock* bbValue:domSet)
        {
            valueBasicBlocks.insert(getSVFBasicBlock(bbValue));
        }
    }
    std::vector<const SVFBasicBlock*> reachableBBs;
    LLVMUtil::getFunReachableBBs(fun, reachableBBs);
    ld->setReachableBBs(reachableBBs);

    for (Function::const_iterator bit = fun->begin(), ebit = fun->end(); bit != ebit; ++bit)
    {
        const BasicBlock* bb = &*bit;
        SVFBasicBlock* svf_bb = getSVFBasicBlock(bb);
        if(DomTreeNode *dtNode = dt.getNode(const_cast<BasicBlock*>(bb)))
        {
            DomTreeNode::iterator DI = dtNode->begin();
            if (DI != dtNode->end())
            {
                for (DomTreeNode::iterator DI = dtNode->begin(), DE = dtNode->end(); DI != DE; ++DI)
                {
                    const SVFBasicBlock* dombb = getSVFBasicBlock((*DI)->getBlock());
                    ld->getDomTreeMap()[svf_bb].insert(dombb);
                }
            }
            else
            {
                ld->getDomTreeMap()[svf_bb] = Set<const SVFBasicBlock* >();
            }
        }

        if(DomTreeNode * pdtNode = pdt.getNode(const_cast<BasicBlock*>(bb)))
        {
            DomTreeNode::iterator DI = pdtNode->begin();
            if (DI != pdtNode->end())
            {
                for (DomTreeNode::iterator DI = pdtNode->begin(), DE = pdtNode->end(); DI != DE; ++DI)
                {
                    const SVFBasicBlock* dombb = getSVFBasicBlock((*DI)->getBlock());
                    ld->getPostDomTreeMap()[svf_bb].insert(dombb);
                }
            }
            else
            {
                ld->getPostDomTreeMap()[svf_bb] = Set<const SVFBasicBlock* >();
            }
        }
        if (const Loop *loop = loopInfo.getLoopFor(bb))
        {
            for (BasicBlock* loopBlock:loop->getBlocks())
            {
                const SVFBasicBlock* loopbb = getSVFBasicBlock(loopBlock);
                ld->addToBB2LoopMap(svf_bb,loopbb);
            }
        }
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
    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
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
                    queue.push(LLVMGlobalFunction(priority
                                                  ->getZExtValue(),
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
    std::vector<const Function* > ctor_funcs;
    std::vector<const Function* > dtor_funcs;
    Function*  orgMain = 0;
    Module* mainMod = nullptr;

    for (Module &mod : modules)
    {
        // Collect ctor and dtor functions
        for (Module::global_iterator it = mod.global_begin(), eit = mod.global_end(); it != eit; ++it)
        {
            const GlobalVariable *global = &*it;

            if (global->getName().equals(SVF_GLOBAL_CTORS) &&
                    global->hasInitializer())
            {
                ctor_funcs = getLLVMGlobalFunctions(global);
            }
            else if (global->getName().equals(SVF_GLOBAL_DTORS) &&
                     global->hasInitializer())
            {
                dtor_funcs = getLLVMGlobalFunctions(global);
            }
        }

        // Find main function
        for (auto &func : mod)
        {
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
            (ctor_funcs.size() > 0 || dtor_funcs.size() > 0))
    {
        assert(mainMod && "Module with main function not found.");
        Module & M = *mainMod;
        // char **
        Type*  i8ptr2 = PointerType::getInt8PtrTy(M.getContext())->getPointerTo();
        Type*  i32 = IntegerType::getInt32Ty(M.getContext());
        // define void @svf.main(i32, i8**, i8**)
#if (LLVM_VERSION_MAJOR >= 9)
        FunctionCallee svfmainFn = M.getOrInsertFunction(
                                       SVF_MAIN_FUNC_NAME,
                                       Type::getVoidTy(M.getContext()),
                                       i32,i8ptr2,i8ptr2
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
        for(auto & ctor: ctor_funcs)
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
        Value*  args[] = {arg_it, arg_it + 1, arg_it + 2 };
        size_t cnt = orgMain->arg_size();
        assert(cnt <= 3 && "Too many arguments for main()");
        Builder.CreateCall(orgMain, llvm::ArrayRef<Value*>(args,args + cnt));
        // emit "call void @dtor()". dtor_funcs is sorted so the functions are
        // emitted in the order of priority
        for (auto &dtor : dtor_funcs)
        {
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
    Set<const Function*> funDecls, funDefs;
    OrderedSet<string> declNames, defNames, intersectNames;
    typedef Map<string, const Function*> NameToFunDefMapTy;
    typedef Map<string, Set<const Function*>> NameToFunDeclsMapTy;

    for (Module& mod : modules)
    {
        /// Function
        for (Module::iterator it = mod.begin(), eit = mod.end(); it != eit; ++it)
        {
            const Function* fun = &*it;
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
    for (Set<const Function*>::iterator it = funDefs.begin(),
            eit = funDefs.end(); it != eit; ++it)
    {
        const Function* fdef = *it;
        string funName = fdef->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        nameToFunDefMap[funName] = fdef;
    }

    ///// name to decls map
    NameToFunDeclsMapTy nameToFunDeclsMap;
    for (Set<const Function*>::iterator it = funDecls.begin(),
            eit = funDecls.end(); it != eit; ++it)
    {
        const Function* fdecl = *it;
        string funName = fdecl->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDeclsMapTy::iterator mit = nameToFunDeclsMap.find(funName);
        if (mit == nameToFunDeclsMap.end())
        {
            Set<const Function*> decls;
            decls.insert(fdecl);
            nameToFunDeclsMap[funName] = decls;
        }
        else
        {
            Set<const Function*> &decls = mit->second;
            decls.insert(fdecl);
        }
    }

    /// Fun decl --> def
    for (Set<const Function*>::iterator it = funDecls.begin(),
            eit = funDecls.end(); it != eit; ++it)
    {
        const Function* fdecl = *it;
        string funName = fdecl->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDefMapTy::iterator mit = nameToFunDefMap.find(funName);
        if (mit == nameToFunDefMap.end())
            continue;
        FunDeclToDefMap[fdecl] = mit->second;
    }

    /// Fun def --> decls
    for (Set<const Function*>::iterator it = funDefs.begin(),
            eit = funDefs.end(); it != eit; ++it)
    {
        const Function* fdef = *it;
        string funName = fdef->getName().str();
        if (intersectNames.find(funName) == intersectNames.end())
            continue;
        NameToFunDeclsMapTy::iterator mit = nameToFunDeclsMap.find(funName);
        if (mit == nameToFunDeclsMap.end())
            continue;
        std::vector<const Function*>& decls = FunDefToDeclsMap[fdef];
        for (Set<const Function*>::iterator sit = mit->second.begin(),
                seit = mit->second.end(); sit != seit; ++sit)
        {
            decls.push_back(*sit);
        }
    }
}

void LLVMModuleSet::buildGlobalDefToRepMap()
{
    typedef Map<string, Set<GlobalVariable*>> NameToGlobalsMapTy;
    NameToGlobalsMapTy nameToGlobalsMap;
    for (Module &mod : modules)
    {
        // Collect ctor and dtor functions
        for (Module::global_iterator it = mod.global_begin(), eit = mod.global_end(); it != eit; ++it)
        {
            GlobalVariable *global = &*it;
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

void LLVMModuleSet::setValueAttr(const Value* val, SVFValue* svfvalue)
{
    SVFValue2LLVMValue[svfvalue] = val;

    if (LLVMUtil::isPtrInUncalledFunction(val))
        svfvalue->setPtrInUncalledFunction();
    if (LLVMUtil::isConstDataOrAggData(val))
        svfvalue->setConstDataOrAggData();

    if (SVFGlobalValue* glob = SVFUtil::dyn_cast<SVFGlobalValue>(svfvalue))
    {
        const Value* llvmVal = LLVMUtil::getGlobalRep(val);
        assert(SVFUtil::isa<GlobalValue>(llvmVal) && "not a GlobalValue?");
        glob->setDefGlobalForMultipleModule(getSVFGlobalValue(SVFUtil::cast<GlobalValue>(llvmVal)));
    }
    if (SVFFunction* svffun = SVFUtil::dyn_cast<SVFFunction>(svfvalue))
    {
        const Function* func = SVFUtil::cast<Function>(val);
        svffun->setIsNotRet(LLVMUtil::functionDoesNotRet(func));
        svffun->setIsUncalledFunction(LLVMUtil::isUncalledFunction(func));
        svffun->setDefFunForMultipleModule(getSVFFunction(LLVMUtil::getDefFunForMultipleModule(func)));
    }

    svfvalue->setSourceLoc(LLVMUtil::getSourceLoc(val));
}

SVFConstantData* LLVMModuleSet::getSVFConstantData(const ConstantData* cd)
{
    LLVMConst2SVFConstMap::const_iterator it = LLVMConst2SVFConst.find(cd);
    if(it!=LLVMConst2SVFConst.end())
    {
        assert(SVFUtil::isa<SVFConstantData>(it->second) && "not a SVFConstantData type!");
        return SVFUtil::cast<SVFConstantData>(it->second);
    }
    else
    {
        SVFConstantData* svfcd = nullptr;
        if(const ConstantInt* cint = SVFUtil::dyn_cast<ConstantInt>(cd))
            svfcd = new SVFConstantInt(cd->getName().str(), getSVFType(cint->getType()), cint->getZExtValue(),cint->getSExtValue());
        else if(const ConstantFP* cfp = SVFUtil::dyn_cast<ConstantFP>(cd))
        {
            double dval = 0;
            if(cfp->isNormalFP() &&  (&cfp->getValueAPF().getSemantics()== &llvm::APFloatBase::IEEEdouble()))
                dval =  cfp->getValueAPF().convertToDouble();
            svfcd = new SVFConstantFP(cd->getName().str(), getSVFType(cd->getType()), dval);
        }
        else if(SVFUtil::isa<ConstantPointerNull>(cd))
            svfcd = new SVFConstantNullPtr(cd->getName().str(), getSVFType(cd->getType()));
        else if (SVFUtil::isa<UndefValue>(cd))
            svfcd = new SVFBlackHoleValue(cd->getName().str(), getSVFType(cd->getType()));
        else
            svfcd = new SVFConstantData(cd->getName().str(), getSVFType(cd->getType()));
        svfModule->addConstant(svfcd);
        addConstantDataMap(cd,svfcd);
        return svfcd;
    }
}

SVFConstant* LLVMModuleSet::getOtherSVFConstant(const Constant* oc)
{
    LLVMConst2SVFConstMap::const_iterator it = LLVMConst2SVFConst.find(oc);
    if(it!=LLVMConst2SVFConst.end())
    {
        return it->second;
    }
    else
    {
        SVFConstant* svfoc = new SVFConstant(oc->getName().str(), getSVFType(oc->getType()));
        svfModule->addConstant(svfoc);
        addOtherConstantMap(oc,svfoc);
        return svfoc;
    }
}

SVFOtherValue* LLVMModuleSet::getSVFOtherValue(const Value* ov)
{
    LLVMValue2SVFOtherValueMap::const_iterator it = LLVMValue2SVFOtherValue.find(ov);
    if(it!=LLVMValue2SVFOtherValue.end())
    {
        return it->second;
    }
    else
    {
        SVFOtherValue* svfov = nullptr;
        if(SVFUtil::isa<MetadataAsValue>(ov))
            svfov = new SVFMetadataAsValue(ov->getName().str(), getSVFType(ov->getType()));
        else
            svfov = new SVFOtherValue(ov->getName().str(), getSVFType(ov->getType()));
        svfModule->addOtherValue(svfov);
        addOtherValueMap(ov,svfov);
        return svfov;
    }
}

SVFValue* LLVMModuleSet::getSVFValue(const Value* value)
{
    if (const Function* fun = SVFUtil::dyn_cast<Function>(value))
        return getSVFFunction(fun);
    else if (const BasicBlock* bb = SVFUtil::dyn_cast<BasicBlock>(value))
        return getSVFBasicBlock(bb);
    else if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(value))
        return getSVFInstruction(inst);
    else if (const Argument* arg = SVFUtil::dyn_cast<Argument>(value))
        return getSVFArgument(arg);
    else if (const Constant* cons = SVFUtil::dyn_cast<Constant>(value))
    {
        if (const ConstantData* cd = SVFUtil::dyn_cast<ConstantData>(cons))
            return getSVFConstantData(cd);
        else if (const GlobalValue* glob = SVFUtil::dyn_cast<GlobalValue>(cons))
            return getSVFGlobalValue(glob);
        else
            return getOtherSVFConstant(cons);
    }
    else
        return getSVFOtherValue(value);
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
    assert(T);
    LLVMType2SVFTypeMap::const_iterator it = LLVMType2SVFType.find(T);
    if (it!=LLVMType2SVFType.end())
        return it->second;
    else
    {
        SVFType* svfType = addSVFTypeInfo(T);
        StInfo* stinfo = collectTypeInfo(T);
        svfType->setTypeInfo(stinfo);
        /// TODO: set the void* to every element for now (imprecise)
        /// For example, [getPointerTo(): char ----> i8*] [getPointerTo(): int ----> i8*] [getPointerTo(): struct ----> i8*]
        PointerType* ptrTy = PointerType::getInt8PtrTy(getContext())->getPointerTo();
        svfType->setPointerTo(SVFUtil::cast<SVFPointerType>(getSVFType(ptrTy)));
        return svfType;
    }
}

StInfo* LLVMModuleSet::collectTypeInfo(const Type* T)
{
    StInfo* stinfo = nullptr;

    Type2TypeInfoMap::iterator tit = Type2TypeInfo.find(T);
    if(tit!=Type2TypeInfo.end())
    {
        stinfo = tit->second.get();
    }
    else
    {
        if (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(T))
        {
            stinfo = collectArrayInfo(aty);
            Type2TypeInfo[T] = std::unique_ptr<StInfo>(stinfo);
        }
        else if (const StructType* sty = SVFUtil::dyn_cast<StructType>(T))
        {
            u32_t nf;
            stinfo = collectStructInfo(sty, nf);
            Type2TypeInfo[T] = std::unique_ptr<StInfo>(stinfo);
            //Record the size of the complete struct and update max_struct.
            if (nf > symInfo->maxStSize)
            {
                symInfo->maxStruct = getSVFType(sty);
                symInfo->maxStSize = nf;
            }
        }
        else
        {
            /// The simple type info should not be processed before
            auto stinfo_own = std::make_unique<StInfo>(1);
            stinfo = stinfo_own.get();
            Type2TypeInfo[T] = std::move(stinfo_own);
            collectSimpleTypeInfo(stinfo, T);
        }


    }
    return stinfo;
}

SVFType* LLVMModuleSet::addSVFTypeInfo(const Type* T)
{
    assert(LLVMType2SVFType.find(T)==LLVMType2SVFType.end() && "SVFType has been added before");

    SVFType* svftype = nullptr;
    if (const PointerType* pt = SVFUtil::dyn_cast<PointerType>(T))
        svftype = new SVFPointerType(getSVFType(LLVMUtil::getPtrElementType(pt)));
    else if (SVFUtil::isa<IntegerType>(T))
        svftype = new SVFIntergerType();
    else if (const FunctionType* ft = SVFUtil::dyn_cast<FunctionType>(T))
        svftype = new SVFFunctionType(getSVFType(ft->getReturnType()));
    else if (SVFUtil::isa<StructType>(T))
        svftype = new SVFStructType();
    else if (SVFUtil::isa<ArrayType>(T))
        svftype = new SVFArrayType();
    else
        svftype = new SVFOtherType(T->isSingleValueType());
    symInfo->addTypeInfo(svftype);
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

    StInfo* stinfo = new StInfo(totalElemNum);

    /// array without any element (this is not true in C/C++ arrays) we assume there is an empty dummy element
    if(totalElemNum==0)
    {
        stinfo->addFldWithType(0, getSVFType(elemTy), 0);
        stinfo->setNumOfFieldsAndElems(1, 1);
        stinfo->getFlattenFieldTypes().push_back(getSVFType(elemTy));
        stinfo->getFlattenElementTypes().push_back(getSVFType(elemTy));
        return stinfo;
    }

    /// Array's flatten field infor is the same as its element's
    /// flatten infor.
    StInfo* elemStInfo = collectTypeInfo(elemTy);
    u32_t nfE = elemStInfo->getNumOfFlattenFields();
    for (u32_t j = 0; j < nfE; j++)
    {
        const SVFType* fieldTy = elemStInfo->getFlattenFieldTypes()[j];
        stinfo->getFlattenFieldTypes().push_back(fieldTy);
    }

    /// Flatten arrays, map each array element index `i` to flattened index `(i * nfE * totalElemNum)/outArrayElemNum`
    /// nfE>1 if the array element is a struct with more than one field.
    u32_t outArrayElemNum = ty->getNumElements();
    for(u32_t i = 0; i < outArrayElemNum; i++)
        stinfo->addFldWithType(0, getSVFType(elemTy), (i * nfE * totalElemNum)/outArrayElemNum);

    for(u32_t i = 0; i < totalElemNum; i++)
    {
        for(u32_t j = 0; j < nfE; j++)
        {
            stinfo->getFlattenElementTypes().push_back(elemStInfo->getFlattenFieldTypes()[j]);
        }
    }

    assert(stinfo->getFlattenElementTypes().size() == nfE * totalElemNum && "typeForArray size incorrect!!!");
    stinfo->setNumOfFieldsAndElems(nfE, nfE * totalElemNum);

    return stinfo;
}


/*!
 * Fill in struct_info for T.
 * Given a Struct type, we recursively extend and record its fields and types.
 */
StInfo* LLVMModuleSet::collectStructInfo(const StructType *sty, u32_t &nf)
{
    /// The struct info should not be processed before
    StInfo* stinfo = new StInfo(1);

    // Number of fields after flattening the struct
    nf = 0;
    // The offset when considering array stride info
    u32_t strideOffset = 0;
    for (StructType::element_iterator it = sty->element_begin(), ie =
                sty->element_end(); it != ie; ++it)
    {
        const Type* et = *it;
        /// offset with int_32 (s32_t) is large enough and will not cause overflow
        stinfo->addFldWithType(nf, getSVFType(et), strideOffset);

        if (SVFUtil::isa<StructType, ArrayType>(et))
        {
            StInfo * subStinfo = collectTypeInfo(et);
            u32_t nfE = subStinfo->getNumOfFlattenFields();
            //Copy ST's info, whose element 0 is the size of ST itself.
            for (u32_t j = 0; j < nfE; j++)
            {
                const SVFType* elemTy = subStinfo->getFlattenFieldTypes()[j];
                stinfo->getFlattenFieldTypes().push_back(elemTy);
            }
            nf += nfE;
            strideOffset += nfE * subStinfo->getStride();
            for(u32_t tpi = 0; tpi < subStinfo->getStride(); tpi++)
            {
                for(u32_t tpj = 0; tpj < nfE; tpj++)
                {
                    stinfo->getFlattenElementTypes().push_back(subStinfo->getFlattenFieldTypes()[tpj]);
                }
            }
        }
        else     //simple type
        {
            nf += 1;
            strideOffset += 1;
            stinfo->getFlattenFieldTypes().push_back(getSVFType(et));
            stinfo->getFlattenElementTypes().push_back(getSVFType(et));
        }
    }

    assert(stinfo->getFlattenElementTypes().size() == strideOffset && "typeForStruct size incorrect!");
    stinfo->setNumOfFieldsAndElems(nf,strideOffset);

    return stinfo;
}


/*!
 * Collect simple type (non-aggregate) info
 */
StInfo* LLVMModuleSet::collectSimpleTypeInfo(StInfo * stinfo, const Type* ty)
{
    /// Only one field
    stinfo->addFldWithType(0, getSVFType(ty), 0);

    stinfo->getFlattenFieldTypes().push_back(getSVFType(ty));
    stinfo->getFlattenElementTypes().push_back(getSVFType(ty));
    stinfo->setNumOfFieldsAndElems(1,1);

    return stinfo;
}