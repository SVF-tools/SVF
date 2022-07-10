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

#include "FastCluster/fastcluster.h"
#include "SVF-FE/LLVMModule.h"
#include "Util/BasicTypes.h"
#include "MemoryModel/PointsTo.h"
#include <time.h>

namespace SVF
{

/*
 * Util class to assist pointer analysis
 */
namespace SVFUtil
{

/// Overwrite llvm::outs()
inline std::ostream &outs()
{
    return std::cout;
}

/// Overwrite llvm::errs()
inline std::ostream  &errs()
{
    return std::cerr;
}

/// Dump sparse bitvector set
void dumpSet(NodeBS To, OutStream & O = SVFUtil::outs());
void dumpSet(PointsTo To, OutStream & O = SVFUtil::outs());

/// Dump points-to set
void dumpPointsToSet(unsigned node, NodeBS To) ;
void dumpSparseSet(const NodeBS& To);

/// Dump alias set
void dumpAliasSet(unsigned node, NodeBS To) ;

/// Returns successful message by converting a string into green string output
std::string sucMsg(std::string msg);

/// Returns warning message by converting a string into yellow string output
std::string wrnMsg(std::string msg);

/// Writes a message run through wrnMsg.
void writeWrnMsg(std::string msg);

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
void reportMemoryUsageKB(const std::string& infor, OutStream & O = SVFUtil::outs());

/// Get memory usage from system file. Return TRUE if succeed.
bool getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb);

/// Increase the stack size limit
void increaseStackSize();

/*!
 * Compare two PointsTo according to their size and points-to elements.
 * 1. PointsTo with smaller size is smaller than the other;
 * 2. If the sizes are equal, comparing the points-to targets.
 */
inline bool cmpPts (const PointsTo& lpts,const PointsTo& rpts)
{
    if (lpts.count() != rpts.count())
        return (lpts.count() < rpts.count());
    else
    {
        PointsTo::iterator bit = lpts.begin(), eit = lpts.end();
        PointsTo::iterator rbit = rpts.begin(), reit = rpts.end();
        for (; bit != eit && rbit != reit; bit++, rbit++)
        {
            if (*bit != *rbit)
                return (*bit < *rbit);
        }

        return false;
    }
}

inline bool cmpNodeBS(const NodeBS& lpts,const NodeBS& rpts)
{
    if (lpts.count() != rpts.count())
        return (lpts.count() < rpts.count());
    else
    {
        NodeBS::iterator bit = lpts.begin(), eit = lpts.end();
        NodeBS::iterator rbit = rpts.begin(), reit = rpts.end();
        for (; bit != eit && rbit != reit; bit++, rbit++)
        {
            if (*bit != *rbit)
                return (*bit < *rbit);
        }

        return false;
    }
}

typedef struct equalPointsTo
{
    bool operator()(const PointsTo& lhs, const PointsTo& rhs) const
    {
        return SVFUtil::cmpPts(lhs, rhs);
    }
} equalPointsTo;

typedef struct equalNodeBS
{
    bool operator()(const NodeBS& lhs, const NodeBS& rhs) const
    {
        return SVFUtil::cmpNodeBS(lhs, rhs);
    }
} equalNodeBS;

inline NodeBS ptsToNodeBS(const PointsTo &pts)
{
    NodeBS nbs;
    for (const NodeID o : pts) nbs.set(o);
    return nbs;
}

typedef OrderedSet<PointsTo, equalPointsTo> PointsToList;
void dumpPointsToList(const PointsToList& ptl);

inline bool isIntrinsicFun(const Function* func)
{
    if (func && (func->getIntrinsicID() == llvm::Intrinsic::donothing ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_addr ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_declare ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_label ||
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_value))
    {
        return true;
    }
    return false;
}

/// Return true if it is an intrinsic instruction
inline bool isIntrinsicInst(const Instruction* inst)
{
    if (const llvm::CallBase* call = llvm::dyn_cast<llvm::CallBase>(inst))
    {
        const Function* func = call->getCalledFunction();
        if (isIntrinsicFun(func))
        {
            return true;
        }
    }
    return false;
}
//@}

/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Instruction* inst)
{
    return SVFUtil::isa<CallBase>(inst);
}
/// Whether an instruction is a call or invoke instruction
inline bool isCallSite(const Value* val)
{
    if(const Instruction* inst = SVFUtil::dyn_cast<Instruction>(val))
        return SVFUtil::isCallSite(inst);
    else
        return false;
}
/// Whether an instruction is a callsite in the application code, excluding llvm intrinsic calls
inline bool isNonInstricCallSite(const Instruction* inst)
{
    if(isIntrinsicInst(inst))
        return false;
    return isCallSite(inst);
}

