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
    WorkList indirectNodes;
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
    }

protected:
    virtual void solveWorklist();
    NodeStack& SCCDetect();
    void handleLoadStore(ConstraintNode* node);
    bool mergeSrcToTgt(NodeID nodeId, NodeID newRepId);
    void processAddr(const AddrCGEdge* addr);

    inline void addSccCandidate(NodeID nodeId) {
        sccCandidates.insert(sccRepNode(nodeId));
    }
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

    CGSCC* scc;
    CSC* csc;
    NodeSet sfrObjNodes;
    FieldReps fieldReps;

public:
    AndersenSFR(PTATY type = AndersenSFR_WPA) :
            AndersenSCD(type) {
    }

    /// Create an singleton instance directly instead of invoking llvm pass manager
    static AndersenSFR *createAndersenSFR(SVFModule svfModule) {
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

protected:
    inline void initialize(SVFModule svfModule) {
        resetData();
        /// Build PAG
        PointerAnalysis::initialize(svfModule);
        /// Build Constraint Graph
        consCG = new ConstraintGraph(pag);
        setGraph(consCG);
        if (!scc)
            scc = new CGSCC(consCG);
        csc = new CSC(consCG, scc);
        /// Create statistic class
        stat = new AndersenStat(this);
        consCG->dump("consCG_initial");

        scc->find();
        mergeSccCycle();
    }

    inline const NodeID pwcRep(NodeID nodeId) const {
        return scc->repNode(nodeId);
    }

    void PWCStrideCalculate();
    NodeStack& SCCDetect();
    bool processCopy(NodeID nodeId, const ConstraintEdge* edge);
    bool processGepPts(PointsTo& pts, const GepCGEdge* edge);
    NodeID getSFRCGNode(NodeID init, NodeID baseId, const NodeBS& strides, NodeID dstId);

};


#endif //PROJECT_ANDERSENSFR_H
