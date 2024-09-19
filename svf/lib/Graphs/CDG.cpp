//===- CDG.cpp -- Control Dependence Graph -------------------------//
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
 * CDG.cpp
 *
 *  Created on: Sep 27, 2023
 *      Author: Xiao Cheng
 */
#include "Graphs/CDG.h"

using namespace SVF;

CDG *CDG::controlDg = nullptr;

void CDG::addCDGEdgeFromSrcDst(const ICFGNode *src, const ICFGNode *dst, const SVFVar *pNode, s32_t branchID)
{
    if (!hasCDGNode(src->getId()))
    {
        addGNode(src->getId(), new CDGNode(src));
    }
    if (!hasCDGNode(dst->getId()))
    {
        addGNode(dst->getId(), new CDGNode(dst));
    }
    if (!hasCDGEdge(getCDGNode(src->getId()), getCDGNode(dst->getId())))
    {
        CDGEdge *pEdge = new CDGEdge(getCDGNode(src->getId()),
                                     getCDGNode(dst->getId()));
        pEdge->insertBranchCondition(pNode, branchID);
        addCDGEdge(pEdge);
        incEdgeNum();
    }
    else
    {
        CDGEdge *pEdge = getCDGEdge(getCDGNode(src->getId()),
                                    getCDGNode(dst->getId()));
        pEdge->insertBranchCondition(pNode, branchID);
    }
}