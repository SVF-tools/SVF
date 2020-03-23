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
#include "WPA/CSC.h"


/*!
 * Selective Cycle Detection Based Andersen Analysis
 */
class AndersenSCD : public Andersen {
public:
    typedef llvm::DenseMap<NodeID, NodeID> NodeToNodeMap;

protected:
    static AndersenSCD* scdAndersen;
    NodeSet sccCandidates;
    NodeToNodeMap pwcReps;

public:
    AndersenSCD(PTATY type = AndersenSCD_WPA) :
            Andersen(type) {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenSCD *createAndersenSCD(SVFModule* svfModule) {
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
    inline void addSccCandidate(NodeID nodeId) {
        sccCandidates.insert(sccRepNode(nodeId));
    }

    virtual NodeStack& SCCDetect();
    virtual void PWCDetect();
    virtual void solveWorklist();
    virtual void handleLoadStore(ConstraintNode* node);
    virtual void processAddr(const AddrCGEdge* addr);
    virtual bool addCopyEdge(NodeID src, NodeID dst);
    virtual bool updateCallGraph(const CallSiteToFunPtrMap& callsites);
    virtual void processPWC(ConstraintNode* rep);
    virtual void handleCopyGep(ConstraintNode* node);

};



/*!
 * Selective Cycle Detection with Stride-based Field Representation
 */
class AndersenSFR : public AndersenSCD {
public:
    typedef llvm::DenseMap<NodeID, NodeBS> NodeStrides;
    typedef llvm::DenseMap<NodeID, NodeSet> FieldReps;
    typedef llvm::DenseMap<NodeID, pair<NodeID, NodeSet>> SFRTrait;

private:
    static AndersenSFR* sfrAndersen;

    CSC* csc;
    NodeSet sfrObjNodes;
    FieldReps fieldReps;

public:
    AndersenSFR(PTATY type = AndersenSFR_WPA) :
            AndersenSCD(type), csc(NULL) {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenSFR *createAndersenSFR(SVFModule* svfModule) {
        if (sfrAndersen == nullptr) {
            new AndersenSFR();
            sfrAndersen->analyze(svfModule);
            return sfrAndersen;
        }
        return sfrAndersen;
    }

    static void releaseAndersenSFR() {
        if (sfrAndersen)
            delete sfrAndersen;
    }

    ~AndersenSFR() {
        if (csc != NULL) {
            delete(csc);
            csc = NULL;
        }
    }

protected:
    void initialize(SVFModule* svfModule);
    void PWCDetect();
    void fieldExpand(NodeSet& initials, Size_t offset, NodeBS& strides, PointsTo& expandPts);
    bool processGepPts(PointsTo& pts, const GepCGEdge* edge);
    bool mergeSrcToTgt(NodeID nodeId, NodeID newRepId);

};


#endif //PROJECT_ANDERSENSFR_H
