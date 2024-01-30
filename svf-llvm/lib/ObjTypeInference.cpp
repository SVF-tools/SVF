//===- ObjTypeInference.cpp -- Type inference----------------------------//
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
 * ObjTypeInference.cpp
 *
 *  Created by Xiao Cheng on 10/01/24.
 *
 */

#include "SVF-LLVM/ObjTypeInference.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/CppUtil.h"

#define TYPE_DEBUG 0 /* Turn this on if you're debugging type inference */
#define ERR_MSG(msg)                                                           \
    do                                                                         \
    {                                                                          \
        SVFUtil::errs() << SVFUtil::errMsg("Error ") << __FILE__ << ':'        \
            << __LINE__ << ": " << (msg) << '\n';                              \
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
using namespace cppUtil;


const std::string TYPEMALLOC = "TYPE_MALLOC";

/// Determine type based on infer site
/// https://llvm.org/docs/OpaquePointers.html#migration-instructions
const Type *infersiteToType(const Value *val)
{
    assert(val && "value cannot be empty");
    if (SVFUtil::isa<LoadInst, StoreInst>(val))
    {
        return llvm::getLoadStoreType(const_cast<Value *>(val));
    }
    else if (const auto *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(val))
    {
        return gepInst->getSourceElementType();
    }
    else if (const auto *call = SVFUtil::dyn_cast<CallBase>(val))
    {
        return call->getFunctionType();
    }
    else if (const auto *allocaInst = SVFUtil::dyn_cast<AllocaInst>(val))
    {
        return allocaInst->getAllocatedType();
    }
    else if (const auto *globalValue = SVFUtil::dyn_cast<GlobalValue>(val))
    {
        return globalValue->getValueType();
    }
    else
    {
        ABORT_MSG("unknown value:" + dumpValueAndDbgInfo(val));
    }
}

const Type *ObjTypeInference::defaultType(const Value *val)
{
    ABORT_IFNOT(val, "val cannot be null");
    // heap has a default type of 8-bit integer type
    if (SVFUtil::isa<Instruction>(val) && SVFUtil::isHeapAllocExtCallViaRet(
                LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<Instruction>(val))))
        return int8Type();
    // otherwise we return a pointer type in the default address space
    return ptrType();
}

LLVMContext &ObjTypeInference::getLLVMCtx()
{
    return LLVMModuleSet::getLLVMModuleSet()->getContext();
}

/*!
 * get or infer the type of the object pointed by var
 * if the start value is a source (alloc/global, heap, static), call fwInferObjType
 * if not, find allocations and then forward get or infer types
 * @param val
 */
const Type *ObjTypeInference::inferObjType(const Value *var)
{
    if (isAlloc(var)) return fwInferObjType(var);
    Set<const Value *> &sources = bwfindAllocOfVar(var);
    Set<const Type *> types;
    if (sources.empty())
    {
        // cannot find allocation, try to fw infer starting from var
        types.insert(fwInferObjType(var));
    }
    else
    {
        for (const auto &source: sources)
        {
            types.insert(fwInferObjType(source));
        }
    }
    const Type *largestTy = selectLargestSizedType(types);
    ABORT_IFNOT(largestTy, "return type cannot be null");
    return largestTy;
}

/*!
 * forward infer the type of the object pointed by var
 * @param var
 */
