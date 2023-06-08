//===- SymbolTableInfo.h -- Symbol information from IR------------------------//
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
 * SymbolTableInfo.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_SYMBOLTABLEINFO_H_
#define INCLUDE_SVFIR_SYMBOLTABLEINFO_H_


#include "Util/SVFUtil.h"
#include "MemoryModel/AccessPath.h"
#include "SVFIR/SVFModule.h"
namespace SVF
{

class MemObj;
class ObjTypeInfo;
class StInfo;

/*!
 * Symbol table of the memory model for analysis
 */
class SymbolTableInfo
{
    friend class SymbolTableBuilder;
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:

    /// Symbol types
    enum SYMTYPE
    {
        NullPtr,
        BlkPtr,
        BlackHole,
        ConstantObj,
        ValSymbol,
        ObjSymbol,
        RetSymbol,
        VarargSymbol
    };

    /// various maps defined
    //{@
    /// llvm value to sym id map
    /// local (%) and global (@) identifiers are pointer types which have a value node id.
    typedef OrderedMap<const SVFValue*, SymID> ValueToIDMapTy;
    /// sym id to memory object map
    typedef OrderedMap<SymID, MemObj*> IDToMemMapTy;
    /// function to sym id map
    typedef OrderedMap<const SVFFunction*, SymID> FunToIDMapTy;
    /// struct type to struct info map
    typedef Set<const SVFType*> SVFTypeSet;
    //@}

private:
    ValueToIDMapTy valSymMap;  ///< map a value to its sym id
    ValueToIDMapTy objSymMap;  ///< map a obj reference to its sym id
    FunToIDMapTy returnSymMap; ///< return map
    FunToIDMapTy varargSymMap; ///< vararg map
    IDToMemMapTy objMap;       ///< map a memory sym id to its obj

    // Singleton pattern here to enable instance of SymbolTableInfo can only be created once.
    static SymbolTableInfo* symInfo;

    /// Module
    SVFModule* mod;

    /// Clean up memory
    void destroy();

    /// Whether to model constants
    bool modelConstants;

    /// total number of symbols
    SymID totalSymNum;

protected:
    /// Constructor
    SymbolTableInfo(void)
        : mod(nullptr), modelConstants(false), totalSymNum(0),
          maxStruct(nullptr), maxStSize(0)
    {
    }

public:

    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static SymbolTableInfo* SymbolInfo();

    static void releaseSymbolInfo()
    {
        delete symInfo;
        symInfo = nullptr;
    }
    virtual ~SymbolTableInfo()
    {
        destroy();
    }
    //@}

    /// Set / Get modelConstants
    //@{
    void setModelConstants(bool _modelConstants)
    {
        modelConstants = _modelConstants;
    }
    bool getModelConstants() const
    {
        return modelConstants;
    }
    //@}

    /// Module
    inline SVFModule* getModule()
    {
        return mod;
    }
    /// Module
    inline void setModule(SVFModule* m)
    {
        mod = m;
    }

    /// special value
    // @{
    static inline bool isBlkPtr(NodeID id)
    {
        return (id == BlkPtr);
    }
    static inline bool isNullPtr(NodeID id)
    {
        return (id == NullPtr);
    }
    static inline bool isBlkObj(NodeID id)
    {
        return (id == BlackHole);
    }
    static inline bool isConstantObj(NodeID id)
    {
        return (id == ConstantObj);
    }
    static inline bool isBlkObjOrConstantObj(NodeID id)
    {
        return (isBlkObj(id) || isConstantObj(id));
    }

    inline MemObj* getBlkObj() const
    {
        return getObj(blackholeSymID());
    }
    inline MemObj* getConstantObj() const
    {
        return getObj(constantSymID());
    }

    inline SymID blkPtrSymID() const
    {
        return BlkPtr;
    }

    inline SymID nullPtrSymID() const
    {
        return NullPtr;
    }

    inline SymID constantSymID() const
    {
        return ConstantObj;
    }

