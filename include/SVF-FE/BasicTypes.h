#ifndef SVF_FE_BASIC_TYPES_H
#define SVF_FE_BASIC_TYPES_H

#include "SVF-FE/GEPTypeBridgeIterator.h"

#include <llvm/Pass.h>

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/DerivedTypes.h>

#include <llvm/Analysis/MemoryLocation.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>

#include <llvm/Support/SourceMgr.h>

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


typedef llvm::SMDiagnostic SMDiagnostic;

typedef llvm::BlockAddress BlockAddress;

// LLVM Metadata
typedef llvm::MDString MDString;
typedef llvm::MetadataAsValue MetadataAsValue;

// LLVM data layout
typedef llvm::StructLayout StructLayout;
typedef llvm::ConstantStruct ConstantStruct;
typedef llvm::MemoryLocation MemoryLocation;
typedef llvm::DataLayout DataLayout;

// LLVM Aliases and constants
typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantAggregateZero ConstantAggregateZero;
typedef llvm::ConstantDataSequential ConstantDataSequential;
typedef llvm::ConstantExpr ConstantExpr;
typedef llvm::ConstantDataArray ConstantDataArray;
typedef llvm::ConstantData ConstantData;
typedef llvm::ConstantArray ConstantArray;
typedef llvm::Constant Constant;

// LLVM Instructions
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

// LLVM Types.
typedef llvm::VectorType VectorType;
#if (LLVM_VERSION_MAJOR >= 9)
typedef llvm::FunctionCallee FunctionCallee;
#endif

#endif  // SVF_FE_BASIC_TYPES_H
