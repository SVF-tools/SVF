//===- AnalysisUtil.cpp -- Helper functions for pointer analysis--------------//
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
 * AnalysisUtil.cpp
 *
 *  Created on: Apr 11, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/SVFUtil.h"
#include "Util/Conditions.h"
#include <sys/resource.h>		/// increase stack size



/// Color for output format
#define KNRM  "\x1B[1;0m"
#define KRED  "\x1B[1;31m"
#define KGRN  "\x1B[1;32m"
#define KYEL  "\x1B[1;33m"
#define KBLU  "\x1B[1;34m"
#define KPUR  "\x1B[1;35m"
#define KCYA  "\x1B[1;36m"
#define KWHT  "\x1B[1;37m"

static llvm::cl::opt<bool> DisableWarn("dwarn", llvm::cl::init(true),
                                 llvm::cl::desc("Disable warning"));


/*!
 * print successful message by converting a string into green string output
 */
std::string SVFUtil::sucMsg(std::string msg) {
    return KGRN + msg + KNRM;
}

/*!
 * print warning message by converting a string into yellow string output
 */
void SVFUtil::wrnMsg(std::string msg) {
    if(DisableWarn) return;
    outs() << KYEL + msg + KNRM << "\n";
}

/*!
 * print error message by converting a string into red string output
 */
std::string SVFUtil::errMsg(std::string msg) {
    return KRED + msg + KNRM;
}

std::string SVFUtil::bugMsg1(std::string msg) {
    return KYEL + msg + KNRM;
}

std::string SVFUtil::bugMsg2(std::string msg) {
    return KPUR + msg + KNRM;
}

std::string SVFUtil::bugMsg3(std::string msg) {
    return KCYA + msg + KNRM;
}

/*!
 * print each pass/phase message by converting a string into blue string output
 */
std::string SVFUtil::pasMsg(std::string msg) {
    return KBLU + msg + KNRM;
}

/*!
 * Dump points-to set
 */
void SVFUtil::dumpPointsToSet(unsigned node, NodeBS bs) {
    outs() << "node " << node << " points-to: {";
    dumpSet(bs);
    outs() << "}\n";
}


/*!
 * Dump alias set
 */
void SVFUtil::dumpAliasSet(unsigned node, NodeBS bs) {
    outs() << "node " << node << " alias set: {";
    dumpSet(bs);
    outs() << "}\n";
}

/*!
 * Dump bit vector set
 */
void SVFUtil::dumpSet(NodeBS bs, raw_ostream & O) {
    for (NodeBS::iterator ii = bs.begin(), ie = bs.end();
            ii != ie; ii++) {
        O << " " << *ii << " ";
    }
}

/*!
 * Print memory usage
 */
void SVFUtil::reportMemoryUsageKB(const std::string& infor, raw_ostream & O)
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
    /* Get the the current process' status file from the proc filesystem */
    char buffer[8192];
    FILE* procfile = fopen("/proc/self/status", "r");
    if(procfile) {
        u32_t result = fread(buffer, sizeof(char), 8192, procfile);
        if (result == 0) {
            fputs ("Reading error\n",stderr);
        }
    }
    else {
        fputs ("/proc/self/status file not exit\n",stderr);
        return false;
    }
    fclose(procfile);

    /* Look through proc status contents line by line */
    char delims[] = "\n";
    char* line = strtok(buffer, delims);

    bool found_vmrss = false;
    bool found_vmsize = false;

    while (line != NULL && (found_vmrss == false || found_vmsize == false)) {
        if (strstr(line, "VmRSS:") != NULL)	{
            sscanf(line, "%*s %u", vmrss_kb);
            found_vmrss = true;
        }

        if (strstr(line, "VmSize:") != NULL) {
            sscanf(line, "%*s %u", vmsize_kb);
            found_vmsize = true;
        }

        line = strtok(NULL, delims);
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
    if (result == 0) {
        if (rl.rlim_cur < kStackSize) {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0)
                outs() << "setrlimit returned result = " << result << "\n";
        }
    }
}
