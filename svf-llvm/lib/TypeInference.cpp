//===- TypeInference.cpp -- Type inference----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * TypeInference.cpp
 *
 *  Created by Xiao Cheng on 10/01/24.
 *
 */

#include "SVF-LLVM/TypeInference.h"

#define TYPE_DEBUG 1 /* Turn this on if you're debugging type inference */
#define ERR_MSG(msg)                                                           \
    do                                                                         \
    {                                                                          \
        SVFUtil::errs() << SVFUtil::errMsg("Error ") << __FILE__ << ':'        \
            << __LINE__ << ": " << msg << '\n';                                \
    } while (0)
#define ABORT_MSG(msg)                                                         \
    do                                                                         \
    {                                                                          \
        ERR_MSG(msg);                                                          \
        abort();                                                               \
    } while (0)
#define ABORT_IFNOT(condition, msg)                                            \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
            ABORT_MSG(msg);                                                    \
    } while (0)

u32_t traceId = 0; // for debug purposes
#define INC_TRACE()                                                            \
do {                                                                           \
      traceId++;                                                               \
} while (0)

#if TYPE_DEBUG
#define WARN_MSG(msg)                                                          \
    do                                                                         \
    {                                                                          \
        SVFUtil::outs() << SVFUtil::wrnMsg("Warning ") << __FILE__ << ':'      \
            << __LINE__ << ": "  << msg   << '\n';                             \
    } while (0)
#define WARN_IFNOT(condition, msg)                                             \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
            WARN_MSG(msg);                                                     \
    } while (0)
#else
#define WARN_MSG(msg)
#define WARN_IFNOT(condition, msg)
#endif

using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

std::unique_ptr<TypeInference> TypeInference::_typeInference = nullptr;

const std::string znwm = "_Znwm";
const std::string zn1Label = "_ZN1"; // c++ constructor
const std::string znstLabel = "_ZNSt"; // _ZNSt5dequeIPK1ASaIS2_EE5frontEv -> std::deque<A const*, std::allocator<A const*> >::front()
const std::string znkst5Label = "_ZNKSt15_"; // _ZNKSt15_Deque_iteratorIPK1ARS2_PS2_EdeEv -> std::_Deque_iterator<A const*, A const*&, A const**>::operator*() const
const std::string dyncast = "__dynamic_cast";

const std::string classTyPrefix = "class.";

const std::string TYPEMALLOC = "TYPE_MALLOC";


const Type *TypeInference::defaultTy(const Value *val) {
    ABORT_IFNOT(val, "val cannot be null");
    // heap has a default type of 8-bit integer type
    if (SVFUtil::isa<Instruction>(val) && SVFUtil::isHeapAllocExtCallViaRet(
            LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<Instruction>(val))))
        return Type::getInt8Ty(LLVMModuleSet::getLLVMModuleSet()->getContext());
    // otherwise we return a pointer type in the default address space
    return defaultPtrTy();
}

/*!
 * get or infer the name of thisptr
 * @param thisPtr
 * @return
 */
