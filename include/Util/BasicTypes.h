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
#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable
#include <llvm/IR/BasicBlock.h>

#include "llvm/BinaryFormat/Dwarf.h"

namespace SVF
{

/// LLVM Basic classes
typedef llvm::Type Type;
typedef llvm::Function Function;
typedef llvm::BasicBlock BasicBlock;
typedef llvm::Value Value;
typedef llvm::Instruction Instruction;
typedef llvm::GlobalValue GlobalValue;

/// LLVM outputs
typedef llvm::raw_string_ostream raw_string_ostream;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::FunctionType FunctionType;
typedef llvm::IntegerType IntegerType;

/// LLVM Aliases and constants
typedef llvm::ConstantData ConstantData;

typedef llvm::GraphPrinter GraphPrinter;


class SVFInstruction;
class SVFBasicBlock;
class SVFArgument;

class SVFValue
{

public:
    typedef s64_t GNodeK;

    enum SVFValKind
    {
        SVFVal,
        SVFFunc,
        SVFBB,
        SVFInst,
        SVFCall,
        SVFGlob,
        SVFArg,
        SVFConstData,
        SVFConstInt,
        SVFConstFP,
        SVFConstNullPtr,
        SVFBlackHole,
        SVFOther
    };

private:
    const Value* value;
    const Type* type;
    GNodeK kind;	///< Type of this SVFValue
    bool ptrInUncalledFun;
    bool isPtr;
    bool has_name;
protected:
    std::string name;
    std::string sourceLoc;
    std::string toStr;
    /// Constructor
    SVFValue(const Value* val, SVFValKind k): value(val), type(val->getType()), kind(k),
        ptrInUncalledFun(false), isPtr(val->getType()->isPointerTy()),
        has_name(false), name(val->getName()), sourceLoc("No source code Info"), toStr("toString not set")
    {
    }

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

    inline const std::string getName() const
    {
        return name;
    }

    inline const Value* getLLVMValue() const
    {
        return value;
    }

    inline const Type* getType() const
    {
        return type;
    }

    inline void setPtrInUncalledFunction()
    {
        ptrInUncalledFun = true;
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
        return getKind() == SVFConstNullPtr;
    }
    inline void setHasName()
    {
        has_name = true;
    }
    inline bool hasName() const
    {
        return has_name;
    }
    inline void setSourceLoc(const std::string& sourceCodeInfo)
    {
        sourceLoc = sourceCodeInfo;
    }
    inline const std::string& getSourceLoc() const
    {
        return sourceLoc;
    }
    inline void setToString(const std::string& str)
    {
        toStr = str;
    }
    inline const std::string& toString() const
    {
        return toStr;
    }
    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const SVFValue &node)
    {
        o << node.getName();
        return o;
    }
    //@}
};

class SVFFunction : public SVFValue
{

public:
    typedef std::vector<const SVFBasicBlock*>::const_iterator const_iterator;

private:
    bool isDecl;
    bool intricsic;
    const SVFFunction* realDefFun;  /// the definition of a function across multiple modules
    const SVFBasicBlock* exitBB;
    std::vector<const SVFBasicBlock*> reachableBBs;
    std::vector<const SVFBasicBlock*> allBBs;
    std::vector<const SVFArgument*> allArgs;

    bool isUncalled;
    bool isNotRet;
    bool varArg;
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> dtBBsMap;
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> dfBBsMap;
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> pdtBBsMap;
    Map<const SVFBasicBlock*,std::vector<const SVFBasicBlock*>> bb2LoopMap;

public:

