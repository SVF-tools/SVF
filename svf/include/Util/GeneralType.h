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
#include <stack>
#include <vector>
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
