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
#include "Util/SVFBasicTypes.h"
#include "WPA/Andersen.h"

using namespace SVF;
using namespace cppUtil;
using namespace SVFUtil;



void CFLVF::analyze()
{
    GrammarBuilder grammarBuilder = GrammarBuilder(Options::GrammarFilename);
    CFGNormalizer normalizer = CFGNormalizer();
    VFCFLGraphBuilder cflGraphBuilder = VFCFLGraphBuilder();
    CFLGramGraphChecker cflChecker = CFLGramGraphChecker();
    if (Options::CFLGraph.empty())
    {
        PointerAnalysis::initialize();
        GrammarBase *grammarBase = grammarBuilder.build();
        AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
        svfg =  memSSA.buildFullSVFG(ander);
        ConstraintGraph *consCG = new ConstraintGraph(svfir);
        if (Options::PEGTransfer)
        {
            graph = cflGraphBuilder.buildBiPEGgraph(consCG, grammarBase->getStartKind(), grammarBase, svfir);
        }
        else
        {
            graph = cflGraphBuilder.buildBigraph(svfg, grammarBase->getStartKind(), grammarBase);
        }

        cflChecker.check(grammarBase, &cflGraphBuilder, graph);
        grammar = normalizer.normalize(grammarBase);
        cflChecker.check(grammar, &cflGraphBuilder, graph);
        delete consCG;
        delete grammarBase;
    }
    else
    {
        GrammarBase *grammarBase = grammarBuilder.build();
        graph = cflGraphBuilder.buildFromDot(Options::CFLGraph, grammarBase);
        cflChecker.check(grammarBase, &cflGraphBuilder, graph);
        grammar = normalizer.normalize(grammarBase);
        cflChecker.check(grammar, &cflGraphBuilder, graph);
        delete grammarBase;
    }
    solver = new CFLSolver(graph, grammar);
    solver->solve();
    if(Options::PrintCFL == true)
    {
        svfir->dump("IR");
        grammar->dump("Grammar");
        graph->dump("CFLGraph");
    }
    if (Options::CFLGraph.empty())
    {
        PointerAnalysis::finalize();
    }
}