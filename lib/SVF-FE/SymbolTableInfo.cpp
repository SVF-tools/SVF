//===- SymbolTableInfo.cpp -- Symbol information from IR------------------------//
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
 * SymbolTableInfo.cpp
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#include <memory>

#include "SVF-FE/SymbolTableInfo.h"
#include "MemoryModel/MemModel.h"
#include "Util/NodeIDAllocator.h"
#include "Util/Options.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/CPPUtil.h"
#include "SVF-FE/BreakConstantExpr.h"
#include "SVF-FE/GEPTypeBridgeIterator.h" // include bridge_gep_iterator

using namespace std;
using namespace SVF;
using namespace SVFUtil;

DataLayout* SymbolTableInfo::dl = nullptr;
SymbolTableInfo* SymbolTableInfo::symInfo = nullptr;

/*
 * Initial the memory object here
 */
void MemObj::init(const Value *val)
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
        Type *objTy = refTy->getElementType();
        if(Options::LocMemModel)
            typeInfo = new LocObjTypeInfo(val, objTy, Options::MaxFieldLimit);
        else
            typeInfo = new ObjTypeInfo(val, objTy, Options::MaxFieldLimit);
        typeInfo->init(val);
    }
    else
    {
        writeWrnMsg("try to create an object with a non-pointer type.");
        writeWrnMsg(val->getName());
        writeWrnMsg("(" + getSourceLoc(val) + ")");
        assert(false && "Memory object must be held by a pointer-typed ref value.");
    }
}

/*!
 * Get the symbol table instance
 */
SymbolTableInfo* SymbolTableInfo::SymbolInfo()
{
    if (symInfo == nullptr)
    {
        if(Options::LocMemModel)
            symInfo = new LocSymTableInfo();
        else
            symInfo = new SymbolTableInfo();
        symInfo->setModelConstants(Options::ModelConsts);
    }
    return symInfo;
}

/*!
 * Collect a LLVM type info
 */
void SymbolTableInfo::collectTypeInfo(const Type* ty)
{
    assert(typeToFieldInfo.find(ty) == typeToFieldInfo.end() && "this type has been collected before");

    if (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(ty))
        collectArrayInfo(aty);
    else if (const StructType* sty = SVFUtil::dyn_cast<StructType>(ty))
        collectStructInfo(sty);
    else
        collectSimpleTypeInfo(ty);
}


/*!
 * Fill in StInfo for an array type.
 */
void SymbolTableInfo::collectArrayInfo(const ArrayType* ty)
{
    StInfo* stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    u64_t out_num = ty->getNumElements();
    const llvm::Type* elemTy = ty->getElementType();
    u32_t out_stride = getTypeSizeInBytes(elemTy);
    while (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        out_num *= aty->getNumElements();
        elemTy = aty->getElementType();
        out_stride = getTypeSizeInBytes(elemTy);
    }

    /// Array itself only has one field which is the inner most element
    stinfo->addFldWithType(0, 0, elemTy);

    /// Array's flatten field infor is the same as its element's
    /// flatten infor.
    StInfo* elemStInfo = getStructInfo(elemTy);
    u32_t nfE = elemStInfo->getFlattenFieldInfoVec().size();
    for (u32_t j = 0; j < nfE; j++)
    {
        u32_t idx = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenFldIdx();
        u32_t off = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenByteOffset();
        const Type* fieldTy = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
        FieldInfo::ElemNumStridePairVec pair = elemStInfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
        /// append the additional number
        pair.push_back(std::make_pair(out_num, out_stride));
        FieldInfo field(idx, off, fieldTy, pair);
        stinfo->getFlattenFieldInfoVec().push_back(field);
    }
}


/*!
 * Fill in struct_info for T.
 * Given a Struct type, we recursively extend and record its fields and types.
 */
