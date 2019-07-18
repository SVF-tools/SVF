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
    }

protected:
    virtual NodeStack& SCCDetect();
    virtual void solveWorklist();
    void handleLoadStore(ConstraintNode* node);
    void processAddr(const AddrCGEdge* addr);
    bool addCopyEdge(NodeID src, NodeID dst);

    virtual void processPWC(NodeID nodeId) {};

    inline void addSccCandidate(NodeID nodeId) {
        sccCandidates.insert(sccRepNode(nodeId));
    }

};


#endif //PROJECT_ANDERSENSFR_H
