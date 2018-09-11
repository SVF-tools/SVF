//===- SCC.h -- SCC detection algorithm---------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2018>  <Yulei Sui>
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
 * ICFG.h
 *
 *  Created on: 11 Sep. 2018
 *      Author: Yulei
 */

#ifndef INCLUDE_UTIL_ICFG_H_
#define INCLUDE_UTIL_ICFG_H_

#include "MemoryModel/GenericGraph.h"
#include "Util/SVFModule.h"
#include <llvm/IR/Module.h>			// llvm module

class ICFGNode;
class PAG;

/*!
 * Every edge represents a control-flow transfer
 */
typedef GenericEdge<ICFGNode> GenericICFGEdgeTy;
class ICFGEdge : public GenericICFGEdgeTy {
public:
    typedef GenericNode<ICFGNode,ICFGEdge>::GEdgeSetTy ICFGEdgeSet;
public:
    typedef std::set<const llvm::Instruction*> CallInstSet;
    enum CEDGEK {
        CallEdge,RetEdge,IntraEdge
    };
};


/*!
 * Every node represents an instruction
 */
typedef GenericNode<ICFGNode,ICFGEdge> GenericICFGNodeTy;
class ICFGNode : public GenericICFGNodeTy {

public:
    typedef ICFGEdge::ICFGEdgeSet CallGraphEdgeSet;
    typedef ICFGEdge::ICFGEdgeSet::iterator iterator;
    typedef ICFGEdge::ICFGEdgeSet::const_iterator const_iterator;

private:
    const llvm::Function* inst;

public:
    /// Constructor
    ICFGNode(NodeID i, const llvm::Instruction* _inst) : GenericICFGNodeTy(i,0), inst(_inst) {

    }

    /// Get function of this call node
    inline const llvm::Instruction* getInst() const {
        return inst;
    }

};


typedef GenericGraph<ICFGNode,ICFGEdge> GenericICFGTy;
class ICFG : public GenericICFGTy {

public:
    /// Constructor
    ICFG(PAG* pag);

    /// Destructor
    virtual ~ICFG() {
    }

};


namespace llvm {
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<ICFGNode*> : public GraphTraits<GenericNode<ICFGNode,ICFGEdge>*  > {
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<ICFGNode *> > : public GraphTraits<Inverse<GenericNode<ICFGNode,ICFGEdge>* > > {
};

template<> struct GraphTraits<ICFG*> : public GraphTraits<GenericGraph<ICFGNode,ICFGEdge>* > {
    typedef ICFGNode *NodeRef;
};


}




#endif /* INCLUDE_UTIL_ICFG_H_ */
