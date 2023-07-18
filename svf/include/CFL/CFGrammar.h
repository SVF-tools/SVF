//===----- CFGrammar.h -- Context-free grammar --------------------------//
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
 * CFGrammar.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */
#ifndef CFLGrammar_H_
#define CFLGrammar_H_
#include "SVFIR/SVFType.h"

namespace SVF
{

class GrammarBase
{
public:
    typedef u32_t Kind;
    typedef u32_t Attribute;
    typedef u32_t VariableAttribute;
    typedef struct Symbol
    {
        Kind kind: 8;
        Attribute attribute: 16;
        VariableAttribute variableAttribute: 8;

        /// Default Value for Symbol is 0.
        Symbol() : kind(0), attribute(0), variableAttribute(0) {}

        /// Contruct from u32_t move the bit to right field
        Symbol(const u32_t& num) : kind(num & 0xFF), attribute((num >> 8 ) & 0xFFFF), variableAttribute((num >> 24)) {}

        /// Conversion of u32_t
        operator u32_t()
        {
            static_assert(sizeof(struct Symbol)==sizeof(u32_t), "sizeof(struct Symbol)!=sizeof(u32_t)");
            u32_t num = 0;
            num += this->variableAttribute << 24;
            num += this->attribute << 8;
            num += this->kind;
            return num;
        }

        operator u32_t() const
        {
            static_assert(sizeof(struct Symbol)==sizeof(u32_t), "sizeof(struct Symbol)!=sizeof(u32_t)");
            u32_t num = 0;
            num += this->variableAttribute << 24;
            num += this->attribute << 8;
            num += this->kind;
            return num;
        }

        bool operator<(const Symbol& rhs)
        {
            return u32_t(*this) < u32_t(rhs);
        }

        void operator=(const u32_t& i)
        {
            this->kind = EdgeKindMask & i;
            this->attribute = i >> EdgeKindMaskBits;
            this->variableAttribute = i >> AttributedKindMaskBits;
        }

        void operator=(unsigned long long num)
        {
            this->kind = num & 0xFF;
            this->attribute = (num >> 8 ) & 0xFFFF;
            this->variableAttribute = num >> 24;
        }

        bool operator==(const Symbol& s)
        {
            return ((this->kind == s.kind) && (this->attribute == s.attribute) && (this->variableAttribute == s.variableAttribute));
        }

        bool operator==(const Symbol& s) const
        {
            return ((kind == s.kind) && (attribute == s.attribute) && (variableAttribute == s.variableAttribute));
        }

        bool operator!=(const Symbol& s) const
        {
            return ! (*this == s) ;
        }

        bool operator==(const u32_t& i)
        {
            return  u32_t(*this) == u32_t(i);
        }

        bool operator==(const Kind& k) const
        {
            return (this->kind == k);
        }
    } Symbol;

    class SymbolHash
    {
    public:
        size_t operator()(const Symbol &s) const
        {
            std::hash<u32_t> h;
            return h(u32_t(s));
        }
    };


    struct SymbolVectorHash
    {
        size_t operator()(const std::vector<Symbol> &v) const
        {
            size_t h = v.size();

            SymbolHash hf;
            for (const Symbol &t : v)
            {
                h ^= hf(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }

            return h;
        }
    };

    template<typename Key, typename Value, typename Hash = SymbolHash,
             typename KeyEqual = std::equal_to<Key>,
             typename Allocator = std::allocator<std::pair<const Key, Value>>>
                     using SymbolMap = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

             template <typename Key, typename Hash = SymbolVectorHash, typename KeyEqual = std::equal_to<Key>,
                       typename Allocator = std::allocator<Key>>
             using SymbolSet = std::unordered_set<Key, Hash, KeyEqual, Allocator>;

             typedef std::vector<Symbol> Production;
             typedef SymbolSet<Production> Productions;


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

    inline Map<std::string, Kind>& getEBNFSigns()
    {
        return this->EBNFSigns;
    }

    inline void setEBNFSigns(Map<std::string, Kind>& EBNFSigns)
    {
        this->EBNFSigns = EBNFSigns;
    }

    inline SymbolMap<Symbol, Productions>& getRawProductions()
    {
        return this->rawProductions;
    }

