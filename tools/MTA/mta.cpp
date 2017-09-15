/*
 // Multi-threaded Program Analysis
 //
 // Author: Yulei Sui,
 */

#include "MTA/MTA.h"

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
#include <llvm/Bitcode/ReaderWriter.h>		// for createBitcodeWriterPass
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

    sys::PrintStackTraceOnErrorSignal();
    llvm::PrettyStackTraceProgram X(argc, argv);

    LLVMContext &Context = getGlobalContext();

    std::string OutputFilename;

    cl::ParseCommandLineOptions(argc, argv, "Multi-threaded Program Analysis\n");
    sys::PrintStackTraceOnErrorSignal();

    PassRegistry &Registry = *PassRegistry::getPassRegistry();

    initializeCore(Registry);
    initializeScalarOpts(Registry);
    initializeIPO(Registry);
    initializeAnalysis(Registry);
    initializeTransformUtils(Registry);
    initializeInstCombine(Registry);
    initializeInstrumentation(Registry);
    initializeTarget(Registry);

    llvm::legacy::PassManager Passes;

    SMDiagnostic Err;

    // Load the input module...
    std::unique_ptr<Module> M1 = parseIRFile(InputFilename, Err, Context);

    if (!M1) {
        Err.print(argv[0], errs());
        return 1;
    }

    std::unique_ptr<tool_output_file> Out;
    std::error_code ErrorInfo;

    OutputFilename = InputFilename + ".mta";

    Out.reset(
        new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                             sys::fs::F_None));

    if (ErrorInfo) {
        errs() << ErrorInfo.message() << '\n';
        return 1;
    }

    // Add an appropriate DataLayout instance for this module.

    if(RACE)
        Passes.add(new MTA());

    Passes.add(createBitcodeWriterPass(Out->os()));

    Passes.run(*M1.get());
    Out->keep();

    return 0;

}

