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

#include "Graphs/CallGraph.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"
#include <sstream>

using namespace SVF;
using namespace SVFUtil;

CallGraph::CallSiteToIdMap CallGraph::csToIdMap;
CallGraph::IdToCallSiteMap CallGraph::idToCSMap;
CallSiteID CallGraph::totalCallSiteNum=1;


const std::string &CallGraphNode::getName() const
{
    return fun->getName();
}


/// Add direct and indirect callsite
//@{
void CallGraphEdge::addDirectCallSite(const CallICFGNode* call)
{
    assert(call->getCalledFunction() && "not a direct callsite??");
    directCalls.insert(call);
}

void CallGraphEdge::addInDirectCallSite(const CallICFGNode* call)
{
    assert((nullptr == call->getCalledFunction() || !SVFUtil::isa<FunValVar>(SVFUtil::getForkedFun(call))) &&
           "not an indirect callsite??");
    indirectCalls.insert(call);
}
//@}

const std::string CallGraphEdge::toString() const
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

const std::string CallGraphNode::toString() const
{
    std::string str;
    std::stringstream  rawstr(str);
    rawstr << "PTACallGraphNode ID: " << getId() << " {fun: " << fun->getName() << "}";
    return rawstr.str();
}

const FunObjVar *CallGraph::getCallerOfCallSite(CallSiteID id) const
{
    return getCallSite(id)->getCaller();
}

bool CallGraphNode::isReachableFromProgEntry(Map<NodeID, bool> &reachableFromEntry, NodeBS &visitedNodes) const
{
    std::function<bool(const CallGraphNode*)> dfs =
        [&reachableFromEntry, &visitedNodes, &dfs](const CallGraphNode *v)
    {
        NodeID id = v->getId();
        if (!visitedNodes.test_and_set(id))
            return reachableFromEntry[id];

        if (SVFUtil::isProgEntryFunction(v->getFunction()))
            return reachableFromEntry[id] = true;

        bool result = false;
        for (const_iterator it = v->InEdgeBegin(), eit = v->InEdgeEnd(); it != eit; ++it)
        {
            CallGraphEdge* edge = *it;
            result |= dfs(edge->getSrcNode());
            if (result)
                break;
        }
        return reachableFromEntry[id] = result;
    };

    return dfs(this);
}


/// Constructor
CallGraph::CallGraph(CGEK k): kind(k)
{
    callGraphNodeNum = 0;
    numOfResolvedIndCallEdge = 0;
}

/// Copy constructor
CallGraph::CallGraph(const CallGraph& other)
{
    callGraphNodeNum = other.getTotalNodeNum();
    numOfResolvedIndCallEdge = 0;
    kind = NormCallGraph;

    /// copy call graph nodes
    for (const auto& item : other)
    {
        const CallGraphNode* cgn = item.second;
        CallGraphNode* callGraphNode = new CallGraphNode(cgn->getId(), cgn->getFunction());
        addGNode(cgn->getId(),callGraphNode);
        funToCallGraphNodeMap[cgn->getFunction()] = callGraphNode;
    }

    /// copy edges
    for (const auto& item : other.callinstToCallGraphEdgesMap)
    {
        const CallICFGNode* cs = item.first;
        for (const CallGraphEdge* edge : item.second)
        {
            CallGraphNode* src = getCallGraphNode(edge->getSrcID());
            CallGraphNode* dst = getCallGraphNode(edge->getDstID());
            CallSiteID csId = addCallSite(cs, dst->getFunction());

            CallGraphEdge* newEdge = new CallGraphEdge(src,dst, CallGraphEdge::CallRetEdge,csId);
            newEdge->addDirectCallSite(cs);
            addEdge(newEdge);
            callinstToCallGraphEdgesMap[cs].insert(newEdge);
        }
    }

}

/*!
 *  Memory has been cleaned up at GenericGraph
 */
void CallGraph::destroy()
{
}

/*!
 *  Whether we have already created this call graph edge
 */
