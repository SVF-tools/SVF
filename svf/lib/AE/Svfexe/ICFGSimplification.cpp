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
#include "AE/Svfexe/ICFGSimplification.h"

namespace SVF
{
void ICFGSimplification::mergeAdjacentNodes(ICFG* icfg)
{
    Map<const SVFBasicBlock*, std::vector<const ICFGNode*>> bbToNodes;
    Set<const ICFGNode*> subNodes;
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
                    bbToNodes[bb].push_back(callNode);
                    subNodes.insert(callNode);

                    const RetICFGNode* retNode = callNode->getRetICFGNode();
                    bbToNodes[bb].push_back(retNode);
                    subNodes.insert(retNode);
                }
                else
                {
                    // For ordinary instructions, we put multiple instructions in an ICFGNode
                    /// e.g.
                    /// entry:
                    ///   %add = add i32 %a, %b    ----> goto branch 1
                    ///   %sub = sub i32 %add, %c  -----> goto branch 2
                    ///   call void @fun()    ----> call and ret has been handled
                    ///   %mul = mul i32 %sub, %d  -----> goto branch 3
                    if (bbToNodes.find(bb) == bbToNodes.end())
                    {
                        // branch 1, for the first node of basic block
                        bbToNodes[bb] = {icfgNode};
                        subNodes.insert(icfgNode);
                    }
                    else
                    {
                        const ICFGNode* pNode = bbToNodes[bb].back();
                        if (!SVFUtil::isa<RetICFGNode>(pNode))
                        {
                            // branch 2, for the middle node of basic block
                            // do nothing if it is not ret node
                        }
                        else
                        {
                            // branch 3, for the node after ret node
                            bbToNodes[bb].push_back(icfgNode);
                            subNodes.insert(icfgNode);
                        }
                    }
                }
            }
        }

        if (const FunEntryICFGNode* funEntryNode = icfg->getFunEntryICFGNode(func))
        {
            if (const SVFBasicBlock* bb = funEntryNode->getBB())
            {
                std::vector<const ICFGNode*>& nodes = bbToNodes[bb];
                nodes.insert(nodes.begin(), funEntryNode);
                subNodes.insert(funEntryNode);
            }
        }
        if (const FunExitICFGNode* funExitNode = icfg->getFunExitICFGNode(func))
        {
            if (const SVFBasicBlock* bb = funExitNode->getBB())
            {
                std::vector<const ICFGNode*>& nodes = bbToNodes[bb];
                nodes.push_back(funExitNode);
                subNodes.insert(funExitNode);
            }
        }
    }

    for (auto &it: subNodes)
    {
        ICFGNode* head = const_cast<ICFGNode*>(it);
        if (head->getOutEdges().size() != 1)
        {
            // if head has more than one out edges, we don't merge any following nodes.
            continue;
        }
        else
        {
            ICFGNode* next = (*head->getOutEdges().begin())->getDstNode();
            // merge the following nodes, until the next subnode
            while (subNodes.find(next) == subNodes.end())
            {
                assert(icfg->getRepNode(next) != head && "should not find a circle here");
                icfg->removeICFGEdge(*head->getOutEdges().begin());
                std::vector<ICFGEdge*> rm_edges;
                // Step 1: merge the out edges of next to head
                for (ICFGEdge* outEdge: next->getOutEdges())
                {
                    rm_edges.push_back(outEdge);
                    ICFGNode* post = outEdge->getDstNode();
                    if (outEdge->isIntraCFGEdge())
                    {
                        IntraCFGEdge* intraEdge = SVFUtil::dyn_cast<IntraCFGEdge>(outEdge);
                        if (intraEdge->getCondition())
                        {
                            icfg->addConditionalIntraEdge(head, post, intraEdge->getCondition(), intraEdge->getSuccessorCondValue());
                        }
                        else
                        {
                            icfg->addIntraEdge(head, post);
                        }
                    }
                }
                // Step 2: update the sub node map and rep node map
                icfg->updateSubAndRep(next, head);
                if (next->getOutEdges().size() == 1)
                {
                    // Step 3: remove the edges from next to its next, since next has been merged to head
                    // if only one out edge, we may continue to merge the next node if it is not a subnode
                    next = (*next->getOutEdges().begin())->getDstNode();
                    for (ICFGEdge* edge: rm_edges)
                        icfg->removeICFGEdge(edge);

                }
                else
                {
                    // if more than one out edges, we don't merge any following nodes.
                    for (ICFGEdge* edge: rm_edges)
                        icfg->removeICFGEdge(edge);
                    break;
                }
            }
        }
    }
}
}