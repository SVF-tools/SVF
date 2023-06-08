//===- BasicTypes.h --Basic Types of SVF-LLVM---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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

#ifndef SVF_FE_BASIC_TYPES_H
#define SVF_FE_BASIC_TYPES_H

#include "SVF-LLVM/GEPTypeBridgeIterator.h"

#include <llvm/Pass.h>

#include <llvm/IR/Instructions.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>	//for gep iterator
#include <llvm/IR/GlobalVariable.h>	// for GlobalVariable
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Statepoint.h>

#include <llvm/Analysis/MemoryLocation.h>
#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>

#include <llvm/Support/SourceMgr.h>

#include <llvm/Bitcode/BitcodeWriter.h>		// for WriteBitcodeToFile
#include <llvm/Bitcode/BitcodeReader.h>     /// for isBitcode
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/IR/InstVisitor.h>	// for instruction visitor
#include <llvm/IR/InstIterator.h>	// for inst iteration

#include <llvm/BinaryFormat/Dwarf.h> // for dwarf tags

#include <llvm/Analysis/LoopInfo.h>

namespace SVF
{

typedef llvm::LLVMContext LLVMContext;
typedef llvm::GlobalObject GlobalObject;
typedef llvm::Use Use;
typedef llvm::ModulePass ModulePass;
typedef llvm::IRBuilder<> IRBuilder;
#if LLVM_VERSION_MAJOR >= 12
typedef llvm::UnifyFunctionExitNodesLegacyPass UnifyFunctionExitNodes;
#else
typedef llvm::UnifyFunctionExitNodes UnifyFunctionExitNodes;
#endif

/// LLVM Basic classes
typedef llvm::Value Value;
typedef llvm::Type Type;
typedef llvm::Module Module;
typedef llvm::Function Function;
typedef llvm::BasicBlock BasicBlock;
typedef llvm::Instruction Instruction;
typedef llvm::GlobalValue GlobalValue;

typedef llvm::SMDiagnostic SMDiagnostic;
typedef llvm::BlockAddress BlockAddress;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::IntegerType IntegerType;
typedef llvm::FunctionType FunctionType;

// LLVM Metadata
typedef llvm::MDString MDString;
typedef llvm::MetadataAsValue MetadataAsValue;

// LLVM data layout
typedef llvm::StructLayout StructLayout;
typedef llvm::ConstantStruct ConstantStruct;
typedef llvm::MemoryLocation MemoryLocation;
typedef llvm::DataLayout DataLayout;

/// LLVM metadata and debug information
typedef llvm::NamedMDNode NamedMDNode;
typedef llvm::MDNode MDNode;
typedef llvm::DISubprogram DISubprogram;

// LLVM Aliases and constants
typedef llvm::ConstantData ConstantData;
typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantAggregateZero ConstantAggregateZero;
typedef llvm::ConstantDataSequential ConstantDataSequential;
typedef llvm::ConstantExpr ConstantExpr;
typedef llvm::ConstantDataArray ConstantDataArray;
typedef llvm::ConstantData ConstantData;
typedef llvm::ConstantArray ConstantArray;
typedef llvm::Constant Constant;
typedef llvm::ConstantInt ConstantInt;
typedef llvm::ConstantFP ConstantFP;
typedef llvm::ConstantPointerNull ConstantPointerNull;
typedef llvm::GlobalAlias GlobalAlias;
typedef llvm::GlobalIFunc GlobalIFunc;
typedef llvm::GlobalVariable GlobalVariable;

/// LLVM Dominators
typedef llvm::DominatorTree DominatorTree;
typedef llvm::DomTreeNode DomTreeNode;
typedef llvm::DominanceFrontier DominanceFrontier;
typedef llvm::PostDominatorTree PostDominatorTree;
typedef llvm::DominanceFrontierBase<llvm::BasicBlock, false> DominanceFrontierBase;

/// LLVM Loop
typedef llvm::Loop Loop;
typedef llvm::LoopInfo LoopInfo;
typedef llvm::User User;

// LLVM Instructions
typedef llvm::Argument Argument;
typedef llvm::CallBase CallBase;
typedef llvm::CallInst CallInst;
typedef llvm::StoreInst StoreInst;
typedef llvm::LoadInst LoadInst;
typedef llvm::AllocaInst AllocaInst;
typedef llvm::AtomicCmpXchgInst AtomicCmpXchgInst;
typedef llvm::AtomicRMWInst AtomicRMWInst;
typedef llvm::BitCastInst BitCastInst;
typedef llvm::BranchInst BranchInst;
typedef llvm::SwitchInst SwitchInst;
typedef llvm::CallBrInst CallBrInst;
typedef llvm::ReturnInst ReturnInst;
typedef llvm::CastInst CastInst;
typedef llvm::CmpInst CmpInst;
typedef llvm::ExtractValueInst  ExtractValueInst;
typedef llvm::ExtractElementInst ExtractElementInst;
typedef llvm::GetElementPtrInst GetElementPtrInst;
typedef llvm::InvokeInst InvokeInst;
typedef llvm::ShuffleVectorInst ShuffleVectorInst;
typedef llvm::PHINode PHINode;
typedef llvm::IntToPtrInst IntToPtrInst;
typedef llvm::InsertValueInst InsertValueInst;
typedef llvm::FenceInst FenceInst;
typedef llvm::FreezeInst FreezeInst;
typedef llvm::UnreachableInst UnreachableInst;
typedef llvm::InsertElementInst InsertElementInst;
typedef llvm::LandingPadInst LandingPadInst;
typedef llvm::ResumeInst ResumeInst;
typedef llvm::SelectInst SelectInst;
typedef llvm::VAArgInst VAArgInst;
typedef llvm::VACopyInst VACopyInst;
typedef llvm::VAEndInst VAEndInst;
typedef llvm::VAStartInst VAStartInst;
typedef llvm::BinaryOperator BinaryOperator;
typedef llvm::UnaryOperator UnaryOperator;
typedef llvm::UndefValue UndefValue;

// LLVM Intrinsic Instructions
#if LLVM_VERSION_MAJOR >= 13
typedef llvm::IntrinsicInst IntrinsicInst;
typedef llvm::DbgInfoIntrinsic DbgInfoIntrinsic;
typedef llvm::DbgVariableIntrinsic DbgVariableIntrinsic;
typedef llvm::DbgDeclareInst DbgDeclareInst;
typedef llvm::DbgAddrIntrinsic DbgAddrIntrinsic;
typedef llvm::DbgValueInst DbgValueInst;
typedef llvm::DbgLabelInst DbgLabelInst;
typedef llvm::VPIntrinsic VPIntrinsic;
typedef llvm::ConstrainedFPIntrinsic ConstrainedFPIntrinsic;
typedef llvm::ConstrainedFPCmpIntrinsic ConstrainedFPCmpIntrinsic;
typedef llvm::MinMaxIntrinsic MinMaxIntrinsic;
typedef llvm::BinaryOpIntrinsic BinaryOpIntrinsic;
typedef llvm::WithOverflowInst WithOverflowInst;
typedef llvm::SaturatingInst SaturatingInst;
typedef llvm::AtomicMemIntrinsic AtomicMemIntrinsic;
typedef llvm::AtomicMemSetInst AtomicMemSetInst;
typedef llvm::AtomicMemTransferInst AtomicMemTransferInst;
typedef llvm::AtomicMemCpyInst AtomicMemCpyInst;
typedef llvm::AtomicMemMoveInst AtomicMemMoveInst;
typedef llvm::MemIntrinsic MemIntrinsic;
typedef llvm::MemSetInst MemSetInst;
typedef llvm::MemTransferInst MemTransferInst;
typedef llvm::MemCpyInst MemCpyInst;
typedef llvm::MemMoveInst MemMoveInst;
typedef llvm::MemCpyInlineInst MemCpyInlineInst;
typedef llvm::AnyMemIntrinsic AnyMemIntrinsic;
typedef llvm::AnyMemSetInst AnyMemSetInst;
typedef llvm::AnyMemTransferInst AnyMemTransferInst;
typedef llvm::AnyMemCpyInst AnyMemCpyInst;
typedef llvm::AnyMemMoveInst AnyMemMoveInst;
typedef llvm::VAStartInst VAStartInst;
typedef llvm::VAEndInst VAEndInst;
typedef llvm::VACopyInst VACopyInst;
typedef llvm::InstrProfIncrementInst InstrProfIncrementInst;
typedef llvm::InstrProfIncrementInstStep InstrProfIncrementInstStep;
typedef llvm::InstrProfValueProfileInst InstrProfValueProfileInst;
typedef llvm::PseudoProbeInst PseudoProbeInst;
typedef llvm::NoAliasScopeDeclInst NoAliasScopeDeclInst;
typedef llvm::GCStatepointInst GCStatepointInst;
typedef llvm::GCProjectionInst GCProjectionInst;
typedef llvm::GCRelocateInst GCRelocateInst;
typedef llvm::GCResultInst GCResultInst;
typedef llvm::AssumeInst AssumeInst;
#endif

// LLVM Debug Information
typedef llvm::DIType DIType;
typedef llvm::DICompositeType DICompositeType;
typedef llvm::DIDerivedType DIDerivedType;
typedef llvm::DebugInfoFinder DebugInfoFinder;
typedef llvm::DISubroutineType DISubroutineType;
typedef llvm::DIBasicType DIBasicType;
typedef llvm::DISubrange DISubrange;
typedef llvm::DINode DINode;
typedef llvm::DINodeArray DINodeArray;
typedef llvm::DITypeRefArray DITypeRefArray;
namespace dwarf = llvm::dwarf;

// Iterators.
typedef llvm::inst_iterator inst_iterator;
typedef llvm::const_inst_iterator const_inst_iterator;
typedef llvm::gep_type_iterator gep_type_iterator;
typedef llvm::bridge_gep_iterator bridge_gep_iterator;
typedef llvm::const_inst_iterator const_inst_iterator;
typedef llvm::const_pred_iterator const_pred_iterator;

// LLVM Scalar Evolution.
typedef llvm::ScalarEvolutionWrapperPass ScalarEvolutionWrapperPass;
typedef llvm::SCEVAddRecExpr SCEVAddRecExpr;
typedef llvm::SCEVConstant SCEVConstant;
typedef llvm::ScalarEvolution ScalarEvolution;
typedef llvm::SCEV SCEV;

/// LLVM outputs
typedef llvm::raw_fd_ostream raw_fd_ostream;

// LLVM Types.
typedef llvm::VectorType VectorType;
#if (LLVM_VERSION_MAJOR >= 9)
typedef llvm::FunctionCallee FunctionCallee;
#endif

/// LLVM Iterators
#if LLVM_VERSION_MAJOR >= 11
typedef llvm::const_succ_iterator succ_const_iterator;
#else
typedef llvm::succ_const_iterator succ_const_iterator;
#endif
} // End namespace SVF

#endif  // SVF_FE_BASIC_TYPES_H
