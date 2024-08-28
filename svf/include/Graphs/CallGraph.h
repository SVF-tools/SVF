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
    enum CEDGEK
    {
        CallRetEdge,TDForkEdge,TDJoinEdge,HareParForEdge
    };


private:
    CallInstSet directCalls;
    CallInstSet indirectCalls;
    CallSiteID csId;
public:
    /// Constructor
    CallGraphEdge(CallGraphNode* s, CallGraphNode* d, CEDGEK kind, CallSiteID cs) :
        GenericCallGraphEdgeTy(s, d, makeEdgeFlagWithInvokeID(kind, cs)), csId(cs)
    {
    }
    /// Destructor
    virtual ~CallGraphEdge()
    {
    }
    /// Compute the unique edgeFlag value from edge kind and CallSiteID.
    static inline GEdgeFlag makeEdgeFlagWithInvokeID(GEdgeKind k, CallSiteID cs)
    {
        return (cs << EdgeKindMaskBits) | k;
    }
    /// Get direct and indirect calls
    //@{
    inline CallSiteID getCallSiteID() const
    {
        return csId;
    }
    inline bool isDirectCallEdge() const
    {
        return !directCalls.empty() && indirectCalls.empty();
    }
    inline bool isIndirectCallEdge() const
    {
        return directCalls.empty() && !indirectCalls.empty();
    }
    inline CallInstSet& getDirectCalls()
    {
        return directCalls;
    }
    inline CallInstSet& getIndirectCalls()
    {
        return indirectCalls;
    }
    inline const CallInstSet& getDirectCalls() const
    {
        return directCalls;
    }
    inline const CallInstSet& getIndirectCalls() const
    {
        return indirectCalls;
    }
    //@}

    /// Add direct and indirect callsite
    //@{
    void addDirectCallSite(const CallICFGNode* call);

    void addInDirectCallSite(const CallICFGNode* call);
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

    inline CallInstSet::const_iterator indirectCallsBegin() const
    {
        return indirectCalls.begin();
    }
    inline CallInstSet::const_iterator indirectCallsEnd() const
    {
        return indirectCalls.end();
    }
    //@}

    /// ClassOf
    //@{
    static inline bool classof(const CallGraphEdge*)
    {
        return true;
    }
    static inline bool classof(const GenericCallGraphEdgeTy *edge)
    {
        return edge->getEdgeKind() == CallGraphEdge::CallRetEdge ||
               edge->getEdgeKind() == CallGraphEdge::TDForkEdge ||
               edge->getEdgeKind() == CallGraphEdge::TDJoinEdge;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const CallGraphEdge &edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;

    typedef GenericNode<CallGraphNode,CallGraphEdge>::GEdgeSetTy CallGraphEdgeSet;

};

/*
 * Call Graph node representing a function
 */
typedef GenericNode<CallGraphNode,CallGraphEdge> GenericCallGraphNodeTy;
class CallGraphNode : public GenericCallGraphNodeTy
{

public:
    typedef CallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef CallGraphEdge::CallGraphEdgeSet::iterator iterator;
    typedef CallGraphEdge::CallGraphEdgeSet::const_iterator const_iterator;

private:
    const SVFFunction* fun;

public:
    /// Constructor
    CallGraphNode(NodeID i, const SVFFunction* f) : GenericCallGraphNodeTy(i,0), fun(f)
    {

    }

    /// Get function of this call node
    inline const SVFFunction* getFunction() const
    {
        return fun;
    }

    /// Return TRUE if this function can be reached from main.
    bool isReachableFromProgEntry() const;


    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const CallGraphNode &node)
    {
        o << node.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;
};

/*!
 * Pointer Analysis Call Graph used internally for various pointer analysis
 */
