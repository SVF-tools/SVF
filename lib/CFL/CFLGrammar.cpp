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
#include <sstream>

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

std::string GrammarBase::sym2StrDump(Symbol sym) const
{
    Kind kind = getSymKind(sym);
    Attribute attribute = getSymAttribute(sym);

    std::string key = "";
    for (auto &i : terminals)
    {
        if (i.second == kind)
        {
            key = i.first;
            if(this->attributeSymbol.find(kind) != this->attributeSymbol.end())
            {
                key.pop_back();
                key.append(std::to_string(attribute));
            }
            return key;
        }
    }

    std::string nkey = "";
    for (auto &ni : nonterminals)
    {
        if (ni.second == kind)
        {
            nkey = ni.first;
            if(this->attributeSymbol.find(kind) != this->attributeSymbol.end())
            {
                nkey.pop_back();
                nkey.append(std::to_string(attribute));
            }
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
    if(strLit.size() >= 3)
    {
        if (strLit.compare(strLit.size()-2, 2, "_i")==0)
        {
        insertAttribute(sym);
        }
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
    std::vector<std::string> strV;
    for (auto pro: this->epsilonProds)
    {   
        std::stringstream ss;
        for (auto sym : pro){
            if (sym == pro[1])
            {
                ss << "-> ";
            }
            ss << sym2StrDump(sym) << '(' << sym << ')' << ' '; 
        }
        strV.insert(strV.begin(), ss.str());
    }
    std::sort(strV.begin(), strV.end());
    for (auto pro: strV)
    {
        gramFile << '\t';
        gramFile << pro;
        gramFile << '\n';
    }
    gramFile << '\n';

    gramFile << "Single Production:\n";
    strV = {};
    for (auto symProPair: singleRHS2Prods) 
    {
        for (auto pro: symProPair.second)
        {
            std::stringstream ss;
            int i = 0;
            for (auto sym : pro){
                i++;
                if (i == 2)
                {
                    ss << "-> ";
                }
                ss << sym2StrDump(sym) << '(' << sym << ')' << ' '; 
            }
            strV.insert(strV.begin(), ss.str());
         }
    }
    std::sort(strV.begin(), strV.end());
    for (auto pro: strV)
    {
        gramFile << '\t';
        gramFile << pro;
        gramFile << '\n';
    }
    gramFile << '\n';

    gramFile << "Binary Production:\n";
    strV = {};
    for (auto symProPair: firstRHS2Prods) 
    {
        for (auto pro: symProPair.second)
        {
            std::stringstream ss;
            int i = 0;
            for (auto sym : pro){
                i++;
                
                if (i == 2)
                {
                    ss << "-> ";
                }
                ss << sym2StrDump(sym) << '(' << sym << ')' << ' '; 
            }
            strV.insert(strV.begin(), ss.str());
         }
    }
    std::sort(strV.begin(), strV.end());
    for (auto pro: strV)
    {
        gramFile << '\t';
        gramFile << pro;
        gramFile << '\n';
    }
    gramFile << '\n';

    gramFile.close();
}
