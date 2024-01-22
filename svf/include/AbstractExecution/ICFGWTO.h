//===--------------- ICFGWTO.h -- WTO for ICFG----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * ICFGWTO.h
 *
 * The implementation is based on F. Bourdoncle's paper:
 * "Efficient chaotic iteration strategies with widenings", Formal
 * Methods in Programming and Their Applications, 1993, pages 128-141.
 *
 *  Created on: Jan 22, 2024
 *      Author: Xiao Cheng
 *
 */
#ifndef SVF_ICFGWTO_H
#define SVF_ICFGWTO_H

#include "Graphs/ICFG.h"
#include "Graphs/WTO.h"

namespace SVF
{

class ICFGWTO : public WTO<ICFG>
{
public:
    typedef WTO<ICFG> Base;
    typedef WTOComponentVisitor<ICFG>::WTONodeT ICFGWTONode;

    explicit ICFGWTO(ICFGNode* node, ICFG* graph) : Base(node, graph) {}

    class TailBuilder : public Base::TailBuilder
    {
    public:
        explicit TailBuilder(ICFG* graph,
                             NodeRefToWTOCycleDepthPtr& cycleDepth_table,
                             NodeRefList& tails, const ICFGNode* head,
                             const GraphTWTOCycleDepth& headNesting)
            : Base::TailBuilder(graph, cycleDepth_table, tails, head,
                                headNesting)
        {
        }

        void visit(const ICFGWTONode& node) override
        {
            if (const auto* callNode =
                    SVFUtil::dyn_cast<CallICFGNode>(node.node()))
            {
                const ICFGNode* succ =
                    _graph->getICFGNode(callNode->getRetICFGNode()->getId());
                const GraphTWTOCycleDepth& succNesting = getWTOCycleDepth(succ);
                if (succ != _head && succNesting <= _headWTOCycleDepth)
                {
                    _tails.insert(node.node());
                }
            }
            else
            {
                for (const auto& edge : node.node()->getOutEdges())
                {
                    if (!edge->isIntraCFGEdge())
                        continue;
                    const ICFGNode* succ = edge->getDstNode();
                    const GraphTWTOCycleDepth& succNesting =
                        getWTOCycleDepth(succ);
                    if (succ != _head && succNesting <= _headWTOCycleDepth)
                    {
                        _tails.insert(node.node());
                    }
                }
            }
        }
    };

    /// Create the cycle component for the given node
    const WTOCycleT* component(const ICFGNode* node) override
    {
        WTOComponentRefList partition;
        if (const auto* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
        {
            const ICFGNode* succ =
                _graph->getICFGNode(callNode->getRetICFGNode()->getId());
            if (getCDN(succ) == 0)
            {
                visit(succ, partition);
            }
        }
        else
        {
            for (const auto& edge : node->getOutEdges())
            {
                if (!edge->isIntraCFGEdge())
                    continue;
                const ICFGNode* succ = edge->getDstNode();
                if (getCDN(succ) == 0)
                {
                    visit(succ, partition);
                }
            }
        }
        const WTOCycleT* ptr = newCycle(node, partition);
        headRefToCycle.emplace(node, ptr);
        return ptr;
    }

    CycleDepthNumber visit(const ICFGNode* node,
                           Base::WTOComponentRefList& partition) override
    {
        CycleDepthNumber head(0);
        CycleDepthNumber min(0);
        bool loop;

        push(node);
        _num += CycleDepthNumber(1);
        head = _num;
        setCDN(node, head);
        loop = false;

        if (const auto* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
        {
            const ICFGNode* succ =
                _graph->getICFGNode(callNode->getRetICFGNode()->getId());
            CycleDepthNumber succ_dfn = getCDN(succ);
            if (succ_dfn == CycleDepthNumber(0))
            {
                min = visit(succ, partition);
            }
            else
            {
                min = succ_dfn;
            }
            if (min <= head)
            {
                head = min;
                loop = true;
            }
        }
        else
        {
            for (const auto& edge : node->getOutEdges())
            {
                if (!edge->isIntraCFGEdge())
                    continue;
                const ICFGNode* succ = edge->getDstNode();
                if (succ->getFun() != node->getFun())
                    continue;
                CycleDepthNumber succ_dfn = getCDN(succ);
                if (succ_dfn == CycleDepthNumber(0))
                {
                    min = visit(succ, partition);
                }
                else
                {
                    min = succ_dfn;
                }
                if (min <= head)
                {
                    head = min;
                    loop = true;
                }
            }
        }

        if (head == getCDN(node))
        {
            setCDN(node, UINT_MAX);
            const NodeT* element = pop();
            if (loop)
            {
                while (element != node)
                {
                    setCDN(element, 0);
                    element = pop();
                }
                partition.push_front(component(node));
            }
            else
            {
                partition.push_front(newNode(node));
            }
        }
        return head;
    }

    void buildTails() override
    {
        for (const auto& head : headRefToCycle)
        {
            NodeRefList tails;
            TailBuilder builder(_graph, _nodeToDepth, tails,
                                const_cast<ICFGNode*>(head.first),
                                cycleDepth(head.first));
            for (const auto& it : *head.second)
            {
                it->accept(builder);
            }
            headRefToTails.emplace(head.first, tails);
        }
    }
};
} // namespace SVF

#endif // SVF_ICFGWTO_H
