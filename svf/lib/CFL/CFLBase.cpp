//===----- CFLBase.cpp -- CFL Analysis Client Base--------------//
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
//===----------------------------------------------------------------------===//

/*
 * CFLBase.cpp
 *
 *  Created on: Oct 13, 2022
 *      Author: Pei Xu
 */


#include "CFL/CFLBase.h"

namespace SVF
{

class CFLStat;

double CFLBase::timeOfBuildCFLGrammar = 0;
double CFLBase::timeOfNormalizeGrammar = 0;
double CFLBase::timeOfBuildCFLGraph = 0;
double CFLBase::timeOfSolving = 0;
double CFLBase::numOfTerminalEdges = 0;
double CFLBase::numOfTemporaryNonterminalEdges = 0;
double CFLBase::numOfNonterminalEdges = 0;
double CFLBase::numOfStartEdges = 0;
double CFLBase::numOfIteration = 1;
double CFLBase::numOfChecks = 1;

void CFLBase::checkParameter()
{
    // Check for valid grammar file before parsing other options
    std::string filename = Options::GrammarFilename();
    bool pagfile = (filename.rfind("PAGGrammar.txt") == filename.length() - std::string("PAGGrammar.txt").length());
    bool pegfile = (filename.rfind("PEGGrammar.txt") == filename.length() - std::string("PEGGrammar.txt").length());
    bool vfgfile = (filename.rfind("VFGGrammar.txt") == filename.length() - std::string("VFGGrammar.txt").length());
    if (!Options::Customized()  && !(pagfile || pegfile || vfgfile))
    {
        SVFUtil::errs() << "Invalid alias grammar file: " << Options::GrammarFilename() << "\n"
                        << "Please use a file that ends with either 'CFGrammar.txt' or 'PEGGrammar.txt', "
                        << "or use the -customized flag to allow custom grammar files.\n";
        assert(false && "grammar loading failed!");  // exit with error
    }
}

void CFLBase::buildCFLGrammar()
{
    // Start building grammar
    double start = stat->getClk(true);

    GrammarBuilder grammarBuilder = GrammarBuilder(Options::GrammarFilename());
    grammarBase = grammarBuilder.build();

    // Get time of build grammar
    double end = stat->getClk(true);
    timeOfBuildCFLGrammar += (end - start) / TIMEINTERVAL;
}

void CFLBase::buildCFLGraph()
{
    // Start building CFLGraph
    double start = stat->getClk(true);

    AliasCFLGraphBuilder cflGraphBuilder = AliasCFLGraphBuilder();
    if (Options::CFLGraph().empty()) // built from svfir
    {
        PointerAnalysis::initialize();
        ConstraintGraph *consCG = new ConstraintGraph(svfir);
        if (Options::PEGTransfer())
            graph = cflGraphBuilder.buildBiPEGgraph(consCG, grammarBase->getStartKind(), grammarBase, svfir);
        else
            graph = cflGraphBuilder.buildBigraph(consCG, grammarBase->getStartKind(), grammarBase);
        delete consCG;
    }
    else
        graph = cflGraphBuilder.build(Options::CFLGraph(), grammarBase);
    // Check CFL Graph and Grammar are accordance with grammar
    CFLGramGraphChecker cflChecker = CFLGramGraphChecker();
    cflChecker.check(grammarBase, &cflGraphBuilder, graph);

    // Get time of build graph
    double end = stat->getClk(true);
    timeOfBuildCFLGraph += (end - start) / TIMEINTERVAL;
}

void CFLBase::normalizeCFLGrammar()
{
    // Start normalize grammar
    double start = stat->getClk(true);

    CFGNormalizer normalizer = CFGNormalizer();
    grammar = normalizer.normalize(grammarBase);

    // Get time of normalize grammar
    double end = stat->getClk(true);
    timeOfNormalizeGrammar += (end - start) / TIMEINTERVAL;
}

void CFLBase::solve()
{
    // Start solving
    double start = stat->getClk(true);

    solver->solve();

    double end = stat->getClk(true);
    timeOfSolving += (end - start) / TIMEINTERVAL;
}

void CFLBase::finalize()
{
    numOfChecks = solver->numOfChecks;

    BVDataPTAImpl::finalize();
}

void CFLBase::analyze()
{
    initialize();

    solve();

    finalize();
}

CFLGraph* CFLBase::getCFLGraph()
{
    return graph;
}

void CFLBase::countSumEdges()
{
    numOfStartEdges = 0;
    for(auto it = getCFLGraph()->getCFLEdges().begin(); it != getCFLGraph()->getCFLEdges().end(); it++ )
    {
        if ((*it)->getEdgeKind() == grammar->getStartKind())
            numOfStartEdges++;
    }
}

} // End namespace SVF
