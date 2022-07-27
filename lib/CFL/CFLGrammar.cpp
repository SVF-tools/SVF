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
#include "Util/SVFUtil.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace SVF;


void GrammarBase::setRawProductions(SymbolMap<Symbol, Productions>& rawProductions)
{
    this->rawProductions = rawProductions;
}

void GrammarBase::setKind2AttrsMap(const Map<Kind,  Set<Attribute>>& kind2AttrsMap)
{
    this->kind2AttrsMap = kind2AttrsMap;
}

void GrammarBase::setAttributeKinds(const Set<Kind>& attributeKinds)
{
    this->attributeKinds = attributeKinds;
}

GrammarBase::Kind GrammarBase::str2Kind(std::string str) const
{

    auto tit = terminals.find(str);
    if(tit!=terminals.end())
        return tit->second;

    auto nit = nonterminals.find(str);
    if(nit!=nonterminals.end())
        return nit->second;

    assert(false && "kind not found!");
    abort();
}

GrammarBase::Symbol GrammarBase::str2Symbol(const std::string str) const
{
    Symbol symbol;
    std::string attributeStr = extractAttributeStrFromSymbolStr(str);
    std::string kindStr = extractKindStrFromSymbolStr(str);
    symbol.kind = str2Kind(kindStr);

    if ( attributeStr == "") return symbol;

    if ( (attributeStr.size() == 1) && (std::isalpha(attributeStr[attributeStr.size()-1])) )
    {
        symbol.variableAttribute = (u32_t)attributeStr[attributeStr.size()-1];
    }
    else
    {
        for( char &c : attributeStr)
        {
            if ( std::isdigit(c) == false )
            {
                SVFUtil::errs() << SVFUtil::errMsg("\t Symbol Attribute Parse Failure :") << str
                                << " Attribute:" << attributeStr << " (only number or single alphabet.)";
                assert(false && "grammar loading failed!");
            }
        }
        symbol.attribute = std::stoi(attributeStr);
    }
    return symbol;
}

std::string GrammarBase::kind2Str(Kind kind) const
{

    std::string key = "";
    for (auto &i : terminals)
    {
        if (i.second == kind)
        {
            key = i.first;
            return key;
        }
    }

    std::string nkey = "";
    for (auto &ni : nonterminals)
    {
        if (ni.second == kind)
        {
            nkey = ni.first;
            return nkey;
        }
    }

    return "";
}

std::string GrammarBase::sym2StrDump(Symbol sym) const
{
    Kind kind = sym.kind;
    Attribute attribute = sym.attribute;

    std::string key = "";
    for (auto &i : terminals)
    {
        if (i.second == kind)
        {
            key = i.first;
            if(attribute != 0)
            {
                key.append("_");
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
            if(attribute != 0)
            {
                nkey.append("_");
                nkey.append(std::to_string(attribute));
            }
            return nkey;
        }
    }

    return "";
}

GrammarBase::Kind GrammarBase::insertTerminalKind(std::string kindStr)
{
    Kind kind;
    if (terminals.find(kindStr) == terminals.end())
    {
        kind = totalKind++;
        terminals.insert({kindStr, kind});
    }
    else
    {
        kind = str2Kind(kindStr);
    }
    return kind;
}

inline GrammarBase::Kind GrammarBase::insertNonterminalKind(std::string const kindStr)
{
    Kind kind;
    if (nonterminals.find(kindStr) == nonterminals.end())
    {
        kind = totalKind++;
        nonterminals.insert({kindStr, kind});
    }
    else
    {
        kind = str2Kind(kindStr);
    }
    return kind;
}

std::string GrammarBase::extractKindStrFromSymbolStr(const std::string symbolStr) const
{
    std::string kindStr;
    // symbolStr end with '_', the whole symbolStr treat as kind, not with attribute.
    auto underscorePosition = symbolStr.find_last_of("_", symbolStr.size()-1);
    if (underscorePosition == std::string::npos)
    {
        return symbolStr;
    }
    return symbolStr.substr(0, underscorePosition);
}

std::string GrammarBase::extractAttributeStrFromSymbolStr(const std::string symbolStr) const
{
    std::string attributeStr;
    // symbolStr end with '_', the whole symbolStr treat as kind, not with attribute.
    auto underscorePosition = symbolStr.find_last_of("_", symbolStr.size()-1);
    if (underscorePosition == std::string::npos)
    {
        return "";
    }
    return symbolStr.substr(underscorePosition+1);
}

GrammarBase::Symbol GrammarBase::insertSymbol(std::string symbolStr)
{
    Symbol symbol;
    if (isupper(symbolStr[0]))
    {
        symbol = insertNonTerminalSymbol(symbolStr);
    }
    else
    {
        symbol.kind = insertTerminalKind(symbolStr);
    }
    return symbol;
}

GrammarBase::Symbol GrammarBase::insertNonTerminalSymbol(std::string symbolStr)
{
    Symbol symbol;
    std::string kindStr = extractKindStrFromSymbolStr(symbolStr);
    std::string attributeStr = extractAttributeStrFromSymbolStr(symbolStr);
    symbol.kind = insertNonterminalKind(kindStr);

    if ( attributeStr == "") return symbol;

    if ( (attributeStr.size() == 1) && (std::isalpha(attributeStr[attributeStr.size()-1])) )
    {
        attributeKinds.insert(symbol.kind);
        symbol.variableAttribute = (u32_t)attributeStr[attributeStr.size()-1];
    }
    else
    {
        for( char &c : attributeStr)
        {
            if ( std::isdigit(c) == false )
            {
                SVFUtil::errs() << SVFUtil::errMsg("\t Symbol Attribute Parse Failure :") << symbolStr
                                << " Attribute:" << attributeStr << " (only number or single alphabet.)";
                assert(false && "grammar loading failed!");
            }
        }
        attributeKinds.insert(symbol.kind);
        symbol.attribute = std::stoi(attributeStr);
    }
    return symbol;
}

void GrammarBase::insertAttribute(Kind kind, Attribute attribute)
{
    attributeKinds.insert(kind);
    if (kind2AttrsMap.find(kind)!= kind2AttrsMap.end())
    {
        kind2AttrsMap[kind].insert(attribute);
    }
    else
    {
        Set<CFLGrammar::Attribute> attrs {attribute};
        kind2AttrsMap.insert(make_pair(kind, attrs));
    }
}

CFLGrammar::CFLGrammar()
{
    newTerminalSubscript = 0;
}

void CFLGrammar::dump() const
{
    dump("Normailized_Grammar.txt");
}

void CFLGrammar::dump(std::string fileName) const
{
    std::ofstream gramFile(fileName);
    gramFile << "Start Kind:\n";
    gramFile << '\t' << kind2Str(startKind) << '(' << startKind << ')' << ' ' << "\n\n";

    gramFile << "Epsilon Production:\n";
    std::vector<std::string> strV;
    for (auto pro: this->epsilonProds)
    {
        std::stringstream ss;
        for (auto sym : pro)
        {
            if (sym == pro[1])
            {
                ss << "-> ";
            }
            ss << sym2StrDump(sym) << '(' << sym.kind << ')' << ' ';
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
            for (auto sym : pro)
            {
                i++;
                if (i == 2)
                {
                    ss << "-> ";
                }
                ss << sym2StrDump(sym) << '(' << sym.kind << ')' << ' ';
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
            for (auto sym : pro)
            {
                i++;

                if (i == 2)
                {
                    ss << "-> ";
                }
                ss << sym2StrDump(sym) << '(' << sym.kind << ')' << ' ';
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
