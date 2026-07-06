//===- MTA.cpp -- Analysis of multithreaded programs-------------//
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
 * MTA.cpp
 *
 *  Created on: May 14, 2014
 *      Author: Yulei Sui, Peng Di
 *
 * This file also implements SlicedMTA, the multi-stage on-demand program
 * slicing pipeline (MSli) introduced in "Multi-Stage On-Demand Program Slicing
 * for Modular Analysis of Multi-Threaded Programs" (ISSTA 2026).
 */

#include "Util/Options.h"
#include "MTA/MTA.h"
#include "MTA/MHP.h"
#include "MTA/TCT.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTAStat.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/FSMPTA.h"
#include "MTA/Slicer.h"
#include "WPA/Andersen.h"
#include "Graphs/ThreadCallGraph.h"
#include "Util/SVFUtil.h"
#include <chrono>
#include <deque>
#include <iomanip>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace SVF;
using namespace SVFUtil;

MTA::MTA() : tcg(nullptr), tct(nullptr), mhp(nullptr), lsa(nullptr)
{
    stat = std::make_unique<MTAStat>();
}

MTA::~MTA()
{
    if (tcg)
        delete tcg;

    delete mhp;
    delete lsa;
}

/*!
 * Perform data race detection
 */
bool MTA::runOnModule(SVFIR* pag)
{
    DBOUT(DGENERAL, outs() << pasMsg("MTA analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MTA analysis\n"));

    PointerAnalysis* pta = AndersenWaveDiff::createAndersenWaveDiff(pag);
    if (Options::DumpMTAGraphs())
        pta->getCallGraph()->dump("ptacg");
    pag->getICFG()->updateCallGraph(pta->getCallGraph());

    DBOUT(DGENERAL, outs() << pasMsg("Build TCT\n"));
    DBOUT(DMTA, outs() << pasMsg("Build TCT\n"));
    DOTIMESTAT(double tctStart = stat->getClk());
    tct = std::make_unique<TCT>(pta);
    tcg = tct->getThreadCallGraph();
    DOTIMESTAT(double tctEnd = stat->getClk());
    DOTIMESTAT(stat->TCTTime += (tctEnd - tctStart) / TIMEINTERVAL);

    if (pta->printStat())
    {
        stat->performThreadCallGraphStat(tcg);
        stat->performTCTStat(tct.get());
    }

    if (Options::DumpMTAGraphs())
        tcg->dump("tcg");

    mhp = computeMHP(tct.get());
    lsa = computeLocksets(tct.get());

    // MTA's only client is race detection; always report.
    reportRaces();

    return false;
}

/*!
 * Compute lock sets
 */
LockAnalysis* MTA::computeLocksets(TCT* tct)
{
    LockAnalysis* lsa = new LockAnalysis(tct);
    lsa->analyze();
    return lsa;
}

MHP* MTA::computeMHP(TCT* tct)
{
    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis\n"));

    DOTIMESTAT(double mhpStart = stat->getClk());
    MHP* mhp = new MHP(tct);
    mhp->analyze();
    DOTIMESTAT(double mhpEnd = stat->getClk());
    DOTIMESTAT(stat->MHPTime += (mhpEnd - mhpStart) / TIMEINTERVAL);

    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis finish\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis finish\n"));
    return mhp;
}

