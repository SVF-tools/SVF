//===- ICFGWrapper.cpp -- ICFG Wrapper-----------------------------------------//
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
 * ICFGWrapper.cpp
 *
 *  Created on: Sep 26, 2023
 *      Author: Xiao Cheng
 */

#include "Graphs/ICFGWrapper.h"

using namespace SVF;
using namespace SVFUtil;

std::unique_ptr<ICFGWrapper> ICFGWrapper::_icfgWrapper = nullptr;

void ICFGWrapper::view()
{
    SVF::ViewGraph(this, "ICFG Wrapper");
}

void ICFGWrapper::dump(const std::string& filename)
{
    GraphPrinter::WriteGraphToFile(SVFUtil::outs(), filename, this);
}

void ICFGWrapper::addICFGNodeWrapperFromICFGNode(const ICFGNode *src)
{

    if (!hasICFGNodeWrapper(src->getId()))
        addICFGNodeWrapper(new ICFGNodeWrapper(src));
    ICFGNodeWrapper *curICFGNodeWrapper = getGNode(src->getId());
    if (isa<FunEntryICFGNode>(src))
    {
        _funcToFunEntry.emplace(src->getFun(), curICFGNodeWrapper);
    }
    else if (isa<FunExitICFGNode>(src))
    {
        _funcToFunExit.emplace(src->getFun(), curICFGNodeWrapper);
    }
    else if (const CallICFGNode *callICFGNode = dyn_cast<CallICFGNode>(src))
    {
        if (!hasICFGNodeWrapper(callICFGNode->getRetICFGNode()->getId()))
            addICFGNodeWrapper(new ICFGNodeWrapper(callICFGNode->getRetICFGNode()));
        if (!curICFGNodeWrapper->getRetICFGNodeWrapper())
            curICFGNodeWrapper->setRetICFGNodeWrapper(getGNode(callICFGNode->getRetICFGNode()->getId()));
    }
    else if (const RetICFGNode *retICFGNode = dyn_cast<RetICFGNode>(src))
    {
        if (!hasICFGNodeWrapper(retICFGNode->getCallICFGNode()->getId()))
            addICFGNodeWrapper(new ICFGNodeWrapper(retICFGNode->getCallICFGNode()));
        if (!curICFGNodeWrapper->getCallICFGNodeWrapper())
            curICFGNodeWrapper->setCallICFGNodeWrapper(getGNode(retICFGNode->getCallICFGNode()->getId()));
    }
    for (const auto &e: src->getOutEdges())
    {
        if (!hasICFGNodeWrapper(e->getDstID()))
            addICFGNodeWrapper(new ICFGNodeWrapper(e->getDstNode()));
        ICFGNodeWrapper *dstNodeWrapper = getGNode(e->getDstID());
        if (!hasICFGEdgeWrapper(curICFGNodeWrapper, dstNodeWrapper, e))
        {
            ICFGEdgeWrapper *pEdge = new ICFGEdgeWrapper(curICFGNodeWrapper, dstNodeWrapper, e);
            addICFGEdgeWrapper(pEdge);
        }
    }
}


void ICFGWrapperBuilder::build(ICFG *icfg)
{
    ICFGWrapper::releaseICFGWrapper();
    const std::unique_ptr<ICFGWrapper> &icfgWrapper = ICFGWrapper::getICFGWrapper(icfg);
    for (const auto &i: *icfg)
    {
        icfgWrapper->addICFGNodeWrapperFromICFGNode(i.second);
    }
}