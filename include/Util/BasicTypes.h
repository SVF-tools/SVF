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
#include <llvm/ADT/SparseBitVector.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>	// for instruction visitor
#include <llvm/IR/InstIterator.h>	// for inst iteration
#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator
#include <llvm/ADT/StringExtras.h>	// for utostr_32
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CallGraph.h>	// call graph
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable

#include <llvm/Bitcode/BitcodeWriter.h>		// for WriteBitcodeToFile
#include <llvm/Bitcode/BitcodeReader.h>     /// for isBitcode
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/ADT/GraphTraits.h>		// for Graphtraits
#include <llvm/Transforms/Utils/Local.h>	// for FindDbgAddrUses

#if (LLVM_VERSION_MAJOR >= 14)
#include <llvm/BinaryFormat/Dwarf.h>
#endif

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/BinaryFormat/Dwarf.h"

namespace SVF
{

class BddCond;


/// LLVM Basic classes
typedef llvm::Type Type;
typedef llvm::Function Function;
typedef llvm::BasicBlock BasicBlock;
typedef llvm::Value Value;
typedef llvm::Instruction Instruction;
typedef llvm::CallBase CallBase;
typedef llvm::GlobalValue GlobalValue;
typedef llvm::GlobalVariable GlobalVariable;
typedef llvm::Module Module;
typedef llvm::User User;
typedef llvm::Loop Loop;
typedef llvm::LoopInfo LoopInfo;

/// LLVM outputs
typedef llvm::raw_string_ostream raw_string_ostream;
typedef llvm::raw_fd_ostream raw_fd_ostream;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::FunctionType FunctionType;

/// LLVM Aliases and constants
typedef llvm::Argument Argument;
typedef llvm::ConstantInt ConstantInt;
typedef llvm::ConstantPointerNull ConstantPointerNull;
typedef llvm::GlobalAlias GlobalAlias;

/// LLVM metadata
typedef llvm::NamedMDNode NamedMDNode;
typedef llvm::MDNode MDNode;

/// LLVM Instructions
typedef llvm::CallInst CallInst;
typedef llvm::StoreInst StoreInst;
typedef llvm::LoadInst LoadInst;

/// LLVM Dominators
typedef llvm::DominanceFrontier DominanceFrontier;
typedef llvm::DominatorTree DominatorTree;
typedef llvm::PostDominatorTree PostDominatorTree;
typedef llvm::DomTreeNode DomTreeNode;
typedef llvm::DominanceFrontierBase<BasicBlock, false> DominanceFrontierBase;

/// LLVM Iterators
#if LLVM_VERSION_MAJOR >= 11
typedef llvm::const_succ_iterator succ_const_iterator;
#else
typedef llvm::succ_const_iterator succ_const_iterator;
#endif
typedef llvm::GraphPrinter GraphPrinter;
typedef llvm::IntegerType IntegerType;

// LLVM Debug Information
typedef llvm::DISubprogram DISubprogram;

class SVFFunction : public SVFValue
{
private:
    bool isDecl;
    bool isIntri;
    Function* fun;
public:
    SVFFunction(const std::string& val): SVFValue(val,SVFValue::SVFFunc),
        isDecl(false), isIntri(false), fun(nullptr)
    {
    }

    SVFFunction(Function* f): SVFValue(f->getName().str(),SVFValue::SVFFunc),
        isDecl(f->isDeclaration()), isIntri(f->isIntrinsic()), fun(f)
    {
    }
    inline Function* getLLVMFun() const
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

    inline u32_t arg_size() const
    {
        return getLLVMFun()->arg_size();
    }

    const Value* getArg(u32_t idx) const
    {
        return getLLVMFun()->getArg(idx);
    }

    inline bool isVarArg() const
    {
        return getLLVMFun()->isVarArg();
    }

    // Dump Control Flow Graph of llvm function, with instructions
    void viewCFG();

    // Dump Control Flow Graph of llvm function, without instructions
    void viewCFGOnly();

};

class SVFGlobal : public SVFValue
{

public:
    SVFGlobal(const std::string& val): SVFValue(val,SVFValue::SVFGlob)
    {
    }

};

class SVFBasicBlock : public SVFValue
{

public:
    SVFBasicBlock(const std::string& val): SVFValue(val,SVFValue::SVFBB)
    {
    }

};

class CallSite
{
private:
    CallBase *CB;
public:
    CallSite(Instruction *I) : CB(SVFUtil::dyn_cast<CallBase>(I)) {}
    CallSite(Value *I) : CB(SVFUtil::dyn_cast<CallBase>(I)) {}

    CallBase *getInstruction() const
    {
        return CB;
    }
    using arg_iterator = User::const_op_iterator;
    Value *getArgument(unsigned ArgNo) const
    {
        return CB->getArgOperand(ArgNo);
    }
    Type *getType() const
    {
        return CB->getType();
    }
    User::const_op_iterator arg_begin() const
    {
        return CB->arg_begin();
    }
    User::const_op_iterator arg_end() const
    {
        return CB->arg_end();
    }
    unsigned arg_size() const
    {
        return CB->arg_size();
    }
    bool arg_empty() const
    {
        return CB->arg_empty();
    }
    Value *getArgOperand(unsigned i) const
    {
        return CB->getArgOperand(i);
    }
    unsigned getNumArgOperands() const
    {
        return CB->arg_size();
    }
    Function *getCalledFunction() const
    {
        return CB->getCalledFunction();
    }
    Value *getCalledValue() const
    {
        return CB->getCalledOperand();
    }
    Function *getCaller() const
    {
        return CB->getCaller();
    }
    FunctionType *getFunctionType() const
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
        std::hash<SVF::Instruction *> h;
        return h(cs.getInstruction());
    }
};

#endif /* BASICTYPES_H_ */
