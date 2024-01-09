//===- SymbolTableBuilder.cpp -- Symbol Table builder---------------------//
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
 * SymbolTableBuilder.cpp
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei Sui
 */

#include <memory>

#include "SVF-LLVM/SymbolTableBuilder.h"
#include "Util/NodeIDAllocator.h"
#include "Util/Options.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-LLVM/BasicTypes.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "Util/CppUtil.h"
#include "SVF-LLVM/GEPTypeBridgeIterator.h" // include bridge_gep_iterator

using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

const std::string TYPEMALLOC = "TYPE_MALLOC";

#define ABORT_MSG(reason)                                                      \
    do                                                                         \
    {                                                                          \
        SVFUtil::errs() << __FILE__ << ':' << __LINE__ << ": " << reason       \
                        << '\n';                                               \
        abort();                                                               \
    } while (0)
#define ABORT_IFNOT(condition, reason)                                         \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
            ABORT_MSG(reason);                                                 \
    } while (0)

#define VALUE_WITH_DBGINFO(value)                                              \
    LLVMUtil::dumpValue(value) + LLVMUtil::getSourceLoc(value)

#define TYPE_DEBUG 0 /* Turn this on if you're debugging type inference */

#if TYPE_DEBUG
#define DBLOG(msg)                                                             \
    do                                                                         \
    {                                                                          \
        SVFUtil::outs() << __FILE__ << ':' << __LINE__ << ": "                 \
            << SVFUtil::wrnMsg(msg)  << '\n';                                  \
    } while (0)

#else
#define DBLOG(msg)                                                             \
    do                                                                         \
    {                                                                          \
    } while (0)
#endif

