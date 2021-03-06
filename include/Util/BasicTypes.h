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
#include "SVF-FE/GEPTypeBridgeIterator.h"
#include "Graphs/GraphPrinter.h"
#include "Util/Casting.h"
#include <llvm/ADT/SmallVector.h>		// for small vector
#include <llvm/ADT/SparseBitVector.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/InstVisitor.h>	// for instruction visitor
#include <llvm/IR/InstIterator.h>	// for inst iteration
#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/ADT/StringExtras.h>	// for utostr_32
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CallGraph.h>	// call graph
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable

#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriter.h>		// for WriteBitcodeToFile
#include <llvm/Bitcode/BitcodeReader.h>     /// for isBitcode
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/ADT/GraphTraits.h>		// for Graphtraits
#include <llvm/Support/GraphWriter.h>		// for graph write
#include <llvm/IR/IRBuilder.h>		// for instrument svf.main
#include <llvm/Transforms/Utils/Local.h>	// for FindDbgAddrUses
#include <llvm/IR/DebugInfo.h>

namespace SVF
{

class BddCond;


/// LLVM Basic classes
using SMDiagnostic = llvm::SMDiagnostic;
using LLVMContext = llvm::LLVMContext;
using Type = llvm::Type;
using Function = llvm::Function;
using BasicBlock = llvm::BasicBlock;
using Value = llvm::Value;
using Instruction = llvm::Instruction;
using CallSite = llvm::CallSite;
using GlobalObject = llvm::GlobalObject;
using GlobalValue = llvm::GlobalValue;
using GlobalVariable = llvm::GlobalVariable;
using Module = llvm::Module;
using LLVMCallGraph = llvm::CallGraph;
using User = llvm::User;
using Use = llvm::Use;
using Loop = llvm::Loop;
using LoopInfo = llvm::LoopInfo;
using UnifyFunctionExitNodes = llvm::UnifyFunctionExitNodes;
using ModulePass = llvm::ModulePass;
using AnalysisUsage = llvm::AnalysisUsage;

/// LLVM outputs
using raw_ostream = llvm::raw_ostream;
using raw_string_ostream = llvm::raw_string_ostream;
using raw_fd_ostream = llvm::raw_fd_ostream;
using StringRef = llvm::StringRef;
using ToolOutputFile = llvm::ToolOutputFile;

/// LLVM types
using StructType = llvm::StructType;
using ArrayType = llvm::ArrayType;
using PointerType = llvm::PointerType;
using FunctionType = llvm::FunctionType;
using VectorType = llvm::VectorType;
using MetadataAsValue = llvm::MetadataAsValue;
using BlockAddress = llvm::BlockAddress;

/// LLVM data layout
using DataLayout = llvm::DataLayout;
using StructLayout = llvm::StructLayout;
using SmallBBVector = llvm::SmallVector<BasicBlock *, 8>;
using ConstantStruct = llvm::ConstantStruct;
using MemoryLocation = llvm::MemoryLocation;

/// LLVM Aliases and constants
using Argument = llvm::Argument;
using Constant = llvm::Constant;
using ConstantData = llvm::ConstantData;
using ConstantExpr = llvm::ConstantExpr;
using ConstantAggregate = llvm::ConstantAggregate;
using ConstantPointerNull = llvm::ConstantPointerNull;
using ConstantArray = llvm::ConstantArray;
using GlobalAlias = llvm::GlobalAlias;
using AliasResult = llvm::AliasResult;
using ModRefInfo = llvm::ModRefInfo;
using AnalysisID = llvm::AnalysisID;
using ConstantDataArray = llvm::ConstantDataArray;

/// LLVM metadata
using NamedMDNode = llvm::NamedMDNode;
using MDString = llvm::MDString;
using MDNode = llvm::MDNode;


/// LLVM Instructions
using AllocaInst = llvm::AllocaInst;
using CallInst = llvm::CallInst;
using InvokeInst = llvm::InvokeInst;
using CallBrInst = llvm::CallBrInst;
using StoreInst = llvm::StoreInst;
using LoadInst = llvm::LoadInst;
using PHINode = llvm::PHINode;
using GetElementPtrInst = llvm::GetElementPtrInst;
using CastInst = llvm::CastInst;
using BitCastInst = llvm::BitCastInst;
using ReturnInst = llvm::ReturnInst;
using ConstantInt = llvm::ConstantInt;
using SelectInst = llvm::SelectInst;
using IntToPtrInst = llvm::IntToPtrInst;
using CmpInst = llvm::CmpInst;
using BranchInst = llvm::BranchInst;
using SwitchInst = llvm::SwitchInst;
using ExtractValueInst = llvm::ExtractValueInst;
using InsertValueInst = llvm::InsertValueInst;
using BinaryOperator = llvm::BinaryOperator;
using UnaryOperator = llvm::UnaryOperator;
using PtrToIntInst = llvm::PtrToIntInst;
using VAArgInst = llvm::VAArgInst;
using ExtractElementInst = llvm::ExtractElementInst;
using InsertElementInst = llvm::InsertElementInst;
using ShuffleVectorInst = llvm::ShuffleVectorInst;
using LandingPadInst = llvm::LandingPadInst;
using ResumeInst = llvm::ResumeInst;
using UnreachableInst = llvm::UnreachableInst;
using FenceInst = llvm::FenceInst;
using AtomicCmpXchgInst = llvm::AtomicCmpXchgInst;
using AtomicRMWInst = llvm::AtomicRMWInst;
using UndefValue = llvm::UndefValue;

#if (LLVM_VERSION_MAJOR >= 9)
using FunctionCallee = llvm::FunctionCallee;
#endif

/// LLVM scalar evolution
using ScalarEvolutionWrapperPass = llvm::ScalarEvolutionWrapperPass;
using ScalarEvolution = llvm::ScalarEvolution;
using SCEVAddRecExpr = llvm::SCEVAddRecExpr;
using SCEVConstant = llvm::SCEVConstant;
using SCEV = llvm::SCEV;

/// LLVM Dominators
using DominanceFrontier = llvm::DominanceFrontier;
using DominatorTree = llvm::DominatorTree;
using PostDominatorTree = llvm::PostDominatorTree;
using DomTreeNode = llvm::DomTreeNode;
using DominanceFrontierBase = llvm::DominanceFrontierBase<BasicBlock, false>;
using PostDominatorTreeWrapperPass = llvm::PostDominatorTreeWrapperPass;
using LoopInfoWrapperPass = llvm::LoopInfoWrapperPass;

/// LLVM Iterators
using inst_iterator = llvm::inst_iterator;
using succ_const_iterator = llvm::succ_const_iterator;
using const_inst_iterator = llvm::const_inst_iterator;
using const_pred_iterator = llvm::const_pred_iterator;
using gep_type_iterator = llvm::gep_type_iterator;
using bridge_gep_iterator = llvm::bridge_gep_iterator;
using GraphPrinter = llvm::GraphPrinter;
using IRBuilder = llvm::IRBuilder<>;
using IntegerType = llvm::IntegerType;

/// LLVM debug information
using DebugInfoFinder = llvm::DebugInfoFinder;
using DIType = llvm::DIType;
using DIBasicType = llvm::DIBasicType;
using DICompositeType = llvm::DICompositeType;
using DIDerivedType = llvm::DIDerivedType;
using DISubroutineType = llvm::DISubroutineType;
using DISubprogram = llvm::DISubprogram;
using DISubrange = llvm::DISubrange;
using DINode = llvm::DINode;
using DINodeArray = llvm::DINodeArray;
using DITypeRefArray = llvm::DITypeRefArray;

namespace dwarf = llvm::dwarf;

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

    SVFFunction(Function* f): SVFValue(f->getName(),SVFValue::SVFFunc),
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

    inline bool isVarArg() const
    {
        return getLLVMFun()->isVarArg();
    }

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

class SVFInstruction : public SVFValue
{

public:
    SVFInstruction(const std::string& val): SVFValue(val,SVFValue::SVFInst)
    {
    }

};

template <typename F, typename S>
raw_ostream& operator<< (raw_ostream &o, const std::pair<F, S> &var)
{
    o << "<" << var.first << ", " << var.second << ">";
    return o;
}

} // End namespace SVF

/// Specialise hash for CallSites.
template <> struct std::hash<SVF::CallSite> {
    size_t operator()(const SVF::CallSite &cs) const {
        std::hash<SVF::Instruction *> h;
        return h(cs.getInstruction());
    }
};

/// Specialise hash for SparseBitVectors.
template <> struct std::hash<llvm::SparseBitVector<>>
{
    size_t operator()(const llvm::SparseBitVector<> &sbv) const {
        std::hash<std::pair<std::pair<size_t, size_t>, size_t>> h;
        return h(std::make_pair(std::make_pair(sbv.count(), sbv.find_first()), sbv.find_last()));
    }
};

#endif /* BASICTYPES_H_ */
