//===----- CFLAlias.cpp -- CFL Alias Analysis Client--------------//
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
 *  Created on: June 27 , 2022
 *      Author: Pei Xu
 */

#include "CFL/CFLAlias.h"

using namespace SVF;

void CFLAlias::analyze()
{
    GrammarBuilder grammarBuilder = GrammarBuilder(Options::GrammarFilename);
    CFGNormalizer normalizer = CFGNormalizer();
    AliasCFLGraphBuilder cflGraphBuilder = AliasCFLGraphBuilder();
    CFLGramGraphChecker cflChecker = CFLGramGraphChecker();
    if (Options::GraphIsFromDot == false)
    {
        PointerAnalysis::initialize();
        GrammarBase *grammarBase = grammarBuilder.build();
        ConstraintGraph *consCG = new ConstraintGraph(svfir);
        graph = cflGraphBuilder.buildBigraph(consCG, grammarBase->getStartKind(), grammarBase);
        cflChecker.check(grammarBase, &cflGraphBuilder, graph);
        grammar = normalizer.normalize(grammarBase);
        cflChecker.check(grammar, &cflGraphBuilder, graph);
        std::string svfirName = Options::InputFilename.c_str();
        svfir->dump(svfirName.append("_IR"));
        std::string grammarName = Options::InputFilename.c_str();
        grammar->dump(grammarName.append("_Grammar"));
        delete consCG;
        delete grammarBase;
    }
    else
    {
        GrammarBase *grammarBase = grammarBuilder.build();
        graph = cflGraphBuilder.buildFromDot(Options::InputFilename, grammarBase);
        cflChecker.check(grammarBase, &cflGraphBuilder, graph);
        grammar = normalizer.normalize(grammarBase);
        cflChecker.check(grammar, &cflGraphBuilder, graph);
        delete grammarBase;
    }
    solver = new CFLSolver(graph, grammar);
    solver->solve();
    std::string CFLGraphFileName = Options::InputFilename.c_str();
    graph->dump(CFLGraphFileName.append("_CFL"));
    if (Options::GraphIsFromDot == false)
    {
        PointerAnalysis::finalize();
    }
}