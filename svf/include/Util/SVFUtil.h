//===- SVFUtil.h -- Analysis helper functions----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * SVFUtil.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui, dye
 */

#ifndef AnalysisUtil_H_
#define AnalysisUtil_H_

#include "FastCluster/fastcluster.h"
#include "SVFIR/SVFValue.h"
#include "Util/SVFLoopAndDomInfo.h"
#include "Util/ExtAPI.h"
#include "MemoryModel/PointsTo.h"
#include <time.h>
#include "Util/NodeIDAllocator.h"
#include "Util/ThreadAPI.h"

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
std::string sucMsg(const std::string& msg);

/// Returns warning message by converting a string into yellow string output
std::string wrnMsg(const std::string& msg);

/// Writes a message run through wrnMsg.
void writeWrnMsg(const std::string& msg);

/// Print error message by converting a string into red string output
//@{
std::string  errMsg(const std::string& msg);
std::string  bugMsg1(const std::string& msg);
std::string  bugMsg2(const std::string& msg);
std::string  bugMsg3(const std::string& msg);
//@}

/// Print each pass/phase message by converting a string into blue string output
std::string  pasMsg(const std::string& msg);

/// Print memory usage in KB.
void reportMemoryUsageKB(const std::string& infor,
                         OutStream& O = SVFUtil::outs());

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

/// Return true if it is an llvm intrinsic instruction
bool isIntrinsicInst(const ICFGNode* inst);
//@}


bool isCallSite(const ICFGNode* inst);

bool isRetInstNode(const ICFGNode* node);


/// Whether an instruction is a callsite in the application code, excluding llvm intrinsic calls
inline bool isNonInstricCallSite(const ICFGNode* inst)
{
    if(isIntrinsicInst(inst))
        return false;
    return isCallSite(inst);
}

/// Match arguments for callsite at caller and callee
/// if the arg size does not match then we do not need to connect this parameter
/// unless the callee is a variadic function (the first parameter of variadic function is its parameter number)
bool matchArgs(const CallICFGNode* cs, const FunObjVar* callee);


/// Split into two substrings around the first occurrence of a separator string.
inline std::vector<std::string> split(const std::string& s, char separator)
{
    std::vector<std::string> output;
    std::string::size_type prev_pos = 0, pos = 0;
    while ((pos = s.find(separator, pos)) != std::string::npos)
    {
        std::string substring(s.substr(prev_pos, pos - prev_pos));
        if (!substring.empty())
        {
            output.push_back(substring);
        }
        prev_pos = ++pos;
    }
    std::string lastSubstring(s.substr(prev_pos, pos - prev_pos));
    if (!lastSubstring.empty())
    {
        output.push_back(lastSubstring);
    }
    return output;
}

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
/// If the library function is redefined in the application code (e.g., memcpy), it will return false and will not be treated as an external call.
//@{

bool isExtCall(const FunObjVar* fun);


/// Return true if the call is a heap allocator/reallocator
//@{
/// note that these two functions are not suppose to be used externally

inline bool isHeapAllocExtFunViaRet(const FunObjVar* fun)
{
    return fun && (ExtAPI::getExtAPI()->is_alloc(fun)
                   || ExtAPI::getExtAPI()->is_realloc(fun));
}

inline bool isHeapAllocExtFunViaArg(const FunObjVar* fun)
{
    return fun && ExtAPI::getExtAPI()->is_arg_alloc(fun);
}

/// Get the position of argument that holds an allocated heap object.
//@{

inline u32_t getHeapAllocHoldingArgPosition(const FunObjVar* fun)
{
    return ExtAPI::getExtAPI()->get_alloc_arg_pos(fun);
}
/// Return true if the call is a heap reallocator
//@{
/// note that this function is not suppose to be used externally

inline bool isReallocExtFun(const FunObjVar* fun)
{
    return fun && (ExtAPI::getExtAPI()->is_realloc(fun));
}

/// Program entry function e.g. main
//@{
/// Return true if this is a program entry function (e.g. main)

bool isProgEntryFunction(const FunObjVar*);

/// Get program entry function from function name.
const FunObjVar* getProgFunction(const std::string& funName);

/// Get program entry function.
const FunObjVar*getProgEntryFunction();


/// Return true if this is a program exit function call
//@{
bool isProgExitFunction(const FunObjVar *fun);



bool isArgOfUncalledFunction(const SVFVar* svfvar);

const ObjVar* getObjVarOfValVar(const ValVar* valVar);

