//===- ae.cpp -- Abstract Execution -------------------------------------//
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
 // Abstract Execution
 //
 // Author: Jiawei Wang, Xiao Cheng, Jiawei Yang, Jiawei Ren, Yulei Sui
 */
#include "SVF-LLVM/SVFIRBuilder.h"
#include "WPA/WPAPass.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"

#include "AbstractExecution/AE-SVFIR/BufOverflowChecker.h"


using namespace llvm;
using namespace std;
using namespace SVF;

int main(int argc, char** argv)
{
    int arg_num = 0;
    int extraArgc = 3;
    char **arg_value = new char *[argc + extraArgc];
    for (; arg_num < argc; ++arg_num) {
        arg_value[arg_num] = argv[arg_num];
    }
    // add extra options
    int orgArgNum = arg_num;
    arg_value[arg_num++] = (char*) "-model-consts=true";
    arg_value[arg_num++] = (char*) "-model-arrays=true";
    arg_value[arg_num++] = (char*) "-pre-field-sensitive=false";
    assert(arg_num == (orgArgNum + extraArgc) && "more extra arguments? Change the value of extraArgc");

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
        arg_num, arg_value, "Static Symbolic Execution", "[options] <input-bitcode...>"
    );

    SVFModule *svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
    SVFIRBuilder builder(svfModule);
    SVFIR* pag = builder.build();
    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    PTACallGraph* callgraph = ander->getPTACallGraph();
    builder.updateCallGraph(callgraph);

    if (Options::BufferOverflowCheck()) {
        BufOverflowChecker ae;
        ae.runOnModule(pag);
    } else {
        AE ae;
        ae.runOnModule(pag);
    }

    LLVMModuleSet::releaseLLVMModuleSet();

    return 0;
}