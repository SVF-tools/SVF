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
 * SVFModule.cpp
 *
 *  Created on: Aug 4, 2017
 *      Author: Xiaokang Fan
 */

#include <queue>
#include <algorithm>
#include "SVFIR/SVFModule.h"
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
    : symInfo(SymbolTableInfo::SymbolInfo()),
      svfModule(SVFModule::getSVFModule()), typeInference(new ObjTypeInference())
{
}

LLVMModuleSet::~LLVMModuleSet()
{
    for (auto& item : LLVMInst2SVFInst)
    {
        delete item.second;
        item.second = nullptr;
    }
    delete typeInference;
    typeInference = nullptr;
}

ObjTypeInference* LLVMModuleSet::getTypeInference()
{
    return typeInference;
}

SVFModule* LLVMModuleSet::buildSVFModule(Module &mod)
{
    LLVMModuleSet* mset = getLLVMModuleSet();

    double startSVFModuleTime = SVFStat::getClk(true);
    SVFModule::getSVFModule()->setModuleIdentifier(mod.getModuleIdentifier());
    mset->modules.emplace_back(mod);    // Populates `modules`; can get context via `this->getContext()`
    mset->loadExtAPIModules();          // Uses context from module through `this->getContext()`
    mset->build();
    double endSVFModuleTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingLLVMModule = (endSVFModuleTime - startSVFModuleTime)/TIMEINTERVAL;

    mset->buildSymbolTable();
    // Don't releaseLLVMModuleSet() here, as IRBuilder might still need LLVMMoudleSet
    return SVFModule::getSVFModule();
}

SVFModule* LLVMModuleSet::buildSVFModule(const std::vector<std::string> &moduleNameVec)
{
    double startSVFModuleTime = SVFStat::getClk(true);

    LLVMModuleSet* mset = getLLVMModuleSet();

    mset->loadModules(moduleNameVec);   // Populates `modules`; can get context via `this->getContext()`
    mset->loadExtAPIModules();          // Uses context from first module through `this->getContext()`

    if (!moduleNameVec.empty())
    {
        SVFModule::getSVFModule()->setModuleIdentifier(moduleNameVec.front());
    }

    mset->build();

    double endSVFModuleTime = SVFStat::getClk(true);
    SVFStat::timeOfBuildingLLVMModule =
        (endSVFModuleTime - startSVFModuleTime) / TIMEINTERVAL;

    mset->buildSymbolTable();
    // Don't releaseLLVMModuleSet() here, as IRBuilder might still need LLVMMoudleSet
    return SVFModule::getSVFModule();
}

void LLVMModuleSet::buildSymbolTable() const
{
    double startSymInfoTime = SVFStat::getClk(true);
    if (!SVFModule::pagReadFromTXT())
    {
        /// building symbol table
        DBOUT(DGENERAL, SVFUtil::outs() << SVFUtil::pasMsg("Building Symbol table ...\n"));
        SymbolTableBuilder builder(symInfo);
        builder.buildMemModel(svfModule);
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
    initSVFFunction();
    ICFGBuilder icfgbuilder;
    icfg = icfgbuilder.build();

    CallGraphBuilder callGraphBuilder;
    callgraph = callGraphBuilder.buildSVFIRCallGraph(svfModule);

    for (const auto& it : *callgraph)
    {
        addFunctionMap(
            SVFUtil::cast<Function>(getLLVMValue(it.second->getFunction())),
            it.second);
    }
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
        createSVFFunction(func);
    }
    for (const Function* func: candidateDecls)
    {
        createSVFFunction(func);
    }

    // Store annotations of functions in extapi.bc
    for (const auto& pair : ExtFun2Annotations)
    {
        const SVFFunction* svffun = getSVFFunction(pair.first);
        ExtAPI::getExtAPI()->setExtFuncAnnotations(svffun, pair.second);
    }

    /// then traverse candidate sets
    for (const Module& mod : modules)
    {
        /// GlobalVariable
        for (const GlobalVariable& global :  mod.globals())
        {
            SVFGlobalValue* svfglobal = new SVFGlobalValue(
                global.getName().str(), getSVFType(global.getType()));
            svfModule->addGlobalSet(svfglobal);
            addGlobalValueMap(&global, svfglobal);
        }

        /// GlobalAlias
        for (const GlobalAlias& alias : mod.aliases())
        {
            SVFGlobalValue* svfalias = new SVFGlobalValue(
                alias.getName().str(), getSVFType(alias.getType()));
            svfModule->addAliasSet(svfalias);
            addGlobalValueMap(&alias, svfalias);
        }

        /// GlobalIFunc
        for (const GlobalIFunc& ifunc : mod.ifuncs())
        {
            SVFGlobalValue* svfifunc = new SVFGlobalValue(
                ifunc.getName().str(), getSVFType(ifunc.getType()));
            svfModule->addAliasSet(svfifunc);
            addGlobalValueMap(&ifunc, svfifunc);
        }
    }
}

