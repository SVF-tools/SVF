//===- PreAnalysis.cpp -- Pre-Analysis for Abstract Interpretation---------//
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
 * PreAnalysis.cpp
 *
 *  Created on: Feb 25, 2026
 *      Author: Jiawei Wang
 */

#include "AE/Svfexe/PreAnalysis.h"

using namespace SVF;

PreAnalysis::PreAnalysis(SVFIR* pag, ICFG* icfg)
    : svfir(pag), icfg(icfg)
{
    pta = AndersenWaveDiff::createAndersenWaveDiff(svfir);
    callGraph = pta->getCallGraph();
    callGraphSCC = pta->getCallGraphSCC();
}

PreAnalysis::~PreAnalysis()
{
    for (auto& [func, wto] : funcToWTO)
        delete wto;
    delete defUseTable;
}

void PreAnalysis::initWTO()
{
    callGraphSCC->find();

    for (auto it = callGraph->begin(); it != callGraph->end(); it++)
    {
        const FunObjVar *fun = it->second->getFunction();
        if (fun->isDeclaration())
            continue;

        NodeID repNodeId = callGraphSCC->repNode(it->second->getId());
        auto cgSCCNodes = callGraphSCC->subNodes(repNodeId);

        bool isEntry = false;
        if (it->second->getInEdges().empty())
            isEntry = true;
        for (auto inEdge: it->second->getInEdges())
        {
            NodeID srcNodeId = inEdge->getSrcID();
            if (!cgSCCNodes.test(srcNodeId))
                isEntry = true;
        }

        if (isEntry)
        {
            Set<const FunObjVar*> funcScc;
            for (const auto& node: cgSCCNodes)
            {
                funcScc.insert(callGraph->getGNode(node)->getFunction());
            }
            ICFGWTO* iwto = new ICFGWTO(icfg->getFunEntryICFGNode(fun), funcScc);
            iwto->init();
            funcToWTO[it->second->getFunction()] = iwto;
        }
    }
}

void PreAnalysis::buildDefUseTable(ICFG* icfg)
{
    if (!pta || !icfg)
        return;
    defUseTable = new SparseDefUse(svfir, pta);
    defUseTable->build(icfg);
}

const PointsTo& PreAnalysis::getPts(NodeID id) const
{
    return pta->getPts(id);
}
