//===- WPAPass.cpp -- Whole program analysis pass------------------------------//
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
//===-----------------------------------------------------------------------===//

/*
 * @file: WPA.cpp
 * @author: yesen
 * @date: 10/06/2014
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */


#include "Util/Options.h"
#include "Util/SVFModule.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "WPA/WPAPass.h"
#include "WPA/Andersen.h"
#include "WPA/AndersenSFR.h"
#include "WPA/FlowSensitive.h"
#include "WPA/FlowSensitiveTBHC.h"
#include "WPA/VersionedFlowSensitive.h"
#include "WPA/TypeAnalysis.h"
#include "WPA/Steensgaard.h"
#include "SVF-FE/PAGBuilder.h"
#include "WPA/InvariantHandler.h"
#include "WPA/Debugger.h"

//#include "InsertFunctionSwitch.h"
//#include "InsertExecutionSwitch.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Transforms/Utils/FunctionComparator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/LegacyPassManager.h"
#include "WPA/LoopInfoConsolidatorPass.h"

#include "llvm/Analysis/LoopInfo.h"


#include <iostream>
using namespace SVF;

char WPAPass::ID = 0;

static llvm::RegisterPass<WPAPass> WHOLEPROGRAMPA("wpa",
        "Whole Program Pointer AnalysWPAis Pass");


/*!
 * Destructor
 */
WPAPass::~WPAPass()
{
    PTAVector::const_iterator it = ptaVector.begin();
    PTAVector::const_iterator eit = ptaVector.end();
    for (; it != eit; ++it)
    {
        PointerAnalysis* pta = *it;
        delete pta;
    }
    ptaVector.clear();
}

/*
CallInst* WPAPass::findCorrespondingCallInClone(CallInst* indCall, llvm::Module* clonedModule) {
    int indCallIndex = -1;
    Function* origFunc = indCall->getFunction();
    llvm::StringRef indCallFuncName = origFunc->getName();
    for (inst_iterator I = llvm::inst_begin(origFunc), E = llvm::inst_end(origFunc); I != E; ++I) {
        if (CallInst* callInst = SVFUtil::dyn_cast<CallInst>(&*I)) {
            if (callInst->isIndirectCall()) {
                indCallIndex++;
                if (indCall == callInst) {
                    break;
                }
            }
        }
    }

    int indexIt = -1;
    // Now find that same function and call-index in the cloned
    Function* clonedFunc = clonedModule->getFunction(indCallFuncName);
    for (inst_iterator I = llvm::inst_begin(clonedFunc), E = llvm::inst_end(clonedFunc); I != E; ++I) {
        if (CallInst* callInst = SVFUtil::dyn_cast<CallInst>(&*I)) {
            if (callInst->isIndirectCall()) {
                indexIt++;
                if (indexIt == indCallIndex) {
                    return callInst;
                }
            }
        }
    }
    return nullptr;
}
*/

