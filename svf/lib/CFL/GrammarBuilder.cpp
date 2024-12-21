//===----- GrammarBuilder.cpp -- Grammar Builder--------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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
#include <sstream>
#include <iostream>
#include "CFL/GrammarBuilder.h"

namespace SVF
{
const inline std::string GrammarBuilder::parseProductionsString() const
{
    std::ifstream textFile(fileName);
    if (!textFile.is_open())
    {
        std::cerr << "Can't open CFL grammar file `" << fileName << "`" << std::endl;
        abort();
    }

    std::string lineString;
    std::string lines = "";
    std::string startString;
    std::string symbolString;
    const std::string WHITESPACE = " \n\r\t\f\v";
    int lineNum = 0;

    while (getline(textFile, lineString))
    {
        if (lineNum == 1)
        {
            startString = stripSpace(lineString);
        }
        else if (lineNum == 3)
        {
            // Trim leading and trailing whitespace
            size_t start = lineString.find_first_not_of(WHITESPACE);
            size_t end = lineString.find_last_not_of(WHITESPACE);
            if (start != std::string::npos && end != std::string::npos)
            {
                symbolString = lineString.substr(start, end - start + 1);
            }
        }

        // Append line to `lines` with whitespace trimmed
        size_t start = lineString.find_first_not_of(WHITESPACE);
        size_t end = lineString.find_last_not_of(WHITESPACE);
        if (start != std::string::npos && end != std::string::npos)
        {
            lines.append(lineString.substr(start, end - start + 1));
        }

        lineNum++;
    }

    // Extract "Productions:" part from `lines` manually
    size_t productionsPos = lines.find("Productions:");
    if (productionsPos != std::string::npos)
    {
        lines = lines.substr(productionsPos + std::string("Productions:").length());
    }

    // Parse `symbolString` to insert symbols
    std::string sString;
    size_t pos = 0;
    while ((pos = symbolString.find(" ")) != std::string::npos)
    {
        sString = stripSpace(symbolString.substr(0, pos));
        symbolString.erase(0, pos + 1); // Remove the processed part
        grammar->insertSymbol(sString);
    }
    // Insert the remaining symbol
    grammar->insertSymbol(symbolString);

    // Set the start kind and add the epsilon terminal
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
    // Remove leading spaces
    size_t start = s.find_first_not_of(" ");
    if (start == std::string::npos)
    {
        return ""; // Return an empty string if no non-space character is found
    }

    // Remove trailing spaces
    size_t end = s.find_last_not_of(" ");
    return s.substr(start, end - start + 1);
}

/// Build grammarbase from textfile
GrammarBase* GrammarBuilder::build() const
{
    std::string delimiter = " ";
    std::string delimiter1 = "->";
    std::string word = "";
    size_t pos;
    GrammarBase::Production prod;
    std::vector<std::string> wordProdVec = loadWordProductions();

    for (auto wordProd : wordProdVec)
    {
        // Find the position of the '->' delimiter
        if ((pos = wordProd.find(delimiter1)) != std::string::npos)
        {
            // Extract and strip RHS (right-hand side) and LHS (left-hand side)
            std::string RHS = stripSpace(wordProd.substr(0, pos));
            std::string LHS = stripSpace(wordProd.substr(pos + delimiter1.size()));

            // Insert RHS symbol into grammar
            GrammarBase::Symbol RHSSymbol = grammar->insertSymbol(RHS);
            prod.push_back(RHSSymbol);

            // Ensure RHS symbol exists in raw productions
            if (grammar->getRawProductions().find(RHSSymbol) == grammar->getRawProductions().end())
            {
                grammar->getRawProductions().insert({RHSSymbol, {}});
            }

            // Parse LHS string into symbols
            while ((pos = LHS.find(delimiter)) != std::string::npos)
            {
                // Extract each word before the space
                word = stripSpace(LHS.substr(0, pos));
                LHS.erase(0, pos + delimiter.length());

                // Insert symbol into production
                prod.push_back(grammar->insertSymbol(word));
            }

            // Insert the remaining word (if any) into the production
            if (!LHS.empty())
            {
                prod.push_back(grammar->insertSymbol(stripSpace(LHS)));
            }

            // Add the production to raw productions
            grammar->getRawProductions().at(RHSSymbol).insert(prod);

            // Clear the production for the next iteration
            prod = {};
        }
    }

    return grammar;
}


}