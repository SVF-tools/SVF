//===- SymbolTableInfo.h -- Symbol information from IR------------------------//
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
 * SymbolTableInfo.h
 *
 *  Created on: Nov 11, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVF_FE_SYMBOLTABLEINFO_H_
#define INCLUDE_SVF_FE_SYMBOLTABLEINFO_H_


#include "Util/SVFUtil.h"
#include "MemoryModel/LocationSet.h"
#include "Util/SVFModule.h"
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

public:

    /// Symbol types
    enum SYMTYPE
    {
        BlackHole,
        ConstantObj,
        BlkPtr,
        NullPtr,
        ValSymbol,
        ObjSymbol,
        RetSymbol,
        VarargSymbol
    };

    /// various maps defined
    //{@
    /// llvm value to sym id map
    /// local (%) and global (@) identifiers are pointer types which have a value node id.
    typedef OrderedMap<const Value *, SymID> ValueToIDMapTy;
    /// sym id to memory object map
    typedef OrderedMap<SymID,MemObj*> IDToMemMapTy;
    /// function to sym id map
    typedef OrderedMap<const Function *, SymID> FunToIDMapTy;
    /// struct type to struct info map
    typedef OrderedMap<const Type*, StInfo*> TypeToFieldInfoMap;
    typedef Set<CallSite> CallSiteSet;
    typedef OrderedMap<const Instruction*,CallSiteID> CallSiteToIDMapTy;
    typedef OrderedMap<CallSiteID,const Instruction*> IDToCallSiteMapTy;

    //@}

private:
    /// Data layout on a target machine
    static DataLayout *dl;

    ValueToIDMapTy valSymMap;	///< map a value to its sym id
    ValueToIDMapTy objSymMap;	///< map a obj reference to its sym id
    FunToIDMapTy returnSymMap;		///< return  map
    FunToIDMapTy varargSymMap;	    ///< vararg map
    IDToMemMapTy		objMap;		///< map a memory sym id to its obj

    CallSiteSet callSiteSet;

    // Singleton pattern here to enable instance of SymbolTableInfo can only be created once.
    static SymbolTableInfo* symInfo;

    /// Module
    SVFModule* mod;

    /// Max field limit
    static u32_t maxFieldLimit;

    /// Clean up memory
    void destroy();

    /// Whether to model constants
    bool modelConstants;

    /// total number of symbols
    SymID totalSymNum;

