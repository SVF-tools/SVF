//===- SlicedMHP.cpp -- MHP analysis over a sliced ICFG view ------------===//
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
 * SlicedMHP.cpp
 *
 *      Author: Jiawei Yang
 *
 * MHP running on a sliced ICFG view.
 */

#include "MTA/SlicedMHP.h"

#include <SVFIR/SVFIR.h>
#include <SVFIR/SVFVariables.h>  // For ValVar complete definition
#include <WPA/Andersen.h>  // For PointerAnalysis complete definition
#include <Util/SVFUtil.h>
#include <Util/WorkList.h>
#include <Util/Options.h>
#include "MTA/Slicer.h"

using namespace SVF;
using namespace SVFUtil;

namespace SVF
{

SlicedMHP::SlicedMHP(TCT* tct, const SlicedSVFIRView* slicedView)
    : MHP(tct), slicedView(slicedView)
{
    // ICFG view from slicedView if available, otherwise nullptr (full ICFG).
    icfgView = (slicedView != nullptr) ? slicedView->getICFG() : nullptr;
}

SlicedMHP::~SlicedMHP() = default;

// All MHP handlers (including handleJoin, which reads the base ForkJoinAnalysis)
// are inherited; only the traversal hooks below and updateNonCandidateFunInterleaving
// are overridden to walk the slice. Fork/join relationships are program-global, so
// the base full-graph ForkJoinAnalysis is exactly what handleJoin needs.

void SlicedMHP::updateNonCandidateFunInterleaving()
{
    // Use the sliced ThreadCallGraph view if available, else the full CallGraph.
    const CallGraph* cg = nullptr;
    if (slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr) {
        cg = slicedView->getThreadCallGraph()->getOriginalCallGraph();
    }
    if (cg == nullptr) {
        cg = PAG::getPAG()->getCallGraph();
    }

    for (const auto& item : *cg)
    {
        const FunObjVar* fun = item.second->getFunction();
        if (getTCT()->isCandidateFun(fun) || isExtCall(fun))
            continue;

        const ICFGNode* entryNode = getFunEntry(fun);
        if (!hasThreadStmtSet(entryNode))
            continue;

        const MHP::CxtThreadStmtSet& tsSet = getThreadStmtSet(entryNode);

        // Iterate over kept ICFG nodes only, instead of all nodes.
        std::vector<const ICFGNode*> funICFGNodes;
        getFunICFGNodes(fun, funICFGNodes);

        for (const CxtThreadStmt& cts : tsSet)
        {
            const CallStrCxt& curCxt = cts.getContext();
            for (const ICFGNode* curNode : funICFGNodes)
            {
                if (curNode == entryNode)
                    continue;
                CxtThreadStmt newCts(cts.getTid(), curCxt, curNode);
                addInterleavingThread(newCts, cts);
            }
        }
    }
}

void SlicedMHP::getInEdgesOfCallGraphNode(const CallGraphNode* node, std::vector<const CallGraphEdge*>& out) const
{
    SlicedViewAdapter::getInEdgesOfCallGraphNode(slicedView, node, out);
}

const ICFGNode* SlicedMHP::getFunEntry(const FunObjVar* fun) const
{
    return SlicedViewAdapter::getFunEntry(icfgView, fun);
}

void SlicedMHP::getFunICFGNodes(const FunObjVar* fun, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getFunICFGNodes(icfgView, fun, out);
}

void SlicedMHP::getSuccNodes(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::getSuccNodes(icfgView, node, out);
}

void SlicedMHP::projectSeedToKept(const ICFGNode* node, std::vector<const ICFGNode*>& out) const
{
    SlicedViewAdapter::projectSeedToKept(icfgView, node, out);
}

} // namespace SVF

