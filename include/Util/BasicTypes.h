//===- BasicTypes.h -- Basic types used in SVF-------------------------------//
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
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef BASICTYPES_H_
#define BASICTYPES_H_

#include "Util/SVFBasicTypes.h"
#include "Graphs/GraphPrinter.h"
#include "Util/Casting.h"
#include <llvm/IR/Instructions.h>

namespace SVF
{

/// LLVM Basic classes
typedef llvm::Type Type;
typedef llvm::Value Value;

/// LLVM outputs
typedef llvm::raw_string_ostream raw_string_ostream;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::FunctionType FunctionType;
typedef llvm::IntegerType IntegerType;

/// LLVM Aliases and constants
typedef llvm::GraphPrinter GraphPrinter;


class SVFInstruction;
class SVFBasicBlock;
class SVFArgument;
class SVFFunction;
class SVFType;


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
    inline u32_t getNumOfFlattenElements() const
    {
        return numOfFlattenElements;
    }

    /// Return the number of fields after flattenning (ignoring array elements)
    inline u32_t getNumOfFlattenFields() const
    {
        return numOfFlattenFields;
    }
    /// Return the stride
    inline u32_t getStride() const
    {
        return stride;
    }
};

class SVFType
{

public:
    typedef s64_t GNodeK;

    enum SVFTyKind
    {
        SVFTy,
        SVFPointerTy,
        SVFIntergerTy,
        SVFFunctionTy,
        SVFStructTy,
        SVFArrayTy,
        SVFOtherTy,
    };

private:
    GNodeK kind;	///< used for classof 
    const Type* type;   ///< LLVM type
    const StInfo* typeinfo; /// < SVF's TypeInfo
    std::string toStr;  ///< string format of the type

protected:
    SVFType(const Type* ty, const StInfo* ti, SVFTyKind k) : kind(k), type(ty), typeinfo(ti), toStr("")
    {
    }

public:
    SVFType(void) = delete;
    virtual ~SVFType() = default;

    inline GNodeK getKind() const
    {
        return kind;
    }

    inline virtual const std::string toString() const
    {
        return toStr;
    }

    inline const StInfo*  getTypeInfo() const
    {
        return typeinfo;
    }

    inline const Type* getLLVMType() const
    {
        return type;
    }
};

class SVFPointerType : public SVFType
{
public:
    SVFPointerType(const Type* ty, const StInfo* ti) : SVFType(ty, ti, SVFPointerTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFPointerTy;
    }
};

class SVFIntergerType : public SVFType
{
public:
    SVFIntergerType(const Type* ty, const StInfo* ti) : SVFType(ty, ti, SVFIntergerTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFIntergerTy;
    }
};

class SVFFunctionType : public SVFType
{
public:
    SVFFunctionType(const Type* ty, const StInfo* ti) : SVFType(ty, ti, SVFFunctionTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFFunctionTy;
    }
};

class SVFStructType : public SVFType
{
public:
    SVFStructType(const Type* ty, const StInfo* ti) : SVFType(ty, ti, SVFStructTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFStructTy;
    }
};

class SVFArrayType : public SVFType
{
public:
    SVFArrayType(const Type* ty, const StInfo* ti) : SVFType(ty, ti, SVFArrayTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFArrayTy;
    }
};

class SVFOtherType : public SVFType
{
public:
    SVFOtherType(const Type* ty, const StInfo* ti) : SVFType(ty, ti, SVFOtherTy)
    {
    }
    static inline bool classof(const SVFType *node)
    {
        return node->getKind() == SVFOtherTy;
    }
};

