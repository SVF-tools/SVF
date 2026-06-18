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
 * Multi-Stage On-Demand Program Slicing for multi-threaded race detection (MSli).
 * Library-side orchestration of the slicing pipeline over the SVFIR.
 */

#include "MTA/SlicedMTA.h"
#include "MTA/TCT.h"
#include "MTA/MHP.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/FSPTA.h"
#include "MTA/Slicer.h"
#include "MTA/SlicedView.h"
#include "MTA/SlicedMHP.h"
#include "MTA/SlicedLockAnalysis.h"
#include "Graphs/ThreadCallGraph.h"
#include "Util/Options.h"
#include "Util/SVFUtil.h"

#include <chrono>
#include <deque>
#include <iomanip>
#include <map>
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
    // Cleanup in reverse order of creation.
    // Release the VFG_pre (and its MemSSA) before the SVFIR it references.
    vfgPreBuilder.reset();
    delete mtaFSPTA;
    delete slicedTCT;
    delete tct;
    // mhp / lockAnalysis / sliced* / slicers / views are unique_ptr: auto-deleted.
    SVFIR::releaseSVFIR();
    // The Andersen pre-analysis is a shared singleton (reused by VFG_pre and the
    // main FSPTA, neither of which frees it); release it once here.
    AndersenWaveDiff::releaseAndersenWaveDiff();
}

BVDataPTAImpl* SlicedMTA::getMainPTA() const
{
    // The main FSPTA phase is the flow-sensitive FSAM (FSPTA), a
    // BVDataPTAImpl, so the downstream race detector queries it polymorphically.
    return mtaFSPTA;
}

// Phase 2: Pre Analysis (Pointer Analysis + TCT + MHP & Lock + Race Detection).
// Build the thread-aware value-flow graph (VFG_pre) once, on a shared Andersen
// (reused by the main FSPTA via the AndersenWaveDiff singleton). This is the
// substrate the paper uses for both slicing (data dependence over the
// thread-aware value flow) and the main sparse FS resolution.
void SlicedMTA::buildVFGPre()
{
    timePhase("Build thread-aware VFG_pre", [&]()
    {
        // preAnder is the pre-analysis Andersen built in phase2_PreAnalysis.
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

bool SlicedMTA::phase2_PreAnalysis(const ResolveIndirectCalls& resolveIndirectCalls)
{
    SVFUtil::outs() << "\n=== Phase 2: Pre Analysis ===\n";

    const bool dumpDot = Options::SlicedDumpDot();

    // Step 1: Pointer Analysis. Inclusion-based Andersen's (more precise than
    // Steensgaard's unification, so fewer spurious MHP/races and a smaller slice).
    // The same Andersen instance (a singleton) is reused for the thread-aware
    // VFG_pre and the main FSPTA, so the whole pipeline shares one pre-analysis.
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
        tct = new TCT(preAnder);
    });
    if (dumpDot)
        tct->dump("original_tct");
    if (!checkAndReport("Build Thread Create Tree", tct != nullptr))
        return false;

    // Step 3: Interleaving and Lock Analysis
    timePhase("Run Interleaving and Lock Analysis", [&]()
    {
        mhp = std::make_unique<MHP>(tct);
        mhp->analyze();
        lockAnalysis = std::make_unique<LockAnalysis>(tct);
        lockAnalysis->analyze();
    });
    if (!checkAndReport("Interleaving and Lock Analysis", mhp != nullptr && lockAnalysis != nullptr))
        return false;

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
        vulnerableStatements = detectRaceStmts(
                                   svfIr, preAnder, mhp.get(), lockAnalysis.get(),
                                   preAnder->getCallGraph(), threadFunctions, racePairs);
    });
    SVFUtil::outs() << "Found " << vulnerableStatements.size() << " vulnerable statements\n";
    SVFUtil::outs() << "Found " << racePairs.size() << " race pairs\n";

    // Step 6: build the thread-aware VFG once (substrate for slicing + main FS).
    buildVFGPre();

    return true;
}