void SymbolTableInfo::collectStructInfo(const StructType *sty)
{
    /// The struct info should not be processed before
    StInfo* stinfo = new StInfo();
    typeToFieldInfo[sty] = stinfo;

    // Number of fields after flattening the struct
    u32_t nf = 0;
    // field of the current struct
    u32_t field_idx = 0;
    for (StructType::element_iterator it = sty->element_begin(), ie =
                sty->element_end(); it != ie; ++it, ++field_idx)
    {
        const Type *et = *it;
        // This offset is computed after alignment with the current struct
        u64_t eOffsetInBytes = getTypeSizeInBytes(sty, field_idx);
        //The offset is where this element will be placed in the exp. struct.
        /// FIXME: As the layout size is uint_64, here we assume
        /// offset with uint_32 (Size_t) is large enough and will not cause overflow
        stinfo->addFldWithType(nf, static_cast<u32_t>(eOffsetInBytes), et);

        if (SVFUtil::isa<StructType>(et) || SVFUtil::isa<ArrayType>(et))
        {
            StInfo * subStinfo = getStructInfo(et);
            u32_t nfE = subStinfo->getFlattenFieldInfoVec().size();
            //Copy ST's info, whose element 0 is the size of ST itself.
            for (u32_t j = 0; j < nfE; j++)
            {
                u32_t fldIdx = nf + subStinfo->getFlattenFieldInfoVec()[j].getFlattenFldIdx();
                u32_t off = eOffsetInBytes + subStinfo->getFlattenFieldInfoVec()[j].getFlattenByteOffset();
                const Type* elemTy = subStinfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
                FieldInfo::ElemNumStridePairVec pair = subStinfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
                pair.push_back(std::make_pair(1, 0));
                FieldInfo field(fldIdx, off,elemTy,pair);
                stinfo->getFlattenFieldInfoVec().push_back(field);
            }
            nf += nfE;
        }
        else     //simple type
        {
            FieldInfo::ElemNumStridePairVec pair;
            pair.push_back(std::make_pair(1,0));
            FieldInfo field(nf, eOffsetInBytes, et,pair);
            stinfo->getFlattenFieldInfoVec().push_back(field);
            ++nf;
        }
    }

    //Record the size of the complete struct and update max_struct.
    if (nf > maxStSize)
    {
        maxStruct = sty;
        maxStSize = nf;
    }
}


/*!
 * Collect simple type (non-aggregate) info
 */
void SymbolTableInfo::collectSimpleTypeInfo(const Type* ty)
{
    StInfo* stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    /// Only one field
    stinfo->addFldWithType(0,0, ty);

    FieldInfo::ElemNumStridePairVec pair;
    pair.push_back(std::make_pair(1,0));
    FieldInfo field(0, 0, ty, pair);
    stinfo->getFlattenFieldInfoVec().push_back(field);
}

/*!
 * Compute gep offset
 */
bool SymbolTableInfo::computeGepOffset(const User *V, LocationSet& ls)
{
    assert(V);

    const llvm::GEPOperator *gepOp = SVFUtil::dyn_cast<const llvm::GEPOperator>(V);
    DataLayout * dataLayout = getDataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule());
    llvm::APInt byteOffset(dataLayout->getIndexSizeInBits(gepOp->getPointerAddressSpace()),0,true);
    if(gepOp && dataLayout && gepOp->accumulateConstantOffset(*dataLayout,byteOffset))
    {
        Size_t bo = byteOffset.getSExtValue();
        ls.setByteOffset(bo + ls.getByteOffset());
    }

    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi)
    {

        // Handling array types, skipe array handling here
        // We treat whole array as one, then we can distinguish different field of an array of struct
        // e.g. s[1].f1 is differet from s[0].f2
        if(SVFUtil::isa<ArrayType>(*gi))
            continue;

        //The int-value object of the current index operand
        //  (may not be constant for arrays).
        ConstantInt *op = SVFUtil::dyn_cast<ConstantInt>(gi.getOperand());

        /// given a gep edge p = q + i,
        if(!op)
        {
            return false;
        }
        //The actual index
        Size_t idx = op->getSExtValue();


        // Handling pointer types
        // These GEP instructions are simply making address computations from the base pointer address
        // e.g. idx = (char*) &p + 4,  at this case gep only one offset index (idx)
        // Case 1: This operation is likely accessing an array through pointer p.
        // Case 2: It may also be used to access a field of a struct (which is not ANSI-compliant)
        // Since this is a field-index based memory model,
        // for case 1: we consider the whole array as one element, This can be improved by LocMemModel as it can distinguish different
        // elements of the same array.
        // for case 2: we treat idx the same as &p by ignoring const 4 (This handling is unsound since the program itself is not ANSI-compliant).
        if (SVFUtil::isa<PointerType>(*gi))
        {
            //off += idx;
        }


        // Handling struct here
        if (const StructType *ST = SVFUtil::dyn_cast<StructType>(*gi) )
        {
            assert(op && "non-const struct index in GEP");
            const vector<u32_t> &so = SymbolTableInfo::SymbolInfo()->getFattenFieldIdxVec(ST);
            if ((unsigned)idx >= so.size())
            {
                outs() << "!! Struct index out of bounds" << idx << "\n";
                assert(0);
            }
            //add the translated offset
            ls.setFldIdx(ls.getOffset() + so[idx]);
        }
    }
    return true;
}


