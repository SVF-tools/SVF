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

    inline Map<std::string, Kind>& getNonterminals()
    {
        return this->nonterminals;
    }

    inline void setNonterminals(Map<std::string, Kind>& nonterminals)
    {
        this->nonterminals = nonterminals;
    }

    inline Map<std::string, Kind>& getTerminals()
    {
        return this->terminals;
    }

    inline void setTerminals(Map<std::string, Kind>& terminals)
    {
        this->terminals = terminals;
    }

    inline Map<Symbol, Productions>& getRawProductions()
    {
        return this->rawProductions;
    }

    inline const Map<Kind,  Set<Attribute>>& getKind2AttrsMap() const
    {
        return this->kind2AttrsMap;
    }

    inline Kind getTotalKind()
    {
        return this->totalKind;
    }

    inline Kind getStartKind()
    {
        return this->startKind;
    }

    inline void setStartKind(Kind startKind)
    {
        this->startKind = startKind;
    }

    inline void setTotalKind(Kind totalKind)
    {
        this->totalKind = totalKind;
    }

    void setRawProductions(Map<Symbol, Productions>& rawProductions);

    void setKind2AttrsMap(const Map<Kind,  Set<Attribute>>& kind2AttrsMap);

    void setAttributeKinds(const Set<Kind>& attributeKind);

    Kind str2Kind(std::string str) const;

    std::string kind2Str(Kind kind) const;

    std::string sym2StrDump(Symbol sym) const;

    Symbol getSymbol(const Production& prod, u32_t pos)
    {
        return prod.at(pos);
    }

    inline const Set<Symbol>& getAttrSyms() const
    {
        return this->attributeKinds;
    }

    Kind insertTerminalKind(std::string strLit);
    Kind insertNonTerminalKind(std::string strLit);
    void insertAttribute(Kind kind, Attribute a);

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
    Kind startKind;
private:
    Set<Kind> attributeKinds;
    Map<Kind,  Set<Attribute>> kind2AttrsMap;
    Map<Symbol, Productions> rawProductions;
    Kind totalKind;
    Map<std::string, Kind> nonterminals;
    Map<std::string, Kind> terminals;
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

    const bool hasProdsFromFirstRHS(const Symbol sym) const
    {
        auto it = firstRHS2Prods.find(sym);
        return it!=firstRHS2Prods.end();
    }

    const bool hasProdsFromSingleRHS(const Symbol sym) const
    {
        auto it = singleRHS2Prods.find(sym);
        return it!=singleRHS2Prods.end();
    }

    const bool hasProdsFromSecondRHS(const Symbol sym) const
    {
        auto it = secondRHS2Prods.find(sym);
        return it!=secondRHS2Prods.end();
    }

    const Productions& getProdsFromSingleRHS(const Symbol sym) const
    {
        auto it = singleRHS2Prods.find(sym);
        assert(it!=singleRHS2Prods.end() && "production (X -> sym) not found for sym!!");
        return it->second;
    }

    const Productions& getProdsFromFirstRHS(const Symbol sym) const
    {
        auto it = firstRHS2Prods.find(sym);
        assert(it!=firstRHS2Prods.end() && "production (X -> sym Y ) not found for sym!!");
        return it->second;
    }

    const Productions& getProdsFromSecondRHS(const Symbol sym) const
    {
        auto it = secondRHS2Prods.find(sym);
        assert(it!=secondRHS2Prods.end() && "production (X -> Y sym) not found for sym!!");
        return it->second;
    }


    const Symbol& getLHSSymbol(const Production& prod) const
    {
        return prod.at(0);
    }

    const Symbol& getFirstRHSSymbol(const Production& prod) const
    {
        return prod.at(1);
    }

    const Symbol& getSecondRHSSymbol(const Production& prod) const
    {
        return prod.at(2);
    }

    void dump() const;

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