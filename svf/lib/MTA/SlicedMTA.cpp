//===- SlicedMTA.cpp -- Multi-stage on-demand slicing race detection -//
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
 * SlicedMTA.cpp
 *
 *      Author: Jiawei Yang
 *
 * Multi-Stage On-Demand Program Slicing for multi-threaded race detection (MSli).
 * Library-side orchestration of the slicing pipeline over the SVFIR.
 */

#include "MTA/SlicedMTA.h"
#include "MTA/TCT.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/FSMPTA.h"
#include "MTA/Slicer.h"
#include "MTA/SlicedView.h"
#include "MTA/SlicedMHP.h"
#include "MTA/SlicedLockAnalysis.h"
#include "Graphs/ThreadCallGraph.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"

#include <chrono>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

using namespace SVF;

namespace
{

// Timing helper.
void timePhase(const char* name, const std::function<void()>& fn)
{
    SVFUtil::outs() << "[TIMER] Phase: " << name << " - started\n";
    auto start = std::chrono::steady_clock::now();
    fn();
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start).count();
    SVFUtil::outs() << "[TIMER] Phase: " << name << " - finished in " << std::fixed << std::setprecision(2) << ms << " ms";
    if (ms >= 1000.0)
        SVFUtil::outs() << " (" << std::fixed << std::setprecision(2) << (ms / 1000.0) << " s)";
    SVFUtil::outs() << "\n";
}

// Output statistics for the original (unsliced) SVFIR.
void reportOriginalStats(SVFIR* svfIr)
{
    size_t icfgNodeCount = 0;
    for (ICFG::iterator it = svfIr->getICFG()->begin(), eit = svfIr->getICFG()->end(); it != eit; ++it)
        icfgNodeCount++;

    size_t functionCount = 0;
    for (auto it = svfIr->getCallGraph()->begin(), eit = svfIr->getCallGraph()->end(); it != eit; ++it)
        functionCount++;

    size_t pagStmtCount = 0;
    for (PAG::iterator it = svfIr->getPAG()->begin(), eit = svfIr->getPAG()->end(); it != eit; ++it)
        pagStmtCount++;

    SVFUtil::outs() << "\n[Original SVFIR] Statistics:\n";
    SVFUtil::outs() << "  ICFG nodes: " << icfgNodeCount << "\n";
    SVFUtil::outs() << "  Functions: " << functionCount << "\n";
    SVFUtil::outs() << "  PAG statements: " << pagStmtCount << "\n";
}

// Check and report a step result.
bool checkAndReport(const char* phase, bool condition)
{
    if (!condition)
        SVFUtil::errs() << "[ERROR] " << phase << " failed\n";
    return condition;
}

// Resolve the max context length for the sliced TCT.
u32_t slicedMaxContextLen()
{
    u32_t v = Options::SlicedMaxCxt();
    return v > 0 ? v : Options::MaxContextLen();
}

// --- observe-mode helpers ---

// Render a points-to set as "{name1, name2, ...}" using source-level names.
std::string ptsToStr(SVFIR* pag, const PointsTo& pts)
{
    std::string s = "{";
    bool first = true;
    for (NodeID o : pts)
    {
        if (!first) s += ", ";
        first = false;
        s += pag->getGNode(o)->getValueName();
    }
    return s + "}";
}

// Short source-level name for a var, falling back to a placeholder.
std::string varName(const SVFVar* v)
{
    std::string n = v->getValueName();
    return n.empty() ? ("v" + std::to_string(v->getId())) : n;
}

// Dump the flow-sensitive points-to set at each load/store in user functions.
void dumpFSAMPts(SVFIR* pag, BVDataPTAImpl* fs,
                 std::vector<std::pair<const ICFGNode*, std::string>>& memOps)
{
    ICFG* icfg = pag->getICFG();
    SVFUtil::outs() << "\n--- Flow-sensitive points-to at loads/stores ---\n";
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        const FunObjVar* fun = node->getFun();
        if (fun == nullptr)
            continue;
        std::string fn = fun->getName();
        for (const SVFStmt* stmt : pag->getSVFStmtList(node))
        {
            if (const LoadStmt* L = SVFUtil::dyn_cast<LoadStmt>(stmt))
            {
                std::string lhs = varName(L->getLHSVar());
                std::string label = lhs + " = *" + varName(L->getRHSVar());
                SVFUtil::outs() << "  [" << fn << " LOAD ] " << label
                                << "    pt(" << lhs << ") = " << ptsToStr(pag, fs->getPts(L->getLHSVarID())) << "\n";
                memOps.emplace_back(node, fn + ":LOAD " + label);
            }
            else if (const StoreStmt* S = SVFUtil::dyn_cast<StoreStmt>(stmt))
            {
                std::string ptr = varName(S->getLHSVar());
                std::string label = "*" + ptr + " = " + varName(S->getRHSVar());
                SVFUtil::outs() << "  [" << fn << " STORE] " << label
                                << "    pt(" << ptr << ") = " << ptsToStr(pag, fs->getPts(S->getLHSVarID())) << "\n";
                memOps.emplace_back(node, fn + ":STORE " + label);
            }
        }
    }
}

