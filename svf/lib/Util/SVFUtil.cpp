//===- AnalysisUtil.cpp -- Helper functions for pointer analysis--------------//
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
 * AnalysisUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include "MemoryModel/PointsTo.h"
#include "Graphs/CallGraph.h"
#include "SVFIR/SVFVariables.h"

#include <sys/resource.h>		/// increase stack size

using namespace SVF;

/// Color for output format
#define KNRM  "\x1B[1;0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KPUR  "\x1B[1;35m"
#define KCYA  "\x1B[1;36m"
#define KWHT  "\x1B[1;37m"



/*!
 * print successful message by converting a string into green string output
 */
std::string SVFUtil::sucMsg(const std::string& msg)
{
    return KGRN + msg + KNRM;
}

/*!
 * print warning message by converting a string into yellow string output
 */
std::string SVFUtil::wrnMsg(const std::string& msg)
{
    return KYEL + msg + KNRM;
}

void SVFUtil::writeWrnMsg(const std::string& msg)
{
    if (Options::DisableWarn())
        return;
    outs() << wrnMsg(msg) << "\n";
}

/*!
 * print error message by converting a string into red string output
 */
std::string SVFUtil::errMsg(const std::string& msg)
{
    return KRED + msg + KNRM;
}

std::string SVFUtil::bugMsg1(const std::string& msg)
{
    return KYEL + msg + KNRM;
}

std::string SVFUtil::bugMsg2(const std::string& msg)
{
    return KPUR + msg + KNRM;
}

std::string SVFUtil::bugMsg3(const std::string& msg)
{
    return KCYA + msg + KNRM;
}

/*!
 * print each pass/phase message by converting a string into blue string output
 */
std::string SVFUtil::pasMsg(const std::string& msg)
{
    return KBLU + msg + KNRM;
}

/*!
 * Dump points-to set
 */
void SVFUtil::dumpPointsToSet(unsigned node, NodeBS bs)
{
    outs() << "node " << node << " points-to: {";
    dumpSet(bs);
    outs() << "}\n";
}

// For use from the debugger.
void SVFUtil::dumpSparseSet(const NodeBS& bs)
{
    outs() << "{";
    dumpSet(bs);
    outs() << "}\n";
}

void SVFUtil::dumpPointsToList(const PointsToList& ptl)
{
    outs() << "{";
    for (PointsToList::const_iterator ii = ptl.begin(), ie = ptl.end();
            ii != ie; ii++)
    {
        auto bs = *ii;
        dumpSet(bs);
    }
    outs() << "}\n";
}

/*!
 * Dump alias set
 */
void SVFUtil::dumpAliasSet(unsigned node, NodeBS bs)
{
    outs() << "node " << node << " alias set: {";
    dumpSet(bs);
    outs() << "}\n";
}

/*!
 * Dump bit vector set
 */
void SVFUtil::dumpSet(NodeBS bs, OutStream & O)
{
    for (NodeBS::iterator ii = bs.begin(), ie = bs.end();
            ii != ie; ii++)
    {
        O << " " << *ii << " ";
    }
}

void SVFUtil::dumpSet(PointsTo pt, OutStream &o)
{
    for (NodeID n : pt)
    {
        o << " " << n << " ";
    }
}

/*!
 * Print memory usage
 */
void SVFUtil::reportMemoryUsageKB(const std::string& infor, OutStream & O)
{
    u32_t vmrss, vmsize;
    if (getMemoryUsageKB(&vmrss, &vmsize))
        O << infor << "\tVmRSS: " << vmrss << "\tVmSize: " << vmsize << "\n";
}

/*!
 * Get memory usage
 */
