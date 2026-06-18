//===- SlicedMTA.h -- Multi-stage on-demand slicing race detection ---//
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
 * SlicedMTA.h
 *
 * Multi-Stage On-Demand Program Slicing for multi-threaded race detection
 * (MSli). This is the library-side orchestration of the slicing pipeline; it
 * operates entirely on the SVFIR (LLVM-free). The single LLVM-dependent step --
 * materialising resolved indirect calls into the PAG -- is injected by the
 * caller as a callback (see runOnModule).
 */

#ifndef MTA_SLICEDMTA_H_
#define MTA_SLICEDMTA_H_

#include <SVFIR/SVFIR.h>          // brings Util/WorkList.h (FIFOWorkList) before SCC.h
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "WPA/Andersen.h"
#include <Graphs/CallGraph.h>
#include <functional>
#include <memory>
#include <set>
#include <utility>

namespace SVF
{

class TCT;
class MHP;
class LockAnalysis;
class MTASVFGBuilder;
class SVFG;
class FSPTA;
class PTASlicer;
class MTASlicer;
class SingleSlicer;
class SlicedSVFIRView;
class SlicedTCT;
class SlicedMHP;
class SlicedLockAnalysis;

/*!
 * Multi-stage on-demand slicing race detection pipeline (MSli).
 *
 * runOnModule drives five stages on a pre-built SVFIR:
 *   1. (caller) build SVFIR + resolve indirect calls into the PAG
 *   2. pre-analysis: Andersen, TCT, MHP, lock, candidate race pairs, VFG_pre
 *   3. MTA slicing: slice the thread-aware graph, build the sliced MHP/lock
 *   4. PTA slicing + main flow-sensitive FSAM (FSPTA) on the slice
 *   5. final race detection on the sliced graph using FSAM points-to
 *
 * Behaviour is controlled by Options (EnableSlicing, SlicedMaxCxt, MainIlaSliced,
 * ThreadVFSources, SlicingSingle, SlicedDumpDot, MTAObserve, MTAObserveSliced).
 */
class SlicedMTA
{
public:
    /// The one LLVM-dependent step of the pipeline: resolve the indirect-call
    /// edges discovered by Andersen into PAG copy/call edges. The caller (the
    /// LLVM-aware tool) supplies it via SVFIRBuilder::updateCallGraph.
    using ResolveIndirectCalls = std::function<void(CallGraph*)>;

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

    // Out-of-line (defined where the member types are complete) so callers that
    // only see the forward-declared unique_ptr member types need not be complete.
    SlicedMTA();
    ~SlicedMTA();

    SlicedMTA(const SlicedMTA&) = delete;
    SlicedMTA& operator=(const SlicedMTA&) = delete;

    /// Run the slicing pipeline (or an observe mode) on a pre-built SVFIR.
    void runOnModule(SVFIR* pag, const ResolveIndirectCalls& resolveIndirectCalls);

private:
    // --- pipeline stages ---
    bool phase2_PreAnalysis(const ResolveIndirectCalls& resolveIndirectCalls);
    bool phase3_MTASlicingAndAnalysis();
    bool phase4_PTASlicingAndAnalysis();
    bool phase5_FinalRaceDetection();
    void buildVFGPre();

    // --- observe modes (soundness / query-preservation checking) ---
    void observeFSAM();
    void observeFSAMSliced();

    /// Main pointer-analysis instance feeding final race detection (the
    /// flow-sensitive FSAM, a BVDataPTAImpl queried polymorphically).
    BVDataPTAImpl* getMainPTA() const;

    /// Union of both statements of every candidate race pair (the slice targets).
    std::set<const SVFStmt*> vulnerableStmts() const;

    // --- race detection (analogous to MTA::detect) ---
    /// Detect all thread (fork-target) functions in the program.
    static std::set<const FunObjVar*> detectAllThreadFunctions(CallGraph* callGraph);
    /// Detect candidate race pairs over the Andersen pre-analysis (fills outRacePairs).
    static std::set<const SVFStmt*> detectRaceStmts(
        SVFIR* svfIr, AndersenBase* pta, MHP* mhp, LockAnalysis* lockAnalysis,
        CallGraph* callGraph, const std::set<const FunObjVar*>& threadFunctions,
        std::set<RacePair>& outRacePairs);
    /// Re-check the candidate race pairs on the sliced graph using FSAM points-to.
    static std::set<RacePair> detectRacePairsOnSlicedGraph(
        const std::set<RacePair>& preAnalysisRacePairs, BVDataPTAImpl* slicedPTA,
        MHP* slicedMHP, LockAnalysis* slicedLockAnalysis, const SlicedSVFIRView* slicedView);

    // race-detection helpers
    static PointsTo _getGlobalObjectVariables(SVFIR* svfIr);
    static PointsTo _getPointsToClosure(AndersenBase* pta, const PointsTo& pts);
    static std::set<const FunObjVar*> _gatherParallelFunctions(
        CallGraph* callGraph, MHP* mhp, const std::set<const FunObjVar*>& funcSet);
    static bool _mayHappenInParallel(MHP* mhp, const FunObjVar* fun1, const FunObjVar* fun2);
    static std::set<const SVFStmt*> _getLoadStoreStatements(const std::set<const FunObjVar*>& functions);
    static std::set<const ICFGNode*> _getFunctionICFGNodes(const FunObjVar* function);

    // --- pipeline state (owned unless noted) ---
    SVFIR* svfIr = nullptr;
    std::unique_ptr<TCT> tct;
    std::unique_ptr<MHP> mhp;
    std::unique_ptr<LockAnalysis> lockAnalysis;
    // Inclusion-based Andersen's pre-analysis (a shared singleton, not owned
    // here -- released once in the destructor). Feeds the TCT / MHP / lock /
    // race pre-analysis, the thread-aware VFG_pre, and the main FSPTA.
    AndersenWaveDiff* preAnder = nullptr;
    std::unique_ptr<MTASVFGBuilder> vfgPreBuilder; // owns vfgPre
    SVFG* vfgPre = nullptr;
    std::unique_ptr<PTASlicer> ptaSlicer;
    std::unique_ptr<MTASlicer> mtaSlicer;
    std::unique_ptr<SingleSlicer> singleSlicer;
    std::unique_ptr<SlicedSVFIRView> mtaSlicedView;
    std::unique_ptr<SlicedSVFIRView> ptaSlicedView;
    std::unique_ptr<FSPTA> mtaFSPTA;
    std::unique_ptr<SlicedTCT> slicedTCT;
    std::unique_ptr<SlicedMHP> slicedMhp;
    std::unique_ptr<SlicedLockAnalysis> slicedLockAnalysis;
    std::set<const FunObjVar*> threadFunctions;
    std::set<RacePair> racePairs;
};

} // namespace SVF

#endif // MTA_SLICEDMTA_H_
