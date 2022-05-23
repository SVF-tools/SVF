//===----- CFGNormalizer.cpp -- CFL Alias Analysis Client--------------//
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

#include <string>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include "Util/BasicTypes.h"
#include "CFL/CFGNormalizer.h"
#include "Util/SVFUtil.h"

using namespace SVF;

/*
    Input: extended Backus–Naur form (EBNF)

    Usage	Notation
    definition	=
    concatenation	,
    termination	;
    alternation	|
    optional	[ ... ]
    repetition	{ ... }
    grouping	( ... )
    terminal string	" ... "
    terminal string	' ... '
    comment	(* ... *)
    special sequence	? ... ?
    exception	-

    Output: Backus-Naur form(BNF)

    <symbol> ::= __expression__

    Possible Step:

    Convert every repetition { E } to a fresh non-terminal X and add
    X = $\epsilon$ | X E.
    Convert every option [ E ] to a fresh non-terminal X and add
    X = $\epsilon$ | E.
    (We can convert X = A [ E ] B. to X = A E B | A B.)
    Convert every group ( E ) to a fresh non-terminal X and add
    X = E.
    We can even do away with alternatives by having several productions with the same non-terminal.
    X = E | E'. becomes X = E. X = E'.

    Only Consider BIN Transformation for BNF form

    And no differentiation between terminal and non-terminal
    1. Handwriting Testing for BIN Transformation Correctness

*/
// BNF
// BIN Transforom
CFLGrammar* CFGNormalizer::normalize(GrammarBase *generalGrammar)
{
    CFLGrammar *grammar = new CFLGrammar();
    grammar->startSymbol = generalGrammar->startSymbol;
    grammar->terminals = generalGrammar->terminals;
    grammar->nonterminals = generalGrammar->nonterminals;
    grammar->totalSymbol = generalGrammar->totalSymbol;
    grammar->attributeSymbol = generalGrammar->attributeSymbol;
    ebnf_sign_replace('*', generalGrammar, grammar);
    ebnf_sign_replace('?', generalGrammar, grammar);
    ebnf_bin(generalGrammar, grammar);
    grammar->rawProductions = generalGrammar->rawProductions;

    for(auto symProdsPair: generalGrammar->rawProductions)
    {
        for(auto prod: symProdsPair.second)
        {   
            /// rawProductions production does not include lhs
            /// so append to the begin of the production
            Production tempP = prod;
            tempP.insert(tempP.begin(), symProdsPair.first);
            insertToCFLGrammar(grammar, tempP);
        }
    }
    return grammar;
}


CFLGrammar* CFGNormalizer::normalize(GrammarBase *generalGrammar, Map<CFLGrammar::Kind, Set<CFLGrammar::Attribute>> *kind2AttrMap)
{
    CFLGrammar* grammar = normalize(generalGrammar);
    fillAttribute(grammar, kind2AttrMap);
    return grammar;
}

CFLGrammar* CFGNormalizer::fillAttribute(CFLGrammar *grammar, Map<CFLGrammar::Kind, Set<CFLGrammar::Attribute>> *kind2AttrMap)
{
    for(auto symProdsPair: grammar->rawProductions)
    {
        for(auto prod: symProdsPair.second)
        {
            /// rawProductions production does not include lhs
            /// so append to the begin of the production
            Production tempP = prod;
            tempP.insert(tempP.begin(), symProdsPair.first);
            Productions filledProductions =  getFilledProductions(tempP, kind2AttrMap, grammar);
            for (auto  filledProd : filledProductions)
            {
                insertToCFLGrammar(grammar, filledProd);
            }
        }
    }

    return grammar;
}

