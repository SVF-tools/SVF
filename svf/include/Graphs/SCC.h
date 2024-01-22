//===- SCC.h -- SCC detection algorithm---------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * SCC.h
 *
 * Esko Nuutila and Eljas Soisalon-Soininen, "On finding the
 *  strongly connected components in a directed graph".
 *  Inf. Process. Letters, 49(1):9-14, 1994.
 *
 * The implementation is derived from the pseudo code in the following paper:
 * Pereira and Berlin, "Wave Propagation and Deep Propagation for Pointer Analysis",
 * CGO 2009, 126-135, 2009.
 *
 * And influenced by implementation from Open64 compiler
 *
 *  Created on: Jul 12, 2013
 *      Author: yusui
 */

#ifndef SCC_H_
#define SCC_H_

#include "SVFIR/SVFValue.h"	// for NodeBS
#include <limits.h>
#include <stack>
#include <map>

namespace SVF
{

class GNodeSCCInfo;

template<class GraphType>
class SCCDetection
{

private:
    ///Define the GTraits and node iterator for printing
    typedef SVF::GenericGraphTraits<GraphType> GTraits;
    typedef typename GTraits::NodeRef          GNODE;
    typedef typename GTraits::nodes_iterator node_iterator;
    typedef typename GTraits::ChildIteratorType child_iterator;
    typedef unsigned NodeID ;

public:
    typedef std::stack<NodeID> GNodeStack;

    class GNodeSCCInfo
    {
    public:
        GNodeSCCInfo() : _visited(false), _inSCC(false), _rep(UINT_MAX) {}

        inline bool visited(void) const
        {
            return _visited;
        }
        inline void visited(bool v)
        {
            _visited = v;
        }
        inline bool inSCC(void)   const
        {
            return _inSCC;
        }
        inline void inSCC(bool v)
        {
            _inSCC = v;
        }
        inline NodeID rep(void)const
        {
            return _rep;
        }
        inline void rep(NodeID n)
        {
            _rep = n;
        }
        inline void addSubNodes(NodeID n)
        {
            _subNodes.set(n);
        }
        inline NodeBS& subNodes()
        {
            return _subNodes;
        }
        inline const NodeBS& subNodes() const
        {
            return _subNodes;
        }
    private:
        bool     _visited;
        bool     _inSCC;
        NodeID  _rep;
        NodeBS _subNodes; /// nodes in the scc represented by this node
    };

    typedef Map<NodeID,GNodeSCCInfo > GNODESCCInfoMap;
    typedef Map<NodeID, NodeID> NodeToNodeMap;

    SCCDetection(const GraphType &GT)
        : _graph(GT),
          _I(0)
    {}


    // Return a handle to the stack of nodes in topological
    // order.  This will be used to seed the initial solution
    // and improve efficiency.
    inline GNodeStack &topoNodeStack()
    {
        return _T;
    }

    const inline  GNODESCCInfoMap &GNodeSCCInfo() const
    {
        return _NodeSCCAuxInfo;
    }

    /// get the rep node if not found return itself
    inline NodeID repNode(NodeID n) const
    {
        typename GNODESCCInfoMap::const_iterator it = _NodeSCCAuxInfo.find(n);
        assert(it!=_NodeSCCAuxInfo.end() && "scc rep not found");
        NodeID rep = it->second.rep();
        return rep!= UINT_MAX ? rep : n ;
    }


    /// whether the node is in a cycle
    inline bool isInCycle(NodeID n) const
    {
        NodeID rep = repNode(n);
        // multi-node cycle
        if (subNodes(rep).count() > 1)
        {
            return true;
        }
        // self-cycle
        else
        {
            child_iterator EI = GTraits::direct_child_begin(Node(rep));
            child_iterator EE = GTraits::direct_child_end(Node(rep));
            for (; EI != EE; ++EI)
            {
                NodeID w = Node_Index(*EI);
                if(w==rep)
                    return true;
            }
            return false;
        }
    }

    /// get all subnodes in one scc, if size is empty insert itself into the set
    inline const NodeBS& subNodes(NodeID n)  const
    {
        typename GNODESCCInfoMap::const_iterator it = _NodeSCCAuxInfo.find(n);
        assert(it!=_NodeSCCAuxInfo.end() && "scc rep not found");
        return it->second.subNodes();
    }