void WPAPass::collectCFI(SVFModule* svfModule, Module& M, bool woInv) {
    
    std::map<const CallInst*, std::set<const Function*>>* indCallMap;
    std::set<const CallInst*>* indCallProhibited;

    std::map<int, int>* histogram;

    if (!woInv) {
        indCallMap = &wInvIndCallMap;
        indCallProhibited = &wInvIndCallProhibited;
        histogram = &wInvHistogram;
    } else {
        indCallMap = &woInvIndCallMap;
        indCallProhibited = &woInvIndCallProhibited;
        histogram = &woInvHistogram;
    }


    PAG* pag = _pta->getPAG();
		const PTACallGraph* ptaCallGraph = _pta->getPTACallGraph();
				const PTACallGraph::CallInstToCallGraphEdgesMap& cInstMap = ptaCallGraph->getCallInstToCallGraphEdgesMap();

				for (auto it: cInstMap) {
					const CallBlockNode* callBlockNode = it.first;
					const Instruction* inst = callBlockNode->getCallSite();
					const CallInst* cInst = SVFUtil::dyn_cast<CallInst>(inst);

					assert(cInst && "If not callinst then what?");
					const Function* callerFun = cInst->getParent()->getParent();
 
					if (cInst->isIndirectCall()) {
						//llvm::errs() << "For a callsite in " << callerFun->getName() << " : \n";
						for (auto callGraphEdge: it.second) {
							const SVFFunction* svfFun = callGraphEdge->getDstNode()->getFunction();
							const Function* calledFunction = svfFun->getLLVMFun();
							//llvm::errs() << "    calls " << calledFunction->getName() << "\n";
							(*indCallMap)[cInst].insert(calledFunction);

						}
					}
				}

    (*histogram)[0] = indCallProhibited->size();
    
    for (auto it: *indCallMap) {
        const CallInst* cInst = it.first;
        int sz = it.second.size();
				/*
        llvm::errs() << "[DUMPING IND CALL] Function: " << cInst->getFunction()->getName() << " : ";
        for (const Function* tgt: it.second) {
            llvm::errs() << tgt->getName() << ", ";
        }
				*/
        (*histogram)[sz]++;
        //llvm::errs() << "\n";
    }

		/*
    llvm::errs() << "EC Size:\t Ind. Call-sites\n";
    int totalTgts = 0;
    int totalIndCallSites = 0;
    for (auto it: (*histogram)) {
        llvm::errs() << it.first << " : " << it.second << "\n";
        totalIndCallSites += it.second;
        totalTgts += it.first*it.second;
    }
    llvm::errs() << "Total Ind. Call-sites: " << totalIndCallSites << "\n";
    llvm::errs() << "Total Tgts: " << totalTgts << "\n";
    std::cerr << "Average CFI: " << std::fixed << (float)totalTgts / (float)totalIndCallSites << "\n";
		*/
}

void WPAPass::instrumentCFICheck(llvm::CallInst* indCall) {
    llvm::Module* M = indCall->getParent()->getParent()->getParent();
    Type* longType = IntegerType::get(M->getContext(), 64);
    Type* intType = IntegerType::get(M->getContext(), 32);
    Type* ptrToLongType = PointerType::get(longType, 0);

    // Create an AllocaInst
    IRBuilder Builder(indCall);

    Constant* csIdCons = ConstantInt::get(intType, indCSToIDMap[indCall]);

    Value* tgt = Builder.CreateBitOrPointerCast(indCall->getCalledOperand(), longType);

    Builder.CreateCall(checkCFI, {csIdCons, tgt});


}

/**
 * We can go forward, from a sizeof operator
 * Or backward from a heap allocation call (malloc, etc) to a cast
 * Going backward might have unintended consequences. 
 * How do you know when you stop? void* p = malloc(..); return p;
 *
 * You can track all type-cast operations and track if they type-cast
 * any of the return values
 *
 */
/*
void WPAPass::deriveHeapAllocationTypes(llvm::Module& module) {
    std::vector<CallInst*> callInsts; // functions that return pointers
    std::vector<Instruction*> typeCasts;
    std::map<CallInst*, StructType*> callInstCastToStructs;
    std::map<CallInst*, ArrayType*> callInstCastToArrays;

    std::vector<std::string> mallocCallers;

    mallocCallers.push_back("malloc");
    mallocCallers.push_back("calloc");

    for (Module::iterator MIterator = module.begin(); MIterator != module.end(); MIterator++) {
        if (Function *F = SVFUtil::dyn_cast<Function>(&*MIterator)) {
            std::vector<AllocaInst*> stackVars;
            std::vector<LoadInst*> loadInsts;
            std::vector<StoreInst*> storeInsts;
            if (!F->isDeclaration()) {
                for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
                    if (CallInst* cInst = SVFUtil::dyn_cast<CallInst>(&*I)) {
                        // Does it return a pointer
                        Function* calledFunc = cInst->getCalledFunction();
                        Type* retTy = cInst->getType();
                        if (retTy->isPointerTy()) {
                            callInsts.push_back(cInst);
                        }
                    }
                }
            }
        }
    }

    // Type-casts
    std::vector<Instruction*> workList;
    for (CallInst* cInst: callInsts) {
        workList.push_back(cInst);
        while (!workList.empty()) {
            Instruction* inst = workList.back();
            workList.pop_back();
            bool term = false;
            for (User* u: inst->users()) {
                Instruction* uInst = SVFUtil::dyn_cast<Instruction>(u);
                if (BitCastInst* bcInst = SVFUtil::dyn_cast<BitCastInst>(u)) {
                    Type* destTy = bcInst->getDestTy();
                    if (destTy->isPointerTy()) {
                        Type* ptrTy = destTy->getPointerElementType();
                        if (StructType* stTy = SVFUtil::dyn_cast<StructType>(ptrTy)) {
                            term = true;
                            callInstCastToStructs[cInst] = stTy;
                        } else if (ArrayType* arrTy = SVFUtil::dyn_cast<ArrayType>(ptrTy)) {
                            term = true;
                            callInstCastToArrays[cInst] = arrTy;
                        }
                    }
                }
                if (!term) {
                    workList.push_back(uInst);
                }
            }
        }
    }

    for (auto pair: callInstCastToStructs) {
        CallInst* cInst = pair.first;
        StructType* stTy = pair.second;
        llvm::errs() << "CallInst in function: " << cInst->getFunction()->getName() << " is cast to struct: " << *stTy << "\n";
        
    }

    for (auto pair: callInstCastToArrays) {
        CallInst* cInst = pair.first;
        ArrayType* arrTy = pair.second;
        llvm::errs() << "CallInst in function: " << cInst->getFunction()->getName() << " is cast to array: " << *arrTy << "\n";
        
    }

}
*/