class SVFLoopAndDomInfo
{
public:
    typedef Set<const SVFBasicBlock*> BBSet;
    typedef std::vector<const SVFBasicBlock*> BBList;
    typedef BBList LoopBBs;

private:
    BBList reachableBBs;    ///< reachable BasicBlocks from the function entry.
    Map<const SVFBasicBlock*,BBSet> dtBBsMap;   ///< map a BasicBlock to BasicBlocks it Dominates 
    Map<const SVFBasicBlock*,BBSet> pdtBBsMap;   ///< map a BasicBlock to BasicBlocks it PostDominates 
    Map<const SVFBasicBlock*,BBSet> dfBBsMap;    ///< map a BasicBlock to its Dominate Frontier BasicBlocks
    Map<const SVFBasicBlock*, LoopBBs> bb2LoopMap;  ///< map a BasicBlock (if it is in a loop) to all the BasicBlocks in this loop 
public:
    SVFLoopAndDomInfo()
    {
    }

    virtual ~SVFLoopAndDomInfo() {}

    inline const Map<const SVFBasicBlock*,BBSet>& getDomFrontierMap() const
    {
        return dfBBsMap;
    }

    inline Map<const SVFBasicBlock*,BBSet>& getDomFrontierMap()
    {
        return dfBBsMap;
    }

    inline bool hasLoopInfo(const SVFBasicBlock* bb) const
    {
        return bb2LoopMap.find(bb)!=bb2LoopMap.end();
    }

    const LoopBBs& getLoopInfo(const SVFBasicBlock* bb) const;

    inline const SVFBasicBlock* getLoopHeader(const LoopBBs& lp) const 
    {
        assert(!lp.empty() && "this is not a loop, empty basic block");
        return lp.front();
    }

    inline bool loopContainsBB(const LoopBBs& lp, const SVFBasicBlock* bb) const
    {
        return std::find(lp.begin(), lp.end(), bb)!=lp.end();
    }

