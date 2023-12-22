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

#include <SVFIR/SVFIR.h>

namespace SVF
{

/**
 * Initialize Control Flow Basic Block Graph (CFBasicBlockG) nodes based on the provided Interprocedural Control Flow Graph (ICFG).
 *
 * @param icfg The Interprocedural Control Flow Graph (ICFG) to initialize from.
 * @param bbToNodes A map that associates each SVFBasicBlock with a vector of CFBasicBlockNode objects.
 */
void CFBasicBlockGBuilder::initCFBasicBlockGNodes(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    for (const auto &func : *PAG::getPAG()->getModule())
    {
        for(const auto& bb: *func)
        {
            for(const auto& inst: *bb)
            {
                if(SVFUtil::isIntrinsicInst(inst)) continue;
                const ICFGNode* icfgNode = icfg->getICFGNode(inst);
                if (const CallICFGNode *callNode = SVFUtil::dyn_cast<CallICFGNode>(icfgNode))
                {
                    // Create a new CFBasicBlockNode for the CallICFGNode
                    CFBasicBlockNode* pNode = new CFBasicBlockNode({callNode});
                    bbToNodes[bb].push_back(pNode);
                    _CFBasicBlockG->addCFBBNode(pNode);

                    // Create a new CFBasicBlockNode for the corresponding RetICFGNode
                    auto *retNode = new CFBasicBlockNode({callNode->getRetICFGNode()});
                    bbToNodes[bb].push_back(retNode);
                    _CFBasicBlockG->addCFBBNode(retNode);
                }
                else
                {
                    if (bbToNodes.find(bb) == bbToNodes.end())
                    {
                        // Create a new CFBasicBlockNode for the non-CallICFGNode
                        CFBasicBlockNode* pNode = new CFBasicBlockNode({icfgNode});
                        bbToNodes[bb] = {pNode};
                        _CFBasicBlockG->addCFBBNode(pNode);
                    }
                    else
                    {
                        CFBasicBlockNode* pNode = bbToNodes[bb].back();
                        if (!SVFUtil::isa<RetICFGNode>(pNode->getICFGNodes()[0]))
                        {
                            // Add the non-CallICFGNode to the existing CFBasicBlockNode
                            pNode->addNode(icfgNode);
                        }
                        else
                        {
                            // Create a new CFBasicBlockNode for the non-CallICFGNode
                            pNode = new CFBasicBlockNode({icfgNode});
                            bbToNodes[bb].push_back(pNode);
                            _CFBasicBlockG->addCFBBNode(pNode);
                        }
                    }
                }
            }
        }

        if(const FunEntryICFGNode* funEntryNode = icfg->getFunEntryICFGNode(func))
        {
            if(const SVFBasicBlock* bb = funEntryNode->getBB())
            {
                std::vector<CFBasicBlockNode *>& nodes = bbToNodes[bb];
                CFBasicBlockNode* pNode = new CFBasicBlockNode({funEntryNode});
                nodes.insert(nodes.begin(), pNode);
                _CFBasicBlockG->addCFBBNode(pNode);
            }
        }

        if(const FunExitICFGNode* funExitNode = icfg->getFunExitICFGNode(func))
        {
            if(const SVFBasicBlock* bb = funExitNode->getBB())
            {
                std::vector<CFBasicBlockNode *>& nodes = bbToNodes[bb];
                CFBasicBlockNode* pNode = new CFBasicBlockNode({funExitNode});
                nodes.push_back(pNode);
                _CFBasicBlockG->addCFBBNode(pNode);
            }
        }
    }
}


/**
 * Add inter-BasicBlock edges to the Control Flow Basic Block Graph (CFBasicBlockG) based on the provided Interprocedural Control Flow Graph (ICFG).
 *
 * @param icfg The Interprocedural Control Flow Graph (ICFG) to extract edges from.
 * @param bbToNodes A map that associates each SVFBasicBlock with a vector of CFBasicBlockNode objects.
 */
void CFBasicBlockGBuilder::addInterBBEdge(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    // Connect inter-BB BBNodes
    for (const auto &node : *icfg)
    {
        for (const auto &succ : node.second->getOutEdges())
        {
            // Check if it's an intraCFGEdge in case of recursive functions
            // if recursive functions, node_fun == succ_fun but they are different call context
            // edges related to recursive functions would be added in addInterProceduralEdge()
            if (succ->isIntraCFGEdge())
            {
                const SVFFunction *node_fun = node.second->getFun();
                const SVFFunction *succ_fun = succ->getDstNode()->getFun();
                const SVFBasicBlock *node_bb = node.second->getBB();
                const SVFBasicBlock *succ_bb = succ->getDstNode()->getBB();
                if (node_fun == succ_fun)
                {
                    if (node_bb != succ_bb)
                    {
                        // Create a new CFBasicBlockEdge connecting the last node of the source BB
                        // and the first node of the destination BB
                        CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(
                            bbToNodes[node_bb].back(),
                            bbToNodes[succ_bb].front(), succ);
                        _CFBasicBlockG->addCFBBEdge(pEdge);
                    }
                }
            }
        }
    }
}

/**
 * Add intra-BasicBlock edges to the Control Flow Basic Block Graph (CFBasicBlockG) based on the provided Interprocedural Control Flow Graph (ICFG).
 *
 * @param icfg The Interprocedural Control Flow Graph (ICFG) to extract edges from.
 * @param bbToNodes A map that associates each SVFBasicBlock with a vector of CFBasicBlockNode objects.
 */
void CFBasicBlockGBuilder::addIntraBBEdge(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    // Connect intra-BB BBNodes
    for (const auto &bbNodes : bbToNodes)
    {
        for (u32_t i = 0; i < bbNodes.second.size() - 1; ++i)
        {
            // Check if an intraCFGEdge exists between the last node of the source BB and the first node of the destination BB
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
                // No intra-procedural edge found, possibly an external API call
            }
        }
    }
}