/*!
 * We start from here
 */
void WPAPass::runOnModule(SVFModule* svfModule)
{
    llvm::Module *module = SVF::LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(); 
    //deriveHeapAllocationTypesWithCloning(*module);

    /*
    if (Options::KaliRunTestDriver) {
        invariantInstrumentationDriver(*module);
    } else {
        for (u32_t i = 0; i<= PointerAnalysis::Default_PTA; i++)
        {
            if (Options::PASelected.isSet(i))
                runPointerAnalysis(svfModule, i);
        }
        assert(!ptaVector.empty() && "No pointer analysis is specified.\n");

    }
    */
    runPointerAnalysis(svfModule, 0);


        //module->dump();
    std::error_code EC;
    llvm::raw_fd_ostream OS("instrumented-module.bc", EC,
            llvm::sys::fs::F_None);
    WriteBitcodeToFile(*module, OS);
    OS.flush();
}

/*!
 * We start from here
 */
bool WPAPass::runOnModule(Module& module)
{
    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(module);
    runOnModule(svfModule);
    return false;
}

void WPAPass::invariantInstrumentationDriver(Module& module) {
    PAG* pag = nullptr;
    Andersen* andersen = new Andersen(pag);
    Module::GlobalListType &Globals = module.getGlobalList();
    for (Module::iterator MIterator = module.begin(); MIterator != module.end(); MIterator++) {
        if (Function *F = SVFUtil::dyn_cast<Function>(&*MIterator)) {
            std::vector<AllocaInst*> stackVars;
            std::vector<LoadInst*> loadInsts;
            std::vector<StoreInst*> storeInsts;
            if (!F->isDeclaration()) {
                // Find the AllocaInsts
                for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
                    if (AllocaInst* stackVar = SVFUtil::dyn_cast<AllocaInst>(&*I)) {
                        if (stackVar->hasName() && stackVar->getName() == "retval") {
                            continue;
                        }
                        stackVars.push_back(stackVar);
                    } else if (LoadInst* loadInst = SVFUtil::dyn_cast<LoadInst>(&*I)) {
                        // Is it a pointer
                        if (SVFUtil::isa<PointerType>(loadInst->getType())) {
                            loadInsts.push_back(loadInst);
                        }
                    } else if (StoreInst* storeInst = SVFUtil::dyn_cast<StoreInst>(&*I)) {
                        if (SVFUtil::isa<PointerType>(storeInst->getType())) {
                            storeInsts.push_back(storeInst);
                        }
                    }
                }
                for (LoadInst* loadInst: loadInsts) {
                    /*
                    for (AllocaInst* stackVar: stackVars) {
                        andersen->instrumentInvariant(loadInst, stackVar);
                        break;
                    }
                    break;
                    */
                    for (GlobalVariable& globalVar: Globals) {
                        if (globalVar.getName() == "myGlobal") {
                            andersen->instrumentInvariant(loadInst, &globalVar);
                        }
                    }
                }
            }
        }
    }
}

