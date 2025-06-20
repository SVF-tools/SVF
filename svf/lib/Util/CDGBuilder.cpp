//===----- CDGBuilder.cpp -- Control Dependence Graph Builder -------------//
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
 * CDGBuilder.cpp
 *
 *  Created on: Sep 27, 2023
 *      Author: Xiao Cheng
 */
#include "Util/CDGBuilder.h"
#include "Graphs/CallGraph.h"

using namespace SVF;
using namespace SVFUtil;

void CDGBuilder::dfsNodesBetweenPdomNodes(const SVFBasicBlock *cur,
        const SVFBasicBlock *tgt,
        std::vector<const SVFBasicBlock *> &path,
        std::vector<const SVFBasicBlock *> &tgtNodes,
        SVFLoopAndDomInfo *ld)
{
    path.push_back(cur);
    if (cur == tgt)
    {
        tgtNodes.insert(tgtNodes.end(), path.begin() + 1, path.end());
    }
    else
    {
        auto it = ld->getPostDomTreeMap().find(cur);
        for (const auto &nxt: it->second)
        {
            dfsNodesBetweenPdomNodes(nxt, tgt, path, tgtNodes, ld);
        }
    }
    path.pop_back();
}

/*!
 * (3) extract nodes from succ to the least common ancestor LCA of pred and succ
 *     including LCA if LCA is pred, excluding LCA if LCA is not pred
 * @param succ
 * @param LCA
 * @param tgtNodes
 */
void
CDGBuilder::extractNodesBetweenPdomNodes(const SVFBasicBlock *succ, const SVFBasicBlock *LCA,
        std::vector<const SVFBasicBlock *> &tgtNodes)
{
    if (succ == LCA) return;
    std::vector<const SVFBasicBlock *> path;
    SVFLoopAndDomInfo *ld = const_cast<FunObjVar *>(LCA->getFunction())->getLoopAndDomInfo();
    dfsNodesBetweenPdomNodes(LCA, succ, path, tgtNodes, ld);
}

/*!
 * Start here
 */
void CDGBuilder::build()
{
    if (_controlDG->getTotalNodeNum() > 0)
        return;
    buildControlDependence();
    buildICFGNodeControlMap();
}


