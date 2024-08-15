/* SVF - Static Value-Flow Analysis Framework
Copyright (C) 2015 Yulei Sui
Copyright (C) 2015 Jingling Xue

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * HeapSimplifier.cpp
 *
 *  Created on: Oct 8, 2013
 */



#define DEBUG_TYPE "heap-type"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "Util/Options.h"
#include "llvm/IR/InstIterator.h"

#include "SVF-FE/HeapSimplifier.h"

using namespace SVF;

// Identifier variable for the pass
char HeapSimplifier::ID = 0;

// Register the pass
static llvm::RegisterPass<HeapSimplifier> HT ("heap-type-analyzer",
        "Heap Type Analyzer");

void HeapSimplifier::handleVersions(Module& module) {
    for (Module::iterator MIterator = module.begin(); MIterator != module.end(); MIterator++) {
        if (Function *F = SVFUtil::dyn_cast<Function>(&*MIterator)) {
            for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
                Instruction* inst = &*I;
                CallInst* cInst = SVFUtil::dyn_cast<CallInst>(inst);
                if (cInst) {
                    Function* calledFunc = cInst->getCalledFunction();
                    if (calledFunc) {
                        for (std::string memAllocFnName: memAllocFns) {
                            if (calledFunc->getName().startswith(memAllocFnName+".")) {
                                Function* memAllocFun = module.getFunction(memAllocFnName);
                                cInst->setCalledFunction(memAllocFun); // remove versioning
                            }
                        }
                        for (std::string la0FnName: L_A0_Fns) {
                            if (calledFunc->getName().startswith(la0FnName+".")) {
                                Function* la0Fun = module.getFunction(la0FnName);
                                cInst->setCalledFunction(la0Fun); // remove versioning
                            }
                        }
                    }
                }
            }
        }
    }
 
}

void HeapSimplifier::removePoolAllocatorBody(Module& module) {
    for (Module::iterator MIterator = module.begin(); MIterator != module.end(); MIterator++) {
        if (Function *F = SVFUtil::dyn_cast<Function>(&*MIterator)) {
            if (std::find(memAllocFns.begin(), memAllocFns.end(), F->getName()) != memAllocFns.end()) {
                F->deleteBody();
            }
            if (std::find(L_A0_Fns.begin(), L_A0_Fns.end(), F->getName()) != L_A0_Fns.end()) {
                F->deleteBody();
            }
            /*
            if (F->getName().startswith("Curl_dyn")
                    || F->getName().startswith("Curl_str")
                    || F->getName().startswith("curl_str")
                    || F->getName().startswith("Curl_llist")
                    || F->getName().startswith("Curl_infof")
                    || F->getName().startswith("Curl_failf")
                    || F->getName().startswith("curl_easy_getinfo")
                    || F->getName().startswith("curl_slist")
                    || F->getName().startswith("tool_setopt")
                    || F->getName().startswith("curl_msnprintf")) {
                F->deleteBody();
            }
            */
            /*
            if (F->getName().startswith("ngx_log_error_core")
                    || F->getName().startswith("config")
                    || F->getName().startswith("array_insert_unique")) {
                F->deleteBody();
            }
            */
        }
    }

}

CallInst* HeapSimplifier::findCInstFA(Value* val) {
    std::vector<Value*> workList;
    workList.push_back(val);

    while (!workList.empty()) {
        Value* val = workList.back();
        workList.pop_back();
        
        for (User* user: val->users()) {
            if (user == val) continue;
            Instruction* userInst = SVFUtil::dyn_cast<Instruction>(user);
            assert(userInst && "Must be instruction");
            if (CallInst* cInst = SVFUtil::dyn_cast<CallInst>(userInst)) {
                return cInst;
            } else {
                workList.push_back(userInst);
            }
        }
    }
    return nullptr;
}

