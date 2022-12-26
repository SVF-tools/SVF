//===- PTAStat.cpp -- Base class for statistics in SVF-----------------------//
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
 * PTAStat.cpp
 *
 *  Created on: Oct 13, 2013
 *      Author: Yulei Sui
 */

#include <iomanip>
#include "Graphs/PTACallGraph.h"
#include "Util/PTAStat.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "SVFIR/SVFIR.h"

using namespace SVF;
using namespace std;

PTAStat::PTAStat(PointerAnalysis* p) : SVFStat(), pta(p)
{
}

void PTAStat::performStat()
{
    callgraphStat();

    SVFStat::performStat();

    SVFIR* pag = SVFIR::getPAG();
    for(SVFIR::iterator it = pag->begin(), eit = pag->end(); it!=eit; ++it)
    {
        PAGNode* node = it->second;
        if(SVFUtil::isa<ObjVar>(node))
        {
            if(pta->isLocalVarInRecursiveFun(node->getId()))
            {
                localVarInRecursion.set(node->getId());
            }
        }
    }
    PTNumStatMap["LocalVarInRecur"] = localVarInRecursion.count();
}

void PTAStat::callgraphStat()
{

    PTACallGraph* graph = pta->getPTACallGraph();
    PointerAnalysis::CallGraphSCC* callgraphSCC = new PointerAnalysis::CallGraphSCC(graph);
    callgraphSCC->find();

    unsigned totalNode = 0;
    unsigned totalCycle = 0;
    unsigned nodeInCycle = 0;
    unsigned maxNodeInCycle = 0;
    unsigned totalEdge = 0;
    unsigned edgeInCycle = 0;

    NodeSet sccRepNodeSet;
    PTACallGraph::iterator it = graph->begin();
    PTACallGraph::iterator eit = graph->end();
    for (; it != eit; ++it)
    {
        totalNode++;
        if(callgraphSCC->isInCycle(it->first))
        {
            sccRepNodeSet.insert(callgraphSCC->repNode(it->first));
            nodeInCycle++;
            const NodeBS& subNodes = callgraphSCC->subNodes(it->first);
            if(subNodes.count() > maxNodeInCycle)
                maxNodeInCycle = subNodes.count();
        }

        PTACallGraphNode::const_iterator edgeIt = it->second->InEdgeBegin();
        PTACallGraphNode::const_iterator edgeEit = it->second->InEdgeEnd();
        for (; edgeIt != edgeEit; ++edgeIt)
        {
            PTACallGraphEdge *edge = *edgeIt;
            totalEdge+= edge->getDirectCalls().size() + edge->getIndirectCalls().size();
            if(callgraphSCC->repNode(edge->getSrcID()) == callgraphSCC->repNode(edge->getDstID()))
            {
                edgeInCycle+=edge->getDirectCalls().size() + edge->getIndirectCalls().size();
            }
        }
    }

    totalCycle = sccRepNodeSet.size();

    PTNumStatMap["TotalNode"] = totalNode;
    PTNumStatMap["TotalCycle"] = totalCycle;
    PTNumStatMap["NodeInCycle"] = nodeInCycle;
    PTNumStatMap["MaxNodeInCycle"] = maxNodeInCycle;
    PTNumStatMap["TotalEdge"] = totalEdge;
    PTNumStatMap["CalRetPairInCycle"] = edgeInCycle;

    SVFStat::printStat("CallGraph Stats");

    delete callgraphSCC;
}