/// Return LLVM callsite given a instruction
inline CallSite getLLVMCallSite(const Instruction* inst)
{
    assert(isCallSite(inst) && "not a callsite?");
    CallSite cs(const_cast<Instruction*>(inst));
    return cs;
}

/// Get the corresponding Function based on its name
inline const SVFFunction* getFunction(std::string name)
{
    Function* fun = nullptr;
    LLVMModuleSet* llvmModuleset = LLVMModuleSet::getLLVMModuleSet();

    for (u32_t i = 0; i < llvmModuleset->getModuleNum(); ++i)
    {
        Module *mod = llvmModuleset->getModule(i);
        fun = mod->getFunction(name);
        if(fun)
        {
            return llvmModuleset->getSVFFunction(fun);
        }
    }
    return nullptr;
}

/// Split into two substrings around the first occurrence of a separator string.
inline std::vector<std::string> split(const std::string& s, char seperator)
{
    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;
    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );
        output.push_back(substring);
        prev_pos = ++pos;
    }
    output.push_back(s.substr(prev_pos, pos-prev_pos));
    return output;
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

/// Get the definition of a function across multiple modules
inline const SVFFunction* getDefFunForMultipleModule(const Function* fun)
{
    if(fun == nullptr) return nullptr;
    LLVMModuleSet* llvmModuleset = LLVMModuleSet::getLLVMModuleSet();
    const SVFFunction* svfFun = llvmModuleset->getSVFFunction(fun);
    if (fun->isDeclaration() && llvmModuleset->hasDefinition(fun))
        svfFun = LLVMModuleSet::getLLVMModuleSet()->getDefinition(fun);
    return svfFun;
}

/// Return callee of a callsite. Return null if this is an indirect call
//@{
inline const SVFFunction* getCallee(const CallSite cs)
{
    // FIXME: do we need to strip-off the casts here to discover more library functions
    Function *callee = SVFUtil::dyn_cast<Function>(cs.getCalledValue()->stripPointerCasts());
    return getDefFunForMultipleModule(callee);
}

inline const SVFFunction* getCallee(const Instruction *inst)
{
    if (!isCallSite(inst))
        return nullptr;
    CallSite cs(const_cast<Instruction*>(inst));
    return getCallee(cs);
}
//@}

/// Return source code including line number and file name from debug information
//@{
std::string  getSourceLoc(const Value *val);
std::string  getSourceLocOfFunction(const Function *F);
const std::string value2String(const Value* value);
//@}

/// Given a map mapping points-to sets to a count, adds from into to.
template <typename Data>
void mergePtsOccMaps(Map<Data, unsigned> &to, const Map<Data, unsigned> from)
{
    for (const typename Map<Data, unsigned>::value_type &ptocc : from)
    {
        to[ptocc.first] += ptocc.second;
    }
}

/// Returns a string representation of a hclust method.
std::string hclustMethodToString(hclust_fast_methods method);

/// Inserts an element into a Set/CondSet (with ::insert).
template <typename Key, typename KeySet>
inline void insertKey(const Key &key, KeySet &keySet)
{
    keySet.insert(key);
}

/// Inserts a NodeID into a NodeBS.
inline void insertKey(const NodeID &key, NodeBS &keySet)
{
    keySet.set(key);
}

/// Removes an element from a Set/CondSet (or anything implementing ::erase).
template <typename Key, typename KeySet>
inline void removeKey(const Key &key, KeySet &keySet)
{
    keySet.erase(key);
}

/// Removes a NodeID from a NodeBS.
inline void removeKey(const NodeID &key, NodeBS &keySet)
{
    keySet.reset(key);
}

/// Function to call when alarm for time limit hits.
void timeLimitReached(int signum);

/// Starts an analysis timer. If timeLimit is 0, sets no timer.
/// If an alarm has already been set, does not set another.
/// Returns whether we set a timer or not.
bool startAnalysisLimitTimer(unsigned timeLimit);

/// Stops an analysis timer. limitTimerSet indicates whether the caller set the
/// timer or not (return value of startLimitTimer).
void stopAnalysisLimitTimer(bool limitTimerSet);

/// Return true if the call is an external call (external library in function summary table)
/// If the libary function is redefined in the application code (e.g., memcpy), it will return false and will not be treated as an external call.
//@{
inline bool isExtCall(const SVFFunction* fun)
{
    return fun && ExtAPI::getExtAPI()->is_ext(fun);
}

