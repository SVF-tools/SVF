//===----- CFGNormalizer.cpp -- Context Free Grammar Normalizer--------------//
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
 * CFGNormalizer.cpp
 *
 *  Created on: April 19 , 2022
 *      Author: Pei Xu
 */

#include "CFL/CFGNormalizer.h"
#include "Util/SVFUtil.h"
#include "Util/WorkList.h"
#include "SVFIR/SVFValue.h"
#include <string>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>

using namespace SVF;

CFGrammar* CFGNormalizer::normalize(GrammarBase *generalGrammar)
{
    CFGrammar *grammar = new CFGrammar();
    grammar->setStartKind(generalGrammar->getStartKind());
    grammar->setTerminals(generalGrammar->getTerminals());
    grammar->setNonterminals(generalGrammar->getNonterminals());
    grammar->setEBNFSigns(generalGrammar->getEBNFSigns());
    grammar->setTotalKind(generalGrammar->getTotalKind());
    grammar->setAttributeKinds(generalGrammar->getAttrSyms());
    grammar->setKindToAttrsMap(generalGrammar->getKindToAttrsMap());
    grammar->setRawProductions(generalGrammar->getRawProductions());
    barReplace(grammar);
    ebnfSignReplace('*', grammar);
    ebnfSignReplace('?', grammar);
    ebnf_bin(grammar);
    fillAttribute(grammar, grammar->getKindToAttrsMap());
    return grammar;
}


CFGrammar* CFGNormalizer::fillAttribute(CFGrammar *grammar, const Map<CFGrammar::Kind, Set<CFGrammar::Attribute>>& kindToAttrsMap)
{
    NodeSet nodeSet = {};
    for (auto pair: kindToAttrsMap)
    {
        for (auto attri: pair.second)
        {
            nodeSet.insert(attri);
        }
    }
    for(auto symProdsPair: grammar->getRawProductions())
    {
        for(auto prod: symProdsPair.second)
        {
            /// rawProductions production does not include lhs
            /// so append to the begin of the production
            GrammarBase::Production tempP = prod;
            tempP.insert(tempP.begin(), symProdsPair.first);
            GrammarBase::Productions normalProds;
            getFilledProductions(tempP, nodeSet, grammar, normalProds);
            for (auto  filledProd : normalProds)
            {
                insertToCFLGrammar(grammar, filledProd);
            }
        }
    }

    return grammar;
}

