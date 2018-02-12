/*
 // Multi-threaded Program Analysis
 //
 // Author: Yulei Sui,
 */

#include "MTA/MTA.h"
#include "Util/AnalysisUtil.h"

#include <llvm-c/Core.h> // for LLVMGetGlobalContext()
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>	// for cl
#include <llvm/Support/FileSystem.h>	// for sys::fs::F_None
#include <llvm/Bitcode/BitcodeWriterPass.h>  // for bitcode write
#include <llvm/IR/LegacyPassManager.h>		// pass manager
#include <llvm/Support/Signals.h>	// singal for command line
#include <llvm/IRReader/IRReader.h>	// IR reader for bit file
#include <llvm/Support/ToolOutputFile.h> // for tool output file
#include <llvm/Support/PrettyStackTrace.h> // for pass list
#include <llvm/IR/LLVMContext.h>		// for llvm LLVMContext
#include <llvm/Support/SourceMgr.h> // for SMDiagnostic
#include <llvm/Bitcode/BitcodeWriterPass.h>		// for createBitcodeWriterPass
#include <llvm/IR/DataLayout.h>		// data layout

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
        cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool> RACE("mhp", cl::init(true),
                          cl::desc("Data Race Detection"));

static cl::opt<std::string>
DefaultDataLayout("default-data-layout",
                  cl::desc("data layout string to use if not specified by module"),
                  cl::value_desc("layout-string"), cl::init(""));

int main(int argc, char ** argv) {

    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    analysisUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Analysis for Multithreaded programs\n");

    SVFModule svfModule(moduleNameVec);

    MTA *mta = new MTA();
    mta->runOnModule(svfModule);

    svfModule.dumpModulesToFile(".mta");

    return 0;

}