    inline void addToBB2LoopMap(const SVFBasicBlock* bb, const SVFBasicBlock* loopBB)
    {
        bb2LoopMap[bb].push_back(loopBB);
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getPostDomTreeMap() const
    {
        return pdtBBsMap;
    }

    inline Map<const SVFBasicBlock*,BBSet>& getPostDomTreeMap()
    {
        return pdtBBsMap;
    }

    inline Map<const SVFBasicBlock*,BBSet>& getDomTreeMap()
    {
        return dtBBsMap;
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getDomTreeMap() const
    {
        return dtBBsMap;
    }

    inline bool isUnreachable(const SVFBasicBlock* bb) const
    {
        return std::find(reachableBBs.begin(), reachableBBs.end(), bb)==reachableBBs.end();
    }
    
    inline const BBList& getReachableBBs() const
    {
        return this->reachableBBs;
    }

    inline const void setReachableBBs(BBList& reachableBBs)
    {
        this->reachableBBs = reachableBBs;
    }
    
    void getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const;

    bool isLoopHeader(const SVFBasicBlock* bb) const;

    bool dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;

    bool postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;
};

class SVFValue
{
friend class LLVMModuleSet;

public:
    typedef s64_t GNodeK;

    enum SVFValKind
    {
        SVFVal,
        SVFFunc,
        SVFBB,
        SVFInst,
        SVFCall,
        SVFVCall,
        SVFGlob,
        SVFArg,
        SVFConst,
        SVFConstData,
        SVFConstInt,
        SVFConstFP,
        SVFNullPtr,
        SVFBlackHole,
        SVFMetaAsValue,
        SVFOther
    };

private:
    const Value* value;
    const Type* type;   ///< Type of this SVFValue
    GNodeK kind;	///< used for classof 
    bool ptrInUncalledFun;  ///< true if this pointer is in an uncalled function
    bool has_name;          ///< true if this value has a name
    bool constDataOrAggData;    ///< true if this value is a ConstantData (e.g., numbers, string, floats) or a constantAggregate

protected:
    std::string name;
    std::string sourceLoc;
    /// Constructor
    SVFValue(const Value* val, SVFValKind k): value(val), type(val->getType()), kind(k),
        ptrInUncalledFun(false), has_name(false), constDataOrAggData(SVFConstData==k), 
        name(val->getName()), sourceLoc("No source code Info")
    {
    }

    ///@{ attributes to be set only through Module builders e.g., LLVMModule
    inline void setConstDataOrAggData()
    {
        constDataOrAggData = true;
    }
    inline void setPtrInUncalledFunction()
    {
        ptrInUncalledFun = true;
    }
    inline void setHasName()
    {
        has_name = true;
    }
    ///@}
public:
    SVFValue(void) = delete;
    virtual ~SVFValue() = default;

    /// Get the type of this SVFValue
    inline GNodeK getKind() const
    {
        return kind;
    }

    /// Add the hash function for std::set (we also can overload operator< to implement this)
    //  and duplicated elements in the set are not inserted (binary tree comparison)
    //@{
    bool operator()(const SVFValue* lhs, const SVFValue* rhs) const
    {
        return lhs->value < rhs->value;
    }

    inline bool operator==(SVFValue* rhs) const
    {
        return value == rhs->value;
    }

    inline bool operator!=(SVFValue* rhs) const
    {
        return value != rhs->value;
    }
    //@}

    inline virtual const std::string getName() const
    {
        return name;
    }

    inline virtual const Type* getType() const
    {
        return type;
    }

    inline const Value* getLLVMValue() const
    {
        return value;
    }
    inline bool isConstDataOrAggData() const
    {
        return constDataOrAggData;
    }
    inline bool ptrInUncalledFunction() const
    {
        return ptrInUncalledFun;
    }
    inline bool isblackHole() const
    {
        return getKind() == SVFBlackHole;;
    }
    inline bool isNullPtr() const
    {
        return getKind() == SVFNullPtr;
    }
    inline bool hasName() const
    {
        return has_name;
    }
    inline virtual void setSourceLoc(const std::string& sourceCodeInfo)
    {
        sourceLoc = sourceCodeInfo;
    }
    inline virtual const std::string& getSourceLoc() const
    {
        return sourceLoc;
    }
    inline void setToString(const std::string& ts)
    {
        name = ts;
    }
    inline virtual const std::string& toString() const
    {
        return name;
    }
    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const SVFValue &node)
    {
        o << node.toString();
        return o;
    }
    //@}
};

class SVFFunction : public SVFValue
{
friend class LLVMModuleSet;

public:
    typedef std::vector<const SVFBasicBlock*>::const_iterator const_iterator;
    typedef SVFLoopAndDomInfo::BBSet BBSet;
    typedef SVFLoopAndDomInfo::BBList BBList;
    typedef SVFLoopAndDomInfo::LoopBBs LoopBBs;

private:
    bool isDecl;   /// return true if this function does not have a body
    bool intricsic; /// return true if this function is an intricsic function (e.g., llvm.dbg), which does not reside in the application code
    bool addrTaken; /// return true if this function is address-taken (for indirect call purposes)
    bool isUncalled;    /// return true if this function is never called
    bool isNotRet;   /// return true if this function never returns
    bool varArg;    /// return true if this function supports variable arguments 
    const FunctionType* funType;    /// the type of this function
    SVFLoopAndDomInfo* loopAndDom;  /// the loop and dominate information
    const SVFFunction* realDefFun;  /// the definition of a function across multiple modules
    std::vector<const SVFBasicBlock*> allBBs;   /// all BasicBlocks of this function
    std::vector<const SVFArgument*> allArgs;    /// all formal arguments of this function


protected:
    ///@{ attributes to be set only through Module builders e.g., LLVMModule
    inline void addBasicBlock(const SVFBasicBlock* bb)
    {
        allBBs.push_back(bb);
    }

    inline void addArgument(SVFArgument* arg)
    {
        allArgs.push_back(arg);
    }

    inline void setIsUncalledFunction(bool isUncalledFunction)
    {
        this->isUncalled = isUncalledFunction;
    }

    inline void setIsNotRet(bool doesNotRet)
    {
        this->isNotRet = doesNotRet;
    }

