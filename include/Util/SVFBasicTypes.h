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

// TODO: these are just for SmallBBVector.
#include <llvm/IR/BasicBlock.h>
#include <llvm/ADT/SmallVector.h>

#include <llvm/Support/CommandLine.h>	// for command line options

#include <Util/SparseBitVector.h>

#include <iostream>
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

/// provide extra hash function for std::pair handling
template <class T> struct Hash;

template <class S, class T> struct Hash<std::pair<S, T>>
{
    // Pairing function from: http://szudzik.com/ElegantPairing.pdf
    static size_t szudzik(size_t a, size_t b)
    {
        return a > b ? b * b + a : a * a + a + b;
    }

    size_t operator()(const std::pair<S, T> &t) const
    {
        Hash<decltype(t.first)> first;
        Hash<decltype(t.second)> second;
        return szudzik(first(t.first), second(t.second));
    }
};

template <class T> struct Hash
{
    size_t operator()(const T &t) const
    {
        std::hash<T> h;
        return h(t);
    }
};

typedef std::ostream OutStream;

typedef unsigned u32_t;
typedef signed s32_t;
typedef unsigned long long u64_t;
typedef signed long long s64_t;

typedef u32_t NodeID;
typedef u32_t EdgeID;
typedef unsigned SymID;
typedef unsigned CallSiteID;
typedef unsigned ThreadID;

typedef SparseBitVector<> NodeBS;
//typedef SparseBitVector<> SparseBitVector;
//typedef NodeBS PointsTo;
class PointsTo;
typedef PointsTo AliasSet;
typedef unsigned PointsToID;

template <typename Key, typename Hash = Hash<Key>, typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
using Set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;

template<typename Key, typename Value, typename Hash = Hash<Key>,
         typename KeyEqual = std::equal_to<Key>,
         typename Allocator = std::allocator<std::pair<const Key, Value>>>
                 using Map = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

         template<typename Key, typename Compare = std::less<Key>, typename Allocator = std::allocator<Key>>
         using OrderedSet = std::set<Key, Compare, Allocator>;

         template<typename Key, typename Value, typename Compare = std::less<Key>,
                  typename Allocator = std::allocator<std::pair<const Key, Value>>>
                          using OrderedMap = std::map<Key, Value, Compare, Allocator>;

                  typedef llvm::SmallVector<llvm::BasicBlock*, 8> SmallBBVector;
                  typedef std::pair<NodeID, NodeID> NodePair;
                  typedef OrderedSet<NodeID> OrderedNodeSet;
                  typedef Set<NodeID> NodeSet;
                  typedef Set<NodePair> NodePairSet;
                  typedef Map<NodePair,NodeID> NodePairMap;
                  typedef std::vector<NodeID> NodeVector;
                  typedef std::vector<EdgeID> EdgeVector;
                  typedef std::stack<NodeID> NodeStack;
                  typedef std::list<NodeID> NodeList;
                  typedef std::deque<NodeID> NodeDeque;
                  typedef NodeSet EdgeSet;
                  typedef std::vector<u32_t> CallStrCxt;

                  typedef unsigned Version;
                  typedef Set<Version> VersionSet;
                  typedef std::pair<NodeID, Version> VersionedVar;
                  typedef Set<VersionedVar> VersionedVarSet;

// TODO: be explicit that this is a pair of 32-bit unsigneds?
                  template <> struct Hash<NodePair>
{
    size_t operator()(const NodePair &p) const
    {
        // Make sure our assumptions are sound: use u32_t
        // and u64_t. If NodeID is not actually u32_t or size_t
        // is not u64_t we should be fine since we get a
        // consistent result.
        uint32_t first = (uint32_t)(p.first);
        uint32_t second = (uint32_t)(p.second);
        return ((uint64_t)(first) << 32) | (uint64_t)(second);
    }
};


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

/// Size of native integer that we'll use for bit vectors, in bits.
#define NATIVE_INT_SIZE (sizeof(unsigned long long) * CHAR_BIT)

enum ModRefInfo
{
    ModRef,
    Ref,
    Mod,
    NoModRef,
};

enum AliasResult
{
    NoAlias,
    MayAlias,
    MustAlias,
    PartialAlias,
};

class SVFValue
{

public:
    typedef s64_t GNodeK;

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

    const std::string getName() const
    {
        return value;
    }

    const std::string& getValue() const
    {
        return value;
    }

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const SVFValue &node)
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


template <> struct std::hash<SVF::NodePair>
{
    size_t operator()(const SVF::NodePair &p) const
    {
        // Make sure our assumptions are sound: use u32_t
        // and u64_t. If NodeID is not actually u32_t or size_t
        // is not u64_t we should be fine since we get a
        // consistent result.
        uint32_t first = (uint32_t)(p.first);
        uint32_t second = (uint32_t)(p.second);
        return ((uint64_t)(first) << 32) | (uint64_t)(second);
    }
};

/// Specialise hash for SparseBitVectors.
template <unsigned N>
struct std::hash<SVF::SparseBitVector<N>>
{
    size_t operator()(const SVF::SparseBitVector<N> &sbv) const
    {
        SVF::Hash<std::pair<std::pair<size_t, size_t>, size_t>> h;
        return h(std::make_pair(std::make_pair(sbv.count(), sbv.find_first()), sbv.find_last()));
    }
};

template <typename T>
struct std::hash<std::vector<T>>
{
    size_t operator()(const std::vector<T> &v) const
    {
        // TODO: repetition with CBV.
        size_t h = v.size();

        SVF::Hash<T> hf;
        for (const T &t : v)
        {
            h ^= hf(t) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }
};

#endif /* INCLUDE_UTIL_SVFBASICTYPES_H_ */
