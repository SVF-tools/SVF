//===----- CFGNormalizer.h -- Context Free Grammar Normalizer--------------//
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
 * CFGNormalizer.h
 *
 *  Created on: April 19 , 2022
 *      Author: Pei Xu
 */

#ifndef INCLUDE_CFL_CFGNORMALIZER_H_
#define INCLUDE_CFL_CFGNORMALIZER_H_

#include "CFGrammar.h"

namespace SVF
{

/*!
 *  Generate Normalized Grammar (backus naur form) from a grammarbase (Extended extended Backus–Naur form )
 *
 *  To Do:
 *      Error Notice for ill formed production,
 *      e.g. not end with ';' and '*' not preceding with '()' and extra space before ';'
 *      '|' sign support
 */

class CFGNormalizer
{

public:
    CFGNormalizer()
    {
    }

    /// Binary Normal Form(BNF) normalization with variable attribute expanded
    CFGrammar* normalize(GrammarBase *generalGrammar);

    /// Expand every variable attribute in rawProductions of grammarbase
    CFGrammar* fillAttribute(CFGrammar *grammar, const Map<CFGrammar::Kind, Set<CFGrammar::Attribute>>& kindToAttrsMap);

private:
    /// Add nonterminal to tranfer long rules to binary rules
    void ebnf_bin(CFGrammar *grammar);

    void ebnfSignReplace(char sign, CFGrammar *grammar);

    void barReplace(CFGrammar *grammar);

    void insertToCFLGrammar(CFGrammar *grammar, GrammarBase::Production &prod);

    int ebnfBracketMatch(GrammarBase::Production& prod, int i, CFGrammar *grammar) ;

    GrammarBase::Symbol check_head(GrammarBase::SymbolMap<GrammarBase::Symbol, GrammarBase::Productions>& grammar, GrammarBase::Production& rule);

    void strTrans(std::string strPro, CFGrammar *grammar, GrammarBase::Production& normalProd);

    void getFilledProductions(GrammarBase::Production &prod,const NodeSet& nodeSet, CFGrammar *grammar, GrammarBase::Productions& normalProds);

    void removeFirstSymbol(CFGrammar *grammar);
};

} // End namespace SVF

#endif /* INCLUDE_CFL_CFGNORMALIZER_H_*/