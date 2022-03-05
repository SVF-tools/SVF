//===----- CFLGraph.h -- Graph for context-free language reachability analysis --//
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
 * CFLGraph.h
 *
 *  Created on: March 5, 2022
 *      Author: Yulei Sui
 */

#include "Graphs/GenericGraph.h"

namespace SVF
{
class CFLNode;

typedef GenericEdge<CFLNode> GenericCFLEdgeTy;

class CFLEdge: public GenericCFLEdgeTy
{
public:
    CFLEdge(CFLNode *s, CFLNode *d, GEdgeFlag k = 0):
        GenericCFLEdgeTy(s,d,k)
    {
    }
    ~CFLEdge() override = default;
};


typedef GenericNode<CFLNode,CFLEdge> GenericCFLNodeTy;
class CFLNode: public GenericCFLNodeTy
{
public:
    CFLNode (NodeID i = 0, GNodeK k = 0):
        GenericCFLNodeTy(i, k)
    {
    }
    ~CFLNode() override = default;
};

/// Edge-labeled graph for CFL Reachability analysis
typedef GenericGraph<CFLNode,CFLEdge> GenericCFLGraphTy;
class CFLGraph: public GenericCFLGraphTy
{
public:
    typedef GenericNode<CFLNode,CFLEdge>::GEdgeSetTy CFLEdgeSet;

    CFLGraph()
    {
    }
    ~CFLGraph() override = default;

    /// Build graph by copying nodes and edges from any graph inherited from GenericGraph
    template<class N, class E> 
    void build(GenericGraph<N,E>* graph){
        for(auto it = graph->begin(); it!= graph->end(); it++){
            CFLNode* node = new CFLNode((*it).first);
            addCFLNode((*it).first, node);
        }
        for(auto it = graph->begin(); it!= graph->end(); it++){
            N* node = (*it).second;
            for(E* edge : node->getOutEdges())
                addCFLEdge(getGNode(edge->getSrcID()), getGNode(edge->getDstID()), edge->getEdgeKind());
        }
    }

    virtual void addCFLNode(NodeID id, CFLNode* node);

    virtual const CFLEdge* addCFLEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label);

    virtual const CFLEdge* hasEdge(CFLNode* src, CFLNode* dst, CFLEdge::GEdgeFlag label);

    /// Build graph from file
    void build(std::string filename);

private:
    CFLEdgeSet cflEdgeSet;
};

}