#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "MTA/MTA.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "MTAResultValidator.h"
#include "LockResultValidator.h"
using namespace llvm;
using namespace std;
using namespace SVF;

int main(int argc, char ** argv)
{

    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
        argc, argv, "MTA Analysis", "[options] <input-bitcode...>"
    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();

    MTA mta;
    mta.runOnModule(pag);

    MTAResultValidator MTAValidator(mta.getMHP());
    MTAValidator.analyze();

    // Initialize the validator and perform validation.
    LockResultValidator lockvalidator(mta.getLockAnalysis());
    lockvalidator.analyze();

    delete[] arg_value;

    return 0;
}
