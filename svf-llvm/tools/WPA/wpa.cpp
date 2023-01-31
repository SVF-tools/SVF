//===- wpa.cpp -- Whole program analysis -------------------------------------//
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
 // Whole Program Pointer Analysis
 //
 // Author: Yulei Sui,
 */

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/WPAPass.h"
#include "SVFIR/SVFModuleRW.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"

using namespace llvm;
using namespace std;
using namespace SVF;

static Option<std::string> dumpToJsonFile("dump-json",
                                          "Dump SVFModule to JSON file", "");

int main(int argc, char** argv)
{

    char** arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    moduleNameVec =
        OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                 "[options] <input-bitcode...>");

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule =
        LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);

    std::string jsonPath = dumpToJsonFile();
    if (!jsonPath.empty())
        SVFModuleWrite(svfModule, jsonPath);

    /// Build SVFIR
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();

    WPAPass wpa;
    wpa.runOnModule(pag);

    delete[] arg_value;
    return 0;
}