    inline const Map<Kind,  Set<Attribute>>& getKindToAttrsMap() const
    {
        return this->kindToAttrsMap;
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

    std::string extractKindStrFromSymbolStr(const std::string& symbolStr) const;

    std::string extractAttributeStrFromSymbolStr(const std::string& symbolStr) const;

    void setRawProductions(SymbolMap<Symbol, Productions>& rawProductions);

    void setKindToAttrsMap(const Map<Kind,  Set<Attribute>>& kindToAttrsMap);

    void setAttributeKinds(const Set<Kind>& attributeKind);

    Kind strToKind(std::string str) const;

    Symbol strToSymbol(const std::string str) const;

    std::string kindToStr(Kind kind) const;

    std::string symToStrDump(Symbol sym) const;

    Symbol getSymbol(const Production& prod, u32_t pos)
    {
        return prod.at(pos);
    }

    inline const Set<Kind>& getAttrSyms() const
    {
        return this->attributeKinds;
    }

    /// Insert kind to nonterminal and return kind.
    Kind insertNonterminalKind(std::string const kindStr);

    /// From SymbolStr extract kind to inserted in nonterminal
    /// symbolStr = <kindStr> [_] [ attributeStr | variableattributeStr ]
    Kind insertTerminalKind(std::string strLit);

    Symbol insertSymbol(std::string strLit);

    Symbol insertNonTerminalSymbol(std::string strLit);

    Symbol insertTerminalSymbol(std::string strLit);

    Symbol insertEBNFSigns(std::string strLit);

    void insertAttribute(Kind kind, Attribute a);

    inline static Kind getAttributedKind(Attribute attribute, Kind kind)
    {
        return ((attribute << EdgeKindMaskBits)| kind );
    }

    inline static Kind getVariabledKind(VariableAttribute variableAttribute, Kind kind)
    {
        return ((variableAttribute << AttributedKindMaskBits) | kind);
    }

protected:
    static constexpr unsigned char EdgeKindMaskBits = 8;  ///< We use the lower 8 bits to denote edge kind
    static constexpr unsigned char AttributedKindMaskBits = 24; ///< We use the lower 24 bits to denote attributed kind
    static constexpr u64_t EdgeKindMask = (~0ULL) >> (64 - EdgeKindMaskBits);
    Kind startKind;
private:
    Map<std::string, Kind> nonterminals;
    Map<std::string, Kind> terminals;
    Map<std::string, Kind> EBNFSigns; /// Map contains Signs' String and associated Symbols
    Set<Kind> attributeKinds;
    Map<Kind,  Set<Attribute>> kindToAttrsMap;
    SymbolMap<Symbol, Productions> rawProductions;
    u32_t totalKind;
};

class CFGrammar : public GrammarBase
{

public:
    CFGrammar();

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CFGrammar *)
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

    SymbolMap<Symbol, Productions>& getSingleRHSToProds()
    {
        return singleRHSToProds;
    }

    SymbolMap<Symbol, Productions>& getFirstRHSToProds()
    {
        return firstRHSToProds;
    }

    SymbolMap<Symbol, Productions>& getSecondRHSToProds()
    {
        return secondRHSToProds;
    }

    const bool hasProdsFromFirstRHS(const Symbol sym) const
    {
        auto it = firstRHSToProds.find(sym);
        return it!=firstRHSToProds.end();
    }

    const bool hasProdsFromSingleRHS(const Symbol sym) const
    {
        auto it = singleRHSToProds.find(sym);
        return it!=singleRHSToProds.end();
    }

    const bool hasProdsFromSecondRHS(const Symbol sym) const
    {
        auto it = secondRHSToProds.find(sym);
        return it!=secondRHSToProds.end();
    }

    const Productions& getProdsFromSingleRHS(const Symbol sym) const
    {
        auto it = singleRHSToProds.find(sym);
        assert(it!=singleRHSToProds.end() && "production (X -> sym) not found for sym!!");
        return it->second;
    }

    const Productions& getProdsFromFirstRHS(const Symbol sym) const
    {
        auto it = firstRHSToProds.find(sym);
        assert(it!=firstRHSToProds.end() && "production (X -> sym Y ) not found for sym!!");
        return it->second;
    }

    const Productions& getProdsFromSecondRHS(const Symbol sym) const
    {
        auto it = secondRHSToProds.find(sym);
        assert(it!=secondRHSToProds.end() && "production (X -> Y sym) not found for sym!!");
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

    void dump(std::string fileName) const;


    const inline u32_t num_generator()
    {
        return newTerminalSubscript++;
    }

private:
    SymbolSet<Production> epsilonProds;
    SymbolMap<Symbol, Productions> singleRHSToProds;
    SymbolMap<Symbol, Productions> firstRHSToProds;
    SymbolMap<Symbol, Productions> secondRHSToProds;
    u32_t newTerminalSubscript;
};

/**
 * Worklist with "first in first out" order.
 * New nodes will be pushed at back and popped from front.
 * Elements in the list are unique as they're recorded by Set.
 */
template<class Data>
class CFLFIFOWorkList
{
    typedef GrammarBase::SymbolSet<Data> DataSet;
    typedef std::deque<Data> DataDeque;
public:
    CFLFIFOWorkList() {}

    ~CFLFIFOWorkList() {}

    inline bool empty() const
    {
        return data_list.empty();
    }

    inline bool find(Data data) const
    {
        return (data_set.find(data) == data_set.end() ? false : true);
    }

    /**
     * Push a data into the work list.
     */
    inline bool push(Data data)
    {
        if (data_set.find(data) == data_set.end())
        {
            data_list.push_back(data);
            data_set.insert(data);
            return true;
        }
        else
            return false;
    }

    /**
     * Pop a data from the END of work list.
     */
    inline Data pop()
    {
        assert(!empty() && "work list is empty");
        Data data = data_list.front();
        data_list.pop_front();
        data_set.erase(data);
        return data;
    }

    /*!
     * Clear all the data
     */
    inline void clear()
    {
        data_list.clear();
        data_set.clear();
    }

private:
    DataSet data_set;	///< store all data in the work list.
    DataDeque data_list;	///< work list using std::vector.
};

}
#endif /* CFLGrammar_H_ */