void CFGNormalizer::ebnf_bin(GrammarBase* generalGrammar, CFLGrammar *grammar)
{
    Map<Symbol, Productions> new_grammar = {};
    std::string tempStr = "";

    for(auto head : generalGrammar->rawProductions)
    {
        for(auto rule: head.second)
        {

            Production long_run = rule;
            long_run.erase(long_run.begin());
            auto it = generalGrammar->rawProductions[head.first].find(rule);
            generalGrammar->rawProductions[head.first].erase(it);
            generalGrammar->rawProductions[head.first].insert(long_run);
        }
    }

    for(auto head : generalGrammar->rawProductions)
    {
        for(auto rule: head.second)
        {
            if (rule.size() < 3) continue;

            Production long_run = rule;
            Symbol first = long_run[0];
            long_run.erase(long_run.begin());
            auto it = generalGrammar->rawProductions[head.first].find(rule);
            generalGrammar->rawProductions[head.first].erase(it);
            rule = {first};
            generalGrammar->rawProductions[head.first].insert(rule);

            Symbol X = check_head(new_grammar, long_run);
            if (int(X) == -1)
            {
                X = check_head(generalGrammar->rawProductions, long_run);
            }
            if (int(X) != -1)
            {
                it = generalGrammar->rawProductions[head.first].find(rule);
                generalGrammar->rawProductions[head.first].erase(it);
                rule.push_back(X);
                generalGrammar->rawProductions[head.first].insert(rule);
            }
            else
            {
                tempStr = "X";
                std::ostringstream ss;
                ss << grammar->num_generator();
                tempStr.append(ss.str());
                for (int i = 0; i < int(rule.size()); i++)
                {
                    if (grammar->attributeSymbol.find(rule[i]) != grammar->attributeSymbol.end())
                    {
                        tempStr.append("_i");
                        break;
                    }
                }
                Symbol tempSym = grammar->insertNonTerminalSymbol(tempStr);
                it = generalGrammar->rawProductions[head.first].find(rule);
                generalGrammar->rawProductions[head.first].erase(it);
                rule.push_back(tempSym);
                generalGrammar->rawProductions[head.first].insert(rule);
                X = tempSym;
            }
            new_grammar[X] = {};
            Production temp_p = long_run;
            Symbol RHX;
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
                Production prev_rule = long_run;
                long_run.erase(long_run.begin());

                X = RHX;
                temp_p = long_run;

                RHX = check_head(new_grammar, long_run);
                if (int(RHX) == -1)
                {
                    RHX = check_head(generalGrammar->rawProductions, long_run);
                }
                if(int(RHX) == -1)
                {
                    tempStr = "X";
                    std::ostringstream ss;
                    ss << grammar->num_generator();
                    tempStr.append(ss.str());
                    Symbol tempSym = grammar->insertNonTerminalSymbol(tempStr);
                    auto it = new_grammar[X].find(prev_rule);
                    new_grammar[X].erase(it);
                    new_grammar[X].insert({first, tempSym});
                    new_grammar[tempSym].insert(long_run);
                    RHX = tempSym;
                }
            }


            for (auto new_head : new_grammar)
            {
                for (auto prod : new_head.second)
                {
                    auto it = generalGrammar->rawProductions[new_head.first].find(prod);
                    if (it == generalGrammar->rawProductions[new_head.first].end())
                    {
                        generalGrammar->rawProductions[new_head.first].insert(prod);
                    }
                }
            }

        }
    }
}

CFGNormalizer::Production CFGNormalizer::getFilledProd(Production &prod, CFLGrammar::Attribute attribute, CFLGrammar *grammar)
{
    Production tempP = prod;
    for (int i = 0; i < int(prod.size()); i++)
    {
        if (grammar->attributeSymbol.find(prod[i]) != grammar->attributeSymbol.end())
        {
            tempP[i] = CFLGrammar::getAttributedKind(attribute, prod[i]);
        }
    }
    return tempP;
}

///Loop through provided production based on existence of attribute of attribute variable
///and expand to productions set
///e.g Xi -> Y Zi with Xi i = 0, 1, Yi i = 0,2
///Will get {X0 -> Y Z0, X1 -> Y Z1, X2 -> Y Z2}
CFGNormalizer::Productions CFGNormalizer::getFilledProductions(Production &prod, Map<CFLGrammar::Kind,  Set<CFLGrammar::Attribute>> *kind2AttriMap, CFLGrammar *grammar)
{
    Productions filledProductioins{};
    filledProductioins.insert(prod);
    for(Symbol variable : prod)
    {
        if (kind2AttriMap->find(variable) != kind2AttriMap->end())
        {
            auto nodeSet = *(kind2AttriMap->find(variable));
            for (auto attribute : nodeSet.second)
            {
                Production filledProd = getFilledProd(prod, attribute, grammar);
                if (filledProductioins.find(filledProd) == filledProductioins.end())
                {
                    filledProductioins.insert(filledProd);
                }
            }
        }
    }
    return filledProductioins;
}

int CFGNormalizer::ebnf_bracket_match(Production &prod, int i, CFLGrammar *grammar)
{
    int index = i;
    while (index >= 0)
    {
        if (grammar->sym2Str(prod[index]) == "(")
        {
            return index;
        }
        index--;
    }
    return 0;
}

