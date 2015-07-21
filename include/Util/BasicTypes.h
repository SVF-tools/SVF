/* SVF - Static Value-Flow Analysis Framework
Copyright (C) 2015 Yulei Sui
Copyright (C) 2015 Jingling Xue

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * BasicTypes.h
 *
 *  Created on: Apr 1, 2014
 *      Author: Yulei Sui
 */

#ifndef BASICTYPES_H_
#define BASICTYPES_H_

#include <llvm/ADT/SmallVector.h>		// for small vector
#include <llvm/ADT/DenseSet.h>		// for dense map, set
#include <llvm/ADT/SparseBitVector.h>	// for points-to
#include <vector>
#include <list>
#include <set>
#include <map>
#include <stack>
#include <deque>

typedef unsigned NodeID;
typedef unsigned EdgeID;
typedef unsigned SymID;
typedef unsigned CallSiteID;
typedef unsigned ThreadID;
typedef unsigned u32_t;
typedef unsigned long long u64_t;
typedef signed s32_t;
typedef signed long Size_t;

typedef llvm::SparseBitVector<> PointsTo;
typedef PointsTo NodeBS;
typedef PointsTo AliasSet;

typedef std::pair<NodeID, NodeID> NodePair;
typedef std::set<NodeID> NodeSet;
typedef llvm::DenseSet<NodePair,llvm::DenseMapInfo<std::pair<NodeID,NodeID> > > NodePairSet;
typedef llvm::DenseMap<NodePair,NodeID,llvm::DenseMapInfo<std::pair<NodeID,NodeID> > > NodePairMap;
typedef std::vector<NodeID> NodeVector;
typedef std::vector<EdgeID> EdgeVector;
typedef std::stack<NodeID> NodeStack;
typedef std::list<NodeID> NodeList;
typedef std::deque<NodeID> NodeDeque;
typedef llvm::SmallVector<u32_t,16> SmallVector16;
typedef llvm::SmallVector<u32_t,8> SmallVector8;
typedef NodeSet EdgeSet;


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

/*
 * Number of clock ticks per second. A clock tick is the unit by which
 * processor time is measured and is returned by 'clock'.
 */
#define TIMEINTERVAL 1000
#define CLOCK_IN_MS() (clock() / (CLOCKS_PER_SEC / TIMEINTERVAL))

class BddCond;

#endif /* BASICTYPES_H_ */
