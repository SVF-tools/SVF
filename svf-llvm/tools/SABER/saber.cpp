//===- saber.cpp -- Source-sink bug checker------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
//===-----------------------------------------------------------------------===//

/*
 // Saber: Software Bug Check.
 //
 // Author: Yulei Sui,
 */

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SABER/LeakChecker.h"
#include "SABER/FileChecker.h"
#include "SABER/DoubleFreeChecker.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "Util/Z3Expr.h"


using namespace llvm;
using namespace SVF;

static Option<bool> LEAKCHECKER(
    "leak",
    "Memory Leak Detection",
    false
);

static Option<bool> FILECHECKER(
    "fileck",
    "File Open/Close Detection",
    false
);

static Option<bool> DFREECHECKER(
    "dfree",
    "Double Free Detection",
    false
);

int main(int argc, char ** argv)
{

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        argc, argv, "Source-Sink Bug Detector", "[options] <input-bitcode...>"
                    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();

    std::unique_ptr<LeakChecker> saber;

    if(LEAKCHECKER())
        saber = std::make_unique<LeakChecker>();
    else if(FILECHECKER())
        saber = std::make_unique<FileChecker>();
    else if(DFREECHECKER())
        saber = std::make_unique<DoubleFreeChecker>();
    else
        saber = std::make_unique<LeakChecker>();  // if no checker is specified, we use leak checker as the default one.

    saber->runOnModule(pag);

    LLVMModuleSet::releaseLLVMModuleSet();

    return 0;

}