void CFGNormalizer::ebnf_bin(CFGrammar *grammar)
{
    GrammarBase::SymbolMap<GrammarBase::Symbol, GrammarBase::Productions> new_grammar = {};
    std::string tempStr = "";
    removeFirstSymbol(grammar);

    auto rawProductions = grammar->getRawProductions();

    for(auto itr : rawProductions)
    {
        auto head = *(grammar->getRawProductions().find(itr.first));
        for(auto rule: head.second)
        {
            if (rule.size() < 3) continue;

            GrammarBase::Symbol first = rule[0];
            GrammarBase::Production long_run(rule.begin() + 1, rule.end());
            auto it = grammar->getRawProductions()[head.first].find(rule);
            grammar->getRawProductions()[head.first].erase(it);
            GrammarBase::Symbol X = check_head(new_grammar, long_run);
            if (X == u32_t(-1))
            {
                X = check_head(grammar->getRawProductions(), long_run);
            }
            if ((X == u32_t(-1)) == false)
            {
                rule = {first, X};
                grammar->getRawProductions()[head.first].insert(rule);
            }
            else
            {
                tempStr = "X";
                std::ostringstream ss;
                ss << grammar->num_generator();
                tempStr.append(ss.str());
                /// Assign _attribute
                /// if target portion of the production contain more than 1 variable then
                /// X add no variable attribute
                /// if target only contain one variable attribute X share the same variable attribute
                Set<GrammarBase::VariableAttribute> variableAttributeSet = {};
                for (unsigned i = 0; i < long_run.size(); i++)
                {
                    GrammarBase::VariableAttribute variableAttribute = long_run[i].variableAttribute;
                    if ( variableAttribute != 0)
                    {
                        variableAttributeSet.insert(variableAttribute);
                    }
                }
                if ( variableAttributeSet.size() == 1)
                {
                    tempStr += "_";
                    tempStr += char(*variableAttributeSet.begin());
                }
                GrammarBase::Symbol tempSym = grammar->insertNonTerminalSymbol(tempStr);
                rule = {first, tempSym};
                grammar->getRawProductions()[head.first].insert(rule);
                X = tempSym;
            }
            new_grammar[X] = {};
            GrammarBase::Production temp_p = long_run;
            GrammarBase::Symbol RHX;
            if (long_run.size() ==2)
            {
                new_grammar[X].insert(temp_p);
                long_run.clear();
            }
            else
            {
                new_grammar[X].insert(long_run);
                RHX = X;
            }
            while (long_run.size() > 2)
            {
                first = long_run[0];
                GrammarBase::Production prev_rule = long_run;
                long_run.erase(long_run.begin());

                X = RHX;
                temp_p = long_run;

                RHX = check_head(new_grammar, long_run);
                if (RHX == u32_t(-1))
                {
                    RHX = check_head(grammar->getRawProductions(), long_run);
                }
                if(RHX == u32_t(-1))
                {
                    tempStr = "X";
                    std::ostringstream ss;
                    ss << grammar->num_generator();
                    tempStr.append(ss.str());
                    Set<GrammarBase::VariableAttribute> variableAttributeSet = {};
                    for (unsigned i = 0; i < long_run.size(); i++)
                    {
                        GrammarBase::VariableAttribute variableAttribute = long_run[i].variableAttribute;
                        if ( variableAttribute != 0)
                        {
                            variableAttributeSet.insert(variableAttribute);
                        }
                    }
                    if ( variableAttributeSet.size() == 1)
                    {
                        tempStr += "_";
                        tempStr += char(*variableAttributeSet.begin());
                    }
                    GrammarBase::Symbol tempSym = grammar->insertNonTerminalSymbol(tempStr);
                    auto it = new_grammar[X].find(prev_rule);
                    new_grammar[X].erase(it);
                    new_grammar[X].insert({first, tempSym});
                    new_grammar[tempSym].insert(long_run);
                    RHX = tempSym;
                }
            }
        }
    }
    for (auto new_head : new_grammar)
    {
        for (auto prod : new_head.second)
        {
            auto it = grammar->getRawProductions()[new_head.first].find(prod);
            if (it == grammar->getRawProductions()[new_head.first].end())
            {
                grammar->getRawProductions()[new_head.first].insert(prod);
            }
        }
    }
}

///Loop through provided production based on existence of attribute of attribute variable
///and expand to productions set
///e.g Xi -> Y Zi with Xi i = 0, 1, Yi i = 0,2
///Will get {X0 -> Y Z0, X1 -> Y Z1, X2 -> Y Z2}
void CFGNormalizer::getFilledProductions(GrammarBase::Production &prod, const NodeSet& nodeSet, CFGrammar *grammar,  GrammarBase::Productions& normalProds)
{
    normalProds.clear();
    CFLFIFOWorkList<GrammarBase::Production> worklist;
    worklist.push(prod);
    while( worklist.empty() == false )
    {
        GrammarBase::Production currentProduction = worklist.pop();
        /// Get the first encounter variable attribute to expand
        GrammarBase::VariableAttribute currentVariableAttribute = 0;
        // GrammarBase::Kind baseKind;
        for ( GrammarBase::Symbol &symbol : currentProduction )
        {
            if ( currentVariableAttribute == 0 )
            {
                currentVariableAttribute = symbol.variableAttribute;
                // baseKind = symbol.kind;
            }
        }
        if ( currentVariableAttribute == 0)
        {
            normalProds.insert(currentProduction);
            continue;
        }
        //*(kindToAttriMap.find(baseKind));
        //for (auto attribute : nodeSet.second)
        for (auto attribute : nodeSet)
        {
            GrammarBase::Production fillingProduction = currentProduction;
            for ( GrammarBase::Symbol &symbol : fillingProduction )
            {
                if ( symbol.variableAttribute == currentVariableAttribute)
                {
                    symbol.attribute = attribute;
                    symbol.variableAttribute = 0;
                }
            }
            /// Check whether all symbol expanded
            bool continueToFill = false;
            for ( GrammarBase::Symbol &symbol : fillingProduction )
            {
                if ( symbol.variableAttribute != 0 )
                {
                    continueToFill = true;
                }
            }
            if ( continueToFill == false)
            {
                normalProds.insert(fillingProduction);
            }
            else
            {
                worklist.push(fillingProduction);
            }
        }
    }
}

