//===- AnalysisUtil.h -- Analysis helper functions----------------------------//
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
 * AnalysisUtil.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui, dye
 */

#ifndef AnalysisUtil_H_
#define AnalysisUtil_H_

#include "Util/ExtAPI.h"
#include "Util/ThreadAPI.h"
#include "Util/BasicTypes.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>				// constants
#include <llvm/IR/CallSite.h>		// callsite
#include <llvm/ADT/SparseBitVector.h>	// sparse bit vector
#include <llvm/IR/Dominators.h>	// dominator tree
#include <llvm/Support/Debug.h>		// debug with types
#include <time.h>

/*
 * Util class to assist pointer analysis
 */
namespace analysisUtil {

/// This function servers a allocation wrapper detector
inline bool isAnAllocationWraper(const llvm::Instruction *inst) {
    return false;
}
/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const llvm::Instruction* inst) {
    return llvm::isa<llvm::CallInst>(inst)|| llvm::isa<llvm::InvokeInst>(inst);
}
/// Whether an instruction is a return instruction
inline bool isReturn(const llvm::Instruction* inst) {
    return llvm::isa<llvm::ReturnInst>(inst);
}
/// Return LLVM function if this value is
inline const llvm::Function* getLLVMFunction(const llvm::Value* val) {
    const llvm::Function *fun = llvm::dyn_cast<llvm::Function>(val->stripPointerCasts());
    return fun;
}
/// Return LLVM callsite given a instruction
inline llvm::CallSite getLLVMCallSite(const llvm::Instruction* inst) {
    assert(llvm::isa<llvm::CallInst>(inst)|| llvm::isa<llvm::InvokeInst>(inst));
    llvm::CallSite cs(const_cast<llvm::Instruction*>(inst));
    return cs;
}
/// Return callee of a callsite. Return null if this is an indirect call
//@{
inline const llvm::Function* getCallee(const llvm::CallSite cs) {
    // FIXME: do we need to strip-off the casts here to discover more library functions
    llvm::Function *callee = llvm::dyn_cast<llvm::Function>(cs.getCalledValue()->stripPointerCasts());
    return callee;
}

inline const llvm::Function* getCallee(const llvm::Instruction *inst) {
    if (!llvm::isa<llvm::CallInst>(inst) && !llvm::isa<llvm::InvokeInst>(inst))
        return NULL;
    llvm::CallSite cs(const_cast<llvm::Instruction*>(inst));
    return getCallee(cs);
}
//@}

/// Return true if the call is an external call (external library in function summary table)
//@{
inline bool isExtCall(const llvm::Function* fun) {
    return fun && ExtAPI::getExtAPI()->is_ext(fun);
}

inline bool isExtCall(const llvm::CallSite cs) {
    return isExtCall(getCallee(cs));
}

inline bool isExtCall(const llvm::Instruction *inst) {
    return isExtCall(getCallee(inst));
}
//@}

/// Return true if the call is a heap allocator/reallocator
//@{
/// note that these two functions are not suppose to be used externally
inline bool isHeapAllocExtFunViaRet(const llvm::Function *fun) {
    return fun && (ExtAPI::getExtAPI()->is_alloc(fun)
            || ExtAPI::getExtAPI()->is_realloc(fun));
}
inline bool isHeapAllocExtFunViaArg(const llvm::Function *fun) {
    return fun && ExtAPI::getExtAPI()->is_arg_alloc(fun);
}

/// interfaces to be used externally
inline bool isHeapAllocExtCallViaRet(const llvm::CallSite cs) {
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(getCallee(cs));
}

inline bool isHeapAllocExtCallViaRet(const llvm::Instruction *inst) {
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(getCallee(inst));
}

inline bool isHeapAllocExtCallViaArg(const llvm::CallSite cs) {
    return isHeapAllocExtFunViaArg(getCallee(cs));
}

inline bool isHeapAllocExtCallViaArg(const llvm::Instruction *inst) {
    return isHeapAllocExtFunViaArg(getCallee(inst));
}

inline bool isHeapAllocExtCall(const llvm::CallSite cs) {
    return isHeapAllocExtCallViaRet(cs) || isHeapAllocExtCallViaArg(cs);
}

inline bool isHeapAllocExtCall(const llvm::Instruction *inst) {
    return isHeapAllocExtCallViaRet(inst) || isHeapAllocExtCallViaArg(inst);
}
//@}

/// Get the position of argument that holds an allocated heap object.
//@{
inline int getHeapAllocHoldingArgPosition(const llvm::Function *fun) {
    return ExtAPI::getExtAPI()->get_alloc_arg_pos(fun);
}

