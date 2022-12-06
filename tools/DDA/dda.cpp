/*
 // On Demand Value Flow Analysis
 //
 // Author: Yulei Sui,
 */

//#include "AliasUtil/AliasAnalysisCounter.h"
//#include "MemoryModel/ComTypeModel.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "DDA/DDAPass.h"
#include "Util/Options.h"

using namespace llvm;
using namespace SVF;

//static cl::list<const PassInfo*, bool, PassNameParser>
//PassList(cl::desc("Optimizations available:"));

static Option<bool> DAA(
    "daa",
    "Demand-Driven Alias Analysis Pass",
    false
);

static Option<bool> REGPT(
    "dreg",
    "Demand-driven regular points-to analysis",
    false
);

static Option<bool> RFINEPT(
    "dref",
    "Demand-driven refinement points-to analysis",
    false
);

static Option<bool> ENABLEFIELD(
    "fdaa",
    "enable field-sensitivity for demand-driven analysis",
    false
);

static Option<bool> ENABLECONTEXT(
    "cdaa",
    "enable context-sensitivity for demand-driven analysis",
    false
);

static Option<bool> ENABLEFLOW(
    "ldaa",
    "enable flow-sensitivity for demand-driven analysis",
    false
);

int main(int argc, char ** argv)
{

    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        argc, argv, "Demand-Driven Points-to Analysis", "[options] <input-bitcode...>"
                    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();

    DDAPass dda;
    dda.runOnModule(pag);

    delete[] arg_value;
    return 0;

}

