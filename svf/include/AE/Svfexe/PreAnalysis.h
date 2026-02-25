//===- PreAnalysis.h -- Pre-Analysis for Abstract Interpretation----------//
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
 * PreAnalysis.h
 *
 *  Created on: Feb 25, 2026
 *      Author: Jiawei Wang
 *
 * This file provides a pre-analysis phase for Abstract Interpretation.
 * It runs Andersen's pointer analysis and builds WTO (Weak Topological Order)
 * for each function before the main abstract interpretation.
 */

#ifndef INCLUDE_AE_SVFEXE_PREANALYSIS_H_
#define INCLUDE_AE_SVFEXE_PREANALYSIS_H_

#include "SVFIR/SVFIR.h"
#include "Graphs/ICFG.h"
#include "Graphs/CallGraph.h"
#include "Graphs/SCC.h"
#include "AE/Core/ICFGWTO.h"
#include "WPA/Andersen.h"

namespace SVF
{

class PreAnalysis
{
public:
    typedef SCCDetection<CallGraph*> CallGraphSCC;

    PreAnalysis(SVFIR* pag, ICFG* icfg, CallGraph* cg)
        : svfir(pag), icfg(icfg), callGraph(cg) {}

    virtual ~PreAnalysis()
    {
        for (auto& [func, wto] : funcToWTO)
            delete wto;
    }

    /// Run Andersen's and build WTO for each function
    void initWTO()
    {
        AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(svfir);
        CallGraphSCC* callGraphScc = ander->getCallGraphSCC();
        callGraphScc->find();

        for (auto it = callGraph->begin(); it != callGraph->end(); it++)
        {
            if (callGraphScc->isInCycle(it->second->getId()))
                recursiveFuns.insert(it->second->getFunction());

            const FunObjVar *fun = it->second->getFunction();
            if (fun->isDeclaration())
                continue;

            NodeID repNodeId = callGraphScc->repNode(it->second->getId());
            auto cgSCCNodes = callGraphScc->subNodes(repNodeId);

            bool isEntry = false;
            if (it->second->getInEdges().empty())
                isEntry = true;
            for (auto inEdge: it->second->getInEdges())
            {
                NodeID srcNodeId = inEdge->getSrcID();
                if (!cgSCCNodes.test(srcNodeId))
                {
                    isEntry = true;
                    const CallICFGNode *callSite = nullptr;
                    if (inEdge->isDirectCallEdge())
                        callSite = *(inEdge->getDirectCalls().begin());
                    else if (inEdge->isIndirectCallEdge())
                        callSite = *(inEdge->getIndirectCalls().begin());
                    else
                        assert(false && "CallGraphEdge must "
                               "be either direct or indirect!");

                    nonRecursiveCallSites.insert(
                    {callSite, inEdge->getDstNode()->getFunction()->getId()});
                }
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

        // Build cycleHeadToCycle map
        for (auto& [func, wto] : funcToWTO)
        {
            collectCycleHeads(wto->getWTOComponents());
        }
    }

    /// Accessors for WTO data
    const Map<const FunObjVar*, const ICFGWTO*>& getFuncToWTO() const
    {
        return funcToWTO;
    }

    const Set<std::pair<const CallICFGNode*, NodeID>>& getNonRecursiveCallSites() const
    {
        return nonRecursiveCallSites;
    }

    const Set<const FunObjVar*>& getRecursiveFuns() const
    {
        return recursiveFuns;
    }

    const Map<const ICFGNode*, const ICFGCycleWTO*>& getCycleHeadToCycle() const
    {
        return cycleHeadToCycle;
    }

private:
    void collectCycleHeads(const std::list<const ICFGWTOComp*>& comps)
    {
        for (const ICFGWTOComp* comp : comps)
        {
            if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                cycleHeadToCycle[cycle->head()->getICFGNode()] = cycle;
                collectCycleHeads(cycle->getWTOComponents());
            }
        }
    }

    SVFIR* svfir;
    ICFG* icfg;
    CallGraph* callGraph;

    Map<const FunObjVar*, const ICFGWTO*> funcToWTO;
    Set<std::pair<const CallICFGNode*, NodeID>> nonRecursiveCallSites;
    Set<const FunObjVar*> recursiveFuns;
    Map<const ICFGNode*, const ICFGCycleWTO*> cycleHeadToCycle;
};

} // End namespace SVF

#endif /* INCLUDE_AE_SVFEXE_PREANALYSIS_H_ */
