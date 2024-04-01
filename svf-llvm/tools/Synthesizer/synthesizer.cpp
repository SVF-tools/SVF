//
// Created by LiShangyu on 2024/3/1.
//


#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "Util/Z3Expr.h"
#include "SYN/LightAnalysis.h"

#include <string>


using namespace llvm;
using namespace SVF;

static Option<std::string> SOURCEPATH(
    "srcpath",
    "Path for source code to transform",
    ""
);
//
//static Option<std::string> IRPATH(
//    "irpath",
//    "Path for ir file",
//    ""
//);

static Option<std::string> NEWSPECPATH(
    "newspec",
    "Path for new specification file",
    ""
);


int main(int argc, char ** argv)
{

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
        argc, argv, "Tool to transform your code automatically", "[options] <input-bitcode...>"
    );

    // unused.
    std::string irPath = "";
    std::string srcPath = "";
    std::string specPath = "";

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    if (SOURCEPATH().empty()) {
        std::cerr << "You should specify the path of source code!" << std::endl;
        std::exit(1);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVF::SVFIRBuilder builder(svfModule);
    auto pag = builder.build();
    assert(pag && "pag cannot be nullptr!");

    auto str = SOURCEPATH();
    auto lightAnalysis = new LightAnalysis(str);

    lightAnalysis->runOnSrc();

    return 0;

}
