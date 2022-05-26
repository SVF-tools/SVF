//===----- CFLGrammar.h -- Context-free grammar --------------------------//
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
 * CFLGrammar.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */
#ifndef CFLGrammar_H_
#define CFLGrammar_H_
#include "Util/BasicTypes.h"

namespace SVF
{
class GrammarBase
{
public:
    typedef u32_t Symbol;
    typedef std::vector<Symbol> Production;
    typedef Set<Production> Productions;
    typedef u32_t Attribute;
    typedef u32_t Kind;
    Map<std::string, Symbol> terminals;
    Map<std::string, Symbol> nonterminals;
    Symbol startSymbol;
    Symbol totalSymbol;
    Map<Symbol, Productions> rawProductions;
    Set<Symbol> attributeSymbol;
    Map<Kind,  Set<Attribute>> kind2AttrMap;

    Symbol str2Sym(std::string str) const;

    std::string sym2Str(Symbol sym) const;

    std::string sym2StrDump(Symbol sym) const;

    Symbol getSymbol(const Production& prod, u32_t pos)
    {
        return prod.at(pos);
    }

    inline Set<Symbol>& getAttrSyms()
    {
        return this->attributeSymbol;
    }

    Symbol insertTerminalSymbol(std::string strLit);
    Symbol insertNonTerminalSymbol(std::string strLit);
    void insertAttribute(Symbol s);

    inline static Kind getSymKind(Symbol sym)
    {
        return (EdgeKindMask & sym);
    }

    inline static Attribute getSymAttribute(Symbol sym)
    {
        return (sym >> EdgeKindMaskBits);
    }

    inline static Kind getAttributedKind(Attribute attribute, Kind kind)
    {
        return ((attribute << EdgeKindMaskBits)| kind );
    }

protected:
    static constexpr unsigned char EdgeKindMaskBits = 8;  ///< We use the lower 8 bits to denote edge kind
    static constexpr u64_t EdgeKindMask = (~0ULL) >> (64 - EdgeKindMaskBits);

};

class CFLGrammar : public GrammarBase
{

public:
    CFLGrammar();

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CFLGrammar *)
    {
        return true;
    }

    static inline bool classof(const GrammarBase *node)
    {
        return true;
    }

    Productions& getEpsilonProds()
    {
        return epsilonProds;
    }

    Map<Symbol, Productions>& getSingleRHS2Prods()
    {
        return singleRHS2Prods;
    }

    Map<Symbol, Productions>& getFirstRHS2Prods()
    {
        return firstRHS2Prods;
    }

    Map<Symbol, Productions>& getSecondRHS2Prods()
    {
        return secondRHS2Prods;
    }

    const bool hasProdsFromFirstRHS(Symbol sym) const
    {
        auto it = firstRHS2Prods.find(sym);
        return it!=firstRHS2Prods.end();
    }

    const bool hasProdsFromSingleRHS(Symbol sym) const
    {
        auto it = singleRHS2Prods.find(sym);
        return it!=singleRHS2Prods.end();
    }

    const bool hasProdsFromSecondRHS(Symbol sym) const
    {
        auto it = secondRHS2Prods.find(sym);
        return it!=secondRHS2Prods.end();
    }

    const Productions getProdsFromSingleRHS(Symbol sym) const
    {
        auto it = singleRHS2Prods.find(sym);
        assert(it!=singleRHS2Prods.end() && "production (X -> sym) not found for sym!!");
        return it->second;
    }

    const Productions getProdsFromFirstRHS(Symbol sym) const
    {
        auto it = firstRHS2Prods.find(sym);
        assert(it!=firstRHS2Prods.end() && "production (X -> sym Y ) not found for sym!!");
        return it->second;
    }

    const Productions getProdsFromSecondRHS(Symbol sym) const
    {
        auto it = secondRHS2Prods.find(sym);
        assert(it!=secondRHS2Prods.end() && "production (X -> Y sym) not found for sym!!");
        return it->second;
    }


    const Symbol getLHSSymbol(const Production& prod)
    {
        return prod.at(0);
    }

    const Symbol getFirstRHSSymbol(const Production& prod)
    {
        return prod.at(1);
    }

    const Symbol getSecondRHSSymbol(const Production& prod)
    {
        return prod.at(2);
    }

    void dump();

    const inline int num_generator()
    {
        return newTerminalSubscript++;
    }

private:
    Set<Production> epsilonProds;
    Map<Symbol, Productions> singleRHS2Prods;
    Map<Symbol, Productions> firstRHS2Prods;
    Map<Symbol, Productions> secondRHS2Prods;
    Symbol newTerminalSubscript;
};

}
#endif /* CFLGrammar_H_ */