    inline void setDefFunForMultipleModule(const SVFFunction* deffun)
    {
        realDefFun = deffun;
    }
    /// @}

public:
    SVFFunction(const Value* f, bool declare, bool intricsic, bool addrTaken, bool varg, const FunctionType* ft, SVFLoopAndDomInfo* ld);
    SVFFunction(const Value* f) = delete;
    SVFFunction(void) = delete;
    virtual ~SVFFunction();

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFFunc;
    }

    inline SVFLoopAndDomInfo* getLoopAndDomInfo()
    {
        return loopAndDom;
    }
    inline bool isDeclaration() const
    {
        return isDecl;
    }

    inline bool isIntrinsic() const
    {
        return intricsic;
    }

    inline bool hasAddressTaken() const
    {
        return addrTaken;
    }

    /// Returns the FunctionType
    inline const FunctionType* getFunctionType() const 
    {
        return funType;
    }

    /// Returns the FunctionType
    inline const Type* getReturnType() const 
    {
        return funType->getReturnType();
    }

    inline const SVFFunction* getDefFunForMultipleModule() const
    {
        if(realDefFun==nullptr)
            return this;
        return realDefFun;
    }

    u32_t arg_size() const;
    const SVFArgument* getArg(u32_t idx) const;
    bool isVarArg() const;

    inline bool hasBasicBlock() const
    {
        return !allBBs.empty();
    }

    inline const SVFBasicBlock* getEntryBlock() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        return allBBs.front();
    }

    inline const SVFBasicBlock* getExitBB() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        return allBBs.back();
    }

    inline const SVFBasicBlock* front() const
    {
        return getEntryBlock();
    }
    
    inline const SVFBasicBlock* back() const
    {
        return getExitBB();
    }

    inline const_iterator begin() const
    {
        return allBBs.begin();
    }

    inline const_iterator end() const
    {
        return allBBs.end();
    }

    inline const std::vector<const SVFBasicBlock*>& getBasicBlockList() const
    {
        return allBBs;
    }

    inline const std::vector<const SVFBasicBlock*>& getReachableBBs() const
    {
        return loopAndDom->getReachableBBs();
    }

    inline bool isUncalledFunction() const
    {
        return isUncalled;
    }

    inline bool isNotRetFunction() const
    {
        return isNotRet;
    }

    inline void getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const
    {
        return loopAndDom->getExitBlocksOfLoop(bb,exitbbs);
    }

    inline bool hasLoopInfo(const SVFBasicBlock* bb) const
    {
        return loopAndDom->hasLoopInfo(bb);
    }

    const LoopBBs& getLoopInfo(const SVFBasicBlock* bb) const
    {
        return loopAndDom->getLoopInfo(bb);
    }

    inline const SVFBasicBlock* getLoopHeader(const BBList& lp) const 
    {
        return loopAndDom->getLoopHeader(lp);
    }

    inline bool loopContainsBB(const BBList& lp, const SVFBasicBlock* bb) const
    {
        return loopAndDom->loopContainsBB(lp,bb);
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getDomTreeMap() const
    {
        return loopAndDom->getDomTreeMap();
    }

    inline const Map<const SVFBasicBlock*,BBSet>& getDomFrontierMap() const
    {
        return loopAndDom->getDomFrontierMap();
    }

    inline bool isLoopHeader(const SVFBasicBlock* bb) const
    {
        return loopAndDom->isLoopHeader(bb);
    }

    inline bool dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
    {
        return loopAndDom->dominate(bbKey,bbValue);
    }

    inline bool postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const
    {
        return loopAndDom->postDominate(bbKey,bbValue);
    }

};

