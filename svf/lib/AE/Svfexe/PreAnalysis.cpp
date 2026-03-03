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

const PointsTo& PreAnalysis::getPts(NodeID id) const
{
    return pta->getPts(id);
}

const Set<NodeID> PreAnalysis::emptyWidenSet;

void PreAnalysis::buildWidenSets()
{
    for (auto& [func, wto] : funcToWTO)
    {
        for (const ICFGWTOComp* comp : wto->getWTOComponents())
        {
            if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
                buildWidenSetForCycle(cycle);
        }
    }
}

void PreAnalysis::buildWidenSetForCycle(const ICFGCycleWTO* cycle)
{
    const ICFGNode* head = cycle->head()->getICFGNode();
    Set<NodeID>& widenVars = cycleHeadToWidenVars[head];

    // Collect all nodes in cycle body (not including head itself)
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            collectDefsAtNode(singleton->getICFGNode(), widenVars);
        }
        else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            // Recursively handle nested cycles
            buildWidenSetForCycle(subCycle);
            // Also collect nested cycle nodes' defs into parent's widen set
            std::vector<const ICFGNode*> nestedNodes;
            collectCycleNodes(subCycle, nestedNodes);
            for (const ICFGNode* node : nestedNodes)
                collectDefsAtNode(node, widenVars);
        }
    }
    // Also collect defs at cycle head itself
    collectDefsAtNode(head, widenVars);
}

void PreAnalysis::collectCycleNodes(const ICFGCycleWTO* cycle, std::vector<const ICFGNode*>& nodes)
{
    nodes.push_back(cycle->head()->getICFGNode());
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            nodes.push_back(singleton->getICFGNode());
        }
        else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            collectCycleNodes(subCycle, nodes);
        }
    }
}

void PreAnalysis::collectDefsAtNode(const ICFGNode* node, Set<NodeID>& defs)
{
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        // Top-level variable definitions (LHS of assignments)
        if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            // Address-taken: pts(lhs_pointer) are the defined objects
            NodeID ptrId = store->getLHSVarID();
            const PointsTo& pts = pta->getPts(ptrId);
            for (NodeID objId : pts)
                defs.insert(objId);
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            defs.insert(load->getLHSVarID());
        }
        else if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
        {
            defs.insert(copy->getLHSVarID());
        }
        else if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            defs.insert(addr->getLHSVarID());
        }
        else if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            defs.insert(gep->getLHSVarID());
        }
        else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
        {
            defs.insert(phi->getResID());
        }
        else if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        {
            defs.insert(select->getResID());
        }
        else if (const BinaryOPStmt* binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
        {
            defs.insert(binary->getResID());
        }
        else if (const UnaryOPStmt* unary = SVFUtil::dyn_cast<UnaryOPStmt>(stmt))
        {
            defs.insert(unary->getResID());
        }
        else if (const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
        {
            defs.insert(cmp->getResID());
        }
        else if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            defs.insert(callPE->getLHSVarID());
        }
        else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            defs.insert(retPE->getLHSVarID());
        }
    }
}
