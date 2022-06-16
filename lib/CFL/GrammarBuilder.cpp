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
const inline std::string GrammarBuilder::loadFileString() const
{
    std::ifstream textFile(fileName);
    std::string lineString;
    std::string lines = "";
    while (getline(textFile, lineString))
    {
        lines.append(lineString);
    }
    textFile.close();
    return lines;
}

const inline std::string GrammarBuilder::parseProduction() const
{
    std::regex reg("Start:([\\s\\S]*)Productions:([\\s\\S]*)");
    std::smatch matches;
    std::string startS = "";
    std::string lines = loadFileString();

    if (std::regex_search(lines, matches, reg))
    {
        lines = matches.str(2);
        startS = matches.str(1);
        startS = stripSpace(startS);
    }
    grammar->insertTerminalKind("epsilon");
    grammar->insertNonTerminalKind(startS);
    grammar->setStartKind(grammar->str2Kind(startS));
    return lines;
}

const inline std::vector<std::string> GrammarBuilder::loadWordProductions() const
{
    size_t pos = 0;
    std::string lines = parseProduction();
    std::string word = "";
    std::vector<std::string> wordProd;
    std::string delimiter = ";";
    while ((pos = lines.find(";")) != std::string::npos)
    {
        word = lines.substr(0, pos);
        wordProd.push_back(word);
        lines.erase(0, pos + delimiter.length());
    }
    return wordProd;
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
    std::string delimiter = ";";
    size_t pos = 0;
    std::string word = "";
    GrammarBase::Production prod;
    std::vector<std::string> wordProd = loadWordProductions();
    std::string delimiter1 = "->";

    for (auto it : wordProd)
    {
        if ((pos = it.find(delimiter1)) != std::string::npos)
        {
            std::string head = it.substr(0, pos);
            std::string LHS = it.substr(pos + delimiter1.size(), it.size() - 1);
            head = stripSpace(head);
            prod.push_back(grammar->insertNonTerminalKind(head));
            if (grammar->getRawProductions().find(grammar->str2Kind(head)) == grammar->getRawProductions().end())
            {
                grammar->getRawProductions().insert({grammar->str2Kind(head), {}});
            }

            std::regex LHSReg("\\s*(.*)");
            std::regex_search(LHS, matches, LHSReg);
            LHS = matches.str(1);
            delimiter = " ";
            while ((pos = LHS.find(delimiter)) != std::string::npos)
            {
                word = LHS.substr(0, pos);
                LHS.erase(0, pos + delimiter.length()); //Capital is Nonterminal, Otherwise is terminal
                if (isupper(word[0]))
                {
                    prod.push_back(grammar->insertNonTerminalKind(word));
                }
                else
                {
                    prod.push_back(grammar->insertTerminalKind(word));
                }
            }
            if (isupper(LHS[0]))
            {
                prod.push_back(grammar->insertNonTerminalKind(LHS));
            }
            else
            {
                prod.push_back(grammar->insertTerminalKind(LHS));
            }
            grammar->getRawProductions().at(grammar->str2Kind(head)).insert(prod);
            prod = {};
        }
    }

    return grammar;
};

GrammarBase* GrammarBuilder::build(Map<std::string, SVF::CFLGraph::Symbol> &preMap) const
{
    grammar->setNonterminals(preMap);
    grammar->setTotalKind(preMap.size());
    return build();
};
}