/// Determine type based on infer site
/// https://llvm.org/docs/OpaquePointers.html#migration-instructions
const Type *infersiteToType(const Value *val) {
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

MemObj* SymbolTableBuilder::createBlkObj(SymID symId)
{
    assert(symInfo->isBlkObj(symId));
    assert(symInfo->objMap.find(symId)==symInfo->objMap.end());
    LLVMModuleSet* llvmset = LLVMModuleSet::getLLVMModuleSet();
    MemObj* obj =
        new MemObj(symId, symInfo->createObjTypeInfo(llvmset->getSVFType(
                       IntegerType::get(llvmset->getContext(), 32))));
    symInfo->objMap[symId] = obj;
    return obj;
}

MemObj* SymbolTableBuilder::createConstantObj(SymID symId)
{
    assert(symInfo->isConstantObj(symId));
    assert(symInfo->objMap.find(symId)==symInfo->objMap.end());
    LLVMModuleSet* llvmset = LLVMModuleSet::getLLVMModuleSet();
    MemObj* obj =
        new MemObj(symId, symInfo->createObjTypeInfo(llvmset->getSVFType(
                       IntegerType::get(llvmset->getContext(), 32))));
    symInfo->objMap[symId] = obj;
    return obj;
}


/*!
 *  This method identify which is value sym and which is object sym
 */
void SymbolTableBuilder::buildMemModel(SVFModule* svfModule)
{
    SVFUtil::increaseStackSize();

    symInfo->setModule(svfModule);

    // Pointer #0 always represents the null pointer.
    assert(symInfo->totalSymNum++ == SymbolTableInfo::NullPtr && "Something changed!");

    // Pointer #1 always represents the pointer points-to black hole.
    assert(symInfo->totalSymNum++ == SymbolTableInfo::BlkPtr && "Something changed!");

    // Object #2 is black hole the object that may point to any object
    assert(symInfo->totalSymNum++ == SymbolTableInfo::BlackHole && "Something changed!");
    createBlkObj(SymbolTableInfo::BlackHole);

    // Object #3 always represents the unique constant of a program (merging all constants if Options::ModelConsts is disabled)
    assert(symInfo->totalSymNum++ == SymbolTableInfo::ConstantObj && "Something changed!");
    createConstantObj(SymbolTableInfo::ConstantObj);

    for (Module &M : LLVMModuleSet::getLLVMModuleSet()->getLLVMModules())
    {
        // Add symbols for all the globals .
        for (const GlobalVariable& gv : M.globals())
        {
            collectSym(&gv);
        }

        // Add symbols for all the global aliases
        for (const GlobalAlias& ga : M.aliases())
        {
            collectSym(&ga);
            collectSym(ga.getAliasee());
        }

        // Add symbols for all of the functions and the instructions in them.
        for (const Function& fun : M.functions())
        {
            collectSym(&fun);
            collectRet(&fun);
            if (fun.getFunctionType()->isVarArg())
                collectVararg(&fun);

            // Add symbols for all formal parameters.
            for (const Argument& arg : fun.args())
            {
                collectSym(&arg);
            }

            // collect and create symbols inside the function body
            for (const Instruction& inst : instructions(fun))
            {
                collectSym(&inst);

                // initialization for some special instructions
                //{@
                if (const StoreInst* st = SVFUtil::dyn_cast<StoreInst>(&inst))
                {
                    collectSym(st->getPointerOperand());
                    collectSym(st->getValueOperand());
                }
                else if (const LoadInst* ld =
                             SVFUtil::dyn_cast<LoadInst>(&inst))
                {
                    collectSym(ld->getPointerOperand());
                }
                else if (const AllocaInst* alloc =
                             SVFUtil::dyn_cast<AllocaInst>(&inst))
                {
                    collectSym(alloc->getArraySize());
                }
                else if (const PHINode* phi = SVFUtil::dyn_cast<PHINode>(&inst))
                {
                    for (u32_t i = 0; i < phi->getNumIncomingValues(); ++i)
                    {
                        collectSym(phi->getIncomingValue(i));
                    }
                }
                else if (const GetElementPtrInst* gep =
                             SVFUtil::dyn_cast<GetElementPtrInst>(&inst))
                {
                    collectSym(gep->getPointerOperand());
                    for (u32_t i = 0; i < gep->getNumOperands(); ++i)
                    {
                        collectSym(gep->getOperand(i));
                    }
                }
                else if (const SelectInst* sel =
                             SVFUtil::dyn_cast<SelectInst>(&inst))
                {
                    collectSym(sel->getTrueValue());
                    collectSym(sel->getFalseValue());
                    collectSym(sel->getCondition());
                }
                else if (const BinaryOperator* binary =
                             SVFUtil::dyn_cast<BinaryOperator>(&inst))
                {
                    for (u32_t i = 0; i < binary->getNumOperands(); i++)
                        collectSym(binary->getOperand(i));
                }
                else if (const UnaryOperator* unary =
                             SVFUtil::dyn_cast<UnaryOperator>(&inst))
                {
                    for (u32_t i = 0; i < unary->getNumOperands(); i++)
                        collectSym(unary->getOperand(i));
                }
                else if (const CmpInst* cmp = SVFUtil::dyn_cast<CmpInst>(&inst))
                {
                    for (u32_t i = 0; i < cmp->getNumOperands(); i++)
                        collectSym(cmp->getOperand(i));
                }
                else if (const CastInst* cast =
                             SVFUtil::dyn_cast<CastInst>(&inst))
                {
                    collectSym(cast->getOperand(0));
                }
                else if (const ReturnInst* ret =
                             SVFUtil::dyn_cast<ReturnInst>(&inst))
                {
                    if (ret->getReturnValue())
                        collectSym(ret->getReturnValue());
                }
                else if (const BranchInst* br =
                             SVFUtil::dyn_cast<BranchInst>(&inst))
                {
                    Value* opnd = br->isConditional() ? br->getCondition() : br->getOperand(0);
                    collectSym(opnd);
                }
                else if (const SwitchInst* sw =
                             SVFUtil::dyn_cast<SwitchInst>(&inst))
                {
                    collectSym(sw->getCondition());
                }
                else if (isNonInstricCallSite(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(&inst)))
                {

                    const CallBase* cs = LLVMUtil::getLLVMCallSite(&inst);
                    for (u32_t i = 0; i < cs->arg_size(); i++)
                    {
                        collectSym(cs->getArgOperand(i));
                    }
                    // Calls to inline asm need to be added as well because the
                    // callee isn't referenced anywhere else.
                    const Value* Callee = cs->getCalledOperand();
                    collectSym(Callee);

                    // TODO handle inlineAsm
                    /// if (SVFUtil::isa<InlineAsm>(Callee))
                    if (Options::EnableTypeCheck()) {
                        validateTypeCheck(cs);
                    }
                }
                //@}
            }
        }
    }

    symInfo->totalSymNum = NodeIDAllocator::get()->endSymbolAllocation();
    if (Options::SymTabPrint())
    {
        symInfo->dump();
    }
}

void SymbolTableBuilder::collectSVFTypeInfo(const Value* val)
{
    Type *valType = val->getType();
    (void)getOrAddSVFTypeInfo(valType);
    if(isGepConstantExpr(val) || SVFUtil::isa<GetElementPtrInst>(val))
    {
        for (bridge_gep_iterator
                gi = bridge_gep_begin(SVFUtil::cast<User>(val)),
                ge = bridge_gep_end(SVFUtil::cast<User>(val));
                gi != ge; ++gi)
        {
            const Type* gepTy = *gi;
            (void)getOrAddSVFTypeInfo(gepTy);
        }
    }
}

/*!
 * Collect symbols, including value and object syms
 */
void SymbolTableBuilder::collectSym(const Value* val)
{

    //TODO: filter the non-pointer type // if (!SVFUtil::isa<PointerType>(val->getType()))  return;

    DBOUT(DMemModel,
          outs()
          << "collect sym from ##"
          << LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val)->toString()
          << " \n");
    //TODO handle constant expression value here??
    handleCE(val);

    // create a value sym
    collectVal(val);

    collectSVFTypeInfo(val);
    collectSVFTypeInfo(LLVMUtil::getGlobalRep(val));

    // create an object If it is a heap, stack, global, function.
    if (isObject(val))
    {
        collectObj(val);
    }
}

