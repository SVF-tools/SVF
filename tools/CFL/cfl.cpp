//===- cfl.cpp -- A driver of CFL Reachability Analysis-------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===-----------------------------------------------------------------------===//

/*
 //  A driver of CFL Reachability Analysis
 //
 // Author: Yulei Sui,
 */


#include "SVF-FE/LLVMUtil.h"
#include "Util/Options.h"
#include "CFL/CFLAlias.h"
#include "CFL/CFLVF.h"
#include "SVF-FE/SVFIRBuilder.h"
#include "CFL/CFGNormalizer.h"

using namespace llvm;
using namespace SVF;

static cl::opt<bool>
StandardCompileOpts("std-compile-opts",
                    cl::desc("Include the standard compile time optimizations"));

static llvm::cl::opt<std::string> InputFilename(llvm::cl::Positional,
        llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

int main(int argc, char ** argv)
{
    int arg_num = 0;
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    LLVMUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
    cl::ParseCommandLineOptions(arg_num, arg_value,
                                "CFL Reachability Analysis\n");

    if (Options::WriteAnder == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }
    if (Options::CFLGraph.empty())
    {
        SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
        svfModule->buildSymbolTableInfo();

        /// Build Program Assignment Graph (SVFIR)
        SVFIRBuilder builder;
        SVFIR* svfir = builder.build(svfModule);
        if (Options::CFLSVFG)
        {
            CFLVF* cflvf = new CFLVF(svfir);
            //cflvf->analyze();
            delete cflvf;
        }
        else
        {
            CFLAlias* cflaa = new CFLAlias(svfir);
            cflaa->analyze();
            delete cflaa;
        }
    }
    else
    {
        SVFIR* svfir = nullptr;
        CFLAlias* cflaa = new CFLAlias(svfir);
        cflaa->analyze();
        delete cflaa;
    }

    SVFIR::releaseSVFIR();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    return 0;

}

