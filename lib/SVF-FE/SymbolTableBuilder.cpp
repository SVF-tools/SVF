//===- SymbolTableBuilder.cpp -- Symbol Table builder---------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
//===----------------------------------------------------------------------===//

/*
 * SymbolTableBuilder.cpp
 *
 *  Created on: Apr 28, 2014
 *      Author: Yulei Sui
 */

#include <memory>

#include "SVF-FE/SymbolTableBuilder.h"
#include "Util/NodeIDAllocator.h"
#include "Util/Options.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/BasicTypes.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/CPPUtil.h"
#include "SVF-FE/GEPTypeBridgeIterator.h" // include bridge_gep_iterator

using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

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
    symInfo->createBlkObj(SymbolTableInfo::BlackHole);

    // Object #3 always represents the unique constant of a program (merging all constants if Options::ModelConsts is disabled)
    assert(symInfo->totalSymNum++ == SymbolTableInfo::ConstantObj && "Something changed!");
    symInfo->createConstantObj(SymbolTableInfo::ConstantObj);

    // Add symbols for all the globals .
    for (SVFModule::global_iterator I = svfModule->global_begin(), E =
                svfModule->global_end(); I != E; ++I)
    {
        collectSym(*I);
    }

    // Add symbols for all the global aliases
    for (SVFModule::alias_iterator I = svfModule->alias_begin(), E =
                svfModule->alias_end(); I != E; I++)
    {
        collectSym(*I);
        collectSym((*I)->getAliasee());
    }

    // Add symbols for all of the functions and the instructions in them.
    for (SVFModule::llvm_iterator F = svfModule->llvmFunBegin(), E = svfModule->llvmFunEnd(); F != E; ++F)
    {
        Function *fun = *F;
        collectSym(fun);
        collectRet(fun);
        if (fun->getFunctionType()->isVarArg())
            collectVararg(fun);

        // Add symbols for all formal parameters.
        for (Function::arg_iterator I = fun->arg_begin(), E = fun->arg_end();
                I != E; ++I)
        {
            collectSym(&*I);
        }

        // collect and create symbols inside the function body
        for (inst_iterator II = inst_begin(*fun), E = inst_end(*fun); II != E; ++II)
        {
            const Instruction *inst = &*II;
            collectSym(inst);

            // initialization for some special instructions
            //{@
            if (const StoreInst *st = SVFUtil::dyn_cast<StoreInst>(inst))
            {
                collectSym(st->getPointerOperand());
                collectSym(st->getValueOperand());
            }
            else if (const LoadInst *ld = SVFUtil::dyn_cast<LoadInst>(inst))
            {
                collectSym(ld->getPointerOperand());
            }
            else if (const AllocaInst *alloc = SVFUtil::dyn_cast<AllocaInst>(inst))
            {
                collectSym(alloc->getArraySize());
            }
            else if (const PHINode *phi = SVFUtil::dyn_cast<PHINode>(inst))
            {
                for (u32_t i = 0; i < phi->getNumIncomingValues(); ++i)
                {
                    collectSym(phi->getIncomingValue(i));
                }
            }
            else if (const GetElementPtrInst *gep = SVFUtil::dyn_cast<GetElementPtrInst>(
                    inst))
            {
                collectSym(gep->getPointerOperand());
            }
            else if (const SelectInst *sel = SVFUtil::dyn_cast<SelectInst>(inst))
            {
                collectSym(sel->getTrueValue());
                collectSym(sel->getFalseValue());
                collectSym(sel->getCondition());
            }
            else if (const BinaryOperator *binary = SVFUtil::dyn_cast<BinaryOperator>(inst))
            {
                for (u32_t i = 0; i < binary->getNumOperands(); i++)
                    collectSym(binary->getOperand(i));
            }
            else if (const UnaryOperator *unary = SVFUtil::dyn_cast<UnaryOperator>(inst))
            {
                for (u32_t i = 0; i < unary->getNumOperands(); i++)
                    collectSym(unary->getOperand(i));
            }
            else if (const CmpInst *cmp = SVFUtil::dyn_cast<CmpInst>(inst))
            {
                for (u32_t i = 0; i < cmp->getNumOperands(); i++)
                    collectSym(cmp->getOperand(i));
            }
            else if (const CastInst *cast = SVFUtil::dyn_cast<CastInst>(inst))
            {
                collectSym(cast->getOperand(0));
            }
            else if (const ReturnInst *ret = SVFUtil::dyn_cast<ReturnInst>(inst))
            {
                if(ret->getReturnValue())
                    collectSym(ret->getReturnValue());
            }
            else if (const BranchInst *br = SVFUtil::dyn_cast<BranchInst>(inst))
            {
                Value* opnd = br->isConditional() ? br->getCondition() : br->getOperand(0);
                collectSym(opnd);
            }
            else if (const SwitchInst *sw = SVFUtil::dyn_cast<SwitchInst>(inst))
            {
                collectSym(sw->getCondition());
            }
            else if (isNonInstricCallSite(inst))
            {

                CallSite cs = SVFUtil::getLLVMCallSite(inst);
                symInfo->callSiteSet.insert(cs);
                for (CallSite::arg_iterator it = cs.arg_begin();
                        it != cs.arg_end(); ++it)
                {
                    collectSym(*it);
                }
                // Calls to inline asm need to be added as well because the callee isn't
                // referenced anywhere else.
                const Value *Callee = cs.getCalledValue();
                collectSym(Callee);

                //TODO handle inlineAsm
                ///if (SVFUtil::isa<InlineAsm>(Callee))

            }
            //@}
        }
    }

    symInfo->totalSymNum = NodeIDAllocator::get()->endSymbolAllocation();
    if (Options::SymTabPrint)
    {
        SymbolTableInfo::SymbolInfo()->dump();
    }
}

