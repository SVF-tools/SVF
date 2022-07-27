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
#include "SVF-FE/LLVMUtil.h"

using namespace std;
using namespace SVF;
using namespace SVFUtil;
using namespace LLVMUtil;

SymbolTableInfo* SymbolTableInfo::symInfo = nullptr;


ObjTypeInfo::ObjTypeInfo(const Type* t, u32_t max) : type(t), flags(0), maxOffsetLimit(max), elemNum(max)
{
    assert(t && "no type information for this object?");
}


void ObjTypeInfo::resetTypeForHeapStaticObj(const Type* t)
{
    assert((isStaticObj() || isHeap()) && "can only reset the inferred type for heap and static objects!");
    type = t;
}

/// Add field (index and offset) with its corresponding type
void StInfo::addFldWithType(u32_t fldIdx, const Type* type, u32_t elemIdx)
{
    fldIdxVec.push_back(fldIdx);
    elemIdxVec.push_back(elemIdx);
    fldIdx2TypeMap[fldIdx] = type;
}

///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
///  OriginalFieldType of b with field_idx 1 : Struct A
///  FlatternedFieldType of b with field_idx 1 : int
//{@
const Type* StInfo::getOriginalElemType(u32_t fldIdx)
{
    Map<u32_t, const Type*>::const_iterator it = fldIdx2TypeMap.find(fldIdx);
    if(it!=fldIdx2TypeMap.end())
        return it->second;
    return nullptr;
}

SymbolTableInfo::TypeToFieldInfoMap::iterator SymbolTableInfo::getStructInfoIter(const Type *T)
{
    assert(T);
    TypeToFieldInfoMap::iterator it = typeToFieldInfo.find(T);
    if (it != typeToFieldInfo.end())
        return it;
    else
    {
        collectTypeInfo(T);
        return typeToFieldInfo.find(T);
    }
}

/*
 * Initial the memory object here (for a dummy object)
 */
ObjTypeInfo* SymbolTableInfo::createObjTypeInfo(const Type* type)
{
    ObjTypeInfo* typeInfo = new ObjTypeInfo(type, Options::MaxFieldLimit);
    if(type && type->isPointerTy())
    {
        typeInfo->setFlag(ObjTypeInfo::HEAP_OBJ);
        typeInfo->setFlag(ObjTypeInfo::HASPTR_OBJ);
    }
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
    u64_t totalElemNum = ty->getNumElements();
    const Type* elemTy = ty->getElementType();
    while (const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(elemTy))
    {
        totalElemNum *= aty->getNumElements();
        elemTy = aty->getElementType();
    }

    StInfo* stinfo = new StInfo(totalElemNum);
    typeToFieldInfo[ty] = stinfo;

    /// array without any element (this is not true in C/C++ arrays) we assume there is an empty dummy element
    if(totalElemNum==0)
    {
        stinfo->addFldWithType(0, elemTy, 0);
        stinfo->setNumOfFieldsAndElems(1, 1);
        stinfo->getFlattenFieldTypes().push_back(elemTy);
        stinfo->getFlattenElementTypes().push_back(elemTy);
        return;
    }

    /// Array's flatten field infor is the same as its element's
    /// flatten infor.
    StInfo* elemStInfo = getStructInfo(elemTy);
    u32_t nfE = elemStInfo->getNumOfFlattenFields();
    for (u32_t j = 0; j < nfE; j++)
    {
        const Type* fieldTy = elemStInfo->getFlattenFieldTypes()[j];
        stinfo->getFlattenFieldTypes().push_back(fieldTy);
    }

    /// Flatten arrays, map each array element index `i` to flattened index `(i * nfE * totalElemNum)/outArrayElemNum`
    /// nfE>1 if the array element is a struct with more than one field.
    u32_t outArrayElemNum = ty->getNumElements();
    for(u32_t i = 0; i < outArrayElemNum; i++)
        stinfo->addFldWithType(0, elemTy, (i * nfE * totalElemNum)/outArrayElemNum);

    for(u32_t i = 0; i < totalElemNum; i++)
    {
        for(u32_t j = 0; j < nfE; j++)
        {
            stinfo->getFlattenElementTypes().push_back(elemStInfo->getFlattenFieldTypes()[j]);
        }
    }

    assert(stinfo->getFlattenElementTypes().size() == nfE * totalElemNum && "typeForArray size incorrect!!!");
    stinfo->setNumOfFieldsAndElems(nfE, nfE * totalElemNum);
}


