//===----- CFLGramGraphChecker.h -- CFL Checker for Grammar and graphBuilder alignment --------------//
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
 * CFLGramGraphChecker.h
 *
 *  Created on: May 23, 2022
 *      Author: Pei Xu
 */
#ifndef INCLUDE_CFL_CFLGRAMGRAPHCHECKER_H_
#define INCLUDE_CFL_CFLGRAMGRAPHCHECKER_H_

#include "CFL/CFGrammar.h"
#include "Graphs/CFLGraph.h"
namespace SVF
{

class CFLGramGraphChecker
{
public:
    void check(GrammarBase *grammar, CFLGraphBuilder *graphBuilder, CFLGraph *graph)
    {
        /// Check all kinds in grammar in graphBuilder with the same label
        for(auto pairV : grammar->getTerminals())
        {
            if (graphBuilder->getLabelToKindMap().find(pairV.first) != graphBuilder->getLabelToKindMap().end())
            {
                assert(graphBuilder->getLabelToKindMap()[pairV.first] == pairV.second);
                assert(graphBuilder->getKindToLabelMap()[pairV.second] == pairV.first);
            }
        }

        for(auto pairV : grammar->getNonterminals())
        {
            if (graphBuilder->getLabelToKindMap().find(pairV.first) != graphBuilder->getLabelToKindMap().end())
            {
                assert(graphBuilder->getLabelToKindMap()[pairV.first] == pairV.second);
                assert(graphBuilder->getKindToLabelMap()[pairV.second] == pairV.first);
            }
            else
            {
                graphBuilder->getLabelToKindMap().insert(std::make_pair (pairV.first,pairV.second));
                graphBuilder->getKindToLabelMap().insert(std::make_pair (pairV.second, pairV.first));
            }
        }

        /// Get KindToAttrs Map from Graph to Grammar
        grammar->setKindToAttrsMap(graphBuilder->getKindToAttrsMap());
        graph->startKind = grammar->getStartKind();
    }
};

}// SVF

#endif /* INCLUDE_CFL_CFLGRAMGRAPHCHECKER_H_*/