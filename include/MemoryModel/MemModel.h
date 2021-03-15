//===- MemModel.h -- Memory model for pointer analysis------------------------//
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
 * MemModel.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef OBJECTANDSYMBOL_H_
#define OBJECTANDSYMBOL_H_

#include "MemoryModel/LocationSet.h"
#include "Util/SVFModule.h"

namespace SVF
{

/// Symbol types
enum SYMTYPE
{
    BlackHole,
    ConstantObj,
    BlkPtr,
    NullPtr,
    ValSym,
    ObjSym,
    RetSym,
    VarargSym
};

/*!
 * Struct information
 */
class StInfo
{

private:
    /// flattened field indices of a struct
    std::vector<u32_t> fldIdxVec;
    /// flattened field offsets of of a struct
    std::vector<u32_t> foffset;
    /// Types of all fields of a struct
    Map<u32_t, const llvm::Type*> fldIdx2TypeMap;
    /// Types of all fields of a struct
    Map<u32_t, const llvm::Type*> offset2TypeMap;
    /// All field infos after flattening a struct
    std::vector<FieldInfo> finfo;

    /// Max field limit
    static u32_t maxFieldLimit;

public:
    /// Constructor
    StInfo()
    {
    }
    /// Destructor
    ~StInfo()
    {
    }

    static inline void setMaxFieldLimit(u32_t limit)
    {
        maxFieldLimit = limit;
    }

    static inline u32_t getMaxFieldLimit()
    {
        return maxFieldLimit;
    }

    /// Get method for fields of a struct
    //{@
    inline const llvm::Type* getFieldTypeWithFldIdx(u32_t fldIdx)
    {
        return fldIdx2TypeMap[fldIdx];
    }
    inline const llvm::Type* getFieldTypeWithByteOffset(u32_t offset)
    {
        return offset2TypeMap[offset];
    }
    inline std::vector<u32_t>& getFieldIdxVec()
    {
        return fldIdxVec;
    }
    inline std::vector<u32_t>& getFieldOffsetVec()
    {
        return foffset;
    }
    inline std::vector<FieldInfo>& getFlattenFieldInfoVec()
    {
        return finfo;
    }
    //@}

    /// Add field (index and offset) with its corresponding type
    inline void addFldWithType(u32_t fldIdx, u32_t offset, const llvm::Type* type)
    {
        fldIdxVec.push_back(fldIdx);
        foffset.push_back(offset);
        fldIdx2TypeMap[fldIdx] = type;
        offset2TypeMap[offset] = type;
    }
};

/*!
 * Type Info of an abstract memory object
 */
class ObjTypeInfo
{

public:
    typedef enum
    {
        FUNCTION_OBJ = 0x1,  // object is a function
        GLOBVAR_OBJ = 0x2,  // object is a global variable
        STATIC_OBJ = 0x4,  // object is a static variable allocated before main
        STACK_OBJ = 0x8,  // object is a stack variable
        HEAP_OBJ = 0x10,  // object is a heap variable
        VAR_STRUCT_OBJ = 0x20,  // object contains struct
        VAR_ARRAY_OBJ = 0x40,  // object contains array
        CONST_STRUCT_OBJ = 0x80,  // constant struct
        CONST_ARRAY_OBJ = 0x100,  // constant array
        CONST_OBJ = 0x200,  // constant object str e.g.
        HASPTR_OBJ = 0x400		// non pointer object including compound type have field that is a pointer type
    } MEMTYPE;

private:
    /// LLVM type
    const Type* type;
    /// Type flags
    Size_t flags;
    /// Max offset for flexible field sensitive analysis
    /// maximum number of field object can be created
    /// minimum number is 0 (field insensitive analysis)
    u32_t maxOffsetLimit;
public:

    /// Constructors
    ObjTypeInfo(const Value*, const Type* t, u32_t max) :
        type(t), flags(0), maxOffsetLimit(max)
    {
    }
    /// Constructor
    ObjTypeInfo(u32_t max, const Type* t) : type(t), flags(0), maxOffsetLimit(max)
    {

    }
    /// Destructor
    virtual ~ObjTypeInfo()
    {

    }
    /// Initialize the object type
    void init(const Value* value);

    /// Get the size of this object,
    /// derived classes can override this to get more precise object size
    virtual u32_t getObjSize(const Value* val);

    /// Analyse types of gobal and stack objects
    void analyzeGlobalStackObjType(const Value* val);

    /// Analyse types of heap and static objects
    void analyzeHeapStaticObjType(const Value* val);

    /// Get LLVM type
    inline const Type* getType() const
    {
        return type;
    }

    /// Get max field offset limit
    inline u32_t getMaxFieldOffsetLimit()
    {
        return maxOffsetLimit;
    }

    /// Get max field offset limit
    inline void setMaxFieldOffsetLimit(u32_t limit)
    {
        maxOffsetLimit = limit;
    }