    SVFFunction(const Function* f, bool declare, bool intricsic);
    SVFFunction(const Function* f) = delete;
    SVFFunction(void) = delete;
    virtual ~SVFFunction();

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFFunc;
    }

    inline const Function* getLLVMFun() const
    {
        assert(getLLVMValue() && "no LLVM Function found!");
        return SVFUtil::dyn_cast<Function>(getLLVMValue());
    }

    inline bool isDeclaration() const
    {
        return isDecl;
    }

    inline bool isIntrinsic() const
    {
        return intricsic;
    }

    inline void setDefFunForMultipleModule(const SVFFunction* deffun)
    {
        realDefFun = deffun;
    }

    inline const SVFFunction* getDefFunForMultipleModule() const
    {
        return realDefFun;
    }

    u32_t arg_size() const;
    const SVFArgument* getArg(u32_t idx) const;
    bool isVarArg() const;

    inline void addBasicBlock(const SVFBasicBlock* bb)
    {
        allBBs.push_back(bb);
    }

    inline void addArgument(SVFArgument* arg)
    {
        allArgs.push_back(arg);
    }

    inline const SVFBasicBlock* getEntryBlock() const
    {
        return allBBs.front();
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
        return reachableBBs;
    }

    inline const bool isUncalledFunction() const
    {
        return isUncalled;
    }

    inline const void setIsUncalledFunction(bool isUncalledFunction)
    {
        this->isUncalled = isUncalledFunction;
    }

    inline const void setIsNotRet(bool doesNotRet)
    {
        this->isNotRet = doesNotRet;
    }

    inline const void setExitBB(const SVFBasicBlock* exitBB)
    {
        this->exitBB = exitBB;
    }

    inline const void setReachableBBs(std::vector<const SVFBasicBlock*> reachableBBs)
    {
        this->reachableBBs = reachableBBs;
    }

    inline const Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& getDomFrontierMap() const
    {
        return dfBBsMap;
    }

    inline Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& getDomFrontierMap()
    {
        return dfBBsMap;
    }

    inline bool hasLoopInfo(const SVFBasicBlock* bb) const
    {
        return bb2LoopMap.find(bb)!=bb2LoopMap.end();
    }

    const std::vector<const SVFBasicBlock*>& getLoopInfo(const SVFBasicBlock* bb) const;

    inline void addToBB2LoopMap(const SVFBasicBlock* bb, const SVFBasicBlock* loopBB)
    {
        bb2LoopMap[bb].push_back(loopBB);
    }

    inline const Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& getPostDomTreeMap() const
    {
        return pdtBBsMap;
    }

    inline Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& getPostDomTreeMap()
    {
        return pdtBBsMap;
    }

    inline Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& getDomTreeMap()
    {
        return dtBBsMap;
    }

    inline const Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>>& getDomTreeMap() const
    {
        return dtBBsMap;
    }

    inline bool isUnreachable(const SVFBasicBlock* bb) const
    {
        return std::find(reachableBBs.begin(), reachableBBs.end(), bb)==reachableBBs.end();
    }

    inline bool isNotRetFunction() const
    {
        return isNotRet;
    }

    inline const SVFBasicBlock* getExitBB() const
    {
        return this->exitBB;
    }

    void getExitBlocksOfLoop(const SVFBasicBlock* bb, Set<const SVFBasicBlock*>& exitbbs) const;

    bool isLoopHeader(const SVFBasicBlock* bb) const;

    bool dominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;

    bool postDominate(const SVFBasicBlock* bbKey, const SVFBasicBlock* bbValue) const;

};

class SVFBasicBlock : public SVFValue
{

public:
    typedef std::vector<const SVFInstruction*>::const_iterator const_iterator;

private:
    std::vector<const SVFInstruction*> allInsts;
    std::vector<const SVFBasicBlock*> succBBs;
    std::vector<const SVFBasicBlock*> predBBs;
    const BasicBlock* bb;
    const SVFFunction* fun;
public:
    SVFBasicBlock(const BasicBlock* b, const SVFFunction* f);
    SVFBasicBlock(void) = delete;
    virtual ~SVFBasicBlock();

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFBB;
    }

    inline void addInstruction(const SVFInstruction* inst)
    {
        allInsts.push_back(inst);
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

    inline const SVFInstruction* front() const
    {
        return allInsts.front();
    }

    inline const SVFInstruction* back() const
    {
        return allInsts.back();
    }

    inline void addSuccBasicBlock(const SVFBasicBlock* succ)
    {
        succBBs.push_back(succ);
    }

    inline void addPredBasicBlock(const SVFBasicBlock* pred)
    {
        predBBs.push_back(pred);
    }

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

    inline const BasicBlock* getLLVMBasicBlock() const
    {
        return bb;
    }
};

class SVFInstruction : public SVFValue
{

private:
    const Instruction* inst;
    const SVFBasicBlock* bb;
    const SVFFunction* fun;
    bool terminator;
    bool ret;
public:
    SVFInstruction(const Instruction* i, const SVFBasicBlock* b, bool isRet, SVFValKind k = SVFInst);
    SVFInstruction(const Instruction* i) = delete;
    SVFInstruction(void) = delete;

