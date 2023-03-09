//===- ThreadAPI.h -- API for threads-----------------------------------------//
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
 * ThreadAPI.h
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui, dye
 */

#ifndef THREADAPI_H_
#define THREADAPI_H_

#include "SVFIR/SVFValue.h"

namespace SVF
{

class SVFModule;

/*
 * ThreadAPI class contains interfaces for pthread programs
 */
class ThreadAPI
{

public:
    enum TD_TYPE
    {
        TD_DUMMY = 0,   /// dummy type
        TD_FORK,        /// create a new thread
        TD_JOIN,        /// wait for a thread to join
        TD_DETACH,      /// detach a thread directly instead wait for it to join
        TD_ACQUIRE,     /// acquire a lock
        TD_TRY_ACQUIRE, /// try to acquire a lock
        TD_RELEASE,     /// release a lock
        TD_EXIT,        /// exit/kill a thread
        TD_CANCEL,      /// cancel a thread by another
        TD_COND_WAIT,   /// wait a condition
        TD_COND_SIGNAL, /// signal a condition
        TD_COND_BROADCAST,  /// broadcast a condition
        TD_MUTEX_INI,       /// initial a mutex variable
        TD_MUTEX_DESTROY,   /// initial a mutex variable
        TD_CONDVAR_INI,     /// initial a mutex variable
        TD_CONDVAR_DESTROY, /// initial a mutex variable
        TD_BAR_INIT,        /// Barrier init
        TD_BAR_WAIT,        /// Barrier wait
        HARE_PAR_FOR
    };

    typedef Map<std::string, TD_TYPE> TDAPIMap;

private:
    /// API map, from a string to threadAPI type
    TDAPIMap tdAPIMap;

    /// Constructor
    ThreadAPI ()
    {
        init();
    }

    /// Initialize the map
    void init();

    /// Static reference
    static ThreadAPI* tdAPI;

    /// Get the function type if it is a threadAPI function
    inline TD_TYPE getType(const SVFFunction* F) const
    {
        if(F)
        {
            TDAPIMap::const_iterator it= tdAPIMap.find(F->getName());
            if(it != tdAPIMap.end())
                return it->second;
        }
        return TD_DUMMY;
    }

public:
    /// Return a static reference
    static ThreadAPI* getThreadAPI()
    {
        if(tdAPI == nullptr)
        {
            tdAPI = new ThreadAPI();
        }
        return tdAPI;
    }

    static void destroy()
    {
        if(tdAPI != nullptr)
        {
            delete tdAPI;
            tdAPI = nullptr;
        }
    }

    /// Return the callee/callsite/func
    //@{
    const SVFFunction* getCallee(const SVFInstruction *inst) const;
    const SVFFunction* getCallee(const CallSite cs) const;
    const CallSite getSVFCallSite(const SVFInstruction *inst) const;
    //@}

    /// Return true if this call create a new thread
    //@{
    inline bool isTDFork(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == TD_FORK;
    }
    inline bool isTDFork(CallSite cs) const
    {
        return isTDFork(cs.getInstruction());
    }
    //@}

    /// Return true if this call proceeds a hare_parallel_for
    //@{
    inline bool isHareParFor(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == HARE_PAR_FOR;
    }
    inline bool isHareParFor(CallSite cs) const
    {
        return isTDFork(cs.getInstruction());
    }
    //@}

    /// Return arguments/attributes of pthread_create / hare_parallel_for
    //@{
    /// Return the first argument of the call,
    /// Note that, it is the pthread_t pointer
    inline const SVFValue* getForkedThread(const SVFInstruction *inst) const
    {
        assert(isTDFork(inst) && "not a thread fork function!");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(0);
    }
    inline const SVFValue* getForkedThread(CallSite cs) const
    {
        return getForkedThread(cs.getInstruction());
    }

    /// Return the third argument of the call,
    /// Note that, it could be function type or a void* pointer
    inline const SVFValue* getForkedFun(const SVFInstruction *inst) const
    {
        assert(isTDFork(inst) && "not a thread fork function!");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(2);
    }
    inline const SVFValue* getForkedFun(CallSite cs) const
    {
        return getForkedFun(cs.getInstruction());
    }

