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
#include "Util/SVFUtil.h"

using namespace SVFUtil;

AndersenHCD *AndersenHCD::hcdAndersen = nullptr;


// --- Methods of AndersenHCD ---

/*!
 * AndersenHCD initilizer,
 * including initilization of PAG, constraint graph and offline constraint graph
 */
void AndersenHCD::initialize(SVFModule svfModule) {
    resetData();
    // Build PAG
    PointerAnalysis::initialize(svfModule);
    // Build constraint graph
    consCG = new ConstraintGraph(pag);
    setGraph(consCG);

    // Build offline constraint graph and solve its constraints
    oCG = new OfflineConsG(pag);
    oCG->solveOCG();

    // Create statistic class
    stat = new AndersenStat(this);
    consCG->dump("consCG_initial");
}

/*!
 * AndersenHCD worklist solver
 */
void AndersenHCD::solveWorklist() {
    while (!isWorklistEmpty()) {
        NodeID nodeId = popFromWorklist();
        collapsePWCNode(nodeId);

        //TODO: Merge detected SCC cycles
        mergeOfflineSCC(nodeId);

        // Keep solving until workList is empty.
        processNode(nodeId);
        collapseFields();
    }
}

/*!
 * Collapse nodes and fields based on the result of offline SCC detection
 */
void AndersenHCD::mergeOfflineSCC(NodeID nodeId) {
    if (hasOfflineRep(nodeId)) {
        // get offline rep node
        NodeID oRep = getOfflineRep(nodeId);
        // get online rep node
        NodeID rep = consCG->sccRepNode(oRep);
        PointsTo &pts = getPts(nodeId);
        for (PointsTo::iterator ptIt = pts.begin(), ptEit = pts.end(); ptIt != ptEit; ++ptIt) {
            NodeID tgt = *ptIt;
            assert(!oCG->isaRef(tgt) && "Point-to target should not be a ref node!");
            // TODO: need to merge pts & merge node ?
            mergeNodeAndPts(tgt, rep);
        }
    }
}

/*!
 * Merge node and its pts to the rep node
 */
void AndersenHCD::mergeNodeAndPts(NodeID node, NodeID rep) {
    bool changed = unionPts(rep, node);
    if (changed)
        pushIntoWorklist(rep);
//    if (consCG->getConstraintNode(node))
    mergeNodeToRep(node, rep);
}