bool SVFUtil::getMemoryUsageKB(u32_t* vmrss_kb, u32_t* vmsize_kb)
{
    /* Get the current process' status file from the proc filesystem */
    char buffer[8192];
    FILE* procfile = fopen("/proc/self/status", "r");
    if(procfile)
    {
        u32_t result = fread(buffer, sizeof(char), 8192, procfile);
        if (result == 0)
        {
            fputs ("Reading error\n",stderr);
        }
    }
    else
    {
        SVFUtil::writeWrnMsg(" /proc/self/status file not exit!");
        return false;
    }
    fclose(procfile);

    /* Look through proc status contents line by line */
    char delims[] = "\n";
    char* line = strtok(buffer, delims);

    bool found_vmrss = false;
    bool found_vmsize = false;

    while (line != nullptr && (found_vmrss == false || found_vmsize == false))
    {
        if (strstr(line, "VmRSS:") != nullptr)
        {
            sscanf(line, "%*s %u", vmrss_kb);
            found_vmrss = true;
        }

        if (strstr(line, "VmSize:") != nullptr)
        {
            sscanf(line, "%*s %u", vmsize_kb);
            found_vmsize = true;
        }

        line = strtok(nullptr, delims);
    }

    return (found_vmrss && found_vmsize);
}

/*!
 * Increase stack size
 */
void SVFUtil::increaseStackSize()
{
    const rlim_t kStackSize = 256L * 1024L * 1024L;   // min stack size = 256 Mb
    struct rlimit rl;
    int result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0)
    {
        if (rl.rlim_cur < kStackSize)
        {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
                writeWrnMsg("setrlimit returned result !=0 \n");
        }
    }
}



std::string SVFUtil::hclustMethodToString(hclust_fast_methods method)
{
    switch (method)
    {
    case HCLUST_METHOD_SINGLE:
        return "single";
    case HCLUST_METHOD_COMPLETE:
        return "complete";
    case HCLUST_METHOD_AVERAGE:
        return "average";
    case HCLUST_METHOD_MEDIAN:
        return "median";
    case HCLUST_METHOD_SVF_BEST:
        return "svf-best";
    default:
        assert(false && "SVFUtil::hclustMethodToString: unknown method");
        abort();
    }
}

void SVFUtil::timeLimitReached(int)
{
    SVFUtil::outs().flush();
    // TODO: output does not indicate which time limit is reached.
    //       This can be better in the future.
    SVFUtil::outs() << "WPA: time limit reached\n";
    exit(101);
}

bool SVFUtil::startAnalysisLimitTimer(unsigned timeLimit)
{
    if (timeLimit == 0) return false;

    // If an alarm is already set, don't set another. That means this analysis
    // is part of another which has a time limit.
    unsigned remainingSeconds = alarm(0);
    if (remainingSeconds != 0)
    {
        // Continue the previous alarm and move on.
        alarm(remainingSeconds);
        return false;
    }

    signal(SIGALRM, &timeLimitReached);
    alarm(timeLimit);
    return true;
}

/// Stops an analysis timer. limitTimerSet indicates whether the caller set the
/// timer or not (return value of startLimitTimer).
void SVFUtil::stopAnalysisLimitTimer(bool limitTimerSet)
{
    if (limitTimerSet) alarm(0);
}

/// Match arguments for callsite at caller and callee
/// if the arg size does not match then we do not need to connect this parameter
/// unless the callee is a variadic function (the first parameter of variadic function is its parameter number)
/// e.g., void variadicFoo(int num, ...); variadicFoo(5, 1,2,3,4,5)
/// for variadic function, callsite arg size must be greater than or equal to callee arg size
bool SVFUtil::matchArgs(const CallICFGNode* call, const FunObjVar* callee)
{
    if (callee->isVarArg() || ThreadAPI::getThreadAPI()->isTDFork(call))
        return call->arg_size() >= callee->arg_size();
    else
        return call->arg_size() == callee->arg_size();
}

bool SVFUtil::isCallSite(const ICFGNode* inst)
{
    return SVFUtil::isa<CallICFGNode>(inst);
}

bool SVFUtil::isIntrinsicInst(const ICFGNode* inst)
{
    if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(inst))
    {
        const FunObjVar* func = call->getCalledFunction();
        if (func && func->isIntrinsic())
        {
            return true;
        }
    }
    return false;
}