void CFGNormalizer::ebnf_sign_replace(char sign, GrammarBase* generalGrammar, CFLGrammar *grammar)
{
    SVF::Map<std::string, std::string> new_rule_checker;
    std::string X = "X";
    
    /// replace Sign Group With Temp Varibale
    /// and load the replace in new_rule_checker
    for (auto &ebnfPair : generalGrammar->rawProductions)
    {
        Productions tempProds = ebnfPair.second;
        for (auto ebnfProd : ebnfPair.second)
        {
            size_t i = 1;
            while (i < ebnfProd.size())
            {
                int repetition_start = -1;
                if (grammar->sym2Str(ebnfProd[i]) == std::string(1, sign))
                {
                    assert(i != 1 && "sign in grammar associate with no symble");
                    /// If sign assoicate wihout group e.i with single symble
                    if (grammar->sym2Str(ebnfProd[i - 1]) != std::string(1, ')'))
                    {
                        repetition_start = i - 1;
                    }
                    /// sign associate with group of symble by brace pair
                    else
                    {
                        repetition_start = ebnf_bracket_match(ebnfProd, i, grammar);
                    }
                    std::string repetition = "";
                    for (size_t j = repetition_start; j < i; j++)
                    {
                        repetition.append(grammar->sym2Str(ebnfProd[j]));
                        repetition.append(" ");
                    }
                    repetition.append(grammar->sym2Str(ebnfProd[i]));
                    if (new_rule_checker.find(repetition) != new_rule_checker.end())
                    {
                        tempProds.erase(ebnfProd);
                        ebnfProd.erase(ebnfProd.begin() + repetition_start, ebnfProd.begin() + i + 1);
                        ebnfProd.insert(ebnfProd.begin() + repetition_start, grammar->str2Sym(new_rule_checker[repetition]));
                        tempProds.insert(ebnfProd);
                    }
                    else
                    {
                        X = "X";
                        std::ostringstream ss;
                        ss << grammar->num_generator();
                        X.append(ss.str());
                        Symbol tempSym = grammar->insertNonTerminalSymbol(X);
                        tempProds.erase(ebnfProd);
                        ebnfProd.erase(ebnfProd.begin() + repetition_start, ebnfProd.begin() + i + 1);
                        ebnfProd.insert(ebnfProd.begin() + repetition_start, tempSym);
                        new_rule_checker[repetition] = X;
                        tempProds.insert(ebnfProd);
                    }
                    
                    i = repetition_start;
                }
                i++;
            }
        }
        ebnfPair.second = tempProds;
    }
    for(auto rep: new_rule_checker)
    {
        /// For Both * and ? need to insert epsilon rule
        std::string new_nonterminal = rep.second;
        Production temp_list = {grammar->str2Sym(new_nonterminal), grammar->str2Sym("epsilon")};
        generalGrammar->rawProductions[grammar->str2Sym(new_nonterminal)].insert(temp_list);
        /// insert second rule for '*' X -> X E for '+' X -> X
        temp_list = {grammar->str2Sym(new_nonterminal)};
        if (sign == '*')
        {
            /// Insert Back the Group
            Production E = strTrans(rep.first, grammar);
            Production withoutSign = {};
            for (auto &word : E)
            {
                if (word != grammar->str2Sym("*")  && word != grammar->str2Sym("(") && word != grammar->str2Sym(")"))
                {
                    withoutSign.push_back(word);
                }
            }
            temp_list.insert(temp_list.end(), withoutSign.begin(), withoutSign.end());
        }
        generalGrammar->rawProductions[grammar->str2Sym(new_nonterminal)].insert(temp_list);
    }

}

CFGNormalizer::Production CFGNormalizer::strTrans(std::string LHS, CFLGrammar *grammar)
{
    Production  prod = {};
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
        prod.push_back(grammar->str2Sym(word));
    }
    prod.push_back(grammar->str2Sym(LHS));
    return prod;
}

int CFGNormalizer::check_head(Map<Symbol, Productions> &grammar, Production &rule)
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
    return -1;
}

/// Based on prod size to add on suitable member field of grammar
void CFGNormalizer::insertToCFLGrammar(CFLGrammar *grammar, CFGNormalizer::Production &prod)
{
    if (prod.size() == 2)
    {
        if ((std::find(prod.begin(), prod.end(), grammar->str2Sym("epsilon")) != prod.end()))
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
