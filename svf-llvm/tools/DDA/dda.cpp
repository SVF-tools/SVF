//===- dda.cpp --On Demand Value Flow Analysis---------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

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
    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        argc, argv, "Demand-Driven Points-to Analysis", "[options] <input-bitcode...>"
                    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();

    DDAPass dda;
    dda.runOnModule(pag);

    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;

}

