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

void CFLVF::buildCFLGraph()
{
    // Build CFL Graph
    VFCFLGraphBuilder cflGraphBuilder = VFCFLGraphBuilder();
    if (Options::CFLGraph().empty()) // built from svfir
    {
        PointerAnalysis::initialize();
        AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
        svfg =  memSSA.buildFullSVFG(ander);
        graph = cflGraphBuilder.buildBigraph(svfg, grammarBase->getStartKind(), grammarBase);
    }
    else
        graph = cflGraphBuilder.buildFromDot(Options::CFLGraph(), grammarBase);

    // Check CFL Graph and Grammar are accordance with grammar
    CFLGramGraphChecker cflChecker = CFLGramGraphChecker();
    cflChecker.check(grammarBase, &cflGraphBuilder, graph);
}

void CFLVF::initialize()
{
    // Build CFL Grammar
    buildCFLGrammar();

    // Build CFL Graph
    buildCFLGraph();

    // Normalize grammar
    normalizeCFLGrammar();

    // Initialize sovler
    solver = new CFLSolver(graph, grammar);
}

void CFLVF::finalize()
{
    if(Options::PrintCFL())
    {
        if (Options::CFLGraph().empty())
            svfir->dump("IR");
        grammar->dump("Grammar");
        graph->dump("CFLGraph");
    }
}