void LLVMModuleSet::createSVFFunction(const Function* func)
{
    SVFFunction* svfFunc = new SVFFunction(
        getSVFType(func->getType()),
        SVFUtil::cast<SVFFunctionType>(
            getSVFType(func->getFunctionType())),
        func->isDeclaration(), LLVMUtil::isIntrinsicFun(func),
        func->hasAddressTaken(), func->isVarArg(), new SVFLoopAndDomInfo);
    svfModule->addFunctionSet(svfFunc);
    addFunctionMap(func, svfFunc);

    for (const Argument& arg : func->args())
    {
        SVFArgument* svfarg = new SVFArgument(
            getSVFType(arg.getType()), svfFunc, arg.getArgNo(),
            LLVMUtil::isArgOfUncalledFunction(&arg));
        // Setting up arg name
        if (!arg.hasName())
            svfarg->setName(std::to_string(arg.getArgNo()));

        svfFunc->addArgument(svfarg);
        addArgumentMap(&arg, svfarg);
    }

    for (const BasicBlock& bb : *func)
    {
        SVFBasicBlock* svfBB =
            new SVFBasicBlock(getSVFType(bb.getType()), svfFunc);
        svfFunc->addBasicBlock(svfBB);
        addBasicBlockMap(&bb, svfBB);
        for (const Instruction& inst : bb)
        {
            SVFInstruction* svfInst = nullptr;
            if (const CallBase* call = SVFUtil::dyn_cast<CallBase>(&inst))
            {
                if (cppUtil::isVirtualCallSite(call))
                    svfInst = new SVFVirtualCallInst(
                        getSVFType(call->getType()), svfBB,
                        call->getFunctionType()->isVarArg(),
                        inst.isTerminator());
                else
                    svfInst = new SVFCallInst(
                        getSVFType(call->getType()), svfBB,
                        call->getFunctionType()->isVarArg(),
                        inst.isTerminator());
            }
            else
            {
                svfInst =
                    new SVFInstruction(getSVFType(inst.getType()),
                                       svfBB, inst.isTerminator(),
                                       SVFUtil::isa<ReturnInst>(inst));
            }

            addInstructionMap(&inst, svfInst);
        }
    }
}

void LLVMModuleSet::initSVFFunction()
{
    for (Module& mod : modules)
    {
        /// Function
        for (const Function& f : mod.functions())
        {
            SVFFunction* svffun = getSVFFunction(&f);
            initSVFBasicBlock(&f);

            if (!SVFUtil::isExtCall(svffun))
            {
                initDomTree(svffun, &f);
            }
        }
    }
}

