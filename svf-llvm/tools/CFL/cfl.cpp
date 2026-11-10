//===- cfl.cpp -- A driver of CFL Reachability Analysis-------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 //  A driver of CFL Reachability Analysis
 //
 // Author: Yulei Sui,
 */

// This file is a driver for Context-Free Language (CFL) Reachability Analysis. The code
// processes command-line arguments, sets up the analysis based on these arguments, and
// then runs the analysis.

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/PAGBuilder.h"
#include "CFL/CFLAlias.h"
#include "CFL/CFLVF.h"


using namespace llvm;
using namespace SVF;

int main(int argc, char ** argv)
{
    // Parses command-line arguments and stores any module names in moduleNameVec
    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        argc, argv, "CFL Reachability Analysis", "[options] <input-bitcode...>"
                    );

    // Pointer to the SVF Intermediate Representation (IR) of the module
    PAG* pag = nullptr;

    // If no CFLGraph option is specified, the PAG is built from the .bc (bytecode) files of the modules
    if (Options::CFLGraph().empty())
    {
        LLVMModuleSet::buildSVFModule(moduleNameVec);
        PAGBuilder builder;
        pag = builder.build();
    }  // if no dot form CFLGraph is specified, we use pag from .bc.

    // The CFLBase pointer that will be used to run the analysis
    std::unique_ptr<CFLBase> cfl;

    // Determines which type of analysis to run based on the options and sets up cfl accordingly
    if (Options::CFLSVFG())
        cfl = std::make_unique<CFLVF>(pag);
    else if (Options::POCRHybrid())
        cfl = std::make_unique<POCRHybrid>(pag);
    else if (Options::POCRAlias())
        cfl = std::make_unique<POCRAlias>(pag);
    else
        cfl = std::make_unique<CFLAlias>(pag); // if no svfg is specified, we use CFLAlias as the default one.

    // Runs the analysis
    cfl->analyze();

    // Releases the PAG and the LLVMModuleSet to free memory
    PAG::releasePAG();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();


    return 0;

}