bool SVFUtil::isExtCall(const CallICFGNode* cs)
{
    return isExtCall(cs->getCalledFunction());
}

bool SVFUtil::isHeapAllocExtCallViaArg(const CallICFGNode* cs)
{
    return isHeapAllocExtFunViaArg(cs->getCalledFunction());
}


u32_t SVFUtil::getHeapAllocHoldingArgPosition(const CallICFGNode* cs)
{
    return getHeapAllocHoldingArgPosition(cs->getCalledFunction());
}


bool SVFUtil::isExtCall(const ICFGNode* node)
{
    if(!isCallSite(node)) return false;
    return isExtCall(cast<CallICFGNode>(node)->getCalledFunction());
}

bool SVFUtil::isHeapAllocExtCall(const ICFGNode* cs)
{
    if(!isCallSite(cs)) return false;
    return isHeapAllocExtCallViaRet(cast<CallICFGNode>(cs)) || isHeapAllocExtCallViaArg(cast<CallICFGNode>(cs));
}

bool SVFUtil::isHeapAllocExtCallViaRet(const CallICFGNode* cs)
{
    bool isPtrTy = cs->getType()->isPointerTy();
    return isPtrTy && isHeapAllocExtFunViaRet(cs->getCalledFunction());
}

bool SVFUtil::isReallocExtCall(const CallICFGNode* cs)
{
    bool isPtrTy = cs->getType()->isPointerTy();
    return isPtrTy && isReallocExtFun(cs->getCalledFunction());
}


bool SVFUtil::isRetInstNode(const ICFGNode* node)
{
    if (const auto& intraNode = dyn_cast<IntraICFGNode>(node))
        return intraNode->isRetInst();
    else
        return false;
}

bool SVFUtil::isProgExitFunction(const FunObjVar *fun)
{
    return fun && (fun->getName() == "exit" ||
                   fun->getName() == "__assert_rtn" ||
                   fun->getName() == "__assert_fail");
}

bool SVFUtil::isProgExitCall(const CallICFGNode* cs)
{
    return isProgExitFunction(cs->getCalledFunction());
}

/// Get program entry function from module.
const FunObjVar* SVFUtil::getProgFunction(const std::string& funName)
{
    const CallGraph* svfirCallGraph = PAG::getPAG()->getCallGraph();
    for (const auto& item: *svfirCallGraph)
    {
        const CallGraphNode*fun = item.second;
        if (fun->getName()==funName)
            return fun->getFunction();
    }
    return nullptr;
}

/// Get program entry function from module.
const FunObjVar* SVFUtil::getProgEntryFunction()
{
    const CallGraph* svfirCallGraph = PAG::getPAG()->getCallGraph();
    for (const auto& item: *svfirCallGraph)
    {
        const CallGraphNode*fun = item.second;
        if (isProgEntryFunction(fun->getFunction()))
            return (fun->getFunction());
    }
    return nullptr;
}

bool SVFUtil::isArgOfUncalledFunction(const SVFVar* svfvar)
{
    const ValVar* pVar = PAG::getPAG()->getBaseValVar(svfvar->getId());
    if(const ArgValVar* arg = SVFUtil::dyn_cast<ArgValVar>(pVar))
        return arg->isArgOfUncalledFunction();
    else
        return false;
}

const ObjVar* SVFUtil::getObjVarOfValVar(const SVF::ValVar* valVar)
{
    assert(valVar->getInEdges().size() == 1);
    return SVFUtil::dyn_cast<ObjVar>((*valVar->getInEdges().begin())->getSrcNode());
}

bool SVFUtil::isExtCall(const FunObjVar* fun)
{
    return fun && ExtAPI::getExtAPI()->is_ext(fun);
}

bool SVFUtil::isProgEntryFunction(const FunObjVar* funObjVar)
{
    return funObjVar && funObjVar->getName() == "main";
}