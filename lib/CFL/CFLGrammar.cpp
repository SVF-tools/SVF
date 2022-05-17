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
#include <iostream>
#include <fstream>

using namespace SVF;

GrammarBase::Symbol GrammarBase::str2Sym(std::string str) const
{

    auto tit = terminals.find(str);
    if(tit!=terminals.end())
        return tit->second;

    auto nit = nonterminals.find(str);
    if(nit!=nonterminals.end())
        return nit->second;

    assert(false && "symbol not found!");
    abort();
}

std::string GrammarBase::sym2Str(Symbol sym) const
{

    std::string key = "";
    for (auto &i : terminals)
    {
        if (i.second == sym)
        {
            key = i.first;
            return key;
        }
    }

    std::string nkey = "";
    for (auto &ni : nonterminals)
    {
        if (ni.second == sym)
        {
            nkey = ni.first;
            return nkey;
        }
    }

    return "";
}

GrammarBase::Symbol GrammarBase::insertTerminalSymbol(std::string strLit)
{
    Symbol sym;
    if (terminals.find(strLit) == terminals.end())
    {
        sym = totalSymbol++;
        terminals.insert({strLit, sym});
    }
    else
    {
        sym = str2Sym(strLit);
    }
    return sym;
}
GrammarBase::Symbol GrammarBase::insertNonTerminalSymbol(std::string strLit)
{
    Symbol sym;
    if (nonterminals.find(strLit) == nonterminals.end())
    {
        sym = totalSymbol++;
        nonterminals.insert({strLit, sym});
    }
    else
    {
        sym = str2Sym(strLit);
    }
    if (strLit.back() == 'i')
    {
        insertAttribute(sym);
    }
    return sym;
}

void GrammarBase::insertAttribute(Symbol s)
{
    attributeSymbol.insert(s);
}

CFLGrammar::CFLGrammar()
{
    newTerminalSubscript = 0;
}

void CFLGrammar::dump(){
    std::ofstream gramFile("Normailized_Grammar.txt");
    gramFile << "Start Symbol:\n";
    gramFile << '\t' << sym2Str(startSymbol) << '(' << startSymbol << ')' << ' ' << "\n\n";

    gramFile << "Epsilon Production:\n";
    for (auto pro: this->epsilonProds)
    {
        gramFile << '\t';
        for (auto sym : pro){
            if (sym == pro[1])
            {
                gramFile << "-> ";
            }
            gramFile << sym2Str(sym) << '(' << sym << ')' << ' '; 
        }
        gramFile << '\n';
    }
    gramFile << '\n';

    gramFile << "Single Production:\n";
    for (auto symProPair: singleRHS2Prods) 
    {
        for (auto pro: symProPair.second)
        {
            gramFile << '\t';
            int i = 0;
            for (auto sym : pro){
                i++;
                if (i == 2)
                {
                    gramFile << "-> ";
                }
                gramFile << sym2Str(sym) << '(' << sym << ')' << ' '; 
            }
            gramFile << '\n';
         }
    }
    gramFile << '\n';

    gramFile << "Binary Production:\n";
    for (auto symProPair: firstRHS2Prods) 
    {
        for (auto pro: symProPair.second)
        {
            gramFile << '\t';
            int i = 0;
            for (auto sym : pro){
                i++;
                if (i == 2)
                {
                    gramFile << "-> ";
                }
                gramFile << sym2Str(sym) << '(' << sym << ')' << ' '; 
            }
            gramFile << "\n";
         }
    }
    gramFile << '\n';
    // Close the file
    gramFile.close();
}