/**
 * func: The function that must be deep cloned
 * mallocFunctions: The list of malloc wrappers that our system is aware of
 * (malloc, calloc)
 * cloneHeapTy: What type the deepest malloc clone should be set to
 * origHeapTy: What type the deepest malloc of the original should be set to
 */
/*
bool HeapSimplifier::deepClone(llvm::Function* func, llvm::Function*& topClonedFunc, std::vector<std::string>& mallocFunctions, 
        Type* cloneHeapTy, Type* origHeapTy) {
    std::vector<Function*> workList;
    std::vector<Function*> visited;

    std::vector<Function*> cloneFuncs; // The list of functions to clone
    cloneFuncs.push_back(func);
    std::vector<CallInst*> mallocCalls; // the list of calls to malloc. Should be 1

    workList.push_back(func);
    visited.push_back(func);
    while (!workList.empty()) {
        Function* F = workList.back();
        workList.pop_back();
        for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
            if (CallInst* CI = SVFUtil::dyn_cast<CallInst>(&*I)) {
                Function* calledFunc = CI->getCalledFunction();
                if (!calledFunc) {
                    // We can't handle indirect calls
                    return false;
                }
                if (std::find(mallocFunctions.begin(), mallocFunctions.end(), calledFunc->getName())
                        != mallocFunctions.end()) {
                    mallocCalls.push_back(CI);
                } else {
                    // If it returns a pointer
                    PointerType* retPtrTy = SVFUtil::dyn_cast<PointerType>(calledFunc->getReturnType());
                    if (!retPtrTy) {
                        // We don't need to clone this function as it doesn't
                        // return a pointer.
                        //
                        // We only need to clone the path that returns the
                        // heap object created on the heap
                        continue;
                    }
                    if (std::find(visited.begin(), visited.end(), calledFunc) == visited.end()) {
                        workList.push_back(calledFunc);
                        visited.push_back(calledFunc);
                        cloneFuncs.push_back(calledFunc);
                    }
                }
            }
        }
    }

    if (mallocCalls.size() != 1) {
        return false;
    }

    std::map<Function*, Function*> cloneMap;

    // Fix the cloned Functions
    // We will handle the callers of the clone later
    for (Function* toCloneFunc: cloneFuncs) {
        llvm::ValueToValueMapTy VMap;
        Function* clonedFunc = llvm::CloneFunction(toCloneFunc, VMap);
        // If the VMap contains the call to malloc, then we can set it right
        // away
        llvm::ValueToValueMapTy::iterator it = VMap.find(mallocCalls[0]);
        if (it != VMap.end()) {
            Value* clonedMallocValue = VMap[mallocCalls[0]];
            //llvm::errs() << *clonedMallocValue << "\n";
            Instruction* clonedMallocInst = SVFUtil::dyn_cast<Instruction>(clonedMallocValue);
            clonedMallocInst->addAnnotationMetadata("ArrayType");
            // CallInst* clonedMalloc = SVFUtil::dyn_cast<CallInst>(&(VMap[mallocCalls[0]]));
            // SVFUtil::dyn_cast<CallInst*>(it->second());
        }
        cloneMap[toCloneFunc] = clonedFunc;
    }

    mallocCalls[0]->addAnnotationMetadata("StructType");
    // Okay, now go over all the original copies of these functions
    // If they had a call to _another_ function that was also cloned, replace
    // them with the code. 
    //

    Function* topLevelClone = cloneMap[func];

    workList.clear();
    workList.push_back(topLevelClone);

    for (auto it: cloneMap) {
        llvm::errs() << "Function " << it.first->getName() << " cloned to " << it.second->getName() << "\n";
    }

    while (!workList.empty()) {
        Function* F = workList.back();
        workList.pop_back();
        for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
            if (CallInst* CI = SVFUtil::dyn_cast<CallInst>(&*I)) {
                Function* calledFunc = CI->getCalledFunction();
                assert(calledFunc && "We shouldn't have to reach here");
                // If this was cloned
                if (std::find(cloneFuncs.begin(), cloneFuncs.end(), calledFunc) != cloneFuncs.end()) {
                    Function* clonedFunction = cloneMap[calledFunc];
                    CI->setCalledFunction(clonedFunction);
                    llvm::errs() << "Updated CI " << *CI << "\n";
                }
            }
        }
    }
    topClonedFunc = cloneMap[func];
    return true;
}
*/