/*!
 * Replace fields with flatten fields of T if the number of its fields is larger than msz.
 */
u32_t SymbolTableInfo::getFields(std::vector<LocationSet>& fields, const Type* T, u32_t msz)
{
    if (!SVFUtil::isa<PointerType>(T))
        return 0;

    T = T->getContainedType(0);
    const std::vector<FieldInfo>& stVec = SymbolTableInfo::SymbolInfo()->getFlattenFieldInfoVec(T);
    u32_t sz = stVec.size();
    if (msz < sz)
    {
        /// Replace fields with T's flatten fields.
        fields.clear();
        for(std::vector<FieldInfo>::const_iterator it = stVec.begin(), eit = stVec.end(); it!=eit; ++it)
            fields.push_back(LocationSet(*it));
    }

    return sz;
}


/*!
 * Find the base type and the max possible offset for an object pointed to by (V).
 */
const Type *SymbolTableInfo::getBaseTypeAndFlattenedFields(const Value *V, std::vector<LocationSet> &fields)
{
    assert(V);
    fields.push_back(LocationSet(0));

    const Type *T = V->getType();
    // Use the biggest struct type out of all operands.
    if (const User *U = SVFUtil::dyn_cast<User>(V))
    {
        u32_t msz = 1;      //the max size seen so far
        // In case of BitCast, try the target type itself
        if (SVFUtil::isa<BitCastInst>(V))
        {
            u32_t sz = getFields(fields, T, msz);
            if (msz < sz)
            {
                msz = sz;
            }
        }
        // Try the types of all operands
        for (User::const_op_iterator it = U->op_begin(), ie = U->op_end();
                it != ie; ++it)
        {
            const Type *operandtype = it->get()->getType();

            u32_t sz = getFields(fields, operandtype, msz);
            if (msz < sz)
            {
                msz = sz;
                T = operandtype;
            }
        }
    }
    // If V is a CE, the actual pointer type is its operand.
    else if (const ConstantExpr *E = SVFUtil::dyn_cast<ConstantExpr>(V))
    {
        T = E->getOperand(0)->getType();
        getFields(fields, T, 0);
    }
    // Handle Argument case
    else if (SVFUtil::isa<Argument>(V))
    {
        getFields(fields, T, 0);
    }

    while (const PointerType *ptype = SVFUtil::dyn_cast<PointerType>(T))
        T = ptype->getElementType();
    return T;
}

/*!
 * Get modulus offset given the type information
 */
LocationSet SymbolTableInfo::getModulusOffset(const MemObj* obj, const LocationSet& ls)
{

    /// if the offset is negative, it's possible that we're looking for an obj node out of range
    /// of current struct. Make the offset positive so we can still get a node within current
    /// struct to represent this obj.

    Size_t offset = ls.getOffset();
    if(offset < 0)
    {
        writeWrnMsg("try to create a gep node with negative offset.");
        offset = abs(offset);
    }
    u32_t maxOffset = obj->getMaxFieldOffsetLimit();
    if (maxOffset != 0)
        offset = offset % maxOffset;
    else
        offset = 0;

    return LocationSet(offset);
}