// Dump MHP + common-lock relations among the collected memory ops.
void dumpILA(MHP* mhp, LockAnalysis* lockAnalysis,
             const std::vector<std::pair<const ICFGNode*, std::string>>& memOps)
{
    SVFUtil::outs() << "\n--- ILA: may-happen-in-parallel (MHP) & common-lock among memory ops ---\n";
    for (size_t i = 0; i < memOps.size(); ++i)
    {
        for (size_t j = i + 1; j < memOps.size(); ++j)
        {
            const ICFGNode* n1 = memOps[i].first;
            const ICFGNode* n2 = memOps[j].first;
            if (!mhp->mayHappenInParallel(n1, n2))
                continue;
            bool locked = lockAnalysis->isProtectedByCommonLock(n1, n2);
            SVFUtil::outs() << "  MHP" << (locked ? " (common-lock)" : "            ")
                            << " : [" << memOps[i].second << "]  ||  [" << memOps[j].second << "]\n";
        }
    }
}

} // anonymous namespace

SlicedMTA::SlicedMTA() = default;

SlicedMTA::~SlicedMTA()
{
    // Cleanup in reverse order of creation. Release the VFG_pre (and its MemSSA),
    // the FSAM, the sliced/base TCTs -- all of which reference the SVFIR -- before
    // releasing the SVFIR itself. The remaining unique_ptr members (mhp /
    // lockAnalysis / sliced* / slicers / views) auto-destroy after this body.
    vfgPreBuilder.reset();
    mtaFSMPTA.reset();
    slicedTCT.reset();
    tct.reset();
    SVFIR::releaseSVFIR();
    // The Andersen pre-analysis is a shared singleton (reused by VFG_pre and the
    // main FSMPTA, neither of which frees it); release it once here.
    AndersenWaveDiff::releaseAndersenWaveDiff();
}

BVDataPTAImpl* SlicedMTA::getMainPTA() const
{
    // The main FSMPTA phase is the flow-sensitive FSAM (FSMPTA), a
    // BVDataPTAImpl, so the downstream race detector queries it polymorphically.
    return mtaFSMPTA.get();
}

std::set<const SVFStmt*> SlicedMTA::getVulnerableStmts() const
{
    std::set<const SVFStmt*> v;
    for (const RacePair& pair : racePairs)
    {
        v.insert(pair.stmt1);
        v.insert(pair.stmt2);
    }
    return v;
}

// Pre-Analysis (Pointer Analysis + TCT + MHP & Lock + Race Detection).
// Build the thread-aware value-flow graph (VFG_pre) once, on a shared Andersen
// (reused by the main FSMPTA via the AndersenWaveDiff singleton). This is the
// substrate the paper uses for both slicing (data dependence over the
// thread-aware value flow) and the main sparse FS resolution.
void SlicedMTA::buildVFGPre()
{
    timePhase("Build thread-aware VFG_pre", [&]()
    {
        // preAnder is the pre-analysis Andersen built in runPreAnalysis.
        // Treat fork/join as calls so the SVFG carries the thread-oblivious
        // (fork/join-ordered) value flow.
        if (ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(preAnder->getCallGraph()))
        {
            tcg->updateCallGraph(preAnder);
            tcg->updateJoinEdge(preAnder);
        }
        vfgPreBuilder = std::make_unique<MTASVFGBuilder>(mhp.get(), lockAnalysis.get());
        vfgPre = vfgPreBuilder->buildPTROnlySVFG(preAnder);
        SVFUtil::outs() << "[VFG_pre] thread-aware SVFG: " << vfgPre->getSVFGNodeNum()
                        << " nodes, " << MTASVFGBuilder::numOfNewSVFGEdges << " interference edges\n";
    });
}

