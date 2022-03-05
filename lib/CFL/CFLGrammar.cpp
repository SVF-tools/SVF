//===----- CFLGrammar.cpp -- Context-free grammar --------------------------//
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
 * CFLGrammar.cpp
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#include "CFL/CFLGrammar.h"
#include <string>

using namespace SVF;


CFLGrammar::CFLGrammar(){

}
/// Read grammar from file
/// Each string format terminal or nonterminal read from file is mapped to a Symbol (int)
void CFLGrammar::readGrammar(std::string filename){
    u32_t totalSymbols = 0;

    startSymbol = totalSymbols++;
    ternimals["S"] = totalSymbols++;
    ternimals["epsilon"] = totalSymbols++;
    nonternimals["X"] = totalSymbols++;
    nonternimals["Y"] = totalSymbols++;
    nonternimals["Z"] = totalSymbols++;

    /// map strings to symbols
    /// check existence of ternimal and nonterminals before making productions
    Symbol e = str2Sym("epsilon");
    Symbol x = str2Sym("X");
    Symbol y = str2Sym("Y");
    Symbol z = str2Sym("Z");

    /// create productions 
    /// x -> y is represented as a vector {x, y}
    /// x -> y z is represented as a vector {x, y, z}
    /// Normalized grammar can only allows three elements in a production.

    Production prod1 = {x, e};
    epsilonProds.insert(prod1);

    Production prod2 = {x, y};
    singleRHS2Prods[y].insert(prod2);

    Production prod3 = {x, y, z};
    firstRHS2Prods[y].insert(prod3);
    
    Production prod4 = {x, z, y};
    secondRHS2Prods[y].insert(prod4);

}

CFLGrammar::Symbol CFLGrammar::str2Sym(std::string str) const{

    auto tit = ternimals.find(str);
    if(tit!=ternimals.end())
        return tit->second;

    auto nit = nonternimals.find(str);
    if(nit!=nonternimals.end())
        return nit->second;

    assert(false && "symbol not found!");
    abort();
}
