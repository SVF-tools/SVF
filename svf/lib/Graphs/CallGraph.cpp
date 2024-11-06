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
 *      Author: Yulei Sui
 */

#include "Graphs/CallGraph.h"
#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"
#include <sstream>

using namespace SVF;
using namespace SVFUtil;


/// Add direct callsite
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
    rawstr << "CallICFGNode ID: " << getEdgeKindWithoutMask();
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


/// Constructor
CallGraph::CallGraph()
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
void CallGraph::addCallGraphNode(const SVFFunction* fun)
{
    NodeID id  = callGraphNodeNum;
    CallGraphNode*callGraphNode = new CallGraphNode(id, fun);
    addGNode(id, callGraphNode);
    funToCallGraphNodeMap[callGraphNode->getFunction()] = callGraphNode;
    callGraphNodeNum++;
}

/*!
 *  Whether we have already created this call graph edge
 */
CallGraphEdge* CallGraph::hasGraphEdge(CallGraphNode* src,
                                       CallGraphNode* dst,
                                       const CallICFGNode* callIcfgNode) const
{
    CallGraphEdge edge(src,dst,callIcfgNode);
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
 * Add direct call edges
 */
void CallGraph::addDirectCallGraphEdge(const CallICFGNode* cs,const SVFFunction* callerFun, const SVFFunction* calleeFun)
{

    CallGraphNode* caller = getCallGraphNode(callerFun);
    CallGraphNode* callee = getCallGraphNode(calleeFun);


    if(!hasGraphEdge(caller,callee, cs))
    {
        CallGraphEdge* edge = new CallGraphEdge(caller,callee, cs);
        edge->addDirectCallSite(cs);
        addEdge(edge);
        callinstToCallGraphEdgesMap[cs].insert(edge);
    }
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
    static std::string getNodeLabel(CallGraphNode*node, CallGraph*)
    {
        return node->toString();
    }

    static std::string getNodeAttributes(CallGraphNode*node, CallGraph*)
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
                                         CallGraph*)
    {

        //TODO: mark indirect call of Fork with different color
        CallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string color = "color=black";
        return color;
    }

    template<class EdgeIter>
    static std::string getEdgeSourceLabel(NodeType*, EdgeIter EI)
    {
        CallGraphEdge* edge = *(EI.getCurrent());
        assert(edge && "No edge found!!");

        std::string str;
        std::stringstream rawstr(str);
        rawstr << edge->getEdgeKindWithoutMask();

        return rawstr.str();
    }
};
} // End namespace llvm
