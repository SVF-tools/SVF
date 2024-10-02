//===- PTACallGraph.cpp -- Call graph used internally in SVF------------------//
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
 * PTACallGraph.cpp
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui
 */

#include "Graphs/PTACallGraph.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include <sstream>

using namespace SVF;
using namespace SVFUtil;


/// Constructor
PTACallGraphEdge::PTACallGraphEdge(CallGraphNode* s, CallGraphNode* d, CEDGEK k, CallSiteID cs)
    : CallGraphEdge(s,d,k,cs)
{
}

/// Add direct and indirect callsite
//@{
void PTACallGraphEdge::addInDirectCallSite(const CallICFGNode* call)
{
    assert((nullptr == call->getCalledFunction() || nullptr == SVFUtil::dyn_cast<SVFFunction> (SVFUtil::getForkedFun(call))) && "not an indirect callsite??");
    indirectCalls.insert(call);
}
//@}



/// Constructor
PTACallGraph::PTACallGraph(CGEK k): CallGraph(k)
{
    numOfResolvedIndCallEdge = 0;
}

/*!
 *  Memory has been cleaned up at GenericGraph
 */
void PTACallGraph::destroy()
{
}


const std::string PTACallGraphEdge::toString() const
{
    std::string str;
    std::stringstream  rawstr(str);
    rawstr << "CallSite ID: " << getCallSiteID();
    if(isDirectCallEdge())
        rawstr << "direct call";
    else
        rawstr << "indirect call";
    rawstr << "[" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

/*!
 * Add call graph node
 */
void PTACallGraph::addPTACallGraphNode(const CallGraphNode* callGraphNode)
{
    CallGraphNode* newNode = new CallGraphNode(callGraphNode->getId(), callGraphNode->getFunction());
    addGNode(callGraphNode->getId(), newNode);
    funToCallGraphNodeMap[callGraphNode->getFunction()] = newNode;
}


/*!
 * Add direct call edges
 */
void PTACallGraph::addDirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun)
{

    CallGraphNode* caller = getCallGraphNode(callerFun);
    CallGraphNode* callee = getCallGraphNode(calleeFun);

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csId))
    {
        PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csId);
        edge->addDirectCallSite(cs);
        addEdge(edge);
        callinstToPTACallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Add indirect call edge to update call graph
 */
void PTACallGraph::addIndirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun)
{

    CallGraphNode* caller = getCallGraphNode(callerFun);
    CallGraphNode* callee = getCallGraphNode(calleeFun);

    numOfResolvedIndCallEdge++;

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge,csId))
    {
        PTACallGraphEdge* edge = new PTACallGraphEdge(caller,callee, PTACallGraphEdge::CallRetEdge, csId);
        edge->addInDirectCallSite(cs);
        addEdge(edge);
        callinstToPTACallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Get all callsite invoking this callee
 */
void PTACallGraph::getAllCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet)
{
    CallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(CallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
        PTACallGraphEdge  * edge = dyn_cast<PTACallGraphEdge>(*it);
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = edge->indirectCallsBegin(),
                ecit = edge->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get direct callsite invoking this callee
 */
void PTACallGraph::getDirCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet)
{
    CallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(CallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get indirect callsite invoking this callee
 */
void PTACallGraph::getIndCallSitesInvokingCallee(const SVFFunction* callee, PTACallGraphEdge::CallInstSet& csSet)
{
    CallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(CallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        PTACallGraphEdge  * edge = dyn_cast<PTACallGraphEdge>(*it);
        for(PTACallGraphEdge::CallInstSet::const_iterator cit = edge->indirectCallsBegin(),
                ecit = edge->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Issue a warning if the function which has indirect call sites can not be reached from program entry.
 */
void PTACallGraph::verifyCallGraph()
{
    CallEdgeMap::const_iterator it = indirectCallMap.begin();
    CallEdgeMap::const_iterator eit = indirectCallMap.end();
    for (; it != eit; ++it)
    {
        const FunctionSet& targets = it->second;
        if (targets.empty() == false)
        {
            const CallICFGNode* cs = it->first;
            const SVFFunction* func = cs->getCaller();
            if (getCallGraphNode(func)->isReachableFromProgEntry() == false)
                writeWrnMsg(func->getName() + " has indirect call site but not reachable from main");
        }
    }
}


namespace SVF
{

/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<PTACallGraph*> : public DefaultDOTGraphTraits
{

    typedef CallGraphNode NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(PTACallGraph*)
    {
        return "PTA Call Graph";
    }
    /// Return function name;
    static std::string getNodeLabel(CallGraphNode*node, PTACallGraph*)
    {
        return node->toString();
    }

    static std::string getNodeAttributes(CallGraphNode*node, PTACallGraph*)
    {
        const SVFFunction* fun = node->getFunction();
        if (!SVFUtil::isExtCall(fun))
        {
            return "shape=box";
        }
        else
            return "shape=Mrecord";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(CallGraphNode*, EdgeIter EI,
                                         PTACallGraph*)
    {

        //TODO: mark indirect call of Fork with different color
        PTACallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string color;

        if (edge->getEdgeKind() == PTACallGraphEdge::TDJoinEdge)
        {
            color = "color=green";
        }
        else if (edge->getEdgeKind() == PTACallGraphEdge::TDForkEdge)
        {
            color = "color=blue";
        }
        else
        {
            color = "color=black";
        }
        if (0 != edge->getIndirectCalls().size())
        {
            color = "color=red";
        }
        return color;
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        PTACallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        rawstr << edge->getCallSiteID();

        return rawstr.str();
    }
};
} // End namespace llvm