class SVFBasicBlock : public SVFValue
{
friend class LLVMModuleSet;

public:
    typedef std::vector<const SVFInstruction*>::const_iterator const_iterator;

private:
    std::vector<const SVFInstruction*> allInsts;    ///< all Instructions in this BasicBlock
    std::vector<const SVFBasicBlock*> succBBs;  ///< all successor BasicBlocks of this BasicBlock
    std::vector<const SVFBasicBlock*> predBBs;  ///< all predecessor BasicBlocks of this BasicBlock
    const SVFFunction* fun;                 /// Function where this BasicBlock is

protected:
    ///@{ attributes to be set only through Module builders e.g., LLVMModule
    inline void addInstruction(const SVFInstruction* inst)
    {
        allInsts.push_back(inst);
    }

    inline void addSuccBasicBlock(const SVFBasicBlock* succ)
    {
        succBBs.push_back(succ);
    }

    inline void addPredBasicBlock(const SVFBasicBlock* pred)
    {
        predBBs.push_back(pred);
    }
    /// @}

public:
    SVFBasicBlock(const Value* b, const SVFFunction* f);
    SVFBasicBlock(void) = delete;
    virtual ~SVFBasicBlock();

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFBB;
    }

    inline const std::vector<const SVFInstruction*>& getInstructionList() const
    {
        return allInsts;
    }

    inline const_iterator begin() const
    {
        return allInsts.begin();
    }

    inline const_iterator end() const
    {
        return allInsts.end();
    }

    inline const SVFFunction* getParent() const
    {
        return fun;
    }

    inline const SVFFunction* getFunction() const
    {
        return fun;
    }

    inline const SVFInstruction* front() const
    {
        return allInsts.front();
    }

    inline const SVFInstruction* back() const
    {
        return allInsts.back();
    }

    /// Returns the terminator instruction if the block is well formed or null
    /// if the block is not well formed.
    const SVFInstruction* getTerminator() const;

    inline const std::vector<const SVFBasicBlock*>& getSuccessors() const
    {
        return succBBs;
    }

    inline const std::vector<const SVFBasicBlock*>& getPredecessors() const
    {
        return predBBs;
    }
    u32_t getNumSuccessors() const
    {
        return succBBs.size();
    }
    u32_t getBBSuccessorPos(const SVFBasicBlock* succbb);
    u32_t getBBSuccessorPos(const SVFBasicBlock* succbb) const;
    u32_t getBBPredecessorPos(const SVFBasicBlock* succbb);
    u32_t getBBPredecessorPos(const SVFBasicBlock* succbb) const;
};

class SVFInstruction : public SVFValue
{
public:
    typedef std::vector<const SVFInstruction*> InstVec;

private:
    const SVFBasicBlock* bb;    /// The BasicBlock where this Instruction resides
    bool terminator;    /// return true if this is a terminator instruction
    bool ret;           /// return true if this is an return instruction of a function
    InstVec succInsts;  /// successor Instructions
    InstVec predInsts;  /// predecessor Instructions

public:
    SVFInstruction(const Value* i, const SVFBasicBlock* b, bool tm, bool isRet, SVFValKind k = SVFInst);
    SVFInstruction(const Value* i) = delete;
    SVFInstruction(void) = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFInst || 
               node->getKind() == SVFCall || 
               node->getKind() == SVFVCall;
    }

    inline const SVFBasicBlock* getParent() const
    {
        return bb;
    }
    
    inline InstVec& getSuccInstructions()
    {
        return succInsts;
    }

    inline InstVec& getPredInstructions()
    {
        return predInsts;
    }

    inline const InstVec& getSuccInstructions() const
    {
        return succInsts;
    }

    inline const InstVec& getPredInstructions() const
    {
        return predInsts;
    }

    inline const SVFFunction* getFunction() const
    {
        return bb->getParent();
    }

    inline bool isTerminator() const
    {
        return terminator;
    }

    inline bool isRetInst() const
    {
        return ret;
    }
};