    /// get all repNodeID
    inline const NodeBS &getRepNodes() const
    {
        return repNodes;
    }

    const inline GraphType & graph()
    {
        return _graph;
    }
private:

    GNODESCCInfoMap  _NodeSCCAuxInfo;

    const GraphType &           _graph;
    NodeID                   _I;
    NodeToNodeMap            _D;
    GNodeStack             _SS;
    GNodeStack             _T;
    NodeBS repNodes;

    inline bool visited(NodeID n)
    {
        return _NodeSCCAuxInfo[n].visited();
    }
    inline bool inSCC(NodeID n)
    {
        return _NodeSCCAuxInfo[n].inSCC();
    }

    inline void setVisited(NodeID n,bool v)
    {
        _NodeSCCAuxInfo[n].visited(v);
    }
    inline void setInSCC(NodeID n,bool v)
    {
        _NodeSCCAuxInfo[n].inSCC(v);
    }
    inline void rep(NodeID n, NodeID r)
    {
        _NodeSCCAuxInfo[n].rep(r);
        _NodeSCCAuxInfo[r].addSubNodes(n);
        if (n != r)
        {
            _NodeSCCAuxInfo[n].subNodes().clear();
            repNodes.reset(n);
            repNodes.set(r);
        }
    }

    inline NodeID rep(NodeID n)
    {
        return _NodeSCCAuxInfo[n].rep();
    }
    inline bool isInSCC(NodeID n)
    {
        return _NodeSCCAuxInfo[n].inSCC();
    }

    inline GNODE Node(NodeID id) const
    {
        return GTraits::getNode(_graph, id);
    }

    inline NodeID Node_Index(GNODE node) const
    {
        return GTraits::getNodeID(node);
    }

    void visit(NodeID v)
    {
        // SVFUtil::outs() << "visit GNODE: " << Node_Index(v)<< "\n";
        _I += 1;
        _D[v] = _I;
        this->rep(v,v);
        this->setVisited(v,true);

        child_iterator EI = GTraits::direct_child_begin(Node(v));
        child_iterator EE = GTraits::direct_child_end(Node(v));

        for (; EI != EE; ++EI)
        {
            NodeID w = Node_Index(*EI);

            if (!this->visited(w))
                visit(w);
            if (!this->inSCC(w))
            {
                NodeID rep;
                rep = _D[this->rep(v)] < _D[this->rep(w)] ?
                      this->rep(v) : this->rep(w);
                this->rep(v,rep);
            }
        }
        if (this->rep(v) == v)
        {
            this->setInSCC(v,true);
            while (!_SS.empty())
            {
                NodeID w = _SS.top();
                if (_D[w] <= _D[v])
                    break;
                else
                {
                    _SS.pop();
                    this->setInSCC(w,true);
                    this->rep(w,v);
                }
            }
            _T.push(v);
        }
        else
            _SS.push(v);
    }

    void clear()
    {
        _NodeSCCAuxInfo.clear();
        _I = 0;
        _D.clear();
        repNodes.clear();
        while(!_SS.empty())
            _SS.pop();
        while(!_T.empty())
            _T.pop();
    }
public:

    void find(void)
    {
        // Visit each unvisited root node.   A root node is defined
        // to be a node that has no incoming copy/skew edges
        clear();
        node_iterator I = GTraits::nodes_begin(_graph);
        node_iterator E = GTraits::nodes_end(_graph);
        for (; I != E; ++I)
        {
            NodeID node = Node_Index(*I);
            if (!this->visited(node))
            {
                // We skip any nodes that have a representative other than
                // themselves.  Such nodes occur as a result of merging
                // nodes either through unifying an ACC or other node
                // merging optimizations.  Any such node should have no
                // outgoing edges and therefore should no longer be a member
                // of an SCC.
                if (this->rep(node) == UINT_MAX || this->rep(node) == node)
                    visit(node);
                else
                    this->visited(node);
            }
        }
    }

    void find(NodeSet &candidates)
    {
        // This function is reloaded to only visit candidate NODES
        clear();
        for (NodeID node : candidates)
        {
            if (!this->visited(node))
            {
                if (this->rep(node) == UINT_MAX || this->rep(node) == node)
                    visit(node);
                else
                    this->visited(node);
            }
        }
    }

};

} // End namespace SVF

#endif /* SCC_H_ */
