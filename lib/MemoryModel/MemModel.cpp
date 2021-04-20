//===- MemModel.cpp -- Memory model for pointer analysis----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * MemModel.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/SymbolTableInfo.h"
#include "MemoryModel/MemModel.h"
#include "Util/SVFModule.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/CPPUtil.h"
#include "SVF-FE/BreakConstantExpr.h"
#include "SVF-FE/GEPTypeBridgeIterator.h" // include bridge_gep_iterator

using namespace std;
using namespace SVF;
using namespace SVFUtil;

u32_t StInfo::maxFieldLimit = 0;

/*!
 * Analyse types of all flattened fields of this object
 */
void ObjTypeInfo::analyzeGlobalStackObjType(const Value* val)
{

    const PointerType * refty = SVFUtil::dyn_cast<PointerType>(val->getType());
    assert(SVFUtil::isa<PointerType>(refty) && "this value should be a pointer type!");
    Type* elemTy = refty->getElementType();
    bool isPtrObj = false;
    // Find the inter nested array element
    while (const ArrayType *AT= SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        elemTy = AT->getElementType();
        if(elemTy->isPointerTy())
            isPtrObj = true;
        if(SVFUtil::isa<GlobalVariable>(val) && SVFUtil::cast<GlobalVariable>(val)->hasInitializer()
                && SVFUtil::isa<ConstantArray>(SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
        {
            setFlag(CONST_ARRAY_OBJ);
        }
        else
            setFlag(VAR_ARRAY_OBJ);
    }
    if (const StructType *ST= SVFUtil::dyn_cast<StructType>(elemTy))
    {
        const std::vector<FieldInfo>& flattenFields = SymbolTableInfo::SymbolInfo()->getFlattenFieldInfoVec(ST);
        for(std::vector<FieldInfo>::const_iterator it = flattenFields.begin(), eit = flattenFields.end();
                it!=eit; ++it)
        {
            if((*it).getFlattenElemTy()->isPointerTy())
                isPtrObj = true;
        }
        if(SVFUtil::isa<GlobalVariable>(val) && SVFUtil::cast<GlobalVariable>(val)->hasInitializer()
                && SVFUtil::isa<ConstantStruct>(SVFUtil::cast<GlobalVariable>(val)->getInitializer()))
            setFlag(CONST_STRUCT_OBJ);
        else
            setFlag(VAR_STRUCT_OBJ);
    }
    else if (elemTy->isPointerTy())
    {
        isPtrObj = true;
    }

    if(isPtrObj)
        setFlag(HASPTR_OBJ);
}

/*!
 * Analyse types of heap and static objects
 */
void ObjTypeInfo::analyzeHeapStaticObjType(const Value*)
{
    // TODO: Heap and static objects are considered as pointers right now.
    //       Refine this function to get more details about heap and static objects.
    setFlag(HASPTR_OBJ);
}

/*!
 * Return size of this Object
 */
u32_t ObjTypeInfo::getObjSize(const Value* val)
{

    Type* ety  = SVFUtil::cast<PointerType>(val->getType())->getElementType();
    u32_t numOfFields = 1;
    if (SVFUtil::isa<StructType>(ety) || SVFUtil::isa<ArrayType>(ety))
    {
        numOfFields = SymbolTableInfo::SymbolInfo()->getFlattenFieldInfoVec(ety).size();
    }
    return numOfFields;
}

/*!
 * Initialize the type info of an object
 */
void ObjTypeInfo::init(const Value* val)
{

    Size_t objSize = 1;
    // Global variable
    if (SVFUtil::isa<Function>(val))
    {
        setFlag(FUNCTION_OBJ);
        analyzeGlobalStackObjType(val);
        objSize = getObjSize(val);
    }
    else if(SVFUtil::isa<AllocaInst>(val))
    {
        setFlag(STACK_OBJ);
        analyzeGlobalStackObjType(val);
        objSize = getObjSize(val);
    }
    else if(SVFUtil::isa<GlobalVariable>(val))
    {
        setFlag(GLOBVAR_OBJ);
        if(SymbolTableInfo::SymbolInfo()->isConstantObjSym(val))
            setFlag(CONST_OBJ);
        analyzeGlobalStackObjType(val);
        objSize = getObjSize(val);
    }
    else if (SVFUtil::isa<Instruction>(val) && isHeapAllocExtCall(SVFUtil::cast<Instruction>(val)))
    {
        setFlag(HEAP_OBJ);
        analyzeHeapStaticObjType(val);
        // Heap object, label its field as infinite here
        objSize = -1;
    }
    else if (SVFUtil::isa<Instruction>(val) && isStaticExtCall(SVFUtil::cast<Instruction>(val)))
    {
        setFlag(STATIC_OBJ);
        analyzeHeapStaticObjType(val);
        // static object allocated before main, label its field as infinite here
        objSize = -1;
    }
    else if(ArgInProgEntryFunction(val))
    {
        setFlag(STATIC_OBJ);
        analyzeHeapStaticObjType(val);
        // user input data, label its field as infinite here
        objSize = -1;
    }
    else
        assert("what other object do we have??");

    // Reset maxOffsetLimit if it is over the total fieldNum of this object
    if(objSize > 0 && maxOffsetLimit > objSize)
        maxOffsetLimit = objSize;

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
/*
 * Initial the memory object here
 */
void MemObj::init(const Type* type)
{
    typeInfo = new ObjTypeInfo(StInfo::getMaxFieldLimit(),type);
    typeInfo->setFlag(ObjTypeInfo::HEAP_OBJ);
    typeInfo->setFlag(ObjTypeInfo::HASPTR_OBJ);
}

/*!
 * Constructor of a memory object
 */
MemObj::MemObj(const Value *val, SymID id) :
    refVal(val), GSymID(id), typeInfo(nullptr)
{
    init(val);
}

/*!
 * Constructor of a memory object
 */
MemObj::MemObj(SymID id, const Type* type) :
    refVal(nullptr), GSymID(id), typeInfo(nullptr)
{
    init(type);
}

/*!
 * Whether it is a black hole object
 */
bool MemObj::isBlackHoleObj() const
{
    return SymbolTableInfo::isBlkObj(GSymID);
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
    else if (refVal && SVFUtil::isa<Instruction>(refVal))
        return SVFUtil::getTypeOfHeapAlloc(SVFUtil::cast<Instruction>(refVal));
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


/*!
 * Compute gep offset
 */
bool LocSymTableInfo::computeGepOffset(const User *V, LocationSet& ls)
{

    assert(V);
    int baseIndex = -1;
    int index = 0;
    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi, ++index)
    {
        if(SVFUtil::isa<ConstantInt>(gi.getOperand()) == false)
            baseIndex = index;
    }

    index = 0;
    for (bridge_gep_iterator gi = bridge_gep_begin(*V), ge = bridge_gep_end(*V);
            gi != ge; ++gi, ++index)
    {

        if (index <= baseIndex)
        {
            /// variant offset
            // Handling pointer types
            if (const PointerType* pty = SVFUtil::dyn_cast<PointerType>(*gi))
            {
                const Type* et = pty->getElementType();
                Size_t sz = getTypeSizeInBytes(et);

                Size_t num = 1;
                if (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(et))
                    num = aty->getNumElements();
                else
                    num = StInfo::getMaxFieldLimit();

                ls.addElemNumStridePair(std::make_pair(num, sz));
            }
            // Calculate the size of the array element
            else if(const ArrayType* at = SVFUtil::dyn_cast<ArrayType>(*gi))
            {
                const Type* et = at->getElementType();
                Size_t sz = getTypeSizeInBytes(et);
                Size_t num = at->getNumElements();
                ls.addElemNumStridePair(std::make_pair(num, sz));
            }
            else
                assert(false && "what other types?");
        }
        // constant offset
        else
        {
            assert(SVFUtil::isa<ConstantInt>(gi.getOperand()) && "expecting a constant");

            ConstantInt *op = SVFUtil::cast<ConstantInt>(gi.getOperand());

            //The actual index
            Size_t idx = op->getSExtValue();

            // Handling pointer types
            // These GEP instructions are simply making address computations from the base pointer address
            // e.g. idx1 = (char*) &MyVar + 4,  at this case gep only one offset index (idx)
            if (const PointerType* pty = SVFUtil::dyn_cast<PointerType>(*gi))
            {
                const Type* et = pty->getElementType();
                Size_t sz = getTypeSizeInBytes(et);
                ls.setByteOffset(ls.getByteOffset() + idx * sz);
            }
            // Calculate the size of the array element
            else if(const ArrayType* at = SVFUtil::dyn_cast<ArrayType>(*gi))
            {
                const Type* et = at->getElementType();
                Size_t sz = getTypeSizeInBytes(et);
                ls.setByteOffset(ls.getByteOffset() + idx * sz);
            }
            // Handling struct here
            else if (const StructType *ST = SVFUtil::dyn_cast<StructType>(*gi))
            {
                assert(op && "non-const struct index in GEP");
                const vector<u32_t> &so = SymbolTableInfo::SymbolInfo()->getFattenFieldOffsetVec(ST);
                if ((unsigned)idx >= so.size())
                {
                    outs() << "!! Struct index out of bounds" << idx << "\n";
                    assert(0);
                }
                //add the translated offset
                ls.setByteOffset(ls.getByteOffset() + so[idx]);
            }
            else
                assert(false && "what other types?");
        }
    }
    return true;
}

/*!
 * Collect array information
 */
void LocSymTableInfo::collectArrayInfo(const llvm::ArrayType*)
{
    /*
    StInfo *stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    /// If this is an array type, calculate the outmost array
    /// information and append them to the inner elements' type
    /// information later.
    u64_t out_num = ty->getNumElements();
    const Type* elemTy = ty->getElementType();
    u32_t out_stride = getTypeSizeInBytes(elemTy);

    /// Array itself only has one field which is the inner most element
    stinfo->addOffsetWithType(0, elemTy);

    while (const ArrayType* aty = dyn_cast<ArrayType>(elemTy)) {
        out_num *= aty->getNumElements();
        elemTy = aty->getElementType();
        out_stride = getTypeSizeInBytes(elemTy);
    }

    /// Array's flatten field infor is the same as its element's
    /// flatten infor with an additional slot for array's element
    /// number and stride pair.
    StInfo* elemStInfo = getStructInfo(elemTy);
    u32_t nfE = elemStInfo->getFlattenFieldInfoVec().size();
    for (u32_t j = 0; j < nfE; j++) {
        u32_t off = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
        const Type* fieldTy = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
        FieldInfo::ElemNumStridePairVec pair = elemStInfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
        /// append the additional number
        pair.push_back(std::make_pair(out_num, out_stride));
        FieldInfo field(off, fieldTy, pair);
        stinfo->getFlattenFieldInfoVec().push_back(field);
    }
    */
}


/*
 * Recursively collect the memory layout information for a struct type
 */
void LocSymTableInfo::collectStructInfo(const StructType*)
{
    /*
    StInfo *stinfo = new StInfo();
    typeToFieldInfo[ty] = stinfo;

    const StructLayout *stTySL = getDataLayout(getModule().getMainLLVMModule())->getStructLayout( const_cast<StructType *>(ty) );

    u32_t field_idx = 0;
    for (StructType::element_iterator it = ty->element_begin(), ie =
                ty->element_end(); it != ie; ++it, ++field_idx) {
        const Type *et = *it;

        // The offset is where this element will be placed in the struct.
        // This offset is computed after alignment with the current struct
        u64_t eOffsetInBytes = stTySL->getElementOffset(field_idx);

        //The offset is where this element will be placed in the exp. struct.
        /// FIXME: As the layout size is uint_64, here we assume
        /// offset with uint_32 (Size_t) is large enough and will not cause overflow
        stinfo->addOffsetWithType(static_cast<u32_t>(eOffsetInBytes), et);

        StInfo* fieldStinfo = getStructInfo(et);
        u32_t nfE = fieldStinfo->getFlattenFieldInfoVec().size();
        //Copy ST's info, whose element 0 is the size of ST itself.
        for (u32_t j = 0; j < nfE; j++) {
            u32_t oft = eOffsetInBytes + fieldStinfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
            const Type* elemTy = fieldStinfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
            FieldInfo::ElemNumStridePairVec pair = fieldStinfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
            pair.push_back(std::make_pair(1, 0));
            FieldInfo newField(oft, elemTy, pair);
            stinfo->getFlattenFieldInfoVec().push_back(newField);
        }
    }

    //	verifyStructSize(stinfo,stTySL->getSizeInBytes());

    //Record the size of the complete struct and update max_struct.
    if (stTySL->getSizeInBytes() > maxStSize) {
        maxStruct = ty;
        maxStSize = stTySL->getSizeInBytes();
    }
    */
}


/*!
 * Given LocationSet from a Gep Instruction, return a new LocationSet which matches
 * the field information of this ObjTypeInfo by considering memory layout
 */
LocationSet LocSymTableInfo::getModulusOffset(const MemObj* obj, const LocationSet& ls)
{
    const Type* ety = obj->getType();

    if (SVFUtil::isa<StructType>(ety) || SVFUtil::isa<ArrayType>(ety))
    {
        /// Find an appropriate field for this LocationSet
        const std::vector<FieldInfo>& infovec = SymbolTableInfo::SymbolInfo()->getFlattenFieldInfoVec(ety);
        std::vector<FieldInfo>::const_iterator it = infovec.begin();
        std::vector<FieldInfo>::const_iterator eit = infovec.end();
        for (; it != eit; ++it)
        {
            const FieldInfo& fieldLS = *it;
            LocationSet rhsLS(fieldLS);
            LocationSet::LSRelation result = LocationSet::checkRelation(ls, rhsLS);
            if (result == LocationSet::Same ||
                    result == LocationSet::Superset ||
                    result == LocationSet::Subset)
                return ls;
            else if (result == LocationSet::Overlap)
            {
                // TODO:
                return ls;
            }
            else if (result == LocationSet::NonOverlap)
            {
                continue;
            }

            assert(false && "cannot find an appropriate field for specified LocationSet");
            return ls;
        }
    }
    else
    {
        if (obj->isStaticObj() || obj->isHeap())
        {
            // TODO: Objects which cannot find proper field for a certain offset including
            //       arguments in main(), static objects allocated before main and heap
            //       objects. Right now they're considered to have infinite fields. So we
            //       just return the location set without modifying it.
            return ls;
        }
        else
        {
            // TODO: Using new memory model (locMM) may create objects with spurious offset
            //       as we simply return new offset by mod operation without checking its
            //       correctness in LocSymTableInfo::getModulusOffset(). So the following
            //       assertion may fail. Try to refine the new memory model.
            assert(ls.isConstantOffset() && "expecting a constant location set");
            return ls;
        }
    }

    /// This location set represent one object
    if (ls.isConstantOffset())
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
    }
    /// This location set represents multiple objects
    else
    {

    }

    return ls;
}

/*!
 * Verify struct size
 */
void LocSymTableInfo::verifyStructSize(StInfo *stinfo, u32_t structSize)
{

    u32_t lastOff = stinfo->getFlattenFieldInfoVec().back().getFlattenByteOffset();
    u32_t strideSize = 0;
    FieldInfo::ElemNumStridePairVec::const_iterator pit = stinfo->getFlattenFieldInfoVec().back().elemStridePairBegin();
    FieldInfo::ElemNumStridePairVec::const_iterator epit = stinfo->getFlattenFieldInfoVec().back().elemStridePairEnd();
    for(; pit!=epit; ++pit)
        strideSize += pit->first * pit->second;

    u32_t lastSize = getTypeSizeInBytes(stinfo->getFlattenFieldInfoVec().back().getFlattenElemTy());
    /// Please note this verify may not be complete as different machine has different alignment mechanism
    assert((structSize == lastOff + strideSize + lastSize) && "struct size not consistent");

}

/*!
 * Get the size of this object
 */
u32_t LocObjTypeInfo::getObjSize(const Value* val)
{

    Type* ety  = SVFUtil::cast<PointerType>(val->getType())->getElementType();
    return LocSymTableInfo::SymbolInfo()->getTypeSizeInBytes(ety);
}
