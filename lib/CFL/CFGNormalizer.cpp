//===----- CFGNormalizer.cpp -- Context Free Grammar Normalizer--------------//
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
 * CFGNormalizer.cpp
 *
 *  Created on: April 19 , 2022
 *      Author: Pei Xu
 */

#include "CFL/CFGNormalizer.h"
#include "Util/SVFUtil.h"
#include "Util/WorkList.h"
#include "Util/BasicTypes.h"
#include <string>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>

using namespace SVF;

CFLGrammar* CFGNormalizer::normalize(GrammarBase *generalGrammar)
{
    CFLGrammar *grammar = new CFLGrammar();
    grammar->setStartKind(generalGrammar->getStartKind());
    grammar->setTerminals(generalGrammar->getTerminals());
    grammar->setNonterminals(generalGrammar->getNonterminals());
    grammar->setTotalKind(generalGrammar->getTotalKind());
    grammar->setAttributeKinds(generalGrammar->getAttrSyms());
    grammar->setKind2AttrsMap(generalGrammar->getKind2AttrsMap());
    grammar->setRawProductions(generalGrammar->getRawProductions());
    ebnfSignReplace('*', grammar);
    ebnfSignReplace('?', grammar);
    ebnf_bin(grammar);
    fillAttribute(grammar, grammar->getKind2AttrsMap());
    return grammar;
}


CFLGrammar* CFGNormalizer::fillAttribute(CFLGrammar *grammar, const Map<CFLGrammar::Kind, Set<CFLGrammar::Attribute>>& kind2AttrsMap)
{
    for(auto symProdsPair: grammar->getRawProductions())
    {
        for(auto prod: symProdsPair.second)
        {
            /// rawProductions production does not include lhs
            /// so append to the begin of the production
            GrammarBase::Production tempP = prod;
            tempP.insert(tempP.begin(), symProdsPair.first);
            GrammarBase::Productions filledProductions =  getFilledProductions(tempP, kind2AttrsMap, grammar);
            for (auto  filledProd : filledProductions)
            {
                insertToCFLGrammar(grammar, filledProd);
            }
        }
    }

    return grammar;
}

void CFGNormalizer::ebnf_bin(CFLGrammar *grammar)
{
    GrammarBase::SymbolMap<GrammarBase::Symbol, GrammarBase::Productions> new_grammar = {};
    std::string tempStr = "";

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

    auto rawProductions = grammar->getRawProductions();

    for(auto itr : rawProductions)
    {
        auto head = *(grammar->getRawProductions().find(itr.first));
        for(auto rule: head.second)
        {
            if (rule.size() < 3) continue;

            GrammarBase::Production long_run = rule;
            GrammarBase::Symbol first = long_run[0];
            long_run.erase(long_run.begin());
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

GrammarBase::Production CFGNormalizer::getFilledProd(GrammarBase::Production &prod, CFLGrammar::Attribute attribute, CFLGrammar *grammar)
{
    GrammarBase::Production tempP = prod;
    for (int i = 0; i < int(prod.size()); i++)
    {
        if (grammar->getAttrSyms().find(prod[i].kind) != grammar->getAttrSyms().end())
        {
            tempP[i] = prod[i].kind;
        }
    }
    return tempP;
}

///Loop through provided production based on existence of attribute of attribute variable
///and expand to productions set
///e.g Xi -> Y Zi with Xi i = 0, 1, Yi i = 0,2
///Will get {X0 -> Y Z0, X1 -> Y Z1, X2 -> Y Z2}
GrammarBase::Productions CFGNormalizer::getFilledProductions(GrammarBase::Production &prod, const Map<CFLGrammar::Kind,  Set<CFLGrammar::Attribute>>& kind2AttriMap, CFLGrammar *grammar)
{
    GrammarBase::Productions filledProductioins{};
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
            filledProductioins.insert(currentProduction);
            continue;
        }
        auto nodeSet = {0, 1, 2, 3, 4, 5, 6, 7, 20};                 //*(kind2AttriMap.find(baseKind));
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
                filledProductioins.insert(fillingProduction);
            }
            else
            {
                worklist.push(fillingProduction);
            }
        }
    }
    return filledProductioins;
}

int CFGNormalizer::ebnfBracketMatch(GrammarBase::Production &prod, int i, CFLGrammar *grammar)
{
    int index = i;
    while (index >= 0)
    {
        if (grammar->kind2Str(prod[index].kind) == "(")
        {
            return index;
        }
        index--;
    }
    return 0;
}

void CFGNormalizer::ebnfSignReplace(char sign, CFLGrammar *grammar)
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
                if (grammar->kind2Str(ebnfProduction[i].kind) == std::string(1, sign))
                {
                    /// If sign assoicate wihout group e.i with single symble
                    assert(i != 1 && "sign in grammar associate with no symbol");
                    if (grammar->kind2Str(ebnfProduction[i - 1].kind) != std::string(1, ')'))
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
                        groupString.append(grammar->kind2Str(ebnfProduction[j].kind));
                        groupString.append(" ");
                    }
                    groupString.append(grammar->kind2Str(ebnfProduction[i].kind));
                    if (newProductions.find(groupString) != newProductions.end())
                    {
                        productions.erase(ebnfProduction);
                        ebnfProduction.erase(ebnfProduction.begin() + signGroupStart, ebnfProduction.begin() + i + 1);
                        ebnfProduction.insert(ebnfProduction.begin() + signGroupStart, grammar->str2Symbol(newProductions[groupString]));
                        productions.insert(ebnfProduction);
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
        GrammarBase::Production temp_list = {grammar->str2Symbol(new_nonterminal), grammar->str2Symbol("epsilon")};
        grammar->getRawProductions()[grammar->str2Symbol(new_nonterminal)].insert(temp_list);
        /// insert second rule for '*' X -> X E for '+' X -> X
        temp_list = {grammar->str2Symbol(new_nonterminal)};
        if (sign == '*')
        {
            /// Insert Back the Group
            GrammarBase::Production E = strTrans(rep.first, grammar);
            GrammarBase::Production withoutSign = {};
            for (auto &word : E)
            {
                if (word != grammar->str2Symbol("*")  && word != grammar->str2Symbol("(") && word != grammar->str2Symbol(")"))
                {
                    withoutSign.push_back(word);
                }
            }
            temp_list.insert(temp_list.end(), withoutSign.begin(), withoutSign.end());
        }
        grammar->getRawProductions()[grammar->str2Symbol(new_nonterminal)].insert(temp_list);
    }

}

GrammarBase::Production CFGNormalizer::strTrans(std::string LHS, CFLGrammar *grammar)
{
    GrammarBase::Production  prod = {};
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
        prod.push_back(grammar->str2Symbol(word));
    }
    prod.push_back(grammar->str2Symbol(LHS));
    return prod;
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
void CFGNormalizer::insertToCFLGrammar(CFLGrammar *grammar, GrammarBase::Production &prod)
{
    if (prod.size() == 2)
    {
        if ((std::find(prod.begin(), prod.end(), grammar->str2Kind("epsilon")) != prod.end()))
        {
            if (std::find(grammar->getEpsilonProds().begin(), grammar->getEpsilonProds().end(), prod) == grammar->getEpsilonProds().end())
            {
                grammar->getEpsilonProds().insert(prod);
            }
        }
        else
        {
            grammar->getSingleRHS2Prods()[prod[1]].insert(prod);
        }
    }
    if (prod.size() == 3)
    {
        grammar->getFirstRHS2Prods()[prod[1]].insert(prod);
        grammar->getSecondRHS2Prods()[prod[2]].insert(prod);
    }
}
