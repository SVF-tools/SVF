//===- mta.cpp --Program Analysis for Multithreaded Programs------------------//
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

#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "MTA/MTA.h"
#include "MTA/SlicedMTA.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"

#include <string>
#include <vector>

using namespace llvm;
using namespace std;
using namespace SVF;

int main(int argc, char** argv)
{
    std::vector<std::string> moduleNameVec = OptionBase::parseOptions(
                argc, argv, "MTA Analysis", "[options] <input-bitcode...>");

    // The slicing pipeline (and its observe modes) lives in the SVF library
    // (SlicedMTA); plain MTA is the default. The sliced runs keep the
    // pre-analysis context-insensitive -- the slice recovers precision later
    // (MSli design) -- so default -max-cxt to 0 for them unless the user set it.
    const bool slicedPipeline = Options::EnableSlicing() || Options::NoSlice()
                                || Options::MTAObserve() || Options::MTAObserveSliced();
    if (slicedPipeline && Options::MaxContextLen.canSet())
        Options::MaxContextLen.setValue(0);

    LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();

    if (slicedPipeline)
    {
        // The only LLVM-dependent step -- materialising resolved indirect calls
        // into the PAG -- is injected here.
        SlicedMTA sliced;
        sliced.runOnModule(pag, [&](CallGraph* cg)
        {
            builder.updateCallGraph(cg);
        });
    }
    else
    {
        MTA mta;
        mta.runOnModule(pag);
    }

    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}