    inline SymID blackholeSymID() const
    {
        return BlackHole;
    }

    /// Can only be invoked by SVFIR::addDummyNode() when creaing SVFIR from file.
    const MemObj* createDummyObj(SymID symId, const SVFType* type);
    // @}

    /// Get different kinds of syms
    //@{
    SymID getValSym(const SVFValue* val);

    bool hasValSym(const SVFValue* val);

    inline SymID getObjSym(const SVFValue* val) const
    {
        const SVFValue* svfVal = val;
        if(const SVFGlobalValue* g = SVFUtil::dyn_cast<SVFGlobalValue>(val))
            svfVal = g->getDefGlobalForMultipleModule();
        ValueToIDMapTy::const_iterator iter = objSymMap.find(svfVal);
        assert(iter!=objSymMap.end() && "obj sym not found");
        return iter->second;
    }

    inline MemObj* getObj(SymID id) const
    {
        IDToMemMapTy::const_iterator iter = objMap.find(id);
        assert(iter!=objMap.end() && "obj not found");
        return iter->second;
    }

    inline SymID getRetSym(const SVFFunction* val) const
    {
        FunToIDMapTy::const_iterator iter =  returnSymMap.find(val);
        assert(iter!=returnSymMap.end() && "ret sym not found");
        return iter->second;
    }

    inline SymID getVarargSym(const SVFFunction* val) const
    {
        FunToIDMapTy::const_iterator iter =  varargSymMap.find(val);
        assert(iter!=varargSymMap.end() && "vararg sym not found");
        return iter->second;
    }
    //@}


    /// Statistics
    //@{
    inline u32_t getTotalSymNum() const
    {
        return totalSymNum;
    }
    inline u32_t getMaxStructSize() const
    {
        return maxStSize;
    }
    //@}

    /// Get different kinds of syms maps
    //@{
    inline ValueToIDMapTy& valSyms()
    {
        return valSymMap;
    }

    inline ValueToIDMapTy& objSyms()
    {
        return objSymMap;
    }

    inline IDToMemMapTy& idToObjMap()
    {
        return objMap;
    }

    inline const IDToMemMapTy& idToObjMap() const
    {
        return objMap;
    }

    inline FunToIDMapTy& retSyms()
    {
        return returnSymMap;
    }

    inline FunToIDMapTy& varargSyms()
    {
        return varargSymMap;
    }

    //@}

    /// Constant reader that won't change the state of the symbol table
    //@{
    inline const SVFTypeSet& getSVFTypes() const
    {
        return svfTypes;
    }

    inline const Set<const StInfo*>& getStInfos() const
    {
        return stInfos;
    }
    //@}

    /// Get struct info
    //@{
    ///Get a reference to StructInfo.
    const StInfo* getTypeInfo(const SVFType* T) const;
    inline bool hasSVFTypeInfo(const SVFType* T)
    {
        return svfTypes.find(T) != svfTypes.end();
    }

    ///Get a reference to the components of struct_info.
    /// Number of flattenned elements of an array or struct
    u32_t getNumOfFlattenElements(const SVFType* T);
    /// Flatterned element idx of an array or struct by considering stride
    u32_t getFlattenedElemIdx(const SVFType* T, u32_t origId);
    /// Return the type of a flattened element given a flattened index
    const SVFType* getFlatternedElemType(const SVFType* baseType, u32_t flatten_idx);
    ///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
    ///  OriginalElemType of b with field_idx 1 : Struct A
    ///  FlatternedElemType of b with field_idx 1 : int
    const SVFType* getOriginalElemType(const SVFType* baseType, u32_t origId) const;
    //@}

    /// Debug method
    void printFlattenFields(const SVFType* type);

    static std::string toString(SYMTYPE symtype);

    /// Another debug method
    virtual void dump();

    /// Given an offset from a Gep Instruction, return it modulus offset by considering memory layout
    virtual APOffset getModulusOffset(const MemObj* obj,
                                      const APOffset& apOffset);