/*!
 * Invoke llvm passes to modify module
 */
void SymbolTableInfo::prePassSchedule(SVFModule* svfModule)
{
    /// BreakConstantGEPs Pass
    std::unique_ptr<BreakConstantGEPs> p1 = std::make_unique<BreakConstantGEPs>();
    for (u32_t i = 0; i < LLVMModuleSet::getLLVMModuleSet()->getModuleNum(); ++i)
    {
        Module *module = LLVMModuleSet::getLLVMModuleSet()->getModule(i);
        p1->runOnModule(*module);
    }

    /// MergeFunctionRets Pass
    std::unique_ptr<UnifyFunctionExitNodes> p2 =
        std::make_unique<UnifyFunctionExitNodes>();
    for (SVFModule::llvm_iterator F = svfModule->llvmFunBegin(), E = svfModule->llvmFunEnd(); F != E; ++F)
    {
        Function *fun = *F;
        if (fun->isDeclaration())
            continue;
        p2->runOnFunction(*fun);
    }
}

/*!
 *  This method identify which is value sym and which is object sym
 */
void SymbolTableInfo::buildMemModel(SVFModule* svfModule)
{
    SVFUtil::increaseStackSize();

    prePassSchedule(svfModule);

    mod = svfModule;

    StInfo::setMaxFieldLimit(Options::MaxFieldLimit);

    // Object #0 is black hole the object that may point to any object
    assert(totalSymNum == BlackHole && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum++, BlackHole));
    createBlkOrConstantObj(BlackHole);

    // Object #1 always represents the constant
    assert(totalSymNum == ConstantObj && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum++, ConstantObj));
    createBlkOrConstantObj(ConstantObj);

    // Pointer #2 always represents the pointer points-to black hole.
    assert(totalSymNum == BlkPtr && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum++, BlkPtr));

    // Pointer #3 always represents the null pointer.
    assert(totalSymNum == NullPtr && "Something changed!");
    symTyMap.insert(std::make_pair(totalSymNum, NullPtr));

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
                callSiteSet.insert(cs);
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

    NodeIDAllocator::get()->endSymbolAllocation();
    if (Options::SymTabPrint) {
        SymbolTableInfo::SymbolInfo()->dump();
    }
}

/*!
 * Destroy the memory for this symbol table after use
 */
void SymbolTableInfo::destroy()
{

    for (IDToMemMapTy::iterator iter = objMap.begin(); iter != objMap.end();
            ++iter)
    {
        if (iter->second)
            delete iter->second;
    }
    for (TypeToFieldInfoMap::iterator iter = typeToFieldInfo.begin();
            iter != typeToFieldInfo.end(); ++iter)
    {
        if (iter->second)
            delete iter->second;
    }
}

/*!
 * Collect symbols, including value and object syms
 */