inline int getHeapAllocHoldingArgPosition(const llvm::CallSite cs) {
    return getHeapAllocHoldingArgPosition(getCallee(cs));
}

inline int getHeapAllocHoldingArgPosition(const llvm::Instruction *inst) {
    return getHeapAllocHoldingArgPosition(getCallee(inst));
}
//@}

/// Return true if the call is a heap reallocator
//@{
/// note that this function is not suppose to be used externally
inline bool isReallocExtFun(const llvm::Function *fun) {
    return fun && (ExtAPI::getExtAPI()->is_realloc(fun));
}

inline bool isReallocExtCall(const llvm::CallSite cs) {
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(getCallee(cs));
}

inline bool isReallocExtCall(const llvm::Instruction *inst) {
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(getCallee(inst));
}
//@}

/// Return true if the call is a heap dealloc or not
//@{
/// note that this function is not suppose to be used externally
inline bool isDeallocExtFun(const llvm::Function *fun) {
    return fun && (ExtAPI::getExtAPI()->is_dealloc(fun));
}

inline bool isDeallocExtCall(const llvm::CallSite cs) {
    return isDeallocExtFun(getCallee(cs));
}

inline bool isDeallocExtCall(const llvm::Instruction *inst) {
    return isDeallocExtFun(getCallee(inst));
}
//@}


/// Return true if the call is a static global call
//@{
/// note that this function is not suppose to be used externally
inline bool isStaticExtFun(const llvm::Function *fun) {
    return fun && ExtAPI::getExtAPI()->has_static(fun);
}

inline bool isStaticExtCall(const llvm::CallSite cs) {
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isStaticExtFun(getCallee(cs));
}

inline bool isStaticExtCall(const llvm::Instruction *inst) {
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isStaticExtFun(getCallee(inst));
}
//@}

/// Return true if the call is a static global call
//@{
inline bool isHeapAllocOrStaticExtCall(const llvm::CallSite cs) {
    return isStaticExtCall(cs) || isHeapAllocExtCall(cs);
}

inline bool isHeapAllocOrStaticExtCall(const llvm::Instruction *inst) {
    return isStaticExtCall(inst) || isHeapAllocExtCall(inst);
}
//@}

/// Return external call type
inline ExtAPI::extf_t extCallTy(const llvm::Function* fun) {
    return ExtAPI::getExtAPI()->get_type(fun);
}

/// Get the reference type of heap/static object from an allocation site.
//@{
inline const llvm::PointerType *getRefTypeOfHeapAllocOrStatic(const llvm::CallSite cs) {
    const llvm::PointerType *refType = NULL;
    // Case 1: heap object held by *argument, we should get its element type.
    if (isHeapAllocExtCallViaArg(cs)) {
        int argPos = getHeapAllocHoldingArgPosition(cs);
        const llvm::Value *arg = cs.getArgument(argPos);
        if (const llvm::PointerType *argType = llvm::dyn_cast<llvm::PointerType>(arg->getType()))
            refType = llvm::dyn_cast<llvm::PointerType>(argType->getElementType());
    }
    // Case 2: heap/static object held by return value.
    else {
        assert((isStaticExtCall(cs) || isHeapAllocExtCallViaRet(cs))
                && "Must be heap alloc via ret, or static allocation site");
        refType = llvm::dyn_cast<llvm::PointerType>(cs.getType());
    }
    assert(refType && "Allocated object must be held by a pointer-typed value.");
    return refType;
}

inline const llvm::PointerType *getRefTypeOfHeapAllocOrStatic(const llvm::Instruction *inst) {
    llvm::CallSite cs(const_cast<llvm::Instruction*>(inst));
    return getRefTypeOfHeapAllocOrStatic(cs);
}
//@}

/// Return true if this is a thread creation call
///@{
inline bool isThreadForkCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDFork(cs);
}
inline bool isThreadForkCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDFork(inst);
}
//@}

/// Return true if this is a hare_parallel_for call
///@{
inline bool isHareParForCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isHareParFor(cs);
}
inline bool isHareParForCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isHareParFor(inst);
}
//@}

/// Return true if this is a thread join call
///@{
inline bool isThreadJoinCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDJoin(cs);
}
inline bool isThreadJoinCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDJoin(inst);
}
//@}

/// Return true if this is a thread exit call
///@{
inline bool isThreadExitCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDExit(cs);
}
inline bool isThreadExitCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDExit(inst);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockAquireCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDAcquire(cs);
}
inline bool isLockAquireCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDAcquire(inst);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockReleaseCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDRelease(cs);
}
inline bool isLockReleaseCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDRelease(inst);
}
//@}

