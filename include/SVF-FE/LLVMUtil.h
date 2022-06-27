//===- SVFUtil.h -- Analysis helper functions----------------------------//
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
 * SVFUtil.h
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


/// Check whether this value is a black hole
inline bool isBlackholeSym(const Value *val)
{
    return (SVFUtil::isa<UndefValue>(val));
}

/// Check whether this value is a black hole
inline bool isNullPtrSym(const Value *val)
{
    if (const Constant* v = SVFUtil::dyn_cast<Constant>(val))
    {
        return v->isNullValue() && v->getType()->isPointerTy();
    }
    return false;
}

/// Check whether this value points-to a constant object
bool isConstantObjSym(const Value *val);


/// Whether an instruction is a return instruction
inline bool isReturn(const Instruction* inst)
{
    return SVFUtil::isa<ReturnInst>(inst);
}

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
        const Value *arg = cs.getArgument(argPos);
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

inline const PointerType *getRefTypeOfHeapAllocOrStatic(const Instruction *inst)
{
    CallSite cs(const_cast<Instruction*>(inst));
    return getRefTypeOfHeapAllocOrStatic(cs);
}
//@}

/// Return true if this value refers to a object
bool isObject (const Value * ref);

/// Method for dead function, which does not have any possible caller
/// function address is not taken and never be used in call or invoke instruction
//@{
/// whether this is a function without any possible caller?
bool isDeadFunction (const Function * fun);

/// whether this is an argument in dead function
inline bool ArgInDeadFunction (const Value * val)
{
    return SVFUtil::isa<Argument>(val)
           && isDeadFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Return true if this is an argument of a program entry function (e.g. main)
inline bool ArgInProgEntryFunction (const Value * val)
{
    return SVFUtil::isa<Argument>(val)
           && SVFUtil::isProgEntryFunction(SVFUtil::cast<Argument>(val)->getParent());
}
/// Return true if this is value in a dead function (function without any caller)
bool isPtrInDeadFunction (const Value * value);
//@}

//@}

/// Function does not have any possible caller in the call graph
//@{
/// Return true if the function does not have a caller (either it is a main function or a dead function)
inline bool isNoCallerFunction (const Function * fun)
{
    return isDeadFunction(fun) || SVFUtil::isProgEntryFunction(fun);
}

/// Return true if the argument in a function does not have a caller
inline bool ArgInNoCallerFunction (const Value * val)
{
    return SVFUtil::isa<Argument>(val)
           && isNoCallerFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Return true if the function has a return instruction reachable from function entry
bool functionDoesNotRet (const Function * fun);

/// Get reachable basic block from function entry
void getFunReachableBBs (const Function * fun, DominatorTree* dt,std::vector<const BasicBlock*>& bbs);

/// Get function exit basic block
/// FIXME: this back() here is only valid when UnifyFunctionExitNodes pass is invoked
inline const BasicBlock* getFunExitBB(const Function* fun)
{
    return &fun->back();
}
/// Strip off the constant casts
const Value * stripConstantCasts(const Value *val);

/// Strip off the all casts
const Value *stripAllCasts(const Value *val) ;

/// Get the type of the heap allocation
const Type *getTypeOfHeapAlloc(const llvm::Instruction *inst) ;

/// Return the bitcast instruction which is val's only use site, otherwise return nullptr
const Value* getUniqueUseViaCastInst(const Value* val);

/// Return corresponding constant expression, otherwise return nullptr
//@{
inline const ConstantExpr *isGepConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::GetElementPtr)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isInt2PtrConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::IntToPtr)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isPtr2IntConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::PtrToInt)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isCastConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::BitCast)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isSelectConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::Select)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isTruncConstantExpr(const Value *val)
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

inline const ConstantExpr *isCmpConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if(constExpr->getOpcode() == Instruction::ICmp || constExpr->getOpcode() == Instruction::FCmp)
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isBinaryConstantExpr(const Value *val)
{
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val))
    {
        if((constExpr->getOpcode() >= Instruction::BinaryOpsBegin) && (constExpr->getOpcode() <= Instruction::BinaryOpsEnd))
            return constExpr;
    }
    return nullptr;
}

inline const ConstantExpr *isUnaryConstantExpr(const Value *val)
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
void getNextInsts(const Instruction* curInst, std::vector<const Instruction*>& instList);

/// Get the previous instructions following control flow
void getPrevInsts(const Instruction* curInst, std::vector<const Instruction*>& instList);

/// Get basic block successor position
u32_t getBBSuccessorPos(const BasicBlock *BB, const BasicBlock *Succ);
/// Get num of BB's successors
u32_t getBBSuccessorNum(const BasicBlock *BB);
/// Get basic block predecessor positin
u32_t getBBPredecessorPos(const BasicBlock *BB, const BasicBlock *Pred);
/// Get num of BB's predecessors
u32_t getBBPredecessorNum(const BasicBlock *BB);



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


} // End namespace LLVMUtil

} // End namespace SVF

#endif /* INCLUDE_SVF_FE_LLVMUTIL_H_ */