    /// Flag for this object type
    //@{
    inline void setFlag(MEMTYPE mask)
    {
        flags |= mask;
    }
    inline bool hasFlag(MEMTYPE mask)
    {
        return (flags & mask) == mask;
    }
    //@}

    /// Object attributes
    //@{
    inline bool isFunction()
    {
        return hasFlag(FUNCTION_OBJ);
    }
    inline bool isGlobalObj()
    {
        return hasFlag(GLOBVAR_OBJ);
    }
    inline bool isStaticObj()
    {
        return hasFlag(STATIC_OBJ);
    }
    inline bool isStack()
    {
        return hasFlag(STACK_OBJ);
    }
    inline bool isHeap()
    {
        return hasFlag(HEAP_OBJ);
    }
    //@}

    /// Object attributes (noted that an object can be a nested compound types)
    /// e.g. both isStruct and isArray can return true
    //@{
    inline bool isVarStruct()
    {
        return hasFlag(VAR_STRUCT_OBJ);
    }
    inline bool isConstStruct()
    {
        return hasFlag(CONST_STRUCT_OBJ);
    }
    inline bool isStruct()
    {
        return hasFlag(VAR_STRUCT_OBJ) || hasFlag(CONST_STRUCT_OBJ);
    }
    inline bool isVarArray()
    {
        return hasFlag(VAR_ARRAY_OBJ);
    }
    inline bool isConstArray()
    {
        return  hasFlag(CONST_ARRAY_OBJ);
    }
    inline bool isArray()
    {
        return hasFlag(VAR_ARRAY_OBJ) || hasFlag(CONST_ARRAY_OBJ);
    }
    inline bool isConstant()
    {
        return hasFlag(CONST_OBJ);
    }
    inline bool hasPtrObj()
    {
        return hasFlag(HASPTR_OBJ);
    }
    virtual bool isNonPtrFieldObj(const LocationSet& ls);
    //@}
};

/*!
 * Memory Object
 */
class MemObj
{

private:
    /// The unique pointer value refer this object
    const Value *refVal;
    /// The unique id in the graph
    SymID GSymID;
    /// Type information of this object
    ObjTypeInfo* typeInfo;

public:

    /// Constructor
    MemObj(const Value *val, SymID id);

    /// Constructor for black hole and constant obj
    MemObj(SymID id, const Type* type = nullptr);

    /// Destructor
    ~MemObj()
    {
        destroy();
    }

    /// Initialize the object
    void init(const Value *val);

    /// Initialize black hole and constant object
    void init(const Type* type);

    /// Get obj type
    const llvm::Type* getType() const;

    /// Get max field offset limit
    inline Size_t getMaxFieldOffsetLimit() const
    {
        return typeInfo->getMaxFieldOffsetLimit();
    }

    /// Get the reference value to this object
    inline const Value *getRefVal() const
    {
        return refVal;
    }

    /// Get the memory object id
    inline SymID getSymId() const
    {
        return GSymID;
    }

    /// Return true if its field limit is 0
    inline bool isFieldInsensitive() const
    {
        return getMaxFieldOffsetLimit() == 0;
    }

    /// Set the memory object to be field insensitive
    inline void setFieldInsensitive()
    {
        typeInfo->setMaxFieldOffsetLimit(0);
    }

    /// Set the memory object to be field sensitive (up to max field limit)
    void setFieldSensitive();

    /// Whether it is a black hole object
    bool isBlackHoleObj() const;

    /// object attributes methods
    //@{
    inline bool isFunction() const
    {
        return typeInfo->isFunction();
    }
    inline bool isGlobalObj() const
    {
        return typeInfo->isGlobalObj();
    }
    inline bool isStaticObj() const
    {
        return typeInfo->isStaticObj();
    }
    inline bool isStack() const
    {
        return typeInfo->isStack();
    }
    inline bool isHeap() const
    {
        return typeInfo->isHeap();
    }

    inline bool isStruct() const
    {
        return typeInfo->isStruct();
    }
    inline bool isArray() const
    {
        return typeInfo->isArray();
    }
    inline bool isVarStruct() const
    {
        return typeInfo->isVarStruct();
    }
    inline bool isVarArray() const
    {
        return typeInfo->isVarArray();
    }
    inline bool isConstStruct() const
    {
        return typeInfo->isConstStruct();
    }
    inline bool isConstArray() const
    {
        return typeInfo->isConstArray();
    }
    inline bool isConstant() const
    {
        return typeInfo->isConstant();
    }
    inline bool hasPtrObj() const
    {
        return typeInfo->hasPtrObj();
    }
    inline bool isNonPtrFieldObj(const LocationSet& ls) const
    {
        return typeInfo->isNonPtrFieldObj(ls);
    }
    //@}

    /// Operator overloading
    inline bool operator==(const MemObj &mem) const
    {
        return refVal == mem.getRefVal();
    }

    /// Clean up memory
    void destroy();
};

} // End namespace SVF




#endif /* OBJECTANDSYMBOL_H_ */
