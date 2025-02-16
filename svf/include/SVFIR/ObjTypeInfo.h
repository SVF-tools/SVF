//===- ObjTypeInfo.h -- Object type information------------------------//
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
 * ObjTypeInfo.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_OBJTYPEINFO_H_
#define INCLUDE_SVFIR_OBJTYPEINFO_H_


#include "Util/SVFUtil.h"
#include "MemoryModel/AccessPath.h"
namespace SVF
{

/*!
 * Type Info of an abstract memory object
 */
class ObjTypeInfo
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SymbolTableBuilder;

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
        CONST_GLOBAL_OBJ = 0x200,  // global constant object
        CONST_DATA = 0x400,  // constant object str e.g. 5, 10, 1.0
    } MEMTYPE;

private:
    /// SVF type
    const SVFType* type;
    /// Type flags
    u32_t flags;
    /// Max offset for flexible field sensitive analysis
    /// maximum number of field object can be created
    /// minimum number is 0 (field insensitive analysis)
    u32_t maxOffsetLimit;
    /// Size of the object or number of elements
    u32_t elemNum;

    /// Byte size of object
    u32_t byteSize;

    inline void resetTypeForHeapStaticObj(const SVFType* t)
    {
        assert((isStaticObj() || isHeap()) && "can only reset the inferred type for heap and static objects!");
        type = t;
    }
public:

    /// Constructors
    ObjTypeInfo(const SVFType* t, u32_t max) : type(t), flags(0), maxOffsetLimit(max), elemNum(max)
    {
        assert(t && "no type information for this object?");
    }

    /// Destructor
    virtual ~ObjTypeInfo()
    {
    }

    /// Get LLVM type
    inline const SVFType* getType() const
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

    /// Set the number of elements of this object
    inline void setNumOfElements(u32_t num)
    {
        elemNum = num;
        setMaxFieldOffsetLimit(num);
    }

    /// Get the number of elements of this object
    inline u32_t getNumOfElements() const
    {
        return elemNum;
    }

    /// Get the byte size of this object
    inline u32_t getByteSizeOfObj() const
    {
        assert(isConstantByteSize() && "This Obj's byte size is not constant.");
        return byteSize;
    }

    /// Set the byte size of this object
    inline void setByteSizeOfObj(u32_t size)
    {
        byteSize = size;
    }

    /// Check if byte size is a const value
    inline bool isConstantByteSize() const
    {
        return byteSize != 0;
    }

    /// Flag for this object type
    //@{
    inline void setFlag(MEMTYPE mask)
    {
        flags |= mask;
    }
    inline u32_t getFlag() const
    {
        return flags;
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
    inline bool isConstantStruct()
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
    inline bool isConstantArray()
    {
        return  hasFlag(CONST_ARRAY_OBJ);
    }
    inline bool isArray()
    {
        return hasFlag(VAR_ARRAY_OBJ) || hasFlag(CONST_ARRAY_OBJ);
    }
    inline bool isConstDataOrConstGlobal()
    {
        return hasFlag(CONST_GLOBAL_OBJ) || hasFlag(CONST_DATA);
    }
    inline bool isConstDataOrAggData()
    {
        return hasFlag(CONST_DATA);
    }
    //@}
};


} // End namespace SVF

#endif /* INCLUDE_SVFIR_OBJTYPEINFO_H_ */
