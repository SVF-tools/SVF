/*
 // On Demand Value Flow Analysis
 //
 // Author: Yulei Sui,
 */

//#include "AliasUtil/AliasAnalysisCounter.h"
//#include "MemoryModel/ComTypeModel.h"
#include "SVF-FE/LLVMUtil.h"
#include "DDA/DDAPass.h"

using namespace llvm;
using namespace SVF;

static cl::opt<std::string> InputFilename(cl::Positional,
        cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool>
StandardCompileOpts("std-compile-opts",
                    cl::desc("Include the standard compile time optimizations"));

//static cl::list<const PassInfo*, bool, PassNameParser>
//PassList(cl::desc("Optimizations available:"));

static cl::opt<bool> DAA("daa", cl::init(false),
                         cl::desc("Demand-Driven Alias Analysis Pass"));

static cl::opt<bool> REGPT("dreg", cl::init(false),
                           cl::desc("Demand-driven regular points-to analysis"));

static cl::opt<bool> RFINEPT("dref", cl::init(false),
                             cl::desc("Demand-driven refinement points-to analysis"));

static cl::opt<bool> ENABLEFIELD("fdaa", cl::init(false),
                                 cl::desc("enable field-sensitivity for demand-driven analysis"));

static cl::opt<bool> ENABLECONTEXT("cdaa", cl::init(false),
                                   cl::desc("enable context-sensitivity for demand-driven analysis"));

static cl::opt<bool> ENABLEFLOW("ldaa", cl::init(false),
                                cl::desc("enable flow-sensitivity for demand-driven analysis"));

int main(int argc, char ** argv)
{

    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    SVFUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "Demand-Driven Points-to Analysis\n");

    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);

    DDAPass *dda = new DDAPass();
    dda->runOnModule(svfModule);

    return 0;

}

