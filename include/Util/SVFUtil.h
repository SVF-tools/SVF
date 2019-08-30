//===- SVFUtil.h -- Analysis helper functions----------------------------//
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
 * SVFUtil.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui, dye
 */

#ifndef AnalysisUtil_H_
#define AnalysisUtil_H_

#include "Util/SVFModule.h"
#include "Util/ExtAPI.h"
#include "Util/ThreadAPI.h"
#include "Util/BasicTypes.h"
#include <time.h>

/*
 * Util class to assist pointer analysis
 */
namespace SVFUtil {

/// Overwrite llvm::outs()
inline raw_ostream &outs(){
	return llvm::outs();
}

/// Overwrite llvm::errs()
inline raw_ostream &errs(){
	return llvm::errs();
}

/// Return true if this function is llvm dbg intrinsic function/instruction
//@{
inline bool isIntrinsicDbgFun(const Function* fun) {
    return fun->getName().startswith("llvm.dbg.declare") ||
           fun->getName().startswith("llvm.dbg.value");
}
bool isInstrinsicDbgInst(const Instruction* inst);
//@}

/// This function servers a allocation wrapper detector
inline bool isAnAllocationWraper(const Instruction *inst) {
    return false;
}
/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Instruction* inst) {
    return SVFUtil::isa<CallInst>(inst) || SVFUtil::isa<InvokeInst>(inst);
}
/// Whether an instruction is a callsite in the application code, excluding llvm intrinsic calls
inline bool isNonInstricCallSite(const Instruction* inst) {
	if(isInstrinsicDbgInst(inst))
		return false;
    return isCallSite(inst);
}
/// Whether an instruction is a return instruction
inline bool isReturn(const Instruction* inst) {
    return SVFUtil::isa<ReturnInst>(inst);
}
/// Return LLVM function if this value is
inline const Function* getLLVMFunction(const Value* val) {
    const Function *fun = SVFUtil::dyn_cast<Function>(val->stripPointerCasts());
    return fun;
}
/// Return LLVM callsite given a instruction
inline CallSite getLLVMCallSite(const Instruction* inst) {
    assert(SVFUtil::isa<CallInst>(inst)|| SVFUtil::isa<InvokeInst>(inst));
    CallSite cs(const_cast<Instruction*>(inst));
    return cs;
}

/// Get the definition of a function across multiple modules
inline const Function* getDefFunForMultipleModule(const Function* fun) {
	if(fun == NULL) return NULL;

    SVFModule svfModule;
    if (fun->isDeclaration() && svfModule.hasDefinition(fun))
        fun = svfModule.getDefinition(fun);
    return fun;
}

/// Return callee of a callsite. Return null if this is an indirect call
//@{
inline const Function* getCallee(const CallSite cs) {
    // FIXME: do we need to strip-off the casts here to discover more library functions
    Function *callee = SVFUtil::dyn_cast<Function>(cs.getCalledValue()->stripPointerCasts());
    return getDefFunForMultipleModule(callee);
}

inline const Function* getCallee(const Instruction *inst) {
    if (!SVFUtil::isa<CallInst>(inst) && !SVFUtil::isa<InvokeInst>(inst))
        return NULL;
    CallSite cs(const_cast<Instruction*>(inst));
    return getCallee(cs);
}
//@}

/// Return true if the call is an external call (external library in function summary table)
//@{
inline bool isExtCall(const Function* fun) {
    return fun && ExtAPI::getExtAPI()->is_ext(fun);
}

inline bool isExtCall(const CallSite cs) {
    return isExtCall(getCallee(cs));
}

inline bool isExtCall(const Instruction *inst) {
    return isExtCall(getCallee(inst));
}
//@}

/// Return true if the call is a heap allocator/reallocator
//@{
/// note that these two functions are not suppose to be used externally
inline bool isHeapAllocExtFunViaRet(const Function *fun) {
    return fun && (ExtAPI::getExtAPI()->is_alloc(fun)
                   || ExtAPI::getExtAPI()->is_realloc(fun));
}
inline bool isHeapAllocExtFunViaArg(const Function *fun) {
    return fun && ExtAPI::getExtAPI()->is_arg_alloc(fun);
}

