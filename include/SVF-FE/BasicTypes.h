#ifndef SVF_FE_BASIC_TYPES_H
#define SVF_FE_BASIC_TYPES_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/DataLayout.h>

#include <llvm/Support/SourceMgr.h>

typedef llvm::SMDiagnostic SMDiagnostic;

typedef llvm::ExtractValueInst  ExtractValueInst;
typedef llvm::ExtractElementInst ExtractElementInst;
typedef llvm::MetadataAsValue MetadataAsValue;
typedef llvm::BlockAddress BlockAddress;

typedef llvm::StructLayout StructLayout;
typedef llvm::ConstantStruct ConstantStruct;

typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantAggregateZero ConstantAggregateZero;
typedef llvm::ConstantDataSequential ConstantDataSequential;
typedef llvm::ConstantExpr ConstantExpr;

typedef llvm::AtomicCmpXchgInst AtomicCmpXchgInst;
typedef llvm::AtomicRMWInst AtomicRMWInst;
typedef llvm::ShuffleVectorInst ShuffleVectorInst;
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

#endif  // SVF_FE_BASIC_TYPES_H
