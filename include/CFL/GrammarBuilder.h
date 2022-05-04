//===----- CFGNormalizer.h -- CFL Alias Analysis Client--------------//
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
 * GrammarBuilder.h
 *
 *  Created on: April 26, 2022
 *      Author: Pei Xu
 */


#include "CFL/CFLGrammar.h"
#include "Graphs/CFLGraph.h"

namespace SVF{

/*!
 * Build Grammar from a user specified grammar text
 * Input Format:
 *      Start:              // Special string 'epsilon' for empty RHS
 *      M                   // Specify Start Symbol in Second Line 
 *      Productions:        // Each Symbol seperate by 'Space', production end with ';'
 *      M -> V d;           // Terminal in NonCapital
 *      M -> dbar V d;      // NonTerminal in Capital
 *      V -> M abar M a M;  // LHS and RHS, Seperate by '->'
 *      V -> ( M ? abar ) * M ? ( a M ? ) *;    // Support '(' ')' '?' '*' four regular expression sign
 * Note:
 *      When provide EBNF form text (i.e Last production above),
 *      Please specify -ebnf flag, otherwise regular experssion sign will treat as NonTerminal. 
 */

class GrammarBuilder{
public:
    std::string fileName;
    GrammarBase *grammar;

    GrammarBuilder(std::string fileName): fileName(fileName), grammar(nullptr){
        grammar = new GrammarBase();
    };

    GrammarBase* build(); 

    GrammarBase* build(Map<std::string, SVF::CFLGraph::Symbol> *preMap);
};

} // SVF 
