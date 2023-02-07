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
CFBasicBlockNode::CFBasicBlockNode(u32_t id, const SVFBasicBlock *svfBasicBlock) : GenericCFBasicBlockNodeTy(id, 0),
    _svfBasicBlock(svfBasicBlock)
{
    for (auto it = svfBasicBlock->begin(); it != svfBasicBlock->end(); ++it)
    {
        const SVFInstruction *ins = *it;
        ICFGNode *icfgNode = PAG::getPAG()->getICFG()->getICFGNode(ins);
        _icfgNodes.push_back(icfgNode);
    }
}

const std::string CFBasicBlockNode::toString() const
{
    std::string rawStr;
    std::stringstream stringstream(rawStr);
    stringstream << "Block Name: " << _svfBasicBlock->getName() << "\n";
    for (const auto &icfgNode: _icfgNodes)
    {
        stringstream << icfgNode->toString() << "\n";
    }
    return stringstream.str();
}

CFBasicBlockEdge* CFBasicBlockGraph::getCFBasicBlockEdge(const SVFBasicBlock *src, const SVFBasicBlock *dst)
{
    return getCFBasicBlockEdge(getCFBasicBlockNode(src), getCFBasicBlockNode(dst));
}

CFBasicBlockEdge* CFBasicBlockGraph::getCFBasicBlockEdge(const CFBasicBlockNode *src, const CFBasicBlockNode *dst)
{
    CFBasicBlockEdge *edge = nullptr;
    size_t counter = 0;
    for (auto iter = src->OutEdgeBegin();
            iter != src->OutEdgeEnd(); ++iter)
    {
        if ((*iter)->getDstID() == dst->getId())
        {
            counter++;
            edge = (*iter);
        }
    }
    assert(counter <= 1 && "there's more than one edge between two nodes");
    return edge;
}

const CFBasicBlockNode* CFBasicBlockGraph::getOrAddCFBasicBlockNode(const SVFBasicBlock *bb)
{
    auto it = _bbToNode.find(bb);
    if (it != _bbToNode.end()) return it->second;
    CFBasicBlockNode *node = new CFBasicBlockNode(_totalCFBasicBlockNode++, bb);
    _bbToNode[bb] = node;
    addGNode(node->getId(), node);
    return node;
}

const CFBasicBlockEdge* CFBasicBlockGraph::getOrAddCFBasicBlockEdge(CFBasicBlockNode *src, CFBasicBlockNode *dst)
{
    if (const CFBasicBlockEdge *edge = getCFBasicBlockEdge(src, dst)) return edge;
    CFBasicBlockEdge *edge = new CFBasicBlockEdge(src, dst);
    bool added1 = edge->getDstNode()->addIncomingEdge(edge);
    bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
    (void) added1;
    (void) added2;
    assert(added1 && added2 && "edge not added??");
    _totalCFBasicBlockEdge++;
    return edge;
}

void CFBasicBlockGBuilder::build()
{
    for (const auto &bb: *_CFBasicBlockG->_svfFunction)
    {
        _CFBasicBlockG->getOrAddCFBasicBlockNode(bb);
    }
    for (const auto &bb: *_CFBasicBlockG->_svfFunction)
    {
        for (const auto &succ: bb->getSuccessors())
        {
            _CFBasicBlockG->getOrAddCFBasicBlockEdge(_CFBasicBlockG->getCFBasicBlockNode(bb),
                    _CFBasicBlockG->getCFBasicBlockNode(succ));
        }
    }
}
}