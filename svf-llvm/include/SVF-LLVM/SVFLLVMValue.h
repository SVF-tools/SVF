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

#ifndef INCLUDE_SVFLLVM_SVFVALUE_H_
#define INCLUDE_SVFLLVM_SVFVALUE_H_

#include "SVFIR/SVFType.h"
#include "Graphs/GraphPrinter.h"
#include "Util/Casting.h"
#include "Graphs/BasicBlockG.h"
#include "SVFIR/SVFValue.h"
#include "Util/SVFLoopAndDomInfo.h"

namespace SVF
{

/// LLVM Aliases and constants
typedef SVF::GraphPrinter GraphPrinter;

class CallGraphNode;
class SVFInstruction;
class SVFBasicBlock;
class SVFArgument;
class SVFFunction;
class SVFType;

class SVFLLVMValue
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
    SVFLLVMValue(const SVFType* ty, SVFValKind k)
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
    SVFLLVMValue() = delete;
    virtual ~SVFLLVMValue() = default;

    /// Get the type of this SVFValue
    inline GNodeK getKind() const
    {
        return kind;
    }

    inline const std::string &getName() const
    {
        return name;
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
    friend OutStream& operator<<(OutStream &os, const SVFLLVMValue &value)
    {
        return os << value.toString();
    }
    //@}
};

class ArgValVar;

class SVFFunction : public SVFLLVMValue
{
    friend class LLVMModuleSet;
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class SVFIRBuilder;

public:
    typename BasicBlockGraph::IDToNodeMapTy::iterator iterator;
    typedef BasicBlockGraph::IDToNodeMapTy::const_iterator const_iterator;
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
    std::vector<const ArgValVar*> allArgs;    /// all formal arguments of this function
    SVFBasicBlock *exitBlock;             /// a 'single' basic block having no successors and containing return instruction in a function
    BasicBlockGraph* bbGraph; /// the basic block graph of this function

protected:

    inline void addArgument(const ArgValVar* arg)
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

    static inline bool classof(const SVFLLVMValue *node)
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

    void setBasicBlockGraph(BasicBlockGraph* graph)
    {
        this->bbGraph = graph;
    }

    BasicBlockGraph* getBasicBlockGraph()
    {
        return bbGraph;
    }

    const BasicBlockGraph* getBasicBlockGraph() const
    {
        return bbGraph;
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
    const ArgValVar* getArg(u32_t idx) const;
    bool isVarArg() const;

    inline bool hasBasicBlock() const
    {
        return bbGraph && bbGraph->begin() != bbGraph->end();
    }

    inline const SVFBasicBlock* getEntryBlock() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        assert(bbGraph->begin()->second->getInEdges().size() == 0 && "the first basic block is not entry block");
        return bbGraph->begin()->second;
    }

    /// Carefully! when you call getExitBB, you need ensure the function has return instruction
    /// more refer to: https://github.com/SVF-tools/SVF/pull/1262
    const SVFBasicBlock* getExitBB() const;

    void setExitBlock(SVFBasicBlock *bb);

    inline const SVFBasicBlock* front() const
    {
        return getEntryBlock();
    }

    inline const SVFBasicBlock* back() const
    {
        assert(hasBasicBlock() && "function does not have any Basicblock, external function?");
        /// Carefully! 'back' is just the last basic block of function,
        /// but not necessarily a exit basic block
        /// more refer to: https://github.com/SVF-tools/SVF/pull/1262
        return std::prev(bbGraph->end())->second;
    }

    inline const_iterator begin() const
    {
        return bbGraph->begin();
    }

    inline const_iterator end() const
    {
        return bbGraph->end();
    }


    inline const std::vector<const SVFBasicBlock*>& getReachableBBs() const
    {
        return loopAndDom->getReachableBBs();
    }

    inline bool isUncalledFunction() const
    {
        return isUncalled;
    }