const std::string &TypeInference::getOrInferThisPtrClassName(const Value *thisPtr) {
    auto it = _thisPtrClassName.find(thisPtr);
    if (it != _thisPtrClassName.end()) return it->second;

    // thisPtr reside in constructor
    if (const Instruction *inst = SVFUtil::dyn_cast<Instruction>(thisPtr)) {
        if (const Function *func = inst->getFunction()) {
            const std::string &className = extractClassNameViaCppCallee(func);
            if (className != "") {
                return _thisPtrClassName[thisPtr] = className;
            }
        }
    }

    // backward find source and then forward find constructor or other mangler functions
    Set<const Value *> sources = bwGetOrfindCPPSources(thisPtr);
    for (const auto &source: sources) {
        if (source == thisPtr) continue;

        if (const Function *func = SVFUtil::dyn_cast<Function>(source)) {
            const std::string &className = extractClassNameViaCppCallee(func);
            if (className != "") {
                return _thisPtrClassName[thisPtr] = className;
            }
        } else if (SVFUtil::isa<LoadInst, StoreInst, GetElementPtrInst, AllocaInst, GlobalValue>(source)) {
            const Type *type = infersiteToType(source);
            const std::string &className = typeToCppClassName(type);
            if (className != "") {
                return _thisPtrClassName[thisPtr] = className;
            }
        } else if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(source)) {
            if (const Function *callFunc = callBase->getCalledFunction()) {
                const std::string &className = extractClassNameViaCppCallee(callFunc);
                if (className != "") {
                    return _thisPtrClassName[thisPtr] = className;
                }
                if (callFunc->getName() == dyncast) {
                    Value *tgtCast = callBase->getArgOperand(2);

                    assert(tgtCast);
                }
            }
        }

        // start from znwm
        if (!SVFUtil::isa<CallBase>(source)) continue;
        const CallBase *callInst = SVFUtil::dyn_cast<CallBase>(source);
        if (!callInst->getCalledFunction()) continue;
        const Function *func = callInst->getCalledFunction();
        if (func->getName() != znwm) continue;
        for (const auto &use: callInst->uses()) {
            if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(use.getUser())) {
                if (!callBase->getCalledFunction()) continue;
                const Function *constructFoo = callBase->getCalledFunction();
                const std::string &className = extractClassNameViaCppCallee(constructFoo);
                if (className != "")
                    return _thisPtrClassName[thisPtr] = className;
            } else if (const BitCastInst *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(use.getUser())) {
                for (const auto &use2: bitCastInst->uses()) {
                    if (const CallBase *callBase2 = SVFUtil::dyn_cast<CallBase>(use2.getUser())) {
                        if (!callBase2->getCalledFunction()) continue;
                        const Function *constructFoo = callBase2->getCalledFunction();
                        const std::string &className = extractClassNameViaCppCallee(constructFoo);
                        if (className != "")
                            return _thisPtrClassName[thisPtr] = className;
                    }
                }
            }
        }
    }
    ABORT_MSG(VALUE_WITH_DBGINFO(thisPtr) + "does not have a type?");
}

/*!
 * get or infer type of a value
 * if the start value is a source (alloc/global, heap, static), call fwGetOrInferLLVMObjType
 * if not, find sources and then forward get or infer types
 * @param startValue
 */
const Type *TypeInference::getOrInferLLVMObjType(const Value *startValue) {
    if (isAllocation(startValue)) return fwGetOrInferLLVMObjType(startValue);
    Set<const Value *> sources = TypeInference::getTypeInference()->bwGetOrfindAllocations(startValue);
    std::vector<const Type *> types;
    for (const auto &source: sources) {
        types.push_back(TypeInference::getTypeInference()->fwGetOrInferLLVMObjType(source));
    }
    return selectLargestType(types);
}

/*!
 * Forward collect all possible infer sites starting from a value
 * @param startValue
 */