s64_t CDGBuilder::getBBSuccessorBranchID(const SVFBasicBlock *BB, const SVFBasicBlock *Succ)
{
    ICFG *icfg = PAG::getPAG()->getICFG();
    assert(!BB->getICFGNodeList().empty() && "empty bb?");
    const ICFGNode *pred = BB->back();
    if (const CallICFGNode* callNode = dyn_cast<CallICFGNode>(pred))
    {
        // not a branch statement:
        //  invoke void %3(ptr noundef nonnull align 8 dereferenceable(8) %1, ptr noundef %2)
        //          to label %invoke.cont1 unwind label %lpad
        pred = callNode->getRetICFGNode();
    }
    const ICFGEdge *edge = icfg->getICFGEdge(pred, Succ->front(), ICFGEdge::ICFGEdgeK::IntraCF);
    if (const IntraCFGEdge *intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
    {
        if(intraEdge->getCondition())
            return intraEdge->getSuccessorCondValue();
        else
            return 0;
    }
    else
    {
        assert(false && "not intra edge?");
        abort();
    }
}

/*!
 * Build control dependence for each function
 *
 * (1) construct CFG for each function
 * (2) extract basic block edges (pred->succ) on the CFG to be processed
 *     succ does not post-dominates pred (!postDT->dominates(succ, pred))
 * (3) extract nodes from succ to the least common ancestor LCA of pred and succ
 *     including LCA if LCA is pred, excluding LCA if LCA is not pred
 * @param svfgModule
 */
void CDGBuilder::buildControlDependence()
{
    const CallGraph* svfirCallGraph = PAG::getPAG()->getCallGraph();
    for (const auto& item: *svfirCallGraph)
    {
        const FunObjVar *svfFun = (item.second)->getFunction();
        if (SVFUtil::isExtCall(svfFun)) continue;
        // extract basic block edges to be processed
        Map<const SVFBasicBlock *, std::vector<const SVFBasicBlock *>> BBS;
        extractBBS(svfFun, BBS);

        for (const auto &item: BBS)
        {
            const SVFBasicBlock *pred = item.first;
            // for each bb pair
            for (const SVFBasicBlock *succ: item.second)
            {
                const SVFBasicBlock *SVFLCA = const_cast<FunObjVar *>(svfFun)->
                                              getLoopAndDomInfo()->findNearestCommonPDominator(pred, succ);
                std::vector<const SVFBasicBlock *> tgtNodes;
                // no common ancestor, may be exit()
                if (SVFLCA == NULL)
                    tgtNodes.push_back(succ);
                else
                {
                    if (SVFLCA == pred) tgtNodes.push_back(SVFLCA);
                    // from succ to LCA
                    extractNodesBetweenPdomNodes(succ, SVFLCA, tgtNodes);
                }

                s64_t pos = getBBSuccessorBranchID(pred, succ);
                for (const SVFBasicBlock *bb: tgtNodes)
                {
                    updateMap(pred, bb, pos);
                }
            }
        }
    }
}


/*!
 * (2) extract basic block edges on the CFG (pred->succ) to be processed
 * succ does not post-dominates pred (!postDT->dominates(succ, pred))
 * @param func
 * @param res
 */
void CDGBuilder::extractBBS(const SVF::FunObjVar *func,
                            Map<const SVF::SVFBasicBlock *, std::vector<const SVFBasicBlock *>> &res)
{
    for (const auto &it: *func)
    {
        const SVFBasicBlock* bb = it.second;
        for (const auto &succ: bb->getSuccessors())
        {
            if (func->postDominate(succ, bb))
                continue;
            res[bb].push_back(succ);
        }
    }
}

/*!
 * Build map at ICFG node level
 */
void CDGBuilder::buildICFGNodeControlMap()
{
    for (const auto &it: _svfcontrolMap)
    {
        for (const auto &it2: it.second)
        {
            const SVFBasicBlock *controllingBB = it2.first;
            const ICFGNode *controlNode = it.first->getICFGNodeList().back();
            if (const CallICFGNode* callNode =
                        SVFUtil::dyn_cast<CallICFGNode>(controlNode))
            {
                // not a branch statement:
                //  invoke void %3(ptr noundef nonnull align 8 dereferenceable(8) %1, ptr noundef %2)
                //          to label %invoke.cont1 unwind label %lpad
                controlNode = callNode->getRetICFGNode();
            }
            if (!controlNode) continue;
            // controlNode control at pos
            for (const auto &controllee: controllingBB->getICFGNodeList())
            {
                _nodeControlMap[controlNode][controllee].insert(it2.second.begin(), it2.second.end());
                _nodeDependentOnMap[controllee][controlNode].insert(it2.second.begin(), it2.second.end());
                for (s32_t pos: it2.second)
                {
                    if (const IntraICFGNode* intraNode =
                                dyn_cast<IntraICFGNode>(controlNode))
                    {
                        assert(intraNode->getSVFStmts().size() == 1 &&
                               "not a branch stmt?");
                        const SVFVar* condition =
                            SVFUtil::cast<BranchStmt>(
                                intraNode->getSVFStmts().front())
                            ->getCondition();
                        _controlDG->addCDGEdgeFromSrcDst(controlNode, controllee,
                                                         condition,
                                                         pos);
                    }
                    else
                    {
                        // not a branch statement:
                        //  invoke void %3(ptr noundef nonnull align 8 dereferenceable(8) %1, ptr noundef %2)
                        //          to label %invoke.cont1 unwind label %lpad
                        SVFIR* pag = PAG::getPAG();
                        _controlDG->addCDGEdgeFromSrcDst(
                            controlNode, controllee,
                            pag->getGNode(pag->getNullPtr()), pos);
                    }

                }
            }
        }
    }
}