const Type *ObjTypeInference::fwInferObjType(const Value *var)
{
    // consult cache
    auto tIt = _valueToType.find(var);
    if (tIt != _valueToType.end())
    {
        return tIt->second ? tIt->second : defaultType(var);
    }

    // simulate the call stack, the second element indicates whether we should update valueTypes for current value
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({var, false});

    while (!workList.empty())
    {
        auto curPair = workList.pop();
        if (visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;
        Set<const Value *> infersites;

        auto insertInferSite = [&infersites, &canUpdate](const Value *infersite)
        {
            if (canUpdate) infersites.insert(infersite);
        };
        auto insertInferSitesOrPushWorklist = [this, &infersites, &workList, &canUpdate](const auto &pUser)
        {
            auto vIt = _valueToInferSites.find(pUser);
            if (canUpdate)
            {
                if (vIt != _valueToInferSites.end())
                {
                    infersites.insert(vIt->second.begin(), vIt->second.end());
                }
            }
            else
            {
                if (vIt == _valueToInferSites.end()) workList.push({pUser, false});
            }
        };
        if (!canUpdate && !_valueToInferSites.count(curValue))
        {
            workList.push({curValue, true});
        }
        if (const auto *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(curValue))
            insertInferSite(gepInst);
        for (const auto &it: curValue->uses())
        {
            if (const auto *loadInst = SVFUtil::dyn_cast<LoadInst>(it.getUser()))
            {
                /*
                 * infer based on load, e.g.,
                 %call = call i8* malloc()
                 %1 = bitcast i8* %call to %struct.MyStruct*
                 %q = load %struct.MyStruct, %struct.MyStruct* %1
                 */
                insertInferSite(loadInst);
            }
            else if (const auto *storeInst = SVFUtil::dyn_cast<StoreInst>(it.getUser()))
            {
                if (storeInst->getPointerOperand() == curValue)
                {
                    /*
                     * infer based on store (pointer operand), e.g.,
                     %call = call i8* malloc()
                     %1 = bitcast i8* %call to %struct.MyStruct*
                     store %struct.MyStruct .., %struct.MyStruct* %1
                     */
                    insertInferSite(storeInst);
                }
                else
                {
                    for (const auto &nit: storeInst->getPointerOperand()->uses())
                    {
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
                    if (const auto *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(
                                                  storeInst->getPointerOperand()))
                    {
                        const Value *gepBase = gepInst->getPointerOperand();
                        if (!SVFUtil::isa<LoadInst>(gepBase)) continue;
                        const auto *load = SVFUtil::dyn_cast<LoadInst>(gepBase);
                        for (const auto &loadUse: load->getPointerOperand()->uses())
                        {
                            if (loadUse.getUser() == load || !SVFUtil::isa<LoadInst>(loadUse.getUser()))
                                continue;
                            for (const auto &gepUse: loadUse.getUser()->uses())
                            {
                                if (!SVFUtil::isa<GetElementPtrInst>(gepUse.getUser())) continue;
                                for (const auto &loadUse2: gepUse.getUser()->uses())
                                {
                                    if (SVFUtil::isa<LoadInst>(loadUse2.getUser()))
                                    {
                                        insertInferSitesOrPushWorklist(loadUse2.getUser());
                                    }
                                }
                            }
                        }

                    }
                }

            }
            else if (const auto *gepInst = SVFUtil::dyn_cast<GetElementPtrInst>(it.getUser()))
            {
                /*
                 * infer based on gep (pointer operand)
                 %call = call i8* malloc()
                 %1 = bitcast i8* %call to %struct.MyStruct*
                 %next = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %1, i32 0..
                 */
                if (gepInst->getPointerOperand() == curValue)
                    insertInferSite(gepInst);
            }
            else if (const auto *bitcast = SVFUtil::dyn_cast<BitCastInst>(it.getUser()))
            {
                // continue on bitcast
                insertInferSitesOrPushWorklist(bitcast);
            }
            else if (const auto *phiNode = SVFUtil::dyn_cast<PHINode>(it.getUser()))
            {
                // continue on bitcast
                insertInferSitesOrPushWorklist(phiNode);
            }
            else if (const auto *retInst = SVFUtil::dyn_cast<ReturnInst>(it.getUser()))
            {
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
                for (const auto &callsite: retInst->getFunction()->uses())
                {
                    if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(callsite.getUser()))
                    {
                        // skip function as parameter
                        // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                        if (callBase->getCalledFunction() != retInst->getFunction()) continue;
                        insertInferSitesOrPushWorklist(callBase);
                    }
                }
            }
            else if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(it.getUser()))
            {
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
                if (Function *calleeFunc = callBase->getCalledFunction())
                {
                    u32_t pos = getArgPosInCall(callBase, curValue);
                    // for variable argument, conservatively collect all params
                    if (calleeFunc->isVarArg()) pos = 0;
                    if (!calleeFunc->isDeclaration())
                    {
                        insertInferSitesOrPushWorklist(calleeFunc->getArg(pos));
                    }
                }
            }
        }
        if (canUpdate)
        {
            Set<const Type *> types;
            std::transform(infersites.begin(), infersites.end(), std::inserter(types, types.begin()),
                           infersiteToType);
            _valueToInferSites[curValue] = SVFUtil::move(infersites);
            _valueToType[curValue] = selectLargestSizedType(types);
        }
    }
    const Type *type = _valueToType[var];
    if (type == nullptr)
    {
        type = defaultType(var);
        WARN_MSG("Using default type, trace ID is " + std::to_string(traceId) + ":" + dumpValueAndDbgInfo(var));
    }
    ABORT_IFNOT(type, "type cannot be a null ptr");
    return type;
}

