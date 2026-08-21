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
 */

#ifndef MTA_H_
#define MTA_H_

#include <set>
#include <string>
#include <vector>
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
class FlowSensitive;
class SlicedSVFGView;
class MultiStageSlicer;
class SingleSlicer;
class SlicedSVFIRView;
class SlicedTCT;

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
        RacePair(const SVFStmt* s1, const SVFStmt* s2)
            : stmt1(statementLess(s2, s1) ? s2 : s1),
              stmt2(statementLess(s2, s1) ? s1 : s2)
        {
        }

        static bool statementLess(const SVFStmt* lhs, const SVFStmt* rhs)
        {
            return lhs->getEdgeID() < rhs->getEdgeID();
        }

        bool operator<(const RacePair& other) const {
            if (stmt1->getEdgeID() != other.stmt1->getEdgeID())
                return stmt1->getEdgeID() < other.stmt1->getEdgeID();
            return stmt2->getEdgeID() < other.stmt2->getEdgeID();
        }
    };

    /// Shared equivalence-class race detector (used by both MTA::reportRaces and
    /// the SlicedMTA pipeline). Returns the racy statements and fills outRacePairs.
    static std::set<const SVFStmt*> detectRace(
        SVFIR* svfir, AndersenBase* pta, MHP* mhp, LockAnalysis* lockAnalysis,
        CallGraph* callGraph, std::set<RacePair>& outRacePairs);

    /// Escape/points-to helpers for the shared detector.
    static PointsTo getGlobalObjectVariables(SVFIR* svfir);
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
        const NodeBS* interleaving;
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
 * It operates entirely on the SVFIR (LLVM-free). The LLVM-aware caller runs the
 * Andersen pre-analysis and materialises its resolved indirect calls into the
 * PAG before invoking runOnModule.
 *
 * Behaviour is controlled by Options (MTFlowSensitive, MTAEnableSlicing,
 * MTASingleStageSlicing, DumpMTAGraphs).
 */
class SlicedMTA
{
public:
    /// The shared race detector lives in MTA; reuse its race-pair type.
    using RacePair = MTA::RacePair;

    // Out-of-line (defined where the member types are complete) so callers that
    // only see the forward-declared unique_ptr member types need not be complete.
    SlicedMTA();
    ~SlicedMTA();

    SlicedMTA(const SlicedMTA&) = delete;
    SlicedMTA& operator=(const SlicedMTA&) = delete;

    /// Run the slicing pipeline with its prepared Andersen pre-analysis.
    bool runOnModule(SVFIR* pag, AndersenWaveDiff& preAnalysis);

private:
    // --- pipeline stages ---
    bool runPreAnalysis();
    bool runMTASlicingAndAnalysis();
    bool runPTASlicingAndAnalysis();
    bool runFinalRaceDetection();
    void buildPreAnalysisSVFG();

    /// Pipeline utilities shared by the sliced and whole-program paths.
    //@{
    static bool checkPhaseResult(const char* phase, bool condition);
    static void reportOriginalStatistics(SVFIR* svfir);
    static std::set<const ICFGNode*> collectICFGNodes(
        SVFG* svfg, const NodeBS& svfgNodeIds);
    static void reportPTASliceStatistics(
        const std::set<const ICFGNode*>& icfgNodes);
    static std::string raceStatementKey(const SVFStmt* statement);
    static void updateDigest(u64_t& digest, const std::string& value);
    static u64_t raceStatementDigest(const std::set<RacePair>& pairs);
    static u64_t racePairDigest(const std::set<RacePair>& pairs);
    //@}

    /// No-slice A/B baseline: run the FSAM detection on the whole program (no
    /// slicing), so its time and race set can be compared against the sliced run.
    bool runWholeProgramDetection();

    /// Main pointer-analysis instance feeding final race detection (the
    /// flow-sensitive FSAM, a BVDataPTAImpl queried polymorphically).
    BVDataPTAImpl* getMainPTA() const;

    /// Union of both statements of every candidate race pair (the slice targets).
    std::set<const SVFStmt*> getVulnerableStmts() const;

    // --- race detection ---
    /// Refine the pre-analysis candidate pairs with main-phase ILA and FSAM.
    std::set<RacePair> detectRacePairsOnSlicedGraph(
        const std::set<RacePair>& preAnalysisRacePairs,
        BVDataPTAImpl* slicedPTA, MHP* slicedMHP,
        LockAnalysis* slicedLockAnalysis);

    // --- pipeline state (owned unless noted) ---
    SVFIR* svfir = nullptr;
    // Main-phase context depth, set by runOnModule; the pre-analysis uses it
    // to reconcile context-truncation-merged thread instances.
    u32_t mainContextDepth;
    std::unique_ptr<TCT> tct;
    std::unique_ptr<MHP> mhp;
    std::unique_ptr<LockAnalysis> lockAnalysis;
    // Inclusion-based Andersen's pre-analysis (a shared singleton, not owned
    // here; the tool releases it after this object is destroyed). Feeds TCT/MHP/lock/
    // race pre-analysis, the thread-aware VFG_pre, and the main FSMPTA.
    AndersenWaveDiff* preAndersen = nullptr;
    ThreadCallGraph* threadCallGraph = nullptr;
    std::unique_ptr<MTASVFGBuilder> preSVFGBuilder; // owns preSVFG
    SVFG* preSVFG = nullptr;
    std::unique_ptr<MultiStageSlicer> multiStageSlicer;
    NodeBS preCandidateSolveNodeIds;
    std::unique_ptr<SingleSlicer> singleSlicer;
    /// VFG'_pre endpoint-filtered, context-insensitive pre-MHP candidates.
    /// This is only a conservative worklist for rebuilding Main-TVF; the main
    /// phase independently re-decides every edge with main MHP and lock facts.
    std::vector<std::pair<NodeID, NodeID>> selectedThreadVFCandidates;
    // -mta-slicing-single: the one unified slice, computed in MTA slicing and reused
    // (not recomputed) for PTA slicing so both stages share V_Single.
    std::set<const ICFGNode*> singleSlicedNodes;
    NodeBS singleSlicedSVFGNodeIds;
    std::unique_ptr<SlicedSVFIRView> mtaSlicedView;
    std::unique_ptr<SlicedSVFIRView> ptaSlicedView;
    std::unique_ptr<SlicedSVFGView> preCandidateSVFGView;
    std::unique_ptr<SlicedSVFGView> slicedSVFGView;
    std::unique_ptr<FlowSensitive> mainFSMPTA;
    std::unique_ptr<SlicedTCT> slicedTCT;
    std::unique_ptr<MHP> slicedMHP;
    std::unique_ptr<LockAnalysis> slicedLockAnalysis;
    bool hasThreadFunctions = false;
    std::set<RacePair> racePairs;
};

} // End namespace SVF

#endif /* MTA_H_ */