const Type *TypeInference::fwGetOrInferLLVMObjType(const Value *startValue) {
    // consult cache
    auto tIt = _valueToType.find(startValue);
    if (tIt != _valueToType.end()) {
        return tIt->second ? tIt->second : defaultTy(startValue);
    }

    INC_TRACE();

    // simulate the call stack, the second element indicates whether we should update valueTypes for current value
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({startValue, false});

    while (!workList.empty()) {
        auto curPair = workList.pop();
        if (visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;
        Set<const Value *> infersites;

        auto insertInferSite = [&infersites, &canUpdate](const Value *infersite) {
            if (canUpdate) infersites.insert(infersite);
        };
        auto insertInferSitesOrPushWorklist = [this, &infersites, &workList, &canUpdate](const auto &pUser) {
            auto vIt = _valueToInferSites.find(pUser);
            if (canUpdate) {
                if (vIt != _valueToInferSites.end()) {
                    infersites.insert(vIt->second.begin(), vIt->second.end());
                }
            } else {
                if (vIt == _valueToInferSites.end()) workList.push({pUser, false});
            }
        };
        if (!canUpdate && !_valueToInferSites.count(curValue)) {
            workList.push({curValue, true});
        }
        if (const GetElementPtrInst *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(curValue))
            insertInferSite(gepInst);
        for (const auto &it: curValue->uses()) {
            if (LoadInst *loadInst = SVFUtil::dyn_cast<LoadInst>(it.getUser())) {
                /*
                 * infer based on load, e.g.,
                 %call = call i8* malloc()
                 %1 = bitcast i8* %call to %struct.MyStruct*
                 %q = load %struct.MyStruct, %struct.MyStruct* %1
                 */
                insertInferSite(loadInst);
            } else if (StoreInst *storeInst = SVFUtil::dyn_cast<StoreInst>(it.getUser())) {
                if (storeInst->getPointerOperand() == curValue) {
                    /*
                     * infer based on store (pointer operand), e.g.,
                     %call = call i8* malloc()
                     %1 = bitcast i8* %call to %struct.MyStruct*
                     store %struct.MyStruct .., %struct.MyStruct* %1
                     */
                    insertInferSite(storeInst);
                } else {
                    for (const auto &nit: storeInst->getPointerOperand()->uses()) {
                        /*
                         * propagate across store (value operand) and load
                         %call = call i8* malloc()
                         store i8* %call, i8** %p
                         %q = load i8*, i8** %p
                         ..infer based on %q..
                        */
                        if (SVFUtil::isa<LoadInst>(nit.getUser()))
                            insertInferSitesOrPushWorklist(nit.getUser());
                    }
                    /*
                     * infer based on store (value operand) <- gep (result element)
                      %call1 = call i8* @TYPE_MALLOC(i32 noundef 16, i32 noundef 2), !dbg !39
                      %2 = bitcast i8* %call1 to %struct.MyStruct*, !dbg !41
                      %3 = load %struct.MyStruct*, %struct.MyStruct** %p, align 8, !dbg !42
                      %next = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %3, i32 0, i32 1, !dbg !43
                      store %struct.MyStruct* %2, %struct.MyStruct** %next, align 8, !dbg !44
                      %5 = load %struct.MyStruct*, %struct.MyStruct** %p, align 8, !dbg !48
                      %next3 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %5, i32 0, i32 1, !dbg !49
                      %6 = load %struct.MyStruct*, %struct.MyStruct** %next3, align 8, !dbg !49
                      infer site -> %f1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %6, i32 0, i32 0, !dbg !50
                      */
                    if (GetElementPtrInst *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(
                            storeInst->getPointerOperand())) {
                        const Value *gepBase = gepInst->getPointerOperand();
                        if (!SVFUtil::isa<LoadInst>(gepBase)) continue;
                        const LoadInst *load = SVFUtil::dyn_cast<LoadInst>(gepBase);
                        for (const auto &loadUse: load->getPointerOperand()->uses()) {
                            if (loadUse.getUser() == load || !SVFUtil::isa<LoadInst>(loadUse.getUser()))
                                continue;
                            for (const auto &gepUse: loadUse.getUser()->uses()) {
                                if (!SVFUtil::isa<GetElementPtrInst>(gepUse.getUser())) continue;
                                for (const auto &loadUse2: gepUse.getUser()->uses()) {
                                    if (SVFUtil::isa<LoadInst>(loadUse2.getUser())) {
                                        insertInferSitesOrPushWorklist(loadUse2.getUser());
                                    }
                                }
                            }
                        }

                    }
                }

            } else if (GetElementPtrInst *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(it.getUser())) {
                /*
                 * infer based on gep (pointer operand)
                 %call = call i8* malloc()
                 %1 = bitcast i8* %call to %struct.MyStruct*
                 %next = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i32 0..
                 */
                if (gepInst->getPointerOperand() == curValue)
                    insertInferSite(gepInst);
            } else if (BitCastInst *bitcast = SVFUtil::dyn_cast<BitCastInst>(it.getUser())) {
                // continue on bitcast
                insertInferSitesOrPushWorklist(bitcast);
            } else if (PHINode *phiNode = SVFUtil::dyn_cast<PHINode>(it.getUser())) {
                // continue on bitcast
                insertInferSitesOrPushWorklist(phiNode);
            } else if (ReturnInst *retInst = SVFUtil::dyn_cast<ReturnInst>(it.getUser())) {
                /*
                 * propagate from return to caller
                  Function Attrs: noinline nounwind optnone uwtable
                  define dso_local i8* @malloc_wrapper() #0 !dbg !22 {
                      entry:
                      %call = call i8* @malloc(i32 noundef 16), !dbg !25
                      ret i8* %call, !dbg !26
                 }
                 %call = call i8* @malloc_wrapper()
                 ..infer based on %call..
                */
                for (const auto &callsite: retInst->getFunction()->uses()) {
                    if (CallBase *callBase = SVFUtil::dyn_cast<CallBase>(callsite.getUser())) {
                        // skip function as parameter
                        // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                        if (callBase->getCalledFunction() != retInst->getFunction()) continue;
                        insertInferSitesOrPushWorklist(callBase);
                    }
                }
            } else if (CallBase *callBase = SVFUtil::dyn_cast<CallBase>(it.getUser())) {
                /*
                 * propagate from callsite to callee
                  %call = call i8* @malloc(i32 noundef 16)
                  %0 = bitcast i8* %call to %struct.Node*, !dbg !43
                  call void @foo(%struct.Node* noundef %0), !dbg !45

                  define dso_local void @foo(%struct.Node* noundef %param) #0 !dbg !22 {...}
                  ..infer based on the formal param %param..
                 */
                // skip global function value -> callsite
                // e.g., def @foo() -> call @foo()
                // we don't skip function as parameter, e.g., def @foo() -> call @bar(..., @foo)
                if (SVFUtil::isa<Function>(curValue) && curValue == callBase->getCalledFunction()) continue;
                // skip indirect call
                // e.g., %0 = ... -> call %0(...)
                if (!callBase->hasArgument(curValue)) continue;
                if (Function *calleeFunc = callBase->getCalledFunction()) {
                    const std::string &fooName = calleeFunc->getName().str();
                    if (isCPPConstructor(fooName)) {
                        // c++ constructor
                        // %call = call noalias noundef nonnull i8* @_Znwm(i64 noundef 8) #7, !dbg !384, !heapallocsite !5
                        // %0 = bitcast i8* %call to %class.B*, !dbg !384
                        // call void @_ZN1BC2Ev(%class.B* noundef nonnull align 8 dereferenceable(8) %0) #8, !dbg !385
                        if (cppClassNameToType(cppUtil::demangle(fooName).className))
                            insertInferSite(callBase);
                    } else {
                        u32_t pos = getArgNoInCallBase(callBase, curValue);
                        // for variable argument, conservatively collect all params
                        if (calleeFunc->isVarArg()) pos = 0;
                        if (!calleeFunc->isDeclaration()) {
                            insertInferSitesOrPushWorklist(calleeFunc->getArg(pos));
                        }
                    }
                }
            }
        }
        if (canUpdate) {
            std::vector<const Type *> types(infersites.size());
            std::transform(infersites.begin(), infersites.end(), types.begin(), getTypeInference()->infersiteToType);
            _valueToInferSites[curValue] = SVFUtil::move(infersites);
            _valueToType[curValue] = selectLargestType(types);
        }
    }
    const Type *type = _valueToType[startValue];
    if (type == nullptr) {
        type = defaultTy(startValue);
        WARN_MSG("Using default type, trace ID is " + std::to_string(traceId) + ":" + VALUE_WITH_DBGINFO(startValue));
    }
    return type;
}

/*!
 * Backward collect all possible allocation sites (stack, static, heap) starting from a value
 * @param startValue 
 * @return 
 */
Set<const Value *> TypeInference::bwGetOrfindAllocations(const Value *startValue) {

    // consult cache
    auto tIt = _valueToAllocs.find(startValue);
    if (tIt != _valueToAllocs.end()) {
        WARN_IFNOT(!tIt->second.empty(), "empty type:" + VALUE_WITH_DBGINFO(startValue));
        return !tIt->second.empty() ? tIt->second : Set<const Value *>({startValue});
    }

    // simulate the call stack, the second element indicates whether we should update sources for current value
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({startValue, false});
    while (!workList.empty()) {
        auto curPair = workList.pop();
        if (visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;

        Set<const Value *> sources;
        auto insertAllocs = [&sources, &canUpdate](const Value *source) {
            if (canUpdate) sources.insert(source);
        };
        auto insertAllocsOrPushWorklist = [this, &sources, &workList, &canUpdate](const auto &pUser) {
            auto vIt = _valueToAllocs.find(pUser);
            if (canUpdate) {
                if (vIt != _valueToAllocs.end()) {
                    sources.insert(vIt->second.begin(), vIt->second.end());
                }
            } else {
                if (vIt == _valueToAllocs.end()) workList.push({pUser, false});
            }
        };

        if (!canUpdate && !_valueToAllocs.count(curValue)) {
            workList.push({curValue, true});
        }

        if (isAllocation(curValue)) {
            insertAllocs(curValue);
        } else if (const BitCastInst *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(curValue)) {
            Value *prevVal = bitCastInst->getOperand(0);
            insertAllocsOrPushWorklist(prevVal);
        } else if (const PHINode *phiNode = SVFUtil::dyn_cast<PHINode>(curValue)) {
            for (u32_t i = 0; i < phiNode->getNumOperands(); ++i) {
                insertAllocsOrPushWorklist(phiNode->getOperand(i));
            }
        } else if (const LoadInst *loadInst = SVFUtil::dyn_cast<LoadInst>(curValue)) {
            for (const auto &use: loadInst->getPointerOperand()->uses()) {
                if (const StoreInst *storeInst = SVFUtil::dyn_cast<StoreInst>(use.getUser())) {
                    if (storeInst->getPointerOperand() == loadInst->getPointerOperand()) {
                        insertAllocsOrPushWorklist(storeInst->getValueOperand());
                    }
                }
            }
        } else if (const Argument *argument = SVFUtil::dyn_cast<Argument>(curValue)) {
            for (const auto &use: argument->getParent()->uses()) {
                if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(use.getUser())) {
                    // skip function as parameter
                    // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                    if (callBase->getCalledFunction() != argument->getParent()) continue;
                    u32_t pos = argument->getParent()->isVarArg() ? 0 : argument->getArgNo();
                    insertAllocsOrPushWorklist(callBase->getArgOperand(pos));
                }
            }
        } else if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(curValue)) {
            ABORT_IFNOT(!callBase->doesNotReturn(), "callbase does not return:" + VALUE_WITH_DBGINFO(callBase));
            if (Function *callee = callBase->getCalledFunction()) {
                if (!callee->isDeclaration()) {
                    const SVFFunction *svfFunc = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(callee);
                    const Value *pValue = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(svfFunc->getExitBB()->back());
                    const ReturnInst *retInst = SVFUtil::dyn_cast<ReturnInst>(pValue);
                    ABORT_IFNOT(retInst && retInst->getReturnValue(), "not return inst?");
                    insertAllocsOrPushWorklist(retInst->getReturnValue());
                }
            }
        }
        if (canUpdate) {
            _valueToAllocs[curValue] = SVFUtil::move(sources);
        }
    }
    Set<const Value *> srcs = _valueToAllocs[startValue];
    if (srcs.empty()) {
        srcs = {startValue};
        WARN_MSG("Using default type, trace ID is " + std::to_string(traceId) + ":" + VALUE_WITH_DBGINFO(startValue));
    }
    return srcs;
}

