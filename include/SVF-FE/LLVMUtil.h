//===- LLVMUtil.h -- Analysis helper functions----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * LLVMUtil.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#ifndef INCLUDE_SVF_FE_LLVMUTIL_H_
#define INCLUDE_SVF_FE_LLVMUTIL_H_

#include "SVF-FE/BasicTypes.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/BasicTypes.h"
#include "Util/BasicTypes.h"
#include "Util/ExtAPI.h"
#include "Util/ThreadAPI.h"

namespace SVF
{

namespace LLVMUtil
{

/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const SVFInstruction* inst)
{
    return SVFUtil::isa<CallBase>(inst->getLLVMInstruction());
}
/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Value* val)
{
    if(SVFUtil::isa<CallBase>(val))
        return true;
    else
        return false;
}

/// Return LLVM callsite given a value
inline CallSite getLLVMCallSite(const Value* value)
{
    assert(isCallSite(value) && "not a callsite?");
    const SVFInstruction* svfInst = LLVMModuleSet::getLLVMModuleSet()->getSVFInstruction(SVFUtil::cast<CallBase>(value));
    CallSite cs(svfInst);
    return cs;
}

/// Return LLVM function if this value is
inline const Function* getLLVMFunction(const Value* val)
{
    const Function* fun = SVFUtil::dyn_cast<Function>(val->stripPointerCasts());
    return fun;
}

/// Check whether this value is a black hole
inline bool isBlackholeSym(const Value* val)
{
    return (SVFUtil::isa<UndefValue>(val));
}

/// Check whether this value is a black hole
inline bool isNullPtrSym(const Value* val)
{
    return SVFUtil::dyn_cast<ConstantPointerNull>(val);
}

/// Check whether this value points-to a constant object
bool isConstantObjSym(const Value* val);

static inline Type *getPtrElementType(const PointerType* pty)
{
#if (LLVM_VERSION_MAJOR < 14)
    return pty->getElementType();
#else
    assert(!pty->isOpaque() && "Opaque Pointer is used, please recompile the source adding '-Xclang -no-opaque-pointer'");
    return pty->getNonOpaquePointerElementType();
#endif
}

/// Get the reference type of heap/static object from an allocation site.
//@{
inline const PointerType *getRefTypeOfHeapAllocOrStatic(const CallSite cs)
{
    const PointerType *refType = nullptr;
    // Case 1: heap object held by *argument, we should get its element type.
    if (SVFUtil::isHeapAllocExtCallViaArg(cs))
    {
        int argPos = SVFUtil::getHeapAllocHoldingArgPosition(cs);
        const SVFValue* arg = cs.getArgument(argPos);
        if (const PointerType *argType = SVFUtil::dyn_cast<PointerType>(arg->getType()))
            refType = SVFUtil::dyn_cast<PointerType>(getPtrElementType(argType));
    }
    // Case 2: heap/static object held by return value.
    else
    {
        assert((SVFUtil::isStaticExtCall(cs) || SVFUtil::isHeapAllocExtCallViaRet(cs))
               && "Must be heap alloc via ret, or static allocation site");
        refType = SVFUtil::dyn_cast<PointerType>(cs.getType());
    }
    assert(refType && "Allocated object must be held by a pointer-typed value.");
    return refType;
}

inline const PointerType *getRefTypeOfHeapAllocOrStatic(const SVFInstruction* inst)
{
    CallSite cs(inst);
    return getRefTypeOfHeapAllocOrStatic(cs);
}
//@}

/// Return true if this value refers to a object
bool isObject (const Value*  ref);

/// Method for dead function, which does not have any possible caller
/// function address is not taken and never be used in call or invoke instruction
//@{
/// whether this is a function without any possible caller?
bool isUncalledFunction (const Function*  fun);