void WPAPass::initKaleidoscope(Module* module) {
	LLVMContext& C = module->getContext();
	Type* longType = IntegerType::get(module->getContext(), 64);
	Type* intType = IntegerType::get(module->getContext(), 32);

	Function* mainFunction = module->getFunction("main");
	Instruction* inst = mainFunction->getEntryBlock().getFirstNonPHIOrDbg();
	IRBuilder builder(inst);

	FunctionType* kaleidoscopeInitFnTy = FunctionType::get(Type::getVoidTy(module->getContext()), {}, false);
	Function* kaleidoscopeInitFn = Function::Create(kaleidoscopeInitFnTy, Function::ExternalLinkage, "kaleidoscopeInit", module);
	builder.CreateCall(kaleidoscopeInitFn, {});
}

void WPAPass::initializeCFITargets(llvm::Module* module) {
    LLVMContext& C = module->getContext();
    Type* longType = IntegerType::get(module->getContext(), 64);
    Type* intType = IntegerType::get(module->getContext(), 32);

    Function* mainFunction = module->getFunction("main");
    Instruction* inst = mainFunction->getEntryBlock().getFirstNonPHIOrDbg();
    IRBuilder builder(inst);

    for (auto pair: wInvIndCallMap) {
        const CallInst* callInst = pair.first;
        std::set<const Function*>& wInvTgts = pair.second;
        std::set<const Function*>& woInvTgts = woInvIndCallMap[callInst]; // TODO: does it need to have it?

        indIDToCSMap[indCSId] = callInst;
        indCSToIDMap[callInst] = indCSId;

        Constant* csIdCons = ConstantInt::get(intType, indCSId);
        for (const Function* tgt: wInvTgts) {
            Value* funcAddr = builder.CreateBitOrPointerCast(const_cast<Function*>(tgt), longType);
            builder.CreateCall(updateTgtWInvFn->getFunctionType(), updateTgtWInvFn, {csIdCons, funcAddr});
        }

        for (const Function* tgt: woInvTgts) {
            Value* funcAddr = builder.CreateBitOrPointerCast(const_cast<Function*>(tgt), longType);
            builder.CreateCall(updateTgtWOInvFn->getFunctionType(), updateTgtWOInvFn, {csIdCons, funcAddr});
        }
        indCSId++;
    }

    if (Options::NoInvariants) {
        // Flip the inv Flag
        FunctionType* initWithNoInvType = FunctionType::get(Type::getVoidTy(module->getContext()), {}, false);
        Function* initWithNoInvFn = Function::Create(initWithNoInvType, Function::ExternalLinkage, "initWithNoInvariant", module);
        builder.CreateCall(initWithNoInvFn, {});

    }
}

void WPAPass::addCFIFunctions(llvm::Module* module) {
    Type* longType = IntegerType::get(module->getContext(), 64);
    Type* intType = IntegerType::get(module->getContext(), 32);
    Type* ptrToLongType = PointerType::get(longType, 0);

    // The updateTgtXX function 
    FunctionType* updateTgtFnTy = FunctionType::get(Type::getVoidTy(module->getContext()), {intType, longType}, false);
    updateTgtWInvFn = Function::Create(updateTgtFnTy, Function::ExternalLinkage, "updateTgtWInv", module);
    updateTgtWOInvFn = Function::Create(updateTgtFnTy, Function::ExternalLinkage, "updateTgtWoInv", module);

    // The checkCFI function
    std::vector<Type*> typeCheckCFI;
    typeCheckCFI.push_back(intType);
    typeCheckCFI.push_back(longType); // checkCFI(unsigned long* valid_tgts, int len, unsigned long* tgt);

    llvm::ArrayRef<Type*> typeCheckCFIArr(typeCheckCFI);

    FunctionType* checkCFIFnTy = FunctionType::get(Type::getVoidTy(module->getContext()), typeCheckCFIArr, false);
    checkCFI = Function::Create(checkCFIFnTy, Function::ExternalLinkage, "checkCFI", module);
}