/*!
 * Backward collect all possible sources starting from a value
 * @param startValue
 * @return
 */
Set<const Value *> TypeInference::bwGetOrfindCPPSources(const Value *startValue) {

    // consult cache
    auto tIt = _valueToCPPSources.find(startValue);
    if (tIt != _valueToCPPSources.end()) {
        return tIt->second;
    }

    // simulate the call stack, the second element indicates whether we should update sources for current value
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({startValue, false});
    while (!workList.empty()) {
        auto curPair = workList.pop();
        if (visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;

        Set<const Value *> sources;
        auto insertSource = [&sources, &canUpdate](const Value *source) {
            if (canUpdate) sources.insert(source);
        };
        auto insertSourcesOrPushWorklist = [this, &sources, &workList, &canUpdate](const auto &pUser) {
            auto vIt = _valueToCPPSources.find(pUser);
            if (canUpdate) {
                if (vIt != _valueToCPPSources.end()) {
                    sources.insert(vIt->second.begin(), vIt->second.end());
                }
            } else {
                if (vIt == _valueToCPPSources.end()) workList.push({pUser, false});
            }
        };

        if (!canUpdate && !_valueToCPPSources.count(curValue)) {
            workList.push({curValue, true});
        }

        // current inst reside in cpp interested function
        if (const Instruction *inst = SVFUtil::dyn_cast<Instruction>(curValue)) {
            if (const Function *fun = inst->getFunction()) {
                if (isCPPSTLAPI(fun->getName().str()) || isCPPConstructor(fun->getName().str())) {
                    insertSource(fun);
                    if (canUpdate) {
                        _valueToCPPSources[curValue] = SVFUtil::move(sources);
                    }
                    continue;
                }
            }
        }
        if (isCPPSource(curValue)) {
            insertSource(curValue);
        } else if (const GetElementPtrInst *getElementPtrInst = SVFUtil::dyn_cast<GetElementPtrInst>(curValue)) {
            insertSource(getElementPtrInst);
            insertSourcesOrPushWorklist(getElementPtrInst->getPointerOperand());
        } else if (const BitCastInst *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(curValue)) {
            Value *prevVal = bitCastInst->getOperand(0);
            insertSourcesOrPushWorklist(prevVal);
        } else if (const PHINode *phiNode = SVFUtil::dyn_cast<PHINode>(curValue)) {
            for (u32_t i = 0; i < phiNode->getNumOperands(); ++i) {
                insertSourcesOrPushWorklist(phiNode->getOperand(i));
            }
        } else if (const LoadInst *loadInst = SVFUtil::dyn_cast<LoadInst>(curValue)) {
            for (const auto &use: loadInst->getPointerOperand()->uses()) {
                if (const StoreInst *storeInst = SVFUtil::dyn_cast<StoreInst>(use.getUser())) {
                    if (storeInst->getPointerOperand() == loadInst->getPointerOperand()) {
                        insertSourcesOrPushWorklist(storeInst->getValueOperand());
                    }
                }
            }
            // array-1.cpp
            if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(loadInst->getPointerOperand())) {
                if (const Function *calledFunc = callBase->getCalledFunction()) {
                    const std::string &funcName = calledFunc->getName().str();
                    if (isCPPSTLAPI(funcName))
                        insertSource(callBase);
                }
            }
        } else if (const Argument *argument = SVFUtil::dyn_cast<Argument>(curValue)) {
            for (const auto &use: argument->getParent()->uses()) {
                if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(use.getUser())) {
                    // skip function as parameter
                    // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                    if (callBase->getCalledFunction() != argument->getParent()) continue;
                    u32_t pos = argument->getParent()->isVarArg() ? 0 : argument->getArgNo();
                    insertSourcesOrPushWorklist(callBase->getArgOperand(pos));
                }
            }
        } else if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(curValue)) {
            ABORT_IFNOT(!callBase->doesNotReturn(), "callbase does not return:" + VALUE_WITH_DBGINFO(callBase));
            if (Function *callee = callBase->getCalledFunction()) {
                if (!callee->isDeclaration()) {
                    const SVFFunction *svfFunc = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(callee);
                    const Value *pValue = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(svfFunc->getExitBB()->back());
                    const ReturnInst *retInst = SVFUtil::dyn_cast<ReturnInst>(pValue);
                    ABORT_IFNOT(retInst && retInst->getReturnValue(), "not return inst?");
                    insertSourcesOrPushWorklist(retInst->getReturnValue());
                }
            }
        }
        if (canUpdate) {
            _valueToCPPSources[curValue] = SVFUtil::move(sources);
        }
    }
    Set<const Value *> srcs = _valueToCPPSources[startValue];
    return srcs;
}

