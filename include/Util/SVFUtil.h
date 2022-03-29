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
                 func->getIntrinsicID() == llvm::Intrinsic::dbg_value)) {
            return true;
    }
    return false;
}

/// Return true if it is an intrinsic instruction
inline bool isIntrinsicInst(const Instruction* inst)
{
    if (const llvm::CallBase* call = llvm::dyn_cast<llvm::CallBase>(inst)) {
        const Function* func = call->getCalledFunction();
        if (isIntrinsicFun(func)) {
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
/// Whether an instruction is a return instruction
inline bool isReturn(const Instruction* inst)
{
    return SVFUtil::isa<ReturnInst>(inst);
}

/// Return LLVM callsite given a instruction
inline CallSite getLLVMCallSite(const Instruction* inst)
{
    assert(isCallSite(inst) && "not a callsite?");
    CallSite cs(const_cast<Instruction*>(inst));
    return cs;
}

/// Get the corresponding Function based on its name
inline const SVFFunction* getFunction(StringRef name)
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

} // End namespace SVFUtil

} // End namespace SVF

#endif /* AnalysisUtil_H_ */