int CFGNormalizer::ebnfBracketMatch(GrammarBase::Production &prod, int i, CFGrammar *grammar)
{
    int index = i;
    while (index >= 0)
    {
        if (grammar->kindToStr(prod[index].kind) == "(")
        {
            return index;
        }
        index--;
    }
    return 0;
}

void CFGNormalizer::barReplace(CFGrammar *grammar)
{
    for (auto &symbolToProductionsPair : grammar->getRawProductions())
    {
        GrammarBase::Productions productions;
        //GrammarBase::Productions Originalproductions = symbolToProductionsPair.second;
        for (auto ebnfProduction : symbolToProductionsPair.second)
        {
            size_t i = 1;
            size_t j = 1;
            while (i < ebnfProduction.size())
            {
                if (grammar->kindToStr(ebnfProduction[i].kind) == "|")
                {
                    GrammarBase::Production tempPro(ebnfProduction.begin()+j, ebnfProduction.begin()+i);
                    tempPro.insert(tempPro.begin(), symbolToProductionsPair.first );
                    productions.insert(tempPro);
                    j = i+1;
                }
                i++;
            }
            GrammarBase::Production tempPro(ebnfProduction.begin()+j, ebnfProduction.begin()+i);
            tempPro.insert(tempPro.begin(), symbolToProductionsPair.first );
            productions.insert(tempPro);
        }
        symbolToProductionsPair.second.clear();
        symbolToProductionsPair.second = productions;
    }
}

void CFGNormalizer::ebnfSignReplace(char sign, CFGrammar *grammar)
{
    /// Replace Sign Group With tempNonterminal 'X'
    /// And load the replace in newProductions
    SVF::Map<std::string, std::string> newProductions;
    std::string tempNonterminal = "X";

    for (auto &symbolToProductionsPair : grammar->getRawProductions())
    {
        GrammarBase::Productions productions = symbolToProductionsPair.second;
        for (auto ebnfProduction : symbolToProductionsPair.second)
        {
            size_t i = 1;
            while (i < ebnfProduction.size())
            {
                s32_t signGroupStart = -1;
                if (grammar->kindToStr(ebnfProduction[i].kind) == std::string(1, sign))
                {
                    /// If sign assoicate wihout group e.i with single symble
                    assert(i != 1 && "sign in grammar associate with no symbol");
                    if (grammar->kindToStr(ebnfProduction[i - 1].kind) != std::string(1, ')'))
                    {
                        signGroupStart = i - 1;
                    }
                    /// sign associate with group of symble by brace pair
                    else
                    {
                        signGroupStart = ebnfBracketMatch(ebnfProduction, i, grammar);
                    }
                    std::string groupString = "";
                    for (size_t j = signGroupStart; j < i; j++)
                    {
                        groupString.append(grammar->kindToStr(ebnfProduction[j].kind));
                        groupString.append(" ");
                    }
                    groupString.append(grammar->kindToStr(ebnfProduction[i].kind));
                    if (newProductions.find(groupString) != newProductions.end())
                    {
                        productions.erase(ebnfProduction);
                        ebnfProduction.erase(ebnfProduction.begin() + signGroupStart, ebnfProduction.begin() + i + 1);
                        ebnfProduction.insert(ebnfProduction.begin() + signGroupStart, grammar->strToSymbol(newProductions[groupString]));
                        productions.insert(ebnfProduction);
                    }
                    else if ( (signGroupStart == 1) && (i == ebnfProduction.size() -1))
                    {
                        newProductions[groupString] = grammar->kindToStr(ebnfProduction[0].kind);
                        productions.erase(ebnfProduction);
                        ebnfProduction.erase(ebnfProduction.begin() + signGroupStart, ebnfProduction.begin() + i + 1);

                    }
                    else
                    {
                        tempNonterminal = "X";
                        std::ostringstream ss;
                        ss << grammar->num_generator();
                        tempNonterminal.append(ss.str());
                        GrammarBase::Symbol tempSym = grammar->insertNonTerminalSymbol(tempNonterminal);
                        productions.erase(ebnfProduction);
                        ebnfProduction.erase(ebnfProduction.begin() + signGroupStart, ebnfProduction.begin() + i + 1);
                        ebnfProduction.insert(ebnfProduction.begin() + signGroupStart, tempSym);
                        newProductions[groupString] = tempNonterminal;
                        productions.insert(ebnfProduction);
                    }

                    i = signGroupStart;
                }
                i++;
            }
        }
        symbolToProductionsPair.second = productions;
    }
    for(auto rep: newProductions)
    {
        /// For Both * and ? need to insert epsilon rule
        std::string new_nonterminal = rep.second;
        GrammarBase::Production temp_list = {grammar->strToSymbol(new_nonterminal), grammar->strToSymbol("epsilon")};
        grammar->getRawProductions()[grammar->strToSymbol(new_nonterminal)].insert(temp_list);
        /// insert second rule for '*' X -> X E for '+' X -> E
        temp_list = {grammar->strToSymbol(new_nonterminal)};
        if (sign == '*' || sign == '?')
        {
            /// Insert Back the Group
            GrammarBase::Production normalProd;
            strTrans(rep.first, grammar, normalProd);
            GrammarBase::Production withoutSign = {};
            if (sign == '*')
            {
                for (auto &word : normalProd)
                {
                    if (word != grammar->strToSymbol("*")  && word != grammar->strToSymbol("(") && word != grammar->strToSymbol(")"))
                    {
                        withoutSign.push_back(word);
                    }
                }
                withoutSign.push_back(grammar->strToSymbol(rep.second));
            }
            if (sign == '?')
            {
                for (auto &word : normalProd)
                {
                    if (word != grammar->strToSymbol("?")  && word != grammar->strToSymbol("(") && word != grammar->strToSymbol(")"))
                    {
                        withoutSign.push_back(word);
                    }
                }
            }
            temp_list.insert(temp_list.end(), withoutSign.begin(), withoutSign.end());
        }
        grammar->getRawProductions()[grammar->strToSymbol(new_nonterminal)].insert(temp_list);
    }
}

