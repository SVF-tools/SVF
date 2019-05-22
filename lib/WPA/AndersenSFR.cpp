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

#include "WPA/Andersen.h"
#include "Util/SVFUtil.h"

using namespace SVFUtil;

AndersenSFR* AndersenSFR::sfrAndersen = NULL;


/*!
 * Override Andersen's SCC detection method to satisfy PWC stride calculation.
 */
NodeStack& AndersenSFR::SCCDetect() {
    numOfSCCDetection++;

    double sccStart = stat->getClk();
    scc->find(sccCandidates);
    double sccEnd = stat->getClk();

    timeOfSCCDetection +=  (sccEnd - sccStart)/TIMEINTERVAL;

    double mergeStart = stat->getClk();
    mergeSccCycle();
    double mergeEnd = stat->getClk();

    timeOfSCCMerges +=  (mergeEnd - mergeStart)/TIMEINTERVAL;

    sccStart = stat->getClk();
    PWCStrideCalculate();
    sccEnd = stat->getClk();

    timeOfSCCDetection +=  (sccEnd - sccStart)/TIMEINTERVAL;

    return scc->topoNodeStack();
}

/*!
 * Call the PWC stride calculation method of class CSC.
 */
void AndersenSFR::PWCStrideCalculate() {
    scc->find(sccCandidates, "copyGep");
    csc->find(scc->topoNodeStack());
}

/*!
 * Propagate point-to set via a copy edge, using SFR
 */
bool AndersenSFR::processCopy(NodeID nodeId, const ConstraintEdge* edge) {
    numOfProcessedCopy++;

    assert((SVFUtil::isa<CopyCGEdge>(edge)) && "not copy/call/ret ??");
    PAGNode* node = pag->getPAGNode(nodeId);
    NodeID dst = edge->getDstID();
    bool changed = false;

    if (SVFUtil::isa<GepObjPN>(node))
        for (NodeID sfr : fieldReps[nodeId]) {
            PointsTo& srcPts = getPts(sfr);
            changed = unionPts(dst,srcPts);
        }
    else if (SFRObjPN* sfr = SVFUtil::dyn_cast<SFRObjPN>(node)) {
        NodeSet propedSfr;
        propedSfr.clear();
        for (NodeID field : sfr->fields)
            for (NodeID fRepId : fieldReps[field])
                if (propedSfr.find(fRepId) == propedSfr.end() && fRepId != nodeId) {
                    PointsTo& srcPts = getPts(fRepId);
                    changed = unionPts(dst,srcPts);
                    propedSfr.insert(fRepId);
                }
    }
    PointsTo& srcPts = getPts(nodeId);
    changed = changed || unionPts(dst,srcPts);

    if (changed)
        pushIntoWorklist(dst);
    return changed;
}

/*!
 * Propagate point-to set via a gep edge, using SFR
 */
