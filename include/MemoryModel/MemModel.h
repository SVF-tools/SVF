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


/// Symbol types
enum SYMTYPE {
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
class StInfo {

private:
    /// flattened field indices of a struct
    std::vector<u32_t> fldIdxVec;
    /// flattened field offsets of of a struct
    std::vector<u32_t> foffset;
    /// Types of all fields of a struct
    std::map<u32_t, const llvm::Type*> fldIdx2TypeMap;
    /// Types of all fields of a struct
    std::map<u32_t, const llvm::Type*> offset2TypeMap;
    /// All field infos after flattening a struct
    std::vector<FieldInfo> finfo;

public:
    /// Constructor
    StInfo() {
    }
    /// Destructor
    ~StInfo() {
    }

    /// Get method for fields of a struct
    //{@
	inline const llvm::Type* getFieldTypeWithFldIdx(u32_t fldIdx) {
		return fldIdx2TypeMap[fldIdx];
	}
	inline const llvm::Type* getFieldTypeWithByteOffset(u32_t offset) {
		return offset2TypeMap[offset];
	}
	inline std::vector<u32_t>& getFieldIdxVec() {
        return fldIdxVec;
    }
	inline std::vector<u32_t>& getFieldOffsetVec() {
        return foffset;
    }
	inline std::vector<FieldInfo>& getFlattenFieldInfoVec() {
        return finfo;
    }
    //@}