void WPAPass::linkVariadics(SVFModule* svfModule, PAG* pag) {
    SVFModule::iterator it = svfModule->begin();
    for (; it != svfModule->end(); it++) {
        const SVFFunction* fun = *it;
        if (fun->isVarArg()) {
            NodeID varargNode = pag->getVarargNode(fun);
            const PointsTo& pts = _pta->getPts(varargNode);
            for (PointsTo::iterator piter = pts.begin(), epiter = pts.end(); piter != epiter; ++piter) {
                NodeID ptd = *piter;
                if (pag->hasPAGNode(ptd)) {
                    PAGNode* tgtNode = pag->getPAGNode(ptd);
                    if (tgtNode->hasValue()) {
                        if (const Function* func = SVFUtil::dyn_cast<Function>(tgtNode->getValue())) {
                            fixupSet.insert(const_cast<Function*>(func));
                        }
                    }
                }
            }
        }
    }
    
    for (Function* fixup: fixupSet) {
        llvm::errs() << "Fix up with function: " << fixup->getName() << "\n";
    }
    /*
    for (auto it: wInvIndCallMap) {
        for (Function* fixUp: fixupVec) {
            wInvIndCallMap[it.first].insert(fixUp);
        }
    }

    for (auto it: woInvIndCallMap) {
        for (Function* fixUp: fixupVec) {
            woInvIndCallMap[it.first].insert(fixUp);
        }
    }
    */
}

bool WPAPass::matchFunctionType(Function* func, CallInst* call) {
    FunctionType* ft1 = func->getFunctionType();
    FunctionType* ft2 = call->getFunctionType();

    if (ft1->getNumParams() != ft2->getNumParams()) return false;
    //if (ft1->getReturnType() != ft1->getReturnType()) return false;
    return true;
}

/*!
 * Create pointer analysis according to a specified kind and then analyze the module.
 */