// Phase 3: MTA Slicing and Analysis (using pre-analysis pointer analysis results)
bool SlicedMTA::phase3_MTASlicingAndAnalysis()
{
    SVFUtil::outs() << "\n=== Phase 3: MTA Slicing and Analysis ===\n";

    if (racePairs.empty())
    {
        SVFUtil::outs() << "[SKIP] No race pairs found in pre-analysis\n";
        return true;
    }

    const bool dumpDot = Options::SlicedDumpDot();

    // Step 1: Get vulnerable statements from race pairs
    std::set<const SVFStmt*> vulnerableStatements;
    for (const RacePair& pair : racePairs)
    {
        vulnerableStatements.insert(pair.stmt1);
        vulnerableStatements.insert(pair.stmt2);
    }

    std::set<const ICFGNode*> mtaSlicedNodes;

    if (Options::SlicingSingle())
    {
        // Use single slicer for both MTA and PTA
        SVFUtil::outs() << "[Slicing Mode] Using Single Slicer\n";

        singleSlicer = std::make_unique<SingleSlicer>(
                           svfIr, preAnder, mhp.get(), lockAnalysis.get(),
                           vfgPre /* data dependence over the thread-aware VFG_pre */);
        if (!checkAndReport("Create SingleSlicer", singleSlicer != nullptr))
            return false;

        // Unified slicing (combines sync, data, and call dependence)
        timePhase("Unified Slicing", [&]()
        {
            mtaSlicedNodes = singleSlicer->performSlicing(vulnerableStatements);
        });
        SVFUtil::outs() << "Unified sliced to " << mtaSlicedNodes.size() << " nodes\n";
    }
    else
    {
        // Use multi (separate) MTA slicer
        SVFUtil::outs() << "[Slicing Mode] Using Multi MTA/PTA Slicers\n";

        mtaSlicer = std::make_unique<MTASlicer>(
                        svfIr, preAnder, mhp.get(), lockAnalysis.get());
        if (!checkAndReport("Create MTASlicer", mtaSlicer != nullptr))
            return false;

        // MTA slicing (includes dual slicing and function expansion). ILA slicing
        // sources = [INIT] race statements + the [THREAD-VF] sources (MSli §4.2)
        // collected while building VFG_pre, so the sliced MHP/lock preserve the
        // queries the main-phase thread-aware value-flow construction will make.
        static const std::set<const ICFGNode*> noSources;
        const bool useThreadVFSources = Options::ThreadVFSources();
        const std::set<const ICFGNode*>& threadVFSources =
            (useThreadVFSources && vfgPreBuilder)
            ? vfgPreBuilder->getThreadVFSrcNodes()
            : noSources;
        SVFUtil::outs() << "[THREAD-VF] " << threadVFSources.size()
                        << " ILA slicing sources from VFG_pre value-flow construction"
                        << (useThreadVFSources ? "\n" : " (ablation: [THREAD-VF] sources OFF)\n");
        timePhase("MTA Slicing", [&]()
        {
            mtaSlicedNodes = mtaSlicer->performSlicing(vulnerableStatements, threadVFSources);
        });
        SVFUtil::outs() << "MTA sliced to " << mtaSlicedNodes.size() << " nodes\n";
    }

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
        slicedTCT = new SlicedTCT(preAnder, slicedView, maxContextLen);
        if (dumpDot)
            slicedTCT->dump("sliced_tct");
    });
    if (!checkAndReport("Sliced Thread Create Tree", slicedTCT != nullptr))
        return false;

    // Step 6: Sliced MHP and Lock Analysis
    timePhase("Sliced Interleaving and Lock Analysis", [&]()
    {
        slicedMhp = std::make_unique<SlicedMHP>(slicedTCT, slicedView);
        slicedMhp->analyze();
        slicedLockAnalysis = std::make_unique<SlicedLockAnalysis>(slicedTCT, slicedView);
        slicedLockAnalysis->analyze();
    });

    return true;
}

