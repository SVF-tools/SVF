//===- PTACallGraph.h -- PTA Call graph representation----------------------------//
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
 * PTACallGraph.h
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#ifndef PTACALLGRAPH_H_
#define PTACALLGRAPH_H_

#include "Graphs/GenericGraph.h"
#include "SVFIR/SVFValue.h"
#include "Graphs/ICFG.h"
#include "Graphs/CallGraph.h"
#include <set>

namespace SVF
{

class SVFModule;

/*
 * PTA Call Graph edge representing a calling relation between two functions
 * Multiple calls from function A to B are merged into one call edge
 * Each call edge has a set of direct callsites and a set of indirect callsites
 */
//typedef GenericEdge<CallGraphNode> GenericPTACallGraphEdgeTy;
class PTACallGraphEdge : public CallGraphEdge
{

protected:
    CallInstSet indirectCalls;

public:
    /// Constructor
    PTACallGraphEdge(CallGraphNode* s, CallGraphNode* d, CEDGEK k, CallSiteID cs);

    /// Destructor
    virtual ~PTACallGraphEdge()
    {
    }

    /// Get indirect calls
    //@{
    inline bool isDirectCallEdge() const
    {
        return !directCalls.empty() && indirectCalls.empty();
    }
    inline bool isIndirectCallEdge() const
    {
        return directCalls.empty() && !indirectCalls.empty();
    }

    inline CallInstSet& getIndirectCalls()
    {
        return indirectCalls;
    }
    inline const CallInstSet& getIndirectCalls() const
    {
        return indirectCalls;
    }
    //@}

    /// Add indirect callsite
    //@{
    void addInDirectCallSite(const CallICFGNode* call);
    //@}

    /// Iterators for indirect callsites
    //@{
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
    static inline bool classof(const CallGraphEdge *edge)
    {
        return edge->getEdgeKind() == PTACallGraphEdge::CallRetEdge ||
               edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge ||
               edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge;
    }
    //@}

    /// Overloading operator << for dumping ICFG node ID
    //@{
    friend OutStream& operator<< (OutStream &o, const PTACallGraphEdge&edge)
    {
        o << edge.toString();
        return o;
    }
    //@}

    virtual const std::string toString() const;

};


/*!
 * Pointer Analysis Call Graph used internally for various pointer analysis
 */
class PTACallGraph : public CallGraph
{
private:
    /// Indirect call map
    CallEdgeMap indirectCallMap;

protected:
    u32_t numOfResolvedIndCallEdge;
    /// Clean up memory
    void destroy();

public:
    /// Constructor
    PTACallGraph(CGEK k = NormCallGraph);

    /// Add PTAcallgraph Node
    void addPTACallGraphNode(const CallGraphNode* callGraphNode);

    /// Destructor
    virtual ~PTACallGraph()
    {
        destroy();
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

    inline u32_t getNumOfResolvedIndCallEdge() const
    {
        return numOfResolvedIndCallEdge;
    }

    /// Issue a warning if the function which has indirect call sites can not be reached from program entry.
    void verifyCallGraph();


    /// Add indirect call edges
    //@{
    void addDirectCallGraphEdge(const CallICFGNode* call, const SVFFunction* callerFun, const SVFFunction* calleeFun);
    void addIndirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun);
    //@}

    /// Get callsites invoking the callee
    //@{
    void getAllCallSitesInvokingCallee(const SVFFunction* callee,
                                       PTACallGraphEdge::CallInstSet& csSet);

    void getIndCallSitesInvokingCallee(const SVFFunction* callee,
                                       PTACallGraphEdge::CallInstSet& csSet);
    //@}


};

} // End namespace SVF

#endif /* CALLGRAPH_H_ */