/*!
* Collect special sym here
*/
void SymbolTableBuilder::collectNullPtrBlackholeSyms(const Value *val)
{
    if (LLVMUtil::isNullPtrSym(val))
        symInfo->nullPtrSyms.insert(val);
    if (LLVMUtil::isBlackholeSym(val))
        symInfo->blackholeSyms.insert(val);
}

/*!
 * Collect symbols, including value and object syms
 */
void SymbolTableBuilder::collectSym(const Value *val)
{

    //TODO: filter the non-pointer type // if (!SVFUtil::isa<PointerType>(val->getType()))  return;

    DBOUT(DMemModel, outs() << "collect sym from ##" << SVFUtil::value2String(val) << " \n");

    //TODO handle constant expression value here??
    handleCE(val);

    // create a value sym
    collectVal(val);

    // create an object If it is a heap, stack, global, function.
    if (isObject(val))
    {
        collectObj(val);
    }
}

/*!
 * Get value sym, if not available create a new one
 */
void SymbolTableBuilder::collectVal(const Value *val)
{
    // collect and record special sym here
    if (LLVMUtil::isNullPtrSym(val) || LLVMUtil::isBlackholeSym(val))
    {
        collectNullPtrBlackholeSyms(val);
        return;
    }
    SymbolTableInfo::ValueToIDMapTy::iterator iter = symInfo->valSymMap.find(val);
    if (iter == symInfo->valSymMap.end())
    {
        // create val sym and sym type
        SymID id = NodeIDAllocator::get()->allocateValueId();
        symInfo->valSymMap.insert(std::make_pair(val, id));
        DBOUT(DMemModel,
              outs() << "create a new value sym " << id << "\n");
        ///  handle global constant expression here
        if (const GlobalVariable* globalVar = SVFUtil::dyn_cast<GlobalVariable>(val))
            handleGlobalCE(globalVar);
    }

    if (LLVMUtil::isConstantObjSym(val))
        collectObj(val);
}

/*!
 * Get memory object sym, if not available create a new one
 */
