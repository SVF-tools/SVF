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
    if (LCA == NULL)
    {
        tgtNodes.push_back(succ);
        return;
    }
    if (succ == LCA) return;
    std::vector<const SVFBasicBlock *> path;
    SVFLoopAndDomInfo *ld = const_cast<SVFFunction *>(LCA->getFunction())->getLoopAndDomInfo();
    dfsNodesBetweenPdomNodes(LCA, succ, path, tgtNodes, ld);
}

/*!
 * Start here
 */
void CDGBuilder::build()
{
    if (_controlDG->getTotalNodeNum() > 0)
        return;
    PAG *pag = PAG::getPAG();
    buildControlDependence(pag->getModule());
    buildICFGNodeControlMap();
}


s64_t CDGBuilder::getBBSuccessorBranchID(const SVFBasicBlock *BB, const SVFBasicBlock *Succ)
{
    ICFG *icfg = PAG::getPAG()->getICFG();
    const ICFGNode *pred = icfg->getICFGNode(BB->getTerminator());
    const ICFGEdge *edge = nullptr;
    for (const auto &inst: Succ->getInstructionList())
    {
        if (const ICFGEdge *e = icfg->getICFGEdge(pred, icfg->getICFGNode(inst), ICFGEdge::ICFGEdgeK::IntraCF))
        {
            edge = e;
            break;
        }
    }
    if (const IntraCFGEdge *intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
    {
        return intraEdge->getSuccessorCondValue();
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
void CDGBuilder::buildControlDependence(const SVFModule *svfgModule)
{
    for (const auto &svfFun: *svfgModule)
    {
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
                const SVFBasicBlock *SVFLCA = const_cast<SVFFunction *>(svfFun)->
                                              getLoopAndDomInfo()->findNearestCommonPDominator(pred, succ);
                std::vector<const SVFBasicBlock *> tgtNodes;
                if (SVFLCA == pred) tgtNodes.push_back(SVFLCA);
                // from succ to LCA
                extractNodesBetweenPdomNodes(succ, SVFLCA, tgtNodes);

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
void CDGBuilder::extractBBS(const SVF::SVFFunction *func,
                            Map<const SVF::SVFBasicBlock *, std::vector<const SVFBasicBlock *>> &res)
{
    for (const auto &bb: *func)
    {
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
    ICFG *icfg = PAG::getPAG()->getICFG();
    for (const auto &it: _svfcontrolMap)
    {
        for (const auto &it2: it.second)
        {
            const SVFBasicBlock *controllingBB = it2.first;
            //            const ICFGNode *controlNode = _bbToNode[it.first].first;
            //            if(!controlNode) continue;
            const SVFInstruction *terminator = it.first->getInstructionList().back();
            if (!terminator) continue;
            const ICFGNode *controlNode = icfg->getICFGNode(terminator);
            if (!controlNode) continue;
            // controlNode control at pos
            for (const auto &inst: *controllingBB)
            {
                const ICFGNode *controllee = icfg->getICFGNode(inst);
                _nodeControlMap[controlNode][controllee].insert(it2.second.begin(), it2.second.end());
                _nodeDependentOnMap[controllee][controlNode].insert(it2.second.begin(), it2.second.end());
                for (s32_t pos: it2.second)
                {
                    _controlDG->addCDGEdgeFromSrcDst(controlNode, controllee,
                                                     SVFUtil::dyn_cast<IntraICFGNode>(controlNode)->getInst(),
                                                     pos);
                }
            }
        }
    }
}