    /// Add field (index and offset) with its corresponding type
	inline void addFldWithType(u32_t fldIdx, u32_t offset, const llvm::Type* type) {
		fldIdxVec.push_back(fldIdx);
		foffset.push_back(offset);
		fldIdx2TypeMap[fldIdx] = type;
		offset2TypeMap[offset] = type;
	}
};

/*!
 * Type Info of an abstract memory object
 */
class ObjTypeInfo {

public:
    typedef enum {
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
    ObjTypeInfo(const Value* val, const Type* t, u32_t max) :
        type(t), flags(0), maxOffsetLimit(max) {
    }
    /// Constructor
    ObjTypeInfo(u32_t max, const Type* t) : type(t), flags(0), maxOffsetLimit(max) {

    }
    /// Destructor
    virtual ~ObjTypeInfo() {

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
    inline const Type* getType() const{
        return type;
    }

    /// Get max field offset limit
    inline u32_t getMaxFieldOffsetLimit() {
        return maxOffsetLimit;
    }

    /// Get max field offset limit
    inline void setMaxFieldOffsetLimit(u32_t limit) {
        maxOffsetLimit = limit;
    }

    /// Flag for this object type
    //@{
    inline void setFlag(MEMTYPE mask) {
        flags |= mask;
    }
    inline bool hasFlag(MEMTYPE mask) {
        return (flags & mask) == mask;
    }
    //@}

    /// Object attributes
    //@{
    inline bool isFunction() {
        return hasFlag(FUNCTION_OBJ);
    }
    inline bool isGlobalObj() {
        return hasFlag(GLOBVAR_OBJ);
    }
    inline bool isStaticObj() {
        return hasFlag(STATIC_OBJ);
    }
    inline bool isStack() {
        return hasFlag(STACK_OBJ);
    }
    inline bool isHeap() {
        return hasFlag(HEAP_OBJ);
    }
    //@}

    /// Object attributes (noted that an object can be a nested compound types)
    /// e.g. both isStruct and isArray can return true
    //@{
    inline bool isVarStruct() {
        return hasFlag(VAR_STRUCT_OBJ);
    }
    inline bool isConstStruct() {
        return hasFlag(CONST_STRUCT_OBJ);
    }
    inline bool isStruct() {
        return hasFlag(VAR_STRUCT_OBJ) || hasFlag(CONST_STRUCT_OBJ);
    }
    inline bool isVarArray() {
        return hasFlag(VAR_ARRAY_OBJ);
    }
    inline bool isConstArray() {
        return  hasFlag(CONST_ARRAY_OBJ);
    }
    inline bool isArray() {
        return hasFlag(VAR_ARRAY_OBJ) || hasFlag(CONST_ARRAY_OBJ);
    }
    inline bool isConstant() {
        return hasFlag(CONST_OBJ);
    }
    inline bool hasPtrObj() {
        return hasFlag(HASPTR_OBJ);
    }
    virtual bool isNonPtrFieldObj(const LocationSet& ls);
    //@}
};

/*!
 * Memory Object
 */
class MemObj {

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
    MemObj(SymID id, const Type* type = NULL);

    /// Destructor
    ~MemObj() {
        destroy();
    }

    /// Initialize the object
    void init(const Value *val);

    /// Initialize black hole and constant object
    void init(const Type* type);

    /// Get obj type
    const llvm::Type* getType() const;

    /// Get max field offset limit
    inline Size_t getMaxFieldOffsetLimit() const {
        return typeInfo->getMaxFieldOffsetLimit();
    }

    /// Get the reference value to this object
    inline const Value *getRefVal() const {
        return refVal;
    }

    /// Get the memory object id
    inline SymID getSymId() const {
        return GSymID;
    }

    /// Return true if its field limit is 0
    inline bool isFieldInsensitive() const {
        return getMaxFieldOffsetLimit() == 0;
    }

    /// Set the memory object to be field insensitive
    inline void setFieldInsensitive() {
        typeInfo->setMaxFieldOffsetLimit(0);
    }

    /// Set the memory object to be field sensitive (up to max field limit)
    void setFieldSensitive();

    /// Whether it is a black hole object
    bool isBlackHoleObj() const;

    /// object attributes methods
    //@{
    inline bool isFunction() const {
        return typeInfo->isFunction();
    }
    inline bool isGlobalObj() const {
        return typeInfo->isGlobalObj();
    }
    inline bool isStaticObj() const {
        return typeInfo->isStaticObj();
    }
    inline bool isStack() const {
        return typeInfo->isStack();
    }
    inline bool isHeap() const {
        return typeInfo->isHeap();
    }

    inline bool isStruct() const {
        return typeInfo->isStruct();
    }
    inline bool isArray() const {
        return typeInfo->isArray();
    }
    inline bool isVarStruct() const {
        return typeInfo->isVarStruct();
    }
    inline bool isVarArray() const {
        return typeInfo->isVarArray();
    }
    inline bool isConstStruct() const {
        return typeInfo->isConstStruct();
    }
    inline bool isConstArray() const {
        return typeInfo->isConstArray();
    }
    inline bool isConstant() const {
        return typeInfo->isConstant();
    }
    inline bool hasPtrObj() const {
        return typeInfo->hasPtrObj();
    }
    inline bool isNonPtrFieldObj(const LocationSet& ls) const {
        return typeInfo->isNonPtrFieldObj(ls);
    }
    //@}

    /// Operator overloading
    inline bool operator==(const MemObj &mem) const {
        return refVal == mem.getRefVal();
    }

    /// Clean up memory
    void destroy();
};


/*!
 * Symbol table of the memory model for analysis
 */
class SymbolTableInfo {

public:
    /// various maps defined
    //{@
    /// llvm value to sym id map
    /// local (%) and global (@) identifiers are pointer types which have a value node id.
    typedef llvm::DenseMap<const Value *, SymID> ValueToIDMapTy;
    /// sym id to memory object map
    typedef llvm::DenseMap<SymID,MemObj*> IDToMemMapTy;
    /// function to sym id map
    typedef llvm::DenseMap<const Function *, SymID> FunToIDMapTy;
    /// sym id to sym type map
    typedef llvm::DenseMap<SymID,SYMTYPE> IDToSymTyMapTy;
    /// struct type to struct info map
    typedef llvm::DenseMap<const Type*, StInfo*> TypeToFieldInfoMap;
    typedef std::set<CallSite> CallSiteSet;
    typedef llvm::DenseMap<const Instruction*,CallSiteID> CallSiteToIDMapTy;
    typedef llvm::DenseMap<CallSiteID,const Instruction*> IDToCallSiteMapTy;

    //@}

private:
    /// Data layout on a target machine
    static DataLayout *dl;

    ValueToIDMapTy valSymMap;	///< map a value to its sym id
    ValueToIDMapTy objSymMap;	///< map a obj reference to its sym id
    IDToMemMapTy		objMap;		///< map a memory sym id to its obj
    IDToSymTyMapTy	symTyMap;	/// < map a sym id to its type
    FunToIDMapTy returnSymMap;		///< return  map
    FunToIDMapTy varargSymMap;	    ///< vararg map

    CallSiteSet callSiteSet;

    // Singleton pattern here to enable instance of SymbolTableInfo can only be created once.
    static SymbolTableInfo* symlnfo;

    /// Module
    SVFModule mod;

    /// Max field limit
    static u32_t maxFieldLimit;

    /// Invoke llvm passes to modify module
    void prePassSchedule(SVFModule svfModule);

    /// Clean up memory
    void destroy();

    /// Whether to model constants
    bool modelConstants;

protected:
    /// Constructor
    SymbolTableInfo() :
        modelConstants(false), maxStruct(NULL), maxStSize(0) {
    }

public:
    /// Statistics
    //@{
    static SymID totalSymNum;

    static inline u32_t getMaxFieldLimit() {
        return maxFieldLimit;
    }
    //@}

    /// Singleton design here to make sure we only have one instance during any analysis
    //@{
    static SymbolTableInfo* Symbolnfo();

    static void releaseSymbolnfo() {
        delete symlnfo;
        symlnfo = NULL;
    }
    virtual ~SymbolTableInfo() {
        destroy();
    }
    //@}

    /// Set / Get modelConstants
    //@{
    void setModelConstants(bool _modelConstants) {
        modelConstants = _modelConstants;
    }
    bool getModelConstants() const {
        return modelConstants;
    }
    //@}

    /// Get callsite set
    //@{
    inline const CallSiteSet& getCallSiteSet() const {
        return callSiteSet;
    }
    //@}

    /// Module
    inline SVFModule getModule() {
        return mod;
    }

    /// Get target machine data layout
    inline static DataLayout* getDataLayout(Module* mod) {
        if(dl==NULL)
            return dl = new DataLayout(mod);
        return dl;
    }

    /// Helper method to get the size of the type from target data layout
    //@{
    u32_t getTypeSizeInBytes(const Type* type);
    u32_t getTypeSizeInBytes(const StructType *sty, u32_t field_index);
    //@}

    /// Start building memory model
    void buildMemModel(SVFModule svfModule);

    /// collect the syms
    //@{
    void collectSym(const Value *val);

    void collectVal(const Value *val);

    void collectObj(const Value *val);

    void collectRet(const Function *val);

    void collectVararg(const Function *val);
    //@}

    /// special value
    // @{
    static bool isNullPtrSym(const Value *val);

    static bool isBlackholeSym(const Value *val);

    bool isConstantObjSym(const Value *val);

    static inline bool isBlkPtr(NodeID id) {
        return (id == BlkPtr);
    }
    static inline bool isNullPtr(NodeID id) {
        return (id == NullPtr);
    }
    static inline bool isBlkObj(NodeID id) {
        return (id == BlackHole);
    }
    static inline bool isConstantObj(NodeID id) {
        return (id == ConstantObj);
    }
    static inline bool isBlkObjOrConstantObj(NodeID id) {
        return (isBlkObj(id) || isConstantObj(id));
    }

    inline void createBlkOrConstantObj(SymID symId) {
        assert(isBlkObjOrConstantObj(symId));
        assert(objMap.find(symId)==objMap.end());
        objMap[symId] = new MemObj(symId);;
    }

    inline MemObj* getBlkObj() const {
        return getObj(blackholeSymID());
    }
    inline MemObj* getConstantObj() const {
        return getObj(constantSymID());
    }

    inline SymID blkPtrSymID() const {
        return BlkPtr;
    }

    inline SymID nullPtrSymID() const {
        return NullPtr;
    }

    inline SymID constantSymID() const {
        return ConstantObj;
    }

    inline SymID blackholeSymID() const {
        return BlackHole;
    }

    /// Can only be invoked by PAG::addDummyNode() when creaing PAG from file.
    inline const MemObj* createDummyObj(SymID symId, const Type* type) {
        assert(objMap.find(symId)==objMap.end() && "this dummy obj has been created before");
        MemObj* memObj = new MemObj(symId, type);
        objMap[symId] = memObj;
        return memObj;
    }
    // @}

    /// Handle constant expression
    // @{
    void handleGlobalCE(const GlobalVariable *G);
    void handleGlobalInitializerCE(const Constant *C, u32_t offset);
    void handleCE(const Value *val);
    // @}

    /// Get different kinds of syms
    //@{
    SymID getValSym(const Value *val) {

        if(isNullPtrSym(val))
            return nullPtrSymID();
        else if(isBlackholeSym(val))
            return blkPtrSymID();
        else {
            ValueToIDMapTy::const_iterator iter =  valSymMap.find(val);
            assert(iter!=valSymMap.end() &&"value sym not found");
            return iter->second;
        }
    }

    inline bool hasValSym(const Value* val) {
        if (isNullPtrSym(val) || isBlackholeSym(val))
            return true;
        else
            return (valSymMap.find(val) != valSymMap.end());
    }

    /// find the unique defined global across multiple modules
    inline const Value* getGlobalRep(const Value* val) const{
        if(const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val)) {
            if (symlnfo->getModule().hasGlobalRep(gvar))
                val = symlnfo->getModule().getGlobalRep(gvar);
        }
        return val;
    }

    inline SymID getObjSym(const Value *val) const {
        ValueToIDMapTy::const_iterator iter = objSymMap.find(getGlobalRep(val));
        assert(iter!=objSymMap.end() && "obj sym not found");
        return iter->second;
    }

    inline MemObj* getObj(SymID id) const {
        IDToMemMapTy::const_iterator iter = objMap.find(id);
        assert(iter!=objMap.end() && "obj not found");
        return iter->second;
    }

    inline SymID getRetSym(const Function *val) const {
        FunToIDMapTy::const_iterator iter =  returnSymMap.find(val);
        assert(iter!=returnSymMap.end() && "ret sym not found");
        return iter->second;
    }

    inline SymID getVarargSym(const Function *val) const {
        FunToIDMapTy::const_iterator iter =  varargSymMap.find(val);
        assert(iter!=varargSymMap.end() && "vararg sym not found");
        return iter->second;
    }
    //@}

    /// Statistics
    //@{
    inline Size_t getTotalSymNum() const {
        return totalSymNum;
    }
    inline u32_t getMaxStructSize() const {
        return maxStSize;
    }
    //@}

    /// Get different kinds of syms maps
    //@{
    inline ValueToIDMapTy& valSyms() {
        return valSymMap;
    }

    inline ValueToIDMapTy& objSyms() {
        return objSymMap;
    }

    inline IDToMemMapTy& idToObjMap() {
        return objMap;
    }

    inline FunToIDMapTy& retSyms() {
        return returnSymMap;
    }

    inline FunToIDMapTy& varargSyms() {
        return varargSymMap;
    }

    //@}

    /// Get struct info
    //@{
    ///Get an iterator for StructInfo, designed as internal methods
    TypeToFieldInfoMap::iterator getStructInfoIter(const Type *T) {
        assert(T);
        TypeToFieldInfoMap::iterator it = typeToFieldInfo.find(T);
        if (it != typeToFieldInfo.end())
            return it;
        else {
            collectTypeInfo(T);
            return typeToFieldInfo.find(T);
        }
    }

    ///Get a reference to StructInfo.
    inline StInfo* getStructInfo(const Type *T) {
        return getStructInfoIter(T)->second;
    }

    ///Get a reference to the components of struct_info.
    const inline std::vector<u32_t>& getFattenFieldIdxVec(const Type *T) {
        return getStructInfoIter(T)->second->getFieldIdxVec();
    }
    const inline std::vector<u32_t>& getFattenFieldOffsetVec(const Type *T) {
        return getStructInfoIter(T)->second->getFieldOffsetVec();
    }
    const inline std::vector<FieldInfo>& getFlattenFieldInfoVec(const Type *T) {
        return getStructInfoIter(T)->second->getFlattenFieldInfoVec();
    }
	const inline Type* getOrigSubTypeWithFldInx(const Type* baseType, u32_t field_idx) {
		return getStructInfoIter(baseType)->second->getFieldTypeWithFldIdx(field_idx);
	}
	const inline Type* getOrigSubTypeWithByteOffset(const Type* baseType, u32_t byteOffset) {
		return getStructInfoIter(baseType)->second->getFieldTypeWithByteOffset(byteOffset);
	}
    //@}

    /// Compute gep offset
    virtual bool computeGepOffset(const User *V, LocationSet& ls);
    /// Get the base type and max offset
    const Type *getBaseTypeAndFlattenedFields(const Value *V, std::vector<LocationSet> &fields);
    /// Replace fields with flatten fields of T if the number of its fields is larger than msz.
    u32_t getFields(std::vector<LocationSet>& fields, const Type* T, u32_t msz);
    /// Collect type info
    void collectTypeInfo(const Type* T);
    /// Given an offset from a Gep Instruction, return it modulus offset by considering memory layout
    virtual LocationSet getModulusOffset(const MemObj* obj, const LocationSet& ls);

    /// Debug method
    void printFlattenFields(const Type* type);

protected:
    /// Collect the struct info
    virtual void collectStructInfo(const StructType *T);
    /// Collect the array info
    virtual void collectArrayInfo(const ArrayType* T);
    /// Collect simple type (non-aggregate) info
    virtual void collectSimpleTypeInfo(const Type* T);

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



#endif /* OBJECTANDSYMBOL_H_ */
