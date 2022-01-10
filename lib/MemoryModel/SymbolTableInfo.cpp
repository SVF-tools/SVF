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

#include "MemoryModel/SymbolTableInfo.h"
#include "Util/Options.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;

DataLayout* SymbolTableInfo::dl = nullptr;
SymbolTableInfo* SymbolTableInfo::symInfo = nullptr;
u32_t StInfo::maxFieldLimit = 0;

/*
 * Initial the memory object here (for a dummy object)
 */
ObjTypeInfo* SymbolTableInfo::createObjTypeInfo(const Type* type)
{
    ObjTypeInfo* typeInfo = new ObjTypeInfo(StInfo::getMaxFieldLimit(),type);
    typeInfo->analyzeHeapObjType(type);
    return typeInfo;
}

/*!
 * Get the symbol table instance
 */
SymbolTableInfo* SymbolTableInfo::SymbolInfo()
{
    if (symInfo == nullptr)
    {
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

    if(dl){
        delete dl;
        dl = nullptr;
    }
    if(mod){
        delete mod;
        mod = nullptr;
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


MemObj* SymbolTableInfo::createBlkObj(SymID symId)
{
    assert(isBlkObj(symId));
    assert(objMap.find(symId)==objMap.end());
    MemObj* obj = new MemObj(symId, createObjTypeInfo());
    objMap[symId] = obj;
    return obj;
}

MemObj* SymbolTableInfo::createConstantObj(SymID symId)
{
    assert(isConstantObj(symId));
    assert(objMap.find(symId)==objMap.end());
    MemObj* obj = new MemObj(symId, createObjTypeInfo());
    objMap[symId] = obj;
    return obj;
}

const MemObj* SymbolTableInfo::createDummyObj(SymID symId, const Type* type)
{
    assert(objMap.find(symId)==objMap.end() && "this dummy obj has been created before");
    MemObj* memObj = new MemObj(symId, createObjTypeInfo(type));
    objMap[symId] = memObj;
    return memObj;
}

const std::vector<u32_t>& SymbolTableInfo::getFattenFieldIdxVec(const Type *T)
{
    return getStructInfoIter(T)->second->getFieldIdxVec();
}

const std::vector<u32_t>& SymbolTableInfo::getFattenFieldOffsetVec(const Type *T)
{
    return getStructInfoIter(T)->second->getFieldOffsetVec();
}

const std::vector<FieldInfo>& SymbolTableInfo::getFlattenFieldInfoVec(const Type *T)
{
    return getStructInfoIter(T)->second->getFlattenFieldInfoVec();
}

const Type* SymbolTableInfo::getOrigSubTypeWithFldInx(const Type* baseType, u32_t field_idx)
{
    return getStructInfoIter(baseType)->second->getFieldTypeWithFldIdx(field_idx);
}

const Type* SymbolTableInfo::getOrigSubTypeWithByteOffset(const Type* baseType, u32_t byteOffset)
{
    return getStructInfoIter(baseType)->second->getFieldTypeWithByteOffset(byteOffset);
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
        case SYMTYPE::ValSymbol: {
            return "ValSym";
        }
        case SYMTYPE::ObjSymbol: {
            return "ObjSym";
        }
        case SYMTYPE::RetSymbol: {
            return "RetSym";
        }
        case SYMTYPE::VarargSymbol: {
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
    for (auto iter : idmap) {
        outs() << iter.first << " " << value2String(iter.second) << "\n";
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



/*!
 * Analyse types of heap and static objects
 */
void ObjTypeInfo::analyzeHeapObjType(const Type*)
{
    // TODO: Heap and static objects are considered as pointers right now.
    //       Refine this function to get more details about heap and static objects.
    setFlag(HEAP_OBJ);
    setFlag(HASPTR_OBJ);
}

/*!
 * Analyse types of heap and static objects
 */
void ObjTypeInfo::analyzeStaticObjType(const Type*)
{
    // TODO: Heap and static objects are considered as pointers right now.
    //       Refine this function to get more details about heap and static objects.
    setFlag(STATIC_OBJ);
    setFlag(HASPTR_OBJ);
}

/*!
 * Whether a location set is a pointer type or not
 */
bool ObjTypeInfo::isNonPtrFieldObj(const LocationSet& ls)
{
    if (isHeap() || isStaticObj())
        return false;

    const Type* ety = getType();
    while (const ArrayType *AT= SVFUtil::dyn_cast<ArrayType>(ety))
    {
        ety = AT->getElementType();
    }

    if (SVFUtil::isa<StructType>(ety) || SVFUtil::isa<ArrayType>(ety))
    {
        bool hasIntersection = false;
        const vector<FieldInfo> &infovec = SymbolTableInfo::SymbolInfo()->getFlattenFieldInfoVec(ety);
        vector<FieldInfo>::const_iterator it = infovec.begin();
        vector<FieldInfo>::const_iterator eit = infovec.end();
        for (; it != eit; ++it)
        {
            const FieldInfo& fieldLS = *it;
            if (ls.intersects(LocationSet(fieldLS)))
            {
                hasIntersection = true;
                if (fieldLS.getFlattenElemTy()->isPointerTy())
                    return false;
            }
        }
        assert(hasIntersection && "cannot find field of specified offset");
        return true;
    }
    else
    {
        if (isStaticObj() || isHeap())
        {
            // TODO: Objects which cannot find proper field for a certain offset including
            //       arguments in main(), static objects allocated before main and heap
            //       objects. Right now they're considered to have infinite fields and we
            //       treat each field as pointers conservatively.
            //       Try to model static and heap objects more accurately in the future.
            return false;
        }
        else
        {
            // TODO: Using new memory model (locMM) may create objects with spurious offset
            //       as we simply return new offset by mod operation without checking its
            //       correctness in LocSymTableInfo::getModulusOffset(). So the following
            //       assertion may fail. Try to refine the new memory model.
            //assert(ls.getOffset() == 0 && "cannot get a field from a non-struct type");
            return (hasPtrObj() == false);
        }
    }
}


/*!
 * Set mem object to be field sensitive (up to maximum field limit)
 */
void MemObj::setFieldSensitive()
{
    typeInfo->setMaxFieldOffsetLimit(StInfo::getMaxFieldLimit());
}


/*!
 * Constructor of a memory object
 */
MemObj::MemObj(SymID id, ObjTypeInfo* ti, const Value *val) :
    symId(id), refVal(val), typeInfo(ti)
{
}

/*!
 * Whether it is a black hole object
 */
bool MemObj::isBlackHoleObj() const
{
    return SymbolTableInfo::isBlkObj(getId());
}


/// Get obj type info
const Type* MemObj::getType() const
{
    if (isHeap() == false)
    {
        if(const PointerType* type = SVFUtil::dyn_cast<PointerType>(typeInfo->getType()))
            return type->getElementType();
        else
            return typeInfo->getType();
    }
    else if (getValue() && SVFUtil::isa<Instruction>(getValue()))
        return SVFUtil::getTypeOfHeapAlloc(SVFUtil::cast<Instruction>(getValue()));
    else
        return typeInfo->getType();
}
/*
 * Destroy the fields of the memory object
 */
void MemObj::destroy()
{
    delete typeInfo;
    typeInfo = nullptr;
}

/// Get max field offset limit
Size_t MemObj::getMaxFieldOffsetLimit() const
{
    return typeInfo->getMaxFieldOffsetLimit();
}

/// Return true if its field limit is 0
bool MemObj::isFieldInsensitive() const
{
    return getMaxFieldOffsetLimit() == 0;
}

/// Set the memory object to be field insensitive
void MemObj::setFieldInsensitive()
{
    typeInfo->setMaxFieldOffsetLimit(0);
}

bool MemObj::isFunction() const
{
    return typeInfo->isFunction();
}

bool MemObj::isGlobalObj() const
{
    return typeInfo->isGlobalObj();
}

bool MemObj::isStaticObj() const
{
    return typeInfo->isStaticObj();
}

bool MemObj::isStack() const
{
    return typeInfo->isStack();
}

bool MemObj::isHeap() const
{
    return typeInfo->isHeap();
}

bool MemObj::isStruct() const
{
    return typeInfo->isStruct();
}

bool MemObj::isArray() const
{
    return typeInfo->isArray();
}

bool MemObj::isVarStruct() const
{
    return typeInfo->isVarStruct();
}

bool MemObj::isVarArray() const
{
    return typeInfo->isVarArray();
}

bool MemObj::isConstStruct() const
{
    return typeInfo->isConstStruct();
}

bool MemObj::isConstArray() const
{
    return typeInfo->isConstArray();
}

bool MemObj::isConstant() const
{
    return typeInfo->isConstant();
}

bool MemObj::hasPtrObj() const
{
    return typeInfo->hasPtrObj();
}

bool MemObj::isNonPtrFieldObj(const LocationSet& ls) const
{
    return typeInfo->isNonPtrFieldObj(ls);
}

const std::string MemObj::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "MemObj : " << getId() << SVFUtil::value2String(getValue())<< "\n";
    return rawstr.str();
}

