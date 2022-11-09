#include "SVF-FE/LLVMUtil.h"
#include "WPA/WPAPass.h"
#include "Util/Options.h"
#include "Util/CallerSensitive.h"

using namespace llvm;
using namespace std;
using namespace SVF;

static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
                                                llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

int main(int argc, char **argv)
{

    CallerSensitve *caller = new CallerSensitve();
    std::string jsonPath = caller->getJsonFile(argc, argv);

    int arg_num = 0;
    char **arg_value = new char *[argc];

    std::vector<std::string> moduleNameVec;
    LLVMUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Whole Program Points-to Analysis\n");

    std::string irPath = moduleNameVec[0];

    caller->callerIRCreate(jsonPath, irPath);

    if (Options::WriteAnder == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVFModule *svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    svfModule->buildSymbolTableInfo();

    WPAPass *wpa = new WPAPass();
    wpa->runOnModule(svfModule);

    delete caller;
    return 0;
}