bool SlicedMTA::runPreAnalysis(const ResolveIndirectCalls& resolveIndirectCalls)
{
    SVFUtil::outs() << "\n=== Pre-Analysis ===\n";

    const bool dumpDot = Options::SlicedDumpDot();

    // Step 1: Pointer Analysis. Inclusion-based Andersen's (more precise than
    // Steensgaard's unification, so fewer spurious MHP/races and a smaller slice).
    // The same Andersen instance (a singleton) is reused for the thread-aware
    // VFG_pre and the main FSMPTA, so the whole pipeline shares one pre-analysis.
    timePhase("Andersen's pointer analysis", [&]()
    {
        preAnder = AndersenWaveDiff::createAndersenWaveDiff(svfIr);
        if (dumpDot)
        {
            preAnder->getConstraintGraph()->dump("original_consg");
            preAnder->getCallGraph()->dump("original_tcg");
        }
        // Materialise resolved indirect calls into the PAG (LLVM-dependent step,
        // injected by the caller), then update the ICFG with the resolved calls.
        resolveIndirectCalls(preAnder->getCallGraph());
        svfIr->getICFG()->updateCallGraph(preAnder->getCallGraph());
        if (dumpDot)
            svfIr->getICFG()->dump("original_icfg");
    });
    if (!checkAndReport("Pointer Analysis", preAnder != nullptr))
        return false;

    // Step 2: Build Thread Create Tree
    timePhase("Create Thread Create Tree", [&]()
    {
        tct = std::make_unique<TCT>(preAnder);
    });
    if (dumpDot)
        tct->dump("original_tct");

    // Step 3: Interleaving and Lock Analysis
    timePhase("Run Interleaving and Lock Analysis", [&]()
    {
        mhp = std::make_unique<MHP>(tct.get());
        mhp->analyze();
        lockAnalysis = std::make_unique<LockAnalysis>(tct.get());
        lockAnalysis->analyze();
    });

    // Step 4: Detect thread functions
    timePhase("Detect Thread Functions", [&]()
    {
        threadFunctions = detectAllThreadFunctions(preAnder->getCallGraph());
    });
    SVFUtil::outs() << "Found " << threadFunctions.size() << " thread functions\n";
    if (threadFunctions.empty())
    {
        SVFUtil::outs() << "[WARNING] No thread functions found\n";
        return true; // Not an error, just no threads to analyze
    }

    // Step 5: Detect race statements
    std::set<const SVFStmt*> vulnerableStatements;
    timePhase("Detect Race Statements", [&]()
    {
        // Shared equivalence-class detector (the same one MTA::reportRaces uses).
        vulnerableStatements = MTA::detectRace(
                                   svfIr, preAnder, mhp.get(), lockAnalysis.get(),
                                   preAnder->getCallGraph(), racePairs);
    });
    SVFUtil::outs() << "Found " << vulnerableStatements.size() << " vulnerable statements\n";
    SVFUtil::outs() << "Found " << racePairs.size() << " race pairs\n";

    // Step 6: build the thread-aware VFG once (substrate for slicing + main FS).
    buildVFGPre();

    return true;
}