void SymbolTableBuilder::collectObj(const Value *val)
{
    val = getGlobalRep(val);
    SymbolTableInfo::ValueToIDMapTy::iterator iter = symInfo->objSymMap.find(val);
    if (iter == symInfo->objSymMap.end())
    {
        // if the object pointed by the pointer is a constant data (e.g., i32 0) or a global constant object (e.g. string)
        // then we treat them as one ConstantObj
        if((LLVMUtil::isConstantObjSym(val) && !symInfo->getModelConstants()))
        {
            symInfo->objSymMap.insert(std::make_pair(val, symInfo->constantSymID()));
        }
        // otherwise, we will create an object for each abstract memory location
        else
        {
            // create obj sym and sym type
            SymID id = NodeIDAllocator::get()->allocateObjectId();
            symInfo->objSymMap.insert(std::make_pair(val, id));
            DBOUT(DMemModel,
                  outs() << "create a new obj sym " << id << "\n");

            // create a memory object
            MemObj* mem = new MemObj(id, createObjTypeInfo(val), val);
            assert(symInfo->objMap.find(id) == symInfo->objMap.end());
            symInfo->objMap[id] = mem;
        }
    }
}

/*!
 * Create unique return sym, if not available create a new one
 */
void SymbolTableBuilder::collectRet(const Function *val)
{
    SymbolTableInfo::FunToIDMapTy::iterator iter = symInfo->returnSymMap.find(val);
    if (iter == symInfo->returnSymMap.end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        symInfo->returnSymMap.insert(std::make_pair(val, id));
        DBOUT(DMemModel,
              outs() << "create a return sym " << id << "\n");
    }
}

/*!
 * Create vararg sym, if not available create a new one
 */
void SymbolTableBuilder::collectVararg(const Function *val)
{
    SymbolTableInfo::FunToIDMapTy::iterator iter = symInfo->varargSymMap.find(val);
    if (iter == symInfo->varargSymMap.end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        symInfo->varargSymMap.insert(std::make_pair(val, id));
        DBOUT(DMemModel,
              outs() << "create a vararg sym " << id << "\n");
    }
}


/*!
 * Handle constant expression
 */
void SymbolTableBuilder::handleCE(const Value *val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* ce = isGepConstantExpr(ref))
        {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << SVFUtil::value2String(ref) << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        }
        else if (const ConstantExpr* ce = isCastConstantExpr(ref))
        {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << SVFUtil::value2String(ref) << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        }
        else if (const ConstantExpr* ce = isSelectConstantExpr(ref))
        {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << SVFUtil::value2String(ref) << "\n");
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
        else if (const ConstantExpr *int2Ptrce = isInt2PtrConstantExpr(ref))
        {
            collectVal(int2Ptrce);
        }
        else if (const ConstantExpr *ptr2Intce = isPtr2IntConstantExpr(ref))
        {
            collectVal(ptr2Intce);
            const Constant *opnd = ptr2Intce->getOperand(0);
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
            // we don't handle constant agrgregate like constant vectors
            collectVal(ref);
        }
        else
        {
            if (SVFUtil::isa<ConstantExpr>(val))
                assert(false && "we don't handle all other constant expression for now!");
            collectVal(ref);
        }
    }
}

/*!
 * Handle global constant expression
 */
