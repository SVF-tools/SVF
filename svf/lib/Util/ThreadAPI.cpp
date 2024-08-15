//===- ThreadAPI.cpp -- Thread API-------------------------------------------//
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
 * ThreadAPI.cpp
 *
 *  Created on: Jan 21, 2014
 *      Author: Yulei Sui, dye
 */

#ifndef THREADAPI_CPP_
#define THREADAPI_CPP_

#include "Util/ThreadAPI.h"
#include "Util/SVFUtil.h"

#include <iostream>		/// std output
#include <stdio.h>
#include <iomanip>		/// for setw

using namespace std;
using namespace SVF;

ThreadAPI* ThreadAPI::tdAPI = nullptr;

namespace
{

/// string and type pair
struct ei_pair
{
    const char *n;
    ThreadAPI::TD_TYPE t;
};

} // End anonymous namespace

//Each (name, type) pair will be inserted into the map.
//All entries of the same type must occur together (for error detection).
static const ei_pair ei_pairs[]=
{
    //The current llvm-gcc puts in the \01.
    {"pthread_create", ThreadAPI::TD_FORK},
    {"apr_thread_create", ThreadAPI::TD_FORK},
    {"pthread_join", ThreadAPI::TD_JOIN},
    {"\01_pthread_join", ThreadAPI::TD_JOIN},
    {"pthread_cancel", ThreadAPI::TD_JOIN},
    {"pthread_mutex_lock", ThreadAPI::TD_ACQUIRE},
    {"pthread_rwlock_rdlock", ThreadAPI::TD_ACQUIRE},
    {"sem_wait", ThreadAPI::TD_ACQUIRE},
    {"_spin_lock", ThreadAPI::TD_ACQUIRE},
    {"SRE_SplSpecLockEx", ThreadAPI::TD_ACQUIRE},
    {"pthread_mutex_trylock", ThreadAPI::TD_TRY_ACQUIRE},
    {"pthread_mutex_unlock", ThreadAPI::TD_RELEASE},
    {"pthread_rwlock_unlock", ThreadAPI::TD_RELEASE},
    {"sem_post", ThreadAPI::TD_RELEASE},
    {"_spin_unlock", ThreadAPI::TD_RELEASE},
    {"SRE_SplSpecUnlockEx", ThreadAPI::TD_RELEASE},
//    {"pthread_cancel", ThreadAPI::TD_CANCEL},
    {"pthread_exit", ThreadAPI::TD_EXIT},
    {"pthread_detach", ThreadAPI::TD_DETACH},
    {"pthread_cond_wait", ThreadAPI::TD_COND_WAIT},
    {"pthread_cond_signal", ThreadAPI::TD_COND_SIGNAL},
    {"pthread_cond_broadcast", ThreadAPI::TD_COND_BROADCAST},
    {"pthread_cond_init", ThreadAPI::TD_CONDVAR_INI},
    {"pthread_cond_destroy", ThreadAPI::TD_CONDVAR_DESTROY},
    {"pthread_mutex_init", ThreadAPI::TD_MUTEX_INI},
    {"pthread_mutex_destroy", ThreadAPI::TD_MUTEX_DESTROY},
    {"pthread_barrier_init", ThreadAPI::TD_BAR_INIT},
    {"pthread_barrier_wait", ThreadAPI::TD_BAR_WAIT},

    // Hare APIs
    {"hare_parallel_for", ThreadAPI::HARE_PAR_FOR},

    //This must be the last entry.
    {0, ThreadAPI::TD_DUMMY}
};

/*!
 * initialize the map
 */
void ThreadAPI::init()
{
    set<TD_TYPE> t_seen;
    TD_TYPE prev_t= TD_DUMMY;
    t_seen.insert(TD_DUMMY);
    for(const ei_pair *p= ei_pairs; p->n; ++p)
    {
        if(p->t != prev_t)
        {
            //This will detect if you move an entry to another block
            //  but forget to change the type.
            if(t_seen.count(p->t))
            {
                fputs(p->n, stderr);
                putc('\n', stderr);
                assert(!"ei_pairs not grouped by type");
            }
            t_seen.insert(p->t);
            prev_t= p->t;
        }
        if(tdAPIMap.count(p->n))
        {
            fputs(p->n, stderr);
            putc('\n', stderr);
            assert(!"duplicate name in ei_pairs");
        }
        tdAPIMap[p->n]= p->t;
    }
}

/*!
 *
 */
const SVFFunction* ThreadAPI::getCallee(const Instruction *inst) const
{
    return SVFUtil::getCallee(inst);
}

/*!
 *
 */
const SVFFunction* ThreadAPI::getCallee(const CallSite cs) const
{
    return SVFUtil::getCallee(cs);
}

/*!
 *
 */
const CallSite ThreadAPI::getLLVMCallSite(const Instruction *inst) const
{
    return SVFUtil::getLLVMCallSite(inst);
}

/*!
 *
 */
