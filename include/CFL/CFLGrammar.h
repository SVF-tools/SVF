//===----- CFLGrammar.h -- Context-free grammar --------------------------//
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
 * CFLGrammar.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */
#ifndef CFLGrammar_H_
#define CFLGrammar_H_
#include "Util/BasicTypes.h"

namespace SVF
{

class GrammarBase
{
public:
    typedef u32_t Symbol;
    typedef std::vector<Symbol> Production;
    typedef Set<Production> Productions;
    Map<std::string, Symbol> terminals;
    Map<std::string, Symbol> nonterminals;

    Symbol str2Sym(std::string str) const;

    std::string sym2Str(Symbol sym) const;

    Symbol getSymbol(const Production& prod, u32_t pos)
    {
        return prod.at(pos);
    }

    Symbol insertTerminalSymbol(std::string strLit);
    Symbol insertNonTerminalSymbol(std::string strLit);

    Symbol startSymbol;
    Symbol totalSymbol;
    Map<Symbol, Productions> rawProductions;
};

class CFLGrammar : public GrammarBase
{

public:
    Set<Production > epsilonProds;
    Map<Symbol, Productions> singleRHS2Prods;
    Map<Symbol, Productions> firstRHS2Prods;
    Map<Symbol, Productions> secondRHS2Prods;
    Symbol newTerminalSubscript;
    CFLGrammar();

    const Productions& getEpsilonProds() const
    {
        return epsilonProds;
    }

    const bool hasProdsFromFirstRHS(Symbol sym) const
    {
        auto it = firstRHS2Prods.find(sym);
        return it!=firstRHS2Prods.end();
    }

    const bool hasProdsFromSingleRHS(Symbol sym) const
    {
        auto it = singleRHS2Prods.find(sym);
        return it!=singleRHS2Prods.end();
    }

    const bool hasProdsFromSecondRHS(Symbol sym) const
    {
        auto it = secondRHS2Prods.find(sym);
        return it!=secondRHS2Prods.end();
    }

    const Productions& getProdsFromSingleRHS(Symbol sym) const
    {
        auto it = singleRHS2Prods.find(sym);
        assert(it!=singleRHS2Prods.end() && "production (X -> sym) not found for sym!!");
        return it->second;
    }

    const Productions& getProdsFromFirstRHS(Symbol sym) const
    {
        auto it = firstRHS2Prods.find(sym);
        assert(it!=firstRHS2Prods.end() && "production (X -> sym Y) not found for sym!!");
        return it->second;
    }

    const Productions& getProdsFromSecondRHS(Symbol sym) const
    {
        auto it = secondRHS2Prods.find(sym);
        assert(it!=secondRHS2Prods.end() && "production (X -> Y sym) not found for sym!!");
        return it->second;
    }

    Symbol getLHSSymbol(const Production& prod)
    {
        return prod.at(0);
    }

    Symbol getFirstRHSSymbol(const Production& prod)
    {
        return prod.at(1);
    }

    Symbol getSecondRHSSymbol(const Production& prod)
    {
        return prod.at(2);
    }

    inline int num_generator()
    {
        return newTerminalSubscript++;
    }

};


}
#endif /* CFLGrammar_H_ */