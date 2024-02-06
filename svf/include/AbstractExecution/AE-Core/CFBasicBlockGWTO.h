//===- CFBasicBlockGWTO.h -- WTO for CFBasicBlockGraph----------------------//
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
 * CFBasicBlockGWTO.h
 *
 * The implementation is based on F. Bourdoncle's paper:
 * "Efficient chaotic iteration strategies with widenings", Formal
 * Methods in Programming and Their Applications, 1993, pages 128-141.
 *
 *  Created on: Jan 22, 2024
 *      Author: Xiao Cheng
 *
 */
#ifndef SVF_CFBASICBLOCKGWTO_H
#define SVF_CFBASICBLOCKGWTO_H
#include "Graphs/CFBasicBlockG.h"
#include "Graphs/WTO.h"

namespace SVF
{
typedef WTOComponent<CFBasicBlockGraph> CFBasicBlockGWTOComp;
typedef WTONode<CFBasicBlockGraph> CFBasicBlockGWTONode;
typedef WTOCycle<CFBasicBlockGraph> CFBasicBlockGWTOCycle;

class CFBasicBlockGWTO : public WTO<CFBasicBlockGraph>
{
public:
    typedef WTO<CFBasicBlockGraph> Base;
    typedef WTOComponentVisitor<CFBasicBlockGraph>::WTONodeT
    CFBasicBlockGWTONode;

    explicit CFBasicBlockGWTO(CFBasicBlockGraph* graph,
                              const CFBasicBlockNode* node)
        : Base(graph, node)
    {
    }

    inline void forEachSuccessor(
        const CFBasicBlockNode* node,
        std::function<void(const CFBasicBlockNode*)> func) const override
    {
        if (const auto* callNode =
                    SVFUtil::dyn_cast<CallICFGNode>(node->getICFGNodes().front()))
        {
            const CFBasicBlockNode* succ = _graph->getCFBasicBlockNode(
                                               callNode->getRetICFGNode()->getId());
            func(succ);
        }
        else
        {
            for (const auto& e : node->getOutEdges())
            {
                if (e->getICFGEdge() &&
                        (!e->getICFGEdge()->isIntraCFGEdge() ||
                         node->getFunction() != e->getDstNode()->getFunction()))
                    continue;
                func(e->getDstNode());
            }
        }
    }
};
} // namespace SVF
#endif // SVF_CFBASICBLOCKGWTO_H
