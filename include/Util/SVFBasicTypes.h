//===- SVFBasicTypes.h -- Basic types used in SVF-------------------------------//
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
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */


#ifndef INCLUDE_UTIL_SVFBASICTYPES_H_
#define INCLUDE_UTIL_SVFBASICTYPES_H_

#include <llvm/ADT/SparseBitVector.h>	// for points-to
#include <llvm/Support/raw_ostream.h>	// for output
#include <llvm/Support/CommandLine.h>	// for command line options
#include <llvm/ADT/StringMap.h>	// for StringMap

#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <stack>
#include <deque>

namespace SVF
{

typedef unsigned u32_t;
typedef unsigned long long u64_t;
typedef signed s32_t;
typedef signed long Size_t;

typedef u32_t NodeID;
typedef u32_t EdgeID;
typedef unsigned SymID;
typedef unsigned CallSiteID;
typedef unsigned ThreadID;
typedef unsigned Version;

typedef llvm::SparseBitVector<> NodeBS;
typedef NodeBS PointsTo;
typedef PointsTo AliasSet;

template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>> 
using Set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;

template<typename Key, typename Value, typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename Allocator = std::allocator<std::pair<const Key, Value>>>
using Map = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

template<typename Key, typename Compare = std::less<Key>, typename Allocator = std::allocator<Key>>
using OrderedSet = std::set<Key, Compare, Allocator>;

template<typename Key, typename Value, typename Compare = std::less<Key>,
         typename Allocator = std::allocator<std::pair<const Key, Value>>>
using OrderedMap = std::map<Key, Value, Compare, Allocator>;

template <typename T, unsigned N>
using SmallVector = llvm::SmallVector<T, N>;

typedef std::pair<NodeID, NodeID> NodePair;
typedef std::pair<NodeID, Version> VersionedVar;
typedef OrderedSet<NodeID> OrderedNodeSet;
typedef Set<NodeID> NodeSet;
typedef Set<NodePair> NodePairSet;
typedef Map<NodePair,NodeID> NodePairMap;
typedef std::vector<NodeID> NodeVector;
typedef std::vector<EdgeID> EdgeVector;
typedef std::stack<NodeID> NodeStack;
typedef std::list<NodeID> NodeList;
typedef std::deque<NodeID> NodeDeque;
typedef SmallVector<u32_t,16> SmallVector16;
typedef SmallVector<u32_t,8> SmallVector8;
typedef NodeSet EdgeSet;
typedef SmallVector16 CallStrCxt;
typedef llvm::StringMap<u32_t> StringMap;

/// LLVM debug macros, define type of your DEBUG model of each pass
#define DBOUT(TYPE, X) 	DEBUG_WITH_TYPE(TYPE, X)
#define DOSTAT(X) 	X
#define DOTIMESTAT(X) 	X

/// General debug flag is for each phase of a pass, it is often in a colorful output format
#define DGENERAL "general"

#define DPAGBuild "pag"
#define DMemModel "mm"
#define DMemModelCE "mmce"
#define DCOMModel "comm"
#define DDDA "dda"
#define DDumpPT "dumppt"
#define DRefinePT "sbpt"
#define DCache "cache"
#define DWPA "wpa"
#define DMSSA "mssa"
#define DInstrument "ins"
#define DAndersen "ander"
#define DSaber "saber"
#define DMTA "mta"
#define DCHA "cha"

/*
 * Number of clock ticks per second. A clock tick is the unit by which
 * processor time is measured and is returned by 'clock'.
 */
#define TIMEINTERVAL 1000
#define CLOCK_IN_MS() (clock() / (CLOCKS_PER_SEC / TIMEINTERVAL))

class SVFValue
{

public:
    typedef s32_t GNodeK;

    enum SVFValKind
    {
        SVFVal,
        SVFFunc,
        SVFGlob,
        SVFBB,
        SVFInst,
    };

private:
    const std::string value;
    GNodeK kind;	///< Type of this SVFValue
public:
    /// Constructor
    SVFValue(const std::string& val, SVFValKind k): value(val), kind(k)
    {
    }

    /// Get the type of this SVFValue
    inline GNodeK getKind() const
    {
        return kind;
    }

    /// Add the hash function for std::set (we also can overload operator< to implement this)
    //  and duplicated elements in the set are not inserted (binary tree comparison)
    //@{
    bool operator()(const SVFValue* lhs, const SVFValue* rhs) const
    {
        return lhs->value < rhs->value;
    }

    inline bool operator==(SVFValue* rhs) const
    {
        return value == rhs->value;
    }

    inline bool operator!=(SVFValue* rhs) const
    {
        return value != rhs->value;
    }
    //@}

    const llvm::StringRef getName() const
    {
        return value;
    }

    const std::string& getValue() const
    {
        return value;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend llvm::raw_ostream& operator<< (llvm::raw_ostream &o, const SVFValue &node)
    {
        o << node.getName();
        return o;
    }
    //@}

    static inline bool classof(const SVFValue *node)
    {
        return node->getKind() == SVFValue::SVFVal ||
               node->getKind() == SVFValue::SVFFunc ||
               node->getKind() == SVFValue::SVFGlob ||
               node->getKind() == SVFValue::SVFBB ||
               node->getKind() == SVFValue::SVFInst;
    }
};



} // End namespace SVF

/// Specialise hash for pairs.
template <typename T, typename U> struct std::hash<std::pair<T, U>> {
    // Pairing function from: http://szudzik.com/ElegantPairing.pdf
    static size_t szudzik(size_t a, size_t b)
    {
        return a > b ? b * b + a : a * a + a + b;
    }

    size_t operator()(const std::pair<T, U> &p) const {
        std::hash<T> h1;
        std::hash<U> h2;
        return szudzik(h1(p.first), h2(p.second));
    }
};

/// Specialise hash for SmallVectors.
template <typename T, unsigned N>
struct std::hash<SVF::SmallVector<T, N>>
{
    size_t operator()(const SVF::SmallVector<T, N> &sv) const {
        if (sv.empty()) return 0;
        if (sv.size() == 1) return sv[0];

        // Iterate and accumulate the hash.
        size_t hash = 0;
        std::hash<std::pair<T, size_t>> hts;
        std::hash<T> ht;
        for (const T &t : sv)
        {
            hash = hts(std::make_pair(ht(t), hash));
        }

        return hash;
    }
};

#endif /* INCLUDE_UTIL_SVFBASICTYPES_H_ */