typedef GenericGraph<CallGraphNode,CallGraphEdge> GenericCallGraphTy;
class CallGraph : public GenericCallGraphTy
{

public:
    typedef CallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef Map<const SVFFunction*, CallGraphNode *> FunToCallGraphNodeMap;
    typedef Map<const CallICFGNode*, CallGraphEdgeSet> CallInstToCallGraphEdgesMap;
    typedef std::pair<const CallICFGNode*, const SVFFunction*> CallSitePair;
    typedef Map<CallSitePair, CallSiteID> CallSiteToIdMap;
    typedef Map<CallSiteID, CallSitePair> IdToCallSiteMap;
    typedef Set<const SVFFunction*> FunctionSet;
    typedef OrderedMap<const CallICFGNode*, FunctionSet> CallEdgeMap;
    typedef CallGraphEdgeSet::iterator CallGraphEdgeIter;
    typedef CallGraphEdgeSet::const_iterator CallGraphEdgeConstIter;

    enum CGEK
    {
        NormCallGraph, ThdCallGraph
    };

private:
    CGEK kind;

    /// Indirect call map
    CallEdgeMap indirectCallMap;

    /// Call site information
    static CallSiteToIdMap csToIdMap;	///< Map a pair of call instruction and callee to a callsite ID
    static IdToCallSiteMap idToCSMap;	///< Map a callsite ID to a pair of call instruction and callee
    static CallSiteID totalCallSiteNum;	///< CallSiteIDs, start from 1;

protected:
    FunToCallGraphNodeMap funToCallGraphNodeMap; ///< Call Graph node map
    CallInstToCallGraphEdgesMap callinstToCallGraphEdgesMap; ///< Map a call instruction to its corresponding call edges

    NodeID callGraphNodeNum;
    u32_t numOfResolvedIndCallEdge;

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    CallGraph(CGEK k = NormCallGraph);

    /// Add callgraph Node
    void addCallGraphNode(const SVFFunction* fun);

    /// Destructor
    virtual ~CallGraph()
    {
        destroy();
    }

    /// Return type of this callgraph
    inline CGEK getKind() const
    {
        return kind;
    }

    /// Get callees from an indirect callsite
    //@{
    inline CallEdgeMap& getIndCallMap()
    {
        return indirectCallMap;
    }
    inline bool hasIndCSCallees(const CallICFGNode* cs) const
    {
        return (indirectCallMap.find(cs) != indirectCallMap.end());
    }
    inline const FunctionSet& getIndCSCallees(const CallICFGNode* cs) const
    {
        CallEdgeMap::const_iterator it = indirectCallMap.find(cs);
        assert(it!=indirectCallMap.end() && "not an indirect callsite!");
        return it->second;
    }
    //@}
    inline u32_t getTotalCallSiteNumber() const
    {
        return totalCallSiteNum;
    }

    inline u32_t getNumOfResolvedIndCallEdge() const
    {
        return numOfResolvedIndCallEdge;
    }

    inline const CallInstToCallGraphEdgesMap& getCallInstToCallGraphEdgesMap() const
    {
        return callinstToCallGraphEdgesMap;
    }

    /// Issue a warning if the function which has indirect call sites can not be reached from program entry.
    void verifyCallGraph();

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

