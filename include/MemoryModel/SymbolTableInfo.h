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
    TypeToFieldInfoMap::iterator getStructInfoIter(const Type *T);

    ///Get a reference to StructInfo.
    inline StInfo* getStructInfo(const Type *T)
    {
        return getStructInfoIter(T)->second;
    }

    ///Get a reference to the components of struct_info.
    /// Number of flattenned elements of an array or struct
    u32_t getNumOfFlattenElements(const Type *T);
    /// Flatterned element idx of an array or struct by considering stride
    u32_t getFlattenedElemIdx(const Type *T, u32_t origId);

    ///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
    ///  OriginalElemType of b with field_idx 1 : Struct A
    ///  FlatternedElemType of b with field_idx 1 : int
    const Type* getOriginalElemType(const Type* baseType, u32_t origId);
    /// Return the type of a flattened element given a flattened index
    const Type* getFlatternedElemType(const Type* baseType, u32_t flatten_idx);
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
    /// Return the flattened field type for struct type only
    const std::vector<const Type*>& getFlattenFieldTypes(const StructType *T);

    /// Collect the struct info
    virtual void collectStructInfo(const StructType *T);
    /// Collect the array info
    virtual void collectArrayInfo(const ArrayType* T);
    /// Collect simple type (non-aggregate) info
    virtual void collectSimpleTypeInfo(const Type* T);

    /// Create an objectInfo based on LLVM type (value is null, and type could be null, representing a dummy object)
    ObjTypeInfo* createObjTypeInfo(const Type *type);

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
    virtual ~MemObj()
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
    bool isConstantData() const;
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
 * Flatterned type information of StructType, ArrayType and SingleValueType
 */
class StInfo
{

private:
    /// flattened field indices of a struct (ignoring arrays)
    std::vector<u32_t> fldIdxVec;
    /// flattened element indices including structs and arrays by considering strides
    std::vector<u32_t> elemIdxVec;
    /// Types of all fields of a struct
    Map<u32_t, const Type*> fldIdx2TypeMap;
    /// All field infos after flattening a struct
    std::vector<const Type*> finfo;
    /// stride represents the number of repetitive elements if this StInfo represent an ArrayType. stride is 1 by default.
    u32_t stride; 
    /// number of elements after flattenning (including array elements)
    u32_t numOfFlattenElements; 
    /// number of fields after flattenning (ignoring array elements)
    u32_t numOfFlattenFields; 
    /// Type vector of fields
    std::vector<const Type*> flattenElementTypes;
    /// Max field limit

    StInfo(); ///< place holder
    StInfo(const StInfo& st); ///< place holder
    void operator=(const StInfo&); ///< place holder

public:
    /// Constructor
    StInfo(u32_t s) : stride(s), numOfFlattenElements(s), numOfFlattenFields(s)
    {
    }
    /// Destructor
    ~StInfo()
    {
    }

    ///  struct A { int id; int salary; }; struct B { char name[20]; struct A a;}   B b;
    ///  OriginalFieldType of b with field_idx 1 : Struct A
    ///  FlatternedFieldType of b with field_idx 1 : int
    //{@
    const Type* getOriginalElemType(u32_t fldIdx);

    inline std::vector<u32_t>& getFlattenedFieldIdxVec()
    {
        return fldIdxVec;
    }
    inline std::vector<u32_t>& getFlattenedElemIdxVec()
    {
        return elemIdxVec;
    }
    inline std::vector<const Type*>& getFlattenElementTypes()
    {
        return flattenElementTypes;
    }
    inline std::vector<const Type*>& getFlattenFieldTypes()
    {
        return finfo;
    }
    //@}

    /// Add field index and element index and their corresponding type
    void addFldWithType(u32_t fldIdx, const Type* type, u32_t elemIdx);

    /// Set number of fields and elements of an aggrate
    inline void setNumOfFieldsAndElems(u32_t nf, u32_t ne)
    {
        numOfFlattenFields = nf;
        numOfFlattenElements = ne;
    }

    /// Return number of elements after flattenning (including array elements)
    inline u32_t getNumOfFlattenElements() const {
        return numOfFlattenElements;
    }

    /// Return the number of fields after flattenning (ignoring array elements)
    inline u32_t getNumOfFlattenFields() const {
        return numOfFlattenFields;
    }
    /// Return the stride
    inline u32_t getStride() const{
        return stride;
    }
};

/*!
 * Type Info of an abstract memory object
 */
class ObjTypeInfo
{
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
    /// LLVM type
    const Type* type;
    /// Type flags
    u32_t flags;
    /// Max offset for flexible field sensitive analysis
    /// maximum number of field object can be created
    /// minimum number is 0 (field insensitive analysis)
    u32_t maxOffsetLimit;
    /// Size of the object or number of elements
    u32_t elemNum;

    void resetTypeForHeapStaticObj(const Type* type);
public:

    /// Constructors
    ObjTypeInfo(const Type* t, u32_t max);

    /// Destructor
    virtual ~ObjTypeInfo()
    {
    }
    
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
    inline bool isConstantData()
    {
        return hasFlag(CONST_DATA);
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