/// interfaces to be used externally
inline bool isHeapAllocExtCallViaRet(const CallSite cs) {
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(getCallee(cs));
}

inline bool isHeapAllocExtCallViaRet(const Instruction *inst) {
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(getCallee(inst));
}

inline bool isHeapAllocExtCallViaArg(const CallSite cs) {
    return isHeapAllocExtFunViaArg(getCallee(cs));
}

inline bool isHeapAllocExtCallViaArg(const Instruction *inst) {
    return isHeapAllocExtFunViaArg(getCallee(inst));
}

inline bool isHeapAllocExtCall(const CallSite cs) {
    return isHeapAllocExtCallViaRet(cs) || isHeapAllocExtCallViaArg(cs);
}

inline bool isHeapAllocExtCall(const Instruction *inst) {
    return isHeapAllocExtCallViaRet(inst) || isHeapAllocExtCallViaArg(inst);
}
//@}

/// Get the position of argument that holds an allocated heap object.
//@{
inline int getHeapAllocHoldingArgPosition(const Function *fun) {
    return ExtAPI::getExtAPI()->get_alloc_arg_pos(fun);
}

inline int getHeapAllocHoldingArgPosition(const CallSite cs) {
    return getHeapAllocHoldingArgPosition(getCallee(cs));
}

inline int getHeapAllocHoldingArgPosition(const Instruction *inst) {
    return getHeapAllocHoldingArgPosition(getCallee(inst));
}
//@}

/// Return true if the call is a heap reallocator
//@{
/// note that this function is not suppose to be used externally
inline bool isReallocExtFun(const Function *fun) {
    return fun && (ExtAPI::getExtAPI()->is_realloc(fun));
}

inline bool isReallocExtCall(const CallSite cs) {
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(getCallee(cs));
}

inline bool isReallocExtCall(const Instruction *inst) {
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(getCallee(inst));
}
//@}

/// Return true if the call is a heap dealloc or not
//@{
/// note that this function is not suppose to be used externally
inline bool isDeallocExtFun(const Function *fun) {
    return fun && (ExtAPI::getExtAPI()->is_dealloc(fun));
}

inline bool isDeallocExtCall(const CallSite cs) {
    return isDeallocExtFun(getCallee(cs));
}

inline bool isDeallocExtCall(const Instruction *inst) {
    return isDeallocExtFun(getCallee(inst));
}
//@}


/// Return true if the call is a static global call
//@{
/// note that this function is not suppose to be used externally
inline bool isStaticExtFun(const Function *fun) {
    return fun && ExtAPI::getExtAPI()->has_static(fun);
}

inline bool isStaticExtCall(const CallSite cs) {
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isStaticExtFun(getCallee(cs));
}

inline bool isStaticExtCall(const Instruction *inst) {
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isStaticExtFun(getCallee(inst));
}
//@}

/// Return true if the call is a static global call
//@{
inline bool isHeapAllocOrStaticExtCall(const CallSite cs) {
    return isStaticExtCall(cs) || isHeapAllocExtCall(cs);
}

inline bool isHeapAllocOrStaticExtCall(const Instruction *inst) {
    return isStaticExtCall(inst) || isHeapAllocExtCall(inst);
}
//@}

/// Return external call type
inline ExtAPI::extf_t extCallTy(const Function* fun) {
    return ExtAPI::getExtAPI()->get_type(fun);
}

