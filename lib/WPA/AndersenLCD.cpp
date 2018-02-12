//===- AndersenLCD.cpp -- LCD based field-sensitive Andersen's analysis-------//
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
 * AndersenLCD.cpp
 *
 *  Created on: Oct 26, 2013
 *      Author: Yulei Sui
 */

#include "WPA/Andersen.h"
#include "Util/AnalysisUtil.h"

#include <llvm/Support/CommandLine.h> // for tool output file
using namespace llvm;
using namespace analysisUtil;

AndersenLCD* AndersenLCD::lcdAndersen = NULL;


/*!
 * Start constraint solving
 */
void AndersenLCD::processNode(NodeID nodeId) {

    numOfIteration++;
    if(0 == numOfIteration % OnTheFlyIterBudgetForStat) {
        dumpStat();
    }

    ConstraintNode* node = consCG->getConstraintNode(nodeId);

    for (ConstraintNode::const_iterator it = node->outgoingAddrsBegin(), eit =  node->outgoingAddrsEnd(); it != eit;
            ++it) {
        processAddr(cast<AddrCGEdge>(*it));
    }

    for (PointsTo::iterator piter = getPts(nodeId).begin(), epiter = getPts(
                nodeId).end(); piter != epiter; ++piter) {
        NodeID ptd = *piter;
        // handle load
        for (ConstraintNode::const_iterator it = node->outgoingLoadsBegin(), eit = node->outgoingLoadsEnd(); it != eit;
                ++it) {
            if(processLoad(ptd, *it))
                pushIntoWorklist(ptd);
        }

        // handle store
        for (ConstraintNode::const_iterator it = node->incomingStoresBegin(), eit =  node->incomingStoresEnd(); it != eit;
                ++it) {
            if(processStore(ptd, *it))
                pushIntoWorklist((*it)->getSrcID());
        }
    }

    // handle copy, call, return, gep
    bool lcd = false;
    for (ConstraintNode::const_iterator it = node->directOutEdgeBegin(), eit = node->directOutEdgeEnd(); it != eit;
            ++it) {
        if(GepCGEdge* gepEdge = llvm::dyn_cast<GepCGEdge>(*it))
            processGep(nodeId,gepEdge);
        else if(processCopy(nodeId,*it))
            lcd = true;
    }
    if(lcd)
        SCCDetect();

    // update call graph
    updateCallGraph(getIndirectCallsites());
}


/*
 *	src --copy--> dst,
 *	union pts(dst) with pts(src)
 */
bool AndersenLCD::processCopy(NodeID node, const ConstraintEdge* edge) {

    assert((isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    NodeID dst = edge->getDstID();
    PointsTo& srcPts = getPts(node);
    PointsTo& dstPts = getPts(dst);
    /// Lazy cycle detection when points-to of source and destination are identical
    /// and we haven't seen this edge before
    if (srcPts == dstPts) {
        if (!srcPts.empty() && isNewLCDEdge(edge)) {
            return true;
        }
    } else {
        if (unionPts(dst, srcPts))
            pushIntoWorklist(dst);
    }
    return false;
}