/*!
 * Get value sym, if not available create a new one
 */
void SymbolTableBuilder::collectVal(const Value* val)
{
    // collect and record special sym here
    if (LLVMUtil::isNullPtrSym(val) || LLVMUtil::isBlackholeSym(val))
    {
        return;
    }
    SymbolTableInfo::ValueToIDMapTy::iterator iter = symInfo->valSymMap.find(
                LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val));
    if (iter == symInfo->valSymMap.end())
    {
        // create val sym and sym type
        SVFValue* svfVal = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val);
        SymID id = NodeIDAllocator::get()->allocateValueId();
        symInfo->valSymMap.insert(std::make_pair(svfVal, id));
        DBOUT(DMemModel,
              outs() << "create a new value sym " << id << "\n");
        ///  handle global constant expression here
        if (const GlobalVariable* globalVar = SVFUtil::dyn_cast<GlobalVariable>(val))
            handleGlobalCE(globalVar);
    }

    if (isConstantObjSym(val))
        collectObj(val);
}

/*!
 * Get memory object sym, if not available create a new one
 */
void SymbolTableBuilder::collectObj(const Value* val)
{
    val = LLVMUtil::getGlobalRep(val);
    SymbolTableInfo::ValueToIDMapTy::iterator iter = symInfo->objSymMap.find(
                LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val));
    if (iter == symInfo->objSymMap.end())
    {
        SVFValue* svfVal = LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val);
        // if the object pointed by the pointer is a constant data (e.g., i32 0) or a global constant object (e.g. string)
        // then we treat them as one ConstantObj
        if (isConstantObjSym(val) && !symInfo->getModelConstants())
        {
            symInfo->objSymMap.insert(std::make_pair(svfVal, symInfo->constantSymID()));
        }
        // otherwise, we will create an object for each abstract memory location
        else
        {
            // create obj sym and sym type
            SymID id = NodeIDAllocator::get()->allocateObjectId();
            symInfo->objSymMap.insert(std::make_pair(svfVal, id));
            DBOUT(DMemModel,
                  outs() << "create a new obj sym " << id << "\n");

            // create a memory object
            MemObj* mem =
                new MemObj(id, createObjTypeInfo(val),
                           LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val));
            assert(symInfo->objMap.find(id) == symInfo->objMap.end());
            symInfo->objMap[id] = mem;
        }
    }
}

/*!
 * Create unique return sym, if not available create a new one
 */
void SymbolTableBuilder::collectRet(const Function* val)
{
    const SVFFunction* svffun =
        LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(val);
    SymbolTableInfo::FunToIDMapTy::iterator iter =
        symInfo->returnSymMap.find(svffun);
    if (iter == symInfo->returnSymMap.end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        symInfo->returnSymMap.insert(std::make_pair(svffun, id));
        DBOUT(DMemModel, outs() << "create a return sym " << id << "\n");
    }
}

/*!
 * Create vararg sym, if not available create a new one
 */
void SymbolTableBuilder::collectVararg(const Function* val)
{
    const SVFFunction* svffun =
        LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(val);
    SymbolTableInfo::FunToIDMapTy::iterator iter =
        symInfo->varargSymMap.find(svffun);
    if (iter == symInfo->varargSymMap.end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        symInfo->varargSymMap.insert(std::make_pair(svffun, id));
        DBOUT(DMemModel, outs() << "create a vararg sym " << id << "\n");
    }
}

/*!
 * Handle constant expression
 */