bool AndersenSFR::processGepPts(PointsTo& pts, const GepCGEdge* edge) {
    numOfProcessedGep++;

    NodeID dstId = edge->getDstID();
    PointsTo tmpDstPts;
    for (PointsTo::iterator piter = pts.begin(), epiter = pts.end(); piter != epiter; ++piter) {
        /// get the object
        NodeID ptd = *piter;
        /// handle blackhole and constant
        if (consCG->isBlkObjOrConstantObj(ptd)) {
            tmpDstPts.set(*piter);
        } else {
            /// handle variant gep edge
            /// If a pointer connected by a variant gep edge,
            /// then set this memory object to be field insensitive
            if (SVFUtil::isa<VariantGepCGEdge>(edge)) {
                if (consCG->isFieldInsensitiveObj(ptd) == false) {
                    consCG->setObjFieldInsensitive(ptd);
                    consCG->addNodeToBeCollapsed(consCG->getBaseObjNode(ptd));
                }
                // add the field-insensitive node into pts.
                NodeID baseId = consCG->getFIObjNode(ptd);
                tmpDstPts.set(baseId);
            }

            else if (const NormalGepCGEdge* normalGepEdge = SVFUtil::dyn_cast<NormalGepCGEdge>(edge)) {
                if (!matchType(edge->getSrcID(), ptd, normalGepEdge))
                    continue;

                NodeStrides& nodeStrides = csc->getNodeStrides();
                if (nodeStrides.find(dstId) != nodeStrides.end()) {
                    // node dst is in PWC
                    NodeBS stride;
                    NodeID init;
                    NodeID base;
                    if (SFRObjPN* sfr = SVFUtil::dyn_cast<SFRObjPN>(pag->getPAGNode(ptd))) {
                        // ptd is an sfr
                        stride = sfr->strides | nodeStrides[dstId];
                        init = sfr->initial + normalGepEdge->getLocationSet().getOffset();
                        base = sfr->baseId;
                    } else {
                        // ptd is not an sfr
                        stride = nodeStrides[dstId];
                        if (SVFUtil::dyn_cast<GepObjPN>(pag->getPAGNode(ptd)))
                            init += normalGepEdge->getLocationSet().getOffset();
                        else
                            init = consCG->getGepObjNode(ptd, normalGepEdge->getLocationSet());
                        base = pag->getBaseObjNode(ptd);
                    }

                    NodeID newSfrId = getSFRCGNode(init, base, stride, dstId);
                    tmpDstPts.set(newSfrId);
                    addTypeForGepObjNode(newSfrId, normalGepEdge);
                } else {
                    // node dst is not in PWC
                    if (SFRObjPN* sfr = SVFUtil::dyn_cast<SFRObjPN>(pag->getPAGNode(ptd))) {
                        // ptd is an sfr
                        NodeBS stride = sfr->strides;
                        NodeID init = sfr->initial + normalGepEdge->getLocationSet().getOffset();
                        NodeID base =sfr->baseId;

                        NodeID newSfrId = getSFRCGNode(init, base, stride, dstId);
                        tmpDstPts.set(newSfrId);
                        addTypeForGepObjNode(newSfrId, normalGepEdge);
                    } else {
                        // ptd is not an sfr
                        NodeID fieldSrcPtdNode = consCG->getGepObjNode(ptd,	normalGepEdge->getLocationSet());
                        tmpDstPts.set(fieldSrcPtdNode);
                        addTypeForGepObjNode(fieldSrcPtdNode, normalGepEdge);
                    }
                }
            } else {
                assert(false && "new gep edge?");
            }
        }
    }

    if (unionPts(dstId, tmpDstPts)) {
        pushIntoWorklist(dstId);
        return true;
    }
    return false;
}

/*!
 * If the required SFR constraint node exists, get its id, else, create one.
 */
NodeID AndersenSFR::getSFRCGNode(NodeID init, NodeID baseId, const NodeBS& stride, NodeID dstId) {
    // if there is one, get it
    for (NodeID sfrId : sfrObjNodes) {
        SFRObjPN* sfr = SVFUtil::dyn_cast<SFRObjPN>(pag->getPAGNode(sfrId));
        if (sfr->initial == init && sfr->strides == stride)
            return sfrId;
    }

    // else, check whether we need to create a new one
    bool newSfr = true;
    PointsTo& dstPts = getPts(dstId);
    for (NodeID dstPtdId : dstPts) {
        if (SFRObjPN* dstPtdSfr = SVFUtil::dyn_cast<SFRObjPN>(pag->getPAGNode(dstPtdId))) {
            NodeBS tmpStride = dstPtdSfr->strides;
            newSfr = fieldReps.find(init) == fieldReps.end() ||
                     fieldReps[init].find(dstPtdId) == fieldReps[init].end() || (tmpStride |= stride);
            if (!newSfr)
                // if not, return
                return dstPtdId;
        }
    }

    // if yes, create one
    // add new SFR pag node
    NodeID newSfrId = pag->addSFRObjNode(init, baseId, stride);
    sfrObjNodes.insert(newSfrId);

    // calculate field expansion
    NodeID maxLimit = pag->getObject(baseId)->getMaxFieldOffsetLimit();
    SFRObjPN *sfrPNode = SVFUtil::dyn_cast<SFRObjPN>(pag->getPAGNode(newSfrId));

    NodeSet fieldExp;
    fieldExp.clear();

    fieldExp.insert(init);
    for (auto _s : stride)
        fieldExp.insert(init + _s);

    bool loopFlag = true;
    while (loopFlag) {
        loopFlag = false;
        for (auto _f : fieldExp)
            for (auto _s : stride) {
                NodeID _f1 = _f + _s;
                loopFlag = (fieldExp.find(_f1) == fieldExp.end()) && (_f1 < maxLimit);
                if (loopFlag) {
                    fieldExp.insert(_f1);
                    fieldReps[_f1].insert(newSfrId);
                }
            }
    }
    sfrPNode->fields = fieldExp;

    // add constraint graph node
    consCG->addConstraintNode(new ConstraintNode(newSfrId), newSfrId);
    return newSfrId;
}