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


#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "CFL/CFLAlias.h"
#include "CFL/CFLVF.h"

using namespace llvm;
using namespace SVF;

int main(int argc, char ** argv)
{
    char **arg_value = new char*[argc];
    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        argc, argv, "CFL Reachability Analysis", "[options] <input-bitcode...>"
                    );

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::getLLVMModuleSet()->preProcessBCs(moduleNameVec);
    }

    SVFIR* svfir = nullptr;
    if (Options::CFLGraph().empty())
    {
        SVFModule* svfModule = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
        SVFIRBuilder builder(svfModule);
        svfir = builder.build();
    }  // if no dot form CFLGraph is specified, we use svfir from .bc.

    std::unique_ptr<CFLBase> cfl;
    if (Options::CFLSVFG())
        cfl = std::make_unique<CFLVF>(svfir);
    else if (Options::CFLData) 
        cfl = std::make_unique<POCRAlias>(svfir);
    else
        cfl = std::make_unique<CFLAlias>(svfir); // if no svfg is specified, we use CFLAlias as the default one.
    cfl->analyze();

    SVFIR::releaseSVFIR();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();

    delete[] arg_value;

    return 0;

}