HeapSimplifier::HeapTy HeapSimplifier::getSizeOfTy(Module& module, LLVMContext& Ctx, MDNode* sizeOfTyName, MDNode* sizeOfTyArgNum, MDNode* mulFactor) {
    // Get the type
    MDString* typeNameStr = (MDString*)sizeOfTyName->getOperand(0).get();
    Type* sizeOfTy = nullptr;
    if (typeNameStr->getString() == "scalar_type") {
        return HeapTy::ScalarTy;
    } else {
        MDString* mulFactorStr = (MDString*)mulFactor->getOperand(0).get();
        int mulFactorInt = std::stoi(mulFactorStr->getString().str());
        assert(mulFactorInt > 0 && "The multiplicator must be greater than 1");

        if (mulFactorInt == 1) {
            return HeapTy::StructTy;
        } else {
            return HeapTy::ArrayTy;
        }
    }
    assert(false && "Shouldn't reach here");
    
}

void HeapSimplifier::deriveHeapAllocationTypes(llvm::Module& module) {
    // Just check all the locations that call the malloc functions 
    // or the pool allocators
    for (Module::iterator MIterator = module.begin(); MIterator != module.end(); MIterator++) {
        if (Function *F = SVFUtil::dyn_cast<Function>(&*MIterator)) {
            for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
                Instruction* inst = &*I;
                LLVMContext& Ctx = inst->getContext();

                MDNode* sizeOfTyName = inst->getMetadata("sizeOfTypeName");
                MDNode* sizeOfTyArgNum = inst->getMetadata("sizeOfTypeArgNum");
                MDNode* mulFactor = inst->getMetadata("sizeOfMulFactor");

                bool handled = false;
                // Check if this is a call to malloc or pool allocator
                CallInst* callInst = SVFUtil::dyn_cast<CallInst>(inst);
                if (callInst) {
                    Function* calledFunc = callInst->getCalledFunction();
                    if (!sizeOfTyName) {
                        Function* calledFunc = callInst->getCalledFunction();
                        if (calledFunc) {
                            if (std::find(memAllocFns.begin(), memAllocFns.end(), calledFunc->getName()) != memAllocFns.end() 
                                    && std::find(heapCalls.begin(), heapCalls.end(), F) == heapCalls.end() 
                                    /* if the caller is a heap allocator we don't care*/) {
                                //llvm::errs() << "No type annotation for heap call: " << *callInst << " in function : " << callInst->getFunction()->getName() << " treating as scalar\n";
                                callInst->addAnnotationMetadata("IntegerType");
                                handled = true;
                            }
                        }

                        continue;
                    }

                    HeapTy ty = getSizeOfTy(module, Ctx, sizeOfTyName, sizeOfTyArgNum, mulFactor);
                    switch(ty) {
                        case ScalarTy:
                            callInst->addAnnotationMetadata("IntegerType");
                            break;
                        case StructTy:
                            callInst->addAnnotationMetadata("StructType");
                            break;
                        case ArrayTy:
                            callInst->addAnnotationMetadata("ArrayType"); 
                            break;
                        default:
                            assert(false && "Shouldn't reach here");
                    }
                    handled = true;
                }
                /*

                // We just leave the metadata in place for future resolution of indirect heap calls
                if (sizeOfTyName && !handled) {
                    if (callInst && callInst->getCalledFunction()) {
                        if (callInst->getCalledFunction()->isIntrinsic()) {
                            continue;
                        } else {
                            llvm::errs() << "Call to function: " << callInst->getCalledFunction()->getName() << " has type info but not a heap allocation\n";
                        }
                    } else {
                        llvm::errs() << "Instruction: " << " in function: " << inst->getFunction()->getName() << " : " << *inst << " has type info but not a heap allocation\n";
                    }
                }
                */
            }
        }
    }
}

