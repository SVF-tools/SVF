//===----- CFGNormalizer.h -- Context Free Grammar Normalizer--------------//
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
    CFGNormalizer()
    {
    }

    /// Normalization with attribute expanded
    CFLGrammar* normalize(GrammarBase *generalGrammar);

    /// Fill Every attribute in CFL grammar
    CFLGrammar* fillAttribute(CFLGrammar *grammar, const Map<CFLGrammar::Kind, Set<CFLGrammar::Attribute>>& kind2AttrsMap);

private:
    /// Add nonterminal to tranfer long rules to binary rules
    void ebnf_bin(CFLGrammar *grammar);

    void ebnf_sign_replace(char sign, CFLGrammar *grammar);

    void insertToCFLGrammar(CFLGrammar *grammar, GrammarBase::Production &prod);

    int ebnf_bracket_match(GrammarBase::Production& prod, int i, CFLGrammar *grammar) ;

    int check_head(Map<GrammarBase::Symbol, GrammarBase::Productions>& grammar, GrammarBase::Production& rule);

    GrammarBase::Production strTrans(std::string strPro, CFLGrammar *grammar);

    GrammarBase::Production getFilledProd(GrammarBase::Production &prod, CFLGrammar::Attribute attribute, CFLGrammar *grammar);

    GrammarBase::Productions getFilledProductions(GrammarBase::Production &prod,const Map<CFLGrammar::Kind, Set<CFLGrammar::Attribute>>& kind2AttriMap, CFLGrammar *grammar);
};

} // End namespace SVF

#endif /* CFGNormalizer_H_*/