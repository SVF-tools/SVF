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
#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/LLVMModule.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/CppUtil.h"
#include "Util/Casting.h"

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
    if (SVFUtil::isa<Instruction>(val) && LLVMUtil::isHeapAllocExtCallViaRet(
                SVFUtil::cast<Instruction>(val)))
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
    const Type* res = inferPointsToType(var);
    // infer type by leveraging the type alignment of src and dst in memcpy
    // for example,
    //
    // %tmp = alloca %struct.outer
    // %inner_v = alloca %struct.inner
    // %ptr = getelementptr inbounds %struct.outer, ptr %tmp, i32 0, i32 1, !dbg !38
    // %0 = load ptr, ptr %ptr, align 8, !dbg !38
    // call void @llvm.memcpy.p0.p0.i64(ptr %inner_v, ptr %0, i64 24, i1 false)
    //
    //  It is difficult to infer the type of %0 without deep alias analysis,
    //  but we can infer the obj type of %0 based on that of %inner_v.
    if (res == defaultType(var))
    {
        for (const auto& use: var->users())
        {
            if (const CallBase* cs = SVFUtil::dyn_cast<CallBase>(use))
            {
                if (const Function* calledFun = cs->getCalledFunction())
                    if (LLVMUtil::isMemcpyExtFun(calledFun))
                    {
                        assert(cs->getNumOperands() > 1 && "arguments should be greater than 1");
                        const Value* dst = cs->getArgOperand(0);
                        const Value* src = cs->getArgOperand(1);
                        if(calledFun->getName().find("iconv") != std::string::npos)
                            dst = cs->getArgOperand(3), src = cs->getArgOperand(1);

                        if (var == dst) return inferPointsToType(src);
                        else if (var == src) return inferPointsToType(dst);
                        else ABORT_MSG("invalid memcpy call");
                    }
            }
        }
    }
    return res;
}

