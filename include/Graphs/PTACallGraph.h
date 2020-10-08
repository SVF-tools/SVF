//===- PTACallGraph.h -- Call graph representation----------------------------//
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
 * PTACallGraph.h
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#ifndef PTACALLGRAPH_H_
#define PTACALLGRAPH_H_

#include "Graphs/GenericGraph.h"
#include "Util/BasicTypes.h"
#include "Graphs/ICFG.h"
#include <set>

namespace SVF
{

class PTACallGraphNode;
class SVFModule;


/*
 * Call Graph edge representing a calling relation between two functions
 * Multiple calls from function A to B are merged into one call edge
 * Each call edge has a set of direct callsites and a set of indirect callsites
 */
typedef GenericEdge<PTACallGraphNode> GenericCallGraphEdgeTy;
class PTACallGraphEdge : public GenericCallGraphEdgeTy
{

public:
    typedef Set<const CallBlockNode*> CallInstSet;
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
    PTACallGraphEdge(PTACallGraphNode* s, PTACallGraphNode* d, CEDGEK kind, CallSiteID cs) :
        GenericCallGraphEdgeTy(s, d, makeEdgeFlagWithInvokeID(kind, cs)), csId(cs)
    {
    }
    /// Destructor
    virtual ~PTACallGraphEdge()
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
    void addDirectCallSite(const CallBlockNode* call);

