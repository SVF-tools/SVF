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

void PreAnalysis::collectValVarsAtNode(
    const ICFGNode* node, Set<NodeID>& out) const
{
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        const ValVar* lhs = nullptr;
        if (const AssignStmt* a = SVFUtil::dyn_cast<AssignStmt>(stmt))
            lhs = SVFUtil::dyn_cast<ValVar>(a->getLHSVar());
        else if (const MultiOpndStmt* m = SVFUtil::dyn_cast<MultiOpndStmt>(stmt))
            lhs = m->getRes();
        if (lhs)
            out.insert(lhs->getId());
    }

    // FunEntryICFGNode owns its ArgValVars (formal parameters): they have no
    // defining stmt at the entry — the CallPE that writes them lives on the
    // caller side. For recursive functions whose cycle_head is the FunEntry,
    // we still need these IDs in the cycle ValVar set so widening/narrowing
    // observes them across iterations.
    if (const FunEntryICFGNode* fe = SVFUtil::dyn_cast<FunEntryICFGNode>(node))
    {
        for (const SVFVar* fp : fe->getFormalParms())
        {
            if (const ValVar* v = SVFUtil::dyn_cast<ValVar>(fp))
                out.insert(v->getId());
        }
    }
}

const Set<NodeID>& PreAnalysis::computeCycleValVars(const ICFGCycleWTO* cycle)
{
    auto it = cycleToValVars.find(cycle);
    if (it != cycleToValVars.end())
        return it->second;

    Set<NodeID>& out = cycleToValVars[cycle];

    // ValVars defined at the cycle head itself (e.g. CallICFGNode arguments).
    collectValVarsAtNode(cycle->head()->getICFGNode(), out);

    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            collectValVarsAtNode(singleton->getICFGNode(), out);
        }
        else if (const ICFGCycleWTO* sub = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            // Bottom-up: nested cycle's set is already complete (or we
            // recurse here to fill it), so its ValVars subsume the inner
            // body for the outer cycle.
            const Set<NodeID>& subSet = computeCycleValVars(sub);
            out.insert(subSet.begin(), subSet.end());
        }
    }
    return out;
}

void PreAnalysis::initCycleValVars()
{
    if (Options::AESparsity() != AbstractInterpretation::AESparsity::SemiSparse)
        return;

    // Walk every function's WTO; for each cycle WTO comp (at any depth),
    // build its ValVar set bottom-up via computeCycleValVars.
    for (const auto& kv : funcToWTO)
    {
        const ICFGWTO* wto = kv.second;
        if (!wto) continue;
        FIFOWorkList<const ICFGWTOComp*> worklist;
        for (const ICFGWTOComp* comp : wto->getWTOComponents())
            worklist.push(comp);
        while (!worklist.empty())
        {
            const ICFGWTOComp* comp = worklist.pop();
            if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                computeCycleValVars(cycle);  // recurses bottom-up internally
            }
        }
    }
}