void SymbolTableInfo::collectSym(const Value *val)
{

    //TODO: filter the non-pointer type // if (!SVFUtil::isa<PointerType>(val->getType()))  return;

    DBOUT(DMemModel, outs() << "collect sym from ##" << *val << " \n");

    // special sym here
    if (isNullPtrSym(val) || isBlackholeSym(val))
        return;

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
void SymbolTableInfo::collectVal(const Value *val)
{
    ValueToIDMapTy::iterator iter = valSymMap.find(val);
    if (iter == valSymMap.end())
    {
        // create val sym and sym type
        SymID id = NodeIDAllocator::get()->allocateValueId();
        valSymMap.insert(std::make_pair(val, id));
        symTyMap.insert(std::make_pair(id, ValSym));
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
void SymbolTableInfo::collectObj(const Value *val)
{
    ValueToIDMapTy::iterator iter = objSymMap.find(val);
    if (iter == objSymMap.end())
    {
        // if the object pointed by the pointer is a constant data (e.g., i32 0) or a global constant object (e.g. string)
        // then we treat them as one ConstantObj
        if(isConstantData(val) || (isConstantObjSym(val) && !getModelConstants()))
        {
            objSymMap.insert(std::make_pair(val, constantSymID()));
        }
        // otherwise, we will create an object for each abstract memory location
        else
        {
            // create obj sym and sym type
            SymID id = NodeIDAllocator::get()->allocateObjectId();
            objSymMap.insert(std::make_pair(val, id));
            symTyMap.insert(std::make_pair(id, ObjSym));
            DBOUT(DMemModel,
                  outs() << "create a new obj sym " << id << "\n");

            // create a memory object
            MemObj* mem = new MemObj(val, id);
            assert(objMap.find(id) == objMap.end());
            objMap[id] = mem;
        }
    }
}

/*!
 * Create unique return sym, if not available create a new one
 */
void SymbolTableInfo::collectRet(const Function *val)
{
    FunToIDMapTy::iterator iter = returnSymMap.find(val);
    if (iter == returnSymMap.end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        returnSymMap.insert(std::make_pair(val, id));
        symTyMap.insert(std::make_pair(id, RetSym));
        DBOUT(DMemModel,
              outs() << "create a return sym " << id << "\n");
    }
}

/*!
 * Create vararg sym, if not available create a new one
 */
void SymbolTableInfo::collectVararg(const Function *val)
{
    FunToIDMapTy::iterator iter = varargSymMap.find(val);
    if (iter == varargSymMap.end())
    {
        SymID id = NodeIDAllocator::get()->allocateValueId();
        varargSymMap.insert(std::make_pair(val, id));
        symTyMap.insert(std::make_pair(id, VarargSym));
        DBOUT(DMemModel,
              outs() << "create a vararg sym " << id << "\n");
    }
}

/*!
 * Check whether this value is null pointer
 */
bool SymbolTableInfo::isNullPtrSym(const Value *val)
{
    if (const Constant* v = SVFUtil::dyn_cast<Constant>(val))
    {
        return v->isNullValue() && v->getType()->isPointerTy();
    }
    return false;
}

/*!
 * Check whether this value is a black hole
 */
bool SymbolTableInfo::isBlackholeSym(const Value *val)
{
    return (SVFUtil::isa<UndefValue>(val));
}

/*!
 * Check whether this value points-to a constant object
 */
bool SymbolTableInfo::isConstantObjSym(const Value *val)
{
    if (const GlobalVariable* v = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        if (cppUtil::isValVtbl(const_cast<GlobalVariable*>(v)))
            return false;
        else if (!v->hasInitializer()){
            if(v->isExternalLinkage(v->getLinkage()))
                return false;
            else
                return true;
        }
        else
        {
            StInfo *stInfo = getStructInfo(v->getInitializer()->getType());
            const std::vector<FieldInfo> &fields = stInfo->getFlattenFieldInfoVec();
            for (std::vector<FieldInfo>::const_iterator it = fields.begin(), eit = fields.end(); it != eit; ++it)
            {
                const FieldInfo &field = *it;
                const Type *elemTy = field.getFlattenElemTy();
                assert(!SVFUtil::isa<FunctionType>(elemTy) && "Initializer of a global is a function?");
                if (SVFUtil::isa<PointerType>(elemTy))
                    return false;
            }

            return v->isConstant();
        }
    }
    return SVFUtil::isConstantData(val);
}


/*!
 * Handle constant expression
 */
void SymbolTableInfo::handleCE(const Value *val)
{
    if (const Constant* ref = SVFUtil::dyn_cast<Constant>(val))
    {
        if (const ConstantExpr* ce = isGepConstantExpr(ref))
        {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << *ref << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        }
        else if (const ConstantExpr* ce = isCastConstantExpr(ref))
        {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << *ref << "\n");
            collectVal(ce);
            collectVal(ce->getOperand(0));
            // handle the recursive constant express case
            // like (gep (bitcast (gep X 1)) 1); the inner gep is ce->getOperand(0)
            handleCE(ce->getOperand(0));
        }
        else if (const ConstantExpr* ce = isSelectConstantExpr(ref))
        {
            DBOUT(DMemModelCE,
                  outs() << "handle constant expression " << *ref << "\n");
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
		else if (const ConstantExpr *int2Ptrce = isInt2PtrConstantExpr(ref)) {
			collectVal(int2Ptrce);
		} else if (const ConstantExpr *ptr2Intce = isPtr2IntConstantExpr(ref)) {
			collectVal(ptr2Intce);
			const Constant *opnd = ptr2Intce->getOperand(0);
			handleCE(opnd);
		} else if (isTruncConstantExpr(ref) || isCmpConstantExpr(ref)) {
			collectVal(ref);
		} else if (isBinaryConstantExpr(ref)) {
			collectVal(ref);
		} else if (isUnaryConstantExpr(ref)) {
			// we don't handle unary constant expression like fneg(x) now
			collectVal(ref);
		} else if (SVFUtil::isa<ConstantAggregate>(ref)) {
			// we don't handle constant agrgregate like constant vectors
			collectVal(ref);
		} else {
			if (SVFUtil::isa<ConstantExpr>(val))
				assert(false && "we don't handle all other constant expression for now!");
			collectVal(ref);
		}
    }
}

/*!
 * Handle global constant expression
 */
void SymbolTableInfo::handleGlobalCE(const GlobalVariable *G)
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
        handleGlobalInitializerCE(G->getInitializer(), 0);
    }
}

/*!
 * Handle global variable initialization
 */
void SymbolTableInfo::handleGlobalInitializerCE(const Constant *C,
        u32_t offset)
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
            handleGlobalInitializerCE(SVFUtil::cast<Constant>(C->getOperand(i)), offset);
        }
    }
    else if (SVFUtil::isa<ConstantStruct>(C))
    {
        const StructType *sty = SVFUtil::cast<StructType>(C->getType());
        const std::vector<u32_t>& offsetvect =
            SymbolTableInfo::SymbolInfo()->getFattenFieldIdxVec(sty);
        for (u32_t i = 0, e = C->getNumOperands(); i != e; i++)
        {
            u32_t off = offsetvect[i];
            handleGlobalInitializerCE(SVFUtil::cast<Constant>(C->getOperand(i)),
                                      offset + off);
        }
    }
}