    void addInDirectCallSite(const CallBlockNode* call);
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
    static inline bool classof(const PTACallGraphEdge*)
    {
        return true;
    }
    static inline bool classof(const GenericCallGraphEdgeTy *edge)
    {
        return edge->getEdgeKind() == PTACallGraphEdge::CallRetEdge ||
               edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge ||
               edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend raw_ostream& operator<< (raw_ostream &o, const PTACallGraphEdge &edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;

    typedef GenericNode<PTACallGraphNode,PTACallGraphEdge>::GEdgeSetTy CallGraphEdgeSet;

};

/*
 * Call Graph node representing a function
 */
typedef GenericNode<PTACallGraphNode,PTACallGraphEdge> GenericCallGraphNodeTy;
class PTACallGraphNode : public GenericCallGraphNodeTy
{

public:
    typedef PTACallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef PTACallGraphEdge::CallGraphEdgeSet::iterator iterator;
    typedef PTACallGraphEdge::CallGraphEdgeSet::const_iterator const_iterator;

private:
    const SVFFunction* fun;

public:
    /// Constructor
    PTACallGraphNode(NodeID i, const SVFFunction* f) : GenericCallGraphNodeTy(i,0), fun(f)
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
    friend raw_ostream& operator<< (raw_ostream &o, const PTACallGraphNode &node)
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
typedef GenericGraph<PTACallGraphNode,PTACallGraphEdge> GenericCallGraphTy;
class PTACallGraph : public GenericCallGraphTy
{

public:
    typedef PTACallGraphEdge::CallGraphEdgeSet CallGraphEdgeSet;
    typedef Map<const SVFFunction*, PTACallGraphNode *> FunToCallGraphNodeMap;
    typedef Map<const CallBlockNode*, CallGraphEdgeSet> CallInstToCallGraphEdgesMap;
    typedef std::pair<const CallBlockNode*, const SVFFunction*> CallSitePair;
    typedef Map<CallSitePair, CallSiteID> CallSiteToIdMap;
    typedef Map<CallSiteID, CallSitePair> IdToCallSiteMap;
    typedef Set<const SVFFunction*> FunctionSet;
    typedef OrderedMap<const CallBlockNode*, FunctionSet> CallEdgeMap;
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
    Size_t numOfResolvedIndCallEdge;

    /// Clean up memory
    void destroy();

public:
    /// Constructor
    PTACallGraph(CGEK k = NormCallGraph);

    /// Add callgraph Node
    void addCallGraphNode(const SVFFunction* fun);

    /// Destructor
    virtual ~PTACallGraph()
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
    inline bool hasIndCSCallees(const CallBlockNode* cs) const
    {
        return (indirectCallMap.find(cs) != indirectCallMap.end());
    }
    inline const FunctionSet& getIndCSCallees(const CallBlockNode* cs) const
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

    inline Size_t getNumOfResolvedIndCallEdge() const
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
    inline PTACallGraphNode* getCallGraphNode(NodeID id) const
    {
        return getGNode(id);
    }
    inline PTACallGraphNode* getCallGraphNode(const SVFFunction* fun) const
    {
        FunToCallGraphNodeMap::const_iterator it = funToCallGraphNodeMap.find(fun);
        assert(it!=funToCallGraphNodeMap.end() && "call graph node not found!!");
        return it->second;
    }
    //@}

    /// Add/Get CallSiteID
    //@{
    inline CallSiteID addCallSite(const CallBlockNode* cs, const SVFFunction* callee)
    {
        std::pair<const CallBlockNode*, const SVFFunction*> newCS(std::make_pair(cs, callee));
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
    inline CallSiteID getCallSiteID(const CallBlockNode* cs, const SVFFunction* callee) const
    {
        CallSitePair newCS(std::make_pair(cs, callee));
        CallSiteToIdMap::const_iterator it = csToIdMap.find(newCS);
        assert(it != csToIdMap.end() && "callsite id not found! This maybe a partially resolved callgraph, please check the indCallEdge limit");
        return it->second;
    }
    inline bool hasCallSiteID(const CallBlockNode* cs, const SVFFunction* callee) const
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
    inline const CallBlockNode* getCallSite(CallSiteID id) const
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
    /// Whether we have aleady created this call graph edge
    PTACallGraphEdge* hasGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId) const;
    /// Get call graph edge via nodes
    PTACallGraphEdge* getGraphEdge(PTACallGraphNode* src, PTACallGraphNode* dst,PTACallGraphEdge::CEDGEK kind, CallSiteID csId);

    /// Get all callees for a callsite
    inline void getCallees(const CallBlockNode* cs, FunctionSet& callees)
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
    inline bool hasCallGraphEdge(const CallBlockNode* inst) const
    {
        return callinstToCallGraphEdgesMap.find(inst)!=callinstToCallGraphEdgesMap.end();
    }
    inline CallGraphEdgeSet::const_iterator getCallEdgeBegin(const CallBlockNode* inst) const
    {
        CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
        assert(it!=callinstToCallGraphEdgesMap.end()
               && "call instruction does not have a valid callee");
        return it->second.begin();
    }
    inline CallGraphEdgeSet::const_iterator getCallEdgeEnd(const CallBlockNode* inst) const
    {
        CallInstToCallGraphEdgesMap::const_iterator it = callinstToCallGraphEdgesMap.find(inst);
        assert(it!=callinstToCallGraphEdgesMap.end()
               && "call instruction does not have a valid callee");
        return it->second.end();
    }
    //@}
    /// Add call graph edge
    inline void addEdge(PTACallGraphEdge* edge)
    {
        edge->getDstNode()->addIncomingEdge(edge);
        edge->getSrcNode()->addOutgoingEdge(edge);
    }

    /// Add direct/indirect call edges
    //@{
    void addDirectCallGraphEdge(const CallBlockNode* call, const SVFFunction* callerFun, const SVFFunction* calleeFun);
    void addIndirectCallGraphEdge(const CallBlockNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun);
    //@}

    /// Get callsites invoking the callee
    //@{
    void getAllCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet);
    void getDirCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet);
    void getIndCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet);
    //@}

    /// Whether its reachable between two functions
    bool isReachableBetweenFunctions(const SVFFunction* srcFn, const SVFFunction* dstFn) const;

    /// Dump the graph
    void dump(const std::string& filename);
};

} // End namespace SVF

namespace llvm
{
/* !
 * GraphTraits specializations for generic graph algorithms.
 * Provide graph traits for traversing from a constraint node using standard graph traversals.
 */
template<> struct GraphTraits<SVF::PTACallGraphNode*> : public GraphTraits<SVF::GenericNode<SVF::PTACallGraphNode,SVF::PTACallGraphEdge>*  >
{
};

/// Inverse GraphTraits specializations for call graph node, it is used for inverse traversal.
template<>
struct GraphTraits<Inverse<SVF::PTACallGraphNode *> > : public GraphTraits<Inverse<SVF::GenericNode<SVF::PTACallGraphNode,SVF::PTACallGraphEdge>* > >
{
};

template<> struct GraphTraits<SVF::PTACallGraph*> : public GraphTraits<SVF::GenericGraph<SVF::PTACallGraphNode,SVF::PTACallGraphEdge>* >
{
    typedef SVF::PTACallGraphNode *NodeRef;
};

} // End namespace llvm

#endif /* PTACALLGRAPH_H_ */