void LLVMModuleSet::initSVFBasicBlock(const Function* func)
{
    SVFFunction *svfFun = getSVFFunction(func);
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

        /// set exit block: exit basic block must have no successors and have a return instruction
        if (svfbb->getSuccessors().empty())
        {
            if (LLVMUtil::basicBlockHasRetInst(bb))
            {
                assert((LLVMUtil::functionDoesNotRet(func) ||
                        SVFUtil::isa<ReturnInst>(bb->back())) &&
                       "last inst must be return inst");
                svfFun->setExitBlock(svfbb);
            }
        }

        for (BasicBlock::const_iterator iit = bb->begin(), eiit = bb->end(); iit != eiit; ++iit)
        {
            const Instruction* inst = &*iit;
            if(const CallBase* call = SVFUtil::dyn_cast<CallBase>(inst))
            {
                SVFInstruction* svfinst = getSVFInstruction(call);
                SVFCallInst* svfcall = SVFUtil::cast<SVFCallInst>(svfinst);
                auto called_llvmval = call->getCalledOperand()->stripPointerCasts();
                if (const Function* called_llvmfunc = SVFUtil::dyn_cast<Function>(called_llvmval))
                {
                    SVFFunction* callee = getSVFFunction(called_llvmfunc);
                    svfcall->setCalledOperand(callee);
                }
                else
                {
                    svfcall->setCalledOperand(getSVFValue(called_llvmval));
                }
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
        }
    }
    // For no return functions, we set the last block as exit BB
    // This ensures that each function that has definition must have an exit BB
    if (svfFun->exitBlock == nullptr && svfFun->hasBasicBlock())
    {
        SVFBasicBlock* retBB = const_cast<SVFBasicBlock*>(svfFun->back());
        assert((LLVMUtil::functionDoesNotRet(func) ||
                SVFUtil::isa<ReturnInst>(&func->back().back())) &&
               "last inst must be return inst");
        svfFun->setExitBlock(retBB);
    }
}


