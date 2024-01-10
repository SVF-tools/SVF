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
void TypeInference::forwardCollectAllInfersites(const Value *startValue) {
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
                    if (CallInst *callInst = SVFUtil::dyn_cast<CallInst>(callsite.getUser()))
                        insertInferSitesOrPushWorklist(callInst);
                }
            } else if (CallInst *callInst = SVFUtil::dyn_cast<CallInst>(it.getUser())) {
                /*
                 * propagate from callsite to callee
                  %call = call i8* @malloc(i32 noundef 16)
                  %0 = bitcast i8* %call to %struct.Node*, !dbg !43
                  call void @foo(%struct.Node* noundef %0), !dbg !45

                  define dso_local void @foo(%struct.Node* noundef %param) #0 !dbg !22 {...}
                  ..infer based on the formal param %param..
                 */
                u32_t pos = getArgNoInCallInst(callInst, curValue);
                if (Function *calleeFunc = callInst->getCalledFunction()) {
                    // for variable argument, conservatively collect all params
                    if (calleeFunc->isVarArg()) pos = 0;
                    if (!calleeFunc->isDeclaration()) {
                        insertInferSitesOrPushWorklist(calleeFunc->getArg(pos));
                    }
                }
            }
        }
        if (canUpdate) {
            _valueToInferSites[curValue] = SVFUtil::move(infersites);
        }
    }
}


/*!
 * Validate type inference
 * @param cs : stub malloc function with element number label
 */
void TypeInference::validateTypeCheck(const CallBase *cs) {
    if (const Function *func = cs->getCalledFunction()) {
        if (func->getName().find(TYPEMALLOC) != std::string::npos) {
            forwardCollectAllInfersites(cs);
            auto vTyIt = _valueToInferSites.find(cs);
            if (vTyIt == _valueToInferSites.end()) {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << "empty types, value ID is "
                                << std::to_string(cs->getValueID()) << ":" << VALUE_WITH_DBGINFO(cs) << "\n";
                abort();
            }
            std::vector<const Type *> types(vTyIt->second.size());
            std::transform(vTyIt->second.begin(), vTyIt->second.end(), types.begin(), infersiteToType);
            const Type *pType = selectLargestType(types);
            ConstantInt *pInt =
                    SVFUtil::dyn_cast<llvm::ConstantInt>(cs->getOperand(1));
            assert(pInt && "the second argument is a integer");
            if (getNumOfElements(pType) >= pInt->getZExtValue())
                SVFUtil::outs() << SVFUtil::sucMsg("\t SUCCESS :") << VALUE_WITH_DBGINFO(cs)
                                << SVFUtil::pasMsg(" TYPE: ")
                                << dumpType(pType) << "\n";
            else {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << ", value ID is "
                                << std::to_string(cs->getValueID()) << ":" << VALUE_WITH_DBGINFO(cs) << " TYPE: "
                                << dumpType(pType) << "\n";
                abort();
            }
        }
    }
}