void SymbolTableBuilder::handleCE(const Value* val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* ce = isGepConstantExpr(ref))
        {
            DBOUT(DMemModelCE, outs() << "handle constant expression "
                  << LLVMModuleSet::getLLVMModuleSet()
                  ->getSVFValue(ref)
                  ->toString()
                  << "\n");
            collectVal(ce);

            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            for (u32_t i = 0; i < ce->getNumOperands(); ++i)
            {
                collectVal(ce->getOperand(i));
                handleCE(ce->getOperand(i));
            }
        }
        else if (const ConstantExpr* ce = isCastConstantExpr(ref))
        {
            DBOUT(DMemModelCE, outs() << "handle constant expression "
                  << LLVMModuleSet::getLLVMModuleSet()
                  ->getSVFValue(ref)
                  ->toString()
                  << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        }
        else if (const ConstantExpr* ce = isSelectConstantExpr(ref))
        {
            DBOUT(DMemModelCE, outs() << "handle constant expression "
                  << LLVMModuleSet::getLLVMModuleSet()
                  ->getSVFValue(ref)
                  ->toString()
                  << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            collectVal(ce->getOperand(1));
            collectVal(ce->getOperand(2));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
            handleCE(ce->getOperand(1));
            handleCE(ce->getOperand(2));
        }
        // if we meet a int2ptr, then it points-to black hole
        else if (const ConstantExpr* int2Ptrce = isInt2PtrConstantExpr(ref))
        {
            collectVal(int2Ptrce);
        }
        else if (const ConstantExpr* ptr2Intce = isPtr2IntConstantExpr(ref))
        {
            collectVal(ptr2Intce);
            const Constant* opnd = ptr2Intce->getOperand(0);
            handleCE(opnd);
        }
        else if (isTruncConstantExpr(ref) || isCmpConstantExpr(ref))
        {
            collectVal(ref);
        }
        else if (isBinaryConstantExpr(ref))
        {
            collectVal(ref);
        }
        else if (isUnaryConstantExpr(ref))
        {
            // we don't handle unary constant expression like fneg(x) now
            collectVal(ref);
        }
        else if (SVFUtil::isa<ConstantAggregate>(ref))
        {
            // we don't handle constant aggregate like constant vectors
            collectVal(ref);
        }
        else
        {
            assert(!SVFUtil::isa<ConstantExpr>(val) &&
                   "we don't handle all other constant expression for now!");
            collectVal(ref);
        }
    }
}

/*!
 * Handle global constant expression
 */
void SymbolTableBuilder::handleGlobalCE(const GlobalVariable* G)
{
    assert(G);

    //The type this global points to
    const Type* T = G->getValueType();
    bool is_array = 0;
    //An array is considered a single variable of its type.
    while (const ArrayType* AT = SVFUtil::dyn_cast<ArrayType>(T))
    {
        T = AT->getElementType();
        is_array = true;
    }

    if (SVFUtil::isa<StructType>(T))
    {
        //A struct may be used in constant GEP expr.
        for (const User* user : G->users())
        {
            handleCE(user);
        }
    }
    else if (is_array)
    {
        for (const User* user : G->users())
        {
            handleCE(user);
        }
    }

    if (G->hasInitializer())
    {
        handleGlobalInitializerCE(G->getInitializer());
    }
}

/*!
 * Handle global variable initialization
 */
void SymbolTableBuilder::handleGlobalInitializerCE(const Constant* C)
{

    if (C->getType()->isSingleValueType())
    {
        if (const ConstantExpr* E = SVFUtil::dyn_cast<ConstantExpr>(C))
        {
            handleCE(E);
        }
        else
        {
            collectVal(C);
        }
    }
    else if (SVFUtil::isa<ConstantArray>(C))
    {
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            handleGlobalInitializerCE(SVFUtil::cast<Constant>(C->getOperand(i)));
        }
    }
    else if (SVFUtil::isa<ConstantStruct>(C))
    {
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            handleGlobalInitializerCE(SVFUtil::cast<Constant>(C->getOperand(i)));
        }
    }
    else if(const ConstantData* data = SVFUtil::dyn_cast<ConstantData>(C))
    {
        if (Options::ModelConsts())
        {
            if (const ConstantDataSequential* seq =
                        SVFUtil::dyn_cast<ConstantDataSequential>(data))
            {
                for(u32_t i = 0; i < seq->getNumElements(); i++)
                {
                    const Constant* ct = seq->getElementAsConstant(i);
                    handleGlobalInitializerCE(ct);
                }
            }
            else
            {
                assert(
                    (SVFUtil::isa<ConstantAggregateZero, UndefValue>(data)) &&
                    "Single value type data should have been handled!");
            }
        }
    }
    else
    {
        //TODO:assert(SVFUtil::isa<ConstantVector>(C),"what else do we have");
    }
}