class SVFCallInst : public SVFInstruction
{
friend class LLVMModuleSet;

private:
    std::vector<const SVFValue*> args;
    const FunctionType* calledFunType;
    const SVFValue* calledVal;

protected:
    ///@{ attributes to be set only through Module builders e.g., LLVMModule
    inline void addArgument(const SVFValue* a)
    {
        args.push_back(a);
    }
    inline void setCalledOperand(const SVFValue* v)
    {
        calledVal = v;
    }
    /// @}

public:
    SVFCallInst(const Value* i, const SVFBasicBlock* b, const FunctionType* t, bool tm, SVFValKind k = SVFCall) : 
        SVFInstruction(i,b,tm,false,k), calledFunType(t), calledVal(nullptr)
    {
    }
    SVFCallInst(const Value* i) = delete;
    SVFCallInst(void) = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFCall || node->getKind() == SVFVCall;
    }
    static inline bool classof(const SVFInstruction *node)
    {
        return node->getKind() == SVFCall || node->getKind() == SVFVCall;
    }
    inline u32_t arg_size() const
    {
        return args.size();
    }
    inline bool arg_empty() const
    {
        return args.empty();
    }
    inline const SVFValue* getArgOperand(u32_t i) const
    {
        assert(i < arg_size() && "out of bound access of the argument");
        return args[i];
    }
    inline u32_t getNumArgOperands() const
    {
        return arg_size();
    }
    inline const SVFValue* getCalledOperand() const
    {
        return calledVal;
    }
    inline const FunctionType* getFunctionType() const
    {
        return calledFunType;
    }
    inline const SVFFunction* getCalledFunction() const
    {
        return SVFUtil::dyn_cast<SVFFunction>(calledVal);
    }
    inline  const SVFFunction* getCaller() const
    {
        return getFunction();
    }
};

class SVFVirtualCallInst : public SVFCallInst
{
friend class LLVMModuleSet;

private:
    const SVFValue* vCallVtblPtr;   /// virtual table pointer
    s32_t virtualFunIdx;            /// virtual function index of the virtual table(s) at a virtual call
    std::string funNameOfVcall;     /// the function name of this virtual call

protected:
    inline void setFunIdxInVtable(s32_t idx)
    {
        virtualFunIdx = idx;
    }
    inline void setFunNameOfVirtualCall(const std::string& name)
    {
        funNameOfVcall = name;
    }
    inline void setVtablePtr(const SVFValue* vptr)
    {
        vCallVtblPtr = vptr;
    }

public:
    SVFVirtualCallInst(const Value* i, const SVFBasicBlock* b, const FunctionType* t, bool tm) : 
        SVFCallInst(i,b,t,tm, SVFVCall), vCallVtblPtr(nullptr), virtualFunIdx(-1), funNameOfVcall("")
    {
    }
    inline const SVFValue* getVtablePtr() const
    {
        assert(vCallVtblPtr && "virtual call does not have a vtblptr? set it first");
        return vCallVtblPtr;
    }
    inline s32_t getFunIdxInVtable() const
    {
        assert(virtualFunIdx >=0 && "virtual function idx is less than 0? not set yet?");
        return virtualFunIdx;
    }
    inline const std::string& getFunNameOfVirtualCall() const
    {
        return funNameOfVcall;
    }
    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFVCall;
    }
    static inline bool classof(const SVFInstruction *node)
    {
        return node->getKind() == SVFVCall;
    }
    static inline bool classof(const SVFCallInst *node)
    {
        return node->getKind() == SVFVCall;
    }
};

class SVFConstant : public SVFValue
{
public:
    SVFConstant(const Value* _const, SVFValKind k = SVFConst): SVFValue(_const, k)
    {
    }
    SVFConstant() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFConst || 
               node->getKind() == SVFGlob ||
               node->getKind() == SVFConstData || 
               node->getKind() == SVFConstInt || 
               node->getKind() == SVFConstFP ||
               node->getKind() == SVFNullPtr ||
               node->getKind() == SVFBlackHole;
    }
};

