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

/// Interprocedural Weak Topological Order
/// Each IWTO has an entry ICFGNode within an function-level SCC boundary. Here scc is one or more functions.
class ICFGWTO : public WTO<ICFG>
{
public:
    typedef WTO<ICFG> Base;
    typedef WTOComponentVisitor<ICFG>::WTONodeT ICFGWTONode;
    Set<const FunObjVar*> scc;

    // 1st argument is the SCC's entry ICFGNode and 2nd argument is the function(s) in this SCC.
    explicit ICFGWTO(const ICFGNode* node, Set<const FunObjVar*> funcScc = {}) :
        Base(node), scc(funcScc)
    {
        if (scc.empty()) // if funcScc is empty, the scc is the function itself
            scc.insert(node->getFun());
    }

    virtual ~ICFGWTO()
    {
    }

    inline virtual std::vector<const ICFGNode*> getSuccessors(const ICFGNode* node) override
    {
        std::vector<const ICFGNode*> successors;

        if (const auto* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
        {

            for (const auto &e : callNode->getOutEdges())
            {
                ICFGNode *calleeEntryICFGNode = e->getDstNode();
                const ICFGNode *succ = nullptr;

                if (scc.find(calleeEntryICFGNode->getFun()) != scc.end()) // caller & callee in the same SCC
                    succ = calleeEntryICFGNode;
                else
                    succ = callNode->getRetICFGNode(); // caller & callee in different SCC

                successors.push_back(succ);
            }
        }
        else
        {
            for (const auto& e : node->getOutEdges())
            {
                ICFGNode *succ = e->getDstNode();
                if (scc.find(succ->getFun()) == scc.end()) // if not in the same SCC, skip
                    continue;
                successors.push_back(succ);
            }
        }

        return successors;
    }
};

} // namespace SVF

#endif // SVF_ICFGWTO_H