    /// Add/Get CallSiteID
    //@{
    inline CallSiteID addCallSite(const CallICFGNode* cs, const SVFFunction* callee)
    {
        std::pair<const CallICFGNode*, const SVFFunction*> newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        //assert(it == csToIdMap.end() && "cannot add a callsite twice");
        if(it == csToIdMap.end())
        {
            CallSiteID id = totalCallSiteNum++;
            csToIdMap.insert(std::make_pair(newCS, id));
            idToCSMap.insert(std::make_pair(id, newCS));
            return id;
        }
        return it->second;
    }
    inline CallSiteID getCallSiteID(const CallICFGNode* cs, const SVFFunction* callee) const
    {
        CallSitePair newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        assert(it != csToIdMap.end() && "callsite id not found! This maybe a partially resolved callgraph, please check the indCallEdge limit");
        return it->second;
    }
    inline bool hasCallSiteID(const CallICFGNode* cs, const SVFFunction* callee) const
    {
        CallSitePair newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        return it != csToIdMap.end();
    }
    inline const CallSitePair& getCallSitePair(CallSiteID id) const
    {
        IdToCallSiteMap::const_iterator it = idToCSMap.find(id);
        assert(it != idToCSMap.end() && "cannot find call site for this CallSiteID");
        return (it->second);
    }
    inline const CallICFGNode* getCallSite(CallSiteID id) const
    {
        return getCallSitePair(id).first;
    }
    inline const SVFFunction* getCallerOfCallSite(CallSiteID id) const
    {
        return getCallSite(id)->getCaller();
    }
    inline const SVFFunction* getCalleeOfCallSite(CallSiteID id) const
    {
        return getCallSitePair(id).second;
    }
    //@}
    /// Whether we have already created this call graph edge
    CallGraphEdge* hasGraphEdge(CallGraphNode* src, CallGraphNode* dst,CallGraphEdge::CEDGEK kind, CallSiteID csId) const;
    /// Get call graph edge via nodes
    CallGraphEdge* getGraphEdge(CallGraphNode* src, CallGraphNode* dst,CallGraphEdge::CEDGEK kind, CallSiteID csId);

    /// Get all callees for a callsite
    inline void getCallees(const CallICFGNode* cs, FunctionSet& callees)
    {
        if(hasCallGraphEdge(cs))
        {
            for (CallGraphEdgeSet::const_iterator it = getCallEdgeBegin(cs), eit =
                        getCallEdgeEnd(cs); it != eit; ++it)
            {
                callees.insert((*it)->getDstNode()->getFunction());
            }
        }
    }

    /// Get call graph edge via call instruction
    //@{
    /// whether this call instruction has a valid call graph edge
    inline bool hasCallGraphEdge(const CallICFGNode* inst) const
    {
        return callinstToCallGraphEdgesMap.find(inst)!=callinstToCallGraphEdgesMap.end();
    }
    inline CallGraphEdgeSet::const_iterator getCallEdgeBegin(const CallICFGNode* inst) const
    {
        CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
        assert(it!=callinstToCallGraphEdgesMap.end()
               && "call instruction does not have a valid callee");
        return it->second.begin();
    }
    inline CallGraphEdgeSet::const_iterator getCallEdgeEnd(const CallICFGNode* inst) const
    {
        CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
        assert(it!=callinstToCallGraphEdgesMap.end()
               && "call instruction does not have a valid callee");
        return it->second.end();
    }
    //@}
    /// Add call graph edge
    inline void addEdge(CallGraphEdge* edge)
    {
        edge->getDstNode()->addIncomingEdge(edge);
        edge->getSrcNode()->addOutgoingEdge(edge);
    }

    /// Add direct/indirect call edges
    //@{
    void addDirectCallGraphEdge(const CallICFGNode* call, const SVFFunction* callerFun, const SVFFunction* calleeFun);
    void addIndirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun);
    //@}

    /// Get callsites invoking the callee
    //@{
    void getAllCallSitesInvokingCallee(const SVFFunction* callee, CallGraphEdge::CallInstSet& csSet);
    void getDirCallSitesInvokingCallee(const SVFFunction* callee, CallGraphEdge::CallInstSet& csSet);
    void getIndCallSitesInvokingCallee(const SVFFunction* callee, CallGraphEdge::CallInstSet& csSet);
    //@}

    /// Whether its reachable between two functions
    bool isReachableBetweenFunctions(const SVFFunction* srcFn, const SVFFunction* dstFn) const;

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
struct GenericGraphTraits<Inverse<SVF::CallGraphNode *> > : public GenericGraphTraits<Inverse<SVF::GenericNode<SVF::CallGraphNode,SVF::CallGraphEdge>* > >
{
};

template<> struct GenericGraphTraits<SVF::CallGraph*> : public GenericGraphTraits<SVF::GenericGraph<SVF::CallGraphNode,SVF::CallGraphEdge>* >
{
    typedef SVF::CallGraphNode *NodeRef;
};

} // End namespace llvm

#endif /* CALLGRAPH_H_ */