/// Get the reference type of heap/static object from an allocation site.
//@{
inline const PointerType *getRefTypeOfHeapAllocOrStatic(const CallSite cs) {
    const PointerType *refType = NULL;
    // Case 1: heap object held by *argument, we should get its element type.
    if (isHeapAllocExtCallViaArg(cs)) {
        int argPos = getHeapAllocHoldingArgPosition(cs);
        const Value *arg = cs.getArgument(argPos);
        if (const PointerType *argType = SVFUtil::dyn_cast<PointerType>(arg->getType()))
            refType = SVFUtil::dyn_cast<PointerType>(argType->getElementType());
    }
    // Case 2: heap/static object held by return value.
    else {
        assert((isStaticExtCall(cs) || isHeapAllocExtCallViaRet(cs))
               && "Must be heap alloc via ret, or static allocation site");
        refType = SVFUtil::dyn_cast<PointerType>(cs.getType());
    }
    assert(refType && "Allocated object must be held by a pointer-typed value.");
    return refType;
}

inline const PointerType *getRefTypeOfHeapAllocOrStatic(const Instruction *inst) {
    CallSite cs(const_cast<Instruction*>(inst));
    return getRefTypeOfHeapAllocOrStatic(cs);
}
//@}

/// Return true if this is a thread creation call
///@{
inline bool isThreadForkCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDFork(cs);
}
inline bool isThreadForkCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDFork(inst);
}
//@}

/// Return true if this is a hare_parallel_for call
///@{
inline bool isHareParForCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isHareParFor(cs);
}
inline bool isHareParForCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isHareParFor(inst);
}
//@}

/// Return true if this is a thread join call
///@{
inline bool isThreadJoinCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDJoin(cs);
}
inline bool isThreadJoinCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDJoin(inst);
}
//@}

/// Return true if this is a thread exit call
///@{
inline bool isThreadExitCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDExit(cs);
}
inline bool isThreadExitCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDExit(inst);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockAquireCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDAcquire(cs);
}
inline bool isLockAquireCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDAcquire(inst);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockReleaseCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDRelease(cs);
}
inline bool isLockReleaseCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDRelease(inst);
}
//@}

/// Return true if this is a barrier wait call
//@{
inline bool isBarrierWaitCall(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->isTDBarWait(cs);
}
inline bool isBarrierWaitCall(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->isTDBarWait(inst);
}
//@}

/// Return thread fork function
//@{
inline const Value* getForkedFun(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->getForkedFun(cs);
}
inline const Value* getForkedFun(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getForkedFun(inst);
}
//@}

/// Return sole argument of the thread routine
//@{
inline const Value* getActualParmAtForkSite(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(cs);
}
inline const Value* getActualParmAtForkSite(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(inst);
}
//@}

/// Return the task function of the parallel_for routine
//@{
inline const Value* getTaskFuncAtHareParForSite(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->getTaskFuncAtHareParForSite(cs);
}
inline const Value* getTaskFuncAtHareParForSite(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getTaskFuncAtHareParForSite(inst);
}
//@}

