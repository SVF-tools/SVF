//===----- CFLGramGraphChecker.h -- CFL Checker for Grammar and Graph alignment --------------//
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
    void check(GrammarBase *grammar, CFLGraph *graph)
    {
        /// Check all the symbol in grammar in graph With the same label
        for(auto pairV : grammar->terminals)
        {
            if (graph->label2SymMap.find(pairV.first) != graph->label2SymMap.end())
            {
                assert(graph->label2SymMap[pairV.first] == pairV.second);
            }
        }

        for(auto pairV : grammar->nonterminals)
        {
            if (graph->label2SymMap.find(pairV.first) != graph->label2SymMap.end())
            {
                assert(graph->label2SymMap[pairV.first] == pairV.second);
            }
        }

        /// Get Kind2Attr Map from Graph to Grammar
        grammar->kind2AttrMap = graph->kind2AttrMap;
        graph->startSymbol = grammar->startSymbol;
    }
};

}// SVF

