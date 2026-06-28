//===- MTA.h -- Analysis of multithreaded programs-------------//
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

/*
 * MTA.h
 *
 *  Created on: May 14, 2014
 *      Author: Yulei Sui, Peng Di
 *
 * The implementation is based on
 * Yulei Sui, Peng Di, and Jingling Xue. "Sparse Flow-Sensitive Pointer Analysis for Multithreaded Programs".
 * 2016 International Symposium on Code Generation and Optimization (CGO'16)
 */

#ifndef MTA_H_
#define MTA_H_

#include <set>
#include <string>
#include <vector>
#include "SVFIR/SVFValue.h"
#include "MemoryModel/PointsTo.h"

namespace SVF
{

class PointerAnalysis;
class AndersenWaveDiff;
class AndersenBase;
class ThreadCallGraph;
class CallGraph;
class MTAStat;
class TCT;
class MHP;
class LockAnalysis;
class SVFStmt;
class SVFIR;
class ICFGNode;

/*!
 * Base data race detector
 */
class MTA
{

public:
    /// Constructor
    MTA();

    /// Destructor
    virtual ~MTA();


    /// We start the pass here
    virtual bool runOnModule(SVFIR* module);
    /// Compute MHP
    virtual MHP* computeMHP(TCT* tct);
    /// Compute locksets
    virtual LockAnalysis* computeLocksets(TCT* tct);
    /// Run the shared detector and print a race report
    virtual void reportRaces();

    // Not implemented for now
    // void dump(Module &module, MHP *mhp, LockAnalysis *lsa);

    MHP* getMHP()
    {
        return mhp;
    }

    LockAnalysis* getLockAnalysis()
    {
        return lsa;
    }

    /// A race pair: two statements that may race.
    struct RacePair {
        const SVFStmt* stmt1;
        const SVFStmt* stmt2;
        RacePair(const SVFStmt* s1, const SVFStmt* s2) : stmt1(s1), stmt2(s2) {}
        bool operator<(const RacePair& other) const {
            if (stmt1 != other.stmt1) return stmt1 < other.stmt1;
            return stmt2 < other.stmt2;
        }
    };

    /// Shared equivalence-class race detector (used by both MTA::reportRaces and
    /// the SlicedMTA pipeline). Returns the racy statements and fills outRacePairs.
    static std::set<const SVFStmt*> detectRace(
        SVFIR* svfIr, AndersenBase* pta, MHP* mhp, LockAnalysis* lockAnalysis,
        CallGraph* callGraph, std::set<RacePair>& outRacePairs);

    /// Escape/points-to helpers for the shared detector.
    static PointsTo getGlobalObjectVariables(SVFIR* svfIr);
    static PointsTo getPointsToClosure(AndersenBase* pta, const PointsTo& pts);

private:
    /// One occurrence of a memory access under one thread instance.
    struct RaceOccurrence {
        const SVFStmt* stmt;
        const ICFGNode* node;
        bool isStore;
        NodeID tid;
        NodeBS interleav;
        std::string lockSig;
    };

    /// Helpers for the equivalence-class race detector.
    //@{
    static std::string contextSignature(const CallStrCxt& context);
    static std::string lockSignature(LockAnalysis* lockAnalysis, const ICFGNode* node);
    static bool occurrencesRace(MHP* mhp, const RaceOccurrence& first, const RaceOccurrence& second);
    static void commitRacePair(std::set<RacePair>& out,
                               const RaceOccurrence& first, const RaceOccurrence& second);
    //@}

    ThreadCallGraph* tcg;
    std::unique_ptr<TCT> tct;
    std::unique_ptr<MTAStat> stat;
    MHP* mhp;
    LockAnalysis* lsa;
};

} // End namespace SVF

#endif /* MTA_H_ */