// MTA Slicing and Analysis (using pre-analysis pointer analysis results)
bool SlicedMTA::runMTASlicingAndAnalysis()
{
    SVFUtil::outs() << "\n=== MTA Slicing and Analysis ===\n";

    if (racePairs.empty())
    {
        SVFUtil::outs() << "[SKIP] No race pairs found in pre-analysis\n";
        return true;
    }

    const bool dumpDot = Options::SlicedDumpDot();

    // Step 1: Get vulnerable statements from race pairs
    std::set<const SVFStmt*> vulnerableStatements = getVulnerableStmts();

    std::set<const ICFGNode*> mtaSlicedNodes;

    if (Options::SlicingSingle())
    {
        // Single-pass baseline (MSli §3/§5.4): one unified slice (V_Single)
        // combining synchronization + data + call dependence, shared by both the
        // ILA and the FSPTA stages. Computed once here; reused in PTA slicing.
        SVFUtil::outs() << "[Slicing Mode] Single unified slice (V_Single) for ILA + FSPTA\n";
        singleSlicer = std::make_unique<SingleSlicer>(
                           svfIr, preAnder, mhp.get(), lockAnalysis.get(),
                           vfgPre /* data dependence over the thread-aware VFG_pre */);
        timePhase("Unified Slicing", [&]()
        {
            singleSlicedNodes = singleSlicer->runSlicing(vulnerableStatements);
        });
        mtaSlicedNodes = singleSlicedNodes;
        SVFUtil::outs() << "Unified sliced to " << mtaSlicedNodes.size() << " nodes\n";
    }
    else
    {
        SVFUtil::outs() << "[Slicing Mode] Differential slices (separate ILA + FSPTA)\n";
        mtaSlicer = std::make_unique<MTASlicer>(
                        svfIr, preAnder, mhp.get(), lockAnalysis.get());

        // MTA slicing (includes dual slicing and function expansion). ILA slicing
        // sources = [INIT] race statements + the [THREAD-VF] sources (MSli §4.2).
        // We keep a candidate edge's query only if the edge survives the FSPTA
        // slice -- ThreadVF(VFG'_pre), i.e. both its endpoints lie in the
        // data-dependence slice over VFG_pre. This restricts the witnesses to the
        // value flows the main phase will actually build, matching Fig. 6's
        // [THREAD-VF] rule, while staying sound (ThreadVF(VFG') subset of
        // ThreadVF(VFG'_pre)). The slice is computed here (pre <-> pre) and reused
        // by PTA slicing.
        std::set<const ICFGNode*> threadVFSources;
        const bool useThreadVFSources = Options::ThreadVFSources();
        if (useThreadVFSources && vfgPreBuilder)
        {
            if (!ptaSlicer)
                ptaSlicer = std::make_unique<PTASlicer>(
                                svfIr, preAnder, mhp.get(), lockAnalysis.get(), vfgPre);
            const std::set<const SVFGNode*>& retained =
                ptaSlicer->getRetainedSVFGNodes(vulnerableStatements);
            for (const auto& entry : vfgPreBuilder->getThreadVFQueryMap())
            {
                const MTASVFGBuilder::ThreadVFEdge& edge = entry.first;
                if (retained.count(edge.first) && retained.count(edge.second))
                    threadVFSources.insert(entry.second.begin(), entry.second.end());
            }
        }
        SVFUtil::outs() << "[THREAD-VF] " << threadVFSources.size()
                        << " ILA slicing sources from VFG_pre value-flow construction"
                        << (useThreadVFSources ? "\n" : " (ablation: [THREAD-VF] sources OFF)\n");
        timePhase("MTA Slicing", [&]()
        {
            mtaSlicedNodes = mtaSlicer->runSlicing(vulnerableStatements, threadVFSources);
        });
        SVFUtil::outs() << "MTA sliced to " << mtaSlicedNodes.size() << " nodes\n";
    } // end differential MTA slice

    // Step 4: Build MTA SlicedSVFIRView (using pre-analysis pointer analysis)
    timePhase("Build MTA Sliced View", [&]()
    {
        mtaSlicedView = std::make_unique<SlicedSVFIRView>(
                            svfIr, preAnder->getCallGraph(), svfIr->getICFG(), mtaSlicedNodes);
    });
    mtaSlicedView->dumpStats("MTA Sliced");

    const SlicedSVFIRView* slicedView = mtaSlicedView.get();

    if (dumpDot)
    {
        SVFUtil::outs() << "\n[Dump] MTA Sliced views:\n";
        slicedView->getICFG()->dump("sliced_icfg");
        if (slicedView->getThreadCallGraph() != nullptr)
            slicedView->getThreadCallGraph()->dump("sliced_tcg");
    }

    // Step 5: Build Sliced TCT (using pre-analysis pointer analysis)
    timePhase("Sliced Thread Create Tree", [&]()
    {
        u32_t maxContextLen = slicedMaxContextLen();
        if (Options::SlicedMaxCxt() > 0)
            SVFUtil::outs() << "[SlicedTCT] Using max context length: " << maxContextLen
                            << " (from -sliced-max-cxt option)\n";
        else
            SVFUtil::outs() << "[SlicedTCT] Using max context length: " << maxContextLen
                            << " (from -max-cxt, same as pre-analysis)\n";
        // Reuse the shared pre-analysis (Andersen) for the sliced TCT.
        slicedTCT = std::make_unique<SlicedTCT>(preAnder, slicedView, maxContextLen);
        if (dumpDot)
            slicedTCT->dump("sliced_tct");
    });

    // Step 6: Sliced MHP and Lock Analysis
    timePhase("Sliced Interleaving and Lock Analysis", [&]()
    {
        slicedMhp = std::make_unique<SlicedMHP>(slicedTCT.get(), slicedView);
        slicedMhp->analyze();
        slicedLockAnalysis = std::make_unique<SlicedLockAnalysis>(slicedTCT.get(), slicedView);
        slicedLockAnalysis->analyze();
    });

    return true;
}

