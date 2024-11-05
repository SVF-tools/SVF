//===- CallGraph.h -- Call graph representation----------------------------//
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
 * CallGraph.h
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#ifndef CALLGRAPH_H_
#define CALLGRAPH_H_

#include "Graphs/GenericGraph.h"
#include "SVFIR/SVFValue.h"
#include "Graphs/ICFG.h"
#include <set>

namespace SVF
{

class CallGraphNode;
class SVFModule;


/*
 * Call Graph edge representing a calling relation between two functions
 * Multiple calls from function A to B are merged into one call edge
 * Each call edge has a set of direct callsites and a set of indirect callsites
 */
typedef GenericEdge<CallGraphNode> GenericCallGraphEdgeTy;
class CallGraphEdge : public GenericCallGraphEdgeTy
{

public:
    typedef Set<const CallICFGNode*> CallInstSet;

private:
    CallInstSet directCalls;
public:
    /// Constructor
    CallGraphEdge(CallGraphNode* s, CallGraphNode* d, const CallICFGNode* icfgNode) :
        GenericCallGraphEdgeTy(s, d, icfgNode->getId())
    {
    }
    /// Destructor
    virtual ~CallGraphEdge()
    {
    }

    /// Add direct callsite
    //@{
    void addDirectCallSite(const CallICFGNode* call);
    //@}

    /// Iterators for direct and indirect callsites
    //@{
    inline CallInstSet::const_iterator directCallsBegin() const
    {
        return directCalls.begin();
    }
    inline CallInstSet::const_iterator directCallsEnd() const
    {
        return directCalls.end();
    }
    //@}

    /// ClassOf
    //@{
    static inline bool classof(const CallGraphEdge*)
    {
        return true;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const CallGraphEdge&edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;

    typedef GenericNode<CallGraphNode, CallGraphEdge>::GEdgeSetTy CallGraphEdgeSet;

};

/*
 * Call Graph node representing a function
 */
typedef GenericNode<CallGraphNode, CallGraphEdge> GenericCallGraphNodeTy;
class CallGraphNode : public GenericCallGraphNodeTy
{
private:
    const SVFFunction* fun;

public:
    /// Constructor
    CallGraphNode(NodeID i, const SVFFunction* f) : GenericCallGraphNodeTy(i,CallNodeKd), fun(f)
    {
    }

    inline const std::string &getName() const
    {
        return fun->getName();
    }

    /// Get function of this call node
    inline const SVFFunction* getFunction() const
    {
        return fun;
    }


    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const CallGraphNode&node)
    {
        o << node.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const CallGraphNode*)
    {
        return true;
    }

    static inline bool classof(const GenericICFGNodeTy* node)
    {
        return node->getNodeKind() == CallNodeKd;
    }

    static inline bool classof(const SVFBaseNode* node)
    {
        return node->getNodeKind() == CallNodeKd;
    }
    //@}
};

/*!
 * Pointer Analysis Call Graph used internally for various pointer analysis
 */
typedef GenericGraph<CallGraphNode, CallGraphEdge> GenericCallGraphTy;
class CallGraph : public GenericCallGraphTy
{
    friend class PTACallGraph;

public:
    typedef CallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef Map<const SVFFunction*, CallGraphNode*> FunToCallGraphNodeMap;
    typedef Map<const CallICFGNode*, CallGraphEdgeSet> CallInstToCallGraphEdgesMap;
    typedef Set<const SVFFunction*> FunctionSet;
    typedef OrderedMap<const CallICFGNode*, FunctionSet> CallEdgeMap;

protected:
    FunToCallGraphNodeMap funToCallGraphNodeMap; ///< Call Graph node map
    CallInstToCallGraphEdgesMap callinstToCallGraphEdgesMap; ///< Map a call instruction to its corresponding call edges

    NodeID callGraphNodeNum;

    /// Clean up memory
    void destroy();

    /// Add call graph edge
    inline void addEdge(CallGraphEdge* edge)
    {
        edge->getDstNode()->addIncomingEdge(edge);
        edge->getSrcNode()->addOutgoingEdge(edge);
    }


public:
    /// Constructor
    CallGraph();

    void addCallGraphNode(const SVFFunction* fun);

    /// Destructor
    virtual ~CallGraph()
    {
        destroy();
    }

    /// Get call graph node
    //@{
    inline CallGraphNode* getCallGraphNode(NodeID id) const
    {
        return getGNode(id);
    }
    inline CallGraphNode* getCallGraphNode(const SVFFunction* fun) const
    {
        FunToCallGraphNodeMap::const_iterator it = funToCallGraphNodeMap.find(fun);
        assert(it!=funToCallGraphNodeMap.end() && "call graph node not found!!");
        return it->second;
    }

    //@}

    /// Whether we have already created this call graph edge
    CallGraphEdge* hasGraphEdge(CallGraphNode* src, CallGraphNode* dst,
                                const CallICFGNode* callIcfgNode) const;

    /// Add direct call edges
    void addDirectCallGraphEdge(const CallICFGNode* call, const SVFFunction* callerFun, const SVFFunction* calleeFun);
    /// Dump the graph
    void dump(const std::string& filename);

    /// View the graph from the debugger
    void view();
};

} // End namespace SVF

namespace SVF
{
/* !
 * GenericGraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GenericGraphTraits<SVF::CallGraphNode*> : public GenericGraphTraits<SVF::GenericNode<SVF::CallGraphNode,SVF::CallGraphEdge>*  >
{
};

/// Inverse GenericGraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GenericGraphTraits<Inverse<SVF::CallGraphNode*> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::CallGraphNode,SVF::CallGraphEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::CallGraph*> : public GenericGraphTraits<SVF::GenericGraph<SVF::CallGraphNode,SVF::CallGraphEdge>* >
{
    typedef SVF::CallGraphNode*NodeRef;
};

} // End namespace llvm

#endif /* CALLGRAPH_H_ */
