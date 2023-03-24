//===- Steensgaard.cpp -- Steensgaard's field-insensitive
// analysis--------------//
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
 * Steensgaard.cpp
 *
 *  Created on: 2 Feb. 2021
 *      Author: Yulei Sui
 */

#include "WPA/Steensgaard.h"

using namespace SVF;
using namespace SVFUtil;

Steensgaard* Steensgaard::steens = nullptr;

/*!
 * Steensgaard analysis
 */

void Steensgaard::solveWorklist()
{

    processAllAddr();

    // Keep solving until workList is empty.
    while (!isWorklistEmpty())
    {
        NodeID nodeId = popFromWorklist();
        ConstraintNode* node = consCG->getConstraintNode(nodeId);

        /// foreach o \in pts(p)
        for (NodeID o : getPts(nodeId))
        {

            /// *p = q : EC(o) == EC(q)
            for (ConstraintEdge* edge : node->getStoreInEdges())
            {
                ecUnion(edge->getSrcID(), o);
            }
            // r = *p : EC(r) == EC(o)
            for (ConstraintEdge* edge : node->getLoadOutEdges())
            {
                ecUnion(o, edge->getDstID());
            }
        }

        /// q = p : EC(q) == EC(p)
        for (ConstraintEdge* edge : node->getCopyOutEdges())
        {
            ecUnion(edge->getSrcID(), edge->getDstID());
        }
        /// q = &p->f : EC(q) == EC(p)
        for (ConstraintEdge* edge : node->getGepOutEdges())
        {
            ecUnion(edge->getSrcID(), edge->getDstID());
        }
    }
}

void Steensgaard::setEC(NodeID node, NodeID rep)
{
    rep = getEC(rep);
    Set<NodeID>& subNodes = getSubNodes(node);
    for (NodeID sub : subNodes)
    {
        nodeToECMap[sub] = rep;
        addSubNode(rep, sub);
    }
    subNodes.clear();
}

/// merge node into equiv class and merge node's pts into ec's pts
void Steensgaard::ecUnion(NodeID node, NodeID ec)
{
    if (unionPts(ec, node))
        pushIntoWorklist(ec);
    setEC(node, ec);
}

/*!
 * Process address edges
 */
void Steensgaard::processAllAddr()
{
    for (ConstraintGraph::const_iterator nodeIt = consCG->begin(),
            nodeEit = consCG->end();
            nodeIt != nodeEit; nodeIt++)
    {
        ConstraintNode* cgNode = nodeIt->second;
        for (ConstraintNode::const_iterator it = cgNode->incomingAddrsBegin(),
                eit = cgNode->incomingAddrsEnd();
                it != eit; ++it)
        {
            numOfProcessedAddr++;

            const AddrCGEdge* addr = cast<AddrCGEdge>(*it);
            NodeID dst = addr->getDstID();
            NodeID src = addr->getSrcID();
            if (addPts(dst, src))
                pushIntoWorklist(dst);
        }
    }
}

bool Steensgaard::updateCallGraph(const CallSiteToFunPtrMap& callsites)
{

    double cgUpdateStart = stat->getClk();

    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites, newEdges);
    NodePairSet cpySrcNodes; /// nodes as a src of a generated new copy edge
    for (CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end();
            it != eit; ++it)
    {
        CallSite cs = SVFUtil::getSVFCallSite(it->first->getCallSite());
        for (FunctionSet::iterator cit = it->second.begin(),
                ecit = it->second.end();
                cit != ecit; ++cit)
        {
            connectCaller2CalleeParams(cs, *cit, cpySrcNodes);
        }
    }
    for (NodePairSet::iterator it = cpySrcNodes.begin(),
            eit = cpySrcNodes.end();
            it != eit; ++it)
    {
        pushIntoWorklist(it->first);
    }

    double cgUpdateEnd = stat->getClk();
    timeOfUpdateCallGraph += (cgUpdateEnd - cgUpdateStart) / TIMEINTERVAL;

    return (!newEdges.empty());
}

