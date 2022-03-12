//===- AndersenSFR.cpp -- SFR based field-sensitive Andersen's analysis-------//
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
 * AndersenSFR.cpp
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#include "WPA/AndersenSFR.h"
#include "MemoryModel/PointsTo.h"

using namespace SVF;
using namespace SVFUtil;

AndersenSFR *AndersenSFR::sfrAndersen = nullptr;

/*!
 *
 */
void AndersenSFR::initialize()
{
    AndersenSCD::initialize();
    setPWCOpt(false);

    if (!csc)
        csc = new CSC(_graph, scc);

    // detect and collapse cycles that only comprise copy edges
    getSCCDetector()->find();
    mergeSccCycle();
}


/*!
 * Call the PWC stride calculation method of class CSC.
 */
void AndersenSFR::PWCDetect()
{
    AndersenSCD::PWCDetect();
    csc->find(getSCCDetector()->topoNodeStack());
}


/*!
 *
 */
bool AndersenSFR::mergeSrcToTgt(NodeID nodeId, NodeID newRepId)
{
    ConstraintNode* node = consCG->getConstraintNode(nodeId);
    if (!node->strides.empty())
    {
        ConstraintNode* newRepNode = consCG->getConstraintNode(newRepId);
        newRepNode->strides |= node->strides;
    }
    return AndersenSCD::mergeSrcToTgt(nodeId, newRepId);
}


/*!
 * Propagate point-to set via a gep edge, using SFR
 */
bool AndersenSFR::processGepPts(const PointsTo& pts, const GepCGEdge* edge)
{
    ConstraintNode* dst = edge->getDstNode();
    NodeID dstId = dst->getId();

    if (!dst->strides.empty() && SVFUtil::isa<NormalGepCGEdge>(edge))        // dst is in pwc
    {
        PointsTo tmpDstPts;
        PointsTo srcInits = pts - getPts(dstId);

        if (!srcInits.empty())
        {
            NodeSet sortSrcInits;
            for (NodeID ptd : srcInits)
                sortSrcInits.insert(ptd);

            s32_t offset = SVFUtil::dyn_cast<NormalGepCGEdge>(edge)->getConstantFieldIdx();
            fieldExpand(sortSrcInits, offset, dst->strides, tmpDstPts);
        }

        if (unionPts(dstId, tmpDstPts))
        {
            pushIntoWorklist(dstId);
            return true;
        }
        else
            return false;
    }
    else
        return Andersen::processGepPts(pts, edge);
}


/*!
 *
 */
void AndersenSFR::fieldExpand(NodeSet& initials, s32_t offset, NodeBS& strides, PointsTo& expandPts)
{
    numOfFieldExpand++;

    while (!initials.empty())
    {
        NodeID init = *initials.begin();
        initials.erase(init);

        if (consCG->isBlkObjOrConstantObj(init))
            expandPts.set(init);
        else
        {
            PAGNode* initPN = pag->getGNode(init);
            const MemObj* obj = pag->getBaseObj(init);
            const u32_t maxLimit = obj->getMaxFieldOffsetLimit();
            s32_t initOffset;
            if (GepObjVar *gepNode = SVFUtil::dyn_cast<GepObjVar>(initPN))
                initOffset = gepNode->getConstantFieldIdx();
            else if (SVFUtil::isa<FIObjVar>(initPN) || SVFUtil::isa<DummyObjVar>(initPN))
                initOffset = 0;
            else
                assert(false && "Not an object node!!");

            Set<s32_t> offsets;
            offsets.insert(offset);

            // calculate offsets
            bool loopFlag = true;
            while (loopFlag)
            {
                loopFlag = false;
                for (auto _f : offsets)
                    for (auto _s : strides)
                    {
                        s32_t _f1 = _f + _s;
                        loopFlag = (offsets.find(_f1) == offsets.end()) && ( (u32_t)(initOffset + _f1) < maxLimit);
                        if (loopFlag)
                            offsets.insert(_f1);
                    }
            }

            // get gep objs
            for (s32_t _f : offsets)
            {
                NodeID gepId = consCG->getGepObjVar(init, LocationSet(_f));
                initials.erase(gepId);  // gep id in initials should be removed to avoid redundant derivation
                expandPts.set(gepId);
            }
        }
    }
}
