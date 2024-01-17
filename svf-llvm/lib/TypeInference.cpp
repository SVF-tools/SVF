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

#define TYPE_DEBUG 0 /* Turn this on if you're debugging type inference */
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

TypeInference *TypeInference::_typeInference = nullptr;

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
 * get or infer type of a value
 * if the start value is a source (alloc/global, heap, static), call fwInferObjType
 * if not, find sources and then forward get or infer types
 * @param startValue
 */
const Type *TypeInference::inferObjType(const Value *startValue) {
    if (isAllocation(startValue)) return fwInferObjType(startValue);
    Set<const Value *> sources = bwfindAllocations(startValue);
    Set<const Type *> types;
    for (const auto &source: sources) {
        types.insert(fwInferObjType(source));
    }
    return selectLargestType(types);
}

/*!
 * Forward collect all possible infer sites starting from a value
 * @param startValue
 */
const Type *TypeInference::fwInferObjType(const Value *startValue) {
    // consult cache
    auto tIt = _valueToType.find(startValue);
    if (tIt != _valueToType.end()) {
        return tIt->second ? tIt->second : defaultTy(startValue);
    }

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
                    u32_t pos = getArgPosInCall(callBase, curValue);
                    // for variable argument, conservatively collect all params
                    if (calleeFunc->isVarArg()) pos = 0;
                    if (!calleeFunc->isDeclaration()) {
                        insertInferSitesOrPushWorklist(calleeFunc->getArg(pos));
                    }
                }
            }
        }
        if (canUpdate) {
            Set<const Type *> types;
            std::transform(infersites.begin(), infersites.end(), std::inserter(types, types.begin()), getTypeInference()->infersiteToType);
            _valueToInferSites[curValue] = SVFUtil::move(infersites);
            _valueToType[curValue] = selectLargestType(types);
        }
    }
    const Type *type = _valueToType[startValue];
    if (type == nullptr) {
        type = defaultTy(startValue);
        WARN_MSG("Using default type, trace ID is " + std::to_string(traceId) + ":" + dumpValueAndDbgInfo(startValue));
    }
    return type;
}

/*!
 * Backward collect all possible allocation sites (stack, static, heap) starting from a value
 * @param startValue 
 * @return 
 */
Set<const Value *> TypeInference::bwfindAllocations(const Value *startValue) {

    // consult cache
    auto tIt = _valueToAllocs.find(startValue);
    if (tIt != _valueToAllocs.end()) {
        WARN_IFNOT(!tIt->second.empty(), "empty type:" + dumpValueAndDbgInfo(startValue));
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
            ABORT_IFNOT(!callBase->doesNotReturn(), "callbase does not return:" + dumpValueAndDbgInfo(callBase));
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
        WARN_MSG("Using default type, trace ID is " + std::to_string(traceId) + ":" + dumpValueAndDbgInfo(startValue));
    }
    return srcs;
}

/*!
 * Validate type inference
 * @param cs : stub malloc function with element number label
 */
void TypeInference::validateTypeCheck(const CallBase *cs) {
    if (const Function *func = cs->getCalledFunction()) {
        if (func->getName().find(TYPEMALLOC) != std::string::npos) {
            const Type *objType = fwInferObjType(cs);
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
                SVFUtil::outs() << SVFUtil::sucMsg("\t SUCCESS :") << dumpValueAndDbgInfo(cs)
                                << SVFUtil::pasMsg(" TYPE: ")
                                << dumpType(objType) << "\n";
            else {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << ":" << dumpValueAndDbgInfo(cs) << " TYPE: "
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
        ABORT_MSG("wrong type, trace ID is " + std::to_string(traceId) + ":" + dumpValueAndDbgInfo(val));
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
        return call->getFunctionType();
    } else if (const AllocaInst *allocaInst = SVFUtil::dyn_cast<AllocaInst>(val)) {
        return allocaInst->getAllocatedType();
    } else if (const GlobalValue *globalValue = SVFUtil::dyn_cast<GlobalValue>(val)) {
        return globalValue->getValueType();
    } else {
        ABORT_MSG("unknown value:" + dumpValueAndDbgInfo(val));
    }
}

u32_t TypeInference::getArgPosInCall(const CallBase *callBase, const Value *arg) {
    assert(callBase->hasArgument(arg) && "callInst does not have argument arg?");
    auto it = std::find(callBase->arg_begin(), callBase->arg_end(), arg);
    assert(it != callBase->arg_end() && "Didn't find argument?");
    return std::distance(callBase->arg_begin(), it);
}


const Type *TypeInference::selectLargestType(Set<const Type *> &objTys) {
    if (objTys.empty()) return nullptr;
    // map type size to types from with key in descending order
    OrderedMap<u32_t, Set<const Type *>, std::greater<int>> typeSzToTypes;
    for (const Type *ty: objTys) {
        u32_t num = Options::MaxFieldLimit();
        if (SVFUtil::isa<ArrayType>(ty))
            num = getNumOfElements(ty);
        else if (const StructType *st = SVFUtil::dyn_cast<StructType>(ty)) {
            /// For an C++ class, it can have variant elements depending on the vtable size,
            /// Hence we only handle non-cpp-class object, the type of the cpp class is treated as default PointerType
            if (!classTyHasVTable(st))
                num = getNumOfElements(st);
        }
        typeSzToTypes[num].insert(ty);
    }
    assert(!typeSzToTypes.empty() && "typeSzToTypes cannot be empty");
    const std::pair<u32_t, Set<const Type *>> &largestElement = *typeSzToTypes.begin();
    assert(!largestElement.second.empty() && "largest element cannot be empty");
    return *largestElement.second.begin();
}
