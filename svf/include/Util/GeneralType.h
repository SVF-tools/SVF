//===- GeneralType.h -- Primitive types used in SVF--------------------------//
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
 * GeneralType.h
 *
 *  Created on: Feb 8, 2024
 *      Author: Jiawei Wang
 */

#pragma once
#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include "Util/SparseBitVector.h"

namespace SVF
{
typedef std::ostream OutStream;
typedef unsigned u32_t;
typedef signed s32_t;
typedef unsigned long long u64_t;
typedef signed long long s64_t;
typedef unsigned char u8_t;
typedef signed char s8_t;
typedef unsigned short u16_t;
typedef signed short s16_t;

typedef u32_t NodeID;
typedef u32_t EdgeID;
typedef unsigned CallSiteID;
typedef unsigned ThreadID;
typedef s64_t APOffset;

typedef SparseBitVector<> NodeBS;
typedef unsigned PointsToID;

/// provide extra hash function for std::pair handling
template <class T> struct Hash;

template <class S, class T> struct Hash<std::pair<S, T>>
{
    // Pairing function from: http://szudzik.com/ElegantPairing.pdf
    static size_t szudzik(size_t a, size_t b)
    {
        return a > b ? b * b + a : a * a + a + b;
    }

    size_t operator()(const std::pair<S, T>& t) const
    {
        Hash<decltype(t.first)> first;
        Hash<decltype(t.second)> second;
        return szudzik(first(t.first), second(t.second));
    }
};

template <class T> struct Hash
{
    size_t operator()(const T& t) const
    {
        std::hash<T> h;
        return h(t);
    }
};

template <typename Key, typename Hash = Hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
using Set = std::unordered_set<Key, Hash, KeyEqual, Allocator>;

template <typename Key, typename Value, typename Hash = Hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<const Key, Value>>>
                  using Map = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;

          template <typename Key, typename Compare = std::less<Key>,
                    typename Allocator = std::allocator<Key>>
          using OrderedSet = std::set<Key, Compare, Allocator>;

          template <typename Key, typename Value, typename Compare = std::less<Key>,
                    typename Allocator = std::allocator<std::pair<const Key, Value>>>
                            using OrderedMap = std::map<Key, Value, Compare, Allocator>;

                    typedef std::pair<NodeID, NodeID> NodePair;
                    typedef OrderedSet<NodeID> OrderedNodeSet;
                    typedef Set<NodeID> NodeSet;
                    typedef Set<NodePair> NodePairSet;
                    typedef Map<NodePair, NodeID> NodePairMap;
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
}