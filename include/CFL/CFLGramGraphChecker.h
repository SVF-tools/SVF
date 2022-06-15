//===----- CFLGramGraphChecker.h -- CFL Checker for Grammar and graphBuilder alignment --------------//
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
 * CFLGramGraphChecker.h
 *
 *  Created on: May 23, 2022
 *      Author: Pei Xu
 */

#include "CFL/CFLGrammar.h"
#include "Graphs/CFLGraph.h"
namespace SVF
{

class CFLGramGraphChecker
{
public:
    void check(GrammarBase *grammar, CFLGraphBuilder *graphBuilder, CFLGraph *graph)
    {
        /// Check all kinds in grammar in graphBuilder with the same label
        for(auto pairV : grammar->terminals)
        {
            if (graphBuilder->label2KindMap.find(pairV.first) != graphBuilder->label2KindMap.end())
            {
                assert(graphBuilder->label2KindMap[pairV.first] == pairV.second);
                assert(graphBuilder->kind2LabelMap[pairV.second] == pairV.first);
            }
        }

        for(auto pairV : grammar->nonterminals)
        {
            if (graphBuilder->label2KindMap.find(pairV.first) != graphBuilder->label2KindMap.end())
            {
                assert(graphBuilder->label2KindMap[pairV.first] == pairV.second);
                assert(graphBuilder->kind2LabelMap[pairV.second] == pairV.first);
            } 
            else
            {
                graphBuilder->label2KindMap.insert(std::make_pair (pairV.first,pairV.second));
                graphBuilder->kind2LabelMap.insert(std::make_pair (pairV.second, pairV.first));
            }
        }

        /// Get Kind2Attrs Map from Graph to Grammar
        grammar->kind2AttrsMap = graphBuilder->kind2AttrsMap;
        graph->startKind = grammar->startKind;
    }
};

}// SVF

