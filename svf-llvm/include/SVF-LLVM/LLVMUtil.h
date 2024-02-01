//===- LLVMUtil.h -- Analysis helper functions----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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

/*
 * LLVMUtil.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVF_FE_LLVMUTIL_H_
#define INCLUDE_SVF_FE_LLVMUTIL_H_

#include "Util/SVFUtil.h"
#include "SVF-LLVM/BasicTypes.h"
#include "SVFIR/SVFValue.h"
#include "Util/ThreadAPI.h"

namespace SVF
{

namespace LLVMUtil
{

/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Instruction* inst)
{
    return SVFUtil::isa<CallBase>(inst);
}
/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Value* val)
{
    return SVFUtil::isa<CallBase>(val);
}

/// Get the definition of a function across multiple modules
const Function* getDefFunForMultipleModule(const Function* fun);

/// Return LLVM callsite given a value
inline const CallBase* getLLVMCallSite(const Value* value)
{
    assert(isCallSite(value) && "not a callsite?");
    return SVFUtil::cast<CallBase>(value);
}

inline const Function* getCallee(const CallBase* cs)
{
    // FIXME: do we need to strip-off the casts here to discover more library functions
    const Function* callee = SVFUtil::dyn_cast<Function>(cs->getCalledOperand()->stripPointerCasts());
    return callee ? getDefFunForMultipleModule(callee) : nullptr;
}

/// Return LLVM function if this value is
inline const Function* getLLVMFunction(const Value* val)
{
    return SVFUtil::dyn_cast<Function>(val->stripPointerCasts());
}

/// Get program entry function from module.
const Function* getProgFunction(const std::string& funName);

/// Check whether a function is an entry function (i.e., main)
inline bool isProgEntryFunction(const Function* fun)
{
    return fun && fun->getName() == "main";
}

/// Check whether this value is a black hole
inline bool isBlackholeSym(const Value* val)
{
    return SVFUtil::isa<UndefValue>(val);
}

/// Check whether this value is a black hole
inline bool isNullPtrSym(const Value* val)
{
    return SVFUtil::dyn_cast<ConstantPointerNull>(val);
}

static inline Type* getPtrElementType(const PointerType* pty)
{
#if (LLVM_VERSION_MAJOR < 14)
    return pty->getPointerElementType();
#elif (LLVM_VERSION_MAJOR < 17)
    assert(!pty->isOpaque() && "Opaque Pointer is used, please recompile the source adding '-Xclang -no-opaque-pointers'");
    return pty->getNonOpaquePointerElementType();
#else
    assert(false && "llvm version 17+ only support opaque pointers!");
#endif
}

/// Return size of this object based on LLVM value
u32_t getNumOfElements(const Type* ety);


/// Return true if this value refers to a object
bool isObject(const Value* ref);

/// Method for dead function, which does not have any possible caller
/// function address is not taken and never be used in call or invoke instruction
//@{
/// whether this is a function without any possible caller?
bool isUncalledFunction(const Function* fun);

/// whether this is an argument in dead function
inline bool ArgInDeadFunction(const Value* val)
{
    return SVFUtil::isa<Argument>(val)
           && isUncalledFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Return true if this is an argument of a program entry function (e.g. main)
inline bool ArgInProgEntryFunction(const Value* val)
{
    return SVFUtil::isa<Argument>(val) &&
           LLVMUtil::isProgEntryFunction(
               SVFUtil::cast<Argument>(val)->getParent());
}
/// Return true if this is value in a dead function (function without any caller)
bool isPtrInUncalledFunction(const Value* value);
//@}

//@}

/// Function does not have any possible caller in the call graph
//@{
/// Return true if the function does not have a caller (either it is a main function or a dead function)
inline bool isNoCallerFunction(const Function* fun)
{
    return isUncalledFunction(fun) || LLVMUtil::isProgEntryFunction(fun);
}

/// Return true if the argument in a function does not have a caller
inline bool isArgOfUncalledFunction (const Value*  val)
{
    return SVFUtil::isa<Argument>(val)
           && isNoCallerFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Return true if the function has a return instruction
bool basicBlockHasRetInst(const BasicBlock* bb);

/// Return true if the function has a return instruction reachable from function
/// entry
bool functionDoesNotRet(const Function* fun);

/// Get reachable basic block from function entry
void getFunReachableBBs(const Function* svfFun,
                        std::vector<const SVFBasicBlock*>& bbs);

/// Strip off the constant casts
const Value* stripConstantCasts(const Value* val);

/// Strip off the all casts
const Value* stripAllCasts(const Value* val);

/// Return the bitcast instruction right next to val, otherwise
/// return nullptr
const Value* getFirstUseViaCastInst(const Value* val);

/// Return corresponding constant expression, otherwise return nullptr
//@{
inline const ConstantExpr* isGepConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::GetElementPtr)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isInt2PtrConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::IntToPtr)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isPtr2IntConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::PtrToInt)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isCastConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::BitCast)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isSelectConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::Select)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isTruncConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::Trunc ||
                constExpr->getOpcode() == Instruction::FPTrunc ||
                constExpr->getOpcode() == Instruction::ZExt ||
                constExpr->getOpcode() == Instruction::SExt ||
                constExpr->getOpcode() == Instruction::FPExt)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isCmpConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if (constExpr->getOpcode() == Instruction::ICmp ||
                constExpr->getOpcode() == Instruction::FCmp)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isBinaryConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if ((constExpr->getOpcode() >= Instruction::BinaryOpsBegin) &&
                (constExpr->getOpcode() <= Instruction::BinaryOpsEnd))
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr* isUnaryConstantExpr(const Value* val)
{
    if (const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if ((constExpr->getOpcode() >= Instruction::UnaryOpsBegin) &&
                (constExpr->getOpcode() <= Instruction::UnaryOpsEnd))
            return constExpr;
    }
    return nullptr;
}
//@}

inline static DataLayout* getDataLayout(Module* mod)
{
    static DataLayout *dl = nullptr;
    if (dl == nullptr)
        dl = new DataLayout(mod);
    return dl;
}

/// Get the next instructions following control flow
void getNextInsts(const Instruction* curInst,
                  std::vector<const SVFInstruction*>& instList);

/// Get the previous instructions following control flow
void getPrevInsts(const Instruction* curInst,
                  std::vector<const SVFInstruction*>& instList);

/// Get the next instructions following control flow
void getNextInsts(const Instruction* curInst,
                  std::vector<const Instruction*>& instList);

/// Get the previous instructions following control flow
void getPrevInsts(const Instruction* curInst,
                  std::vector<const Instruction*>& instList);

/// Get num of BB's predecessors
u32_t getBBPredecessorNum(const BasicBlock* BB);

/// Check whether a file is an LLVM IR file
bool isIRFile(const std::string& filename);

/// Parse argument for multi-module analysis
void processArguments(int argc, char** argv, int& arg_num, char** arg_value,
                      std::vector<std::string>& moduleNameVec);

/// Helper method to get the size of the type from target data layout
//@{
u32_t getTypeSizeInBytes(const Type* type);
u32_t getTypeSizeInBytes(const StructType* sty, u32_t field_index);
//@}

const std::string getSourceLoc(const Value* val);
const std::string getSourceLocOfFunction(const Function* F);

bool isIntrinsicInst(const Instruction* inst);
bool isIntrinsicFun(const Function* func);

/// Get all called funcions in a parent function
std::vector<const Function *> getCalledFunctions(const Function *F);
void removeFunAnnotations(Set<Function*>& removedFuncList);
bool isUnusedGlobalVariable(const GlobalVariable& global);
void removeUnusedGlobalVariables(Module* module);
/// Delete unused functions, annotations and global variables in extapi.bc
void removeUnusedFuncsAndAnnotationsAndGlobalVariables(Set<Function*> removedFuncList);

/// Get the corresponding Function based on its name
const SVFFunction* getFunction(const std::string& name);

/// Return true if the value refers to constant data, e.g., i32 0
inline bool isConstDataOrAggData(const Value* val)
{
    return SVFUtil::isa<ConstantData, ConstantAggregate,
           MetadataAsValue, BlockAddress>(val);
}

/// find the unique defined global across multiple modules
const Value* getGlobalRep(const Value* val);

/// Check whether this value points-to a constant object
bool isConstantObjSym(const SVFValue* val);

/// Check whether this value points-to a constant object
bool isConstantObjSym(const Value* val);

// Dump Control Flow Graph of llvm function, with instructions
void viewCFG(const Function* fun);

// Dump Control Flow Graph of llvm function, without instructions
void viewCFGOnly(const Function* fun);

std::string dumpValue(const Value* val);

std::string dumpType(const Type* type);

std::string dumpValueAndDbgInfo(const Value* val);

/**
 * See more: https://github.com/SVF-tools/SVF/pull/1191
 *
 * Given the code:
 *
 * switch (a) {
 *   case 0: printf("0\n"); break;
 *   case 1:
 *   case 2:
 *   case 3: printf("a >=1 && a <= 3\n"); break;
 *   case 4:
 *   case 6:
 *   case 7:  printf("a >= 4 && a <=7\n"); break;
 *   default: printf("a < 0 || a > 7"); break;
 * }
 *
 * Generate the IR:
 *
 * switch i32 %0, label %sw.default [
 *  i32 0, label %sw.bb
 *  i32 1, label %sw.bb1
 *  i32 2, label %sw.bb1
 *  i32 3, label %sw.bb1
 *  i32 4, label %sw.bb3
 *  i32 6, label %sw.bb3
 *  i32 7, label %sw.bb3
 * ]
 *
 * We can get every case basic block and related case value:
 * [
 *   {%sw.default, -1},
 *   {%sw.bb, 0},
 *   {%sw.bb1, 1},
 *   {%sw.bb1, 2},
 *   {%sw.bb1, 3},
 *   {%sw.bb3, 4},
 *   {%sw.bb3, 6},
 *   {%sw.bb3, 7},
 * ]
 * Note: default case value is nullptr
 */
void getSuccBBandCondValPairVec(const SwitchInst &switchInst, SuccBBAndCondValPairVec &vec);

/**
 * Note: default case value is nullptr
 */
s64_t getCaseValue(const SwitchInst &switchInst, SuccBBAndCondValPair &succBB2CondVal);

} // End namespace LLVMUtil

} // End namespace SVF

#endif /* INCLUDE_SVF_FE_LLVMUTIL_H_ */
