//===- BasicTypes.h -- Basic types used in SVF-------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVFIR_SVFVALUE_H_
#define INCLUDE_SVFIR_SVFVALUE_H_

#include "SVFIR/SVFType.h"
#include "Graphs/GraphPrinter.h"
#include "Util/Casting.h"

namespace SVF
{

/// LLVM Aliases and constants
typedef SVF::GraphPrinter GraphPrinter;


class SVFInstruction;
class SVFBasicBlock;
class SVFArgument;
class SVFFunction;
class SVFType;

class SVFLoopAndDomInfo
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
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
        return bb2LoopMap.find(bb) != bb2LoopMap.end();
    }

    const LoopBBs& getLoopInfo(const SVFBasicBlock* bb) const;

    inline const SVFBasicBlock* getLoopHeader(const LoopBBs& lp) const
    {
        assert(!lp.empty() && "this is not a loop, empty basic block");
        return lp.front();
    }

    inline bool loopContainsBB(const LoopBBs& lp, const SVFBasicBlock* bb) const
    {
        return std::find(lp.begin(), lp.end(), bb) != lp.end();
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
        return std::find(reachableBBs.begin(), reachableBBs.end(), bb) ==
               reachableBBs.end();
    }

    inline const BBList& getReachableBBs() const
    {
        return reachableBBs;
    }

    inline void setReachableBBs(BBList& bbs)
    {
        reachableBBs = bbs;
    }

    void getExitBlocksOfLoop(const SVFBasicBlock* bb, BBList& exitbbs) const;

    bool isLoopHeader(const SVFBasicBlock* bb) const;

    bool dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;

    bool postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;
};

class SVFValue
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
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
    GNodeK kind;	///< used for classof
    bool ptrInUncalledFun;  ///< true if this pointer is in an uncalled function
    bool constDataOrAggData;    ///< true if this value is a ConstantData (e.g., numbers, string, floats) or a constantAggregate

protected:
    const SVFType* type;   ///< Type of this SVFValue
    std::string name;       ///< Short name of value for printing & debugging
    std::string sourceLoc;  ///< Source code information of this value
    /// Constructor without name
    SVFValue(const SVFType* ty, SVFValKind k)
        : kind(k), ptrInUncalledFun(false),
          constDataOrAggData(SVFConstData == k), type(ty), sourceLoc("NoLoc")
    {
    }

    ///@{ attributes to be set only through Module builders e.g.,
    /// LLVMModule
    inline void setConstDataOrAggData()
    {
        constDataOrAggData = true;
    }
    inline void setPtrInUncalledFunction()
    {
        ptrInUncalledFun = true;
    }
    ///@}
public:
    SVFValue() = delete;
    virtual ~SVFValue() = default;

    /// Get the type of this SVFValue
    inline GNodeK getKind() const
    {
        return kind;
    }

    inline const std::string &getName() const
    {
        return name;
    }
    inline void setName(const std::string& n)
    {
        name = n;
    }
    inline void setName(std::string&& n)
    {
        name = std::move(n);
    }

    inline virtual const SVFType* getType() const
    {
        return type;
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
    inline virtual void setSourceLoc(const std::string& sourceCodeInfo)
    {
        sourceLoc = sourceCodeInfo;
    }
    inline virtual const std::string getSourceLoc() const
    {
        return sourceLoc;
    }

    /// Needs to be implemented by a SVF front end
    std::string toString() const;

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<<(OutStream &os, const SVFValue &value)
    {
        return os << value.toString();
    }
    //@}
};

class SVFFunction : public SVFValue
{
    friend class LLVMModuleSet;
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;

public:
    typedef std::vector<const SVFBasicBlock*>::const_iterator const_iterator;
    typedef SVFLoopAndDomInfo::BBSet BBSet;
    typedef SVFLoopAndDomInfo::BBList BBList;
    typedef SVFLoopAndDomInfo::LoopBBs LoopBBs;

private:
    bool isDecl;   /// return true if this function does not have a body
    bool intrinsic; /// return true if this function is an intrinsic function (e.g., llvm.dbg), which does not reside in the application code
    bool addrTaken; /// return true if this function is address-taken (for indirect call purposes)
    bool isUncalled;    /// return true if this function is never called
    bool isNotRet;   /// return true if this function never returns
    bool varArg;    /// return true if this function supports variable arguments
    const SVFFunctionType* funcType; /// FunctionType, which is different from the type (PointerType) of this SVFFunction
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

    inline void setIsUncalledFunction(bool uncalledFunction)
    {
        isUncalled = uncalledFunction;
    }

    inline void setIsNotRet(bool notRet)
    {
        isNotRet = notRet;
    }