/*
 * Print out the composite type information
 */
void SymbolTableInfo::printFlattenFields(const Type* type)
{

    if(const ArrayType *at = SVFUtil::dyn_cast<ArrayType> (type))
    {
        outs() <<"  {Type: ";
        at->print(outs());
        outs() << "}\n";
        outs() << "\tarray type ";
        outs() << "\t [element size = " << getTypeSizeInBytes(at->getElementType()) << "]\n";
        outs() << "\n";
    }

    else if(const StructType *st = SVFUtil::dyn_cast<StructType> (type))
    {
        outs() <<"  {Type: ";
        st->print(outs());
        outs() << "}\n";
        std::vector<FieldInfo>& finfo = getStructInfo(st)->getFlattenFieldInfoVec();
        int field_idx = 0;
        for(std::vector<FieldInfo>::iterator it = finfo.begin(), eit = finfo.end();
                it!=eit; ++it, field_idx++)
        {
            outs() << " \tField_idx = " << (*it).getFlattenFldIdx() << " [offset: " << (*it).getFlattenByteOffset();
            outs() << ", field type: ";
            (*it).getFlattenElemTy()->print(outs());
            outs() << ", field size: " << getTypeSizeInBytes((*it).getFlattenElemTy());
            outs() << ", field stride pair: ";
            for(FieldInfo::ElemNumStridePairVec::const_iterator pit = (*it).elemStridePairBegin(),
                    peit = (*it).elemStridePairEnd(); pit!=peit; ++pit)
            {
                outs() << "[ " << pit->first << ", " << pit->second << " ] ";
            }
            outs() << "\n";
        }
        outs() << "\n";
    }

    else if (const PointerType* pt= SVFUtil::dyn_cast<PointerType> (type))
    {
        u32_t sizeInBits = getTypeSizeInBytes(type);
        u32_t eSize = getTypeSizeInBytes(pt->getElementType());
        outs() << "  {Type: ";
        pt->print(outs());
        outs() << "}\n";
        outs() <<"\t [pointer size = " << sizeInBits << "]";
        outs() <<"\t [target size = " << eSize << "]\n";
        outs() << "\n";
    }

    else if ( const FunctionType* fu= SVFUtil::dyn_cast<FunctionType> (type))
    {
        outs() << "  {Type: ";
        fu->getReturnType()->print(outs());
        outs() << "(Function)}\n\n";
    }

    else
    {
        assert(type->isSingleValueType() && "not a single value type, then what else!!");
        /// All rest types are scalar type?
        u32_t eSize = getTypeSizeInBytes(type);
        outs() <<"  {Type: ";
        type->print(outs());
        outs() << "}\n";
        outs() <<"\t [object size = " << eSize << "]\n";
        outs() << "\n";
    }
}

