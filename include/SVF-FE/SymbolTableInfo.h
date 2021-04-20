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

#include "MemoryModel/MemModel.h"
#include "SVF-FE/LLVMModule.h"

namespace SVF
{

/*!
 * Symbol table of the memory model for analysis
 */
class SymbolTableInfo
{

public:
    /// various maps defined
    //{@
    /// llvm value to sym id map
    /// local (%) and global (@) identifiers are pointer types which have a value node id.
    typedef OrderedMap<const Value *, SymID> ValueToIDMapTy;
    /// sym id to memory object map
    typedef OrderedMap<SymID,MemObj*> IDToMemMapTy;
    /// function to sym id map
    typedef OrderedMap<const Function *, SymID> FunToIDMapTy;
    /// sym id to sym type map
    typedef OrderedMap<SymID,SYMTYPE> IDToSymTyMapTy;
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
    IDToMemMapTy		objMap;		///< map a memory sym id to its obj
    IDToSymTyMapTy	symTyMap;	/// < map a sym id to its type
    FunToIDMapTy returnSymMap;		///< return  map
    FunToIDMapTy varargSymMap;	    ///< vararg map

    CallSiteSet callSiteSet;

    // Singleton pattern here to enable instance of SymbolTableInfo can only be created once.
    static SymbolTableInfo* symInfo;

    /// Module
    SVFModule* mod;

    /// Max field limit
    static u32_t maxFieldLimit;

    /// Invoke llvm passes to modify module
    void prePassSchedule(SVFModule* svfModule);

    /// Clean up memory
    void destroy();

    /// Whether to model constants
    bool modelConstants;

    /// total number of symbols
    SymID totalSymNum;

protected:
    /// Constructor
    SymbolTableInfo(void) :
        modelConstants(false), maxStruct(nullptr), maxStSize(0), mod(nullptr), totalSymNum(0)
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

    /// Start building memory model
    void buildMemModel(SVFModule* svfModule);

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

    inline void createBlkOrConstantObj(SymID symId)
    {
        assert(isBlkObjOrConstantObj(symId));
        assert(objMap.find(symId)==objMap.end());
        objMap[symId] = new MemObj(symId);;
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

    /// Can only be invoked by PAG::addDummyNode() when creaing PAG from file.
    inline const MemObj* createDummyObj(SymID symId, const Type* type)
    {
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

    /// find the unique defined global across multiple modules
    inline const Value* getGlobalRep(const Value* val) const
    {
        if(const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val))
        {
            if (LLVMModuleSet::getLLVMModuleSet()->hasGlobalRep(gvar))
                val = LLVMModuleSet::getLLVMModuleSet()->getGlobalRep(gvar);
        }
        return val;
    }

    inline SymID getObjSym(const Value *val) const
    {
        ValueToIDMapTy::const_iterator iter = objSymMap.find(getGlobalRep(val));
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
    const inline std::vector<u32_t>& getFattenFieldIdxVec(const Type *T)
    {
        return getStructInfoIter(T)->second->getFieldIdxVec();
    }
    const inline std::vector<u32_t>& getFattenFieldOffsetVec(const Type *T)
    {
        return getStructInfoIter(T)->second->getFieldOffsetVec();
    }
    const inline std::vector<FieldInfo>& getFlattenFieldInfoVec(const Type *T)
    {
        return getStructInfoIter(T)->second->getFlattenFieldInfoVec();
    }
    const inline Type* getOrigSubTypeWithFldInx(const Type* baseType, u32_t field_idx)
    {
        return getStructInfoIter(baseType)->second->getFieldTypeWithFldIdx(field_idx);
    }
    const inline Type* getOrigSubTypeWithByteOffset(const Type* baseType, u32_t byteOffset)
    {
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
 * Bytes/bits-level modeling of memory locations to handle weakly type languages.
 * (declared with one type but accessed as another)
 * Abstract memory objects are created according to the static allocated size.
 */
class LocSymTableInfo : public SymbolTableInfo
{

public:
    /// Constructor
    LocSymTableInfo()
    {
    }
    /// Destructor
    virtual ~LocSymTableInfo()
    {
    }
    /// Compute gep offset
    virtual bool computeGepOffset(const User *V, LocationSet& ls);
    /// Given an offset from a Gep Instruction, return it modulus offset by considering memory layout
    virtual LocationSet getModulusOffset(const MemObj* obj, const LocationSet& ls);

    /// Verify struct size construction
    void verifyStructSize(StInfo *stInfo, u32_t structSize);

protected:
    /// Collect the struct info
    virtual void collectStructInfo(const StructType *T);
    /// Collect the array info
    virtual void collectArrayInfo(const ArrayType *T);
};


class LocObjTypeInfo : public ObjTypeInfo
{

public:
    /// Constructor
    LocObjTypeInfo(const Value* val, Type* t, Size_t max) : ObjTypeInfo(val,t,max)
    {

    }
    /// Destructor
    virtual ~LocObjTypeInfo()
    {

    }
    /// Get the size of this object
    u32_t getObjSize(const Value* val);

};

} // End namespace SVF

#endif /* INCLUDE_SVF_FE_SYMBOLTABLEINFO_H_ */
