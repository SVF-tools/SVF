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

#include "Util/GEPTypeBridgeIterator.h"
#include "Util/GraphPrinter.h"
#include "Util/Casting.h"
#include <llvm/ADT/SmallVector.h>		// for small vector
#include <llvm/ADT/DenseSet.h>		// for dense map, set
#include <llvm/ADT/SparseBitVector.h>	// for points-to
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Support/raw_ostream.h>	// for output
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

#include <vector>
#include <list>
#include <set>
#include <map>
#include <stack>
#include <deque>

typedef unsigned NodeID;
typedef unsigned EdgeID;
typedef unsigned SymID;
typedef unsigned CallSiteID;
typedef unsigned ThreadID;
typedef unsigned u32_t;
typedef unsigned long long u64_t;
typedef signed s32_t;
typedef signed long Size_t;

typedef llvm::SparseBitVector<> NodeBS;
typedef NodeBS PointsTo;
typedef PointsTo AliasSet;

typedef std::pair<NodeID, NodeID> NodePair;
typedef std::set<NodeID> NodeSet;
typedef llvm::DenseSet<NodePair,llvm::DenseMapInfo<std::pair<NodeID,NodeID> > > NodePairSet;
typedef llvm::DenseMap<NodePair,NodeID,llvm::DenseMapInfo<std::pair<NodeID,NodeID> > > NodePairMap;
typedef std::vector<NodeID> NodeVector;
typedef std::vector<EdgeID> EdgeVector;
typedef std::stack<NodeID> NodeStack;
typedef std::list<NodeID> NodeList;
typedef std::deque<NodeID> NodeDeque;
typedef llvm::SmallVector<u32_t,16> SmallVector16;
typedef llvm::SmallVector<u32_t,8> SmallVector8;
typedef NodeSet EdgeSet;
typedef SmallVector16 CallStrCxt;
typedef llvm::StringMap<u32_t> StringMap;

/// LLVM debug macros, define type of your DEBUG model of each pass
#define DBOUT(TYPE, X) 	DEBUG_WITH_TYPE(TYPE, X)
#define DOSTAT(X) 	X
#define DOTIMESTAT(X) 	X

/// General debug flag is for each phase of a pass, it is often in a colorful output format
#define DGENERAL "general"

#define DPAGBuild "pag"
#define DMemModel "mm"
#define DMemModelCE "mmce"
#define DCOMModel "comm"
#define DDDA "dda"
#define DDumpPT "dumppt"
#define DRefinePT "sbpt"
#define DCache "cache"
#define DWPA "wpa"
#define DMSSA "mssa"
#define DInstrument "ins"
#define DAndersen "ander"
#define DSaber "saber"
#define DMTA "mta"
#define DCHA "cha"

/*
 * Number of clock ticks per second. A clock tick is the unit by which
 * processor time is measured and is returned by 'clock'.
 */
#define TIMEINTERVAL 1000
#define CLOCK_IN_MS() (clock() / (CLOCKS_PER_SEC / TIMEINTERVAL))

class BddCond;


/// LLVM Basic classes
typedef llvm::SMDiagnostic SMDiagnostic;
typedef llvm::LLVMContext LLVMContext;
typedef llvm::Type Type;
typedef llvm::Function Function;
typedef llvm::BasicBlock BasicBlock;
typedef llvm::Value Value;
typedef llvm::Instruction Instruction;
typedef llvm::CallSite CallSite;
typedef llvm::GlobalValue GlobalValue;
typedef llvm::GlobalVariable GlobalVariable;
typedef llvm::Module Module;
typedef llvm::CallGraph LLVMCallGraph;
typedef llvm::User User;
typedef llvm::Use Use;
typedef llvm::Loop Loop;
typedef llvm::LoopInfo LoopInfo;
typedef llvm::UnifyFunctionExitNodes UnifyFunctionExitNodes;
typedef llvm::ModulePass ModulePass;
typedef llvm::AnalysisUsage AnalysisUsage;

/// LLVM outputs
typedef llvm::raw_ostream raw_ostream;
typedef llvm::raw_string_ostream raw_string_ostream;
typedef llvm::raw_fd_ostream raw_fd_ostream;
typedef llvm::StringRef StringRef;
typedef llvm::ToolOutputFile ToolOutputFile;

/// LLVM types
typedef llvm::StructType StructType;
typedef llvm::ArrayType ArrayType;
typedef llvm::PointerType PointerType;
typedef llvm::FunctionType FunctionType;
typedef llvm::VectorType VectorType;

/// LLVM data layout
typedef llvm::DataLayout DataLayout;
typedef llvm::StructLayout StructLayout;
typedef llvm::SmallVector<BasicBlock*, 8> SmallBBVector;
typedef llvm::ConstantStruct ConstantStruct;
typedef llvm::MemoryLocation MemoryLocation;

