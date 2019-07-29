//===- AndersenSFR.h -- SFR based field-sensitive Andersen's analysis-------------//
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
 * AndersenSFR.h
 *
 *  Created on: 09, Feb, 2019
 *      Author: Yuxiang Lei
 */

#ifndef PROJECT_ANDERSENSFR_H
#define PROJECT_ANDERSENSFR_H


#include "WPA/Andersen.h"


/*!
 * Selective Cycle Detection Based Andersen Analysis
 */
class AndersenSCD : public Andersen {
protected:
    static AndersenSCD* scdAndersen;
    NodeSet sccCandidates;

public:
    AndersenSCD(PTATY type = AndersenSCD_WPA) :
            Andersen(type) {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenSCD *createAndersenSCD(SVFModule svfModule) {
        if (scdAndersen == nullptr) {
            new AndersenSCD();
            scdAndersen->analyze(svfModule);
            return scdAndersen;
        }
        return scdAndersen;
    }

    static void releaseAndersenSCD() {
        if (scdAndersen)
            delete scdAndersen;
        scdAndersen = NULL;
    }

protected:
    virtual NodeStack& SCCDetect();
    virtual void solveWorklist();
    virtual void handleLoadStore(ConstraintNode* node);
    virtual void processAddr(const AddrCGEdge* addr);
    virtual bool addCopyEdge(NodeID src, NodeID dst);
    virtual bool updateCallGraph(const CallSiteToFunPtrMap& callsites);

    virtual void processPWC(NodeID nodeId) {};

    inline void addSccCandidate(NodeID nodeId) {
        sccCandidates.insert(sccRepNode(nodeId));
    }

};



/*!
 * Differential-selective Cycle Detection based Andersen analysis
 */
class AndersenDSCD : public AndersenSCD {
private:
    static AndersenDSCD* scdDiff; // static instance

public:
    AndersenDSCD(PTATY type = AndersenDSCD_WPA): AndersenSCD(type) {}

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenDSCD* createAndersenSCDDiff(SVFModule svfModule) {
        if(scdDiff==NULL) {
            scdDiff = new AndersenDSCD();
            scdDiff->analyze(svfModule);
            return scdDiff;
        }
        return scdDiff;
    }
    static void releaseAndersenSCDDiff() {
        if (scdDiff)
            delete scdDiff;
        scdDiff = NULL;
    }

private:
    PointsTo & getCachePts(const ConstraintEdge* edge) {
        EdgeID edgeId = edge->getEdgeID();
        return getDiffPTDataTy()->getCachePts(edgeId);
    }

    /// Handle diff points-to set.
    virtual inline void computeDiffPts(NodeID id) {
        NodeID rep = sccRepNode(id);
        getDiffPTDataTy()->computeDiffPts(rep, getDiffPTDataTy()->getPts(rep));
    }
    virtual inline PointsTo& getDiffPts(NodeID id) {
        NodeID rep = sccRepNode(id);
        return getDiffPTDataTy()->getDiffPts(rep);
    }

    /// Handle propagated points-to set.
    inline void updatePropaPts(NodeID dstId, NodeID srcId) {
        NodeID srcRep = sccRepNode(srcId);
        NodeID dstRep = sccRepNode(dstId);
        getDiffPTDataTy()->updatePropaPtsMap(srcRep, dstRep);
    }
    inline void clearPropaPts(NodeID src) {
        NodeID rep = sccRepNode(src);
        getDiffPTDataTy()->clearPropaPts(rep);
    }

public:
    void handleCopyGep(ConstraintNode* node);
    bool processCopy(NodeID node, const ConstraintEdge* edge);
    bool processGep(NodeID node, const GepCGEdge* edge);
    bool addCopyEdge(NodeID src, NodeID dst);
    bool mergeSrcToTgt(NodeID nodeId, NodeID newRepId);

};


#endif //PROJECT_ANDERSENSFR_H