    inline bool hasReturn() const
    {
        return  !isNotRet;
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

class ICFGNode;
class FunObjVar;

class SVFInstruction : public SVFLLVMValue
{
    friend class SVFIRWriter;
    friend class SVFIRReader;

private:
    const SVFBasicBlock* bb;    /// The BasicBlock where this Instruction resides
    bool terminator;    /// return true if this is a terminator instruction
    bool ret;           /// return true if this is an return instruction of a function

public:
    /// Constructor without name, set name with setName()
    SVFInstruction(const SVFType* ty, const SVFBasicBlock* b, bool tm,
                   bool isRet, SVFValKind k = SVFInst);
    SVFInstruction(void) = delete;

    static inline bool classof(const SVFLLVMValue *node)
    {
        return node->getKind() == SVFInst ||
               node->getKind() == SVFCall ||
               node->getKind() == SVFVCall;
    }

    inline const SVFBasicBlock* getParent() const
    {
        return bb;
    }

    inline const FunObjVar* getFunction() const
    {
        return bb->getParent();
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
    std::vector<const SVFLLVMValue*> args;
    bool varArg;
    const SVFLLVMValue* calledVal;

protected:
    ///@{ attributes to be set only through Module builders e.g., LLVMModule
    inline void addArgument(const SVFLLVMValue* a)
    {
        args.push_back(a);
    }
    inline void setCalledOperand(const SVFLLVMValue* v)
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

    static inline bool classof(const SVFLLVMValue *node)
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
    inline const SVFLLVMValue* getArgOperand(u32_t i) const
    {
        assert(i < arg_size() && "out of bound access of the argument");
        return args[i];
    }
    inline u32_t getNumArgOperands() const
    {
        return arg_size();
    }
    inline const SVFLLVMValue* getCalledOperand() const
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
    inline  const FunObjVar* getCaller() const
    {
        return getFunction();
    }
};

class SVFConstant : public SVFLLVMValue
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    SVFConstant(const SVFType* ty, SVFValKind k = SVFConst): SVFLLVMValue(ty, k)
    {
    }
    SVFConstant() = delete;

    static inline bool classof(const SVFLLVMValue *node)
    {
        return node->getKind() == SVFConst ||
               node->getKind() == SVFGlob ||
               node->getKind() == SVFConstData;
    }

};

class SVFGlobalValue : public SVFConstant
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
    friend class LLVMModuleSet;

private:
    const SVFLLVMValue* realDefGlobal;  /// the definition of a function across multiple modules

protected:
    inline void setDefGlobalForMultipleModule(const SVFLLVMValue* defg)
    {
        realDefGlobal = defg;
    }

public:
    SVFGlobalValue(const SVFType* ty): SVFConstant(ty, SVFLLVMValue::SVFGlob), realDefGlobal(nullptr)
    {
    }
    SVFGlobalValue(std::string&& name, const SVFType* ty) : SVFGlobalValue(ty)
    {
        setName(std::move(name));
    }
    SVFGlobalValue() = delete;

    inline const SVFLLVMValue* getDefGlobalForMultipleModule() const
    {
        if(realDefGlobal==nullptr)
            return this;
        return realDefGlobal;
    }
    static inline bool classof(const SVFLLVMValue *node)
    {
        return node->getKind() == SVFGlob;
    }
    static inline bool classof(const SVFConstant *node)
    {
        return node->getKind() == SVFGlob;
    }
};

class SVFArgument : public SVFLLVMValue
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
        : SVFLLVMValue(ty, SVFLLVMValue::SVFArg), fun(fun), argNo(argNo),
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

    static inline bool classof(const SVFLLVMValue *node)
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

    static inline bool classof(const SVFLLVMValue *node)
    {
        return node->getKind() == SVFConstData;
    }
    static inline bool classof(const SVFConstantData *node)
    {
        return node->getKind() == SVFConstData;
    }
};

class SVFOtherValue : public SVFLLVMValue
{
    friend class SVFIRWriter;
    friend class SVFIRReader;
public:
    SVFOtherValue(const SVFType* ty, SVFValKind k = SVFLLVMValue::SVFOther)
        : SVFLLVMValue(ty, k)
    {
    }
    SVFOtherValue() = delete;

    static inline bool classof(const SVFLLVMValue *node)
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
        : SVFOtherValue(ty, SVFLLVMValue::SVFMetaAsValue)
    {
    }
    SVFMetadataAsValue() = delete;

    static inline bool classof(const SVFLLVMValue *node)
    {
        return node->getKind() == SVFMetaAsValue;
    }
    static inline bool classof(const SVFOtherValue *node)
    {
        return node->getKind() == SVFMetaAsValue;
    }
};


/// [FOR DEBUG ONLY, DON'T USE IT UNSIDE `svf`!]
/// Converts an SVFValue to corresponding LLVM::Value, then get the string
/// representation of it. Use it only when you are debugging. Don't use
/// it in any SVF algorithm because it relies on information stored in LLVM bc.
std::string dumpLLVMValue(const SVFLLVMValue* svfValue);



} // End namespace SVF

#endif /* INCLUDE_SVFLLVM_SVFVALUE_H_ */
