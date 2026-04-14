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
#include "AE/Svfexe/AbstractInterpretation.h"
#include "Util/Options.h"
#include "Util/WorkList.h"

using namespace SVF;

PreAnalysis::PreAnalysis(SVFIR* pag, ICFG* icfg)
    : svfir(pag), icfg(icfg), pta(nullptr), callGraph(nullptr), callGraphSCC(nullptr)
{
    pta = AndersenWaveDiff::createAndersenWaveDiff(svfir);
    callGraph = pta->getCallGraph();
    callGraphSCC = pta->getCallGraphSCC();
}

PreAnalysis::~PreAnalysis()
{
    for (auto& [func, wto] : funcToWTO)
        delete wto;
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

// ---------------------------------------------------------------------------
// Semi-sparse cycle ValVar set precomputation
// ---------------------------------------------------------------------------
//
// In semi-sparse mode, ValVars live at their def-sites and do not flow
// through the cycle-head merge. handleLoopOrRecursion uses the cycle's
// ValVar set to gather/scatter ValVars around each widening iteration.
// We precompute the set once per cycle here so the main loop does no work.

void PreAnalysis::initCycleValVars()
{
    if (Options::AESparsity() != AbstractInterpretation::AESparsity::SemiSparse)
        return;

    // Step 1: Collect all cycles in top-down order using a stack-based DFS,
    // then reverse to get bottom-up order (inner cycles before outer ones).
    std::vector<const ICFGCycleWTO*> cycles;
    {
        std::vector<const ICFGWTOComp*> stack;
        for (const auto& kv : funcToWTO)
        {
            if (!kv.second) continue;
            for (const ICFGWTOComp* comp : kv.second->getWTOComponents())
                stack.push_back(comp);
        }
        while (!stack.empty())
        {
            const ICFGWTOComp* comp = stack.back();
            stack.pop_back();
            if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                for (const ICFGWTOComp* sub : cycle->getWTOComponents())
                    stack.push_back(sub);
                cycles.push_back(cycle);
            }
        }
        std::reverse(cycles.begin(), cycles.end());
    }

    // Step 2: Build ValVar sets bottom-up.  Inner cycles are already in the
    // map when we reach their enclosing cycle.
    for (const ICFGCycleWTO* cycle : cycles)
    {
        Set<const ValVar*>& out = cycleToValVars[cycle];

        // Gather every ICFG node in this cycle (head + body singletons).
        // For nested sub-cycles, merge their already-computed sets instead.
        std::vector<const ICFGNode*> nodes;
        nodes.push_back(cycle->head()->getICFGNode());
        for (const ICFGWTOComp* comp : cycle->getWTOComponents())
        {
            if (const ICFGSingletonWTO* s = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
                nodes.push_back(s->getICFGNode());
            else if (const ICFGCycleWTO* sub = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
                out.insert(cycleToValVars[sub].begin(), cycleToValVars[sub].end());
        }

        // For each node, collect LHS ValVar IDs from its statements.
        for (const ICFGNode* node : nodes)
        {
            for (const SVFStmt* stmt : node->getSVFStmts())
            {
                const ValVar* lhs = nullptr;
                if (const AssignStmt* a = SVFUtil::dyn_cast<AssignStmt>(stmt))
                    lhs = SVFUtil::dyn_cast<ValVar>(a->getLHSVar());
                else if (const MultiOpndStmt* m = SVFUtil::dyn_cast<MultiOpndStmt>(stmt))
                    lhs = m->getRes();
                if (lhs)
                    out.insert(lhs);
            }
            // FunEntryICFGNode owns ArgValVars (formal parameters) that have
            // no defining stmt at the entry — the CallPE lives on the caller
            // side.  For recursive cycles whose head is a FunEntry, we need
            // these IDs so widening/narrowing observes them across iterations.
            if (const FunEntryICFGNode* fe = SVFUtil::dyn_cast<FunEntryICFGNode>(node))
            {
                for (const SVFVar* fp : fe->getFormalParms())
                    if (const ValVar* v = SVFUtil::dyn_cast<ValVar>(fp))
                        out.insert(v);
            }
        }
    }
}