class SVFGlobalValue : public SVFConstant
{
friend class LLVMModuleSet;

private:
    const SVFValue* realDefGlobal;  /// the definition of a function across multiple modules

protected:
    inline void setDefGlobalForMultipleModule(const SVFValue* defg)
    {
        realDefGlobal = defg;
    }

public:
    SVFGlobalValue(const Value* _gv): SVFConstant(_gv, SVFValue::SVFGlob), realDefGlobal(nullptr)
    {
    }
    SVFGlobalValue() = delete;


    inline const SVFValue* getDefGlobalForMultipleModule() const
    {
        if(realDefGlobal==nullptr)
            return this;
        return realDefGlobal;
    }
    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFGlob;
    }
    static inline bool classof(const SVFConstant *node)
    {
        return node->getKind() == SVFGlob;
    }
};

class SVFArgument : public SVFValue
{
private:
    const SVFFunction* fun;
    u32_t argNo;
    bool uncalled;
public:
    SVFArgument(const Value* _arg, const SVFFunction* _fun, u32_t _argNo, bool _uncalled): SVFValue(_arg, SVFValue::SVFArg), fun(_fun), argNo(_argNo), uncalled(_uncalled)
    {
    }
    SVFArgument() = delete;

    inline const SVFFunction* getParent() const
    {
        return fun;
    }

    ///  Return the index of this formal argument in its containing function.
    /// For example in "void foo(int a, float b)" a is 0 and b is 1.
    inline u32_t getArgNo() const
    {
        return argNo;
    }

    inline bool isArgOfUncalledFunction() const
    {
        return uncalled;
    }

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFArg;
    }
};



class SVFConstantData : public SVFConstant
{
public:
    SVFConstantData(const Value* _const, SVFValKind k = SVFConstData): SVFConstant(_const, k)
    {
    }
    SVFConstantData() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFConstData || 
               node->getKind() == SVFConstInt || 
               node->getKind() == SVFConstFP ||
               node->getKind() == SVFNullPtr ||
               node->getKind() == SVFBlackHole;
    }
    static inline bool classof(const SVFConstantData *node)
    {
        return node->getKind() == SVFConstData || 
               node->getKind() == SVFConstInt || 
               node->getKind() == SVFConstFP ||
               node->getKind() == SVFNullPtr ||
               node->getKind() == SVFBlackHole;
    }
};


class SVFConstantInt : public SVFConstantData
{
private:
    u64_t zval;
    s64_t sval;
public:
    SVFConstantInt(const Value* _const, u64_t z, s64_t s): SVFConstantData(_const, SVFValue::SVFConstInt), zval(z), sval(s)
    {
    }
    SVFConstantInt() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFConstInt;
    }
    static inline bool classof(const SVFConstantData *node)
    {
        return node->getKind() == SVFConstInt;
    }
    //  Return the constant as a 64-bit unsigned integer value after it has been zero extended as appropriate for the type of this constant.
    inline u64_t getZExtValue () const
    {
        return zval;
    }
    // Return the constant as a 64-bit integer value after it has been sign extended as appropriate for the type of this constan
    inline s64_t getSExtValue () const
    {
        return sval;
    }
};


class SVFConstantFP : public SVFConstantData
{
private:
    float dval;
public:
    SVFConstantFP(const Value* _const, double d): SVFConstantData(_const, SVFValue::SVFConstFP), dval(d)
    {
    }
    SVFConstantFP() = delete;

    inline double getFPValue () const
    {
        return dval;
    }
    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFConstFP;
    }
    static inline bool classof(const SVFConstantData *node)
    {
        return node->getKind() == SVFConstFP;
    }
};


class SVFConstantNullPtr : public SVFConstantData
{

public:
    SVFConstantNullPtr(const Value* _const): SVFConstantData(_const, SVFValue::SVFNullPtr)
    {
    }
    SVFConstantNullPtr() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFNullPtr;
    }
    static inline bool classof(const SVFConstantData *node)
    {
        return node->getKind() == SVFNullPtr;
    }
};

