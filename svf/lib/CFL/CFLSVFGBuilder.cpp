//===- CFLSVFGBuilder.cpp -- Building SVFG for CFL--------------------------//
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

//
// Created by Xiao on 30/12/23.
//
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Graphs/SVFG.h"
#include "Util/Options.h"
#include "CFL/CFLSVFGBuilder.h"


using namespace SVF;
using namespace SVFUtil;

void CFLSVFGBuilder::buildSVFG()
{

    MemSSA* mssa = svfg->getMSSA();
    svfg->buildSVFG();
    BVDataPTAImpl* pta = mssa->getPTA();
    DBOUT(DGENERAL, outs() << pasMsg("\tCollect Global Variables\n"));

    collectGlobals(pta);

    DBOUT(DGENERAL, outs() << pasMsg("\tRemove Dereference Direct SVFG Edge\n"));

    rmDerefDirSVFGEdges(pta);

    rmIncomingEdgeForSUStore(pta);

    DBOUT(DGENERAL, outs() << pasMsg("\tAdd Sink SVFG Nodes\n"));

    AddExtActualParmSVFGNodes(pta->getCallGraph());

    if(pta->printStat())
        svfg->performStat();
}


/*!
 * Remove Incoming Edge for strong-update (SU) store instruction
 * Because the SU node does not receive indirect value
 *
 * e.g.,
 *      L1: *p = O; (singleton)
 *      L2: *p = _; (SU here)
 *      We should remove the indirect value flow L1 -> L2
 *      Because the points-to set of O from L1 does not pass to that after L2
 */
void CFLSVFGBuilder::rmIncomingEdgeForSUStore(BVDataPTAImpl* pta)
{

    for(SVFG::iterator it = svfg->begin(), eit = svfg->end(); it!=eit; ++it)
    {
        const SVFGNode* node = it->second;

        if(const StoreSVFGNode* stmtNode = SVFUtil::dyn_cast<StoreSVFGNode>(node))
        {
            if(SVFUtil::isa<StoreStmt>(stmtNode->getPAGEdge()))
            {
                NodeID singleton;
                if(isStrongUpdate(node, singleton, pta))
                {
                    Set<SVFGEdge*> toRemove;
                    for (SVFGNode::const_iterator it2 = node->InEdgeBegin(), eit2 = node->InEdgeEnd(); it2 != eit2; ++it2)
                    {
                        if ((*it2)->isIndirectVFGEdge())
                        {
                            toRemove.insert(*it2);
                        }
                    }
                    for (SVFGEdge* edge: toRemove)
                    {
                        svfg->removeSVFGEdge(edge);
                    }
                }
            }
        }
    }
}