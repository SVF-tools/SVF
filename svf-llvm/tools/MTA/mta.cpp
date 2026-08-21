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
#include "MTA/MTAStat.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"

#include <string>
#include <vector>

using namespace llvm;
using namespace std;
using namespace SVF;

namespace
{

AndersenWaveDiff* preparePreAnalysis(SVFIR* pag, SVFIRBuilder& builder)
{
    ScopedPhaseTimer timer("Andersen's pointer analysis");

    AndersenWaveDiff* preAnalysis =
        AndersenWaveDiff::createAndersenWaveDiff(pag);
    if (Options::DumpMTAGraphs())
    {
        preAnalysis->getConstraintGraph()->dump("original_consg");
        preAnalysis->getCallGraph()->dump("original_tcg");
    }
    builder.updateCallGraph(preAnalysis->getCallGraph());
    pag->getICFG()->updateCallGraph(preAnalysis->getCallGraph());
    if (Options::DumpMTAGraphs())
        pag->getICFG()->dump("original_icfg");

    return preAnalysis;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> moduleNameVec = OptionBase::parseOptions(
                argc, argv, "MTA Analysis", "[options] <input-bitcode...>");

    LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVFIRBuilder builder;
    SVFIR* pag = builder.build();

    // MTA's only client is race detection. -mta-flow-sensitive (default) selects the
    // FSAM pipeline (SlicedMTA), which decides slicing and the pre-analysis
    // context handling internally; otherwise run the flow-insensitive Andersen
    // detector.
    bool succeeded = true;
    if (Options::MTFlowSensitive())
    {
        AndersenWaveDiff* preAnalysis = preparePreAnalysis(pag, builder);
        SlicedMTA sliced;
        succeeded = sliced.runOnModule(pag, *preAnalysis);
    }
    else
    {
        MTA mta;
        succeeded = !mta.runOnModule(pag);
    }

    AndersenWaveDiff::releaseAndersenWaveDiff();
    SVFIR::releaseSVFIR();
    LLVMModuleSet::releaseLLVMModuleSet();
    return succeeded ? 0 : 1;
}