/*!
 * Fill in struct_info for T.
 * Given a Struct type, we recursively extend and record its fields and types.
 */
void SymbolTableInfo::collectStructInfo(const StructType *sty)
{
    /// The struct info should not be processed before
    StInfo* stinfo = new StInfo(1);
    typeToFieldInfo[sty] = stinfo;

    // Number of fields after flattening the struct
    u32_t nf = 0;
    // The offset when considering array stride info
    u32_t strideOffset = 0;
    for (StructType::element_iterator it = sty->element_begin(), ie =
                sty->element_end(); it != ie; ++it)
    {
        const Type *et = *it;
        /// offset with int_32 (s32_t) is large enough and will not cause overflow
        stinfo->addFldWithType(nf, et, strideOffset);

        if (SVFUtil::isa<StructType>(et) || SVFUtil::isa<ArrayType>(et))
        {
            StInfo * subStinfo = getStructInfo(et);
            u32_t nfE = subStinfo->getNumOfFlattenFields();
            //Copy ST's info, whose element 0 is the size of ST itself.
            for (u32_t j = 0; j < nfE; j++)
            {
                const Type* elemTy = subStinfo->getFlattenFieldTypes()[j];
                stinfo->getFlattenFieldTypes().push_back(elemTy);
            }
            nf += nfE;
            strideOffset += nfE * subStinfo->getStride();
            for(u32_t tpi = 0; tpi < subStinfo->getStride(); tpi++)
            {
                for(u32_t tpj = 0; tpj < nfE; tpj++)
                {
                    stinfo->getFlattenElementTypes().push_back(subStinfo->getFlattenFieldTypes()[tpj]);
                }
            }
        }
        else     //simple type
        {
            nf += 1;
            strideOffset += 1;
            stinfo->getFlattenFieldTypes().push_back(et);
            stinfo->getFlattenElementTypes().push_back(et);
        }
    }

    assert(stinfo->getFlattenElementTypes().size() == strideOffset && "typeForStruct size incorrect!");
    stinfo->setNumOfFieldsAndElems(nf,strideOffset);

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
    StInfo* stinfo = new StInfo(1);
    typeToFieldInfo[ty] = stinfo;

    /// Only one field
    stinfo->addFldWithType(0, ty, 0);

    stinfo->getFlattenFieldTypes().push_back(ty);
    stinfo->getFlattenElementTypes().push_back(ty);
    stinfo->setNumOfFieldsAndElems(1,1);
}


/*!
 * Get modulus offset given the type information
 */
LocationSet SymbolTableInfo::getModulusOffset(const MemObj* obj, const LocationSet& ls)
{

    /// if the offset is negative, it's possible that we're looking for an obj node out of range
    /// of current struct. Make the offset positive so we can still get a node within current
    /// struct to represent this obj.

    s32_t offset = ls.accumulateConstantFieldIdx();
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

    if(mod)
    {
        delete mod;
        mod = nullptr;
    }
}

/*!
 * Check whether this value is null pointer
 */
bool SymbolTableInfo::isNullPtrSym(const Value *val)
{
    return symInfo->nullPtrSyms.find(val)!=symInfo->nullPtrSyms.end();
}

/*!
 * Check whether this value is blackhole object
 */
bool SymbolTableInfo::isBlackholeSym(const Value *val)
{
    return symInfo->blackholeSyms.find(val)!=symInfo->blackholeSyms.end();
}



MemObj* SymbolTableInfo::createBlkObj(SymID symId)
{
    assert(isBlkObj(symId));
    assert(objMap.find(symId)==objMap.end());
    MemObj* obj = new MemObj(symId, createObjTypeInfo(IntegerType::get(LLVMModuleSet::getLLVMModuleSet()->getContext(), 32)));
    objMap[symId] = obj;
    return obj;
}