/// Return the task data argument of the parallel_for rountine
//@{
inline const Value* getTaskDataAtHareParForSite(const CallSite cs) {
    return ThreadAPI::getThreadAPI()->getTaskDataAtHareParForSite(cs);
}
inline const Value* getTaskDataAtHareParForSite(const Instruction *inst) {
    return ThreadAPI::getThreadAPI()->getTaskDataAtHareParForSite(inst);
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
inline bool ArgInDeadFunction (const Value * val) {
    return SVFUtil::isa<Argument>(val)
           && isDeadFunction(SVFUtil::cast<Argument>(val)->getParent());
}
//@}

/// Program entry function e.g. main
//@{
/// Return true if this is a program entry function (e.g. main)
inline bool isProgEntryFunction (const Function * fun) {
    return fun && fun->getName().str() == "main";
}

/// Get program entry function from module.
inline const Function* getProgEntryFunction(SVFModule svfModule) {
    for (SVFModule::const_iterator it = svfModule.begin(), eit = svfModule.end(); it != eit; ++it) {
        const Function *fun = *it;
        if (isProgEntryFunction(fun))
            return (fun);
    }
    return NULL;
}

/// Return true if this is an argument of a program entry function (e.g. main)
inline bool ArgInProgEntryFunction (const Value * val) {
    return SVFUtil::isa<Argument>(val)
           && isProgEntryFunction(SVFUtil::cast<Argument>(val)->getParent());
}
/// Return true if this is value in a dead function (function without any caller)
bool isPtrInDeadFunction (const Value * value);
//@}

/// Return true if this is a program exit function call
//@{
inline bool isProgExitFunction (const Function * fun) {
    return fun && (fun->getName().str() == "exit" ||
                   fun->getName().str() == "__assert_rtn" ||
                   fun->getName().str() == "__assert_fail" );
}

inline bool isProgExitCall(const CallSite cs) {
    return isProgExitFunction(getCallee(cs));
}

inline bool isProgExitCall(const Instruction *inst) {
    return isProgExitFunction(getCallee(inst));
}
//@}

/// Function does not have any possible caller in the call graph
//@{
/// Return true if the function does not have a caller (either it is a main function or a dead function)
inline bool isNoCallerFunction (const Function * fun) {
    return isDeadFunction(fun) || isProgEntryFunction(fun);
}

/// Return true if the argument in a function does not have a caller
inline bool ArgInNoCallerFunction (const Value * val) {
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
inline const BasicBlock* getFunExitBB(const Function* fun) {
    return &fun->back();
}
/// Strip off the constant casts
const Value * stripConstantCasts(const Value *val);

/// Strip off the all casts
Value *stripAllCasts(Value *val) ;

/// Get the type of the heap allocation
const Type *getTypeOfHeapAlloc(const llvm::Instruction *inst) ;

/// Return corresponding constant expression, otherwise return NULL
//@{
inline const ConstantExpr *isGepConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::GetElementPtr)
            return constExpr;
    }
    return NULL;
}

inline const ConstantExpr *isInt2PtrConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::IntToPtr)
            return constExpr;
    }
    return NULL;
}

inline const ConstantExpr *isPtr2IntConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::PtrToInt)
            return constExpr;
    }
    return NULL;
}

inline const ConstantExpr *isCastConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::BitCast)
            return constExpr;
    }
    return NULL;
}

inline const ConstantExpr *isSelectConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::Select)
            return constExpr;
    }
    return NULL;
}

inline const ConstantExpr *isTruncConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::Trunc ||
                constExpr->getOpcode() == Instruction::FPTrunc ||
                constExpr->getOpcode() == Instruction::ZExt ||
                constExpr->getOpcode() == Instruction::SExt ||
                constExpr->getOpcode() == Instruction::FPExt)
            return constExpr;
    }
    return NULL;
}

inline const ConstantExpr *isCmpConstantExpr(const Value *val) {
    if(const ConstantExpr* constExpr = SVFUtil::dyn_cast<ConstantExpr>(val)) {
        if(constExpr->getOpcode() == Instruction::ICmp || constExpr->getOpcode() == Instruction::FCmp)
            return constExpr;
    }
    return NULL;
}
//@}

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

/// Return source code including line number and file name from debug information
//@{
std::string  getSourceLoc(const Value *val);
std::string  getSourceLocOfFunction(const Function *F);
//@}

/// Dump sparse bitvector set
void dumpSet(NodeBS To, raw_ostream & O = SVFUtil::outs());

/// Dump points-to set
void dumpPointsToSet(unsigned node, NodeBS To) ;

/// Dump alias set
void dumpAliasSet(unsigned node, NodeBS To) ;

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
void reportMemoryUsageKB(const std::string& infor, raw_ostream & O = SVFUtil::outs());

/// Get memory usage from system file. Return TRUE if succeed.
bool getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb);

/// Increase the stack size limit
void increaseStackSize();

/// Check whether a file is an LLVM IR file
bool isIRFile(const std::string &filename);

/// Parse argument for multi-module analysis
void processArguments(int argc, char **argv, int &arg_num, char **arg_value,
                      std::vector<std::string> &moduleNameVec);
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