    inline void setDefFunForMultipleModule(const SVFFunction* deffun)
    {
        realDefFun = deffun;
    }
    /// @}

public:
    SVFFunction(const SVFType* ty,const SVFFunctionType* ft, bool declare, bool intrinsic, bool addrTaken, bool varg, SVFLoopAndDomInfo* ld);
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
        return intrinsic;
    }

    inline bool hasAddressTaken() const
    {
        return addrTaken;
    }

    /// Returns the FunctionType
    inline const SVFFunctionType* getFunctionType() const
    {
        return funcType;
    }

    /// Returns the FunctionType
    inline const SVFType* getReturnType() const
    {
        return funcType->getReturnType();
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;

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
    /// Constructor without name
    SVFBasicBlock(const SVFType* ty, const SVFFunction* f);
    SVFBasicBlock() = delete;
    ~SVFBasicBlock() override;

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
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    typedef std::vector<const SVFInstruction*> InstVec;

private:
    const SVFBasicBlock* bb;    /// The BasicBlock where this Instruction resides
    bool terminator;    /// return true if this is a terminator instruction
    bool ret;           /// return true if this is an return instruction of a function
    InstVec succInsts;  /// successor Instructions
    InstVec predInsts;  /// predecessor Instructions

public:
    /// Constructor without name, set name with setName()
    SVFInstruction(const SVFType* ty, const SVFBasicBlock* b, bool tm,
                   bool isRet, SVFValKind k = SVFInst);
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class LLVMModuleSet;
    friend class SVFIRBuilder;

private:
    std::vector<const SVFValue*> args;
    bool varArg;
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
    SVFCallInst(const SVFType* ty, const SVFBasicBlock* b, bool va, bool tm, SVFValKind k = SVFCall) :
        SVFInstruction(ty, b, tm, false, k), varArg(va), calledVal(nullptr)
    {
    }
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
    inline bool isVarArg() const
    {
        return varArg;
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
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
    SVFVirtualCallInst(const SVFType* ty, const SVFBasicBlock* b, bool vararg,
                       bool tm)
        : SVFCallInst(ty, b, vararg, tm, SVFVCall), vCallVtblPtr(nullptr),
          virtualFunIdx(-1), funNameOfVcall()
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    SVFConstant(const SVFType* ty, SVFValKind k = SVFConst): SVFValue(ty, k)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class LLVMModuleSet;

private:
    const SVFValue* realDefGlobal;  /// the definition of a function across multiple modules

protected:
    inline void setDefGlobalForMultipleModule(const SVFValue* defg)
    {
        realDefGlobal = defg;
    }

public:
    SVFGlobalValue(const SVFType* ty): SVFConstant(ty, SVFValue::SVFGlob), realDefGlobal(nullptr)
    {
    }
    SVFGlobalValue(std::string&& name, const SVFType* ty) : SVFGlobalValue(ty)
    {
        setName(std::move(name));
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    const SVFFunction* fun;
    u32_t argNo;
    bool uncalled;
public:
    SVFArgument(const SVFType* ty, const SVFFunction* fun, u32_t argNo,
                bool uncalled)
        : SVFValue(ty, SVFValue::SVFArg), fun(fun), argNo(argNo),
          uncalled(uncalled)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    SVFConstantData(const SVFType* ty, SVFValKind k = SVFConstData)
        : SVFConstant(ty, k)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    u64_t zval;
    s64_t sval;
public:
    SVFConstantInt(const SVFType* ty, u64_t z, s64_t s)
        : SVFConstantData(ty, SVFValue::SVFConstInt), zval(z), sval(s)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
private:
    float dval;
public:
    SVFConstantFP(const SVFType* ty, double d)
        : SVFConstantData(ty, SVFValue::SVFConstFP), dval(d)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    SVFConstantNullPtr(const SVFType* ty)
        : SVFConstantData(ty, SVFValue::SVFNullPtr)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    SVFBlackHoleValue(const SVFType* ty)
        : SVFConstantData(ty, SVFValue::SVFBlackHole)
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
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    SVFOtherValue(const SVFType* ty, SVFValKind k = SVFValue::SVFOther)
        : SVFValue(ty, k)
    {
    }
    SVFOtherValue() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFOther || node->getKind() == SVFMetaAsValue;
    }
};

/*
 * This class is only for LLVM's MetadataAsValue
*/
class SVFMetadataAsValue : public SVFOtherValue
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    SVFMetadataAsValue(const SVFType* ty)
        : SVFOtherValue(ty, SVFValue::SVFMetaAsValue)
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
    friend class SVFIRReader;

private:
    const SVFCallInst* CB;

    /// Constructs empty CallSite (for SVFIRReader/deserialization)
    CallSite() : CB{} {}

public:
    CallSite(const SVFInstruction* I) : CB(SVFUtil::dyn_cast<SVFCallInst>(I))
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
    const SVFType* getType() const
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
    bool isVarArg() const
    {
        return CB->isVarArg();
    }
    bool isVirtualCall() const
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
    bool operator==(const CallSite& CS) const
    {
        return CB == CS.CB;
    }
    bool operator!=(const CallSite& CS) const
    {
        return CB != CS.CB;
    }
    bool operator<(const CallSite& CS) const
    {
        return getInstruction() < CS.getInstruction();
    }
};

/// [FOR DEBUG ONLY, DON'T USE IT UNSIDE `svf`!]
/// Converts an SVFValue to corresponding LLVM::Value, then get the string
/// representation of it. Use it only when you are debugging. Don't use
/// it in any SVF algorithm because it relies on information stored in LLVM bc.
std::string dumpLLVMValue(const SVFValue* svfValue);

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

#endif /* INCLUDE_SVFIR_SVFVALUE_H_ */