/*!
 * backward collect all possible allocation sites (stack, static, heap) of var
 * @param var
 * @return
 */
Set<const Value *> &ObjTypeInference::bwfindAllocOfVar(const Value *var)
{

    // consult cache
    auto tIt = _valueToAllocs.find(var);
    if (tIt != _valueToAllocs.end())
    {
        return tIt->second;
    }

    // simulate the call stack, the second element indicates whether we should update sources for current value
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({var, false});
    while (!workList.empty())
    {
        auto curPair = workList.pop();
        if (visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;

        Set<const Value *> sources;
        auto insertAllocs = [&sources, &canUpdate](const Value *source)
        {
            if (canUpdate) sources.insert(source);
        };
        auto insertAllocsOrPushWorklist = [this, &sources, &workList, &canUpdate](const auto &pUser)
        {
            auto vIt = _valueToAllocs.find(pUser);
            if (canUpdate)
            {
                if (vIt != _valueToAllocs.end())
                {
                    sources.insert(vIt->second.begin(), vIt->second.end());
                }
            }
            else
            {
                if (vIt == _valueToAllocs.end()) workList.push({pUser, false});
            }
        };

        if (!canUpdate && !_valueToAllocs.count(curValue))
        {
            workList.push({curValue, true});
        }

        if (isAlloc(curValue))
        {
            insertAllocs(curValue);
        }
        else if (const auto *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(curValue))
        {
            Value *prevVal = bitCastInst->getOperand(0);
            insertAllocsOrPushWorklist(prevVal);
        }
        else if (const auto *phiNode = SVFUtil::dyn_cast<PHINode>(curValue))
        {
            for (u32_t i = 0; i < phiNode->getNumOperands(); ++i)
            {
                insertAllocsOrPushWorklist(phiNode->getOperand(i));
            }
        }
        else if (const auto *loadInst = SVFUtil::dyn_cast<LoadInst>(curValue))
        {
            for (const auto &use: loadInst->getPointerOperand()->uses())
            {
                if (const StoreInst *storeInst = SVFUtil::dyn_cast<StoreInst>(use.getUser()))
                {
                    if (storeInst->getPointerOperand() == loadInst->getPointerOperand())
                    {
                        insertAllocsOrPushWorklist(storeInst->getValueOperand());
                    }
                }
            }
        }
        else if (const auto *argument = SVFUtil::dyn_cast<Argument>(curValue))
        {
            for (const auto &use: argument->getParent()->uses())
            {
                if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(use.getUser()))
                {
                    // skip function as parameter
                    // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                    if (callBase->getCalledFunction() != argument->getParent()) continue;
                    u32_t pos = argument->getParent()->isVarArg() ? 0 : argument->getArgNo();
                    insertAllocsOrPushWorklist(callBase->getArgOperand(pos));
                }
            }
        }
        else if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(curValue))
        {
            ABORT_IFNOT(!callBase->doesNotReturn(), "callbase does not return:" + dumpValueAndDbgInfo(callBase));
            if (Function *callee = callBase->getCalledFunction())
            {
                if (!callee->isDeclaration())
                {
                    const SVFFunction *svfFunc = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(callee);
                    const Value *pValue = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(svfFunc->getExitBB()->back());
                    const auto *retInst = SVFUtil::dyn_cast<ReturnInst>(pValue);
                    ABORT_IFNOT(retInst && retInst->getReturnValue(), "not return inst?");
                    insertAllocsOrPushWorklist(retInst->getReturnValue());
                }
            }
        }
        if (canUpdate)
        {
            _valueToAllocs[curValue] = SVFUtil::move(sources);
        }
    }
    Set<const Value *> &srcs = _valueToAllocs[var];
    if (srcs.empty())
    {
        WARN_MSG("Cannot find allocation: " + dumpValueAndDbgInfo(var));
    }
    return srcs;
}

