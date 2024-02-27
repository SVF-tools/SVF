//===- ICFGSimplification.h -- Simplify ICFG----------------------------------//
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
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)
//===----------------------------------------------------------------------===//


//
// Created by Jiawei Wang on 2024/2/25.
//
#include "Graphs/ICFG.h"
#include "AE/Svfexe/SVFIR2ItvExeState.h"


namespace SVF
{

class ICFGSimplification
{
public:
ICFGSimplification() = default;

virtual ~ICFGSimplification() = default;

static void mergeAdjacentNodes(ICFG* icfg)
{
    Map<const SVFBasicBlock*, std::vector<const ICFGNode*>> bbToNodes;
    Set<ICFGNode*> rm_nodes;
    Set<const ICFGNode*> simplifiedNodes;
    for (const auto& func : *PAG::getPAG()->getModule())
    {
        for (const auto& bb : *func)
        {
            for (const auto& inst : *bb)
            {
                if (SVFUtil::isIntrinsicInst(inst))
                    continue;
                const ICFGNode* icfgNode = icfg->getICFGNode(inst);
                // e.g.
                // block: entry
                // insts: call = i32 %fun(i32 %arg)
                // We put the callnode in an icfgnode alone, and the retnode is similar.
                if (const CallICFGNode* callNode =
                        SVFUtil::dyn_cast<CallICFGNode>(icfgNode))
                {
                    icfg->appendSubNode(callNode, callNode);
                    icfg->addRepNode(callNode, callNode);
                    bbToNodes[bb].push_back(callNode);
                    simplifiedNodes.insert(callNode);

                    const RetICFGNode* retNode = callNode->getRetICFGNode();
                    icfg->appendSubNode(retNode, retNode);
                    icfg->addRepNode(retNode, retNode);
                    bbToNodes[bb].push_back(retNode);
                    simplifiedNodes.insert(retNode);
                }
                else
                {
                    // For ordinary instructions, we put multiple instructions in an ICFGNode
                    if (bbToNodes.find(bb) == bbToNodes.end())
                    {
                        icfg->appendSubNode(icfgNode, icfgNode);
                        icfg->addRepNode(icfgNode, icfgNode);
                        bbToNodes[bb] = {icfgNode};
                        simplifiedNodes.insert(icfgNode);
                    }
                    else
                    {
                        const ICFGNode* pNode = bbToNodes[bb].back();
                        if (!SVFUtil::isa<RetICFGNode>(pNode))
                        {
                            icfg->appendSubNode(pNode, icfgNode);
                            icfg->addRepNode(icfgNode, pNode);
                        }
                        else
                        {
                            icfg->appendSubNode(icfgNode, icfgNode);
                            icfg->addRepNode(icfgNode, icfgNode);
                            bbToNodes[bb].push_back(icfgNode);
                            simplifiedNodes.insert(icfgNode);
                        }
                    }
                }
            }
        }

        if (const FunEntryICFGNode* funEntryNode =
                icfg->getFunEntryICFGNode(func))
        {
            if (const SVFBasicBlock* bb = funEntryNode->getBB())
            {
                std::vector<const ICFGNode*>& nodes = bbToNodes[bb];
                icfg->appendSubNode(funEntryNode, funEntryNode);
                icfg->addRepNode(funEntryNode, funEntryNode);
                nodes.insert(nodes.begin(), funEntryNode);
                simplifiedNodes.insert(funEntryNode);
            }
        }
        if (const FunExitICFGNode* funExitNode = icfg->getFunExitICFGNode(func))
        {
            if (const SVFBasicBlock* bb = funExitNode->getBB())
            {
                std::vector<const ICFGNode*>& nodes = bbToNodes[bb];
                icfg->appendSubNode(funExitNode, funExitNode);
                icfg->addRepNode(funExitNode, funExitNode);
                nodes.push_back(funExitNode);
                simplifiedNodes.insert(funExitNode);
            }
        }
    }

    for (auto &it: *icfg) {
         if (simplifiedNodes.find(it.second) == simplifiedNodes.end() &&
             !SVFUtil::isa<GlobalICFGNode>(it.second)) {
             rm_nodes.insert(const_cast<ICFGNode*>(it.second));
         }
    }

    /// reconnect the ICFGNodes,
    /// 1. intraCFGEdges between different basicblocks, but in the safe function e.g. entry:
    ///         cond = icmp eq i32 %a, 0  ---------> srcblk
    ///         br i1 cond, label %bb1, label %bb2 ---------> srcblk_tail
    ///      bb1:
    ///         ... --------> dstblk
    ///      bb2:
    ///         ... --------> another dstblk
    ///   tmpEdge is the edge between srcblk and dstblk, may have condition var (cond is true or false)
    for (const auto& node : *icfg)
    {
        for (const auto& succ : node.second->getOutEdges())
        {
            if (succ->isIntraCFGEdge())
            {
                const SVFFunction* node_fun = node.second->getFun();
                const SVFFunction* succ_fun = succ->getDstNode()->getFun();
                const SVFBasicBlock* node_bb = node.second->getBB();
                const SVFBasicBlock* succ_bb = succ->getDstNode()->getBB();
                if (node_fun == succ_fun)
                {
                    if (node_bb != succ_bb)
                    {
                        ICFGNode* srcblk = const_cast<ICFGNode*>(
                            bbToNodes[node_bb].back());
                        ICFGNode* dstblk = const_cast<ICFGNode*>(
                            bbToNodes[succ_bb].front());
                        ICFGNode* srcblk_tail = const_cast<ICFGNode*>(
                            icfg->getSubNodes(bbToNodes[node_bb].back()).back());
                        ICFGEdge* tmpEdge =
                            icfg->getICFGEdge(srcblk_tail, dstblk,
                                        ICFGEdge::ICFGEdgeK::IntraCF);
                        IntraCFGEdge* pEdge = nullptr;
                        if (tmpEdge)
                        {
                            IntraCFGEdge* intraEdge =
                                SVFUtil::dyn_cast<IntraCFGEdge>(tmpEdge);
                            pEdge = new IntraCFGEdge(srcblk, dstblk);
                            if (intraEdge->getCondition())
                                pEdge->setBranchCondition(
                                    intraEdge->getCondition(),
                                    intraEdge->getSuccessorCondValue());
                        }
                        else
                        {
                            pEdge = new IntraCFGEdge(srcblk, dstblk);
                        }
                        // avoid duplicate edges
                        if (srcblk->hasIncomingEdge(pEdge) ||
                            dstblk->hasIncomingEdge(pEdge))
                            continue;
                        else
                            icfg->addICFGEdge(pEdge);
                    }
                }
            }
        }
    }

    /// 2. intraCFGEdges in the same basicblock, but seperated by callInst
    /// e.g. entry:
    ///         .....  --------> bbNode0
    ///         call void @foo() --------->  callNode: bbNode1, retNode bbNode2
    ///         .....  --------> bbNode3
    /// entry block has 4 bbNodes. the following for loop just links bbNodes in the same basic block.
    for (const auto& bbNodes : bbToNodes)
    {
        for (u32_t i = 0; i < bbNodes.second.size() - 1; ++i)
        {
            ICFGNode* srcblk_tail =
                const_cast<ICFGNode*>(icfg->getSubNodes(bbNodes.second[i]).back());
            ICFGNode* srcblk = const_cast<ICFGNode*>(bbNodes.second[i]);
            ICFGNode* dstblk = const_cast<ICFGNode*>(bbNodes.second[i + 1]);
            if (!icfg->hasIntraICFGEdge(srcblk_tail, dstblk,
                                  ICFGEdge::ICFGEdgeK::IntraCF))
                continue;
            IntraCFGEdge* pEdge = new IntraCFGEdge(srcblk, dstblk);
            if (srcblk->hasIncomingEdge(pEdge) ||
                dstblk->hasIncomingEdge(pEdge))
                continue;
            else
                icfg->addICFGEdge(pEdge);
        }
    }

    /// 3. CallCFGEdges and RetCFGEdges
    /// CallEdge is the edge between CallNode and FunEntryNode.
    /// RetEdge is the edge between FunExitNode and RetNode.

    for (const auto& bbNodes : bbToNodes)
    {
        for (u32_t i = 0; i < bbNodes.second.size(); ++i)
        {
            if (const CallICFGNode* callICFGNode =
                    SVFUtil::dyn_cast<CallICFGNode>(bbNodes.second[i]))
            {
                for (const auto& icfgEdge : callICFGNode->getOutEdges())
                {
                    if (const CallCFGEdge* callEdge =
                            SVFUtil::dyn_cast<CallCFGEdge>(icfgEdge))
                    {
                        ICFGNode* srcblk =
                            const_cast<ICFGNode*>(bbNodes.second[i]);
                        ICFGNode* dstblk =
                            const_cast<ICFGNode*>(callEdge->getDstNode());
                        ICFGEdge* pEdge = new ICFGEdge(
                            srcblk, dstblk, ICFGEdge::ICFGEdgeK::CallCF);
                        if (srcblk->hasIncomingEdge(pEdge) ||
                            dstblk->hasIncomingEdge(pEdge))
                            continue;
                        else
                            icfg->addICFGEdge(pEdge);
                    }
                }
            }
            else if (const RetICFGNode* retICFGNode =
                         SVFUtil::dyn_cast<RetICFGNode>(bbNodes.second[i]))
            {
                for (const auto& icfgEdge : retICFGNode->getInEdges())
                {
                    if (const RetCFGEdge* retEdge =
                            SVFUtil::dyn_cast<RetCFGEdge>(icfgEdge))
                    {
                        if (!retEdge->getSrcNode()->getFun()->hasReturn())
                            continue;
                        ICFGNode* srcblk =
                            const_cast<ICFGNode*>(retEdge->getSrcNode());
                        ICFGNode* dstblk =
                            const_cast<ICFGNode*>(bbNodes.second[i]);
                        ICFGEdge* pEdge = new ICFGEdge(
                            srcblk, dstblk, ICFGEdge::ICFGEdgeK::RetCF);
                        if (srcblk->hasIncomingEdge(pEdge) ||
                            dstblk->hasIncomingEdge(pEdge))
                            continue;
                        else
                            icfg->addICFGEdge(pEdge);
                    }
                }
            }
            else
            {
            }
        }
    }
    for (auto& it : rm_nodes)
    {
        ICFGNode* node = it;
        Set<ICFGEdge*> rm_outedges, rm_inedges;
        for (ICFGEdge* edge : node->getOutEdges())
        {
            rm_outedges.insert(edge);
        }
        for (ICFGEdge* edge : node->getInEdges())
        {
            rm_inedges.insert(edge);
        }
        for (ICFGEdge* edge : rm_outedges)
        {
            node->removeOutgoingEdge(edge);
            edge->getDstNode()->removeIncomingEdge(edge);
        }
        for (ICFGEdge* edge : rm_inedges)
        {
            node->removeIncomingEdge(edge);
            edge->getSrcNode()->removeOutgoingEdge(edge);
        }
    }
}
};
}