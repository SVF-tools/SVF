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
#include "Graphs/CallGraph.h"

namespace SVF
{

typedef WTOComponent<ICFG> ICFGWTOComp;
typedef WTONode<ICFG> ICFGSingletonWTO;
typedef WTOCycle<ICFG> ICFGCycleWTO;

// Added for IWTO
class WTORegion : public WTO<ICFG>
{
public:
    typedef WTO<ICFG> Base;
    typedef WTOComponentVisitor<ICFG>::WTONodeT ICFGWTONode;
    NodeBS &funcPar;
    CallGraph *cg;

    explicit WTORegion(ICFG* graph, const ICFGNode* node, NodeBS & funcPar, CallGraph* cg) :
        Base(graph, node), funcPar(funcPar), cg(cg) {}

    virtual ~WTORegion()
    {
    }

    inline void forEachSuccessor(
        const ICFGNode* node,
        std::function<void(const ICFGNode*)> func) const override
    {
        if (const auto* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
        {

            for (const auto &e : callNode->getOutEdges())
            {
                ICFGNode *calleeEntryICFGNode = e->getDstNode();
                CallGraphNode * calleeCGNode = cg->getCallGraphNode(calleeEntryICFGNode->getFun());

                const ICFGNode* succ = nullptr;
                if (funcPar.test(calleeCGNode->getId()))
                    succ = calleeEntryICFGNode;
                else
                    succ = callNode->getRetICFGNode();

                func(succ);
            }
        }
        else
        {
            for (const auto& e : node->getOutEdges())
            {
                ICFGNode *succ = e->getDstNode();
                CallGraphNode *succCGNode = cg->getCallGraphNode(succ->getFun());
                if (!funcPar.test(succCGNode->getId()))
                    continue;
                func(succ);
            }
        }
    }
};

} // namespace SVF

#endif // SVF_ICFGWTO_H