bool ObjTypeInference::isAlloc(const SVF::Value *val)
{
    return LLVMUtil::isObject(val);
}

/*!
 * validate type inference
 * @param cs : stub malloc function with element number label
 */
void ObjTypeInference::validateTypeCheck(const CallBase *cs)
{
    if (const Function *func = cs->getCalledFunction())
    {
        if (func->getName().find(TYPEMALLOC) != std::string::npos)
        {
            const Type *objType = fwInferObjType(cs);
            const auto *pInt =
                SVFUtil::dyn_cast<llvm::ConstantInt>(cs->getOperand(1));
            assert(pInt && "the second argument is a integer");
            u32_t iTyNum = objTyToNumFields(objType);
            if (iTyNum >= pInt->getZExtValue())
                SVFUtil::outs() << SVFUtil::sucMsg("\t SUCCESS :") << dumpValueAndDbgInfo(cs)
                                << SVFUtil::pasMsg(" TYPE: ")
                                << dumpType(objType) << "\n";
            else
            {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << ":" << dumpValueAndDbgInfo(cs) << " TYPE: "
                                << dumpType(objType) << "\n";
                abort();
            }
        }
    }
}

void ObjTypeInference::typeSizeDiffTest(const PointerType *oPTy, const Type *iTy, const Value *val)
{
#if TYPE_DEBUG
    Type *oTy = getPtrElementType(oPTy);
    u32_t iTyNum = objTyToNumFields(iTy);
    if (getNumOfElements(oTy) > iTyNum)
    {
        ERR_MSG("original type is:" + dumpType(oTy));
        ERR_MSG("infered type is:" + dumpType(iTy));
        ABORT_MSG("wrong type, trace ID is " + std::to_string(traceId) + ":" + dumpValueAndDbgInfo(val));
    }
#endif
}

u32_t ObjTypeInference::getArgPosInCall(const CallBase *callBase, const Value *arg)
{
    assert(callBase->hasArgument(arg) && "callInst does not have argument arg?");
    auto it = std::find(callBase->arg_begin(), callBase->arg_end(), arg);
    assert(it != callBase->arg_end() && "Didn't find argument?");
    return std::distance(callBase->arg_begin(), it);
}


const Type *ObjTypeInference::selectLargestSizedType(Set<const Type *> &objTys)
{
    if (objTys.empty()) return nullptr;
    // map type size to types from with key in descending order
    OrderedMap<u32_t, OrderedSet<const Type *>, std::greater<int>> typeSzToTypes;
    for (const Type *ty: objTys)
    {
        typeSzToTypes[objTyToNumFields(ty)].insert(ty);
    }
    assert(!typeSzToTypes.empty() && "typeSzToTypes cannot be empty");
    OrderedSet<const Type *> largestTypes;
    std::tie(std::ignore, largestTypes) = *typeSzToTypes.begin();
    assert(!largestTypes.empty() && "largest element cannot be empty");
    return *largestTypes.begin();
}

u32_t ObjTypeInference::objTyToNumFields(const Type *objTy)
{
    u32_t num = Options::MaxFieldLimit();
    if (SVFUtil::isa<ArrayType>(objTy))
        num = getNumOfElements(objTy);
    else if (const auto *st = SVFUtil::dyn_cast<StructType>(objTy))
    {
        /// For an C++ class, it can have variant elements depending on the vtable size,
        /// Hence we only handle non-cpp-class object, the type of the cpp class is treated as default PointerType
        if (!classTyHasVTable(st))
            num = getNumOfElements(st);
    }
    return num;
}


