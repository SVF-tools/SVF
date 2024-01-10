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
#define ABORT_MSG(msg)                                                         \
    do                                                                         \
    {                                                                          \
        SVFUtil::errs() << __FILE__ << ':' << __LINE__ << ": " << msg          \
                        << '\n';                                               \
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

const std::string TYPEMALLOC = "TYPE_MALLOC";

/// Determine type based on infer site
/// https://llvm.org/docs/OpaquePointers.html#migration-instructions
const Type *TypeInference::infersiteToType(const Value *val) {
    assert(val && "value cannot be empty");
    if (SVFUtil::isa<LoadInst>(val) || SVFUtil::isa<StoreInst>(val)) {
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
        ABORT_IFNOT(false, "unknown value:" + VALUE_WITH_DBGINFO(val));
    }
}

/*!
 * Forward collect all possible infer sites starting from a value
 * @param startValue
 */
const Type *TypeInference::getOrInferLLVMObjType(const Value *startValue) {
    // consult cache
    auto tIt = _valueToType.find(startValue);
    if (tIt != _valueToType.end()) {
        WARN_IFNOT(tIt->second, "empty type:" + VALUE_WITH_DBGINFO(startValue));
        return tIt->second;
    }

    INC_TRACE();

    // simulate the call stack, the second element indicates whether we should update valueTypes for current value
    typedef std::pair<const Value *, bool> ValueBoolPair;
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
                      */
                    if (GetElementPtrInst *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(
                            storeInst->getPointerOperand()))
                        insertInferSite(gepInst);
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
                u32_t pos = getArgNoInCallBase(callBase, curValue);
                if (Function *calleeFunc = callBase->getCalledFunction()) {
                    // for variable argument, conservatively collect all params
                    if (calleeFunc->isVarArg()) pos = 0;
                    if (!calleeFunc->isDeclaration()) {
                        insertInferSitesOrPushWorklist(calleeFunc->getArg(pos));
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
    const Type* type = _valueToType[startValue];
    if (type == nullptr) {
        WARN_MSG("empty type, trace ID is " + std::to_string(traceId) + ":" + VALUE_WITH_DBGINFO(startValue));
    }
    return type;
}


/*!
 * Validate type inference
 * @param cs : stub malloc function with element number label
 */
void TypeInference::validateTypeCheck(const CallBase *cs) {
    if (const Function *func = cs->getCalledFunction()) {
        if (func->getName().find(TYPEMALLOC) != std::string::npos) {
            const Type *objType = getOrInferLLVMObjType(cs);
            if (!objType) {
                // return an 8-bit integer type if the inferred type is empty
                objType = Type::getInt8Ty(LLVMModuleSet::getLLVMModuleSet()->getContext());
            }
            ConstantInt *pInt =
                    SVFUtil::dyn_cast<llvm::ConstantInt>(cs->getOperand(1));
            assert(pInt && "the second argument is a integer");
            if (getNumOfElements(objType) >= pInt->getZExtValue())
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

void TypeInference::typeDiffTest(const Type *oTy, const Type *iTy, const Value *val) {
#if TYPE_DEBUG
    ABORT_IFNOT(getNumOfElements(oTy) <= getNumOfElements(iTy),
                "wrong type, trace ID is " + std::to_string(traceId) + ":" + VALUE_WITH_DBGINFO(val));
#endif
}