    ///The struct type with the most fields
    const SVFType* maxStruct;

    ///The number of fields in max_struct
    u32_t maxStSize;

    inline void addTypeInfo(const SVFType* ty)
    {
        bool inserted = svfTypes.insert(ty).second;
        if(!inserted)
            assert(false && "this type info has been added before");
    }

    inline void addStInfo(StInfo* stInfo)
    {
        stInfos.insert(stInfo);
    }

protected:

    /// Return the flattened field type for struct type only
    const std::vector<const SVFType*>& getFlattenFieldTypes(const SVFStructType *T);

    /// Create an objectInfo based on LLVM type (value is null, and type could be null, representing a dummy object)
    ObjTypeInfo* createObjTypeInfo(const SVFType* type);

    /// (owned) All SVF Types
    /// Every type T is mapped to StInfo
    /// which contains size (fsize) , offset(foffset)
    /// fsize[i] is the number of fields in the largest such struct, else fsize[i] = 1.
    /// fsize[0] is always the size of the expanded struct.
    SVFTypeSet svfTypes;

    /// @brief (owned) All StInfo
    Set<const StInfo*> stInfos;
};


/*!
 * Memory object symbols or MemObj (address-taken variables in LLVM-based languages)
 */
class MemObj
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    /// Type information of this object
    ObjTypeInfo* typeInfo;
    /// The unique value of this symbol/variable
    const SVFValue* refVal;
    /// The unique id to represent this symbol
    SymID symId;

public:
    /// Constructor
    MemObj(SymID id, ObjTypeInfo* ti, const SVFValue* val = nullptr);

    /// Destructor
    virtual ~MemObj()
    {
        destroy();
    }

    virtual const std::string toString() const;

    /// Get the reference value to this object
    inline const SVFValue* getValue() const
    {
        return refVal;
    }

    /// Get the memory object id
    inline SymID getId() const
    {
        return symId;
    }

    /// Get obj type
    const SVFType* getType() const;

    /// Get the number of elements of this object
    u32_t getNumOfElements() const;

    /// Set the number of elements of this object
    void setNumOfElements(u32_t num);

    /// Get max field offset limit
    u32_t getMaxFieldOffsetLimit() const;

    /// Return true if its field limit is 0
    bool isFieldInsensitive() const;

    /// Set the memory object to be field insensitive
    void setFieldInsensitive();

    /// Set the memory object to be field sensitive (up to max field limit)
    void setFieldSensitive();

    /// Whether it is a black hole object
    bool isBlackHoleObj() const;

    /// object attributes methods
    //@{
    bool isFunction() const;
    bool isGlobalObj() const;
    bool isStaticObj() const;
    bool isStack() const;
    bool isHeap() const;
    bool isStruct() const;
    bool isArray() const;
    bool isVarStruct() const;
    bool isVarArray() const;
    bool isConstantStruct() const;
    bool isConstantArray() const;
    bool isConstDataOrConstGlobal() const;
    bool isConstDataOrAggData() const;
    bool hasPtrObj() const;
    bool isNonPtrFieldObj(const APOffset& apOffset) const;
    //@}

    /// Operator overloading
    inline bool operator==(const MemObj& mem) const
    {
        return getValue() == mem.getValue();
    }

    /// Clean up memory
    void destroy();
};

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
        HASPTR_OBJ = 0x800		// the object stores a pointer address
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

    void resetTypeForHeapStaticObj(const SVFType* type);
public:

    /// Constructors
    ObjTypeInfo(const SVFType* t, u32_t max);

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
    inline bool hasPtrObj()
    {
        return hasFlag(HASPTR_OBJ);
    }
    virtual bool isNonPtrFieldObj(const APOffset& apOffset);
    //@}
};


} // End namespace SVF

#endif /* INCLUDE_SVFIR_SYMBOLTABLEINFO_H_ */
