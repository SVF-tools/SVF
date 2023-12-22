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

}

