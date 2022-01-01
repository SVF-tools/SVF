//===- SVFSymbols.cpp -- SVF symbols and variables----------------------//
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
 * SVFSymbols.cpp
 *
 *  Created on: Oct 11, 2013
 *      Author: Yulei Sui
 */

#include "MemoryModel/SymbolTableInfo.h"
#include "MemoryModel/SVFSymbols.h"
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
void ObjSym::setFieldSensitive()
{
    typeInfo->setMaxFieldOffsetLimit(StInfo::getMaxFieldLimit());
}


/*!
 * Constructor of a memory object
 */
ObjSym::ObjSym(SymID id, ObjTypeInfo* ti, const Value *val) :
    SVFVar(id, SYMTYPE::ObjSymbol, val), typeInfo(ti)
{
}

/*!
 * Whether it is a black hole object
 */
bool ObjSym::isBlackHoleObj() const
{
    return SymbolTableInfo::isBlkObj(getId());
}


/// Get obj type info
const Type* ObjSym::getType() const
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
void ObjSym::destroy()
{
    delete typeInfo;
    typeInfo = nullptr;
}

/// Get max field offset limit
Size_t ObjSym::getMaxFieldOffsetLimit() const
{
    return typeInfo->getMaxFieldOffsetLimit();
}

/// Return true if its field limit is 0
bool ObjSym::isFieldInsensitive() const
{
    return getMaxFieldOffsetLimit() == 0;
}

/// Set the memory object to be field insensitive
void ObjSym::setFieldInsensitive()
{
    typeInfo->setMaxFieldOffsetLimit(0);
}

bool ObjSym::isFunction() const
{
    return typeInfo->isFunction();
}

bool ObjSym::isGlobalObj() const
{
    return typeInfo->isGlobalObj();
}

bool ObjSym::isStaticObj() const
{
    return typeInfo->isStaticObj();
}

bool ObjSym::isStack() const
{
    return typeInfo->isStack();
}

bool ObjSym::isHeap() const
{
    return typeInfo->isHeap();
}

bool ObjSym::isStruct() const
{
    return typeInfo->isStruct();
}

bool ObjSym::isArray() const
{
    return typeInfo->isArray();
}

bool ObjSym::isVarStruct() const
{
    return typeInfo->isVarStruct();
}

bool ObjSym::isVarArray() const
{
    return typeInfo->isVarArray();
}

bool ObjSym::isConstStruct() const
{
    return typeInfo->isConstStruct();
}

bool ObjSym::isConstArray() const
{
    return typeInfo->isConstArray();
}

bool ObjSym::isConstant() const
{
    return typeInfo->isConstant();
}

bool ObjSym::hasPtrObj() const
{
    return typeInfo->hasPtrObj();
}

bool ObjSym::isNonPtrFieldObj(const LocationSet& ls) const
{
    return typeInfo->isNonPtrFieldObj(ls);
}

const std::string ValSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ValSym : " << getId() << SVFUtil::value2String(getValue())<< "\n";
    return rawstr.str();
}

const std::string ObjSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ObjSym : " << getId() << SVFUtil::value2String(getValue())<< "\n";
    return rawstr.str();
}

const std::string BlackHoleSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BlackHoleSym : " << getId() << "\n";
    return rawstr.str();
}

const std::string ConstantObjSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "ConstantObjSym : " << getId() << "\n";
    return rawstr.str();
}

const std::string BlkPtrSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "BlkPtrSym : " << getId() << "\n";
    return rawstr.str();
}

const std::string NullPtrSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "NullPtrSym : " << getId() << "\n";
    return rawstr.str();
}

const std::string RetSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "RetSym : " << getId() << " of function: " << SVFUtil::cast<Function>(getValue())->getName() << "\n";
    return rawstr.str();
}

const std::string VarargSym::toString() const{
    std::string str;
    raw_string_ostream rawstr(str);
    rawstr << "VarargSym : " << getId() << " of function: " << SVFUtil::cast<Function>(getValue())->getName() << "\n";
    return rawstr.str();
}


