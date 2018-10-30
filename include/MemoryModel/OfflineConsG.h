//===- OfflineConsG.h -- Offline constraint graph -----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * OfflineConsG.h
 *
 *  Created on: Oct 28, 2018
 *      Author: Yuxiang Lei
 */

#ifndef OFFLINECONSG_H
#define OFFLINECONSG_H

#include "MemoryModel/ConsG.h"
#include "Util/SCC.h"

/*!
 * Offline constraint graph for Andersen's analysis.
 * In OCG, a 'ref' node is used to represent the point-to set of a constraint node.
 * 'Nor' means a constraint node of its corresponding ref node.
 */
class OfflineConsG: public ConstraintGraph{
public:
    typedef SCCDetection<OfflineConsG*> OSCC;

protected:
    NodeSet refNodes;
    NodeToRepMap nodeToRefMap;  // a --> *a
    NodeToRepMap norToRepMap;   // for each *a construct a --> rep, i.e., mapping a node of to a rep node for online constraint solving
    OSCC* oscc;

public:
    OfflineConsG(PAG *p) : ConstraintGraph(p),
                           nodeToRefMap({}), norToRepMap({}) {
        buildOfflineCG();
    }

    // Determine whether a node has a OCG rep node
    inline const bool hasOCGRep(NodeID node) const {
        return hasNorRep(node);
    }
    // Get a node's OCG rep node
    inline NodeID getOCGRep(NodeID node) {
        return getNorRep(node);
    }

    // Determine whether a node is a ref node
    inline const bool isaRef(NodeID node) const {
        NodeSet::const_iterator it = refNodes.find(node);
        return it != refNodes.end();
    };
    // Determine whether a node has ref nodes
    inline const bool hasRef(NodeID node) const {
        NodeToRepMap::const_iterator it = nodeToRefMap.find(node);
        return it != nodeToRefMap.end();
    };
    // Use a constraint node to track its corresponding ref node
    inline NodeID getRef(NodeID node) {
        NodeToRepMap::const_iterator it = nodeToRefMap.find(node);
        assert(it != nodeToRefMap.end() && "No such ref node in ref to node map!");
        return it->second;
    }

    // Constraint solver of offline constraint graph
    //{@
    void solveOCG();
    void offlineSCCDetect();
    void buildOfflineMap();
    //@}

    // Dump graph into dot file
    void dump(std::string name);

protected:
    inline bool hasNorRep(NodeID nor) const {
        NodeToRepMap::const_iterator it = norToRepMap.find(nor);
        return it != norToRepMap.end();
    };

    inline void setNorRep(NodeID nor, NodeID rep) {
        norToRepMap.insert(std::pair<NodeID, NodeID>(nor, rep));
    };

    inline NodeID getNorRep(NodeID nor) {
        NodeToRepMap::const_iterator it = norToRepMap.find(nor);
        assert(it != norToRepMap.end() && "No such rep node in nor to rep map!");
        return it->second;
    };
    NodeID solveRep(NodeID rep);

    void buildOfflineCG();
    bool addRefLoadEdge(NodeID src, NodeID dst);
    bool addRefStoreEdge(NodeID src, NodeID dst);
    bool createRefNode(NodeID nodeId);
};



namespace llvm {
/* !
 * GraphTraits specializations for the generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */

template<> struct GraphTraits<OfflineConsG*> : public GraphTraits<GenericGraph<ConstraintNode,ConstraintEdge>* > {
typedef ConstraintNode *NodeRef;
};

}

#endif //OFFLINECONSG_H