// PTA Slicing and Sliced Pointer Analysis
bool SlicedMTA::runPTASlicingAndAnalysis()
{
    SVFUtil::outs() << "\n=== PTA Slicing and Sliced Pointer Analysis ===\n";

    if (threadFunctions.empty())
    {
        SVFUtil::outs() << "[SKIP] No thread functions found in pre-analysis, skipping PTA slicing\n";
        return true;
    }
    if (racePairs.empty())
    {
        SVFUtil::outs() << "[SKIP] No race pairs found in pre-analysis, skipping PTA slicing\n";
        return true;
    }

    const bool dumpDot = Options::SlicedDumpDot();

    std::set<const SVFStmt*> vulnerableStatements = getVulnerableStmts();

    std::set<const ICFGNode*> ptaSlicedNodes;

    if (Options::SlicingSingle())
    {
        // Single-pass baseline: reuse the unified V_Single computed in MTA slicing
        // (no separate data-dependence slice); FSPTA runs on the same slice as ILA.
        SVFUtil::outs() << "[Slicing Mode] Reusing unified slice (V_Single) for FSPTA\n";
        ptaSlicedNodes = singleSlicedNodes;
        SVFUtil::outs() << "PTA reuses unified slice: " << ptaSlicedNodes.size() << " nodes\n";
    }
    else
    {
        SVFUtil::outs() << "Using " << vulnerableStatements.size() << " vulnerable statements from pre-analysis\n";
        SVFUtil::outs() << "Using " << racePairs.size() << " race pairs from pre-analysis\n";

        // Reuse the slicer built during MTA slicing (it memoised the shared
        // data-dependence closure over VFG_pre); construct it only if absent
        // (e.g. the [THREAD-VF] ablation skipped its early creation).
        if (!ptaSlicer)
            ptaSlicer = std::make_unique<PTASlicer>(
                            svfIr, preAnder, mhp.get(), lockAnalysis.get(),
                            vfgPre /* paper-faithful data dependence over the thread-aware VFG */);

        timePhase("PTA Slicing", [&]()
        {
            ptaSlicedNodes = ptaSlicer->runSlicing(vulnerableStatements);
        });
        SVFUtil::outs() << "PTA sliced to " << ptaSlicedNodes.size() << " nodes\n";
    }

    // Step 4: Build PTA SlicedSVFIRView for pointer analysis
    timePhase("Build PTA Sliced View", [&]()
    {
        ptaSlicedView = std::make_unique<SlicedSVFIRView>(
                            svfIr, preAnder->getCallGraph(), svfIr->getICFG(), ptaSlicedNodes);
    });
    ptaSlicedView->dumpStats("PTA Sliced");

    // Step 5: Main FSMPTA phase -- sparse flow-sensitive FSAM. The paper's main
    // analysis runs a sparse flow-sensitive pointer analysis over a thread-aware
    // value-flow graph; FSMPTA builds that thread-aware SVFG (via MHP + lock
    // spans) and solves it with SVF's flow-sensitive engine.
    //
    // -main-ila-sliced (paper-faithful): rebuild the thread-aware value flow with
    // the SLICED ILA. The interference edges are then constructed by querying
    // SlicedMHP / SlicedLockAnalysis -- which can only answer correctly because
    // [THREAD-VF] source extraction kept every interference endpoint and lock-span
    // witness in the ILA slice. A divergence here (vs the full-ILA result) would
    // mean the slice dropped a required ILA query. Costs a fresh SVFG build.
    //
    // default: reuse the thread-aware VFG_pre built once in pre-analysis with the
    // whole-program ILA, and restrict the flow-sensitive solve to the PTA slice
    // (processNode skips sliced-away memory ops). Under query preservation the two
    // give the same result; this path avoids the second SVFG build.
    bool useSlicedIla = (Options::MainIlaSliced() &&
                         slicedMhp != nullptr && slicedLockAnalysis != nullptr);
    if (useSlicedIla)
    {
        SVFUtil::outs() << "[Main FSMPTA] Thread-aware value flow from the SLICED ILA "
                        "(paper-faithful; fresh SVFG; [THREAD-VF] load-bearing)\n";
    }
    else
    {
        if (Options::MainIlaSliced())
            SVFUtil::outs() << "[Main FSMPTA] -main-ila-sliced requested but sliced ILA unavailable; "
                            "falling back to full ILA\n";
        SVFUtil::outs() << "[Main FSMPTA] Using flow-sensitive FSAM (FSMPTA, sliced solve, reusing VFG_pre)\n";
    }
    timePhase("Flow-Sensitive FSAM Analysis", [&]()
    {
        if (useSlicedIla)
            mtaFSMPTA = std::make_unique<FSMPTA>(slicedMhp.get(), slicedLockAnalysis.get(),
                                    ptaSlicedView.get(), /*preBuilt=*/nullptr);
        else
            mtaFSMPTA = std::make_unique<FSMPTA>(mhp.get(), lockAnalysis.get(),
                                    ptaSlicedView.get(), vfgPre);
        mtaFSMPTA->analyze();
        if (dumpDot)
            mtaFSMPTA->getSVFG()->dump("mta_svfg");
    });

    return true;
}

