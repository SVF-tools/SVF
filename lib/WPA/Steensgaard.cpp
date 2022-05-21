/*
 * Steensgaard.cpp
 *
 *  Created on: 2 Feb. 2021
 *      Author: Yulei Sui
 */

#include "WPA/Steensgaard.h"

using namespace SVF;
using namespace SVFUtil;

Steensgaard *Steensgaard::steens = nullptr;

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
        for(NodeID o : getPts(nodeId))
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
            ecUnion(edge->getSrcID(),edge->getDstID());
        }
        /// q = &p->f : EC(q) == EC(p)
        for (ConstraintEdge* edge : node->getGepOutEdges())
        {
            ecUnion(edge->getSrcID(),edge->getDstID());
        }
    }
}


void Steensgaard::setEC(NodeID node, NodeID rep)
{
    rep = getEC(rep);
    Set<NodeID>& subNodes = getSubNodes(node);
    for(NodeID sub : subNodes)
    {
        nodeToECMap[sub] = rep;
        addSubNode(rep,sub);
    }
    subNodes.clear();
}


/// merge node into equiv class and merge node's pts into ec's pts
void Steensgaard::ecUnion(NodeID node, NodeID ec)
{
    if(unionPts(ec, node))
        pushIntoWorklist(ec);
    setEC(node,ec);
}

/*!
 * Process address edges
 */
void Steensgaard::processAllAddr()
{
    for (ConstraintGraph::const_iterator nodeIt = consCG->begin(), nodeEit = consCG->end(); nodeIt != nodeEit; nodeIt++)
    {
        ConstraintNode * cgNode = nodeIt->second;
        for (ConstraintNode::const_iterator it = cgNode->incomingAddrsBegin(), eit = cgNode->incomingAddrsEnd();
                it != eit; ++it)
        {
            numOfProcessedAddr++;

            const AddrCGEdge* addr = cast<AddrCGEdge>(*it);
            NodeID dst = addr->getDstID();
            NodeID src = addr->getSrcID();
            if(addPts(dst,src))
                pushIntoWorklist(dst);
        }
    }
}