    inline const Instruction* getLLVMInstruction() const
    {
        return inst;
    }

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFInst || node->getKind() == SVFCall;
    }

    inline const SVFBasicBlock* getParent() const
    {
        return bb;
    }

    inline const SVFFunction* getFunction() const
    {
        return fun;
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
private:
    std::vector<const SVFValue*> args;
    const FunctionType* calledFunType;
    const SVFValue* calledVal;
public:
    SVFCallInst(const Instruction* i, const SVFBasicBlock* b, const FunctionType* t) : SVFInstruction(i,b,false,SVFCall), calledFunType(t), calledVal(nullptr)
    {
    }
    SVFCallInst(const Instruction* i) = delete;
    SVFCallInst(void) = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFCall;
    }
    static inline bool classof(const SVFInstruction *node)
    {
        return node->getKind() == SVFCall;
    }
    inline void addArgument(const SVFValue* a)
    {
        args.push_back(a);
    }
    u32_t arg_size() const
    {
        return args.size();
    }
    bool arg_empty() const
    {
        return args.empty();
    }
    const SVFValue* getArgOperand(u32_t i) const
    {
        assert(i < arg_size() && "out of bound access of the argument");
        return args[i];
    }
    u32_t getNumArgOperands() const
    {
        return arg_size();
    }
    void setCalledOperand(const SVFValue* v)
    {
        calledVal = v;
    }
    const SVFValue* getCalledOperand() const
    {
        return calledVal;
    }
    const FunctionType* getFunctionType() const
    {
        return calledFunType;
    }
    const SVFFunction* getCalledFunction() const
    {
        return SVFUtil::dyn_cast<SVFFunction>(calledVal);
    }
    const SVFFunction* getCaller() const
    {
        return getFunction();
    }
};

class SVFGlobalValue : public SVFValue
{
private:
    const GlobalValue* gv;
public:
    SVFGlobalValue(const GlobalValue* _gv): SVFValue(_gv, SVFValue::SVFGlob), gv(_gv)
    {
    }
    SVFGlobalValue() = delete;

    const GlobalValue* getLLVMGlobalValue() const
    {
        return gv;
    }

    static inline bool classof(const SVFValue *node)
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


class SVFConstantData : public SVFValue
{
private:
    const ConstantData* constData;

public:
    SVFConstantData(const ConstantData* _const, SVFValKind k = SVFConstData): SVFValue(_const, k), constData(_const)
    {
    }
    SVFConstantData() = delete;

    const ConstantData* getLLVMConstantData() const
    {
        return constData;
    }

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFConstData || 
               node->getKind() == SVFConstInt || 
               node->getKind() == SVFConstFP ||
               node->getKind() == SVFConstNullPtr ||
               node->getKind() == SVFBlackHole;
    }
};


class SVFConstantInt : public SVFConstantData
{
private:
    u64_t zval;
    s64_t sval;
public:
    SVFConstantInt(const ConstantData* _const, u64_t z, s64_t s): SVFConstantData(_const, SVFValue::SVFConstInt), zval(z), sval(s)
    {
    }
    SVFConstantInt() = delete;

    static inline bool classof(const SVFValue *node)
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
    SVFConstantFP(const ConstantData* _const, double d): SVFConstantData(_const, SVFValue::SVFConstFP), dval(d)
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
};


class SVFConstantNullPtr : public SVFConstantData
{

public:
    SVFConstantNullPtr(const ConstantData* _const): SVFConstantData(_const, SVFValue::SVFConstNullPtr)
    {
    }
    SVFConstantNullPtr() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFConstNullPtr;
    }
};

class SVFBlackHoleValue : public SVFConstantData
{

public:
    SVFBlackHoleValue(const ConstantData* _const): SVFConstantData(_const, SVFValue::SVFBlackHole)
    {
    }
    SVFBlackHoleValue() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFBlackHole;
    }
};

class SVFOtherValue : public SVFValue
{
public:
    SVFOtherValue(const Value* other): SVFValue(other, SVFValue::SVFOther)
    {
    }
    SVFOtherValue() = delete;

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFOther;
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
    const Function* getCaller() const
    {
        return CB->getCaller()->getLLVMFun();
    }
    const FunctionType* getFunctionType() const
    {
        return CB->getFunctionType();
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
