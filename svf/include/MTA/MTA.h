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
 * The base data race detector is based on
 * Yulei Sui, Peng Di, and Jingling Xue. "Sparse Flow-Sensitive Pointer Analysis for Multithreaded Programs".
 * 2016 International Symposium on Code Generation and Optimization (CGO'16)
 *
 * This file also declares SlicedMTA, the multi-stage on-demand program slicing
 * pipeline (MSli) introduced in "Multi-Stage On-Demand Program Slicing for
 * Modular Analysis of Multi-Threaded Programs" (ISSTA 2026).
 */

#ifndef MTA_H_
#define MTA_H_

#include <set>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <utility>
#include "SVFIR/SVFIR.h"
#include "SVFIR/SVFValue.h"
#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFVariables.h"
#include "MemoryModel/PointsTo.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "WPA/Andersen.h"
#include "Graphs/CallGraph.h"

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
// Forward declarations for the SlicedMTA slicing pipeline (see SlicedMTA impl).
class MTASVFGBuilder;
class SVFG;
class FSMPTA;
class PTASlicer;
class MTASlicer;
class SingleSlicer;
class SlicedSVFIRView;
class SlicedTCT;
class SlicedMHP;
class SlicedLockAnalysis;

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

    /// Whether the program has any thread (fork-target) function reachable via a
    /// fork edge.
    static bool hasThreadFunctions(CallGraph* callGraph);

private:
    /// One occurrence of a memory access under one thread instance.
    struct RaceOccurrence {
        const SVFStmt* stmt;
        const ICFGNode* node;
        bool isStore;
        NodeID tid;
        NodeBS interleav;
        bool locked;
    };

    /// Helpers for the equivalence-class race detector.
    //@{
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

/*!
 * Multi-stage on-demand slicing race detection pipeline (MSli).
 *
 * runOnModule drives five stages on a pre-built SVFIR:
 *   1. (caller) build SVFIR + resolve indirect calls into the PAG
 *   2. pre-analysis: Andersen, TCT, MHP, lock, candidate race pairs, VFG_pre
 *   3. MTA slicing: slice the thread-aware graph, build the sliced MHP/lock
 *   4. PTA slicing + main flow-sensitive FSAM (FSMPTA) on the slice
 *   5. final race detection on the sliced graph using FSAM points-to
 *
 * It operates entirely on the SVFIR (LLVM-free). The single LLVM-dependent step --
 * materialising resolved indirect calls into the PAG -- is injected by the caller
 * as a callback (see runOnModule).
 *
 * Behaviour is controlled by Options (MTFlowSensitive, EnableSlicing,
 * SlicingSingle, SlicedDumpDot).
 */
class SlicedMTA
{
public:
    /// The one LLVM-dependent step of the pipeline: resolve the indirect-call
    /// edges discovered by Andersen into PAG copy/call edges. The caller (the
    /// LLVM-aware tool) supplies it via SVFIRBuilder::updateCallGraph.
    using ResolveIndirectCalls = std::function<void(CallGraph*)>;

    /// The shared race detector lives in MTA; reuse its race-pair type.
    using RacePair = MTA::RacePair;

    // Out-of-line (defined where the member types are complete) so callers that
    // only see the forward-declared unique_ptr member types need not be complete.
    SlicedMTA();
    ~SlicedMTA();

    SlicedMTA(const SlicedMTA&) = delete;
    SlicedMTA& operator=(const SlicedMTA&) = delete;

    /// Run the slicing pipeline on a pre-built SVFIR.
    void runOnModule(SVFIR* pag, const ResolveIndirectCalls& resolveIndirectCalls);

private:
    // --- pipeline stages ---
    bool runPreAnalysis(const ResolveIndirectCalls& resolveIndirectCalls);
    bool runMTASlicingAndAnalysis();
    bool runPTASlicingAndAnalysis();
    bool runFinalRaceDetection();
    void buildVFGPre();

    /// No-slice A/B baseline: run the FSAM detection on the whole program (no
    /// slicing), so its time and race set can be compared against the sliced run.
    void runWholeProgramDetection();

    /// Main pointer-analysis instance feeding final race detection (the
    /// flow-sensitive FSAM, a BVDataPTAImpl queried polymorphically).
    BVDataPTAImpl* getMainPTA() const;

    /// Union of both statements of every candidate race pair (the slice targets).
    std::set<const SVFStmt*> getVulnerableStmts() const;

    // --- race detection ---
    /// Re-check the candidate race pairs on the sliced graph using FSAM points-to.
    std::set<RacePair> detectRacePairsOnSlicedGraph(
        BVDataPTAImpl* slicedPTA, MHP* slicedMHP, LockAnalysis* slicedLockAnalysis);

    // Lock analysis over the WHOLE ICFG (real control flow, no bridging) for the
    // final detection's lock signature. The sliced lock analysis walks bridged
    // edges, which fabricate lock-carrying paths the whole program lacks (a
    // query-preservation break); lock-span is control-flow-sensitive, so it must
    // see real edges. Built lazily, cheap (no FSAM/MHP) next to the slicing win.
    LockAnalysis* buildFullLockAnalysis();

    // --- pipeline state (owned unless noted) ---
    SVFIR* svfIr = nullptr;
    std::unique_ptr<TCT> tct;
    std::unique_ptr<MHP> mhp;
    std::unique_ptr<LockAnalysis> lockAnalysis;
    // Inclusion-based Andersen's pre-analysis (a shared singleton, not owned
    // here -- released once in the destructor). Feeds the TCT / MHP / lock /
    // race pre-analysis, the thread-aware VFG_pre, and the main FSMPTA.
    AndersenWaveDiff* preAnder = nullptr;
    std::unique_ptr<MTASVFGBuilder> vfgPreBuilder; // owns vfgPre
    SVFG* vfgPre = nullptr;
    std::unique_ptr<PTASlicer> ptaSlicer;
    std::unique_ptr<MTASlicer> mtaSlicer;
    std::unique_ptr<SingleSlicer> singleSlicer;
    // -mta-slicing-single: the one unified slice, computed in MTA slicing and reused
    // (not recomputed) for PTA slicing so both stages share V_Single.
    std::set<const ICFGNode*> singleSlicedNodes;
    std::unique_ptr<SlicedSVFIRView> mtaSlicedView;
    std::unique_ptr<SlicedSVFIRView> ptaSlicedView;
    std::unique_ptr<FSMPTA> mtaFSMPTA;
    std::unique_ptr<SlicedTCT> slicedTCT;
    std::unique_ptr<SlicedMHP> slicedMhp;
    std::unique_ptr<SlicedLockAnalysis> slicedLockAnalysis;
    // Whole-ICFG lock analysis for the final detection (see buildFullLockAnalysis).
    std::unique_ptr<SlicedSVFIRView> fullLockView;
    std::unique_ptr<SlicedTCT> fullLockTCT;
    std::unique_ptr<SlicedLockAnalysis> fullLockAnalysis;
    bool hasThreadFunctions = false;
    std::set<RacePair> racePairs;
};

} // End namespace SVF

#endif /* MTA_H_ */
