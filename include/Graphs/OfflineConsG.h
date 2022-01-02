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

#include "Graphs/ConsG.h"
#include "Util/SCC.h"

namespace SVF
{

/*!
 * Offline constraint graph for Andersen's analysis.
 * In OCG, a 'ref' node is used to represent the point-to set of a constraint node.
 * 'Nor' means a constraint node of its corresponding ref node.
 */
class OfflineConsG: public ConstraintGraph
{

public:
    typedef SCCDetection<OfflineConsG*> OSCC;
    typedef Set<LoadCGEdge*> LoadEdges;
    typedef Set<StoreCGEdge*> StoreEdges;

protected:
    NodeSet refNodes;
    NodeToRepMap nodeToRefMap;  // a --> *a
    NodeToRepMap norToRepMap;   // for each *a construct a --> rep, i.e., mapping a node of to a rep node for online constraint solving

public:
    OfflineConsG(SVFIR *p) : ConstraintGraph(p),
        nodeToRefMap( {}), norToRepMap({})
    {
        buildOfflineCG();
    }

    // Determine whether a node has a OCG rep node
    inline bool hasOCGRep(NodeID node) const
    {
        return hasNorRep(node);
    }
    // Get a node's OCG rep node
    inline NodeID getOCGRep(NodeID node) const
    {
        return getNorRep(node);
    }
    // Get the OCG node to rep map (this map is const and should not be modified)
    inline const NodeToRepMap& getOCGRepMap () const
    {
        return norToRepMap;
    }

    // Determine whether a node is a ref node
    inline bool isaRef(NodeID node) const
    {
        NodeSet::const_iterator it = refNodes.find(node);
        return it != refNodes.end();
    };
    // Determine whether a node has ref nodes
    inline bool hasRef(NodeID node) const
    {
        NodeToRepMap::const_iterator it = nodeToRefMap.find(node);
        return it != nodeToRefMap.end();
    };
    // Use a constraint node to track its corresponding ref node
    inline NodeID getRef(NodeID node) const
    {
        NodeToRepMap::const_iterator it = nodeToRefMap.find(node);
        assert(it != nodeToRefMap.end() && "No such ref node in ref to node map!");
        return it->second;
    }

    // Constraint solver of offline constraint graph
    //{@
    void solveOfflineSCC(OSCC* oscc);
    void buildOfflineMap(OSCC* oscc);
    //@}

    // Dump graph into dot file
    void dump(std::string name);

protected:
    inline bool hasNorRep(NodeID nor) const
    {
        NodeToRepMap::const_iterator it = norToRepMap.find(nor);
        return it != norToRepMap.end();
    };

    inline void setNorRep(NodeID nor, NodeID rep)
    {
        norToRepMap.insert(std::pair<NodeID, NodeID>(nor, rep));
    };

    inline NodeID getNorRep(NodeID nor) const
    {
        NodeToRepMap::const_iterator it = norToRepMap.find(nor);
        assert(it != norToRepMap.end() && "No such rep node in nor to rep map!");
        return it->second;
    };
    NodeID solveRep(OSCC* oscc, NodeID rep);

    void buildOfflineCG();
    bool addRefLoadEdge(NodeID src, NodeID dst);
    bool addRefStoreEdge(NodeID src, NodeID dst);
    bool createRefNode(NodeID nodeId);
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for the generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */

template<> struct GraphTraits<SVF::OfflineConsG*> : public GraphTraits<SVF::GenericGraph<SVF::ConstraintNode,SVF::ConstraintEdge>* >
{
    typedef SVF::ConstraintNode *NodeRef;
};

} // End namespace llvm

#endif //OFFLINECONSG_H