// Phase 4: PTA Slicing and Sliced Pointer Analysis
bool SlicedMTA::phase4_PTASlicingAndAnalysis()
{
    SVFUtil::outs() << "\n=== Phase 4: PTA Slicing and Sliced Pointer Analysis ===\n";

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

    std::set<const SVFStmt*> vulnerableStatements;
    for (const RacePair& pair : racePairs)
    {
        vulnerableStatements.insert(pair.stmt1);
        vulnerableStatements.insert(pair.stmt2);
    }

    std::set<const ICFGNode*> ptaSlicedNodes;

    if (Options::SlicingSingle())
    {
        // Use the single slicer result for PTA (same as MTA)
        SVFUtil::outs() << "[Slicing Mode] Using Single Slicer result for PTA\n";
        if (singleSlicer == nullptr)
        {
            SVFUtil::errs() << "[ERROR] SingleSlicer not created in phase 3\n";
            return false;
        }
        timePhase("PTA Slicing (reuse Single Slicer)", [&]()
        {
            ptaSlicedNodes = singleSlicer->performSlicing(vulnerableStatements);
        });
        SVFUtil::outs() << "PTA sliced to " << ptaSlicedNodes.size() << " nodes (same as MTA)\n";
    }
    else
    {
        // Use separate PTA slicer
        SVFUtil::outs() << "Using " << vulnerableStatements.size() << " vulnerable statements from pre-analysis\n";
        SVFUtil::outs() << "Using " << racePairs.size() << " race pairs from pre-analysis\n";

        ptaSlicer = std::make_unique<PTASlicer>(
                        svfIr, preAnder, mhp.get(), lockAnalysis.get(),
                        vfgPre /* paper-faithful data dependence over the thread-aware VFG */);
        if (!checkAndReport("Create PTASlicer", ptaSlicer != nullptr))
            return false;

        timePhase("PTA Slicing", [&]()
        {
            ptaSlicedNodes = ptaSlicer->performSlicing(vulnerableStatements);
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

    // Step 5: Main FSPTA phase -- sparse flow-sensitive FSAM. The paper's main
    // analysis runs a sparse flow-sensitive pointer analysis over a thread-aware
    // value-flow graph; FSPTA builds that thread-aware SVFG (via MHP + lock
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
        SVFUtil::outs() << "[Main FSPTA] Thread-aware value flow from the SLICED ILA "
                        "(paper-faithful; fresh SVFG; [THREAD-VF] load-bearing)\n";
    }
    else
    {
        if (Options::MainIlaSliced())
            SVFUtil::outs() << "[Main FSPTA] -main-ila-sliced requested but sliced ILA unavailable; "
                            "falling back to full ILA\n";
        SVFUtil::outs() << "[Main FSPTA] Using flow-sensitive FSAM (FSPTA, sliced solve, reusing VFG_pre)\n";
    }
    timePhase("Flow-Sensitive FSAM Analysis", [&]()
    {
        if (useSlicedIla)
            mtaFSPTA = new FSPTA(slicedMhp.get(), slicedLockAnalysis.get(),
                                    ptaSlicedView.get(), /*preBuilt=*/nullptr);
        else
            mtaFSPTA = new FSPTA(mhp.get(), lockAnalysis.get(),
                                    ptaSlicedView.get(), vfgPre);
        mtaFSPTA->analyze();
        if (dumpDot)
            mtaFSPTA->getSVFG()->dump("mta_svfg");
    });
    if (!checkAndReport("Flow-Sensitive FSAM Analysis", mtaFSPTA != nullptr))
        return false;

    return true;
}

// Phase 5: Final Race Detection using sliced analysis results
bool SlicedMTA::phase5_FinalRaceDetection()
{
    SVFUtil::outs() << "\n=== Phase 5: Final Race Detection ===\n";

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

// Observe whole-program FSAM points-to and ILA (Layer 1) for soundness checking.
void SlicedMTA::observeFSAM()
{
    SVFUtil::outs() << "\n===== [OBSERVE] Flow-sensitive FSAM points-to & ILA =====\n";
    MTASVFGBuilder::numOfNewSVFGEdges = 0;
    FSPTA* fs = new FSPTA(mhp.get(), lockAnalysis.get(), nullptr, vfgPre);
    fs->analyze();
    SVFUtil::outs() << "Thread-aware (interference) SVFG edges added: "
                    << MTASVFGBuilder::numOfNewSVFGEdges << "\n";
    std::vector<std::pair<const ICFGNode*, std::string>> memOps;
    dumpFSAMPts(svfIr, fs, memOps);
    dumpILA(mhp.get(), lockAnalysis.get(), memOps);
    SVFUtil::outs() << "===== [OBSERVE] end =====\n\n";
    delete fs;
}

// Observe SLICED FSAM points-to (Layer 2): compute the FSPTA slice from the race
// targets, then run the sliced flow-sensitive analysis and dump pt at the query
// load. Used to check query preservation (sliced pt == unsliced pt).
void SlicedMTA::observeFSAMSliced()
{
    SVFUtil::outs() << "\n===== [OBSERVE-SLICED] Sliced flow-sensitive FSAM points-to =====\n";
    if (racePairs.empty())
    {
        SVFUtil::outs() << "[no race targets -- nothing to slice]\n===== [OBSERVE-SLICED] end =====\n\n";
        return;
    }
    std::set<const SVFStmt*> vuln;
    for (const RacePair& p : racePairs)
    {
        vuln.insert(p.stmt1);
        vuln.insert(p.stmt2);
    }
    PTASlicer slicer(svfIr, preAnder, mhp.get(), lockAnalysis.get(), vfgPre);
    std::set<const ICFGNode*> ptaSlicedNodes = slicer.performSlicing(vuln);
    SlicedSVFIRView view(svfIr, preAnder->getCallGraph(), svfIr->getICFG(), ptaSlicedNodes);
    SVFUtil::outs() << "Sliced to " << ptaSlicedNodes.size() << " ICFG nodes\n";

    MTASVFGBuilder::numOfNewSVFGEdges = 0;
    FSPTA* fs = new FSPTA(mhp.get(), lockAnalysis.get(), &view, vfgPre);
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

    if (!phase2_PreAnalysis(resolveIndirectCalls))
        return;

    // Observe modes: dump FSAM points-to + ILA for soundness / query preservation.
    if (Options::MTAObserve())
    {
        observeFSAM();
        SVFUtil::outs() << "\n=== Analysis Complete (observe) ===\n";
        return;
    }
    if (Options::MTAObserveSliced())
    {
        observeFSAMSliced();
        SVFUtil::outs() << "\n=== Analysis Complete (observe-sliced) ===\n";
        return;
    }

    if (Options::EnableSlicing())
    {
        if (!phase3_MTASlicingAndAnalysis()) return;
        if (!phase4_PTASlicingAndAnalysis()) return;
        if (!phase5_FinalRaceDetection()) return;
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

// Helper: Get global object variables
PointsTo SlicedMTA::_getGlobalObjectVariables(SVFIR* svfIr) {
    PointsTo globalObjVars;
    const ICFGNode* globalICFGNode = svfIr->getICFG()->getGlobalICFGNode();

    for (const SVFStmt* stmt : globalICFGNode->getSVFStmts()) {
        const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt);
        if (addrStmt != nullptr) {
            const GlobalValVar* globalVar = SVFUtil::dyn_cast<GlobalValVar>(addrStmt->getLHSVar());
            if (globalVar != nullptr) {
                globalObjVars.set(addrStmt->getRHSVarID());
            }
        }
    }

    return globalObjVars;
}

// Helper: Get PointsTo closure
PointsTo SlicedMTA::_getPointsToClosure(AndersenBase* pta, const PointsTo& pts) {
    PointsTo ptsClosure = pts;
    std::deque<NodeID> worklist;

    // Initialize worklist with all points in pts
    for (NodeID pt : pts) {
        worklist.push_back(pt);
    }

    while (!worklist.empty()) {
        NodeID pt = worklist.front();
        worklist.pop_front();

        const PointsTo& newPts = pta->getPts(pt);
        for (NodeID newPt : newPts) {
            if (!ptsClosure.test(newPt)) {
                ptsClosure.set(newPt);
                worklist.push_back(newPt);
            }
        }
    }

    return ptsClosure;
}

// Helper: Check if two functions may happen in parallel
bool SlicedMTA::_mayHappenInParallel(MHP* mhp, const FunObjVar* fun1, const FunObjVar* fun2) {
    if (fun1->hasBasicBlock() == false || fun2->hasBasicBlock() == false) {
        return false;
    }
    const ICFGNode* entry1 = fun1->getEntryBlock()->front();
    const ICFGNode* entry2 = fun2->getEntryBlock()->front();
    return mhp->mayHappenInParallelCache(entry1, entry2);
}

// Helper: Gather parallel functions of a function set
std::set<const FunObjVar*> SlicedMTA::_gatherParallelFunctions(
    CallGraph* callGraph,
    MHP* mhp,
    const std::set<const FunObjVar*>& funcSet) {
    std::set<const FunObjVar*> allFunctions;
    std::set<const FunObjVar*> mhpFunctions;

    // Get all functions from call graph
    for (CallGraph::iterator it = callGraph->begin(), eit = callGraph->end(); it != eit; ++it) {
        const CallGraphNode* cgNode = it->second;
        const FunObjVar* func = cgNode->getFunction();
        if (func != nullptr) {
            allFunctions.insert(func);
        }
    }

    // For each thread function, find functions that may happen in parallel
    for (const FunObjVar* threadFunc : funcSet) {
        std::set<const FunObjVar*> remainingFunctions = allFunctions;
        for (const FunObjVar* func : remainingFunctions) {
            if (_mayHappenInParallel(mhp, threadFunc, func)) {
                mhpFunctions.insert(func);
            }
        }
        // Remove found functions from all_functions for next iteration
        for (const FunObjVar* foundFunc : mhpFunctions) {
            allFunctions.erase(foundFunc);
        }
    }

    return mhpFunctions;
}

// Helper: Get all ICFG nodes of a function
std::set<const ICFGNode*> SlicedMTA::_getFunctionICFGNodes(const FunObjVar* function) {
    std::set<const ICFGNode*> funcICFGNodes;

    if (!function->hasBasicBlock()) {
        return funcICFGNodes;
    }

    const ICFGNode* entryICFGNode = function->getEntryBlock()->front();
    funcICFGNodes.insert(entryICFGNode);

    std::deque<const ICFGNode*> worklist;
    worklist.push_back(entryICFGNode);

    while (!worklist.empty()) {
        const ICFGNode* icfgNode = worklist.front();
        worklist.pop_front();

        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(icfgNode);
        if (callNode != nullptr) {
            const RetICFGNode* retNode = callNode->getRetICFGNode();
            if (retNode != nullptr && retNode->getFun() == function) {
                if (funcICFGNodes.find(retNode) == funcICFGNodes.end()) {
                    funcICFGNodes.insert(retNode);
                    worklist.push_back(retNode);
                }
            }
        } else {
            for (const ICFGEdge* outEdge : icfgNode->getOutEdges()) {
                const ICFGNode* dstNode = outEdge->getDstNode();
                if (dstNode->getFun() == function &&
                    funcICFGNodes.find(dstNode) == funcICFGNodes.end()) {
                    funcICFGNodes.insert(dstNode);
                    worklist.push_back(dstNode);
                }
            }
        }
    }

    return funcICFGNodes;
}

// Helper: Get all load/store statements of functions
std::set<const SVFStmt*> SlicedMTA::_getLoadStoreStatements(
    const std::set<const FunObjVar*>& functions) {
    std::set<const SVFStmt*> ldStStmts;

    for (const FunObjVar* function : functions) {
        std::set<const ICFGNode*> icfgNodes = _getFunctionICFGNodes(function);
        for (const ICFGNode* icfgNode : icfgNodes) {
            for (const SVFStmt* stmt : icfgNode->getSVFStmts()) {
                if (SVFUtil::isa<LoadStmt>(stmt) || SVFUtil::isa<StoreStmt>(stmt)) {
                    ldStStmts.insert(stmt);
                }
            }
        }
    }

    return ldStStmts;
}

// Race detection: detect candidate race pairs between load/store statements.
std::set<const SVFStmt*> SlicedMTA::detectRaceStmts(
    SVFIR* svfIr,
    AndersenBase* pta,
    MHP* mhp,
    LockAnalysis* lockAnalysis,
    CallGraph* callGraph,
    const std::set<const FunObjVar*>& threadFunctions,
    std::set<RacePair>& outRacePairs) {

    std::set<const SVFStmt*> bugStmts;
    PointsTo globalObjVars = _getGlobalObjectVariables(svfIr);

    // For each thread function
    for (const FunObjVar* threadFunc : threadFunctions) {
        // Get argument points-to set and its closure
        if (!threadFunc->hasBasicBlock() || threadFunc->arg_size() == 0) {
            continue;
        }

        NodeID argId = threadFunc->getArg(0)->getId();
        PointsTo argPts = pta->getPts(argId);
        PointsTo argPtsClosure = _getPointsToClosure(pta, argPts);
        PointsTo potentialPts = argPtsClosure;
        potentialPts |= globalObjVars;  // Union operation

        // Get thread function's parallel functions
        std::set<const FunObjVar*> threadFuncSet = {threadFunc};
        std::set<const FunObjVar*> parallelFuncs = _gatherParallelFunctions(callGraph, mhp, threadFuncSet);

        std::set<const SVFStmt*> ldStStmts1 = _getLoadStoreStatements(threadFuncSet);
        std::set<const SVFStmt*> ldStStmts2 = _getLoadStoreStatements(parallelFuncs);

        // filter out stmts with no intersection with potentialPts
        std::set<const LoadStmt*> ldStmts1;
        std::set<const StoreStmt*> stStmts1;
        std::map<const SVFStmt *, PointsTo> stmt1ToIntersection;
        for (auto stmt1 : ldStStmts1) {
            if (const LoadStmt *ldStmt1 = SVFUtil::dyn_cast<LoadStmt>(stmt1)) {
                NodeID ldVar = ldStmt1->getRHSVarID();
                PointsTo pts = pta->getPts(ldVar);
                if (pts.intersects(potentialPts)) {
                    ldStmts1.insert(ldStmt1);
                    stmt1ToIntersection[ldStmt1] = pts & potentialPts;
                }
            } else if (const StoreStmt *stStmt1 = SVFUtil::dyn_cast<StoreStmt>(stmt1)) {
                NodeID stVar = stStmt1->getLHSVarID();
                PointsTo pts = pta->getPts(stVar);
                if (pts.intersects(potentialPts)) {
                    stStmts1.insert(stStmt1);
                    stmt1ToIntersection[stStmt1] = pts & potentialPts;
                }
            }
        }

        std::set<const LoadStmt*> ldStmts2;
        std::set<const StoreStmt*> stStmts2;
        std::map<const SVFStmt *, PointsTo> stmt2ToIntersection;
        for (auto stmt2 : ldStStmts2) {
            if (const LoadStmt *ldStmt2 = SVFUtil::dyn_cast<LoadStmt>(stmt2)) {
                NodeID ldVar = ldStmt2->getRHSVarID();
                PointsTo pts = pta->getPts(ldVar);
                if (pts.intersects(potentialPts)) {
                    ldStmts2.insert(ldStmt2);
                    stmt2ToIntersection[ldStmt2] = pts & potentialPts;
                }
            } else if (const StoreStmt *stStmt2 = SVFUtil::dyn_cast<StoreStmt>(stmt2)) {
                NodeID stVar = stStmt2->getLHSVarID();
                PointsTo pts = pta->getPts(stVar);
                if (pts.intersects(potentialPts)) {
                    stStmts2.insert(stStmt2);
                    stmt2ToIntersection[stStmt2] = pts & potentialPts;
                }
            }
        }

        for (const LoadStmt *ldStmt1 : ldStmts1) {
            PointsTo pts1 = stmt1ToIntersection[ldStmt1];
            for (const StoreStmt *stStmt2 : stStmts2) {
                if (lockAnalysis->isProtectedByCommonLock(ldStmt1->getICFGNode(), stStmt2->getICFGNode()))
                    continue;
                PointsTo pts2 = stmt2ToIntersection[stStmt2];
                if (pts1.intersects(pts2)) {
                    bugStmts.insert(ldStmt1);
                    bugStmts.insert(stStmt2);
                    outRacePairs.insert(RacePair(ldStmt1, stStmt2));
                }
            }
        }

        // st1 + ld2
        for (const StoreStmt *stStmt1 : stStmts1) {
            PointsTo pts1 = stmt1ToIntersection[stStmt1];
            for (const LoadStmt *ldStmt2 : ldStmts2) {
                if (lockAnalysis->isProtectedByCommonLock(stStmt1->getICFGNode(), ldStmt2->getICFGNode()))
                    continue;
                PointsTo pts2 = stmt2ToIntersection[ldStmt2];
                if (pts1.intersects(pts2)) {
                    bugStmts.insert(stStmt1);
                    bugStmts.insert(ldStmt2);
                    outRacePairs.insert(RacePair(stStmt1, ldStmt2));
                }
            }
        }

        // st1 + st2
        for (const StoreStmt *stStmt1 : stStmts1) {
            PointsTo pts1 = stmt1ToIntersection[stStmt1];
            for (const StoreStmt *stStmt2 : stStmts2) {
                if (lockAnalysis->isProtectedByCommonLock(stStmt1->getICFGNode(), stStmt2->getICFGNode()))
                    continue;
                PointsTo pts2 = stmt2ToIntersection[stStmt2];
                if (pts1.intersects(pts2)) {
                    bugStmts.insert(stStmt1);
                    bugStmts.insert(stStmt2);
                    outRacePairs.insert(RacePair(stStmt1, stStmt2));
                }
            }
        }
    }

    return bugStmts;
}

// Detect race pairs on the sliced graph using sliced analysis results.
std::set<SlicedMTA::RacePair> SlicedMTA::detectRacePairsOnSlicedGraph(
    const std::set<RacePair>& preAnalysisRacePairs,
    BVDataPTAImpl* slicedPTA,
    MHP* slicedMHP,
    LockAnalysis* slicedLockAnalysis,
    const SlicedSVFIRView* slicedView) {

    std::set<RacePair> filteredRacePairs;

    // Filter pre-analysis race pairs: check if they still exist in sliced graph
    for (const RacePair& pair : preAnalysisRacePairs) {
        const ICFGNode* node1 = pair.stmt1->getICFGNode();
        const ICFGNode* node2 = pair.stmt2->getICFGNode();

        // Check if they may happen in parallel (using sliced MHP)
        if (!slicedMHP->mayHappenInParallelCache(node1, node2)) {
            continue;
        }

        // Check if they are protected by common lock (using sliced LockAnalysis)
        if (slicedLockAnalysis->isProtectedByCommonLock(node1, node2)) {
            continue;
        }

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
        if (pts1.intersects(pts2)) {
            filteredRacePairs.insert(pair);
        }
    }

    return filteredRacePairs;
}

