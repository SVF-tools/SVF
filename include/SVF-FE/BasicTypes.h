#ifndef SVF_FE_BASIC_TYPES_H
#define SVF_FE_BASIC_TYPES_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DataLayout.h>

#include <llvm/Analysis/MemoryLocation.h>

#include <llvm/Support/SourceMgr.h>

typedef llvm::SMDiagnostic SMDiagnostic;

typedef llvm::MetadataAsValue MetadataAsValue;
typedef llvm::BlockAddress BlockAddress;

// LLVM data layout
typedef llvm::StructLayout StructLayout;
typedef llvm::ConstantStruct ConstantStruct;
typedef llvm::MemoryLocation MemoryLocation;

// LLVM Aliases and constants
typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantAggregateZero ConstantAggregateZero;
typedef llvm::ConstantDataSequential ConstantDataSequential;
typedef llvm::ConstantExpr ConstantExpr;

// LLVM Instructions
typedef llvm::AtomicCmpXchgInst AtomicCmpXchgInst;
typedef llvm::AtomicRMWInst AtomicRMWInst;
typedef llvm::BitCastInst BitCastInst;
typedef llvm::CallBrInst CallBrInst;
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

#endif  // SVF_FE_BASIC_TYPES_H