void Steensgaard::heapAllocatorViaIndCall(CallSite cs, NodePairSet& cpySrcNodes)
{
    assert(SVFUtil::getCallee(cs) == nullptr && "not an indirect callsite?");
    RetICFGNode* retBlockNode =
        pag->getICFG()->getRetICFGNode(cs.getInstruction());
    const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
    NodeID srcret;
    CallSite2DummyValPN::const_iterator it = callsite2DummyValPN.find(cs);
    if (it != callsite2DummyValPN.end())
    {
        srcret = getEC(it->second);
    }
    else
    {
        NodeID valNode = pag->addDummyValNode();
        NodeID objNode = pag->addDummyObjNode(cs.getType());
        addPts(valNode, objNode);
        callsite2DummyValPN.insert(std::make_pair(cs, valNode));
        consCG->addConstraintNode(new ConstraintNode(valNode), valNode);
        consCG->addConstraintNode(new ConstraintNode(objNode), objNode);
        srcret = valNode;
    }

    NodeID dstrec = getEC(cs_return->getId());
    if (addCopyEdge(srcret, dstrec))
        cpySrcNodes.insert(std::make_pair(srcret, dstrec));
}

/*!
 * Connect formal and actual parameters for indirect callsites
 */
void Steensgaard::connectCaller2CalleeParams(CallSite cs, const SVFFunction* F,
        NodePairSet& cpySrcNodes)
{
    assert(F);

    DBOUT(DAndersen, outs() << "connect parameters from indirect callsite "
          << cs.getInstruction()->toString() << " to callee "
          << *F << "\n");

    CallICFGNode* callBlockNode =
        pag->getICFG()->getCallICFGNode(cs.getInstruction());
    RetICFGNode* retBlockNode =
        pag->getICFG()->getRetICFGNode(cs.getInstruction());

    if (SVFUtil::isHeapAllocExtFunViaRet(F) &&
            pag->callsiteHasRet(retBlockNode))
    {
        heapAllocatorViaIndCall(cs, cpySrcNodes);
    }

    if (pag->funHasRet(F) && pag->callsiteHasRet(retBlockNode))
    {
        const PAGNode* cs_return = pag->getCallSiteRet(retBlockNode);
        const PAGNode* fun_return = pag->getFunRet(F);
        if (cs_return->isPointer() && fun_return->isPointer())
        {
            NodeID dstrec = getEC(cs_return->getId());
            NodeID srcret = getEC(fun_return->getId());
            if (addCopyEdge(srcret, dstrec))
            {
                cpySrcNodes.insert(std::make_pair(srcret, dstrec));
            }
        }
        else
        {
            DBOUT(DAndersen, outs() << "not a pointer ignored\n");
        }
    }

    if (pag->hasCallSiteArgsMap(callBlockNode) && pag->hasFunArgsList(F))
    {

        // connect actual and formal param
        const SVFIR::SVFVarList& csArgList =
            pag->getCallSiteArgsList(callBlockNode);
        const SVFIR::SVFVarList& funArgList = pag->getFunArgsList(F);
        // Go through the fixed parameters.
        DBOUT(DPAGBuild, outs() << "      args:");
        SVFIR::SVFVarList::const_iterator funArgIt = funArgList.begin(),
                                          funArgEit = funArgList.end();
        SVFIR::SVFVarList::const_iterator csArgIt = csArgList.begin(),
                                          csArgEit = csArgList.end();
        for (; funArgIt != funArgEit; ++csArgIt, ++funArgIt)
        {
            // Some programs (e.g. Linux kernel) leave unneeded parameters
            // empty.
            if (csArgIt == csArgEit)
            {
                DBOUT(DAndersen, outs() << " !! not enough args\n");
                break;
            }
            const PAGNode* cs_arg = *csArgIt;
            const PAGNode* fun_arg = *funArgIt;

            if (cs_arg->isPointer() && fun_arg->isPointer())
            {
                DBOUT(DAndersen, outs() << "process actual parm  "
                      << cs_arg->toString() << " \n");
                NodeID srcAA = getEC(cs_arg->getId());
                NodeID dstFA = getEC(fun_arg->getId());
                if (addCopyEdge(srcAA, dstFA))
                {
                    cpySrcNodes.insert(std::make_pair(srcAA, dstFA));
                }
            }
        }

        // Any remaining actual args must be varargs.
        if (F->isVarArg())
        {
            NodeID vaF = getEC(pag->getVarargNode(F));
            DBOUT(DPAGBuild, outs() << "\n      varargs:");
            for (; csArgIt != csArgEit; ++csArgIt)
            {
                const PAGNode* cs_arg = *csArgIt;
                if (cs_arg->isPointer())
                {
                    NodeID vnAA = getEC(cs_arg->getId());
                    if (addCopyEdge(vnAA, vaF))
                    {
                        cpySrcNodes.insert(std::make_pair(vnAA, vaF));
                    }
                }
            }
        }
        if (csArgIt != csArgEit)
        {
            writeWrnMsg("too many args to non-vararg func.");
            writeWrnMsg("(" + cs.getInstruction()->getSourceLoc() + ")");
        }
    }
}