/*!
 * Validate type inference
 * @param cs : stub malloc function with element number label
 */
void TypeInference::validateTypeCheck(const CallBase *cs) {
    if (const Function *func = cs->getCalledFunction()) {
        if (func->getName().find(TYPEMALLOC) != std::string::npos) {
            const Type *objType = fwGetOrInferLLVMObjType(cs);
            ConstantInt *pInt =
                    SVFUtil::dyn_cast<llvm::ConstantInt>(cs->getOperand(1));
            assert(pInt && "the second argument is a integer");
            u32_t iTyNum = Options::MaxFieldLimit();
            if (SVFUtil::isa<ArrayType>(objType))
                iTyNum = getNumOfElements(objType);
            else if (const StructType *st = SVFUtil::dyn_cast<StructType>(objType)) {
                /// For an C++ class, it can have variant elements depending on the vtable size,
                /// Hence we only handle non-cpp-class object, the type of the cpp class is treated as default PointerType
                if (!classTyHasVTable(st))
                    iTyNum = getNumOfElements(st);
            }
            if (iTyNum >= pInt->getZExtValue())
                SVFUtil::outs() << SVFUtil::sucMsg("\t SUCCESS :") << VALUE_WITH_DBGINFO(cs)
                                << SVFUtil::pasMsg(" TYPE: ")
                                << dumpType(objType) << "\n";
            else {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << ":" << VALUE_WITH_DBGINFO(cs) << " TYPE: "
                                << dumpType(objType) << "\n";
                abort();
            }
        }
    }
}