// Collect the global objects (addr-taken global vars at the global ICFG node).
PointsTo MTA::getGlobalObjectVariables(SVFIR* svfIr) {
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

// Transitive closure of a set under TWO relations: points-to (an object -> the
// objects it points to) and containment (a base object -> its field sub-objects).
// The containment step is essential for field sensitivity: a field-sensitive
// access resolves to a GepObjVar that is NOT reachable from its base by points-to
// edges, so without it a race on a (non-zero-offset) struct field -- or on an
// object reached through a struct's pointer field -- would be screened out of the
// escape set as "not shared".
PointsTo MTA::getPointsToClosure(AndersenBase* pta, const PointsTo& pts) {
    SVFIR* pag = pta->getPAG();
    PointsTo ptsClosure = pts;
    std::deque<NodeID> worklist;
    for (NodeID pt : pts) {
        worklist.push_back(pt);
    }

    while (!worklist.empty()) {
        NodeID obj = worklist.front();
        worklist.pop_front();

        for (NodeID target : pta->getPts(obj))           // points-to
            if (!ptsClosure.test(target)) { ptsClosure.set(target); worklist.push_back(target); }

        if (pag->getBaseObject(obj) != nullptr)          // containment (object nodes only)
            for (NodeID field : pta->getAllFieldsObjVars(pta->getBaseObjVarID(obj)))
                if (!ptsClosure.test(field)) { ptsClosure.set(field); worklist.push_back(field); }
    }

    return ptsClosure;
}

// C3: distinct threads must mutually interleave; the same thread self-races only
// when it is multiforked (more than one dynamic instance).
bool MTA::occurrencesRace(MHP* mhp, const RaceOccurrence& first, const RaceOccurrence& second) {
    if (first.tid != second.tid)
        return first.interleav.test(second.tid) && second.interleav.test(first.tid);
    return mhp->getTCT()->getTCTNode(first.tid)->isMultiforked();
}

// Record one order-normalised racing statement pair.
void MTA::commitRacePair(std::set<RacePair>& out,
                         const RaceOccurrence& first, const RaceOccurrence& second) {
    const SVFStmt* stmt1 = first.stmt;
    const SVFStmt* stmt2 = second.stmt;
    if (stmt2 < stmt1) std::swap(stmt1, stmt2);
    out.insert(RacePair(stmt1, stmt2));
}

// Equivalence-class race detector: screen accesses, bucket by object, collapse
// occurrences sharing the race predicate's inputs into classes, then pair classes.
std::set<const SVFStmt*> MTA::detectRace(
    SVFIR* svfIr, AndersenBase* pta, MHP* mhp, LockAnalysis* lockAnalysis,
    CallGraph* callGraph,
    std::set<RacePair>& outRacePairs) {

    std::set<const SVFStmt*> bugStmts;

    // Escape set: objects shared across threads. Seed from globals + the actual
    // argument at each fork site (the spawner's value, which a spawnee-formal
    // closure can miss), then take the transitive points-to closure.
    PointsTo seed = getGlobalObjectVariables(svfIr);
    if (ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(callGraph)) {
        const ThreadAPI* tapi = tcg->getThreadAPI();
        for (auto it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
            if (const CallICFGNode* cs = SVFUtil::dyn_cast<CallICFGNode>(*it))
                if (const ValVar* actual = tapi->getActualParmAtForkSite(cs))
                    seed |= pta->getPts(actual->getId());
    }
    const PointsTo escSet = getPointsToClosure(pta, seed);

    // One occurrence per (statement, thread instance), indexed by object (C1 ->
    // "same bucket") as collected, so the points-to set is consumed straight into
    // the buckets and never stored per occurrence.
    std::vector<RaceOccurrence> occurrences;
    Map<NodeID, std::vector<size_t>> objectToOccurrences;
    for (const auto& item : *callGraph) {
        const FunObjVar* fun = item.second->getFunction();
        if (!fun || !fun->hasBasicBlock()) continue;
        for (auto bbIt : *fun)
            for (const ICFGNode* node : bbIt.second->getICFGNodeList()) {
                if (!mhp->hasThreadStmtSet(node)) continue;          // screen 1: concurrent?
                for (const SVFStmt* stmt : svfIr->getSVFStmtList(node)) {
                    NodeID accessedPtr; bool isStore;
                    if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt)) {
                        accessedPtr = load->getRHSVarID(); isStore = false;
                    } else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt)) {
                        accessedPtr = store->getLHSVarID(); isStore = true;
                    } else continue;
                    PointsTo objects = pta->getPts(accessedPtr);
                    objects &= escSet;                               // screen 2: touches shared object?
                    if (objects.empty()) continue;
                    bool locked = lockAnalysis->isProtectedByCommonLock(node, node);
                    const size_t firstNew = occurrences.size();
                    for (const CxtThreadStmt& threadStmt : mhp->getThreadStmtSet(node))
                        occurrences.push_back({stmt, node, isStore, threadStmt.getTid(),
                                               mhp->getInterleavingThreads(threadStmt), locked});
                    for (NodeID object : objects)
                        for (size_t k = firstNew; k < occurrences.size(); ++k)
                            objectToOccurrences[object].push_back(k);
                }
            }
    }

    // Within each object, occurrences sharing the race predicate's inputs (tid,
    // interleaving, isStore, lock sig) race the same partners, so collapse into a
    // class and judge C2/C3/C4 once per class pair -- O(classes^2) not O(occ^2).
    for (const auto& objectAndOccs : objectToOccurrences) {
        struct RaceClass { bool isStore, locked; size_t rep; std::vector<size_t> members; };
        struct RaceKey {
            NodeID tid;
            bool isStore;
            NodeBS interleav;
            const ICFGNode* lockNode;

            bool operator<(const RaceKey& other) const
            {
                if (tid != other.tid)
                    return tid < other.tid;
                if (isStore != other.isStore)
                    return isStore < other.isStore;
                if (SVFUtil::cmpNodeBS(interleav, other.interleav))
                    return true;
                if (SVFUtil::cmpNodeBS(other.interleav, interleav))
                    return false;
                return lockNode < other.lockNode;
            }
        };
        std::vector<RaceClass> classes;
        OrderedMap<RaceKey, size_t> keyToClass;
        for (size_t occIdx : objectAndOccs.second) {
            const RaceOccurrence& occ = occurrences[occIdx];
            RaceKey key{occ.tid, occ.isStore, occ.interleav, occ.locked ? occ.node : nullptr};
            auto found = keyToClass.find(key);
            if (found == keyToClass.end()) {
                keyToClass[key] = classes.size();
                classes.push_back({occ.isStore, occ.locked, occIdx, {occIdx}});
            } else
                classes[found->second].members.push_back(occIdx);
        }
        for (size_t firstIdx = 0; firstIdx < classes.size(); ++firstIdx)
            for (size_t secondIdx = firstIdx; secondIdx < classes.size(); ++secondIdx) {
                RaceClass& firstClass = classes[firstIdx];
                RaceClass& secondClass = classes[secondIdx];
                if (!firstClass.isStore && !secondClass.isStore) continue;   // C2: >=1 write
                const RaceOccurrence& firstRep = occurrences[firstClass.rep];
                const RaceOccurrence& secondRep = occurrences[secondClass.rep];
                if (!occurrencesRace(mhp, firstRep, secondRep)) continue;    // C3
                // C4 + emit. Lock relation is uniform per class (the reps decide it),
                // so emit a statement-COVERING set -- every racy member as an endpoint.
                if (firstIdx != secondIdx) {
                    if (firstClass.locked && secondClass.locked &&
                        lockAnalysis->isProtectedByCommonLock(firstRep.node, secondRep.node))
                        continue;
                    for (size_t memberIdx : firstClass.members)
                        commitRacePair(outRacePairs, occurrences[memberIdx],
                                       occurrences[secondClass.members[0]]);
                    for (size_t memberIdx : secondClass.members)
                        commitRacePair(outRacePairs, occurrences[firstClass.members[0]],
                                       occurrences[memberIdx]);
                } else {
                    const std::vector<size_t>& members = firstClass.members;
                    // Multiforked self-race: every member self-races a concurrent
                    // instance of itself, independent of any cross-race below.
                    for (size_t memberIdx : members)
                        if (!firstClass.locked ||
                            !lockAnalysis->isProtectedByCommonLock(occurrences[memberIdx].node,
                                                                   occurrences[memberIdx].node))
                            commitRacePair(outRacePairs, occurrences[memberIdx], occurrences[memberIdx]);
                    // Cross-race needs >=2 members: two distinct occurrences to pair.
                    if (members.size() >= 2 &&
                        (!firstClass.locked ||
                         !lockAnalysis->isProtectedByCommonLock(occurrences[members[0]].node,
                                                                occurrences[members[1]].node)))
                        for (size_t pos = 0; pos < members.size(); ++pos)
                            commitRacePair(outRacePairs, occurrences[members[pos]],
                                           occurrences[members[pos == 0 ? 1 : 0]]);
                }
            }
    }

    for (const RacePair& r : outRacePairs) { bugStmts.insert(r.stmt1); bugStmts.insert(r.stmt2); }
    return bugStmts;
}

