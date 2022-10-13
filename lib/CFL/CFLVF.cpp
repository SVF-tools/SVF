//===----- CFLVF.cpp -- CFL Value Flow Client--------------//
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
//===----------------------------------------------------------------------===//

/*
 * CFLAlias.cpp
 *
 *  Created on: September 7 , 2022
 *      Author: Pei Xu
 */

#include "CFL/CFLVF.h"

using namespace SVF;
using namespace cppUtil;
using namespace SVFUtil;

void CFLVF::initialize()
{
    // Build CFL Grammar and Normalize
    GrammarBuilder grammarBuilder = GrammarBuilder(Options::GrammarFilename);
    GrammarBase *grammarBase = grammarBuilder.build();

    // Build CFL Graph
    VFCFLGraphBuilder cflGraphBuilder = VFCFLGraphBuilder();
    if (Options::CFLGraph.empty()) // built from svfir
    {
        PointerAnalysis::initialize();
        AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
        svfg =  memSSA.buildFullSVFG(ander);
        graph = cflGraphBuilder.buildBigraph(svfg, grammarBase->getStartKind(), grammarBase);
    }
    else
        graph = cflGraphBuilder.buildFromDot(Options::CFLGraph, grammarBase);

    // Check CFL Graph and Grammar are accordance with grammar
    CFLGramGraphChecker cflChecker = CFLGramGraphChecker();
    cflChecker.check(grammarBase, &cflGraphBuilder, graph);

    // Normalize grammar
    CFGNormalizer normalizer = CFGNormalizer();
    grammar = normalizer.normalize(grammarBase);

    solver = new CFLSolver(graph, grammar);
    delete grammarBase;
}

void CFLVF::finalize()
{
    if(Options::PrintCFL == true)
    {
        if (Options::CFLGraph.empty())
            svfir->dump("IR");
        grammar->dump("Grammar");
        graph->dump("CFLGraph");
    }
}

void CFLVF::analyze()
{
    initialize();

    solver->solve();

    finalize();
}

void CFLVF::countSumEdges()
{
    numOfSumEdges = 0;
    for(auto it = getCFLGraph()->getCFLEdges().begin(); it != getCFLGraph()->getCFLEdges().end(); it++ )
    {
        if ((*it)->getEdgeKind() == grammar->getStartKind())
            numOfSumEdges++;
    }
}