/**
 * Add inter-procedural edges between CFBasicBlockNodes based on the provided Interprocedural Control Flow Graph (ICFG).
 *
 * @param icfg The Interprocedural Control Flow Graph (ICFG) to extract edges from.
 * @param bbToNodes A map that associates each SVFBasicBlock with a vector of CFBasicBlockNode objects.
 */
void CFBasicBlockGBuilder::addInterProceduralEdge(ICFG *icfg,
        Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> &bbToNodes)
{
    // Connect inter-procedural BBNodes
    for (const auto &bbNodes : bbToNodes)
    {
        for (u32_t i = 0; i < bbNodes.second.size(); ++i)
        {
            if (const CallICFGNode *callICFGNode = SVFUtil::dyn_cast<CallICFGNode>(
                    bbNodes.second[i]->getICFGNodes().front()))
            {
                for (const auto &icfgEdge : callICFGNode->getOutEdges())
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
                for (const auto &icfgEdge : retICFGNode->getInEdges())
                {
                    if (const RetCFGEdge *retEdge = SVFUtil::dyn_cast<RetCFGEdge>(icfgEdge))
                    {
                        // no return function
                        if(!retEdge->getSrcNode()->getFun()->hasReturn()) continue;
                        CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbToNodes[retEdge->getSrcNode()->getBB()].back(),
                                bbNodes.second[i],
                                retEdge);
                        _CFBasicBlockG->addCFBBEdge(pEdge);
                    }
                }
            }
            else
            {
                // Other nodes are intra-procedural
            }
        }
    }
}

/**
 * Build the Control Flow Basic Block Graph (CFBasicBlockG) based on the provided Interprocedural Control Flow Graph (ICFG).
 *
 * @param icfg The Interprocedural Control Flow Graph (ICFG) to extract control flow information from.
 */
void CFBasicBlockGBuilder::build(ICFG *icfg)
{
    _CFBasicBlockG = new CFBasicBlockGraph();
    Map<const SVFBasicBlock *, std::vector<CFBasicBlockNode *>> bbToNodes;

    initCFBasicBlockGNodes(icfg, bbToNodes);
    addInterBBEdge(icfg, bbToNodes);
    addIntraBBEdge(icfg, bbToNodes);
    addInterProceduralEdge(icfg, bbToNodes);
}

}