//===- llvm2svf.cpp -- LLVM IR to SVF IR conversion  -------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2023>  <Yulei Sui>
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
 * llvm2svf.cpp
 *
 *  Created on: 21 Apr 2023
 *     Authors: Xudong Wang
 */

#include "SVF-LLVM/SVFIRBuilder.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "SVFIR/SVFFileSystem.h"


using namespace std;
using namespace SVF;

#include <iostream>
#include <string>

std::string replaceExtension(const std::string& path)
{
    size_t pos = path.rfind('.');
    if (pos == std::string::npos ||
            (path.substr(pos) != ".bc" && path.substr(pos) != ".ll"))
    {
        SVFUtil::errs() << "Error: expect file with extension .bc or .ll\n";
        exit(EXIT_FAILURE);
    }
    return path.substr(0, pos) + ".svf.json";
}

int main(int argc, char** argv)
{
    auto moduleNameVec = OptionBase::parseOptions(
                             argc, argv, "llvm2svf", "[options] <input-bitcode...>");

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);
    const std::string jsonPath = replaceExtension(moduleNameVec.front());
    // PAG is borrowed from a unique_ptr, so we don't need to delete it.
    const SVFIR* pag = SVFIRBuilder(svfModule).build();
    SVFIRWriter::writeJsonToPath(pag, jsonPath);
    SVFUtil::outs() << "SVF IR is written to '" << jsonPath << "'\n";

    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}