void SymbolTableBuilder::handleGlobalCE(const GlobalVariable *G)
{
    assert(G);

    //The type this global points to
    const Type *T = G->getType()->getContainedType(0);
    bool is_array = 0;
    //An array is considered a single variable of its type.
    while (const ArrayType *AT = SVFUtil::dyn_cast<ArrayType>(T))
    {
        T = AT->getElementType();
        is_array = 1;
    }

    if (SVFUtil::isa<StructType>(T))
    {
        //A struct may be used in constant GEP expr.
        for (Value::const_user_iterator it = G->user_begin(), ie = G->user_end();
                it != ie; ++it)
        {
            handleCE(*it);
        }
    }
    else
    {
        if (is_array)
        {
            for (Value::const_user_iterator it = G->user_begin(), ie =
                        G->user_end(); it != ie; ++it)
            {
                handleCE(*it);
            }
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
void SymbolTableBuilder::handleGlobalInitializerCE(const Constant *C)
{

    if (C->getType()->isSingleValueType())
    {
        if (const ConstantExpr *E = SVFUtil::dyn_cast<ConstantExpr>(C))
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
        if(Options::ModelConsts)
        {
            if(const ConstantDataSequential* seq = SVFUtil::dyn_cast<ConstantDataSequential>(data))
            {
                for(u32_t i = 0; i < seq->getNumElements(); i++)
                {
                    const Constant* ct = seq->getElementAsConstant(i);
                    handleGlobalInitializerCE(ct);
                }
            }
            else
            {
                assert((SVFUtil::isa<ConstantAggregateZero>(data) || SVFUtil::isa<UndefValue>(data)) && "Single value type data should have been handled!");
            }
        }
    }
    else
    {
        //TODO:assert(SVFUtil::isa<ConstantVector>(C),"what else do we have");
    }
}

/*
 * Initial the memory object here
 */
ObjTypeInfo* SymbolTableBuilder::createObjTypeInfo(const Value *val)
{
    const PointerType *refTy = nullptr;

    const Instruction *I = SVFUtil::dyn_cast<Instruction>(val);

    // We consider two types of objects:
    // (1) A heap/static object from a callsite
    if (I && isNonInstricCallSite(I))
        refTy = getRefTypeOfHeapAllocOrStatic(I);
    // (2) Other objects (e.g., alloca, global, etc.)
    else
        refTy = SVFUtil::dyn_cast<PointerType>(val->getType());

    if (refTy)
    {
        Type *objTy = getPtrElementType(refTy);
        ObjTypeInfo* typeInfo = new ObjTypeInfo(objTy, Options::MaxFieldLimit);
        initTypeInfo(typeInfo,val);
        return typeInfo;
    }
    else
    {
        writeWrnMsg("try to create an object with a non-pointer type.");
        writeWrnMsg(val->getName().str());
        writeWrnMsg("(" + getSourceLoc(val) + ")");
        if(LLVMUtil::isConstantObjSym(val))
        {
            ObjTypeInfo* typeInfo = new ObjTypeInfo(val->getType(), 0);
            initTypeInfo(typeInfo,val);
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

    const PointerType * refty = SVFUtil::dyn_cast<PointerType>(val->getType());
    assert(refty && "this value should be a pointer type!");
    Type* elemTy = getPtrElementType(refty);
    bool isPtrObj = false;
    // Find the inter nested array element
    while (const ArrayType *AT= SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        elemTy = AT->getElementType();
        if(elemTy->isPointerTy())
            isPtrObj = true;
        if(SVFUtil::isa<GlobalVariable>(val) && SVFUtil::cast<GlobalVariable>(val)->hasInitializer()
                && SVFUtil::isa<ConstantArray>(SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            typeinfo->setFlag(ObjTypeInfo::CONST_ARRAY_OBJ);
        else
            typeinfo->setFlag(ObjTypeInfo::VAR_ARRAY_OBJ);
    }
    if (const StructType *ST= SVFUtil::dyn_cast<StructType>(elemTy))
    {
        const std::vector<const Type*>& flattenFields = SymbolTableInfo::SymbolInfo()->getFlattenFieldTypes(ST);
        for(std::vector<const Type*>::const_iterator it = flattenFields.begin(), eit = flattenFields.end();
                it!=eit; ++it)
        {
            if((*it)->isPointerTy())
                isPtrObj = true;
        }
        if(SVFUtil::isa<GlobalVariable>(val) && SVFUtil::cast<GlobalVariable>(val)->hasInitializer()
                && SVFUtil::isa<ConstantStruct>(SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            typeinfo->setFlag(ObjTypeInfo::CONST_STRUCT_OBJ);
        else
            typeinfo->setFlag(ObjTypeInfo::VAR_STRUCT_OBJ);
    }
    else if (elemTy->isPointerTy())
    {
        isPtrObj = true;
    }

    if(isPtrObj)
        typeinfo->setFlag(ObjTypeInfo::HASPTR_OBJ);
}

/*!
 * Analyse types of heap and static objects
 */
void SymbolTableBuilder::analyzeHeapObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    if(const Value* castUse = getUniqueUseViaCastInst(val))
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
        typeinfo->resetTypeForHeapStaticObj(castUse->getType());
        analyzeObjType(typeinfo,castUse);
    }
    else
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
        typeinfo->setFlag(ObjTypeInfo::HASPTR_OBJ);
    }
}

/*!
 * Analyse types of heap and static objects
 */
void SymbolTableBuilder::analyzeStaticObjType(ObjTypeInfo* typeinfo, const Value* val)
{
    if(const Value* castUse = getUniqueUseViaCastInst(val))
    {
        typeinfo->setFlag(ObjTypeInfo::STATIC_OBJ);
        typeinfo->resetTypeForHeapStaticObj(castUse->getType());
        analyzeObjType(typeinfo,castUse);
    }
    else
    {
        typeinfo->setFlag(ObjTypeInfo::HEAP_OBJ);
        typeinfo->setFlag(ObjTypeInfo::HASPTR_OBJ);
    }
}

/*!
 * Initialize the type info of an object
 */
void SymbolTableBuilder::initTypeInfo(ObjTypeInfo* typeinfo, const Value* val)
{

    u32_t objSize = 1;
    // Global variable
    if (SVFUtil::isa<Function>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::FUNCTION_OBJ);
        analyzeObjType(typeinfo,val);
        objSize = getObjSize(typeinfo->getType());
    }
    else if(const AllocaInst* allocaInst = SVFUtil::dyn_cast<AllocaInst>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::STACK_OBJ);
        analyzeObjType(typeinfo,val);
        /// This is for `alloca <ty> <NumElements>`. For example, `alloca i64 3` allocates 3 i64 on the stack (objSize=3)
        /// In most cases, `NumElements` is not specified in the instruction, which means there is only one element (objSize=1).
        if(const ConstantInt *sz = SVFUtil::dyn_cast<ConstantInt>(allocaInst->getArraySize()))
            objSize = sz->getZExtValue() * getObjSize(typeinfo->getType());
        else
            objSize = getObjSize(typeinfo->getType());
    }
    else if(SVFUtil::isa<GlobalVariable>(val))
    {
        typeinfo->setFlag(ObjTypeInfo::GLOBVAR_OBJ);
        if(LLVMUtil::isConstantObjSym(val))
            typeinfo->setFlag(ObjTypeInfo::CONST_GLOBAL_OBJ);
        analyzeObjType(typeinfo,val);
        objSize = getObjSize(typeinfo->getType());
    }
    else if (SVFUtil::isa<Instruction>(val) && isHeapAllocExtCall(SVFUtil::cast<Instruction>(val)))
    {
        analyzeHeapObjType(typeinfo,val);
        // Heap object, label its field as infinite here
        objSize = typeinfo->getMaxFieldOffsetLimit();
    }
    else if (SVFUtil::isa<Instruction>(val) && isStaticExtCall(SVFUtil::cast<Instruction>(val)))
    {
        analyzeStaticObjType(typeinfo,val);
        // static object allocated before main, label its field as infinite here
        objSize = typeinfo->getMaxFieldOffsetLimit();
    }
    else if(ArgInProgEntryFunction(val))
    {
        analyzeStaticObjType(typeinfo,val);
        // user input data, label its field as infinite here
        objSize = typeinfo->getMaxFieldOffsetLimit();
    }
    else if(SVFUtil::isConstantData(val))
    {
        typeinfo->setFlag(ObjTypeInfo::CONST_DATA);
        objSize = SymbolTableInfo::SymbolInfo()->getNumOfFlattenElements(val->getType());
    }
    else
    {
        assert("what other object do we have??");
        abort();
    }

    // Reset maxOffsetLimit if it is over the total fieldNum of this object
    if(typeinfo->getMaxFieldOffsetLimit() > objSize)
        typeinfo->setNumOfElements(objSize);
}

/*!
 * Return size of this Object
 */
u32_t SymbolTableBuilder::getObjSize(const Type* ety)
{
    assert(ety && "type is null?");
    u32_t numOfFields = 1;
    if (SVFUtil::isa<StructType>(ety) || SVFUtil::isa<ArrayType>(ety))
    {
        numOfFields = SymbolTableInfo::SymbolInfo()->getNumOfFlattenElements(ety);
    }
    return numOfFields;
}