void MTA::reportRaces()
{
    DBOUT(DGENERAL, outs() << pasMsg("Starting Race Detection\n"));

    SVFIR* pag = SVFIR::getPAG();
    AndersenBase* pta = AndersenWaveDiff::createAndersenWaveDiff(pag);
    CallGraph* callGraph = pta->getCallGraph();

    // Shared equivalence-class detector (the same one the slicing pipeline uses),
    // run over the Andersen pre-analysis with this MTA's MHP/lock results.
    std::set<RacePair> racePairs;
    detectRace(pag, pta, mhp, lsa, callGraph, racePairs);

    for (const RacePair& rp : racePairs)
        outs() << SVFUtil::bugMsg1("race pair(") << " stmt1: " << rp.stmt1->toString()
               << ", stmt2: " << rp.stmt2->toString() << SVFUtil::bugMsg1(")") << "\n";
}

//===----------------------------------------------------------------------===//
// SlicedMTA -- Multi-stage on-demand slicing race detection (MSli).
//
// Library-side orchestration of the slicing pipeline over the SVFIR.
//===----------------------------------------------------------------------===//

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

u32_t slicedMaxContextLen()
{
    if (Options::EnableSlicing() && !Options::MaxContextLen.isSet())
        return 2;
    return Options::MaxContextLen();
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

    // Step 2: Build Thread Create Tree (the caller forces -max-cxt to 0 around the
    // whole pre-analysis when slicing; see runOnModule).
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

        // ILA slicing sources = [INIT] race statements + [THREAD-VF] sources. Keep
        // a candidate edge's query (see MTASVFGBuilder::getThreadVFQueryMap) only if
        // both endpoints survive the FSPTA slice -- i.e. the edge is in
        // ThreadVF(VFG'_pre). Closure computed here (pre<->pre) and reused by PTA slicing.
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
        SVFUtil::outs() << "[SlicedTCT] Using max context length: " << maxContextLen
                        << " (from -max-cxt)\n";
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

    // Step 5: Main FSMPTA phase (flow-sensitive FSAM over a thread-aware SVFG).
    // -main-ila-sliced: rebuild the thread-aware value flow from the SLICED ILA
    // (paper-faithful; relies on [THREAD-VF] keeping the queried witnesses; costs
    // a fresh SVFG). Default: reuse VFG_pre and restrict the solve to the PTA
    // slice -- same result under query preservation, no second SVFG build.
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

// Build a lock analysis over the WHOLE ICFG (every node kept => real control flow,
// no bridged edges). Used for the final detection's lock signature so the sliced
// run reproduces the whole-program lock relation exactly (query preservation).
LockAnalysis* SlicedMTA::buildFullLockAnalysis()
{
    if (fullLockAnalysis != nullptr)
        return fullLockAnalysis.get();
    std::set<const ICFGNode*> allNodes;
    for (ICFG::iterator it = svfIr->getICFG()->begin(), eit = svfIr->getICFG()->end(); it != eit; ++it)
        allNodes.insert(it->second);
    fullLockView = std::make_unique<SlicedSVFIRView>(
                       svfIr, preAnder->getCallGraph(), svfIr->getICFG(), allNodes);
    fullLockTCT = std::make_unique<SlicedTCT>(preAnder, fullLockView.get(), slicedMaxContextLen());
    fullLockAnalysis = std::make_unique<SlicedLockAnalysis>(fullLockTCT.get(), fullLockView.get());
    fullLockAnalysis->analyze();
    return fullLockAnalysis.get();
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
    LockAnalysis* fullLock = buildFullLockAnalysis();  // whole-ICFG lock (no bridging)
    timePhase("Final Race Detection", [&]()
    {
        detectedPairs = detectRacePairsOnSlicedGraph(
                            getMainPTA(),     // Use flow-sensitive FSAM points-to
                            slicedMhp.get(), fullLock);
    });

    // Distinct racy statements (the endpoints of the race pairs) -- a stabler,
    // smaller-to-report metric than the pair count.
    std::set<const SVFStmt*> racyStmts;
    for (const RacePair& rp : detectedPairs) { racyStmts.insert(rp.stmt1); racyStmts.insert(rp.stmt2); }

    SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
    SVFUtil::outs() << "Race pairs (pre-analysis): " << racePairs.size() << "\n";
    SVFUtil::outs() << "Race pairs (sliced graph): " << detectedPairs.size() << "\n";
    SVFUtil::outs() << "Race statements (sliced graph): " << racyStmts.size() << "\n";
    // Machine-readable line for the artifact's `msli` table generator: the race
    // statements reported after slicing (the preservation metric).
    SVFUtil::outs() << "[MSLI-RQ] mode=MSli alarms=" << racyStmts.size() << "\n";

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
                            getMainPTA(), slicedMhp.get(), slicedLockAnalysis.get());
    });

    std::set<const SVFStmt*> racyStmts;
    for (const RacePair& rp : detectedPairs) { racyStmts.insert(rp.stmt1); racyStmts.insert(rp.stmt2); }

    SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
    SVFUtil::outs() << "Race pairs (pre-analysis): " << racePairs.size() << "\n";
    SVFUtil::outs() << "Race pairs (whole program): " << detectedPairs.size() << "\n";
    SVFUtil::outs() << "Race statements (whole program): " << racyStmts.size() << "\n";
    SVFUtil::outs() << "[MSLI-RQ] mode=FSAM alarms=" << racyStmts.size() << "\n";
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

    const u32_t mainCxt = slicedMaxContextLen();
    if (Options::EnableSlicing())
        Options::MaxContextLen.setValue(0);
    const bool preOk = runPreAnalysis(resolveIndirectCalls);
    Options::MaxContextLen.setValue(mainCxt);
    if (!preOk)
        return;

    // Observe mode: dump FSAM points-to + ILA instead of detecting races; the
    // sliced/whole-program variant follows -enable-slicing.
    if (Options::MTAObserve())
    {
        if (Options::EnableSlicing())
            runObserveFSAMSliced();
        else
            runObserveFSAM();
        SVFUtil::outs() << "\n=== Analysis Complete (observe) ===\n";
        return;
    }

    if (Options::EnableSlicing())
    {
        if (!runMTASlicingAndAnalysis()) return;
        if (!runPTASlicingAndAnalysis()) return;
        if (!runFinalRaceDetection()) return;
    }
    else
    {
        runWholeProgramDetection();
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
    BVDataPTAImpl* slicedPTA,
    MHP* slicedMHP,
    LockAnalysis* slicedLockAnalysis) {

    std::set<RacePair> filteredRacePairs;

    // Re-derive candidates at the main context on this graph: slicedMHP carries
    // only kept nodes, so the sliced and whole runs invoke the identical detector.
    std::set<RacePair> candidatePairs;
    MTA::detectRace(svfIr, preAnder, slicedMHP, slicedLockAnalysis,
                    preAnder->getCallGraph(), candidatePairs);

    // The only remaining screen is the flow-sensitive points-to refinement (the
    // ILA conditions C1-C4 were already applied by detectRace above).
    for (const RacePair& pair : candidatePairs) {
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
