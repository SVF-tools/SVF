//===- AndersenHCD.cpp -- HCD based Field-sensitive Andersen's analysis-------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * AndersenHCD.cpp
 *
 *  Created on: 09 Oct. 2018
 *      Author: Yuxiang Lei
 */

#include "WPA/Andersen.h"
#include "MemoryModel/PointsTo.h"

using namespace SVF;
using namespace SVFUtil;

AndersenHCD *AndersenHCD::hcdAndersen = nullptr;


// --- Methods of AndersenHCD ---

/*!
 * AndersenHCD initilizer,
 * including initilization of SVFIR, constraint graph and offline constraint graph
 */
void AndersenHCD::initialize()
{
    Andersen::initialize();
    // Build offline constraint graph and solve its constraints
    oCG = new OfflineConsG(pag);
    OSCC* oscc = new OSCC(oCG);
    oscc->find();
    oCG->solveOfflineSCC(oscc);
    delete oscc;
}

/*!
 * AndersenHCD worklist solver
 */
void AndersenHCD::solveWorklist()
{
    while (!isWorklistEmpty())
    {
        NodeID nodeId = popFromWorklist();
        collapsePWCNode(nodeId);

        //Merge detected offline SCC cycles
        mergeSCC(nodeId);

        // Keep solving until workList is empty.
        processNode(nodeId);
        collapseFields();
    }
}

/*!
 * Collapse a node to its ref, if the ref exists
 */
void AndersenHCD::mergeSCC(NodeID nodeId)
{
    if (hasOfflineRep(nodeId))
    {
        // get offline rep node
        NodeID oRep = getOfflineRep(nodeId);
        // get online rep node
        NodeID rep = consCG->sccRepNode(oRep);
        const PointsTo &pts = getPts(nodeId);
        for (PointsTo::iterator ptIt = pts.begin(), ptEit = pts.end(); ptIt != ptEit; ++ptIt)
        {
            NodeID tgt = *ptIt;
            ConstraintNode* tgtNode = consCG->getConstraintNode(tgt);
            if (!tgtNode->getDirectInEdges().empty())
                continue;
            if (tgtNode->getAddrOutEdges().size() > 1)
                continue;
            assert(!oCG->isaRef(tgt) && "Point-to target should not be a ref node!");
            mergeNodeAndPts(tgt, rep);
        }
    }
}

/*!
 * Merge node and its pts to the rep node
 */
void AndersenHCD::mergeNodeAndPts(NodeID node, NodeID rep)
{
    node = sccRepNode(node);
    rep = sccRepNode(rep);
    if (!isaMergedNode(node))
    {
        if (unionPts(rep, node))
            pushIntoWorklist(rep);
        // Once a 'Node' is merged to its rep, it is collapsed,
        // only its 'NodeID' remaining in the set 'subNodes' of its rep node.
        mergeNodeToRep(node, rep);
        setMergedNode(node);
    }
}