/// Return true if the call is a heap allocator/reallocator
//@{
/// note that these two functions are not suppose to be used externally
inline bool isHeapAllocExtFunViaRet(const SVFFunction* fun)
{
    return fun && (ExtAPI::getExtAPI()->is_alloc(fun)
                   || ExtAPI::getExtAPI()->is_realloc(fun));
}

inline bool isHeapAllocExtFunViaArg(const SVFFunction* fun)
{
    return fun && ExtAPI::getExtAPI()->is_arg_alloc(fun);
}

/// Get the position of argument that holds an allocated heap object.
//@{
inline int getHeapAllocHoldingArgPosition(const SVFFunction* fun)
{
    return ExtAPI::getExtAPI()->get_alloc_arg_pos(fun);
}

/// Return true if the call is a heap reallocator
//@{
/// note that this function is not suppose to be used externally
inline bool isReallocExtFun(const SVFFunction* fun)
{
    return fun && (ExtAPI::getExtAPI()->is_realloc(fun));
}

/// Return true if the call is a heap dealloc or not
//@{
/// note that this function is not suppose to be used externally
inline bool isDeallocExtFun(const SVFFunction* fun)
{
    return fun && (ExtAPI::getExtAPI()->is_dealloc(fun));
}

/// Return true if the call is a static global call
//@{
/// note that this function is not suppose to be used externally
inline bool isStaticExtFun(const SVFFunction* fun)
{
    return fun && ExtAPI::getExtAPI()->has_static(fun);
}

/// Program entry function e.g. main
//@{
/// Return true if this is a program entry function (e.g. main)
inline bool isProgEntryFunction (const SVFFunction * fun)
{
    return fun && fun->getName() == "main";
}


/// Get program entry function from module.
inline const SVFFunction* getProgFunction(SVFModule* svfModule, const std::string& funName)
{
    for (SVFModule::const_iterator it = svfModule->begin(), eit = svfModule->end(); it != eit; ++it)
    {
        const SVFFunction *fun = *it;
        if (fun->getName()==funName)
            return fun;
    }
    return nullptr;
}

/// Get program entry function from module.
inline const SVFFunction* getProgEntryFunction(SVFModule* svfModule)
{
    for (SVFModule::const_iterator it = svfModule->begin(), eit = svfModule->end(); it != eit; ++it)
    {
        const SVFFunction *fun = *it;
        if (isProgEntryFunction(fun))
            return (fun);
    }
    return nullptr;
}

/// Return true if this is a program exit function call
//@{
inline bool isProgExitFunction (const SVFFunction * fun)
{
    return fun && (fun->getName() == "exit" ||
                   fun->getName() == "__assert_rtn" ||
                   fun->getName() == "__assert_fail" );
}

/// Return true if the value refers to constant data, e.g., i32 0
inline bool isConstantData(const Value* val)
{
    return SVFUtil::isa<ConstantData>(val)
           || SVFUtil::isa<ConstantAggregate>(val)
           || SVFUtil::isa<MetadataAsValue>(val)
           || SVFUtil::isa<BlockAddress>(val);
}

/// Return thread fork function
//@{
inline const Value* getForkedFun(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->getForkedFun(cs);
}
inline const Value* getForkedFun(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->getForkedFun(inst);
}
//@}

const std::string type2String(const Type* type);

/// This function servers a allocation wrapper detector
inline bool isAnAllocationWraper(const Instruction*)
{
    return false;
}

/// Return LLVM function if this value is
inline const Function* getLLVMFunction(const Value* val)
{
    const Function *fun = SVFUtil::dyn_cast<Function>(val->stripPointerCasts());
    return fun;
}

inline bool isExtCall(const CallSite cs)
{
    return isExtCall(getCallee(cs));
}

inline bool isExtCall(const Instruction *inst)
{
    return isExtCall(getCallee(inst));
}

inline bool isHeapAllocExtCallViaArg(const CallSite cs)
{
    return isHeapAllocExtFunViaArg(getCallee(cs));
}

inline bool isHeapAllocExtCallViaArg(const Instruction *inst)
{
    return isHeapAllocExtFunViaArg(getCallee(inst));
}

/// interfaces to be used externally
inline bool isHeapAllocExtCallViaRet(const CallSite cs)
{
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(getCallee(cs));
}

inline bool isHeapAllocExtCallViaRet(const Instruction *inst)
{
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(getCallee(inst));
}