std::string SymbolTableInfo::toString(SYMTYPE symtype)
{
    switch (symtype) {
        case SYMTYPE::BlackHole: {
            return "BlackHole";
        }
        case SYMTYPE::ConstantObj: {
            return "ConstantObj";
        }
        case SYMTYPE::BlkPtr: {
            return "BlkPtr";
        }
        case SYMTYPE::NullPtr: {
            return "NullPtr";
        }
        case SYMTYPE::ValSym: {
            return "ValSym";
        }
        case SYMTYPE::ObjSym: {
            return "ObjSym";
        }
        case SYMTYPE::RetSym: {
            return "RetSym";
        }
        case SYMTYPE::VarargSym: {
            return "VarargSym";
        }
        default: {
            return "Invalid SYMTYPE";
        }
    }
}

void SymbolTableInfo::dump()
{
    OrderedMap<SymID, Value*> idmap;
    SymID maxid = 0;
    for (ValueToIDMapTy::iterator iter = valSymMap.begin(); iter != valSymMap.end();
         ++iter)
    {
        const SymID i = iter->second;
        maxid = max(i, maxid);
        Value* val = (Value*) iter->first;
        idmap[i] = val;
    }
    for (ValueToIDMapTy::iterator iter = objSymMap.begin(); iter != objSymMap.end();
         ++iter)
    {
        const SymID i = iter->second;
        maxid = max(i, maxid);
        Value* val = (Value*) iter->first;
        idmap[i] = val;
    }
    for (FunToIDMapTy::iterator iter = returnSymMap.begin(); iter != returnSymMap.end();
         ++iter)
    {
        const SymID i = iter->second;
        maxid = max(i, maxid);
        Value* val = (Value*) iter->first;
        idmap[i] = val;
    }
    for (FunToIDMapTy::iterator iter = varargSymMap.begin(); iter != varargSymMap.end();
         ++iter)
    {
        const SymID i = iter->second;
        maxid = max(i, maxid);
        Value* val = (Value*) iter->first;
        idmap[i] = val;
    }
    outs() << "{SymbolTableInfo \n";
    for (SymID symid = 0; symid <= maxid; ++symid) {
        SYMTYPE symtype = this->symTyMap.at(symid);
        string typestring = toString(symtype);
        outs() << "  " << typestring << symid;
        if (symtype < SYMTYPE::ValSym) {
            outs() << "\n";
        } else {
            auto I = idmap.find(symid);
            if (I == idmap.end()) {
                outs() << "No value\n";
                break;
            }
            const Value* val = I->second;
            outs() << " -> " << value2String(val) << "\n";
        }
    }
    outs() << "}\n";
}

/*
 * Get the type size given a target data layout
 */
u32_t SymbolTableInfo::getTypeSizeInBytes(const Type* type)
{

    // if the type has size then simply return it, otherwise just return 0
    if(type->isSized())
        return  getDataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule())->getTypeStoreSize(const_cast<Type*>(type));
    else
        return 0;
}

u32_t SymbolTableInfo::getTypeSizeInBytes(const StructType *sty, u32_t field_idx)
{

    const StructLayout *stTySL = getDataLayout(LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule())->getStructLayout( const_cast<StructType *>(sty) );
    /// if this struct type does not have any element, i.e., opaque
    if(sty->isOpaque())
        return 0;
    else
        return stTySL->getElementOffset(field_idx);
}