/*!
 * Forward collect all possible infer sites starting from a value
 * @param startValue
 */
void SymbolTableBuilder::forwardCollectAllInfersites(const Value* startValue) {
    // simulate the call stack, the second element indicates whether we should update valueTypes for current value
    typedef std::pair<const Value*, bool> ValueBoolPair;
    FILOWorkList<ValueBoolPair> workList;
    Set<ValueBoolPair> visited;
    workList.push({startValue, false});

    while (!workList.empty()) {
        auto curPair = workList.pop();
        if(visited.count(curPair)) continue;
        visited.insert(curPair);
        const Value *curValue = curPair.first;
        bool canUpdate = curPair.second;
        Set<const Value*> infersites;

        auto insertInferSite = [&infersites, &canUpdate] (const Value* infersite) {
            if(canUpdate) infersites.insert(infersite);
        };
        auto insertInferSitesOrPushWorklist = [this, &infersites, &workList, &canUpdate](const auto& pUser) {
            auto vIt = valueToInferSites.find(pUser);
            if(canUpdate) {
                if (vIt != valueToInferSites.end()) {
                    infersites.insert(vIt->second.begin(), vIt->second.end());
                }
            } else {
                if(vIt == valueToInferSites.end()) workList.push({pUser, false});
            }
        };
        if (!canUpdate && !valueToInferSites.count(curValue)) {
            workList.push({curValue, true});
        }
        for (const auto &it : curValue->uses())
        {
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
            valueToInferSites[curValue] = SVFUtil::move(infersites);
        }
    }
}



/*!
 * Return the type of the object from a heap allocation
 */
const Type* SymbolTableBuilder::inferTypeOfHeapObjOrStaticObj(const Instruction *inst)
{
    const Value* startValue = inst;
    const PointerType *originalPType = SVFUtil::dyn_cast<PointerType>(inst->getType());
    assert(originalPType && "empty type?");
    const SVFInstruction* svfinst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(inst);
    if(SVFUtil::isHeapAllocExtCallViaRet(svfinst))
    {
        if(const Value* v = getFirstUseViaCastInst(inst))
        {
            if (const PointerType *newTy = SVFUtil::dyn_cast<PointerType>(v->getType())) {
                originalPType = newTy;
            }
        }
        forwardCollectAllInfersites(startValue);
    }
    else if(SVFUtil::isHeapAllocExtCallViaArg(svfinst))
    {
        const CallBase* cs = LLVMUtil::getLLVMCallSite(inst);
        int arg_pos = SVFUtil::getHeapAllocHoldingArgPosition(SVFUtil::getSVFCallSite(svfinst));
        const Value* arg = cs->getArgOperand(arg_pos);
        originalPType = SVFUtil::dyn_cast<PointerType>(arg->getType());
        forwardCollectAllInfersites(startValue = arg);
    }
    else
    {
        assert( false && "not a heap allocation instruction?");
    }

    const Type* inferedType = nullptr;
    auto vIt = valueToInferSites.find(startValue);
    if (vIt != valueToInferSites.end() && !vIt->second.empty()) {
        std::vector<const Type*> types(vIt->second.size());
        std::transform(vIt->second.begin(), vIt->second.end(), types.begin(), infersiteToType);
        inferedType = selectLargestType(types);
    } else {
        // return an 8-bit integer type if the inferred type is empty
        inferedType = Type::getInt8Ty(LLVMModuleSet::getLLVMModuleSet()->getContext());
        DBLOG("empty types, value ID is " + std::to_string(inst->getValueID()) + ":" + VALUE_WITH_DBGINFO(inst));
    }

#if TYPE_DEBUG
    ABORT_IFNOT(getNumOfElements(getPtrElementType(originalPType)) <= getNumOfElements(inferedType),
                "wrong type, value ID is " + std::to_string(inst->getValueID()) + ":" + VALUE_WITH_DBGINFO(inst));
#endif
    return inferedType;
}



/*!
 * Validate type inference
 * @param cs : stub malloc function with element number label
 */
