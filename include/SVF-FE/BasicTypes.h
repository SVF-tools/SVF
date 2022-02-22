#ifndef SVF_FE_BASIC_TYPES_H
#define SVF_FE_BASIC_TYPES_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>

#include <llvm/Support/SourceMgr.h>

typedef llvm::SMDiagnostic SMDiagnostic;

typedef llvm::ExtractValueInst  ExtractValueInst;
typedef llvm::ExtractElementInst ExtractElementInst;
typedef llvm::MetadataAsValue MetadataAsValue;
typedef llvm::BlockAddress BlockAddress;

typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantAggregateZero ConstantAggregateZero;

#endif  // SVF_FE_BASIC_TYPES_H