class SVFBlackHoleValue : public SVFConstantData
{

public:
    SVFBlackHoleValue(const Value* _const): SVFConstantData(_const, SVFValue::SVFBlackHole)
    {
    }
    SVFBlackHoleValue() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFBlackHole;
    }
    static inline bool classof(const SVFConstantData *node)
    {
        return node->getKind() == SVFBlackHole;
    }
};

class SVFOtherValue : public SVFValue
{
public:
    SVFOtherValue(const Value* other, SVFValKind k = SVFValue::SVFOther): SVFValue(other, k)
    {
    }
    SVFOtherValue() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFOther;
    }
};

/*
 * This class is only for LLVM's MetadataAsValue
*/
class SVFMetadataAsValue : public SVFOtherValue
{
public:
    SVFMetadataAsValue(const Value* other): SVFOtherValue(other, SVFValue::SVFMetaAsValue)
    {
    }
    SVFMetadataAsValue() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFMetaAsValue;
    }
    static inline bool classof(const SVFOtherValue *node)
    {
        return node->getKind() == SVFMetaAsValue;
    }
};


class CallSite
{
private:
    const SVFCallInst *CB;
public:
    CallSite(const SVFInstruction *I) : CB(SVFUtil::dyn_cast<SVFCallInst>(I))
    {
        assert(CB && "not a callsite?");
    }
    const SVFInstruction* getInstruction() const
    {
        return CB;
    }
    const SVFValue* getArgument(u32_t ArgNo) const
    {
        return CB->getArgOperand(ArgNo);
    }
    const Type* getType() const
    {
        return CB->getType();
    }
    u32_t arg_size() const
    {
        return CB->arg_size();
    }
    bool arg_empty() const
    {
        return CB->arg_empty();
    }
    const SVFValue* getArgOperand(u32_t i) const
    {
        return CB->getArgOperand(i);
    }
    u32_t getNumArgOperands() const
    {
        return CB->arg_size();
    }
    const SVFFunction* getCalledFunction() const
    {
        return CB->getCalledFunction();
    }
    const SVFValue* getCalledValue() const
    {
        return CB->getCalledOperand();
    }
    const SVFFunction* getCaller() const
    {
        return CB->getCaller();
    }
    const FunctionType* getFunctionType() const
    {
        return CB->getFunctionType();
    }
    const bool isVirtualCall() const
    {
        return SVFUtil::isa<SVFVirtualCallInst>(CB);
    }
    const SVFValue* getVtablePtr() const
    {
        assert(isVirtualCall() && "not a virtual call?");
        return SVFUtil::cast<SVFVirtualCallInst>(CB)->getVtablePtr();
    }
    s32_t getFunIdxInVtable() const
    {
        assert(isVirtualCall() && "not a virtual call?");
        return SVFUtil::cast<SVFVirtualCallInst>(CB)->getFunIdxInVtable();
    }
    const std::string& getFunNameOfVirtualCall() const
    {
        assert(isVirtualCall() && "not a virtual call?");
        return SVFUtil::cast<SVFVirtualCallInst>(CB)->getFunNameOfVirtualCall();
    }
    bool operator==(const CallSite &CS) const
    {
        return CB == CS.CB;
    }
    bool operator!=(const CallSite &CS) const
    {
        return CB != CS.CB;
    }
    bool operator<(const CallSite &CS) const
    {
        return getInstruction() < CS.getInstruction();
    }

};

template <typename F, typename S>
OutStream& operator<< (OutStream &o, const std::pair<F, S> &var)
{
    o << "<" << var.first << ", " << var.second << ">";
    return o;
}

} // End namespace SVF

/// Specialise hash for CallSites.
template <> struct std::hash<SVF::CallSite>
{
    size_t operator()(const SVF::CallSite &cs) const
    {
        std::hash<const SVF::SVFInstruction *> h;
        return h(cs.getInstruction());
    }
};

#endif /* BASICTYPES_H_ */
