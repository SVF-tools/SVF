/*
 // Multi-threaded Program Analysis
 //
 // Author: Yulei Sui,
 */

#include "MTA/MTA.h"
#include "Util/SVFUtil.h"
#include <llvm/IR/LegacyPassManager.h>


using namespace llvm;

static llvm::cl::opt<std::string> InputFilename(cl::Positional,
        llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

static llvm::cl::opt<bool> RACE("mhp", llvm::cl::init(true),
                          llvm::cl::desc("Data Race Detection"));

static llvm::cl::opt<std::string>
DefaultDataLayout("default-data-layout",
                  llvm::cl::desc("data layout string to use if not specified by module"),
                  llvm::cl::value_desc("layout-string"), llvm::cl::init(""));

int main(int argc, char ** argv) {

    llvm::legacy::PassManager Passes;

    PassRegistry &Registry = *PassRegistry::getPassRegistry();

    initializeCore(Registry);
    initializeScalarOpts(Registry);
    initializeIPO(Registry);
    initializeAnalysis(Registry);
    initializeTransformUtils(Registry);
    initializeInstCombine(Registry);
    initializeInstrumentation(Registry);
    initializeTarget(Registry);

    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    SVFUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Analysis for Multithreaded programs\n");

    SVFModule svfModule(moduleNameVec);

    Passes.add(new MTA());
    Passes.run(*svfModule.getMainLLVMModule());

    svfModule.dumpModulesToFile(".mta");

    return 0;

}