void CFGNormalizer::strTrans(std::string LHS, CFGrammar *grammar, GrammarBase::Production& normalProd)
{
    std::smatch matches;
    std::regex LHSReg("\\s*(.*)");
    std::string delimiter;
    size_t pos;
    std::string word;
    std::regex_search(LHS, matches, LHSReg);
    LHS = matches.str(1);
    delimiter = " ";
    while ((pos = LHS.find(delimiter)) != std::string::npos)
    {
        word = LHS.substr(0, pos);
        LHS.erase(0, pos + delimiter.length());
        normalProd.push_back(grammar->strToSymbol(word));
    }
    normalProd.push_back(grammar->strToSymbol(LHS));
}

GrammarBase::Symbol CFGNormalizer::check_head(GrammarBase::SymbolMap<GrammarBase::Symbol, GrammarBase::Productions> &grammar, GrammarBase::Production &rule)
{
    for(auto symProdPair: grammar)
    {
        for(auto prod: symProdPair.second)
        {
            if (rule == prod)
            {
                return symProdPair.first;
            }
        }
    }
    GrammarBase::Symbol symbol = u32_t(-1);
    return symbol;
}

/// Based on prod size to add on suitable member field of grammar
void CFGNormalizer::insertToCFLGrammar(CFGrammar *grammar, GrammarBase::Production &prod)
{
    if (prod.size() == 2)
    {
        if ((std::find(prod.begin(), prod.end(), grammar->strToKind("epsilon")) != prod.end()))
        {
            if (std::find(grammar->getEpsilonProds().begin(), grammar->getEpsilonProds().end(), prod) == grammar->getEpsilonProds().end())
            {
                grammar->getEpsilonProds().insert(prod);
            }
        }
        else
        {
            grammar->getSingleRHSToProds()[prod[1]].insert(prod);
        }
    }
    if (prod.size() == 3)
    {
        grammar->getFirstRHSToProds()[prod[1]].insert(prod);
        grammar->getSecondRHSToProds()[prod[2]].insert(prod);
    }
}

void CFGNormalizer::removeFirstSymbol(CFGrammar *grammar)
{
    // Remove First Terminal
    for(auto head : grammar->getRawProductions())
    {
        for(auto rule: head.second)
        {

            GrammarBase::Production long_run = rule;
            long_run.erase(long_run.begin());
            auto it = grammar->getRawProductions().at(head.first).find(rule);
            grammar->getRawProductions().at(head.first).erase(it);
            grammar->getRawProductions()[head.first].insert(long_run);
        }
    }
}