// Final Race Detection using sliced analysis results
bool SlicedMTA::runFinalRaceDetection()
{
    SVFUtil::outs() << "\n=== Final Race Detection ===\n";

    if (threadFunctions.empty())
    {
        SVFUtil::outs() << "[SKIP] No thread functions found\n";
        return true;
    }
    if (mtaSlicedView == nullptr)
    {
        SVFUtil::outs() << "[SKIP] MTA sliced view not available\n";
        return true;
    }
    if (slicedMhp == nullptr || slicedLockAnalysis == nullptr)
    {
        SVFUtil::outs() << "[SKIP] Sliced MHP or LockAnalysis not available\n";
        return true;
    }
    if (getMainPTA() == nullptr)
    {
        SVFUtil::outs() << "[SKIP] Main flow-sensitive pointer analysis not available\n";
        return true;
    }

    std::set<RacePair> detectedPairs;
    timePhase("Final Race Detection", [&]()
    {
        detectedPairs = detectRacePairsOnSlicedGraph(
                            racePairs,        // Use pre-analysis race pairs
                            getMainPTA(),     // Use flow-sensitive FSAM points-to
                            slicedMhp.get(),
                            slicedLockAnalysis.get(),
                            mtaSlicedView.get());
    });

    SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
    SVFUtil::outs() << "Race pairs (pre-analysis): " << racePairs.size() << "\n";
    SVFUtil::outs() << "Race pairs (sliced graph): " << detectedPairs.size() << "\n";

    if (!detectedPairs.empty())
    {
        SVFUtil::outs() << "\n=== Bug Report ===\n";
        SVFUtil::outs() << "Found " << detectedPairs.size() << " race pair(s) in sliced graph\n";
    }
    else
    {
        SVFUtil::outs() << "\nNo race pairs detected in sliced graph.\n";
    }

    return true;
}

// No-slice A/B baseline: run the SAME refined machinery as the sliced path
// (SlicedTCT/MHP/LockAnalysis + flow-sensitive FSAM + the same final re-check),
// but over a "slice" that keeps EVERY ICFG node -- i.e. the whole program. This
// is the correct reference: if slicing preserves the result, this must produce
// the same race set as the real (reduced) slice, only slower.
void SlicedMTA::runWholeProgramDetection()
{
    SVFUtil::outs() << "\n=== Whole-program FSAM Race Detection (no slicing) ===\n";
    if (threadFunctions.empty() || racePairs.empty())
    {
        SVFUtil::outs() << "[SKIP] No thread functions / race pairs in pre-analysis\n";
        return;
    }

    // Full "slice" = every ICFG node.
    std::set<const ICFGNode*> allNodes;
    for (ICFG::iterator it = svfIr->getICFG()->begin(), eit = svfIr->getICFG()->end(); it != eit; ++it)
        allNodes.insert(it->second);
    ptaSlicedView = std::make_unique<SlicedSVFIRView>(
                        svfIr, preAnder->getCallGraph(), svfIr->getICFG(), allNodes);

    timePhase("Whole-program Sliced TCT/MHP/Lock", [&]()
    {
        slicedTCT = std::make_unique<SlicedTCT>(preAnder, ptaSlicedView.get(), slicedMaxContextLen());
        slicedMhp = std::make_unique<SlicedMHP>(slicedTCT.get(), ptaSlicedView.get());
        slicedMhp->analyze();
        slicedLockAnalysis = std::make_unique<SlicedLockAnalysis>(slicedTCT.get(), ptaSlicedView.get());
        slicedLockAnalysis->analyze();
    });

    timePhase("Whole-program Flow-Sensitive FSAM Analysis", [&]()
    {
        mtaFSMPTA = std::make_unique<FSMPTA>(mhp.get(), lockAnalysis.get(),
                                           ptaSlicedView.get(), vfgPre);
        mtaFSMPTA->analyze();
    });

    std::set<RacePair> detectedPairs;
    timePhase("Final Race Detection (whole program)", [&]()
    {
        detectedPairs = detectRacePairsOnSlicedGraph(
                            racePairs, getMainPTA(), slicedMhp.get(),
                            slicedLockAnalysis.get(), ptaSlicedView.get());
    });

    SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
    SVFUtil::outs() << "Race pairs (pre-analysis): " << racePairs.size() << "\n";
    SVFUtil::outs() << "Race pairs (whole program): " << detectedPairs.size() << "\n";
}