const Type *ObjTypeInference::inferPointsToType(const Value *var)
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
    if (const AllocaInst *allocaInst = SVFUtil::dyn_cast<AllocaInst>(var))
    {
        // stack object
        return infersiteToType(allocaInst);
    }
    else if (const GlobalValue *global = SVFUtil::dyn_cast<GlobalValue>(var))
    {
        // global object
        return infersiteToType(global);
    }
    else
    {
        // for heap or static object, we forward infer its type

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
            if (visited.count(curPair))
                continue;
            visited.insert(curPair);
            const Value* curValue = curPair.first;
            bool canUpdate = curPair.second;
            Set<const Value*> infersites;

            auto insertInferSite = [&infersites,
                                    &canUpdate](const Value* infersite)
            {
                if (canUpdate)
                    infersites.insert(infersite);
            };
            auto insertInferSitesOrPushWorklist =
                [this, &infersites, &workList, &canUpdate](const auto& pUser)
            {
                auto vIt = _valueToInferSites.find(pUser);
                if (canUpdate)
                {
                    if (vIt != _valueToInferSites.end())
                    {
                        infersites.insert(vIt->second.begin(),
                                          vIt->second.end());
                    }
                }
                else
                {
                    if (vIt == _valueToInferSites.end())
                        workList.push({pUser, false});
                }
            };
            if (!canUpdate && !_valueToInferSites.count(curValue))
            {
                workList.push({curValue, true});
            }
            if (const auto* gepInst =
                        SVFUtil::dyn_cast<GetElementPtrInst>(curValue))
                insertInferSite(gepInst);
            for (const auto it : curValue->users())
            {
                if (const auto* loadInst = SVFUtil::dyn_cast<LoadInst>(it))
                {
                    /*
                     * infer based on load, e.g.,
                     %call = call i8* malloc()
                     %1 = bitcast i8* %call to %struct.MyStruct*
                     %q = load %struct.MyStruct, %struct.MyStruct* %1
                     */
                    insertInferSite(loadInst);
                }
                else if (const auto* storeInst =
                             SVFUtil::dyn_cast<StoreInst>(it))
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
                        for (const auto nit :
                                storeInst->getPointerOperand()->users())
                        {
                            /*
                             * propagate across store (value operand) and load
                             %call = call i8* malloc()
                             store i8* %call, i8** %p
                             %q = load i8*, i8** %p
                             ..infer based on %q..
                            */
                            if (SVFUtil::isa<LoadInst>(nit))
                                insertInferSitesOrPushWorklist(nit);
                        }
                        /*
                        * infer based on store (value operand) <- gep (result element)
                         */
                        if (const auto* gepInst =
                                    SVFUtil::dyn_cast<GetElementPtrInst>(
                                        storeInst->getPointerOperand()))
                        {
                            /*
                              %call1 = call i8* @TYPE_MALLOC(i32 noundef 16, i32
                              noundef 2), !dbg !39 %2 = bitcast i8* %call1 to
                              %struct.MyStruct*, !dbg !41 %3 = load
                              %struct.MyStruct*, %struct.MyStruct** %p, align 8,
                              !dbg !42 %next = getelementptr inbounds
                              %struct.MyStruct, %struct.MyStruct* %3, i32 0, i32
                              1, !dbg !43 store %struct.MyStruct* %2,
                              %struct.MyStruct** %next, align 8, !dbg !44 %5 =
                              load %struct.MyStruct*, %struct.MyStruct** %p,
                              align 8, !dbg !48 %next3 = getelementptr inbounds
                              %struct.MyStruct, %struct.MyStruct* %5, i32 0, i32
                              1, !dbg !49 %6 = load %struct.MyStruct*,
                              %struct.MyStruct** %next3, align 8, !dbg !49 infer
                              site -> %f1 = getelementptr inbounds
                              %struct.MyStruct, %struct.MyStruct* %6, i32 0, i32
                              0, !dbg !50
                             */
                            const Value* gepBase = gepInst->getPointerOperand();
                            if (const auto* load =
                                        SVFUtil::dyn_cast<LoadInst>(gepBase))
                            {
                                for (const auto loadUse :
                                        load->getPointerOperand()->users())
                                {
                                    if (loadUse == load ||
                                            !SVFUtil::isa<LoadInst>(loadUse))
                                        continue;
                                    for (const auto gepUse : loadUse->users())
                                    {
                                        if (!SVFUtil::isa<GetElementPtrInst>(
                                                    gepUse))
                                            continue;
                                        for (const auto loadUse2 :
                                                gepUse->users())
                                        {
                                            if (SVFUtil::isa<LoadInst>(
                                                        loadUse2))
                                            {
                                                insertInferSitesOrPushWorklist(
                                                    loadUse2);
                                            }
                                        }
                                    }
                                }
                            }
                            else if (const auto* alloc =
                                         SVFUtil::dyn_cast<AllocaInst>(gepBase))
                            {
                                /*
                                  %2 = alloca %struct.ll, align 8
                                  store i32 0, ptr %1, align 4
                                  %3 = call noalias noundef nonnull ptr
                                  @_Znwm(i64 noundef 16) #2 %4 = getelementptr
                                  inbounds %struct.ll, ptr %2, i32 0, i32 1
                                  store ptr %3, ptr %4, align 8
                                  %5 = getelementptr inbounds %struct.ll, ptr
                                  %2, i32 0, i32 1 %6 = load ptr, ptr %5, align
                                  8 %7 = getelementptr inbounds %struct.ll, ptr
                                  %6, i32 0, i32 0
                                 */
                                for (const auto gepUse : alloc->users())
                                {
                                    if (!SVFUtil::isa<GetElementPtrInst>(
                                                gepUse))
                                        continue;
                                    for (const auto loadUse2 : gepUse->users())
                                    {
                                        if (SVFUtil::isa<LoadInst>(loadUse2))
                                        {
                                            insertInferSitesOrPushWorklist(
                                                loadUse2);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else if (const auto* gepInst =
                             SVFUtil::dyn_cast<GetElementPtrInst>(it))
                {
                    /*
                     * infer based on gep (pointer operand)
                     %call = call i8* malloc()
                     %1 = bitcast i8* %call to %struct.MyStruct*
                     %next = getelementptr inbounds %struct.MyStruct,
                     %struct.MyStruct* %1, i32 0..
                     */
                    if (gepInst->getPointerOperand() == curValue)
                        insertInferSite(gepInst);
                }
                else if (const auto* bitcast =
                             SVFUtil::dyn_cast<BitCastInst>(it))
                {
                    // continue on bitcast
                    insertInferSitesOrPushWorklist(bitcast);
                }
                else if (const auto* phiNode = SVFUtil::dyn_cast<PHINode>(it))
                {
                    // continue on bitcast
                    insertInferSitesOrPushWorklist(phiNode);
                }
                else if (const auto* retInst =
                             SVFUtil::dyn_cast<ReturnInst>(it))
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
                    for (const auto callsite : retInst->getFunction()->users())
                    {
                        if (const auto* callBase =
                                    SVFUtil::dyn_cast<CallBase>(callsite))
                        {
                            // skip function as parameter
                            // e.g., call void @foo(%struct.ssl_ctx_st* %9, i32 (i8*, i32, i32, i8*)* @passwd_callback)
                            if (callBase->getCalledFunction() !=
                                    retInst->getFunction())
                                continue;
                            insertInferSitesOrPushWorklist(callBase);
                        }
                    }
                }
                else if (const auto* callBase = SVFUtil::dyn_cast<CallBase>(it))
                {
                    /*
                     * propagate from callsite to callee
                      %call = call i8* @malloc(i32 noundef 16)
                      %0 = bitcast i8* %call to %struct.Node*, !dbg !43
                      call void @foo(%struct.Node* noundef %0), !dbg !45

                      define dso_local void @foo(%struct.Node* noundef %param)
                     #0 !dbg !22 {...}
                      ..infer based on the formal param %param..
                     */
                    // skip global function value -> callsite
                    // e.g., def @foo() -> call @foo()
                    // we don't skip function as parameter, e.g., def @foo() -> call @bar(..., @foo)
                    if (SVFUtil::isa<Function>(curValue) &&
                            curValue == callBase->getCalledFunction())
                        continue;
                    // skip indirect call
                    // e.g., %0 = ... -> call %0(...)
                    if (!callBase->hasArgument(curValue))
                        continue;
                    if (Function* calleeFunc = callBase->getCalledFunction())
                    {
                        u32_t pos = getArgPosInCall(callBase, curValue);
                        // for varargs function, we cannot directly get the value-flow between actual and formal args e.g., consider the following vararg function @callee 1: call void @callee(%arg) 2: define dso_local i32 @callee(...) #0 !dbg !17 { 3:  ....... 4:  %5 = load i32, ptr %vaarg.addr, align 4, !dbg !55 5:  .......
                        // 6: }
                        // it is challenging to precisely identify the forward value-flow of %arg (Line 2) because the function definition of callee (Line 2) does not have any formal args related to the actual arg %arg therefore we track all possible instructions like ``load i32, ptr %vaarg.addr''
                        if (calleeFunc->isVarArg())
                        {
                            // conservatively track all var args
                            for (auto& I : instructions(calleeFunc))
                            {
                                if (auto* load =
                                            llvm::dyn_cast<llvm::LoadInst>(&I))
                                {
                                    llvm::Value* loadPointer =
                                        load->getPointerOperand();
                                    if (loadPointer->getName().compare(
                                                "vaarg.addr") == 0)
                                    {
                                        insertInferSitesOrPushWorklist(load);
                                    }
                                }
                            }
                        }
                        else if (!calleeFunc->isDeclaration())
                        {
                            insertInferSitesOrPushWorklist(
                                calleeFunc->getArg(pos));
                        }
                    }
                }
            }
            if (canUpdate)
            {
                Set<const Type*> types;
                std::transform(infersites.begin(), infersites.end(),
                               std::inserter(types, types.begin()),
                               infersiteToType);
                _valueToInferSites[curValue] = SVFUtil::move(infersites);
                _valueToType[curValue] = selectLargestSizedType(types);
            }
        }
        const Type* type = _valueToType[var];
        if (type == nullptr)
        {
            type = defaultType(var);
            WARN_MSG("Using default type, trace ID is " +
                     std::to_string(traceId) + ":" + dumpValueAndDbgInfo(var));
        }
        ABORT_IFNOT(type, "type cannot be a null ptr");
        return type;
    }
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
            for (const auto use: loadInst->getPointerOperand()->users())
            {
                if (const StoreInst *storeInst = SVFUtil::dyn_cast<StoreInst>(use))
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
            for (const auto use: argument->getParent()->users())
            {
                if (const CallBase *callBase = SVFUtil::dyn_cast<CallBase>(use))
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

                    LLVMModuleSet* llvmmodule = LLVMModuleSet::getLLVMModuleSet();
                    const BasicBlock* exitBB = SVFUtil::dyn_cast<BasicBlock>(llvmmodule->getLLVMValue(llvmmodule->getFunExitBB(callee)));
                    assert (exitBB && "exit bb is not a basic block?");
                    const Value *pValue = &exitBB->back();
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
            if (iTyNum >= LLVMUtil::getIntegerValue(pInt).second)
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
 * get or infer the class names of thisptr; starting from :param:`thisPtr`, will walk backwards to find
 * all potential sources for the class name. Valid sources include global or stack variables, heap allocations,
 * or C++ dynamic casts/constructors/destructors.
 * If the source site is a global/stack/heap variable, find the corresponding constructor/destructor to
 * extract the class' name from (since the type of the variable is not reliable but the demangled name is)
 * @param thisPtr
 * @return a set of all possible type names that :param:`thisPtr` could point to
 */
Set<std::string> &ObjTypeInference::inferThisPtrClsName(const Value *thisPtr)
{
    auto it = _thisPtrClassNames.find(thisPtr);
    if (it != _thisPtrClassNames.end()) return it->second;

    Set<std::string> names;

    // Lambda for checking a function is a valid name source & extracting a class name from it
    auto addNamesFromFunc = [&names](const Function *func) -> void
    {
        ABORT_IFNOT(isClsNameSource(func), "Func is invalid class name source: " + dumpValueAndDbgInfo(func));
        for (const auto &name : extractClsNamesFromFunc(func)) names.insert(name);
    };

    // Lambda for getting callee & extracting class name for calls to constructors/destructors/template funcs
    auto addNamesFromCall = [&names, &addNamesFromFunc](const CallBase *call) -> void
    {
        ABORT_IFNOT(isClsNameSource(call), "Call is invalid class name source: " + dumpValueAndDbgInfo(call));

        const auto *func = call->getCalledFunction();
        if (isDynCast(func)) names.insert(extractClsNameFromDynCast(call));
        else addNamesFromFunc(func);
    };

    // Walk backwards to find all valid source sites for the pointer (e.g. stack/global/heap variables)
    for (const auto &val: bwFindAllocOrClsNameSources(thisPtr))
    {
        // A source site is either a constructor/destructor/template function from which the class name can be
        // extracted; a call to a C++ constructor/destructor/template function from which the class name can be
        // extracted; or an allocation site of an object (i.e. a stack/global/heap variable), from which a
        // forward walk can be performed to find calls to C++ constructor/destructor/template functions from
        // which the class' name can then be extracted; skip starting pointer
        if (val == thisPtr) continue;

        if (const auto *func = SVFUtil::dyn_cast<Function>(val))
        {
            // Constructor/destructor/template func; extract name from func directly
            addNamesFromFunc(func);
        }
        else if (isClsNameSource(val))
        {
            // Call to constructor/destructor/template func; get callee; extract name from callee
            ABORT_IFNOT(SVFUtil::isa<CallBase>(val), "Call source site is not a callbase: " + dumpValueAndDbgInfo(val));
            addNamesFromCall(SVFUtil::cast<CallBase>(val));
        }
        else if (isAlloc(val))
        {
            // Stack/global/heap allocation site; walk forward; find constructor/destructor/template calls
            ABORT_IFNOT((SVFUtil::isa<AllocaInst, CallBase, GlobalVariable>(val)),
                        "Alloc site source is not a stack/heap/global variable: " + dumpValueAndDbgInfo(val));
            for (const auto *src : fwFindClsNameSources(val))
            {
                if (const auto *func = SVFUtil::dyn_cast<Function>(src)) addNamesFromFunc(func);
                else if (const auto *call = SVFUtil::dyn_cast<CallBase>(src)) addNamesFromCall(call);
                else ABORT_MSG("Source site from forward walk is invalid: " + dumpValueAndDbgInfo(src));
            }
        }
        else
        {
            ERR_MSG("Unsupported source type found:" + dumpValueAndDbgInfo(val));
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

        // If current value is an instruction inside a constructor/destructor/template, use it as a source
        if (const auto *inst = SVFUtil::dyn_cast<Instruction>(curValue))
        {
            if (const auto *parent = inst->getFunction())
            {
                if (isClsNameSource(parent)) insertSource(parent);
            }
        }

        // If the current value is an object (global, heap, stack, etc) or name source (constructor/destructor,
        // a C++ dynamic cast, or a template function), use it as a source
        if (isAlloc(curValue) || isClsNameSource(curValue))
        {
            insertSource(curValue);
        }

        // Explore the current value further depending on the type of the value; use cached values if possible
        if (const auto *getElementPtrInst = SVFUtil::dyn_cast<GetElementPtrInst>(curValue))
        {
            insertSourcesOrPushWorklist(getElementPtrInst->getPointerOperand());
        }
        else if (const auto *bitCastInst = SVFUtil::dyn_cast<BitCastInst>(curValue))
        {
            insertSourcesOrPushWorklist(bitCastInst->getOperand(0));
        }
        else if (const auto *phiNode = SVFUtil::dyn_cast<PHINode>(curValue))
        {
            for (const auto *op : phiNode->operand_values())
            {
                insertSourcesOrPushWorklist(op);
            }
        }
        else if (const auto *loadInst = SVFUtil::dyn_cast<LoadInst>(curValue))
        {
            for (const auto *user : loadInst->getPointerOperand()->users())
            {
                if (const auto *storeInst = SVFUtil::dyn_cast<StoreInst>(user))
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
            for (const auto *user: argument->getParent()->users())
            {
                if (const auto *callBase = SVFUtil::dyn_cast<CallBase>(user))
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
            if (const auto *callee = callBase->getCalledFunction())
            {
                if (!callee->isDeclaration())
                {
                    LLVMModuleSet* llvmmodule = LLVMModuleSet::getLLVMModuleSet();
                    const BasicBlock* exitBB = SVFUtil::dyn_cast<BasicBlock>(llvmmodule->getLLVMValue(llvmmodule->getFunExitBB(callee)));
                    assert (exitBB && "exit bb is not a basic block?");
                    const Value *pValue = &exitBB->back();
                    const auto *retInst = SVFUtil::dyn_cast<ReturnInst>(pValue);
                    ABORT_IFNOT(retInst && retInst->getReturnValue(), "not return inst?");
                    insertSourcesOrPushWorklist(retInst->getReturnValue());
                }
            }
        }

        // If updating is allowed; store the gathered sources as sources for the current value in the cache
        if (canUpdate)
        {
            _valueToAllocOrClsNameSources[curValue] = sources;
        }
    }

    return _valueToAllocOrClsNameSources[startValue];
}

Set<const CallBase *> &ObjTypeInference::fwFindClsNameSources(const Value *startValue)
{
    assert(startValue && "startValue was null?");

    // consult cache
    auto tIt = _objToClsNameSources.find(startValue);
    if (tIt != _objToClsNameSources.end())
    {
        return tIt->second;
    }

    Set<const CallBase *> sources;

    // Lambda for adding a callee to the sources iff it is a constructor/destructor/template/dyncast
    auto inferViaCppCall = [&sources](const CallBase *caller)
    {
        if (!caller) return;
        if (isClsNameSource(caller)) sources.insert(caller);
    };

    // Find all calls of starting val (or through cast); add as potential source iff applicable
    for (const auto *user : startValue->users())
    {
        if (const auto *caller = SVFUtil::dyn_cast<CallBase>(user))
        {
            inferViaCppCall(caller);
        }
        else if (const auto *bitcast = SVFUtil::dyn_cast<BitCastInst>(user))
        {
            for (const auto *cast_user : bitcast->users())
            {
                if (const auto *caller = SVFUtil::dyn_cast<CallBase>(cast_user))
                {
                    inferViaCppCall(caller);
                }
            }
        }
    }

    // Store sources in cache for starting value & return the found sources
    return _objToClsNameSources[startValue] = SVFUtil::move(sources);
}