/*!
 * get or infer the class names of thisptr
 * @param thisPtr
 * @return
 */
Set<std::string> &ObjTypeInference::inferThisPtrClsName(const Value *thisPtr)
{
    auto it = _thisPtrClassNames.find(thisPtr);
    if (it != _thisPtrClassNames.end()) return it->second;

    Set<std::string> names;
    auto insertClassNames = [&names](Set<std::string> &classNames)
    {
        names.insert(classNames.begin(), classNames.end());
    };

    // backward find heap allocations or class name sources
    Set<const Value *> &vals = bwFindAllocOrClsNameSources(thisPtr);
    for (const auto &val: vals)
    {
        if (val == thisPtr) continue;

        if (const auto *func = SVFUtil::dyn_cast<Function>(val))
        {
            // extract class name from function name
            Set<std::string> classNames = extractClsNamesFromFunc(func);
            insertClassNames(classNames);
        }
        else if (SVFUtil::isa<LoadInst, StoreInst, GetElementPtrInst, AllocaInst, GlobalValue>(val))
        {
            // extract class name from instructions
            const Type *type = infersiteToType(val);
            const std::string &className = typeToClsName(type);
            if (!className.empty())
            {
                Set<std::string> tgt{className};
                insertClassNames(tgt);
            }
        }
        else if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(val))
        {
            if (const Function *callFunc = callBase->getCalledFunction())
            {
                Set<std::string> classNames = extractClsNamesFromFunc(callFunc);
                insertClassNames(classNames);
                if (isDynCast(callFunc))
                {
                    // dynamic cast
                    Set<std::string> tgt{extractClsNameFromDynCast(callBase)};
                    insertClassNames(tgt);
                }
                else if (isNewAlloc(callFunc))
                {
                    // for heap allocation, we forward find class name sources
                    Set<const Function *>& srcs = fwFindClsNameSources(callBase);
                    for (const auto &src: srcs)
                    {
                        classNames = extractClsNamesFromFunc(src);
                        insertClassNames(classNames);
                    }
                }
            }
        }
    }
    return _thisPtrClassNames[thisPtr] = names;
}

/*!
 * find all possible allocations or class name sources
 * (e.g., constructors/destructors or template functions)
 * if we already find class name sources, we don't need to find the allocations and forward find class name sources
 * @param startValue
 * @return
 */