void LLVMModuleSet::initDomTree(SVFFunction* svffun, const Function* fun)
{
    if (fun->isDeclaration())
        return;
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

    for (Function::const_iterator bit = fun->begin(), beit = fun->end(); bit!=beit; ++bit)
    {
        const BasicBlock &bb = *bit;
        SVFBasicBlock* svfBB = getSVFBasicBlock(&bb);
        if (DomTreeNode* dtNode = dt.getNode(&bb))
        {
            SVFLoopAndDomInfo::BBSet& bbSet = ld->getDomTreeMap()[svfBB];
            for (const auto domBB : *dtNode)
            {
                const auto* domSVFBB = getSVFBasicBlock(domBB->getBlock());
                bbSet.insert(domSVFBB);
            }
        }

        if (DomTreeNode* pdtNode = pdt.getNode(&bb))
        {
            u32_t level = pdtNode->getLevel();
            ld->getBBPDomLevel()[svfBB] = level;
            BasicBlock* idomBB = pdtNode->getIDom()->getBlock();
            const SVFBasicBlock* idom = idomBB == NULL ? NULL: getSVFBasicBlock(idomBB);
            ld->getBB2PIdom()[svfBB] = idom;

            SVFLoopAndDomInfo::BBSet& bbSet = ld->getPostDomTreeMap()[svfBB];
            for (const auto domBB : *pdtNode)
            {
                const auto* domSVFBB = getSVFBasicBlock(domBB->getBlock());
                bbSet.insert(domSVFBB);
            }
        }

        if (const Loop* loop = loopInfo.getLoopFor(&bb))
        {
            for (const BasicBlock* loopBlock : loop->getBlocks())
            {
                const SVFBasicBlock* loopbb = getSVFBasicBlock(loopBlock);
                ld->addToBB2LoopMap(svfBB, loopbb);
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
        SVFModule::setPagFromTXT(Options::Graphtxt());

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
            // Clone the body of extFunToClone into clonedFunction
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

void LLVMModuleSet::addFunctionMap(const SVF::Function* func, SVF::CallGraphNode* svfFunc)
{
    LLVMFunc2CallGraphNode[func] = svfFunc;
    setValueAttr(func,svfFunc);
}

void LLVMModuleSet::setValueAttr(const Value* val, SVFValue* svfvalue)
{
    SVFValue2LLVMValue[svfvalue] = val;

    if(val->hasName())
        svfvalue->setName(val->getName().str());
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
        svffun->setDefFunForMultipleModule(getSVFFunction(func));
    }

    svfvalue->setSourceLoc(LLVMUtil::getSourceLoc(val));
}

void LLVMModuleSet::setValueAttr(const SVF::Value* val, SVF::SVFBaseNode* svfBaseNode)
{
    SVFBaseNode2LLVMValue[svfBaseNode] = val;
    svfBaseNode->setSourceLoc(LLVMUtil::getSourceLoc(val));
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
        {
            /// bitwidth == 1 : cint has value from getZExtValue() because `bool true` will be translated to -1 using sign extension (i.e., getSExtValue).
            /// bitwidth <=64 1 : cint has value from getSExtValue()
            /// bitwidth >64 1 : cint has value 0 because it represents an invalid int
            if(cint->getBitWidth() == 1)
                svfcd = new SVFConstantInt(getSVFType(cint->getType()), cint->getZExtValue(), cint->getZExtValue());
            else if(cint->getBitWidth() <= 64 && cint->getBitWidth() > 1)
                svfcd = new SVFConstantInt(getSVFType(cint->getType()), cint->getZExtValue(), cint->getSExtValue());
            else
                svfcd = new SVFConstantInt(getSVFType(cint->getType()), 0, 0);
        }
        else if(const ConstantFP* cfp = SVFUtil::dyn_cast<ConstantFP>(cd))
        {
            double dval = 0;
            // TODO: Why only double is considered? What about float?
            if (cfp->isNormalFP())
            {
                const llvm::fltSemantics& semantics = cfp->getValueAPF().getSemantics();
                if (&semantics == &llvm::APFloat::IEEEhalf() ||
                        &semantics == &llvm::APFloat::IEEEsingle() ||
                        &semantics == &llvm::APFloat::IEEEdouble() ||
                        &semantics == &llvm::APFloat::IEEEquad() ||
                        &semantics == &llvm::APFloat::x87DoubleExtended())
                {
                    dval = cfp->getValueAPF().convertToDouble();
                }
                else
                {
                    assert (false && "Unsupported floating point type");
                    abort();
                }
            }
            else
            {
                // other cfp type, like isZero(), isInfinity(), isNegative(), etc.
                // do nothing
            }
            svfcd = new SVFConstantFP(getSVFType(cd->getType()), dval);
        }
        else if(SVFUtil::isa<ConstantPointerNull>(cd))
            svfcd = new SVFConstantNullPtr(getSVFType(cd->getType()));
        else if (SVFUtil::isa<UndefValue>(cd))
            svfcd = new SVFBlackHoleValue(getSVFType(cd->getType()));
        else
            svfcd = new SVFConstantData(getSVFType(cd->getType()));


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
        SVFConstant* svfoc = new SVFConstant(getSVFType(oc->getType()));
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
        SVFOtherValue* svfov =
            SVFUtil::isa<MetadataAsValue>(ov)
            ? new SVFMetadataAsValue(getSVFType(ov->getType()))
            : new SVFOtherValue(getSVFType(ov->getType()));
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
        if (nf > symInfo->maxStSize)
        {
            symInfo->maxStruct = getSVFType(sty);
            symInfo->maxStSize = nf;
        }
    }
    else
    {
        stInfo = collectSimpleTypeInfo(T);
    }
    Type2TypeInfo.emplace(T, stInfo);
    symInfo->addStInfo(stInfo);
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
    if (SVFUtil::isa<PointerType>(T))
    {
        svftype = new SVFPointerType(byteSize);
    }
    else if (const IntegerType* intT = SVFUtil::dyn_cast<IntegerType>(T))
    {
        auto svfIntT = new SVFIntegerType(byteSize);
        unsigned signWidth = intT->getBitWidth();
        assert(signWidth < INT16_MAX && "Integer width too big");
        svfIntT->setSignAndWidth(intT->getSignBit() ? -signWidth : signWidth);
        svftype = svfIntT;
    }
    else if (const FunctionType* ft = SVFUtil::dyn_cast<FunctionType>(T))
        svftype = new SVFFunctionType(getSVFType(ft->getReturnType()));
    else if (const StructType* st = SVFUtil::dyn_cast<StructType>(T))
    {
        auto svfst = new SVFStructType(byteSize);
        if (st->hasName())
            svfst->setName(st->getName().str());
        svftype = svfst;
    }
    else if (const auto at = SVFUtil::dyn_cast<ArrayType>(T))
    {
        auto svfat = new SVFArrayType(byteSize);
        svfat->setNumOfElement(at->getNumElements());
        svfat->setTypeOfElement(getSVFType(at->getElementType()));
        svftype = svfat;
    }
    else
    {
        std::string buffer;
        auto ot = new SVFOtherType(T->isSingleValueType(), byteSize);
        llvm::raw_string_ostream(buffer) << *T;
        ot->setRepr(std::move(buffer));
        svftype = ot;
    }

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
