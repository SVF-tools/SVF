//===----- GrammarBuilder.cpp -- Grammar Builder--------------//
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
 *  Created on: April 27, 2022
 *      Author: Pei Xu
 */

#include <string>
#include <fstream>
#include <regex>
#include <sstream>
#include <iostream>
#include "CFL/GrammarBuilder.h"

namespace SVF
{
const inline std::string GrammarBuilder::parseProductionsString() const
{
    std::ifstream textFile(fileName);
    std::string lineString;
    std::string lines = "";
    std::string startString;
    int lineNum = 0;
    while (getline(textFile, lineString))
    {
        if(lineNum == 1)
        {
            startString = stripSpace(lineString);
        }
        lines.append(lineString);
        lineNum++;
    }

    std::regex reg("Start:([\\s\\S]*)Terminal:[\\s]*([\\s\\S]*)Productions:([\\s\\S]*)");
    std::smatch matches;
    if (std::regex_search(lines, matches, reg))
    {
        lines = matches.str(3);
    }
    std::string terminalString = matches.str(2);
    std::string symbolString;
    size_t pos;
    while ((pos = terminalString.find(" ")) != std::string::npos)
    {
        symbolString = stripSpace(terminalString.substr(0, pos));
        terminalString.erase(0, pos + 1); //Capital is Nonterminal, Otherwise is terminal
        grammar->insertSymbol(symbolString);
    }
    grammar->insertSymbol(symbolString);
    grammar->setStartKind(grammar->insertSymbol(startString));
    grammar->insertTerminalKind("epsilon");

    return lines;
}

const inline std::vector<std::string> GrammarBuilder::loadWordProductions() const
{
    size_t pos = 0;
    std::string lines = parseProductionsString();
    std::string word = "";
    std::vector<std::string> wordProds;
    std::string delimiter = ";";
    while ((pos = lines.find(";")) != std::string::npos)
    {
        word = lines.substr(0, pos);
        wordProds.push_back(word);
        lines.erase(0, pos + delimiter.length());
    }
    return wordProds;
}

const inline std::string GrammarBuilder::stripSpace(std::string s) const
{
    std::smatch matches;
    std::regex stripReg("\\s*(\\S*)\\s*");
    std::regex_search(s, matches, stripReg);
    return matches.str(1);
}


/// build grammarbase from textfile
GrammarBase* GrammarBuilder::build() const
{
    std::smatch matches;
    std::string delimiter = " ";
    std::string delimiter1 = "->";
    std::string word = "";
    size_t pos;
    GrammarBase::Production prod;
    std::vector<std::string> wordProdVec = loadWordProductions();

    for (auto wordProd : wordProdVec)
    {
        if ((pos = wordProd.find(delimiter1)) != std::string::npos)
        {
            std::string RHS = stripSpace(wordProd.substr(0, pos));
            std::string LHS = wordProd.substr(pos + delimiter1.size(), wordProd.size() - 1);
            GrammarBase::Symbol RHSSymbol = grammar->insertNonTerminalSymbol(RHS);
            prod.push_back(RHSSymbol);
            if (grammar->getRawProductions().find(RHSSymbol) == grammar->getRawProductions().end())  grammar->getRawProductions().insert({RHSSymbol, {}});
            std::regex LHSRegEx("\\s*(.*)");
            std::regex_search(LHS, matches, LHSRegEx);
            LHS = matches.str(1);
            while ((pos = LHS.find(delimiter)) != std::string::npos)
            {
                word = LHS.substr(0, pos);
                LHS.erase(0, pos + delimiter.length()); //Capital is Nonterminal, Otherwise is terminal
                prod.push_back(grammar->insertSymbol(word));
            }
            prod.push_back(grammar->insertSymbol(LHS));
            grammar->getRawProductions().at(RHSSymbol).insert(prod);
            prod = {};
        }
    }

    return grammar;
};

}