// Observe whole-program FSAM points-to and ILA (Layer 1) for soundness checking.
void SlicedMTA::runObserveFSAM()
{
    SVFUtil::outs() << "\n===== [OBSERVE] Flow-sensitive FSAM points-to & ILA =====\n";
    MTASVFGBuilder::numOfNewSVFGEdges = 0;
    FSMPTA* fs = new FSMPTA(mhp.get(), lockAnalysis.get(), nullptr, vfgPre);
    fs->analyze();
    SVFUtil::outs() << "Thread-aware (interference) SVFG edges added: "
                    << MTASVFGBuilder::numOfNewSVFGEdges << "\n";
    std::vector<std::pair<const ICFGNode*, std::string>> memOps;
    dumpFSAMPts(svfIr, fs, memOps);
    dumpILA(mhp.get(), lockAnalysis.get(), memOps);
    SVFUtil::outs() << "===== [OBSERVE] end =====\n\n";
    delete fs;
}

// Observe SLICED FSAM points-to (Layer 2): compute the FSMPTA slice from the race
// targets, then run the sliced flow-sensitive analysis and dump pt at the query
// load. Used to check query preservation (sliced pt == unsliced pt).
void SlicedMTA::runObserveFSAMSliced()
{
    SVFUtil::outs() << "\n===== [OBSERVE-SLICED] Sliced flow-sensitive FSAM points-to =====\n";
    if (racePairs.empty())
    {
        SVFUtil::outs() << "[no race targets -- nothing to slice]\n===== [OBSERVE-SLICED] end =====\n\n";
        return;
    }
    std::set<const SVFStmt*> vuln = getVulnerableStmts();
    PTASlicer slicer(svfIr, preAnder, mhp.get(), lockAnalysis.get(), vfgPre);
    std::set<const ICFGNode*> ptaSlicedNodes = slicer.runSlicing(vuln);
    SlicedSVFIRView view(svfIr, preAnder->getCallGraph(), svfIr->getICFG(), ptaSlicedNodes);
    SVFUtil::outs() << "Sliced to " << ptaSlicedNodes.size() << " ICFG nodes\n";

    MTASVFGBuilder::numOfNewSVFGEdges = 0;
    FSMPTA* fs = new FSMPTA(mhp.get(), lockAnalysis.get(), &view, vfgPre);
    fs->analyze();
    SVFUtil::outs() << "Thread-aware (interference) SVFG edges added: "
                    << MTASVFGBuilder::numOfNewSVFGEdges << "\n";
    std::vector<std::pair<const ICFGNode*, std::string>> memOps;
    dumpFSAMPts(svfIr, fs, memOps);
    SVFUtil::outs() << "===== [OBSERVE-SLICED] end =====\n\n";
    delete fs;
}