void WPAPass::runPointerAnalysis(SVFModule* svfModule, u32_t kind)
{
    llvm::Module *module = SVF::LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule(); 

    llvm::legacy::PassManager PM;
    llvm::DominatorTreeWrapperPass* domPass = new llvm::DominatorTreeWrapperPass();
    LoopInfoWrapperPass* loopInfoPass = new llvm::LoopInfoWrapperPass();
    svfLoopInfo = new LoopInfoConsolidatorPass();
    PM.add(domPass);
    PM.add(loopInfoPass);
    PM.add(svfLoopInfo);
    PM.run(*module);

    if ((Options::InvariantVGEP || Options::InvariantPWC) && Options::NoInvariants) {
        llvm::errs() << "Invalid configuration ...\n";
        return;
    }
	/// Build PAG
	PAGBuilder builder;

	Options::InvariantVGEP = true;
	Options::InvariantPWC = true;

	PAG* pag = builder.build(svfModule);
    Andersen* anderAnalysis = new AndersenWaveDiff(pag);
    _pta = anderAnalysis;
    anderAnalysis->setSVFLoopInfo(svfLoopInfo);
    ptaVector.push_back(_pta);
    _pta->analyze();

    collectCFI(svfModule, *module, false);

		std::string statDir = _pta->getStat()->getStatDir();
    if (Options::ShortCircuit) {
        collectCFI(svfModule, *module, true);
    } else {

			/*
				// Round 2: VGEP
        Options::InvariantVGEP = true;
        Options::InvariantPWC = false;

				llvm::errs() << "Running with InvariantVGEP = " << Options::InvariantVGEP << " InvariantPWC = " << Options::InvariantPWC << "\n";

        builder.getPAG()->resetPAG();
        SymbolTableInfo::releaseSymbolInfo();

        svfModule->buildSymbolTableInfo();

        PAGBuilder builder2;

        PAG* pag2 = builder2.build(svfModule);
        _pta = new AndersenWaveDiff(pag2);
				_pta->getStat()->setStatDir(statDir); // copy the stat dir
        ptaVector.clear();
        ptaVector.push_back(_pta);
        _pta->analyze();

				// Round 2: VGEP
        Options::InvariantVGEP = false;
        Options::InvariantPWC = true;

				llvm::errs() << "Running with InvariantVGEP = " << Options::InvariantVGEP << " InvariantPWC = " << Options::InvariantPWC << "\n";

        builder.getPAG()->resetPAG();
				builder2.getPAG()->resetPAG();
        SymbolTableInfo::releaseSymbolInfo();

        svfModule->buildSymbolTableInfo();

        PAGBuilder builder3;

        PAG* pag3 = builder3.build(svfModule);
        _pta = new AndersenWaveDiff(pag3);
				_pta->getStat()->setStatDir(statDir); // copy the stat dir
        ptaVector.clear();
        ptaVector.push_back(_pta);
        _pta->analyze();
				*/

				// Round 4: The default
        Options::InvariantVGEP = false;
        Options::InvariantPWC = false;


        builder.getPAG()->resetPAG();
				/*
				builder2.getPAG()->resetPAG();
				builder3.getPAG()->resetPAG();
				*/
        SymbolTableInfo::releaseSymbolInfo();

        svfModule->buildSymbolTableInfo();

        PAGBuilder builder4;

        PAG* pag4 = builder4.build(svfModule);
        _pta = new AndersenWaveDiff(pag4);
				_pta->getStat()->setStatDir(statDir); // copy the stat dir
        ptaVector.clear();
        ptaVector.push_back(_pta);
        _pta->analyze();

				// Collect CFI at the end
        collectCFI(svfModule, *module, true);
    }


    // At the end of collectCFI
    // we have two maps inside WPAPass populated
    // the wInvIndCallMap and the woInvIndCallMap
    //
    // Using these, we must build the CFI policies for both the with invariant
    // and without invariant versions.
    // And then have the memory view switcher switch between these
    //
    // We do this as follows:
    // We use the wInvMaps to instrument the module
    // In case of indirect calls where the profile produced by wInvMap and
    // woInvMap differs, we create a clone of the Function and add the pair
    // (the original Function and the cloned Function) to
    // a vector of memory-view-pairs.
    //
    // We instrument the original function according to the wInvMap's profile
    // and the cloned function according to the woInvMap's profile
    //
    // We pass the memory-view-pairs vector the the Memory View Switcher pass
    //


		if (Options::ApplyCFI) {
			addCFIFunctions(module);
			initializeCFITargets(module);

			for (auto pair: wInvIndCallMap) {
				llvm::CallInst* callInst = const_cast<CallInst*>(pair.first);

				instrumentCFICheck(callInst);
			}
		}
    
    // Now go over all functions 
    // and see if the versions are different

    // Handle the invariants
    if (!Options::NoInvariants) {
				initKaleidoscope(module);
        InvariantHandler IHandler(svfModule, module, pag, loopInfoPass); // this one should be the original PAG
        IHandler.init();
        IHandler.handleVGEPInvariants();
        IHandler.handlePWCInvariants();

        Debugger debugger(svfModule, module, pag, loopInfoPass, _pta);
        debugger.init();

    }

#if 0
    // Print CDF / PDF
    std::map<int, float> pdfWInv;
    std::map<int, float> cdfWInv;
    std::map<int, float> pdfWoInv;
    std::map<int, float> cdfWoInv;

    // Find the largest EC Size (this is going to be without Invariant
    // Find the total number of callsites. This won't change depending on
    // whether or not we're running w/ or wo/ invariants

    int largestEC = 0;
    int totalCS = 0;
    for (auto it: woInvHistogram) {
        if (it.first > largestEC) {
            largestEC = it.first;
        }
        totalCS += it.second;
    }

    // Compute the PDF and CDF for wo/ invariant
    for (auto it: woInvHistogram) {
        int ecSize = it.first;
        int csCount = it.second;
        pdfWoInv[ecSize] = ((float) csCount) / ((float) totalCS) * 100;
    }

    for (int i = 0; i <= largestEC; i++) {
        if (pdfWoInv.find(i) == pdfWoInv.end()) {
            pdfWoInv[i] = 0.0;
        }
    }

    cdfWoInv[0] = pdfWoInv[0];
    for (int i = 1; i <= largestEC; i++) {
        cdfWoInv[i] = cdfWoInv[i-1] + pdfWoInv[i];
    }
    cdfWoInv[0] = 0; // testing
    
    // Compute the PDF and CDF for w/ invariant
    for (auto it: wInvHistogram) {
        int ecSize = it.first;
        int csCount = it.second;
        pdfWInv[ecSize] = ((float) csCount) / ((float) totalCS) * 100;
    }

    for (int i = 0; i <= largestEC; i++) {
        if (pdfWInv.find(i) == pdfWInv.end()) {
            pdfWInv[i] = 0.0;
        }
    }

    cdfWInv[0] = pdfWInv[0];
    for (int i = 1; i <= largestEC; i++) {
        cdfWInv[i] = cdfWInv[i-1] + pdfWInv[i];
    }
    cdfWInv[0] = 0; // testing


    std::cerr << std::fixed;
    std::cerr << "CDF:\n";
    std::string appName = std::get<0>(module->getName().split(".")).str();
    appName[0] = std::toupper(appName[0]);
    for (auto it: cdfWoInv) {
        std::cerr << appName << "," << "0, " << it.first << "," << it.second << "\n";
    }
 
    for (auto it: cdfWInv) {
        std::cerr << appName << "," << "1, " << it.first << "," << it.second << "\n";
        if (it.second >= 100.0) {
            break;
        }
    }
#endif
}