void SymbolTableBuilder::validateTypeCheck(const CallBase *cs) {
    if (const Function* func = cs->getCalledFunction())
    {
        if (func->getName().find(TYPEMALLOC) != std::string::npos)
        {
            forwardCollectAllInfersites(cs);
            auto vTyIt = valueToInferSites.find(cs);
            if (vTyIt == valueToInferSites.end()) {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << "empty types, value ID is "
                                << std::to_string(cs->getValueID()) << ":" << VALUE_WITH_DBGINFO(cs) << "\n";
                abort();
            }
            std::vector<const Type*> types(vTyIt->second.size());
            std::transform(vTyIt->second.begin(), vTyIt->second.end(), types.begin(), infersiteToType);
            const Type *pType = selectLargestType(types);
            ConstantInt* pInt =
                    SVFUtil::dyn_cast<llvm::ConstantInt>(cs->getOperand(1));
            assert(pInt && "the second argument is a integer");
            if (getNumOfElements(pType) >= pInt->getZExtValue())
                SVFUtil::outs() << SVFUtil::sucMsg("\t SUCCESS :") << VALUE_WITH_DBGINFO(cs) << SVFUtil::pasMsg(" TYPE: ")
                                << dumpType(pType) << "\n";
            else
            {
                SVFUtil::errs() << SVFUtil::errMsg("\t FAILURE :") << ", value ID is "
                                << std::to_string(cs->getValueID()) << ":" << VALUE_WITH_DBGINFO(cs) << " TYPE: "
                                << dumpType(pType) << "\n";
                abort();
            }
        }
    }
}

/*
 * Initial the memory object here
 */
ObjTypeInfo* SymbolTableBuilder::createObjTypeInfo(const Value* val)
{
    const Type* objTy = nullptr;

    const Instruction* I = SVFUtil::dyn_cast<Instruction>(val);

    // We consider two types of objects:
    // (1) A heap/static object from a callsite
    if (I && isNonInstricCallSite(LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(I)))
    {
        objTy = inferTypeOfHeapObjOrStaticObj(I);
    }
    // (2) Other objects (e.g., alloca, global, etc.)
    else
    {
        if (SVFUtil::isa<PointerType>(val->getType()))
        {
            if (const AllocaInst *allocaInst = SVFUtil::dyn_cast<AllocaInst>(val))
            {
                // get the type of the allocated memory
                // e.g., for `%retval = alloca i64, align 4`, we return i64
                objTy = allocaInst->getAllocatedType();
            }
            else if (const GlobalValue *global = SVFUtil::dyn_cast<GlobalValue>(val))
            {
                // get the pointee type of the global pointer (begins with @ symbol in llvm)
                objTy = global->getValueType();
            }
            else
            {
                SVFUtil::errs() << VALUE_WITH_DBGINFO(val) << "\n";
                assert(false && "not an allocation or global?");
            }
        }
    }

    if (objTy)
    {
        (void) getOrAddSVFTypeInfo(objTy);
        ObjTypeInfo* typeInfo = new ObjTypeInfo(
            LLVMModuleSet::getLLVMModuleSet()->getSVFType(objTy),
            Options::MaxFieldLimit());
        initTypeInfo(typeInfo,val, objTy);
        return typeInfo;
    }
    else
    {
        writeWrnMsg("try to create an object with a non-pointer type.");
        writeWrnMsg(val->getName().str());
        writeWrnMsg("(" + LLVMModuleSet::getLLVMModuleSet()->getSVFValue(val)->getSourceLoc() + ")");
        if (isConstantObjSym(val))
        {
            ObjTypeInfo* typeInfo = new ObjTypeInfo(
                LLVMModuleSet::getLLVMModuleSet()->getSVFType(val->getType()),
                0);
            initTypeInfo(typeInfo,val, val->getType());
            return typeInfo;
        }
        else
        {
            assert(false && "Memory object must be either (1) held by a pointer-typed ref value or (2) a constant value (e.g., 10).");
            abort();
        }
    }
}

/*!
 * Analyse types of all flattened fields of this object
 */
