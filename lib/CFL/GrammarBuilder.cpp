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
 *  Created on: April 27, 2022
 *      Author: Pei Xu
 */

#include <string>
#include <fstream>
#include <regex>
#include <sstream>
#include <iostream>
#include "CFL/GrammarBuilder.h"

namespace SVF{
    GrammarBase* GrammarBuilder::build(){
        grammar->insertTerminalSymbol("epsilon");

        std::ifstream textFile(fileName);
        std::string lineString;
        std::string lines = "";
        std::regex reg("Start:([\\s\\S]*)Productions:([\\s\\S]*)");
        std::string startS = "";
        std::regex stripReg("\\s*(\\S*)\\s*");
        std::vector<std::string> wordProd;

        while (getline(textFile, lineString))
        {
            lines.append(lineString);
        }
        textFile.close();
        std::smatch matches;
        GrammarBase::Production prod;
        if (std::regex_search(lines, matches, reg))
        {
            lines = matches.str(2);
            startS = matches.str(1);
            std::regex_search(startS, matches, stripReg);
            startS = matches.str(1);
        }
        size_t pos = 0;

        std::string delimiter = ";";
        std::string word = "";
        while ((pos = lines.find(";")) != std::string::npos)
        {
            word = lines.substr(0, pos);
            wordProd.push_back(word);
            lines.erase(0, pos + delimiter.length());
        }
        std::string delimiter1 = "->";

        for (auto it : wordProd)
        {
            if ((pos = it.find(delimiter1)) != std::string::npos)
            {
                std::string head = it.substr(0, pos);
                std::string LHS = it.substr(pos + delimiter1.size(), it.size() - 1);
                std::regex_search(head, matches, stripReg);
                head = matches.str(1);// Capital is Non-terminal
                if (grammar->nonterminals.find(head) == grammar->nonterminals.end())
                {
                    grammar->nonterminals.insert({head, grammar->totalSymbol});
                    prod.push_back(grammar->totalSymbol++);
                }
                else
                {
                    prod.push_back(grammar->str2Sym(head));
                }
                if (grammar->rawProductions.find(grammar->str2Sym(head)) == grammar->rawProductions.end())
                {
                    grammar->rawProductions.insert({grammar->str2Sym(head), {}});
                }

                std::regex LHSReg("\\s*(.*)");
                std::regex_search(LHS, matches, LHSReg);
                LHS = matches.str(1);
                delimiter = " ";
                while ((pos = LHS.find(delimiter)) != std::string::npos)
                {
                    word = LHS.substr(0, pos);
                    LHS.erase(0, pos + delimiter.length());//Capital is Nonterminal, Otherwise is terminal
                    if (isupper(word[0])){
                        if (grammar->nonterminals.find(word) == grammar->nonterminals.end())
                        {
                            grammar->nonterminals.insert({word, grammar->totalSymbol});
                            prod.push_back(grammar->totalSymbol++);
                        }
                        else
                            prod.push_back(grammar->str2Sym(word));
                    }
                    else {
                        if (grammar->terminals.find(word) == grammar->terminals.end())
                        {
                            grammar->terminals.insert({word, grammar->totalSymbol});
                            prod.push_back(grammar->totalSymbol++);
                        }
                        else
                            prod.push_back(grammar->str2Sym(word));
                    }
                }
                if (isupper(LHS[0])){
                    if (grammar->nonterminals.find(LHS) == grammar->nonterminals.end())
                    {
                        grammar->nonterminals.insert({LHS, grammar->totalSymbol});
                        prod.push_back(grammar->totalSymbol++);
                    }
                    else
                        prod.push_back(grammar->str2Sym(LHS));
                }
                else {
                    if (grammar->terminals.find(LHS) == grammar->terminals.end())
                    {
                        grammar->terminals.insert({LHS, grammar->totalSymbol});
                        prod.push_back(grammar->totalSymbol++);
                    }
                    else
                        prod.push_back(grammar->str2Sym(LHS));
                }
                grammar->rawProductions[grammar->str2Sym(head)].insert(prod);
                prod = {};
            }
        }
        grammar->startSymbol = grammar->str2Sym(startS); 
        return grammar; 
    }; 

GrammarBase* GrammarBuilder::build(Map<std::string, SVF::CFLGraph::Symbol> *preMap){
    grammar->nonterminals = *preMap;
    grammar->totalSymbol = preMap->size();
    return build();
};
}