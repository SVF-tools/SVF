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
 * CFGNormalizer.h
 *
 *  Created on: April 19 , 2022
 *      Author: Pei Xu
 */

#ifndef CFGNormalizer_H_
#define CFGNormalizer_H_

#include "CFLGrammar.h"

namespace SVF
{

/*!
 *  Normalize Grammar from a grammarbase
 */

class CFGNormalizer
{

public:
    typedef u32_t Symbol;
    typedef std::vector<Symbol> Production;
    typedef Set<Production> Productions;

    CFGNormalizer()
    {
    }

    /// Start Normalize (BIN Only)
    // TODO: Add different Combination Transformation Option

    /// Normalization without attribute expanded
    CFLGrammar* normalize(GrammarBase *generalGrammar);

    /// Fill Every attribute in CFL grammar
    CFLGrammar* fillAttribute(CFLGrammar *grammar, Map<CFLGrammar::Kind, Set<CFLGrammar::Attribute>> *kind2AttrMap);

private:
    /// Add nonterminal to tranfer long rules to binary rules
    void ebnf_bin(GrammarBase *generalGrammar, CFLGrammar *grammar);

    void ebnf_sign_replace(char sign, GrammarBase* generalGrammar, CFLGrammar *grammar);

    void insertToCFLGrammar(CFLGrammar *grammar, Production &prod);

    int ebnf_bracket_match(Production& prod, int i, CFLGrammar *grammar) ;

    int check_head(Map<Symbol, Productions>& grammar, Production& rule);

    Production strTrans(std::string strPro, CFLGrammar *grammar);

    Production getFilledProd(Production &prod, CFLGrammar::Attribute attribute, CFLGrammar *grammar);

    Productions getFilledProductions(Production &prod, Map<CFLGrammar::Kind,  Set<CFLGrammar::Attribute>> *kind2AttriMap, CFLGrammar *grammar);
};

} // End namespace SVF

#endif /* CFGNormalizer_H_*/