/// LLVM Aliases and constants
typedef llvm::Argument Argument;
typedef llvm::Constant Constant;
typedef llvm::ConstantData ConstantData;
typedef llvm::ConstantExpr ConstantExpr;
typedef llvm::ConstantAggregate ConstantAggregate;
typedef llvm::ConstantPointerNull ConstantPointerNull;
typedef llvm::ConstantArray ConstantArray;
typedef llvm::GlobalAlias GlobalAlias;
typedef llvm::AliasResult AliasResult;
typedef llvm::AnalysisID AnalysisID;
typedef llvm::ConstantDataArray ConstantDataArray;

/// LLVM metadata
typedef llvm::NamedMDNode NamedMDNode;
typedef llvm::MDString MDString;
typedef llvm::MDNode MDNode;


/// LLVM Instructions
typedef llvm::AllocaInst AllocaInst;
typedef llvm::CallInst CallInst;
typedef llvm::InvokeInst InvokeInst;
typedef llvm::StoreInst StoreInst;
typedef llvm::LoadInst LoadInst;
typedef llvm::PHINode PHINode;
typedef llvm::GetElementPtrInst GetElementPtrInst;
typedef llvm::CastInst CastInst;
typedef llvm::BitCastInst BitCastInst;
typedef llvm::ReturnInst ReturnInst;
typedef llvm::ConstantInt ConstantInt;
typedef llvm::SelectInst SelectInst;
typedef llvm::IntToPtrInst IntToPtrInst;
typedef llvm::CmpInst CmpInst;
typedef llvm::BranchInst BranchInst;
typedef llvm::SwitchInst SwitchInst;
typedef llvm::ExtractValueInst  ExtractValueInst;
typedef llvm::InsertValueInst InsertValueInst;
typedef llvm::BinaryOperator BinaryOperator;
typedef llvm::PtrToIntInst PtrToIntInst;
typedef llvm::VAArgInst VAArgInst;
typedef llvm::ExtractElementInst ExtractElementInst;
typedef llvm::InsertElementInst InsertElementInst;
typedef llvm::ShuffleVectorInst ShuffleVectorInst;
typedef llvm::LandingPadInst LandingPadInst;
typedef llvm::ResumeInst ResumeInst;
typedef llvm::UnreachableInst UnreachableInst;
typedef llvm::FenceInst FenceInst;
typedef llvm::AtomicCmpXchgInst AtomicCmpXchgInst;
typedef llvm::AtomicRMWInst AtomicRMWInst;
typedef llvm::UndefValue UndefValue;
#if (LLVM_VERSION_MAJOR >= 9)
typedef llvm::FunctionCallee FunctionCallee;
#endif

/// LLVM scalar evolution
typedef llvm::ScalarEvolutionWrapperPass ScalarEvolutionWrapperPass;
typedef llvm::ScalarEvolution ScalarEvolution;
typedef llvm::SCEVAddRecExpr SCEVAddRecExpr;
typedef llvm::SCEVConstant SCEVConstant;
typedef llvm::SCEV SCEV;

/// LLVM Dominators
typedef llvm::DominanceFrontier DominanceFrontier;
typedef llvm::DominatorTree DominatorTree;
typedef llvm::PostDominatorTree PostDominatorTree;
typedef llvm::DomTreeNode DomTreeNode;
typedef llvm::DominanceFrontierBase<BasicBlock, false> DominanceFrontierBase;
typedef llvm::PostDominatorTreeWrapperPass PostDominatorTreeWrapperPass;

/// LLVM Iterators
typedef llvm::inst_iterator inst_iterator;
typedef llvm::succ_const_iterator succ_const_iterator;
typedef llvm::const_inst_iterator const_inst_iterator;
typedef llvm::const_pred_iterator const_pred_iterator;
typedef llvm::gep_type_iterator gep_type_iterator;
typedef llvm::bridge_gep_iterator bridge_gep_iterator;
typedef llvm::GraphPrinter GraphPrinter;
typedef llvm::IRBuilder<> IRBuilder;
typedef llvm::IntegerType IntegerType;

/// LLVM debug information
typedef llvm::DebugInfoFinder DebugInfoFinder;
typedef llvm::DIType DIType;
typedef llvm::DIBasicType DIBasicType;
typedef llvm::DICompositeType DICompositeType;
typedef llvm::DIDerivedType DIDerivedType;
typedef llvm::DISubroutineType DISubroutineType;
typedef llvm::DISubprogram DISubprogram;
namespace dwarf = llvm::dwarf;

#endif /* BASICTYPES_H_ */