Set<const Value *> &ObjTypeInference::bwFindAllocOrClsNameSources(const Value *startValue)
{

    // consult cache
    auto tIt = _valueToAllocOrClsNameSources.find(startValue);
    if (tIt != _valueToAllocOrClsNameSources.end())
    {
        return tIt->second;
    }

    // simulate the call stack, the second element indicates whether we should update sources for current value
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({startValue, false});
    while (!workList.empty())
    {
        auto curPair = workList.pop();
        if (visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;

        Set<const Value *> sources;
        auto insertSource = [&sources, &canUpdate](const Value *source)
        {
            if (canUpdate) sources.insert(source);
        };
        auto insertSourcesOrPushWorklist = [this, &sources, &workList, &canUpdate](const auto &pUser)
        {
            auto vIt = _valueToAllocOrClsNameSources.find(pUser);
            if (canUpdate)
            {
                if (vIt != _valueToAllocOrClsNameSources.end() && !vIt->second.empty())
                {
                    sources.insert(vIt->second.begin(), vIt->second.end());
                }
            }
            else
            {
                if (vIt == _valueToAllocOrClsNameSources.end()) workList.push({pUser, false});
            }
        };

        if (!canUpdate && !_valueToAllocOrClsNameSources.count(curValue))
        {
            workList.push({curValue, true});
        }

        // current inst reside in cpp self-inference function
        if (const auto *inst = SVFUtil::dyn_cast<Instruction>(curValue))
        {
            if (const Function *foo = inst->getFunction())
            {
                if (isConstructor(foo) || isDestructor(foo) || isTemplateFunc(foo) || isDynCast(foo))
                {
                    insertSource(foo);
                    if (canUpdate)
                    {
                        _valueToAllocOrClsNameSources[curValue] = sources;
                    }
                    continue;
                }
            }
        }
        if (isAlloc(curValue) || isClsNameSource(curValue))
        {
            insertSource(curValue);
        }
        else if (const auto *getElementPtrInst = SVFUtil::dyn_cast<GetElementPtrInst>(curValue))
        {
            insertSource(getElementPtrInst);
            insertSourcesOrPushWorklist(getElementPtrInst->getPointerOperand());
        }
        else if (const auto *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(curValue))
        {
            Value *prevVal = bitCastInst->getOperand(0);
            insertSourcesOrPushWorklist(prevVal);
        }
        else if (const auto *phiNode = SVFUtil::dyn_cast<PHINode>(curValue))
        {
            for (u32_t i = 0; i < phiNode->getNumOperands(); ++i)
            {
                insertSourcesOrPushWorklist(phiNode->getOperand(i));
            }
        }
        else if (const auto *loadInst = SVFUtil::dyn_cast<LoadInst>(curValue))
        {
            for (const auto &use: loadInst->getPointerOperand()->uses())
            {
                if (const auto *storeInst = SVFUtil::dyn_cast<StoreInst>(use.getUser()))
                {
                    if (storeInst->getPointerOperand() == loadInst->getPointerOperand())
                    {
                        insertSourcesOrPushWorklist(storeInst->getValueOperand());
                    }
                }
            }
        }
        else if (const auto *argument = SVFUtil::dyn_cast<Argument>(curValue))
        {
            for (const auto &use: argument->getParent()->uses())
            {
                if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(use.getUser()))
                {
                    // skip function as parameter
                    // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                    if (callBase->getCalledFunction() != argument->getParent()) continue;
                    u32_t pos = argument->getParent()->isVarArg() ? 0 : argument->getArgNo();
                    insertSourcesOrPushWorklist(callBase->getArgOperand(pos));
                }
            }
        }
        else if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(curValue))
        {
            ABORT_IFNOT(!callBase->doesNotReturn(), "callbase does not return:" + dumpValueAndDbgInfo(callBase));
            if (Function *callee = callBase->getCalledFunction())
            {
                if (!callee->isDeclaration())
                {
                    const SVFFunction *svfFunc = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(callee);
                    const Value *pValue = LLVMModuleSet::getLLVMModuleSet()->getLLVMValue(svfFunc->getExitBB()->back());
                    const auto *retInst = SVFUtil::dyn_cast<ReturnInst>(pValue);
                    ABORT_IFNOT(retInst && retInst->getReturnValue(), "not return inst?");
                    insertSourcesOrPushWorklist(retInst->getReturnValue());
                }
            }
        }
        if (canUpdate)
        {
            _valueToAllocOrClsNameSources[curValue] = sources;
        }
    }
    return _valueToAllocOrClsNameSources[startValue];
}

Set<const Function *> &ObjTypeInference::fwFindClsNameSources(const CallBase *alloc)
{
    // consult cache
    auto tIt = _allocToClsNameSources.find(alloc);
    if (tIt != _allocToClsNameSources.end())
    {
        return tIt->second;
    }

    Set<const Function *> clsSources;
    // for heap allocation, we forward find class name sources
    auto inferViaCppCall = [&clsSources](const CallBase *callBase)
    {
        if (!callBase->getCalledFunction()) return;
        const Function *constructFoo = callBase->getCalledFunction();
        clsSources.insert(constructFoo);
    };
    for (const auto &use: alloc->uses())
    {
        if (const auto *cppCall = SVFUtil::dyn_cast<CallBase>(use.getUser()))
        {
            inferViaCppCall(cppCall);
        }
        else if (const auto *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(use.getUser()))
        {
            for (const auto &use2: bitCastInst->uses())
            {
                if (const auto *cppCall2 = SVFUtil::dyn_cast<CallBase>(use2.getUser()))
                {
                    inferViaCppCall(cppCall2);
                }
            }
        }
    }
    return _allocToClsNameSources[alloc] = SVFUtil::move(clsSources);
}