/*
void HeapSimplifier::deriveHeapAllocationTypesWithCloning(llvm::Module& module) {

    //StructType* stTy = StructType::getTypeByName(module.getContext(), tyName);
    std::vector<std::string> mallocFunctions;

    mallocFunctions.push_back("malloc");
    mallocFunctions.push_back("calloc");

    typedef std::map<Function*, std::set<Type*>> FunctionSizeOfTypeMapTy;
    FunctionSizeOfTypeMapTy functionSizeOfTypeMap;
    typedef std::map<CallInst*, Type*> MallocSizeOfTypeMapTy;
    MallocSizeOfTypeMapTy mallocSizeOfMap;

    typedef std::map<Function*, std::set<CallInst*>> WrapperTypeCallerTy;
    WrapperTypeCallerTy arrayTyCallers;
    WrapperTypeCallerTy structTyCallers;


    for (Module::iterator MIterator = module.begin(); MIterator != module.end(); MIterator++) {
        if (Function *F = SVFUtil::dyn_cast<Function>(&*MIterator)) {
            if (!F->isDeclaration()) {
                for (llvm::inst_iterator I = llvm::inst_begin(F), E = llvm::inst_end(F); I != E; ++I) {
                    Instruction* inst = &*I;
                    LLVMContext& Ctx = inst->getContext();

                    MDNode* sizeOfTyName = inst->getMetadata("sizeOfTypeName");
                    MDNode* sizeOfTyArgNum = inst->getMetadata("sizeOfTypeArgNum");
                    MDNode* mulFactor = inst->getMetadata("sizeOfMulFactor");
                    if (sizeOfTyName) {
                        CallInst* cInst = SVFUtil::dyn_cast<CallInst>(&*I);
                        if (!cInst) {
                            StoreInst* SI = SVFUtil::dyn_cast<StoreInst>(&*I);
                            assert(SI && "Can only handle Store Insts here");

                            cInst = findCInstFA(SI->getPointerOperand());
                        }
                        if (!cInst) {
                            continue;
                        }
                        Function* calledFunction = cInst->getCalledFunction();
                        if (!calledFunction) {
                            continue;
                        }
                        // Get the type
                        MDString* typeNameStr = (MDString*)sizeOfTyName->getOperand(0).get();
                        Type* sizeOfTy = nullptr;
                        if (typeNameStr->getString() == "scalar_type") {
                            sizeOfTy = IntegerType::get(Ctx, 8);
                        } else {
                            std::string prefix = "struct.";
                            std::string structNameStr = typeNameStr->getString().str();
                            StringRef llvmTypeName(prefix+structNameStr);
                            sizeOfTy = StructType::getTypeByName(module.getContext(), llvmTypeName);
                        }
                        if (!sizeOfTy) {
                            continue;
                        }

                        assert(mulFactor && "Sizeof operator must have a multiplicative factor present");

                        MDString* mulFactorStr = (MDString*)mulFactor->getOperand(0).get();
                        int mulFactorInt = std::stoi(mulFactorStr->getString().str());
                        assert(mulFactorInt > 0 && "The multiplicator must be greater than 1");

                        // Create the type
                        if (mulFactorInt > 1) {
                            sizeOfTy = ArrayType::get(sizeOfTy, mulFactorInt);
                        }
                        
                        if (std::find(mallocFunctions.begin(), mallocFunctions.end(), calledFunction->getName()) == mallocFunctions.end()) {
                            functionSizeOfTypeMap[calledFunction].insert(sizeOfTy);
                            if (SVFUtil::isa<ArrayType>(sizeOfTy)) {
                                arrayTyCallers[calledFunction].insert(cInst); 
                            } else if (SVFUtil::isa<StructType>(sizeOfTy)) {
                                structTyCallers[calledFunction].insert(cInst); 
                            }
                        } else {
                            mallocSizeOfMap[cInst] = sizeOfTy;
                        }
                    }
                }
            }
        }
    }

    for (auto it: functionSizeOfTypeMap) {
        Function* potentialMallocWrapper = it.first;
        std::set<Type*>& types = it.second;
        Type* arrTy = nullptr;
        Type* structTy = nullptr;
        
        llvm::errs() << "Potential malloc wrapper: " << potentialMallocWrapper->getName() << "\n";
        for (Type* type: types) {
            llvm::errs() << "\t" << *type << "\n";
            if (SVFUtil::isa<ArrayType>(type)) {
                arrTy = SVFUtil::dyn_cast<ArrayType>(type);
            } else if (SVFUtil::isa<StructType>(type)) {
                structTy = SVFUtil::dyn_cast<StructType>(type);
            }
        }

        if (arrTy && structTy) {
            // This function is invoked with both structtype and arraytype
            // Try to deep clone it, and specialize it for the 
            Function* clonedFunc = nullptr;
            bool couldSpecialize = deepClone(potentialMallocWrapper, clonedFunc, mallocFunctions, arrTy, structTy);
            // Update the callsites
            // It _might_ be possible we're re-updating something that was
            // already updated inside deepClone, but it _should_ be okay
            // The cloned function is for the array, and the original is for
            // the struct type.

            if (couldSpecialize) {
                for (CallInst* caller: arrayTyCallers[potentialMallocWrapper]) {
                    caller->setCalledFunction(clonedFunc);
                } 
            }
        }
    }

    for (auto it: mallocSizeOfMap) {
        CallInst* cInst = it.first;
        Type* type = it.second;
        if (SVFUtil::isa<ArrayType>(type)) {
            cInst->addAnnotationMetadata("ArrayType"); 
        } else if (SVFUtil::isa<StructType>(type)) {
            cInst->addAnnotationMetadata("StructType");
        } else if (SVFUtil::isa<IntegerType>(type)) {
            cInst->addAnnotationMetadata("IntegerType");
        }
        llvm::errs() << "Call inst in function: " << cInst->getFunction()->getName() << " takes type " << *type << "\n";
    }

}
*/