void TypeInference::typeSizeDiffTest(const PointerType *oPTy, const Type *iTy, const Value *val) {
#if TYPE_DEBUG
    Type *oTy = getPtrElementType(oPTy);
    u32_t iTyNum = Options::MaxFieldLimit();
    if (SVFUtil::isa<ArrayType>(iTy))
        iTyNum = getNumOfElements(iTy);
    else if (const StructType *st = SVFUtil::dyn_cast<StructType>(iTy)) {
        /// For an C++ class, it can have variant elements depending on the vtable size,
        /// Hence we only handle non-cpp-class object, the type of the cpp class is treated as default PointerType
        if (!classTyHasVTable(st))
            iTyNum = getNumOfElements(st);
    }
    if (getNumOfElements(oTy) > iTyNum) {
        ERR_MSG("original type is:" + dumpType(oTy));
        ERR_MSG("infered type is:" + dumpType(iTy));
        ABORT_MSG("wrong type, trace ID is " + std::to_string(traceId) + ":" + VALUE_WITH_DBGINFO(val));
    }
#endif
}

void TypeInference::typeDiffTest(const PointerType *oPTy, const Type *iTy, const Value *val) {
#if TYPE_DEBUG
    Type *oTy = getPtrElementType(oPTy);
    if (oTy != iTy) {
        ERR_MSG("original type is:" + dumpType(oTy));
        ERR_MSG("infered type is:" + dumpType(iTy));
        ABORT_MSG("wrong type, trace ID is " + std::to_string(traceId) + ":" + VALUE_WITH_DBGINFO(val));
    }
#endif
}