protected:
    /// Constructor
    SymbolTableInfo(void) :
        mod(nullptr), modelConstants(false), totalSymNum(0), maxStruct(nullptr), maxStSize(0)
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

    /// Get callsite set
    //@{
    inline const CallSiteSet& getCallSiteSet() const
    {
        return callSiteSet;
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

    /// Get target machine data layout
    inline static DataLayout* getDataLayout(Module* mod)
    {
        if(dl==nullptr)
            return dl = new DataLayout(mod);
        return dl;
    }

    /// Helper method to get the size of the type from target data layout
    //@{
    u32_t getTypeSizeInBytes(const Type* type);
    u32_t getTypeSizeInBytes(const StructType *sty, u32_t field_index);
    //@}

    /// special value
    // @{
    static bool isNullPtrSym(const Value *val);

    static bool isBlackholeSym(const Value *val);

    bool isConstantObjSym(const Value *val);

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

    MemObj* createBlkObj(SymID symId);

    MemObj* createConstantObj(SymID symId);

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
    const MemObj* createDummyObj(SymID symId, const Type* type);
    // @}

    /// Get different kinds of syms
    //@{
    SymID getValSym(const Value *val)
    {

        if(isNullPtrSym(val))
            return nullPtrSymID();
        else if(isBlackholeSym(val))
            return blkPtrSymID();
        else
        {
            ValueToIDMapTy::const_iterator iter =  valSymMap.find(val);
            assert(iter!=valSymMap.end() &&"value sym not found");
            return iter->second;
        }
    }

    inline bool hasValSym(const Value* val)
    {
        if (isNullPtrSym(val) || isBlackholeSym(val))
            return true;
        else
            return (valSymMap.find(val) != valSymMap.end());
    }

    inline SymID getObjSym(const Value *val) const
    {
        ValueToIDMapTy::const_iterator iter = objSymMap.find(SVFUtil::getGlobalRep(val));
        assert(iter!=objSymMap.end() && "obj sym not found");
        return iter->second;
    }

    inline MemObj* getObj(SymID id) const
    {
        IDToMemMapTy::const_iterator iter = objMap.find(id);
        assert(iter!=objMap.end() && "obj not found");
        return iter->second;
    }

    inline SymID getRetSym(const Function *val) const
    {
        FunToIDMapTy::const_iterator iter =  returnSymMap.find(val);
        assert(iter!=returnSymMap.end() && "ret sym not found");
        return iter->second;
    }

    inline SymID getVarargSym(const Function *val) const
    {
        FunToIDMapTy::const_iterator iter =  varargSymMap.find(val);
        assert(iter!=varargSymMap.end() && "vararg sym not found");
        return iter->second;
    }
    //@}

 
    /// Statistics
    //@{
    inline Size_t getTotalSymNum() const
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

    inline FunToIDMapTy& retSyms()
    {
        return returnSymMap;
    }

    inline FunToIDMapTy& varargSyms()
    {
        return varargSymMap;
    }

    //@}

    /// Get struct info
    //@{
    ///Get an iterator for StructInfo, designed as internal methods
    TypeToFieldInfoMap::iterator getStructInfoIter(const Type *T)
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

    ///Get a reference to StructInfo.
    inline StInfo* getStructInfo(const Type *T)
    {
        return getStructInfoIter(T)->second;
    }

    ///Get a reference to the components of struct_info.
    const std::vector<u32_t>& getFattenFieldIdxVec(const Type *T);
    const std::vector<u32_t>& getFattenFieldOffsetVec(const Type *T);
    const std::vector<FieldInfo>& getFlattenFieldInfoVec(const Type *T);
    const Type* getOrigSubTypeWithFldInx(const Type* baseType, u32_t field_idx);
    const Type* getOrigSubTypeWithByteOffset(const Type* baseType, u32_t byteOffset);
    //@}

    /// Collect type info
    void collectTypeInfo(const Type* T);
    /// Given an offset from a Gep Instruction, return it modulus offset by considering memory layout
    virtual LocationSet getModulusOffset(const MemObj* obj, const LocationSet& ls);

    /// Debug method
    void printFlattenFields(const Type* type);

    static std::string toString(SYMTYPE symtype);

    /// Another debug method
    virtual void dump();

protected:
    /// Collect the struct info
    virtual void collectStructInfo(const StructType *T);
    /// Collect the array info
    virtual void collectArrayInfo(const ArrayType* T);
    /// Collect simple type (non-aggregate) info
    virtual void collectSimpleTypeInfo(const Type* T);

    /// Create an objectInfo based on LLVM type (value is null, and type could be null, representing a dummy object)
    ObjTypeInfo* createObjTypeInfo(const Type *type = nullptr);

    /// Every type T is mapped to StInfo
    /// which contains size (fsize) , offset(foffset)
    /// fsize[i] is the number of fields in the largest such struct, else fsize[i] = 1.
    /// fsize[0] is always the size of the expanded struct.
    TypeToFieldInfoMap typeToFieldInfo;

    ///The struct type with the most fields
    const Type* maxStruct;

    ///The number of fields in max_struct
    u32_t maxStSize;
};


/*!
 * Memory object symbols or MemObj (address-taken variables in LLVM-based languages)
 */
class MemObj 
{

private:
    /// Type information of this object
    ObjTypeInfo* typeInfo;
    /// The unique value of this symbol/variable
    const Value *refVal;
    /// The unique id to represent this symbol
    SymID symId;
public:

    /// Constructor
    MemObj(SymID id, ObjTypeInfo* ti, const Value *val = nullptr);

    /// Destructor
    ~MemObj()
    {
        destroy();
    }

    virtual const std::string toString() const;

    /// Get the reference value to this object
    inline const Value* getValue() const
    {
        return refVal;
    }

    /// Get the memory object id
    inline SymID getId() const
    {
        return symId;
    }

    /// Get obj type
    const Type* getType() const;

    /// Get max field offset limit
    Size_t getMaxFieldOffsetLimit() const;

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
    bool isConstStruct() const;
    bool isConstArray() const;
    bool isConstant() const;
    bool hasPtrObj() const;
    bool isNonPtrFieldObj(const LocationSet& ls) const;
    //@}

    /// Operator overloading
    inline bool operator==(const MemObj &mem) const
    {
        return getValue() == mem.getValue();
    }

    /// Clean up memory
    void destroy();
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

    /// Analyse types of heap and static objects
    void analyzeHeapObjType(const Type* type);

    /// Analyse types of heap and static objects
    void analyzeStaticObjType(const Type* type);

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


} // End namespace SVF

#endif /* INCLUDE_SVF_FE_SYMBOLTABLEINFO_H_ */