void HeapSimplifier::buildCallGraphs (Module & module) {
    for (llvm::Function& F: module.getFunctionList()) {
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
            if (CallInst* cInst = SVFUtil::dyn_cast<CallInst>(&*I)) {
                if (Function* calledFunc = cInst->getCalledFunction()) {
                    callers[calledFunc].push_back(&F);
                    callees[&F].push_back(calledFunc);
                }
            }
        }
    }
    
    std::map<Function*, int> funcToCallerSizeMap;

    for (auto it: callers) {
        Function* calledFunc = it.first;
        bool hasStructTyArg = false;
        for (int i = 0; i< calledFunc->arg_size(); i++) {
            Argument* arg = calledFunc->getArg(i);
            Type* argTy = arg->getType();
            while (PointerType* argPtrTy = SVFUtil::dyn_cast<PointerType>(argTy)) {
                argTy = argPtrTy->getPointerElementType();
            }
            if (SVFUtil::isa<StructType>(argTy)) {
                hasStructTyArg = true;
            }
        }
        if (hasStructTyArg) 
            funcToCallerSizeMap[calledFunc] = it.second.size();
    }

    for (auto it: funcToCallerSizeMap) {
        Function* calledFunc = it.first;
        if (calledFunc->isDeclaration()) continue;
        callerDistMap[it.second].push_back(calledFunc);
    }

    int i = 0;
    for (auto it: callerDistMap) {
        int callerCount = it.first;
        llvm::errs() << callerCount << " callers: ";
        for (Function* func: it.second) {
            llvm::errs() << func->getName() << " ["<< func->isDeclaration() << "], ";
            if ( i < Options::RemoveThres
                    && std::find(heapCalls.begin(), heapCalls.end(), func)
                    == heapCalls.end()) {
                func->deleteBody();
                llvm::errs() << "Removing body\n";
                i++;
            }
        }
        llvm::errs() << "\n";
        
    }
    
}