/// Determine type based on infer site
/// https://llvm.org/docs/OpaquePointers.html#migration-instructions
const Type *TypeInference::infersiteToType(const Value *val) {
    assert(val && "value cannot be empty");
    if (SVFUtil::isa<LoadInst, StoreInst>(val)) {
        return llvm::getLoadStoreType(const_cast<Value *>(val));
    } else if (const GetElementPtrInst *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(val)) {
        return gepInst->getSourceElementType();
    } else if (const CallBase *call = SVFUtil::dyn_cast<CallBase>(val)) {
        if (const Function *calledFunc = call->getCalledFunction()) {
            if (isCPPConstructor(calledFunc->getName().str())) {
                const Type *classTy = cppClassNameToType(cppUtil::demangle(calledFunc->getName().str()).className);
                ABORT_IFNOT(classTy, "does not have a class type?");
                return classTy;
            } else {
                return call->getFunctionType();
            }
        } else {
            return call->getFunctionType();
        }
    } else if (const AllocaInst *allocaInst = SVFUtil::dyn_cast<AllocaInst>(val)) {
        return allocaInst->getAllocatedType();
    } else if (const GlobalValue *globalValue = SVFUtil::dyn_cast<GlobalValue>(val)) {
        return globalValue->getValueType();
    } else {
        ABORT_MSG("unknown value:" + VALUE_WITH_DBGINFO(val));
    }
}

