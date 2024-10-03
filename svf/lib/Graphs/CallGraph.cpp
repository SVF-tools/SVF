//===- CallGraph.cpp -- Call graph used internally in SVF------------------//
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
 * CallGraph.cpp
 *
 *  Created on: Nov 7, 2013
 *      Author: Yulei Sui, Weigang He
 */

#include <sstream>
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include "Graphs/CallGraph.h"

using namespace SVF;
using namespace SVFUtil;

CallGraph::CallSiteToIdMap CallGraph::csToIdMap;
CallGraph::IdToCallSiteMap CallGraph::idToCSMap;
CallSiteID CallGraph::totalCallSiteNum = 1;


/// Add direct and indirect callsite
//@{
void CallGraphEdge::addDirectCallSite(const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "not a direct callsite??");
    directCalls.insert(call);
}
//@}

const std::string CallGraphEdge::toString() const
{
    std::string str;
    std::stringstream  rawstr(str);
    rawstr << "CallSite ID: " << getCallSiteID();
    rawstr << "direct call";
    rawstr << "[" << getDstID() << "<--" << getSrcID() << "]\t";
    return rawstr.str();
}

const std::string CallGraphNode::toString() const
{
    std::string str;
    std::stringstream  rawstr(str);
    rawstr << "CallGraphNode ID: " << getId() << " {fun: " << fun->getName() << "}";
    return rawstr.str();
}

bool CallGraphNode::isReachableFromProgEntry() const
{
    std::stack<const CallGraphNode*> nodeStack;
    NodeBS visitedNodes;
    nodeStack.push(this);
    visitedNodes.set(getId());

    while (nodeStack.empty() == false)
    {
        CallGraphNode* node = const_cast<CallGraphNode*>(nodeStack.top());
        nodeStack.pop();

        if (SVFUtil::isProgEntryFunction(node->getFunction()))
            return true;

        for (const_iterator it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit; ++it)
        {
            CallGraphEdge* edge = *it;
            if (visitedNodes.test_and_set(edge->getSrcID()))
                nodeStack.push(edge->getSrcNode());
        }
    }

    return false;
}


/// Constructor
CallGraph::CallGraph(CGEK k): kind(k)
{
    callGraphNodeNum = 0;
}

/*!
 *  Memory has been cleaned up at GenericGraph
 */
void CallGraph::destroy()
{
}

/*!
 * Add call graph node
 */
void CallGraph::addCallGraphNode(const CallGraphNode* callGraphNode)
{
    CallGraphNode* newNode= const_cast<CallGraphNode*>(callGraphNode);
    addGNode(callGraphNode->getId(), newNode);
    funToCallGraphNodeMap[callGraphNode->getFunction()] = newNode;
}

/*!
 *  Whether we have already created this call graph edge
 */
CallGraphEdge* CallGraph::hasGraphEdge(CallGraphNode* src,
                                          CallGraphNode* dst,
                                             CallGraphEdge::CEDGEK cedgek, CallSiteID csId) const
{
    CallGraphEdge edge(src,dst, cedgek,csId);
    CallGraphEdge* outEdge = src->hasOutgoingEdge(&edge);
    CallGraphEdge* inEdge = dst->hasIncomingEdge(&edge);
    if (outEdge && inEdge)
    {
        assert(outEdge == inEdge && "edges not match");
        return outEdge;
    }
    else
        return nullptr;
}

/*!
 * get CallGraph edge via nodes
 */
CallGraphEdge* CallGraph::getGraphEdge(CallGraphNode* src,
                                          CallGraphNode* dst,
                                             CallGraphEdge::CEDGEK cedgek, CallSiteID)
{
    for (CallGraphEdge::CallGraphEdgeSet::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        CallGraphEdge* edge = (*iter);
        if (edge->getEdgeKind() == cedgek && edge->getDstID() == dst->getId())
            return edge;
    }
    return nullptr;
}

/*!
 * Add direct call edges
 */
void CallGraph::addDirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun)
{

    CallGraphNode* caller = getCallGraphNode(callerFun);
    CallGraphNode* callee = getCallGraphNode(calleeFun);

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, CallGraphEdge::CallRetEdge,csId))
    {
        CallGraphEdge* edge = new CallGraphEdge(caller,callee, CallGraphEdge::CallRetEdge,csId);
        edge->addDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Get direct callsite invoking this callee
 */
void CallGraph::getDirCallSitesInvokingCallee(const SVFFunction* callee, CallGraphEdge::CallInstSet& csSet)
{
    CallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(CallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(CallGraphEdge::CallInstSet::const_iterator cit = (*it)->directCallsBegin(),
                ecit = (*it)->directCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}


/*!
 * Whether its reachable between two functions
 */
bool CallGraph::isReachableBetweenFunctions(const SVFFunction* srcFn, const SVFFunction* dstFn) const
{
    CallGraphNode* dstNode = getCallGraphNode(dstFn);

    std::stack<const CallGraphNode*> nodeStack;
    NodeBS visitedNodes;
    nodeStack.push(dstNode);
    visitedNodes.set(dstNode->getId());

    while (nodeStack.empty() == false)
    {
        CallGraphNode* node = const_cast<CallGraphNode*>(nodeStack.top());
        nodeStack.pop();

        if (node->getFunction() == srcFn)
            return true;

        for (CallGraphEdgeConstIter it = node->InEdgeBegin(), eit = node->InEdgeEnd(); it != eit; ++it)
        {
            CallGraphEdge* edge = *it;
            if (visitedNodes.test_and_set(edge->getSrcID()))
                nodeStack.push(edge->getSrcNode());
        }
    }

    return false;
}

/*!
 * Dump call graph into dot file
 */
void CallGraph::dump(const std::string& filename)
{
    GraphPrinter::WriteGraphToFile(outs(), filename, this);
}

void CallGraph::view()
{
    SVF::ViewGraph(this, "Call Graph");
}

namespace SVF
{

/*!
 * Write value flow graph into dot file for debugging
 */
template<>
struct DOTGraphTraits<CallGraph*> : public DefaultDOTGraphTraits
{

    typedef CallGraphNode NodeType;
    typedef NodeType::iterator ChildIteratorType;
    DOTGraphTraits(bool isSimple = false) :
        DefaultDOTGraphTraits(isSimple)
    {
    }

    /// Return name of the graph
    static std::string getGraphName(CallGraph*)
    {
        return "Call Graph";
    }
    /// Return function name;
    static std::string getNodeLabel(CallGraphNode *node, CallGraph*)
    {
        return node->toString();
    }

    static std::string getNodeAttributes(CallGraphNode *node, CallGraph*)
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
    static std::string getEdgeAttributes(CallGraphNode*, EdgeIter EI, CallGraph*)
    {

        //TODO: mark indirect call of Fork with different color
        CallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string color;

        if (edge->getEdgeKind() == CallGraphEdge::TDJoinEdge)
        {
            color = "color=green";
        }
        else if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge)
        {
            color = "color=blue";
        }
        else
        {
            color = "color=black";
        }
        return color;
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        CallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        rawstr << edge->getCallSiteID();

        return rawstr.str();
    }
};
} // End namespace llvm