bool HeapSimplifier::returnsUntypedMalloc(Function* potentialMallocWrapper) {
    llvm::CFLAndersAAWrapperPass& aaPass = getAnalysis<llvm::CFLAndersAAWrapperPass>();
    llvm::CFLAndersAAResult& aaResult = aaPass.getResult();
    
    Instruction* mallockedPtr = nullptr;


    for (inst_iterator I = inst_begin(potentialMallocWrapper), E = inst_end(potentialMallocWrapper); I != E; ++I) {
        if (CallInst* cInst = SVFUtil::dyn_cast<CallInst>(&*I)) {
            if (cInst->getCalledFunction() && 
                    std::find(memAllocFns.begin(), memAllocFns.end(), cInst->getCalledFunction()->getName().str()) != memAllocFns.end()) {
                mallockedPtr = cInst;
                break;
            }
        }
    }
    // There are situations that call malloc with a known type inside a small function. 
    // We don't care about these if the type is known.
    if (mallockedPtr->getMetadata("sizeOfTypeName")) { 
        return false;
    }

    if (!mallockedPtr) {
        return false;
    }

    for (inst_iterator I = inst_begin(potentialMallocWrapper), E = inst_end(potentialMallocWrapper); I != E; ++I) {
        if (ReturnInst* retInst = SVFUtil::dyn_cast<ReturnInst>(&*I)) {
            if (Instruction* retValue = SVFUtil::dyn_cast<Instruction>(retInst->getReturnValue())) {
                if (retValue == mallockedPtr) return true;
                llvm::AliasResult isAlias = aaResult.query(
                        llvm::MemoryLocation(mallockedPtr, llvm::LocationSize(64)), 
                        llvm::MemoryLocation(retValue, llvm::LocationSize(64)));
                if (isAlias == llvm::AliasResult::MayAlias) {
                    return true;
                }
            } else {
                return false;
            }
        }
    }
    return false;
}

void HeapSimplifier::findHeapContexts (Module& M) {
    std::vector<Function*> oneLevelFuncs;

    std::vector<Function*> twoLevelFuncs;

    for (std::string& memFnStr: memAllocFns) {
        Function* memAllocFn = M.getFunction(memFnStr);
        if (memAllocFn) {
            for (Function* caller: callers[memAllocFn]) {
                if (caller->getReturnType()->isVoidTy()) {
                    continue;
                }
                if (returnsUntypedMalloc(caller) && callees[caller].size() < 7) {
                    oneLevelFuncs.push_back(caller);
                    heapCalls.push_back(caller);
                }
            }
        }
    }

    for (Function* f: oneLevelFuncs) {
        llvm::errs() << "One Level Function name: " << f->getName() << "\n";
        memAllocFns.push_back(f->getName().str());
    }

    for (Function* memAllocFn: oneLevelFuncs) {
        for (Function* caller: callers[memAllocFn]) {
            if (caller->getInstructionCount() < 10) {
                heapCalls.push_back(caller);
                twoLevelFuncs.push_back(caller);
            }
        }
    }

    for (Function* f: twoLevelFuncs) {
        llvm::errs() << "Two Level Function name: " << f->getName() << "\n";
        memAllocFns.push_back(f->getName().str());
    }

    // IMPORTANT: Make sure this is consistent with the list in ExtAPI.cpp
    std::vector<std::string> allocFns {
        "ngx_alloc",
        "ngx_array_create",
        "ngx_calloc",
        "ngx_palloc",
        "ngx_palloc_small",
        "ngx_pcalloc",
        "ngx_pnalloc",
        "ngx_resolver_alloc",
        "ngx_resolver_calloc",
        "ngx_slab_alloc",
        "ngx_slab_calloc_locked",
        "ngx_palloc_large",
        "ngx_calloc",
        "ngx_create_pool",
        "ngx_array_push",
        "ngx_array_push_n",

        "luaM_reallocv",
        "luaM_malloc",
        "luaM_new",
        "luaM_newvector",
        "luaM_growvector",
        "luaM_reallocvector",
        "mytest_malloc"
    };

    for (Function& f: M.getFunctionList()) {
        if (std::find(allocFns.begin(), allocFns.end(), f.getName().str()) != allocFns.end()) {
            heapCalls.push_back(&f);

            llvm::ValueToValueMapTy  vmap;
            Function* cloned = llvm::CloneFunction(&f, vmap, NULL);
            clonedFunctionMap[f.getName()] = cloned;
        }
    }




    for (Function* f: heapCalls) {
        llvm::errs() << " Heap call function: " << f->getName() << "\n";
        memAllocFns.push_back(f->getName().str());
    }

    buildCallGraphs(M);
}