/// Return true if this is a barrier wait call
//@{
inline bool isBarrierWaitCall(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDBarWait(cs);
}
inline bool isBarrierWaitCall(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDBarWait(inst);
}
//@}

/// Return thread fork function
//@{
inline const llvm::Value* getForkedFun(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->getForkedFun(cs);
}
inline const llvm::Value* getForkedFun(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getForkedFun(inst);
}
//@}

/// Return sole argument of the thread rountine
//@{
inline const llvm::Value* getActualParmAtForkSite(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(cs);
}
inline const llvm::Value* getActualParmAtForkSite(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(inst);
}
//@}

/// Return the task function of the parallel_for rountine
//@{
inline const llvm::Value* getTaskFuncAtHareParForSite(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->getTaskFuncAtHareParForSite(cs);
}
inline const llvm::Value* getTaskFuncAtHareParForSite(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getTaskFuncAtHareParForSite(inst);
}
//@}

/// Return the task data argument of the parallel_for rountine
//@{
inline const llvm::Value* getTaskDataAtHareParForSite(const llvm::CallSite cs) {
    return ThreadAPI::getThreadAPI()->getTaskDataAtHareParForSite(cs);
}
inline const llvm::Value* getTaskDataAtHareParForSite(const llvm::Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getTaskDataAtHareParForSite(inst);
}
//@}

/// Return true if this value refers to a object
bool isObject (const llvm::Value * ref);

/// Return true if this function is llvm dbg intrinsic function/instruction
//@{
inline bool isIntrinsicDbgFun(const llvm::Function* fun) {
    return fun->getName().startswith("llvm.dbg.declare") ||
           fun->getName().startswith("llvm.dbg.value");
}
bool isInstrinsicDbgInst(const llvm::Instruction* inst);
//@}

/// Method for dead function, which does not have any possible caller
/// function address is not taken and never be used in call or invoke instruction
//@{
/// whether this is a function without any possible caller?
bool isDeadFunction (const llvm::Function * fun);

/// whether this is an argument in dead function
inline bool ArgInDeadFunction (const llvm::Value * val) {
    return llvm::isa<llvm::Argument>(val)
           && isDeadFunction(llvm::cast<llvm::Argument>(val)->getParent());
}
//@}

/// Program entry function e.g. main
//@{
/// Return true if this is a program entry function (e.g. main)
inline bool isProgEntryFunction (const llvm::Function * fun) {
    return fun && fun->getName().str() == "main";
}

/// Get program entry function from module.
inline const llvm::Function* getProgEntryFunction(const llvm::Module* mod) {
    for (llvm::Module::const_iterator it = mod->begin(), eit = mod->end(); it != eit; ++it) {
        const llvm::Function& fun = *it;
        if (isProgEntryFunction(&fun))
            return (&fun);
    }
    return NULL;
}

/// Return true if this is an argument of a program entry function (e.g. main)
inline bool ArgInProgEntryFunction (const llvm::Value * val) {
    return llvm::isa<llvm::Argument>(val)
           && isProgEntryFunction(llvm::cast<llvm::Argument>(val)->getParent());
}
/// Return true if this is value in a dead function (function without any caller)
bool isPtrInDeadFunction (const llvm::Value * value);
//@}

/// Return true if this is a program exit function call
//@{
inline bool isProgExitFunction (const llvm::Function * fun) {
    return fun && (fun->getName().str() == "exit" ||
                   fun->getName().str() == "__assert_rtn" ||
                   fun->getName().str() == "__assert_fail" );
}

inline bool isProgExitCall(const llvm::CallSite cs) {
    return isProgExitFunction(getCallee(cs));
}

inline bool isProgExitCall(const llvm::Instruction *inst) {
    return isProgExitFunction(getCallee(inst));
}
//@}

/// Function does not have any possible caller in the call graph
//@{
/// Return true if the function does not have a caller (either it is a main function or a dead function)
inline bool isNoCallerFunction (const llvm::Function * fun) {
    return isDeadFunction(fun) || isProgEntryFunction(fun);
}

/// Return true if the argument in a function does not have a caller
inline bool ArgInNoCallerFunction (const llvm::Value * val) {
    return llvm::isa<llvm::Argument>(val)
           && isNoCallerFunction(llvm::cast<llvm::Argument>(val)->getParent());
}
//@}

/// Return true if the function has a return instruction reachable from function entry
bool functionDoesNotRet (const llvm::Function * fun);

