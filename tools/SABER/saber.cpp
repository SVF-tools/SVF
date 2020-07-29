//===- saber.cpp -- Source-sink bug checker------------------------------------//
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
//===-----------------------------------------------------------------------===//

/*
 // Saber: Software Bug Check.
 //
 // Author: Yulei Sui,
 */

#include "SVF-FE/LLVMUtil.h"
#include "SABER/LeakChecker.h"
#include "SABER/FileChecker.h"
#include "SABER/DoubleFreeChecker.h"

using namespace llvm;
using namespace SVF;

static llvm::cl::opt<std::string> InputFilename(cl::Positional,
        llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

static llvm::cl::opt<bool> LEAKCHECKER("leak", llvm::cl::init(false),
                                       llvm::cl::desc("Memory Leak Detection"));

static llvm::cl::opt<bool> FILECHECKER("fileck", llvm::cl::init(false),
                                       llvm::cl::desc("File Open/Close Detection"));

static llvm::cl::opt<bool> DFREECHECKER("dfree", llvm::cl::init(false),
                                        llvm::cl::desc("Double Free Detection"));

static llvm::cl::opt<bool> UAFCHECKER("uaf", llvm::cl::init(false),
                                      llvm::cl::desc("Use-After-Free Detection"));

int main(int argc, char ** argv)
{

    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    SVFUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Source-Sink Bug Detector\n");

    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);

    LeakChecker *saber;

    if(LEAKCHECKER)
        saber = new LeakChecker();
    else if(FILECHECKER)
        saber = new FileChecker();
    else if(DFREECHECKER)
        saber = new DoubleFreeChecker();
    else
        saber = new LeakChecker();  // if no checker is specified, we use leak checker as the default one.

    saber->runOnModule(svfModule);

    return 0;

}