bool
HeapSimplifier::runOnModule (Module & module) {
    /*
    // Replace all invariant geps with 1-field index

    IntegerType* i64Ty = IntegerType::get(module.getContext(), 64);
    Constant* oneCons = ConstantInt::get(i64Ty, 1);

    for (Function& F: module.getFunctionList()) {
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
            Instruction* inst = &*I;
            if (GetElementPtrInst* gep = SVFUtil::dyn_cast<GetElementPtrInst>(inst)) {
                int i = 0;
                for (auto& it: gep->indices()) {
                    Value& op = *it;
                    if (!SVFUtil::isa<Constant>(&op)) {
                        gep->setOperand(i+1, oneCons);
                    }
                    i++;
                }
            }
        }
    }
    */
    /*
    llvm::CFLAndersAAWrapperPass& aaPass = getAnalysis<llvm::CFLAndersAAWrapperPass>();
    llvm::CFLAndersAAResult& aaResult = aaPass.getResult();
 
    for (Function& F: module.getFunctionList()) {
        for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
            if (ReturnInst* retInst = SVFUtil::dyn_cast<ReturnInst>(&*I)) {
                if (!retInst->getReturnValue()) continue;
                if (Instruction* retValue = SVFUtil::dyn_cast<Instruction>(retInst->getReturnValue())) {
                    // Check if it aliases with an argument
                    for (int i = 0; i < F.arg_size(); i++) {
                        Argument* arg = F.getArg(i);
                        // Find the stack location
                        AllocaInst* stackArg = nullptr;
                        for (User* u: arg->users()) {
                            if (StoreInst* store = SVFUtil::dyn_cast<StoreInst>(u)) {
                                if (AllocaInst* stack = SVFUtil::dyn_cast<AllocaInst>(store->getPointerOperand())) {
                                    stackArg = stack;
                                    break;
                                }
                            }
                        }
                        if (!stackArg || !stackArg->getType()->isPointerTy() || !retValue->getType()->isPointerTy()) {
                            continue;
                        }
                        llvm::AliasResult isAlias = aaResult.query(
                                llvm::MemoryLocation(stackArg, llvm::LocationSize(8)), 
                                llvm::MemoryLocation(retValue, llvm::LocationSize(8)));
                        if (isAlias == llvm::AliasResult::MayAlias) {
                            llvm::errs() << "Function " << F.getName() << " returns an argument\n";
                        }
                    }
                }
            }
        }
    }
    */
 
    findHeapContexts(module);
//    handleVersions(module);
    removePoolAllocatorBody(module);
    deriveHeapAllocationTypes(module);
    
    std::error_code EC;
    llvm::raw_fd_ostream OS("heap-cloned-module.bc", EC,
            llvm::sys::fs::F_None);
    WriteBitcodeToFile(module, OS);
    OS.flush();

    return false;
}
