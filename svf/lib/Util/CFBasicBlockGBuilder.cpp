//===- CFBasicBlockGBuilder.cpp ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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
 * CFBasicBlockGBuilder.cpp
 *
 *  Created on: 17 Oct. 2023
 *      Author: Xiao, Jiawei
 */
#include "Util/CFBasicBlockGBuilder.h"

namespace SVF
{

void CFBasicBlockGBuilder::initCFBasicBlockGNodes(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    for (const auto &node: *icfg)
    {
        CFBasicBlockNode *pNode;
        if (const SVFBasicBlock *bb = node.second->getBB())
        {
            if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(node.second))
            {
                pNode = new CFBasicBlockNode({callNode});
                bbToNodes[bb].push_back(pNode);
                _CFBasicBlockG->addCFBBNode(pNode);

                auto *retNode = new CFBasicBlockNode({callNode->getRetICFGNode()});
                bbToNodes[bb].push_back(retNode);
                _CFBasicBlockG->addCFBBNode(retNode);

            }
            else if (!SVFUtil::isa<RetICFGNode>(node.second))
            {
                if (bbToNodes.find(bb) == bbToNodes.end())
                {
                    pNode = new CFBasicBlockNode({node.second});
                    bbToNodes[node.second->getBB()] = {pNode};
                    _CFBasicBlockG->addCFBBNode(pNode);
                }
                else
                {
                    pNode = bbToNodes[node.second->getBB()].back();
                    if (!SVFUtil::isa<RetICFGNode>(pNode->getICFGNodes()[0]))
                    {
                        pNode->addNode(node.second);
                    }
                    else
                    {
                        pNode = new CFBasicBlockNode({node.second});
                        bbToNodes[node.second->getBB()].push_back(pNode);
                        _CFBasicBlockG->addCFBBNode(pNode);
                    }
                }
            }
        }
    }
}

void CFBasicBlockGBuilder::addInterBBEdge(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    // connect inter-BB BBNodes
    for (const auto &node: *icfg)
    {
        for (const auto &succ: node.second->getOutEdges())
        {
            const SVFFunction *node_fun = node.second->getFun();
            const SVFFunction *succ_fun = succ->getDstNode()->getFun();
            const SVFBasicBlock *node_bb = node.second->getBB();
            const SVFBasicBlock *succ_bb = succ->getDstNode()->getBB();
            if (node_fun == succ_fun)
            {
                if (node_bb != succ_bb)
                {
                    CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbToNodes[node_bb].back(),
                            bbToNodes[succ_bb].front(), succ);
                    _CFBasicBlockG->addCFBBEdge(pEdge);
                }
            }
        }
    }
}

void CFBasicBlockGBuilder::addIntraBBEdge(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    // connect intra-BB BBNodes
    for (const auto &bbNodes: bbToNodes)
    {
        for (u32_t i = 0; i < bbNodes.second.size() - 1; ++i)
        {
            if (ICFGEdge *icfgEdge = icfg->getICFGEdge(
                                         const_cast<ICFGNode *>(bbNodes.second[i]->getICFGNodes().back()),
                                         const_cast<ICFGNode *>(bbNodes.second[i + 1]->getICFGNodes().front()),
                                         ICFGEdge::IntraCF))
            {
                CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbNodes.second[i], bbNodes.second[i + 1], icfgEdge);
                _CFBasicBlockG->addCFBBEdge(pEdge);
            }
            else
            {
                // no intra-procedural edge, maybe ext api
            }
        }
    }
}

void CFBasicBlockGBuilder::addInterProceduralEdge(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    // connect inter-procedural BBNodes
    for (const auto &bbNodes: bbToNodes)
    {
        for (u32_t i = 0; i < bbNodes.second.size(); ++i)
        {
            if (const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(
                    bbNodes.second[i]->getICFGNodes().front()))
            {
                for (const auto &icfgEdge: callICFGNode->getOutEdges())
                {
                    if (const CallCFGEdge *callEdge = SVFUtil::dyn_cast<CallCFGEdge>(icfgEdge))
                    {
                        CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbNodes.second[i],
                                bbToNodes[callEdge->getDstNode()->getBB()].front(),
                                callEdge);
                        _CFBasicBlockG->addCFBBEdge(pEdge);
                    }
                }
            }
            else if (const RetICFGNode *retICFGNode = SVFUtil::dyn_cast<RetICFGNode>(
                         bbNodes.second[i]->getICFGNodes().front()))
            {
                for (const auto &icfgEdge: retICFGNode->getInEdges())
                {
                    if (const RetCFGEdge *retEdge = SVFUtil::dyn_cast<RetCFGEdge>(icfgEdge))
                    {
                        CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbToNodes[retEdge->getSrcNode()->getBB()].back(),
                                bbNodes.second[i],
                                retEdge);
                        _CFBasicBlockG->addCFBBEdge(pEdge);
                    }
                }
            }
            else
            {
                // other nodes are intra-procedural
            }
        }
    }
}

void CFBasicBlockGBuilder::build(ICFG* icfg)
{
    _CFBasicBlockG = new CFBasicBlockGraph();
    Map<const SVFBasicBlock*, std::vector<CFBasicBlockNode*>> bbToNodes;

    initCFBasicBlockGNodes(icfg, bbToNodes);
    addInterBBEdge(icfg, bbToNodes);
    addIntraBBEdge(icfg, bbToNodes);
    addInterProceduralEdge(icfg, bbToNodes);
}
}