MemObj* SymbolTableInfo::createConstantObj(SymID symId)
{
    assert(isConstantObj(symId));
    assert(objMap.find(symId)==objMap.end());
    MemObj* obj = new MemObj(symId, createObjTypeInfo(IntegerType::get(LLVMModuleSet::getLLVMModuleSet()->getContext(), 32)));
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

/// Number of flattenned elements of an array or struct
u32_t SymbolTableInfo::getNumOfFlattenElements(const Type *T)
{
    if(Options::ModelArrays)
        return getStructInfoIter(T)->second->getNumOfFlattenElements();
    else
        return getStructInfoIter(T)->second->getNumOfFlattenFields();
}

/// Flatterned offset information of a struct or an array including its array fields
u32_t SymbolTableInfo::getFlattenedElemIdx(const Type *T, u32_t origId)
{
    if(Options::ModelArrays)
    {
        std::vector<u32_t>& so = getStructInfoIter(T)->second->getFlattenedElemIdxVec();
        assert ((unsigned)origId < so.size() && !so.empty() && "element index out of bounds, can't get flattened index!");
        return so[origId];
    }
    else
    {
        if(SVFUtil::isa<StructType>(T))
        {
            std::vector<u32_t>& so = getStructInfoIter(T)->second->getFlattenedFieldIdxVec();
            assert ((unsigned)origId < so.size() && !so.empty() && "Struct index out of bounds, can't get flattened index!");
            return so[origId];
        }
        else
        {
            /// When Options::ModelArrays is disabled, any element index Array is modeled as the base
            assert(SVFUtil::isa<ArrayType>(T) && "Only accept struct or array type if Options::ModelArrays is disabled!");
            return 0;
        }
    }
}

const Type* SymbolTableInfo::getOriginalElemType(const Type* baseType, u32_t origId)
{
    return getStructInfoIter(baseType)->second->getOriginalElemType(origId);
}

/// Return the type of a flattened element given a flattened index
const Type* SymbolTableInfo::getFlatternedElemType(const Type* baseType, u32_t flatten_idx)
{
    if(Options::ModelArrays)
    {
        const std::vector<const Type*>& so = getStructInfoIter(baseType)->second->getFlattenElementTypes();
        assert (flatten_idx < so.size() && !so.empty() && "element index out of bounds or struct opaque type, can't get element type!");
        return so[flatten_idx];
    }
    else
    {
        const std::vector<const Type*>& so = getStructInfoIter(baseType)->second->getFlattenFieldTypes();
        assert (flatten_idx < so.size() && !so.empty() && "element index out of bounds or struct opaque type, can't get element type!");
        return so[flatten_idx];
    }
}


const std::vector<const Type*>& SymbolTableInfo::getFlattenFieldTypes(const StructType *T)
{
    return getStructInfoIter(T)->second->getFlattenFieldTypes();
}

/*
 * Print out the composite type information
 */
void SymbolTableInfo::printFlattenFields(const Type* type)
{

    if(const ArrayType *at = SVFUtil::dyn_cast<ArrayType> (type))
    {
        outs() <<"  {Type: ";
        outs() << type2String(at);
        outs() << "}\n";
        outs() << "\tarray type ";
        outs() << "\t [element size = " << getNumOfFlattenElements(at) << "]\n";
        outs() << "\n";
    }

    else if(const StructType *st = SVFUtil::dyn_cast<StructType> (type))
    {
        outs() <<"  {Type: ";
        outs() << type2String(st);
        outs() << "}\n";
        std::vector<const Type*>& finfo = getStructInfo(st)->getFlattenFieldTypes();
        int field_idx = 0;
        for(std::vector<const Type*>::const_iterator it = finfo.begin(), eit = finfo.end();
                it!=eit; ++it, field_idx++)
        {
            outs() << " \tField_idx = " << field_idx;
            outs() << ", field type: ";
            outs() << type2String(*it);
            outs() << "\n";
        }
        outs() << "\n";
    }

    else if (const PointerType* pt= SVFUtil::dyn_cast<PointerType> (type))
    {
        u32_t eSize = getNumOfFlattenElements(getPtrElementType(pt));
        outs() << "  {Type: ";
        outs() << type2String(pt);
        outs() << "}\n";
        outs() <<"\t [target size = " << eSize << "]\n";
        outs() << "\n";
    }

    else if ( const FunctionType* fu= SVFUtil::dyn_cast<FunctionType> (type))
    {
        outs() << "  {Type: ";
        outs() << type2String(fu->getReturnType());
        outs() << "(Function)}\n\n";
    }

    else
    {
        assert(type->isSingleValueType() && "not a single value type, then what else!!");
        /// All rest types are scalar type?
        u32_t eSize = getNumOfFlattenElements(type);
        outs() <<"  {Type: ";
        outs() << type2String(type);
        outs() << "}\n";
        outs() <<"\t [object size = " << eSize << "]\n";
        outs() << "\n";
    }
}

std::string SymbolTableInfo::toString(SYMTYPE symtype)
{
    switch (symtype)
    {
    case SYMTYPE::BlackHole:
    {
        return "BlackHole";
    }
    case SYMTYPE::ConstantObj:
    {
        return "ConstantObj";
    }
    case SYMTYPE::BlkPtr:
    {
        return "BlkPtr";
    }
    case SYMTYPE::NullPtr:
    {
        return "NullPtr";
    }
    case SYMTYPE::ValSymbol:
    {
        return "ValSym";
    }
    case SYMTYPE::ObjSymbol:
    {
        return "ObjSym";
    }
    case SYMTYPE::RetSymbol:
    {
        return "RetSym";
    }
    case SYMTYPE::VarargSymbol:
    {
        return "VarargSym";
    }
    default:
    {
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
    for (auto iter : idmap)
    {
        outs() << iter.first << " " << value2String(iter.second) << "\n";
    }
    outs() << "}\n";
}

/*!
 * Whether a location set is a pointer type or not
 */
bool ObjTypeInfo::isNonPtrFieldObj(const LocationSet& ls)
{
    if (hasPtrObj() == false)
        return true;

    const Type* ety = getType();

    if ((SVFUtil::isa<StructType>(ety) && !SVFUtil::cast<StructType>(ety)->isOpaque()) || SVFUtil::isa<ArrayType>(ety))
    {
        const Type* elemTy = SymbolTableInfo::SymbolInfo()->getFlatternedElemType(ety, ls.accumulateConstantFieldIdx());
        if(elemTy->isPointerTy())
            return false;
        else
            return true;
    }
    else
    {
        return (hasPtrObj() == false);
    }
}

/*!
 * Set mem object to be field sensitive (up to maximum field limit)
 */
void MemObj::setFieldSensitive()
{
    typeInfo->setMaxFieldOffsetLimit(typeInfo->getNumOfElements());
}


/*!
 * Constructor of a memory object
 */
MemObj::MemObj(SymID id, ObjTypeInfo* ti, const Value *val) :
    typeInfo(ti), refVal(val), symId(id)
{
}

/*!
 * Whether it is a black hole object
 */
bool MemObj::isBlackHoleObj() const
{
    return SymbolTableInfo::isBlkObj(getId());
}

/// Get the number of elements of this object
u32_t MemObj::getNumOfElements() const
{
    return typeInfo->getNumOfElements();
}

/// Set the number of elements of this object
void MemObj::setNumOfElements(u32_t num)
{
    return typeInfo->setNumOfElements(num);
}

/// Get obj type info
const Type* MemObj::getType() const
{
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
u32_t MemObj::getMaxFieldOffsetLimit() const
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

bool MemObj::isConstantStruct() const
{
    return typeInfo->isConstantStruct();
}

bool MemObj::isConstantArray() const
{
    return typeInfo->isConstantArray();
}

bool MemObj::isConstDataOrConstGlobal() const
{
    return typeInfo->isConstDataOrConstGlobal();
}

bool MemObj::isConstantData() const
{
    return typeInfo->isConstantData();
}

bool MemObj::hasPtrObj() const
{
    return typeInfo->hasPtrObj();
}

bool MemObj::isNonPtrFieldObj(const LocationSet& ls) const
{
    return typeInfo->isNonPtrFieldObj(ls);
}

const std::string MemObj::toString() const
{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "MemObj : " << getId() << SVFUtil::value2String(getValue())<< "\n";
    return rawstr.str();
}

/// Get different kinds of syms
//@{
SymID SymbolTableInfo::getValSym(const Value *val)
{

    if(SymbolTableInfo::isNullPtrSym(val))
        return nullPtrSymID();
    else if (SymbolTableInfo::isBlackholeSym(val))
        return blkPtrSymID();
    else
    {
        ValueToIDMapTy::const_iterator iter =  valSymMap.find(val);
        assert(iter!=valSymMap.end() &&"value sym not found");
        return iter->second;
    }
}

bool SymbolTableInfo::hasValSym(const Value* val)
{
    if (SymbolTableInfo::isNullPtrSym(val) || SymbolTableInfo::isBlackholeSym(val))
        return true;
    else
        return (valSymMap.find(val) != valSymMap.end());
}