inline bool isHeapAllocExtCall(const CallSite cs)
{
    return isHeapAllocExtCallViaRet(cs) || isHeapAllocExtCallViaArg(cs);
}

inline bool isHeapAllocExtCall(const Instruction *inst)
{
    return isHeapAllocExtCallViaRet(inst) || isHeapAllocExtCallViaArg(inst);
}
//@}

inline int getHeapAllocHoldingArgPosition(const CallSite cs)
{
    return getHeapAllocHoldingArgPosition(getCallee(cs));
}

inline int getHeapAllocHoldingArgPosition(const Instruction *inst)
{
    return getHeapAllocHoldingArgPosition(getCallee(inst));
}
//@}

inline bool isReallocExtCall(const CallSite cs)
{
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(getCallee(cs));
}

inline bool isReallocExtCall(const Instruction *inst)
{
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(getCallee(inst));
}
//@}

inline bool isDeallocExtCall(const CallSite cs)
{
    return isDeallocExtFun(getCallee(cs));
}

inline bool isDeallocExtCall(const Instruction *inst)
{
    return isDeallocExtFun(getCallee(inst));
}
//@}

inline bool isStaticExtCall(const CallSite cs)
{
    bool isPtrTy = cs.getInstruction()->getType()->isPointerTy();
    return isPtrTy && isStaticExtFun(getCallee(cs));
}

inline bool isStaticExtCall(const Instruction *inst)
{
    bool isPtrTy = inst->getType()->isPointerTy();
    return isPtrTy && isStaticExtFun(getCallee(inst));
}
//@}

/// Return true if the call is a static global call
//@{
inline bool isHeapAllocOrStaticExtCall(const CallSite cs)
{
    return isStaticExtCall(cs) || isHeapAllocExtCall(cs);
}

inline bool isHeapAllocOrStaticExtCall(const Instruction *inst)
{
    return isStaticExtCall(inst) || isHeapAllocExtCall(inst);
}
//@}

/// Return true if this is a thread creation call
///@{
inline bool isThreadForkCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isTDFork(cs);
}
inline bool isThreadForkCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isTDFork(inst);
}
//@}

/// Return true if this is a hare_parallel_for call
///@{
inline bool isHareParForCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isHareParFor(cs);
}
inline bool isHareParForCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isHareParFor(inst);
}
//@}

/// Return true if this is a thread join call
///@{
inline bool isThreadJoinCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isTDJoin(cs);
}
inline bool isThreadJoinCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isTDJoin(inst);
}
//@}

/// Return true if this is a thread exit call
///@{
inline bool isThreadExitCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isTDExit(cs);
}
inline bool isThreadExitCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isTDExit(inst);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockAquireCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isTDAcquire(cs);
}
inline bool isLockAquireCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isTDAcquire(inst);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockReleaseCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isTDRelease(cs);
}
inline bool isLockReleaseCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isTDRelease(inst);
}
//@}

/// Return true if this is a barrier wait call
//@{
inline bool isBarrierWaitCall(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->isTDBarWait(cs);
}
inline bool isBarrierWaitCall(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->isTDBarWait(inst);
}
//@}

/// Return sole argument of the thread routine
//@{
inline const Value* getActualParmAtForkSite(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(cs);
}
inline const Value* getActualParmAtForkSite(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(inst);
}
//@}

/// Return the task function of the parallel_for routine
//@{
inline const Value* getTaskFuncAtHareParForSite(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->getTaskFuncAtHareParForSite(cs);
}
inline const Value* getTaskFuncAtHareParForSite(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->getTaskFuncAtHareParForSite(inst);
}
//@}

/// Return the task data argument of the parallel_for rountine
//@{
inline const Value* getTaskDataAtHareParForSite(const CallSite cs)
{
    return ThreadAPI::getThreadAPI()->getTaskDataAtHareParForSite(cs);
}
inline const Value* getTaskDataAtHareParForSite(const Instruction *inst)
{
    return ThreadAPI::getThreadAPI()->getTaskDataAtHareParForSite(inst);
}
//@}

inline bool isProgEntryFunction (const Function * fun)
{
    return fun && fun->getName() == "main";
}


inline bool isProgExitCall(const CallSite cs)
{
    return isProgExitFunction(getCallee(cs));
}

inline bool isProgExitCall(const Instruction *inst)
{
    return isProgExitFunction(getCallee(inst));
}

template<typename T>
constexpr typename std::remove_reference<T>::type &&
move(T &&t) noexcept
{
    return std::move(t);
}

} // End namespace SVFUtil

} // End namespace SVF

#endif /* AnalysisUtil_H_ */
