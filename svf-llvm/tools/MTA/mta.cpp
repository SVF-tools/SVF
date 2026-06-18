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

// The multi-stage slicing pipeline (and its observe modes) runs the pre-analysis
// context-insensitively (the slice recovers precision later), matching the MSli
// design. Force -max-cxt=0 for those runs unless the user set it explicitly.
static bool wantsSlicedPipeline(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("-enable-slicing", 0) == 0 || arg.rfind("-observe", 0) == 0)
            return true;
    }
    return false;
}

static bool hasMaxCxt(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]).rfind("-max-cxt", 0) == 0)
            return true;
    return false;
}

int main(int argc, char** argv)
{
    // Inject -max-cxt=0 for sliced runs (before option parsing) when not set.
    std::vector<char*> args(argv, argv + argc);
    std::string injected = "-max-cxt=0";
    if (wantsSlicedPipeline(argc, argv) && !hasMaxCxt(argc, argv))
        args.push_back(const_cast<char*>(injected.c_str()));

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
                        static_cast<int>(args.size()), args.data(),
                        "MTA Analysis", "[options] <input-bitcode...>");

    LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();

    // MTA has a single client; slicing is an option (-enable-slicing). The
    // multi-stage on-demand slicing pipeline (MSli) and its observe modes live
    // in the SVF library (SlicedMTA); the only LLVM-dependent step --
    // materialising resolved indirect calls into the PAG -- is injected here.
    if (Options::EnableSlicing() || Options::MTAObserve() || Options::MTAObserveSliced())
    {
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
