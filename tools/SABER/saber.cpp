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

#include "SABER/LeakChecker.h"
#include "SABER/FileChecker.h"
#include "SABER/DoubleFreeChecker.h"

#include <llvm-c/Core.h> // for LLVMGetGlobalContext()
#include <llvm/Support/CommandLine.h>	// for cl
#include <llvm/Bitcode/BitcodeWriterPass.h>  // for bitcode write
#include <llvm/IR/LegacyPassManager.h>		// pass manager
#include <llvm/Support/Signals.h>	// singal for command line
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Support/ToolOutputFile.h> // for tool output file
#include <llvm/Support/PrettyStackTrace.h> // for pass list
#include <llvm/IR/LLVMContext.h>		// for llvm LLVMContext
#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriterPass.h> // for createBitcodeWriterPass


using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
        cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool> LEAKCHECKER("leak", cl::init(false),
                                 cl::desc("Memory Leak Detection"));

static cl::opt<bool> FILECHECKER("fileck", cl::init(false),
                                 cl::desc("File Open/Close Detection"));

static cl::opt<bool> DFREECHECKER("dfree", cl::init(false),
                                  cl::desc("Double Free Detection"));

static cl::opt<bool> UAFCHECKER("uaf", cl::init(false),
                                cl::desc("Use-After-Free Detection"));

int main(int argc, char ** argv) {

    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    analysisUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Source-Sink Bug Detector\n");

    SVFModule svfModule(moduleNameVec);

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

    svfModule.dumpModulesToFile(".dvf");

    return 0;

}