void SymbolTableBuilder::analyzeObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    const Type *elemTy = LLVMModuleSet::getLLVMModuleSet()->getLLVMType(typeinfo->getType());
    // Find the inter nested array element
    while (const ArrayType* AT = SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        elemTy = AT->getElementType();
        if (SVFUtil::isa<GlobalVariable>(val) &&
                SVFUtil::cast<GlobalVariable>(val)->hasInitializer() &&
                SVFUtil::isa<ConstantArray>(
                    SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            typeinfo->setFlag(ObjTypeInfo::CONST_ARRAY_OBJ);
        else
            typeinfo->setFlag(ObjTypeInfo::VAR_ARRAY_OBJ);
    }
    if (SVFUtil::isa<StructType>(elemTy))
    {
        if (SVFUtil::isa<GlobalVariable>(val) &&
                SVFUtil::cast<GlobalVariable>(val)->hasInitializer() &&
                SVFUtil::isa<ConstantStruct>(
                    SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            typeinfo->setFlag(ObjTypeInfo::CONST_STRUCT_OBJ);
        else
            typeinfo->setFlag(ObjTypeInfo::VAR_STRUCT_OBJ);
    }
}

/*!
 * Analyze byte size of heap alloc function (e.g. malloc/calloc/...)
 * 1) __attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0")))
     void* safe_malloc(unsigned long size).
     Byte Size is the size(Arg0)
   2)__attribute__((annotate("ALLOC_RET"), annotate("AllocSize:Arg0*Arg1")))
    char* safecalloc(int a, int b)
    Byte Size is a(Arg0) * b(Arg1)
   3)__attribute__((annotate("ALLOC_RET"), annotate("UNKNOWN")))
    void* __sysv_signal(int a, void *b)
    Byte Size is Unknown
    If all required arg values are constant, byte Size is also constant,
    otherwise return ByteSize 0
 */
u32_t SymbolTableBuilder::analyzeHeapAllocByteSize(const Value* val)
{
    if(const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(val))
    {
        if (const llvm::Function* calledFunction =
                    callInst->getCalledFunction())
        {
            const SVFFunction* svfFunction =
                LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(
                    calledFunction);
            std::vector<const Value*> args;
            // Heap alloc functions have annoation like "AllocSize:Arg1"
            for (std::string annotation : svfFunction->getAnnotations())
            {
                if (annotation.find("AllocSize:") != std::string::npos)
                {
                    std::string allocSize = annotation.substr(10);
                    std::stringstream ss(allocSize);
                    std::string token;
                    // Analyaze annotation string and attract Arg list
                    while (std::getline(ss, token, '*'))
                    {
                        if (token.rfind("Arg", 0) == 0)
                        {
                            u32_t argIndex;
                            std::istringstream(token.substr(3)) >> argIndex;
                            if (argIndex < callInst->getNumOperands() - 1)
                            {
                                args.push_back(
                                    callInst->getArgOperand(argIndex));
                            }
                        }
                    }
                }
            }
            u64_t product = 1;
            if (args.size() > 0)
            {
                // for annotations like "AllocSize:Arg0*Arg1"
                for (const llvm::Value* arg : args)
                {
                    if (const llvm::ConstantInt* constIntArg =
                                llvm::dyn_cast<llvm::ConstantInt>(arg))
                    {
                        // Multiply the constant Value if all Args are const
                        product *= constIntArg->getZExtValue();
                    }
                    else
                    {
                        // if Arg list has non-const value, return 0 to indicate it is non const byte size
                        return 0;
                    }
                }
                // If all the Args are const, return product
                return product;
            }
            else
            {
                // for annotations like "AllocSize:UNKNOWN"
                return 0;
            }
        }
    }
    // if it is not CallInst or CallInst has no CalledFunction, return 0 to indicate it is non const byte size
    return 0;
}

/*!
 * Analyse types of heap and static objects
 */
u32_t SymbolTableBuilder::analyzeHeapObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    if(const Value* castUse = getFirstUseViaCastInst(val))
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
        analyzeObjType(typeinfo,castUse);
        const Type* objTy = LLVMModuleSet::getLLVMModuleSet()->getLLVMType(typeinfo->getType());
        if(SVFUtil::isa<ArrayType>(objTy))
            return getNumOfElements(objTy);
        else if(const StructType* st = SVFUtil::dyn_cast<StructType>(objTy))
        {
            /// For an C++ class, it can have variant elements depending on the vtable size,
            /// Hence we only handle non-cpp-class object, the type of the cpp class is treated as PointerType at the cast site
            if(classTyHasVTable(st))
                typeinfo->resetTypeForHeapStaticObj(LLVMModuleSet::getLLVMModuleSet()->getSVFType(castUse->getType()));
            else
                return getNumOfElements(objTy);
        }
    }
    else
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
    }
    return typeinfo->getMaxFieldOffsetLimit();
}

/*!
 * Analyse types of heap and static objects
 */
void SymbolTableBuilder::analyzeStaticObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    if(const Value* castUse = getFirstUseViaCastInst(val))
    {
        typeinfo->setFlag(ObjTypeInfo::STATIC_OBJ);
        analyzeObjType(typeinfo,castUse);
    }
    else
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
    }
}

/*!
 * Initialize the type info of an object
 */