void ThreadAPI::statInit(StringMap& tdAPIStatMap)
{

    tdAPIStatMap["pthread_create"] = 0;

    tdAPIStatMap["pthread_join"] = 0;

    tdAPIStatMap["pthread_mutex_lock"] = 0;

    tdAPIStatMap["pthread_mutex_trylock"] = 0;

    tdAPIStatMap["pthread_mutex_unlock"] = 0;

    tdAPIStatMap["pthread_cancel"] = 0;

    tdAPIStatMap["pthread_exit"] = 0;

    tdAPIStatMap["pthread_detach"] = 0;

    tdAPIStatMap["pthread_cond_wait"] = 0;

    tdAPIStatMap["pthread_cond_signal"] = 0;

    tdAPIStatMap["pthread_cond_broadcast"] = 0;

    tdAPIStatMap["pthread_cond_init"] = 0;

    tdAPIStatMap["pthread_cond_destroy"] = 0;

    tdAPIStatMap["pthread_mutex_init"] = 0;

    tdAPIStatMap["pthread_mutex_destroy"] = 0;

    tdAPIStatMap["pthread_barrier_init"] = 0;

    tdAPIStatMap["pthread_barrier_wait"] = 0;

    tdAPIStatMap["hare_parallel_for"] = 0;
}

void ThreadAPI::performAPIStat(SVFModule* module)
{

    StringMap tdAPIStatMap;

    statInit(tdAPIStatMap);

    for (SVFModule::llvm_iterator it = module->llvmFunBegin(), eit = module->llvmFunEnd(); it != eit;
            ++it)
    {

        for (inst_iterator II = inst_begin(*it), E = inst_end(*it); II != E;
                ++II)
        {
            const Instruction *inst = &*II;
            if (!SVFUtil::isa<CallInst>(inst))
                continue;
            const SVFFunction* fun = getCallee(inst);
            TD_TYPE type = getType(fun);
            switch (type)
            {
            case TD_FORK:
            {
                tdAPIStatMap["pthread_create"]++;
                break;
            }
            case TD_JOIN:
            {
                tdAPIStatMap["pthread_join"]++;
                break;
            }
            case TD_ACQUIRE:
            {
                tdAPIStatMap["pthread_mutex_lock"]++;
                break;
            }
            case TD_TRY_ACQUIRE:
            {
                tdAPIStatMap["pthread_mutex_trylock"]++;
                break;
            }
            case TD_RELEASE:
            {
                tdAPIStatMap["pthread_mutex_unlock"]++;
                break;
            }
            case TD_CANCEL:
            {
                tdAPIStatMap["pthread_cancel"]++;
                break;
            }
            case TD_EXIT:
            {
                tdAPIStatMap["pthread_exit"]++;
                break;
            }
            case TD_DETACH:
            {
                tdAPIStatMap["pthread_detach"]++;
                break;
            }
            case TD_COND_WAIT:
            {
                tdAPIStatMap["pthread_cond_wait"]++;
                break;
            }
            case TD_COND_SIGNAL:
            {
                tdAPIStatMap["pthread_cond_signal"]++;
                break;
            }
            case TD_COND_BROADCAST:
            {
                tdAPIStatMap["pthread_cond_broadcast"]++;
                break;
            }
            case TD_CONDVAR_INI:
            {
                tdAPIStatMap["pthread_cond_init"]++;
                break;
            }
            case TD_CONDVAR_DESTROY:
            {
                tdAPIStatMap["pthread_cond_destroy"]++;
                break;
            }
            case TD_MUTEX_INI:
            {
                tdAPIStatMap["pthread_mutex_init"]++;
                break;
            }
            case TD_MUTEX_DESTROY:
            {
                tdAPIStatMap["pthread_mutex_destroy"]++;
                break;
            }
            case TD_BAR_INIT:
            {
                tdAPIStatMap["pthread_barrier_init"]++;
                break;
            }
            case TD_BAR_WAIT:
            {
                tdAPIStatMap["pthread_barrier_wait"]++;
                break;
            }
            case HARE_PAR_FOR:
            {
                tdAPIStatMap["hare_parallel_for"]++;
                break;
            }
            case TD_DUMMY:
            {
                break;
            }
            }
        }

    }

    StringRef n(module->getModuleIdentifier());
    StringRef name = n.split('/').second;
    name = name.split('.').first;
    std::cout << "################ (program : " << name.str()
              << ")###############\n";
    std::cout.flags(std::ios::left);
    unsigned field_width = 20;
    for (llvm::StringMap<u32_t>::iterator it = tdAPIStatMap.begin(), eit =
                tdAPIStatMap.end(); it != eit; ++it)
    {
        std::string apiName = it->first().str();
        // format out put with width 20 space
        std::cout << std::setw(field_width) << apiName << " : " << it->second
                  << "\n";
    }
    std::cout << "#######################################################"
              << std::endl;

}

#endif /* THREADAPI_CPP_ */