void SlicedMTA::runOnModule(SVFIR* pag, const ResolveIndirectCalls& resolveIndirectCalls)
{
    svfIr = pag;

    SVFUtil::outs() << "[Config] Slicing: " << (Options::EnableSlicing() ? "enabled" : "disabled") << "\n";
    SVFUtil::outs() << "[Config] Main-phase ILA: "
                    << (Options::MainIlaSliced() ? "sliced (paper-faithful, fresh SVFG)"
                        : "full (reuse VFG_pre, build once)") << "\n";

    reportOriginalStats(svfIr);

    if (!runPreAnalysis(resolveIndirectCalls))
        return;

    // Observe modes: dump FSAM points-to + ILA for soundness / query preservation.
    if (Options::MTAObserve())
    {
        runObserveFSAM();
        SVFUtil::outs() << "\n=== Analysis Complete (observe) ===\n";
        return;
    }
    if (Options::MTAObserveSliced())
    {
        runObserveFSAMSliced();
        SVFUtil::outs() << "\n=== Analysis Complete (observe-sliced) ===\n";
        return;
    }

    if (Options::NoSlice())
    {
        runWholeProgramDetection();
    }
    else if (Options::EnableSlicing())
    {
        if (!runMTASlicingAndAnalysis()) return;
        if (!runPTASlicingAndAnalysis()) return;
        if (!runFinalRaceDetection()) return;
    }
    else
    {
        SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
        SVFUtil::outs() << "Race pairs detected: " << racePairs.size() << "\n";
    }

    SVFUtil::outs() << "\n=== Analysis Complete ===\n";
}

//===----------------------------------------------------------------------===//
// Race detection (the detector folded in from RaceDetector; analogous to
// MTA::detect but driven by the sliced/FSAM analyses).
//===----------------------------------------------------------------------===//

// Detect all thread (fork-target) functions.
std::set<const FunObjVar*> SlicedMTA::detectAllThreadFunctions(CallGraph* callGraph) {
    std::set<const FunObjVar*> threadFunctions;

    // Iterate through all CallGraph nodes and check their out edges for fork edges
    for (CallGraph::iterator it = callGraph->begin(), eit = callGraph->end(); it != eit; ++it) {
        const CallGraphNode* node = it->second;
        for (const CallGraphEdge* edge : node->getOutEdges()) {
            if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge) {
                const FunObjVar* func = edge->getDstNode()->getFunction();
                if (func != nullptr) {
                    threadFunctions.insert(func);
                }
            }
        }
    }

    return threadFunctions;
}

// Detect race pairs on the sliced graph using sliced analysis results.
std::set<SlicedMTA::RacePair> SlicedMTA::detectRacePairsOnSlicedGraph(
    const std::set<RacePair>& preAnalysisRacePairs,
    BVDataPTAImpl* slicedPTA,
    MHP* slicedMHP,
    LockAnalysis* slicedLockAnalysis,
    const SlicedSVFIRView* slicedView) {

    std::set<RacePair> filteredRacePairs;

    // Re-check each candidate race pair on the sliced graph: it survives only if
    // the statements still (1) may happen in parallel under the sliced ILA,
    // (2) are not protected by a common lock, and (3) have intersecting points-to
    // under the flow-sensitive FSAM. Stages (1)/(2) preserve the whole-program
    // result (the sliced ILA recomputes the same MHP/lock facts); stage (3) is the
    // flow-sensitive refinement that prunes candidates the flow-insensitive
    // pre-analysis over-approximated.
    for (const RacePair& pair : preAnalysisRacePairs) {
        const ICFGNode* node1 = pair.stmt1->getICFGNode();
        const ICFGNode* node2 = pair.stmt2->getICFGNode();

        // Check if they may happen in parallel (using sliced MHP)
        if (!slicedMHP->mayHappenInParallelCache(node1, node2))
            continue;

        // Check if they are protected by common lock (using sliced LockAnalysis)
        if (slicedLockAnalysis->isProtectedByCommonLock(node1, node2))
            continue;

        // Re-check points-to intersection using sliced PTA
        PointsTo pts1, pts2;
        if (const LoadStmt* ldStmt1 = SVFUtil::dyn_cast<LoadStmt>(pair.stmt1)) {
            pts1 = slicedPTA->getPts(ldStmt1->getRHSVarID());
        } else if (const StoreStmt* stStmt1 = SVFUtil::dyn_cast<StoreStmt>(pair.stmt1)) {
            pts1 = slicedPTA->getPts(stStmt1->getLHSVarID());
        } else {
            continue;
        }

        if (const LoadStmt* ldStmt2 = SVFUtil::dyn_cast<LoadStmt>(pair.stmt2)) {
            pts2 = slicedPTA->getPts(ldStmt2->getRHSVarID());
        } else if (const StoreStmt* stStmt2 = SVFUtil::dyn_cast<StoreStmt>(pair.stmt2)) {
            pts2 = slicedPTA->getPts(stStmt2->getLHSVarID());
        } else {
            continue;
        }

        // Check if points-to sets still intersect
        if (pts1.intersects(pts2))
            filteredRacePairs.insert(pair);
    }

    return filteredRacePairs;
}