/// Get reachable basic block from function entry
void getFunReachableBBs (const llvm::Function * fun, llvm::DominatorTree* dt,std::vector<const llvm::BasicBlock*>& bbs);

/// Get function exit basic block
/// FIXME: this back() here is only valid when UnifyFunctionExitNodes pass is invoked
inline const llvm::BasicBlock* getFunExitBB(const llvm::Function* fun) {
    return &fun->back();
}
/// Strip off the constant casts
const llvm::Value * stripConstantCasts(const llvm::Value *val);

/// Strip off the all casts
llvm::Value *stripAllCasts(llvm::Value *val) ;

/// Return corresponding constant expression, otherwise return NULL
//@{
inline const llvm::ConstantExpr *isGepConstantExpr(const llvm::Value *val) {
    if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
        if(constExpr->getOpcode() == llvm::Instruction::GetElementPtr)
            return constExpr;
    }
    return NULL;
}

inline const llvm::ConstantExpr *isInt2PtrConstantExpr(const llvm::Value *val) {
    if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
        if(constExpr->getOpcode() == llvm::Instruction::IntToPtr)
            return constExpr;
    }
    return NULL;
}

inline const llvm::ConstantExpr *isPtr2IntConstantExpr(const llvm::Value *val) {
    if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
        if(constExpr->getOpcode() == llvm::Instruction::PtrToInt)
            return constExpr;
    }
    return NULL;
}

inline const llvm::ConstantExpr *isCastConstantExpr(const llvm::Value *val) {
    if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
        if(constExpr->getOpcode() == llvm::Instruction::BitCast)
            return constExpr;
    }
    return NULL;
}

inline const llvm::ConstantExpr *isSelectConstantExpr(const llvm::Value *val) {
    if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
        if(constExpr->getOpcode() == llvm::Instruction::Select)
            return constExpr;
    }
    return NULL;
}
//@}

/// Get basic block successor position
u32_t getBBSuccessorPos(const llvm::BasicBlock *BB, const llvm::BasicBlock *Succ);
/// Get num of BB's successors
u32_t getBBSuccessorNum(const llvm::BasicBlock *BB);
/// Get basic block predecessor positin
u32_t getBBPredecessorPos(const llvm::BasicBlock *BB, const llvm::BasicBlock *Pred);
/// Get num of BB's predecessors
u32_t getBBPredecessorNum(const llvm::BasicBlock *BB);

/// Return source code including line number and file name from debug information
//@{
std::string  getSourceLoc(const llvm::Value *val);
std::string  getSourceLocOfFunction(const llvm::Function *F);
//@}

/// Dump sparse bitvector set
void dumpSet(llvm::SparseBitVector<> To, llvm::raw_ostream & O = llvm::outs());

/// Dump points-to set
void dumpPointsToSet(unsigned node, llvm::SparseBitVector<> To) ;

/// Dump alias set
void dumpAliasSet(unsigned node, llvm::SparseBitVector<> To) ;

/// Print successful message by converting a string into green string output
std::string sucMsg(std::string msg);

/// Print warning message by converting a string into yellow string output
void wrnMsg(std::string msg);

/// Print error message by converting a string into red string output
//@{
std::string  errMsg(std::string msg);
std::string  bugMsg1(std::string msg);
std::string  bugMsg2(std::string msg);
std::string  bugMsg3(std::string msg);
//@}

/// Print each pass/phase message by converting a string into blue string output
std::string  pasMsg(std::string msg);

/// Print memory usage in KB.
void reportMemoryUsageKB(const std::string& infor, llvm::raw_ostream & O = llvm::outs());

/// Get memory usage from system file. Return TRUE if succeed.
bool getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb);

/// Increase the stack size limit
void increaseStackSize();

/*!
 * Compare two PointsTo according to their size and points-to elements.
 * 1. PointsTo with smaller size is smaller than the other;
 * 2. If the sizes are equal, comparing the points-to targets.
 */
inline bool cmpPts (const PointsTo& lpts,const PointsTo& rpts) {
    if (lpts.count() != rpts.count())
        return (lpts.count() < rpts.count());
    else {
        PointsTo::iterator bit = lpts.begin(), eit = lpts.end();
        PointsTo::iterator rbit = rpts.begin(), reit = rpts.end();
        for (; bit != eit && rbit != reit; bit++, rbit++) {
            if (*bit != *rbit)
                return (*bit < *rbit);
        }

        return false;
    }
}

}

#endif /* AnalysisUtil_H_ */