/// Return thread fork function
//@{
inline const ValVar* getForkedFun(const CallICFGNode *inst)
{
    return ThreadAPI::getThreadAPI()->getForkedFun(inst);
}
//@}


bool isExtCall(const CallICFGNode* cs);

bool isExtCall(const ICFGNode* node);

bool isHeapAllocExtCallViaArg(const CallICFGNode* cs);


/// interfaces to be used externally
bool isHeapAllocExtCallViaRet(const CallICFGNode* cs);

bool isHeapAllocExtCall(const ICFGNode* cs);

//@}

u32_t getHeapAllocHoldingArgPosition(const CallICFGNode* cs);
//@}

bool isReallocExtCall(const CallICFGNode* cs);
//@}

/// Return true if this is a thread creation call
///@{
inline bool isThreadForkCall(const CallICFGNode *inst)
{
    return ThreadAPI::getThreadAPI()->isTDFork(inst);
}
//@}

/// Return true if this is a thread join call
///@{
inline bool isThreadJoinCall(const CallICFGNode* cs)
{
    return ThreadAPI::getThreadAPI()->isTDJoin(cs);
}
//@}

/// Return true if this is a thread exit call
///@{
inline bool isThreadExitCall(const CallICFGNode* cs)
{
    return ThreadAPI::getThreadAPI()->isTDExit(cs);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockAquireCall(const CallICFGNode* cs)
{
    return ThreadAPI::getThreadAPI()->isTDAcquire(cs);
}
//@}

/// Return true if this is a lock acquire call
///@{
inline bool isLockReleaseCall(const CallICFGNode* cs)
{
    return ThreadAPI::getThreadAPI()->isTDRelease(cs);
}
//@}

/// Return true if this is a barrier wait call
//@{
inline bool isBarrierWaitCall(const CallICFGNode* cs)
{
    return ThreadAPI::getThreadAPI()->isTDBarWait(cs);
}
//@}

/// Return sole argument of the thread routine
//@{
inline const ValVar* getActualParmAtForkSite(const CallICFGNode* cs)
{
    return ThreadAPI::getThreadAPI()->getActualParmAtForkSite(cs);
}
//@}


bool isProgExitCall(const CallICFGNode* cs);


template<typename T>
constexpr typename std::remove_reference<T>::type &&
move(T &&t) noexcept
{
    return std::move(t);
}

/// void_t is not available until C++17. We define it here for C++11/14.
template <typename... Ts> struct make_void
{
    typedef void type;
};
template <typename... Ts> using void_t = typename make_void<Ts...>::type;

/// @brief Type trait that checks if a type is iterable
/// (can be applied on a range-based for loop)
///@{
template <typename T, typename = void> struct is_iterable : std::false_type {};
template <typename T>
struct is_iterable<T, void_t<decltype(std::begin(std::declval<T&>()) !=
                                      std::end(std::declval<T&>()))>>
: std::true_type {};
template <typename T> constexpr bool is_iterable_v = is_iterable<T>::value;
///@}

/// @brief Type trait to check if a type is a map or unordered_map.
///@{
template <typename T> struct is_map : std::false_type {};
template <typename... Ts> struct is_map<std::map<Ts...>> : std::true_type {};
template <typename... Ts>
struct is_map<std::unordered_map<Ts...>> : std::true_type {};
template <typename... Ts> constexpr bool is_map_v = is_map<Ts...>::value;
///@}

/// @brief Type trait to check if a type is a set or unordered_set.
///@{
template <typename T> struct is_set : std::false_type {};
template <typename... Ts> struct is_set<std::set<Ts...>> : std::true_type {};
template <typename... Ts>
struct is_set<std::unordered_set<Ts...>> : std::true_type {};
template <typename... Ts> constexpr bool is_set_v = is_set<Ts...>::value;
///@}

/// @brief Type trait to check if a type is vector or list.
template <typename T> struct is_sequence_container : std::false_type {};
template <typename... Ts>
struct is_sequence_container<std::vector<Ts...>> : std::true_type {};
template <typename... Ts>
struct is_sequence_container<std::deque<Ts...>> : std::true_type {};
template <typename... Ts>
struct is_sequence_container<std::list<Ts...>> : std::true_type {};
template <typename... Ts>
constexpr bool is_sequence_container_v = is_sequence_container<Ts...>::value;
///@}


} // End namespace SVFUtil

} // End namespace SVF

#endif /* AnalysisUtil_H_ */