void WPAPass::PrintAliasPairs(PointerAnalysis* pta)
{
    PAG* pag = pta->getPAG();
    for (PAG::iterator lit = pag->begin(), elit = pag->end(); lit != elit; ++lit)
    {
        PAGNode* node1 = lit->second;
        PAGNode* node2 = node1;
        for (PAG::iterator rit = lit, erit = pag->end(); rit != erit; ++rit)
        {
            node2 = rit->second;
            if(node1==node2)
                continue;
            const Function* fun1 = node1->getFunction();
            const Function* fun2 = node2->getFunction();
            AliasResult result = pta->alias(node1->getId(), node2->getId());
            SVFUtil::outs()	<< (result == AliasResult::NoAlias ? "NoAlias" : "MayAlias")
                            << " var" << node1->getId() << "[" << node1->getValueName()
                            << "@" << (fun1==nullptr?"":fun1->getName()) << "] --"
                            << " var" << node2->getId() << "[" << node2->getValueName()
                            << "@" << (fun2==nullptr?"":fun2->getName()) << "]\n";
        }
    }
}

/*!
 * Return alias results based on our points-to/alias analysis
 * TODO: Need to handle PartialAlias and MustAlias here.
 */
AliasResult WPAPass::alias(const Value* V1, const Value* V2)
{

    AliasResult result = llvm::MayAlias;

    PAG* pag = _pta->getPAG();

    /// TODO: When this method is invoked during compiler optimizations, the IR
    ///       used for pointer analysis may been changed, so some Values may not
    ///       find corresponding PAG node. In this case, we only check alias
    ///       between two Values if they both have PAG nodes. Otherwise, MayAlias
    ///       will be returned.
    if (pag->hasValueNode(V1) && pag->hasValueNode(V2))
    {
        /// Veto is used by default
        if (Options::AliasRule.getBits() == 0 || Options::AliasRule.isSet(Veto))
        {
            /// Return NoAlias if any PTA gives NoAlias result
            result = llvm::MayAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it)
            {
                if ((*it)->alias(V1, V2) == llvm::NoAlias)
                    result = llvm::NoAlias;
            }
        }
        else if (Options::AliasRule.isSet(Conservative))
        {
            /// Return MayAlias if any PTA gives MayAlias result
            result = llvm::NoAlias;

            for (PTAVector::const_iterator it = ptaVector.begin(), eit = ptaVector.end();
                    it != eit; ++it)
            {
                if ((*it)->alias(V1, V2) == llvm::MayAlias)
                    result = llvm::MayAlias;
            }
        }
    }

    return result;
}

/*!
 * Return mod-ref result of a CallInst
 */
ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst)
{
    assert(Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallBlockNode* cbn = icfg->getCallBlockNode(callInst);
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn);
}

/*!
 * Return mod-ref results of a CallInst to a specific memory location
 */
ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst, const Value* V)
{
    assert(Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallBlockNode* cbn = icfg->getCallBlockNode(callInst);
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn, V);
}

/*!
 * Return mod-ref result between two CallInsts
 */
ModRefInfo WPAPass::getModRefInfo(const CallInst* callInst1, const CallInst* callInst2)
{
    assert(Options::PASelected.isSet(PointerAnalysis::AndersenWaveDiff_WPA) && Options::AnderSVFG && "mod-ref query is only support with -ander and -svfg turned on");
    ICFG* icfg = _svfg->getPAG()->getICFG();
    const CallBlockNode* cbn1 = icfg->getCallBlockNode(callInst1);
    const CallBlockNode* cbn2 = icfg->getCallBlockNode(callInst2);
    return _svfg->getMSSA()->getMRGenerator()->getModRefInfo(cbn1, cbn2);
}