CallGraphEdge* CallGraph::hasGraphEdge(CallGraphNode* src,
                                       CallGraphNode* dst,
                                       CallGraphEdge::CEDGEK kind, CallSiteID csId) const
{
    CallGraphEdge edge(src,dst,kind,csId);
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
 * get PTACallGraph edge via nodes
 */
CallGraphEdge* CallGraph::getGraphEdge(CallGraphNode* src,
                                       CallGraphNode* dst,
                                       CallGraphEdge::CEDGEK kind, CallSiteID)
{
    for (CallGraphEdge::CallGraphEdgeSet::iterator iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        CallGraphEdge* edge = (*iter);
        if (edge->getEdgeKind() == kind && edge->getDstID() == dst->getId())
            return edge;
    }
    return nullptr;
}


/*!
 * Add indirect call edge to update call graph
 */
void CallGraph::addIndirectCallGraphEdge(const CallICFGNode* cs,const FunObjVar* callerFun, const FunObjVar* calleeFun)
{

    CallGraphNode* caller = getCallGraphNode(callerFun);
    CallGraphNode* callee = getCallGraphNode(calleeFun);

    numOfResolvedIndCallEdge++;

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if(!hasGraphEdge(caller,callee, CallGraphEdge::CallRetEdge,csId))
    {
        CallGraphEdge* edge = new CallGraphEdge(caller,callee, CallGraphEdge::CallRetEdge, csId);
        edge->addInDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
}

/*!
 * Get all callsite invoking this callee
 */
void CallGraph::getAllCallSitesInvokingCallee(const FunObjVar* callee, CallGraphEdge::CallInstSet& csSet)
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
        for(CallGraphEdge::CallInstSet::const_iterator cit = (*it)->indirectCallsBegin(),
                ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Get direct callsite invoking this callee
 */
void CallGraph::getDirCallSitesInvokingCallee(const FunObjVar* callee, CallGraphEdge::CallInstSet& csSet)
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
 * Get indirect callsite invoking this callee
 */
void CallGraph::getIndCallSitesInvokingCallee(const FunObjVar* callee, CallGraphEdge::CallInstSet& csSet)
{
    CallGraphNode* callGraphNode = getCallGraphNode(callee);
    for(CallGraphNode::iterator it = callGraphNode->InEdgeBegin(), eit = callGraphNode->InEdgeEnd();
            it!=eit; ++it)
    {
        for(CallGraphEdge::CallInstSet::const_iterator cit = (*it)->indirectCallsBegin(),
                ecit = (*it)->indirectCallsEnd(); cit!=ecit; ++cit)
        {
            csSet.insert((*cit));
        }
    }
}

/*!
 * Issue a warning if the function which has indirect call sites can not be reached from program entry.
 */
void CallGraph::verifyCallGraph()
{
    if (Options::DisableWarn())
        return;

    Map<NodeID, bool> reachableFromEntry;
    NodeBS visitedNodes;
    CallEdgeMap::const_iterator it = indirectCallMap.begin();
    CallEdgeMap::const_iterator eit = indirectCallMap.end();
    for (; it != eit; ++it)
    {
        const FunctionSet& targets = it->second;
        if (targets.empty() == false)
        {
            const CallICFGNode* cs = it->first;
            const FunObjVar* func = cs->getCaller();
            if (getCallGraphNode(func)->
                    isReachableFromProgEntry(reachableFromEntry, visitedNodes) == false)
                writeWrnMsg(func->getName() + " has indirect call site but not reachable from main");
        }
    }
}

/*!
 * Whether its reachable between two functions
 */
bool CallGraph::isReachableBetweenFunctions(const FunObjVar* srcFn, const FunObjVar* dstFn) const
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


/*!
 * Add call graph node
 */
void CallGraph::addCallGraphNode(const FunObjVar* fun)
{
    NodeID id  = callGraphNodeNum;
    CallGraphNode*callGraphNode = new CallGraphNode(id, fun);
    addGNode(id, callGraphNode);
    funToCallGraphNodeMap[callGraphNode->getFunction()] = callGraphNode;
    callGraphNodeNum++;
}

const CallGraphNode* CallGraph::getCallGraphNode(const std::string& name)
{
    for (const auto& item : *this)
    {
        if (item.second->getName() == name)
            return item.second;
    }
    return nullptr;
}

/*!
 * Add direct call edges
 */
void CallGraph::addDirectCallGraphEdge(const CallICFGNode* cs,const FunObjVar* callerFun, const FunObjVar* calleeFun)
{

    CallGraphNode* caller = getCallGraphNode(callerFun);
    CallGraphNode* callee = getCallGraphNode(calleeFun);
    CallSiteID csId = addCallSite(cs, calleeFun);

    if(!hasGraphEdge(caller,callee, CallGraphEdge::CallRetEdge, csId))
    {
        CallGraphEdge* edge = new CallGraphEdge(caller,callee, CallGraphEdge::CallRetEdge, csId);
        edge->addDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
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
    static std::string getNodeLabel(CallGraphNode*node, CallGraph*)
    {
        return node->toString();
    }

    static std::string getNodeAttributes(CallGraphNode*node, CallGraph*)
    {
        const FunObjVar* fun = node->getFunction();
        if (!SVFUtil::isExtCall(fun))
        {
            return "shape=box";
        }
        else
            return "shape=Mrecord";
    }

    template<class EdgeIter>
    static std::string getEdgeAttributes(CallGraphNode*, EdgeIter EI,
                                         CallGraph*)
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
        if (0 != edge->getIndirectCalls().size())
        {
            color = "color=red";
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
