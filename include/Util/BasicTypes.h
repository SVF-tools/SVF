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
typedef llvm::CallBase CallBase;
typedef llvm::GlobalValue GlobalValue;
typedef llvm::GlobalVariable GlobalVariable;

/// LLVM outputs
typedef llvm::raw_string_ostream raw_string_ostream;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::FunctionType FunctionType;
typedef llvm::IntegerType IntegerType;

/// LLVM Aliases and constants
typedef llvm::Argument Argument;
typedef llvm::ConstantInt ConstantInt;
typedef llvm::ConstantPointerNull ConstantPointerNull;
typedef llvm::GlobalAlias GlobalAlias;

/// LLVM metadata
typedef llvm::NamedMDNode NamedMDNode;
typedef llvm::MDNode MDNode;

typedef llvm::GraphPrinter GraphPrinter;

// LLVM Debug Information
typedef llvm::DISubprogram DISubprogram;


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
        SVFGlob,
        SVFArg
    };

private:
    const Value* value;
    const Type* type;
    GNodeK kind;	///< Type of this SVFValue

protected:
    std::string name;

    /// Constructor
    SVFValue(const Value* val, SVFValKind k): value(val), type(val->getType()), kind(k), name(val->getName())
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

    const std::string getName() const
    {
        return name;
    }

    const Value* getValue() const
    {
        return value;
    }

    const Type* getType() const
    {
        return type;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const SVFValue &node)
    {
        o << node.getName();
        return o;
    }
    //@}

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFValue::SVFVal ||
               node->getKind() == SVFValue::SVFFunc ||
               node->getKind() == SVFValue::SVFGlob ||
               node->getKind() == SVFValue::SVFBB ||
               node->getKind() == SVFValue::SVFInst;
    }
};

class SVFFunction : public SVFValue
{

public:
    typedef std::vector<const SVFBasicBlock*>::const_iterator const_iterator;

private:
    bool isDecl;
    bool isIntri;
    const Function* fun;
    const SVFBasicBlock* exitBB;
    std::vector<const SVFBasicBlock*> reachableBBs;
    std::vector<const SVFBasicBlock*> allBBs;
    std::vector<const SVFInstruction*> allInsts;
    std::vector<const SVFArgument*> allArgs;

    bool isUncalled;
    bool isNotRet;
    bool varArg;
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> dtBBsMap;
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> dfBBsMap;
    Map<const SVFBasicBlock*,Set<const SVFBasicBlock*>> pdtBBsMap;
    Map<const SVFBasicBlock*,std::vector<const SVFBasicBlock*>> bb2LoopMap;

public:

    SVFFunction(const Function* f);
    SVFFunction(void) = delete;
    virtual ~SVFFunction();

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFFunc;
    }

    inline const Function* getLLVMFun() const
    {
        assert(fun && "no LLVM Function found!");
        return fun;
    }

    inline bool isDeclaration() const
    {
        return isDecl;
    }

    inline bool isIntrinsic() const
    {
        return isIntri;
    }

    u32_t arg_size() const;
    const SVFArgument* getArg(u32_t idx) const;
    bool isVarArg() const;

    inline void addBasicBlock(const SVFBasicBlock* bb) 
    {
        allBBs.push_back(bb);
    }
    
    inline void addInstruction(const SVFInstruction* inst) 
    {
        allInsts.push_back(inst);
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

    inline const std::vector<const SVFInstruction*>& getInstructionList() const
    {
        return allInsts;
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

    // Dump Control Flow Graph of llvm function, with instructions
    void viewCFG();

    // Dump Control Flow Graph of llvm function, without instructions
    void viewCFGOnly();

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

    inline u32_t getBBsuccessorNum() const
    {
        return bb->getTerminator()->getNumSuccessors();
    }

    u32_t getBBSuccessorPos(const SVFBasicBlock* succbb);
    u32_t getBBSuccessorPos(const SVFBasicBlock* succbb) const;
    u32_t getBBPredecessorPos(const SVFBasicBlock* succbb);

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
    SVFInstruction(const Instruction* i, const SVFBasicBlock* b, bool isRet);
    SVFInstruction(const Instruction* i) = delete;
    SVFInstruction(void) = delete;

    inline const Instruction* getLLVMInstruction() const
    {
        return inst;
    }

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFInst;
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
    const Argument* arg;
    u32_t argNo;
    bool uncalled;
public:
    SVFArgument(const Argument* _arg, const SVFFunction* _fun, bool _uncalled): SVFValue(_arg, SVFValue::SVFArg), fun(_fun), arg(_arg), argNo(_arg->getArgNo()), uncalled(_uncalled)
    {
    }
    SVFArgument() = delete;

    const Argument* getLLVMAgument() const
    {
        return arg;
    }

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



class CallSite
{
private:
    const CallBase *CB;
    const SVFInstruction* inst;
public:
    CallSite(const SVFInstruction *I) : CB(SVFUtil::dyn_cast<CallBase>(I->getLLVMInstruction())), inst(I) {
        assert(CB && "not a callsite?");
    }

    const SVFInstruction* getInstruction() const
    {
        return inst;
    }
    const Value* getArgument(u32_t ArgNo) const
    {
        return CB->getArgOperand(ArgNo);
    }
    Type* getType() const
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
    const Value* getArgOperand(u32_t i) const
    {
        return CB->getArgOperand(i);
    }
    u32_t getNumArgOperands() const
    {
        return CB->arg_size();
    }
    const Function* getCalledFunction() const
    {
        return CB->getCalledFunction();
    }
    const Value* getCalledValue() const
    {
        return CB->getCalledOperand();
    }
    const Function* getCaller() const
    {
        return CB->getCaller();
    }
    const FunctionType* getFunctionType() const
    {
        return CB->getFunctionType();
    }
    bool paramHasAttr(unsigned ArgNo, llvm::Attribute::AttrKind Kind) const
    {
        return CB->paramHasAttr(ArgNo, Kind);
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