const std::string extractClassNameInSTL(const std::string &demangledStr) {
    // "std::array<A const*, 2ul>" -> A
    // "std::queue<A*, std::deque<A*, std::allocator<A*> > >" -> A
    s32_t leftPos = 0, rightPos = 0;
    while (leftPos < (s32_t) demangledStr.size() && demangledStr[leftPos] != '<') {
        leftPos++;
    }
    rightPos = leftPos;
    while (rightPos < (s32_t) demangledStr.size() && demangledStr[rightPos] != '*' && demangledStr[rightPos] != ',' &&
           demangledStr[rightPos] != ' ' && demangledStr[rightPos] != '>') {
        rightPos++;
    }
    if (leftPos + 1 < (s32_t) demangledStr.size() && rightPos - leftPos - 1 >= 0)
        return demangledStr.substr(leftPos + 1, rightPos - leftPos - 1);
    return "";
}

const std::string TypeInference::extractClassNameViaCppCallee(const Function *callee) {
    const std::string &name = callee->getName().str();
    if (isCPPConstructor(name)) {
        // c++ constructor
        return cppUtil::demangle(name).className;
    } else if (isCPPSTLAPI(name)) {
        // array index
        const std::string &demangledStr = cppUtil::demangle(name).className;
        const std::string &className = extractClassNameInSTL(demangledStr);
        ABORT_IFNOT(className != "", demangledStr);
        return className;
    }
    return "";
}

const Type *TypeInference::cppClassNameToType(const std::string &className) {
    return StructType::getTypeByName(getLLVMCtx(), classTyPrefix + className);
}

const std::string TypeInference::typeToCppClassName(const Type *ty) {
    if (const StructType *stTy = SVFUtil::dyn_cast<StructType>(ty)) {
        const std::string &typeName = stTy->getName().str();
        const std::string &className = typeName.substr(
                classTyPrefix.size(), typeName.size() - classTyPrefix.size());
        return className;
    }
    return "";
}

bool TypeInference::isCPPSource(const Value *val) {
    if (isAllocation(val)) return true;
    if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(val)) {
        const std::string &name = callBase->getCalledFunction()->getName().str();
        if (isCPPConstructor(name) || isCPPSTLAPI(name) || isCPPDynCast(name)) {
            return true;
        }
    }
    return false;
}

bool TypeInference::matchMangler(const std::string &str, const std::string &label) {
    return str.compare(0, label.size(), label) == 0;
}

bool TypeInference::isCPPConstructor(const std::string &str) {
    return matchMangler(str, zn1Label);
}

bool TypeInference::isCPPSTLAPI(const std::string &str) {
    return matchMangler(str, znstLabel) || matchMangler(str, znkst5Label);
}

bool TypeInference::isCPPDynCast(const std::string &str) {
    return str == dyncast;
}