    /// Return the forth argument of the call,
    /// Note that, it is the sole argument of start routine ( a void* pointer )
    inline const SVFValue* getActualParmAtForkSite(const SVFInstruction *inst) const
    {
        assert(isTDFork(inst) && "not a thread fork function!");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(3);
    }
    inline const SVFValue* getActualParmAtForkSite(CallSite cs) const
    {
        return getActualParmAtForkSite(cs.getInstruction());
    }
    //@}

    /// Get the task function (i.e., the 5th parameter) of the hare_parallel_for call
    //@{
    inline const SVFValue* getTaskFuncAtHareParForSite(const SVFInstruction *inst) const
    {
        assert(isHareParFor(inst) && "not a hare_parallel_for function!");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(4);
    }
    inline const SVFValue* getTaskFuncAtHareParForSite(CallSite cs) const
    {
        return getTaskFuncAtHareParForSite(cs.getInstruction());
    }
    //@}

    /// Get the task data (i.e., the 6th parameter) of the hare_parallel_for call
    //@{
    inline const SVFValue* getTaskDataAtHareParForSite(const SVFInstruction *inst) const
    {
        assert(isHareParFor(inst) && "not a hare_parallel_for function!");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(5);
    }
    inline const SVFValue* getTaskDataAtHareParForSite(CallSite cs) const
    {
        return getTaskDataAtHareParForSite(cs.getInstruction());
    }
    //@}

    /// Return true if this call wait for a worker thread
    //@{
    inline bool isTDJoin(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == TD_JOIN;
    }
    inline bool isTDJoin(CallSite cs) const
    {
        return isTDJoin(cs.getInstruction());
    }
    //@}

    /// Return arguments/attributes of pthread_join
    //@{
    /// Return the first argument of the call,
    /// Note that, it is the pthread_t pointer
    const SVFValue* getJoinedThread(const SVFInstruction *inst) const;
    inline const SVFValue* getJoinedThread(CallSite cs) const
    {
        return getJoinedThread(cs.getInstruction());
    }
    /// Return the send argument of the call,
    /// Note that, it is the pthread_t pointer
    inline const SVFValue* getRetParmAtJoinedSite(const SVFInstruction *inst) const
    {
        assert(isTDJoin(inst) && "not a thread join function!");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(1);
    }
    inline const SVFValue* getRetParmAtJoinedSite(CallSite cs) const
    {
        return getRetParmAtJoinedSite(cs.getInstruction());
    }
    //@}


    /// Return true if this call exits/terminate a thread
    //@{
    inline bool isTDExit(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == TD_EXIT;
    }

    inline bool isTDExit(CallSite cs) const
    {
        return getType(getCallee(cs)) == TD_EXIT;
    }
    //@}

    /// Return true if this call acquire a lock
    //@{
    inline bool isTDAcquire(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == TD_ACQUIRE;
    }

    inline bool isTDAcquire(CallSite cs) const
    {
        return getType(getCallee(cs)) == TD_ACQUIRE;
    }
    //@}

    /// Return true if this call release a lock
    //@{
    inline bool isTDRelease(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == TD_RELEASE;
    }

    inline bool isTDRelease(CallSite cs) const
    {
        return getType(getCallee(cs)) == TD_RELEASE;
    }
    //@}

    /// Return lock value
    //@{
    /// First argument of pthread_mutex_lock/pthread_mutex_unlock
    inline const SVFValue* getLockVal(const SVFInstruction *inst) const
    {
        assert((isTDAcquire(inst) || isTDRelease(inst)) && "not a lock acquire or release function");
        CallSite cs = getSVFCallSite(inst);
        return cs.getArgument(0);
    }
    inline const SVFValue* getLockVal(CallSite cs) const
    {
        return getLockVal(cs.getInstruction());
    }
    //@}

    /// Return true if this call waits for a barrier
    //@{
    inline bool isTDBarWait(const SVFInstruction *inst) const
    {
        return getType(getCallee(inst)) == TD_BAR_WAIT;
    }

    inline bool isTDBarWait(CallSite cs) const
    {
        return getType(getCallee(cs)) == TD_BAR_WAIT;
    }
    //@}

    void performAPIStat(SVFModule* m);
    void statInit(Map<std::string, u32_t>& tdAPIStatMap);
};

} // End namespace SVF

#endif /* THREADAPI_H_ */
