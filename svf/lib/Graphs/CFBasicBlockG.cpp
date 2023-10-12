//===- CFBasicBlockG.cpp ----------------------------------------------------------------//
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
 * CFBasicBlockG.cpp
 *
 *  Created on: 24 Dec. 2022
 *      Author: Xiao, Jiawei
 */

#include "Graphs/CFBasicBlockG.h"
#include "SVFIR/SVFIR.h"

namespace SVF
{
CFBasicBlockNode::CFBasicBlockNode(const SVFBasicBlock* svfBasicBlock)
    : GenericCFBasicBlockNodeTy(
          PAG::getPAG()
          ->getICFG()
          ->getICFGNode(*svfBasicBlock->getInstructionList().begin())
          ->getId(),
          0)
{
    for (auto it = svfBasicBlock->begin(); it != svfBasicBlock->end(); ++it)
    {
        const SVFInstruction* ins = *it;
        ICFGNode* icfgNode = PAG::getPAG()->getICFG()->getICFGNode(ins);
        _icfgNodes.push_back(icfgNode);
    }
}

const std::string CFBasicBlockNode::toString() const
{
    std::string rawStr;
    std::stringstream stringstream(rawStr);
    stringstream << "Block Name: " << getName() << "\n";
    for (const auto &icfgNode: _icfgNodes)
    {
        stringstream << icfgNode->toString() << "\n";
    }
    return stringstream.str();
}

CFBasicBlockEdge* CFBasicBlockGraph::getCFBasicBlockEdge(const CFBasicBlockNode *src, const CFBasicBlockNode *dst, const ICFGEdge* icfgEdge)
{
    CFBasicBlockEdge *edge = nullptr;
    size_t counter = 0;
    for (auto iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        if ((*iter)->getDstID() == dst->getId() && (*iter)->getICFGEdge() == icfgEdge)
        {
            counter++;
            edge = (*iter);
        }
    }
    assert(counter <= 1 && "there's more than one edge between two nodes");
    return edge;
}

std::vector<CFBasicBlockEdge*> CFBasicBlockGraph::getCFBasicBlockEdge(const CFBasicBlockNode *src, const CFBasicBlockNode *dst)
{
    std::vector<CFBasicBlockEdge*> edges;
    for (auto iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        if ((*iter)->getDstID() == dst->getId())
        {
            edges.push_back(*iter);
        }
    }
    return SVFUtil::move(edges);
}

void CFBasicBlockGBuilder::build(SVFModule* module)
{
    _CFBasicBlockG = new CFBasicBlockGraph();
    Map<const SVFBasicBlock*, CFBasicBlockNode*> bbToNode;
    for (const auto& func : *module)
    {
        for (const auto& bb : *func)
        {
            CFBasicBlockNode* pNode = new CFBasicBlockNode(bb);
            bbToNode[bb] = pNode;
            _CFBasicBlockG->addCFBBNode(pNode);
        }
    }

    for (const auto& func : *module)
    {
        for (const auto& bb : *func)
        {
            for (const auto &succ: bb->getSuccessors())
            {
                ICFG *icfg = PAG::getPAG()->getICFG();
                const ICFGNode *pred = icfg->getICFGNode(bb->getTerminator());
                const ICFGEdge *edge = nullptr;
                for (const auto &inst: succ->getInstructionList())
                {
                    if (const ICFGEdge *e = icfg->getICFGEdge(pred, icfg->getICFGNode(inst), ICFGEdge::ICFGEdgeK::IntraCF))
                    {
                        edge = e;
                        break;
                    }
                }
                if (SVFUtil::isa<IntraCFGEdge>(edge))
                {
                    CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbToNode[bb], bbToNode[succ], edge);
                    _CFBasicBlockG->addCFBBEdge(pEdge);
                }
            }
        }
    }
}

void CFBasicBlockGBuilder::build(ICFG* icfg)
{
    _CFBasicBlockG = new CFBasicBlockGraph();
    Map<const SVFBasicBlock*, CFBasicBlockNode*> bbToNode;
    Map<const SVFFunction*, CFBasicBlockNode*> funToFirstNode;
    for (const auto& node : *icfg)
    {
        CFBasicBlockNode* pNode  = nullptr;
        if (const SVFBasicBlock* bb = node.second->getBB())
        {
            if (bbToNode.find(bb) == bbToNode.end())
            {
                pNode = new CFBasicBlockNode({node.second});
                bbToNode[node.second->getBB()] = pNode;
                _CFBasicBlockG->addCFBBNode(pNode);
            }
            else
            {
                pNode = bbToNode[node.second->getBB()];
                pNode->addNode(node.second);
            }
            const SVFFunction* fun = node.second->getFun();
            if (funToFirstNode.find(fun) == funToFirstNode.end())
            {
                funToFirstNode[fun] = nullptr;
            }
        }
    }

    for (const auto& node : *icfg)
    {
        for (const auto &succ: node.second->getOutEdges())
        {
            const SVFFunction* node_fun = node.second->getFun();
            const SVFFunction* succ_fun = succ->getDstNode()->getFun();
            const SVFBasicBlock* node_bb = node.second->getBB();
            const SVFBasicBlock* succ_bb = succ->getDstNode()->getBB();
            if (node_fun == succ_fun)
            {
                if (node_bb != succ_bb)
                {
                    CFBasicBlockEdge *pEdge = new CFBasicBlockEdge(bbToNode[node_bb], bbToNode[succ_bb], succ);
                    _CFBasicBlockG->addCFBBEdge(pEdge);
                }
            }
        }
    }

    for (auto it = funToFirstNode.begin(); it != funToFirstNode.end(); ++it)
    {
        const SVFFunction* fun = it->first;
        const SVFBasicBlock* bb = *fun->getBasicBlockList().begin();
        funToFirstNode[fun] = bbToNode[bb];
    }
    _CFBasicBlockG->_funToFirstNode = funToFirstNode;
}
}