void SymbolTableBuilder::initTypeInfo(ObjTypeInfo* typeinfo, const Value* val,
                                      const Type* objTy)
{

    u32_t elemNum = 1;
    // init byteSize = 0, If byteSize is changed in the following process,
    // it means that ObjTypeInfo has a Constant Byte Size
    u32_t byteSize = 0;
    // Global variable
    // if val is Function Obj, byteSize is not set
    if (SVFUtil::isa<Function>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::FUNCTION_OBJ);
        analyzeObjType(typeinfo,val);
        elemNum = getNumOfElements(objTy);
    }
    /// if val is AllocaInst, byteSize is Type's LLVM ByteSize * ArraySize
    /// e.g. alloc i32, 10. byteSize is 4 (i32's size) * 10 (ArraySize) = 40
    else if(const AllocaInst* allocaInst = SVFUtil::dyn_cast<AllocaInst>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::STACK_OBJ);
        analyzeObjType(typeinfo,val);
        /// This is for `alloca <ty> <NumElements>`. For example, `alloca i64 3` allocates 3 i64 on the stack (objSize=3)
        /// In most cases, `NumElements` is not specified in the instruction, which means there is only one element (objSize=1).
        if(const ConstantInt* sz = SVFUtil::dyn_cast<ConstantInt>(allocaInst->getArraySize()))
        {
            elemNum = sz->getZExtValue() * getNumOfElements(objTy);
            byteSize = sz->getZExtValue() * typeinfo->getType()->getByteSize();
        }
        /// if ArraySize is not constant, byteSize is not static determined.
        else
        {
            elemNum = getNumOfElements(objTy);
            byteSize = 0;
        }
    }
    /// if val is GlobalVar, byteSize is Type's LLVM ByteSize
    /// All GlobalVariable must have constant size
    else if(SVFUtil::isa<GlobalVariable>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::GLOBVAR_OBJ);
        if(isConstantObjSym(val))
            typeinfo->setFlag(ObjTypeInfo::CONST_GLOBAL_OBJ);
        analyzeObjType(typeinfo,val);
        elemNum = getNumOfElements(objTy);
        byteSize = typeinfo->getType()->getByteSize();
    }
    /// if val is heap alloc
    else if (SVFUtil::isa<Instruction>(val) &&
             isHeapAllocExtCall(
                 LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(
                     SVFUtil::cast<Instruction>(val))))
    {
        elemNum = analyzeHeapObjType(typeinfo,val);
        // analyze heap alloc like (malloc/calloc/...), the alloc functions have
        // annotation like "AllocSize:Arg1". Please refer to extapi.c.
        // e.g. calloc(4, 10), annotation is "AllocSize:Arg0*Arg1",
        // it means byteSize = 4 (Arg0) * 10 (Arg1) = 40
        byteSize = analyzeHeapAllocByteSize(val);
    }
    else if(ArgInProgEntryFunction(val))
    {
        analyzeStaticObjType(typeinfo,val);
        // user input data, label its field as infinite here
        elemNum = typeinfo->getMaxFieldOffsetLimit();
        byteSize = typeinfo->getType()->getByteSize();
    }
    else if(LLVMUtil::isConstDataOrAggData(val))
    {
        typeinfo->setFlag(ObjTypeInfo::CONST_DATA);
        elemNum = getNumOfFlattenElements(val->getType());
        byteSize = typeinfo->getType()->getByteSize();
    }
    else
    {
        assert("what other object do we have??");
        abort();
    }

    // Reset maxOffsetLimit if it is over the total fieldNum of this object
    if(typeinfo->getMaxFieldOffsetLimit() > elemNum)
        typeinfo->setNumOfElements(elemNum);

    // set ByteSize. If ByteSize > 0, this typeinfo has constant type.
    // If ByteSize == 0, this typeinfo has 1) zero byte 2) non-const byte size
    // If ByteSize>MaxFieldLimit, set MaxFieldLimit to the byteSize;
    byteSize = Options::MaxFieldLimit() > byteSize? byteSize: Options::MaxFieldLimit();
    typeinfo->setByteSizeOfObj(byteSize);
}

/*!
 * Return size of this Object
 */
u32_t SymbolTableBuilder::getNumOfElements(const Type* ety)
{
    assert(ety && "type is null?");
    u32_t numOfFields = 1;
    if (SVFUtil::isa<StructType, ArrayType>(ety))
    {
        numOfFields = getNumOfFlattenElements(ety);
    }
    return numOfFields;
}

/// Number of flattened elements of an array or struct
u32_t SymbolTableBuilder::getNumOfFlattenElements(const Type* T)
{
    if(Options::ModelArrays())
        return getOrAddSVFTypeInfo(T)->getNumOfFlattenElements();
    else
        return getOrAddSVFTypeInfo(T)->getNumOfFlattenFields();
}

StInfo* SymbolTableBuilder::getOrAddSVFTypeInfo(const Type* T)
{
    return LLVMModuleSet::getLLVMModuleSet()->getSVFType(T)->getTypeInfo();
}