/// whether this is an argument in dead function
inline bool ArgInDeadFunction (const Value*  val)
{
    return SVFUtil::isa<Argument>(val)
           && isUncalledFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Return true if this is an argument of a program entry function (e.g. main)
inline bool ArgInProgEntryFunction (const Value*  val)
{
    return SVFUtil::isa<Argument>(val)
           && SVFUtil::isProgEntryFunction(SVFUtil::cast<Argument>(val)->getParent());
}
/// Return true if this is value in a dead function (function without any caller)
bool isPtrInUncalledFunction (const Value*  value);
//@}

//@}

/// Function does not have any possible caller in the call graph
//@{
/// Return true if the function does not have a caller (either it is a main function or a dead function)
inline bool isNoCallerFunction (const Function*  fun)
{
    return isUncalledFunction(fun) || SVFUtil::isProgEntryFunction(fun);
}

/// Return true if the argument in a function does not have a caller
inline bool isArgOfUncalledFunction (const Value*  val)
{
    return SVFUtil::isa<Argument>(val)
           && isNoCallerFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Return true if the function has a return instruction reachable from function entry
bool functionDoesNotRet (const Function*  fun);

/// Get reachable basic block from function entry
void getFunReachableBBs (const SVFFunction* svfFun, std::vector<const SVFBasicBlock*>& bbs);

/// Strip off the constant casts
const Value*  stripConstantCasts(const Value* val);

/// Strip off the all casts
const Value* stripAllCasts(const Value* val) ;

/// Get the type of the heap allocation
const Type *getTypeOfHeapAlloc(const SVFInstruction* inst) ;

/// Return the bitcast instruction which is val's only use site, otherwise return nullptr
const Value* getUniqueUseViaCastInst(const Value* val);

/// Return corresponding constant expression, otherwise return nullptr
//@{
inline const ConstantExpr *isGepConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::GetElementPtr)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isInt2PtrConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::IntToPtr)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isPtr2IntConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::PtrToInt)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isCastConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::BitCast)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isSelectConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::Select)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isTruncConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::Trunc ||
                constExpr->getOpcode() == Instruction::FPTrunc ||
                constExpr->getOpcode() == Instruction::ZExt ||
                constExpr->getOpcode() == Instruction::SExt ||
                constExpr->getOpcode() == Instruction::FPExt)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isCmpConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::ICmp || constExpr->getOpcode() == Instruction::FCmp)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isBinaryConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if((constExpr->getOpcode() >= Instruction::BinaryOpsBegin) && (constExpr->getOpcode() <= Instruction::BinaryOpsEnd))
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isUnaryConstantExpr(const Value* val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if((constExpr->getOpcode() >= Instruction::UnaryOpsBegin) && (constExpr->getOpcode() <= Instruction::UnaryOpsEnd))
            return constExpr;
    }
    return nullptr;
}
//@}

inline static DataLayout* getDataLayout(Module* mod)
{
    static DataLayout *dl = nullptr;
    if (dl == nullptr) dl = new DataLayout(mod);
    return dl;
}

/// Get the next instructions following control flow
void getNextInsts(const SVFInstruction* curInst, std::vector<const SVFInstruction*>& instList);

/// Get the previous instructions following control flow
void getPrevInsts(const SVFInstruction* curInst, std::vector<const SVFInstruction*>& instList);

/// Get num of BB's predecessors
u32_t getBBPredecessorNum(const BasicBlock* BB);



/// Check whether a file is an LLVM IR file
bool isIRFile(const std::string &filename);

/// Parse argument for multi-module analysis
void processArguments(int argc, char **argv, int &arg_num, char **arg_value,
                      std::vector<std::string> &moduleNameVec);

/// Helper method to get the size of the type from target data layout
//@{
u32_t getTypeSizeInBytes(const Type* type);
u32_t getTypeSizeInBytes(const StructType *sty, u32_t field_index);
//@}

const std::string getSourceLoc(const Value* val);
const std::string getSourceLocOfFunction(const Function* F);
const std::string value2String(const Value* value);
const std::string value2ShortString(const Value* value);

bool isIntrinsicInst(const Instruction* inst);
bool isIntrinsicFun(const Function* func);

/// Get the corresponding Function based on its name
inline const SVFFunction* getFunction(std::string name)
{
    return LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(name);
}

/// Return true if the value refers to constant data, e.g., i32 0
inline bool isConstantOrMetaData(const Value* val)
{
    return SVFUtil::isa<ConstantData>(val)
           || SVFUtil::isa<ConstantAggregate>(val)
           || SVFUtil::isa<MetadataAsValue>(val)
           || SVFUtil::isa<BlockAddress>(val);
}

/// Get the definition of a function across multiple modules
inline const Function* getDefFunForMultipleModule(const Function* fun)
{
    if(fun == nullptr) return nullptr;
    LLVMModuleSet* llvmModuleset = LLVMModuleSet::getLLVMModuleSet();
    if (fun->isDeclaration() && llvmModuleset->hasDefinition(fun))
        fun = LLVMModuleSet::getLLVMModuleSet()->getDefinition(fun);
    return fun;
}

/// find the unique defined global across multiple modules
inline const Value* getGlobalRep(const Value* val)
{
    if(const GlobalVariable* gvar = SVFUtil::dyn_cast<GlobalVariable>(val))
    {
        if (LLVMModuleSet::getLLVMModuleSet()->hasGlobalRep(gvar))
            val = LLVMModuleSet::getLLVMModuleSet()->getGlobalRep(gvar);
    }
    return val;
}


// Dump Control Flow Graph of llvm function, with instructions
void viewCFG(const Function* fun);

// Dump Control Flow Graph of llvm function, without instructions
void viewCFGOnly(const Function* fun);

} // End namespace LLVMUtil

} // End namespace SVF

#endif /* INCLUDE_SVF_FE_LLVMUTIL_H_ */
