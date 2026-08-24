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
 */

#include "Util/Options.h"
#include "MTA/MTA.h"
#include "MTA/MHP.h"
#include "MTA/TCT.h"
#include "MTA/LockAnalysis.h"
#include "MTA/MTAStat.h"
#include "MTA/MTASVFGBuilder.h"
#include "MTA/FSMPTA.h"
#include "MTA/MTASlicer.h"
#include "WPA/Andersen.h"
#include "Graphs/ThreadCallGraph.h"
#include "Util/SVFUtil.h"
#include <algorithm>
#include <deque>
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
    tct = TCT::create(pta);
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
    lsa->analyze(PAG::getPAG()->getICFG(), const_cast<CallGraph*>(PAG::getPAG()->getCallGraph()));
    return lsa;
}

MHP* MTA::computeMHP(TCT* tct)
{
    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis\n"));

    DOTIMESTAT(double mhpStart = stat->getClk());
    std::unique_ptr<MHP> mhp = MHP::create(
                                   tct, PAG::getPAG()->getICFG(),
                                   const_cast<CallGraph*>(PAG::getPAG()->getCallGraph()));
    mhp->analyze(PAG::getPAG()->getICFG(), const_cast<CallGraph*>(PAG::getPAG()->getCallGraph()));
    DOTIMESTAT(double mhpEnd = stat->getClk());
    DOTIMESTAT(stat->MHPTime += (mhpEnd - mhpStart) / TIMEINTERVAL);

    DBOUT(DGENERAL, outs() << pasMsg("MHP analysis finish\n"));
    DBOUT(DMTA, outs() << pasMsg("MHP analysis finish\n"));
    return mhp.release();
}

// Collect the global objects (addr-taken global vars at the global ICFG node).
PointsTo MTA::getGlobalObjectVariables(SVFIR* svfir)
{
    PointsTo globalObjVars;
    const ICFGNode* globalICFGNode = svfir->getICFG()->getGlobalICFGNode();

    for (const SVFStmt* stmt : globalICFGNode->getSVFStmts())
    {
        const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt);
        if (addrStmt != nullptr)
        {
            const GlobalValVar* globalVar =
                SVFUtil::dyn_cast<GlobalValVar>(addrStmt->getLHSVar());
            if (globalVar != nullptr)
            {
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
PointsTo MTA::getPointsToClosure(AndersenBase* pta, const PointsTo& pts)
{
    SVFIR* pag = pta->getPAG();
    PointsTo ptsClosure = pts;
    std::deque<NodeID> worklist;
    for (NodeID pt : pts)
    {
        worklist.push_back(pt);
    }

    while (!worklist.empty())
    {
        NodeID obj = worklist.front();
        worklist.pop_front();

        for (NodeID target : pta->getPts(obj))           // points-to
            if (!ptsClosure.test(target))
            {
                ptsClosure.set(target);
                worklist.push_back(target);
            }

        if (pag->getBaseObject(obj) != nullptr)          // containment (object nodes only)
            for (NodeID field : pta->getAllFieldsObjVars(pta->getBaseObjVarID(obj)))
                if (!ptsClosure.test(field))
                {
                    ptsClosure.set(field);
                    worklist.push_back(field);
                }
    }

    return ptsClosure;
}

// C3: distinct threads must mutually interleave; the same thread self-races only
// when it is multiforked (more than one dynamic instance).
bool MTA::occurrencesRace(
    MHP* mhp, const RaceOccurrence& first, const RaceOccurrence& second)
{
    if (first.tid != second.tid)
        return first.interleaving->test(second.tid) &&
               second.interleaving->test(first.tid);
    return mhp->getTCT()->getTCTNode(first.tid)->isMultiforked();
}

// Record one order-normalised racing statement pair.
void MTA::commitRacePair(std::set<RacePair>& out,
                         const RaceOccurrence& first,
                         const RaceOccurrence& second)
{
    out.emplace(first.stmt, second.stmt);
}

bool MTA::RaceClassKey::operator<(const RaceClassKey& other) const
{
    if (tid != other.tid)
        return tid < other.tid;
    if (isStore != other.isStore)
        return isStore < other.isStore;
    if (SVFUtil::cmpNodeBS(*interleaving, *other.interleaving))
        return true;
    if (SVFUtil::cmpNodeBS(*other.interleaving, *interleaving))
        return false;
    if (locked != other.locked)
        return locked < other.locked;
    return lockNodeId < other.lockNodeId;
}

void MTA::collectRaceOccurrences(
    SVFIR* svfir, AndersenBase* pta, MHP* mhp,
    LockAnalysis* lockAnalysis, CallGraph* callGraph,
    const PointsTo& escapedObjects,
    std::vector<RaceOccurrence>& occurrences,
    ObjectToRaceOccurrences& objectToOccurrences)
{
    for (const auto& item : *callGraph)
    {
        const FunObjVar* fun = item.second->getFunction();
        if (!fun || !fun->hasBasicBlock())
            continue;
        for (auto bbIt : *fun)
            for (const ICFGNode* node : bbIt.second->getICFGNodeList())
            {
                const MHP::NodeThreadSummary* threadSummary =
                    mhp->getThreadSummary(node);
                if (threadSummary == nullptr)
                    continue;
                for (const SVFStmt* stmt : svfir->getSVFStmtList(node))
                {
                    NodeID accessedPtr;
                    bool isStore;
                    if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
                    {
                        accessedPtr = load->getRHSVarID();
                        isStore = false;
                    }
                    else if (const StoreStmt* store =
                                 SVFUtil::dyn_cast<StoreStmt>(stmt))
                    {
                        accessedPtr = store->getLHSVarID();
                        isStore = true;
                    }
                    else
                        continue;

                    PointsTo objects = pta->getPts(accessedPtr);
                    objects &= escapedObjects;
                    if (objects.empty())
                        continue;

                    const bool locked =
                        lockAnalysis->isProtectedByCommonLock(node, node);
                    const size_t firstNewOccurrence = occurrences.size();
                    for (const auto& tidAndInterleaving :
                            threadSummary->interleavingByTid)
                        occurrences.push_back(
                    {
                        stmt, node, isStore, tidAndInterleaving.first,
                        &tidAndInterleaving.second, locked});
                    for (NodeID object : objects)
                        for (size_t index = firstNewOccurrence;
                                index < occurrences.size(); ++index)
                            objectToOccurrences[object].push_back(index);
                }
            }
    }
}

std::vector<MTA::RaceClass> MTA::buildRaceClasses(
    const std::vector<RaceOccurrence>& occurrences,
    const std::vector<size_t>& occurrenceIndices)
{
    std::vector<RaceClass> classes;
    OrderedMap<RaceClassKey, size_t> keyToClass;
    for (size_t occurrenceIndex : occurrenceIndices)
    {
        const RaceOccurrence& occurrence = occurrences[occurrenceIndex];
        const RaceClassKey key
        {
            occurrence.tid, occurrence.isStore, occurrence.interleaving,
            occurrence.locked,
            occurrence.locked ? occurrence.node->getId() : 0};
        const auto found = keyToClass.find(key);
        if (found == keyToClass.end())
        {
            keyToClass[key] = classes.size();
            classes.push_back(
            {
                occurrence.isStore, occurrence.locked, occurrenceIndex,
                {occurrenceIndex}});
        }
        else
            classes[found->second].members.push_back(occurrenceIndex);
    }
    return classes;
}

void MTA::emitRacePairs(
    MHP* mhp, LockAnalysis* lockAnalysis,
    const std::vector<RaceOccurrence>& occurrences,
    const std::vector<RaceClass>& classes,
    std::set<RacePair>& outRacePairs)
{
    for (size_t firstIndex = 0; firstIndex < classes.size(); ++firstIndex)
        for (size_t secondIndex = firstIndex;
                secondIndex < classes.size(); ++secondIndex)
        {
            const RaceClass& firstClass = classes[firstIndex];
            const RaceClass& secondClass = classes[secondIndex];
            if (!firstClass.isStore && !secondClass.isStore)
                continue;

            const RaceOccurrence& firstRepresentative =
                occurrences[firstClass.representative];
            const RaceOccurrence& secondRepresentative =
                occurrences[secondClass.representative];
            if (!occurrencesRace(
                        mhp, firstRepresentative, secondRepresentative))
                continue;

            if (firstIndex != secondIndex)
            {
                if (firstClass.locked && secondClass.locked &&
                        lockAnalysis->isProtectedByCommonLock(
                            firstRepresentative.node, secondRepresentative.node))
                    continue;
                for (size_t memberIndex : firstClass.members)
                    for (size_t otherIndex : secondClass.members)
                        commitRacePair(outRacePairs, occurrences[memberIndex],
                                       occurrences[otherIndex]);
            }
            else
            {
                const std::vector<size_t>& members = firstClass.members;
                if (firstClass.locked &&
                        lockAnalysis->isProtectedByCommonLock(
                            firstRepresentative.node, firstRepresentative.node))
                    continue;
                for (size_t firstPosition = 0;
                        firstPosition < members.size(); ++firstPosition)
                    for (size_t secondPosition = firstPosition;
                            secondPosition < members.size(); ++secondPosition)
                        commitRacePair(
                            outRacePairs, occurrences[members[firstPosition]],
                            occurrences[members[secondPosition]]);
            }
        }
}

// Equivalence-class race detector: screen accesses, bucket by object, collapse
// occurrences sharing the race predicate's inputs into classes, then pair classes.
std::set<const SVFStmt*> MTA::detectRace(
    SVFIR* svfir, AndersenBase* pta, MHP* mhp, LockAnalysis* lockAnalysis,
    CallGraph* callGraph,
    std::set<RacePair>& outRacePairs)
{

    outRacePairs.clear();
    std::set<const SVFStmt*> bugStmts;

    // Escape set: objects shared across threads. Seed from globals + the actual
    // argument at each fork site (the spawner's value, which a spawnee-formal
    // closure can miss), then take the transitive points-to closure.
    PointsTo seed = getGlobalObjectVariables(svfir);
    if (ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(callGraph))
    {
        const ThreadAPI* tapi = tcg->getThreadAPI();
        for (auto it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
            if (const CallICFGNode* cs = SVFUtil::dyn_cast<CallICFGNode>(*it))
                if (const ValVar* actual = tapi->getActualParmAtForkSite(cs))
                    seed |= pta->getPts(actual->getId());
    }
    const PointsTo escSet = getPointsToClosure(pta, seed);

    std::vector<RaceOccurrence> occurrences;
    ObjectToRaceOccurrences objectToOccurrences;
    collectRaceOccurrences(
        svfir, pta, mhp, lockAnalysis, callGraph, escSet,
        occurrences, objectToOccurrences);

    // Within each object, occurrences sharing the race predicate's inputs (tid,
    // interleaving, isStore, lock sig) race the same partners, so collapse into a
    // class and judge C2/C3/C4 once per class pair -- O(classes^2) not O(occ^2).
    for (const auto& objectAndOccs : objectToOccurrences)
    {
        const std::vector<RaceClass> classes =
            buildRaceClasses(occurrences, objectAndOccs.second);
        emitRacePairs(
            mhp, lockAnalysis, occurrences, classes, outRacePairs);
    }

    for (const RacePair& pair : outRacePairs)
    {
        bugStmts.insert(pair.stmt1);
        bugStmts.insert(pair.stmt2);
    }
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

// Output statistics for the original (unsliced) SVFIR.
void SlicedMTA::reportOriginalStatistics(SVFIR* svfir)
{
    size_t icfgNodeCount = 0;
    for (ICFG::iterator it = svfir->getICFG()->begin(), eit = svfir->getICFG()->end(); it != eit; ++it)
        icfgNodeCount++;

    size_t functionCount = 0;
    for (auto it = svfir->getCallGraph()->begin(), eit = svfir->getCallGraph()->end(); it != eit; ++it)
        functionCount++;

    size_t pagStmtCount = 0;
    for (ICFG::iterator it = svfir->getICFG()->begin(),
            eit = svfir->getICFG()->end(); it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        if (svfir->hasSVFStmtList(node))
            pagStmtCount += svfir->getSVFStmtList(node).size();
    }

    SVFUtil::outs() << "\n[Original SVFIR] Statistics:\n";
    SVFUtil::outs() << "  ICFG nodes: " << icfgNodeCount << "\n";
    SVFUtil::outs() << "  Functions: " << functionCount << "\n";
    SVFUtil::outs() << "  PAG statements: " << pagStmtCount << "\n";
}

std::set<const ICFGNode*> SlicedMTA::collectICFGNodes(
    SVFG* svfg, const NodeBS& svfgNodeIds)
{
    std::set<const ICFGNode*> nodes;
    for (NodeID id : svfgNodeIds)
    {
        if (!svfg->hasSVFGNode(id))
            continue;
        if (const StmtVFGNode* stmtNode =
                    SVFUtil::dyn_cast<StmtVFGNode>(svfg->getSVFGNode(id)))
            if (stmtNode->getICFGNode() != nullptr)
                nodes.insert(stmtNode->getICFGNode());
    }
    return nodes;
}

void SlicedMTA::reportPTASliceStatistics(
    const std::set<const ICFGNode*>& icfgNodes)
{
    std::set<const FunObjVar*> functions;
    std::set<const SVFStmt*> statements;
    for (const ICFGNode* node : icfgNodes)
    {
        if (node->getFun() != nullptr)
            functions.insert(node->getFun());
        statements.insert(node->getSVFStmts().begin(), node->getSVFStmts().end());
    }

    SVFUtil::outs() << "[PTA Sliced] Statistics:\n";
    SVFUtil::outs() << "  ICFG nodes: " << icfgNodes.size() << "\n";
    SVFUtil::outs() << "  Functions: " << functions.size() << "\n";
    SVFUtil::outs() << "  PAG statements: " << statements.size() << "\n";
}

std::string SlicedMTA::raceStatementKey(const SVFStmt* statement)
{
    std::string key;
    const ICFGNode* node = statement->getICFGNode();
    const FunObjVar* function = node == nullptr ? nullptr : node->getFun();
    const SVFBasicBlock* block = statement->getBB();
    const SVFVar* value = statement->getValue();

    const std::string fields[] =
    {
        std::to_string(statement->getEdgeKind()),
        function == nullptr ? std::string() : function->getName(),
        block == nullptr ? std::string() : block->getName(),
        node == nullptr ? std::string() : node->getSourceLoc(),
        value == nullptr ? std::string() : value->getName(),
        value == nullptr ? std::string() : value->getSourceLoc(),
        value != nullptr && value->hasLLVMValue()
        ? value->valueOnlyToString() : std::string()
    };
    for (const std::string& field : fields)
    {
        key += std::to_string(field.size());
        key += ':';
        key += field;
    }
    return key;
}

void SlicedMTA::updateDigest(u64_t& digest, const std::string& value)
{
    for (unsigned char byte : value)
    {
        digest ^= static_cast<u64_t>(byte);
        digest *= 1099511628211ULL;
    }
    digest ^= 0xffULL;
    digest *= 1099511628211ULL;
}

SlicedMTA::RaceDigests SlicedMTA::computeRaceDigests(
    const std::set<RacePair>& pairs)
{
    Map<const SVFStmt*, std::string> statementToKey;
    std::set<std::string> statementKeys;
    for (const RacePair& pair : pairs)
    {
        statementToKey.emplace(pair.stmt1, std::string());
        statementToKey.emplace(pair.stmt2, std::string());
    }
    for (auto& statementAndKey : statementToKey)
    {
        statementAndKey.second = raceStatementKey(statementAndKey.first);
        statementKeys.insert(statementAndKey.second);
    }

    RaceDigests digests{1469598103934665603ULL, 1469598103934665603ULL};
    for (const std::string& key : statementKeys)
        updateDigest(digests.alarm, key);

    std::set<std::pair<std::string, std::string>> pairKeys;
    for (const RacePair& pair : pairs)
    {
        std::string first = statementToKey.at(pair.stmt1);
        std::string second = statementToKey.at(pair.stmt2);
        if (second < first)
            std::swap(first, second);
        pairKeys.emplace(std::move(first), std::move(second));
    }

    for (const auto& pair : pairKeys)
    {
        updateDigest(digests.pair, pair.first);
        updateDigest(digests.pair, pair.second);
    }
    return digests;
}

SlicedMTA::SlicedMTA() : mainContextDepth(Options::MaxContextLen())
{
}

SlicedMTA::~SlicedMTA() = default;

BVDataPTAImpl* SlicedMTA::getMainPTA() const
{
    // The main FSMPTA phase is the flow-sensitive FSAM (FSMPTA), a
    // BVDataPTAImpl, so the downstream race detector queries it polymorphically.
    return mainFSMPTA.get();
}

std::set<const SVFStmt*> SlicedMTA::getVulnerableStmts() const
{
    std::set<const SVFStmt*> vulnerableStatements;
    for (const RacePair& pair : racePairs)
    {
        vulnerableStatements.insert(pair.stmt1);
        vulnerableStatements.insert(pair.stmt2);
    }
    return vulnerableStatements;
}

// Pre-Analysis (Pointer Analysis + TCT + MHP & Lock + Race Detection).
// Build the BaseSVFG once, then attach its pre-analysis TVF overlay. The base is
// reused by the main FSMPTA after replacing only that overlay.
void SlicedMTA::buildPreAnalysisSVFG()
{
    ScopedPhaseTimer timer("Build thread-aware VFG_pre");
    // Treat fork/join as calls so the SVFG carries the thread-oblivious
    // (fork/join-ordered) value flow.
    if (ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(preAndersen->getCallGraph()))
    {
        tcg->updateCallGraph(preAndersen);
        tcg->updateJoinEdge(preAndersen);
    }
    preSVFGBuilder = std::make_unique<MTASVFGBuilder>(
                         mhp.get(), lockAnalysis.get(),
                         MTASVFGBuilder::InterferenceEdgeMode::SlicingOnly);
    // The Pre-TVF overlay is sliced, never solved: omit its interference-edge
    // labels. Main-TVF labels are added after the pre overlay is removed.
    preSVFG = preSVFGBuilder->buildPTROnlySVFG(preAndersen);
    if (isMTAStatEnabled())
        SVFUtil::outs() << "[BaseSVFG] built once: " << preSVFG->getSVFGNodeNum()
                        << " nodes; [Pre-TVF] "
                        << preSVFGBuilder->getThreadAwareEdgeCount()
                        << " interference edges\n";
}

bool SlicedMTA::runPreAnalysis()
{
    SVFUtil::outs() << "\n=== Pre-Analysis ===\n";

    const bool dumpDot = Options::DumpMTAGraphs();

    // The LLVM-aware tool has already run Andersen and materialised its resolved
    // indirect calls into the PAG. Reuse that same analysis throughout MSli.
    threadCallGraph =
        SVFUtil::dyn_cast<ThreadCallGraph>(preAndersen->getCallGraph());
    if (threadCallGraph == nullptr)
    {
        SVFUtil::errs() << "[ERROR] Thread call graph failed\n";
        return false;
    }

    // Step 2: Build the context-insensitive pre-analysis Thread Creation Tree.
    {
        ScopedPhaseTimer timer("Create Thread Create Tree");
        tct = TCT::create(preAndersen, 0);
    }
    if (dumpDot)
        tct->dump("original_tct");

    // A thread with several instances at the main depth must be multiforked in
    // this depth-0 TCT, or the pre-analysis under-approximates the main phase.
    {
        ScopedPhaseTimer timer("Mark truncation-merged multiforked threads");
        std::unique_ptr<TCT> deepTct =
            TCT::create(preAndersen, mainContextDepth);

        // >1 instance at the main depth, or a single instance that is itself
        // multiforked (merged just beyond the main depth), marks the fork site.
        Map<const ICFGNode*, u32_t> forkSiteInstances;
        for (const auto& deepPair : *deepTct)
            if (const ICFGNode* forkSite = deepPair.second->getCxtThread().getThread())
            {
                ++forkSiteInstances[forkSite];
                if (deepPair.second->isMultiforked())
                    forkSiteInstances[forkSite] = 2;
            }

        for (const auto& prePair : *tct)
        {
            const ICFGNode* forkSite = prePair.second->getCxtThread().getThread();
            if (forkSite == nullptr)
                continue;
            Map<const ICFGNode*, u32_t>::const_iterator fIt = forkSiteInstances.find(forkSite);
            if (fIt != forkSiteInstances.end() && fIt->second > 1)
                prePair.second->setMultiforked(true);
        }
    }

    // Step 3: Interleaving and Lock Analysis
    {
        ScopedPhaseTimer ilaTimer("Run Interleaving and Lock Analysis");
        {
            ScopedPhaseTimer timer("ILA: construct MHP/ForkJoin");
            mhp = MHP::create(
                      tct.get(), svfir->getICFG(),
                      const_cast<CallGraph*>(svfir->getCallGraph()));
        }
        {
            ScopedPhaseTimer timer("ILA: MHP propagation");
            mhp->analyze(svfir->getICFG(), const_cast<CallGraph*>(svfir->getCallGraph()));
        }
        {
            ScopedPhaseTimer timer("ILA: Lock analysis");
            lockAnalysis = std::make_unique<LockAnalysis>(tct.get());
            lockAnalysis->analyze(svfir->getICFG(), const_cast<CallGraph*>(svfir->getCallGraph()));
        }
    }

    // Step 4: Detect thread functions
    {
        ScopedPhaseTimer timer("Detect Thread Functions");
        hasThreadFunctions = MTA::hasThreadFunctions(preAndersen->getCallGraph());
    }
    if (!hasThreadFunctions)
    {
        SVFUtil::outs() << "[WARNING] No thread functions found\n";
        return true; // Not an error, just no threads to analyze
    }

    // Step 5: Detect race statements
    std::set<const SVFStmt*> vulnerableStatements;
    {
        ScopedPhaseTimer timer("Detect Race Statements");
        // Shared equivalence-class detector (the same one MTA::reportRaces uses).
        vulnerableStatements = MTA::detectRace(
                                   svfir, preAndersen, mhp.get(), lockAnalysis.get(),
                                   preAndersen->getCallGraph(), racePairs);
    }
    SVFUtil::outs() << "Found " << vulnerableStatements.size() << " vulnerable statements\n";
    SVFUtil::outs() << "Found " << racePairs.size() << " race pairs\n";

    // Step 6: build the thread-aware VFG only when a downstream slice/solve
    // exists. No-race programs end after the pre-detector.
    if (!racePairs.empty())
        buildPreAnalysisSVFG();
    else
        SVFUtil::outs() << "[SKIP] No race candidates; VFG_pre is unnecessary\n";

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

    const bool dumpDot = Options::DumpMTAGraphs();

    // Step 1: Get vulnerable statements from race pairs
    std::set<const SVFStmt*> vulnerableStatements = getVulnerableStmts();

    std::set<const ICFGNode*> mtaSlicedNodes;

    if (Options::MTASingleStageSlicing())
    {
        // Single-pass baseline (MSli §3/§5.4): one unified slice (V_Single)
        // combining synchronization + data + call dependence, shared by both the
        // ILA and the FSPTA stages. Computed once here; reused in PTA slicing.
        SVFUtil::outs() << "[Slicing Mode] Single unified slice (V_Single) for ILA + FSPTA\n";
        singleSlicer = std::make_unique<SingleSlicer>(
                           svfir, preAndersen, mhp.get(), lockAnalysis.get(),
                           preSVFG /* data dependence over the thread-aware VFG_pre */);
        {
            ScopedPhaseTimer timer("Unified Slicing");
            ValueFlowSlice singleSlice =
                singleSlicer->runSlicing(vulnerableStatements);
            singleSlicedNodes = std::move(singleSlice.icfgNodes);
            singleSlicedSVFGNodeIds = singleSlice.nodeIds();
        }
        mtaSlicedNodes = singleSlicedNodes;
        if (isMTAStatEnabled())
            SVFUtil::outs() << "Unified sliced to " << mtaSlicedNodes.size()
                            << " nodes\n";
    }
    else
    {
        SVFUtil::outs() << "[Slicing Mode] Differential slices (separate ILA + FSPTA)\n";
        multiStageSlicer = std::make_unique<MultiStageSlicer>(
                               svfir, preAndersen, mhp.get(), lockAnalysis.get(), preSVFG);

        // ILA slicing sources = [INIT] race statements + [THREAD-VF] sources. Keep
        // a candidate edge's query (see MTASVFGBuilder::getThreadVFQueryMap) only if
        // both endpoints survive the FSPTA slice -- i.e. the edge is in
        // ThreadVF(VFG'_pre). Closure computed here (pre<->pre) and reused by PTA slicing.
        std::set<const ICFGNode*> threadVFSources;
        selectedThreadVFCandidates.clear();
        {
            ScopedPhaseTimer timer("Select THREAD-VF slicing sources");
            if (preSVFGBuilder)
            {
                multiStageSlicer->computePreCandidateSlice(vulnerableStatements);
                const ValueFlowSlice& preCandidate =
                    multiStageSlicer->getPreCandidateSlice();
                preCandidateSolveNodeIds =
                    FSMPTA<const SlicedSVFGView*>::buildExecutionDependencyClosure(
                        preSVFG, preAndersen, preCandidate.nodeIds());
                if (preCandidateSolveNodeIds.empty() &&
                        !preCandidate.svfgNodes.empty())
                {
                    SVFUtil::errs() << "[ERROR] Failed to execution-close VFG'_pre\n";
                    return false;
                }
                if (isMTAStatEnabled())
                    SVFUtil::outs()
                            << "[VFG'_pre] " << preCandidate.svfgNodes.size()
                            << " dependency nodes, "
                            << preCandidateSolveNodeIds.count()
                            << " execution-closure nodes\n";

                // The query map can be large; use node-ID membership rather than
                // two ordered-set lookups for every candidate edge.
                for (const auto& entry : preSVFGBuilder->getThreadVFQueryMap())
                {
                    const MTASVFGBuilder::ThreadVFEdge& edge = entry.first;
                    if (preCandidateSolveNodeIds.test(edge.first->getId()) &&
                            preCandidateSolveNodeIds.test(edge.second->getId()))
                    {
                        selectedThreadVFCandidates.emplace_back(
                            edge.first->getId(), edge.second->getId());
                        // The query value holds only the lock-span witnesses; the
                        // endpoints are implicit in the edge key.
                        threadVFSources.insert(edge.first->getICFGNode());
                        threadVFSources.insert(edge.second->getICFGNode());
                        threadVFSources.insert(entry.second.begin(), entry.second.end());
                    }
                }
            }
        }
        std::sort(selectedThreadVFCandidates.begin(),
                  selectedThreadVFCandidates.end());
        selectedThreadVFCandidates.erase(
            std::unique(selectedThreadVFCandidates.begin(),
                        selectedThreadVFCandidates.end()),
            selectedThreadVFCandidates.end());
        {
            ScopedPhaseTimer timer("MTA Slicing");
            mtaSlicedNodes = multiStageSlicer->runILASlicing(vulnerableStatements, threadVFSources);
        }
        if (isMTAStatEnabled())
            SVFUtil::outs() << "MTA sliced to " << mtaSlicedNodes.size()
                            << " nodes\n";
    } // end differential MTA slice

    // Step 4: Build MTA SlicedSVFIRView (using pre-analysis pointer analysis)
    {
        ScopedPhaseTimer timer("Build MTA Sliced View");
        mtaSlicedView = std::make_unique<SlicedSVFIRView>(
                            svfir, *threadCallGraph, svfir->getICFG(),
                            mtaSlicedNodes);
    }
    if (isMTAStatEnabled())
        mtaSlicedView->dumpStats("MTA Sliced");

    const SlicedSVFIRView* slicedView = mtaSlicedView.get();

    if (dumpDot)
    {
        SVFUtil::outs() << "\n[Dump] MTA Sliced views:\n";
        slicedView->getICFG()->dump("sliced_icfg");
        if (slicedView->getThreadCallGraph() != nullptr)
            slicedView->getThreadCallGraph()->dump("sliced_tcg");
        slicedView->getPAG()->dump("sliced_pag");
    }

    // Step 5: Build Sliced TCT (using pre-analysis pointer analysis)
    {
        ScopedPhaseTimer timer("Sliced Thread Create Tree");
        if (isMTAStatEnabled())
            SVFUtil::outs() << "[SlicedTCT] Using max context length: "
                            << mainContextDepth << " (from -max-cxt)\n";
        // Reuse the shared pre-analysis (Andersen) for the sliced TCT.
        slicedTCT = SlicedTCT::create(
                        *preAndersen, *slicedView, mainContextDepth);
        if (dumpDot)
            slicedTCT->dump("sliced_tct");
    }

    // Step 6: Sliced MHP and Lock Analysis
    {
        ScopedPhaseTimer ilaTimer("Sliced Interleaving and Lock Analysis");
        {
            ScopedPhaseTimer timer("Sliced ILA: construct MHP/ForkJoin");
            slicedMHP = MHP::create(
                            slicedTCT.get(), slicedView->getICFG(),
                            slicedView->getThreadCallGraph(),
                            MHP::StateRepresentation::QuerySummaries);
        }
        {
            ScopedPhaseTimer timer("Sliced ILA: MHP propagation");
            slicedMHP->analyze(slicedView->getICFG(), slicedView->getThreadCallGraph());
        }
        {
            ScopedPhaseTimer timer("Sliced ILA: Lock analysis");
            slicedLockAnalysis = std::make_unique<LockAnalysis>(slicedTCT.get());
            slicedLockAnalysis->analyze(slicedView->getICFG(), slicedView->getThreadCallGraph());
        }
    }

    return true;
}

// PTA Slicing and Sliced Pointer Analysis
bool SlicedMTA::runPTASlicingAndAnalysis()
{
    SVFUtil::outs() << "\n=== PTA Slicing and Sliced Pointer Analysis ===\n";

    if (!hasThreadFunctions)
    {
        SVFUtil::outs() << "[SKIP] No thread functions found in pre-analysis, skipping PTA slicing\n";
        return true;
    }
    if (racePairs.empty())
    {
        SVFUtil::outs() << "[SKIP] No race pairs found in pre-analysis, skipping PTA slicing\n";
        return true;
    }

    const bool dumpDot = Options::DumpMTAGraphs();

    std::set<const SVFStmt*> vulnerableStatements = getVulnerableStmts();

    std::set<const ICFGNode*> ptaSlicedNodes;
    NodeBS finalSVFGNodeIds;

    if (slicedMHP == nullptr || slicedLockAnalysis == nullptr ||
            preSVFGBuilder == nullptr || preSVFG == nullptr)
    {
        SVFUtil::outs() << "[Main FSMPTA] Base SVFG or sliced ILA unavailable\n";
        return false;
    }

    if (Options::MTASingleStageSlicing())
    {
        // Single-pass baseline: reuse the unified V_Single computed in MTA slicing
        // (no separate data-dependence slice); FSPTA runs on the same slice as ILA.
        SVFUtil::outs() << "[Slicing Mode] Reusing unified slice (V_Single) for FSPTA\n";
        finalSVFGNodeIds =
            FSMPTA<const SlicedSVFGView*>::buildExecutionDependencyClosure(
                preSVFG, preAndersen,
                singleSlicedSVFGNodeIds);
        if (finalSVFGNodeIds.empty() && !singleSlicedSVFGNodeIds.empty())
        {
            SVFUtil::errs() << "[ERROR] Single-slice FSMPTA execution closure failed\n";
            return false;
        }
        ptaSlicedNodes = collectICFGNodes(preSVFG, finalSVFGNodeIds);
        if (isMTAStatEnabled())
            SVFUtil::outs() << "PTA reuses unified slice: "
                            << ptaSlicedNodes.size() << " ICFG nodes, "
                            << finalSVFGNodeIds.count()
                            << " execution-closure SVFG nodes\n";

        const SlicedSVFGView mainScope(preSVFG, finalSVFGNodeIds);

        const MTASVFGBuilder::ThreadVFBuildConfig mainConfig =
            MTASVFGBuilder::ThreadVFBuildConfig::mainPhase(
                mainScope);
        {
            ScopedPhaseTimer timer("Replace Pre-TVF with Main-TVF overlay");
            preSVFGBuilder->replaceThreadAwareOverlay(
                slicedMHP.get(), slicedLockAnalysis.get(), mainConfig);
        }
    }
    else
    {
        SVFUtil::outs() << "Using " << vulnerableStatements.size() << " vulnerable statements from pre-analysis\n";
        SVFUtil::outs() << "Using " << racePairs.size() << " race pairs from pre-analysis\n";

        // Differential stage 1 always constructs this slicer and its conservative
        // pre-candidate closure before stage 2 starts.
        if (multiStageSlicer == nullptr)
        {
            SVFUtil::errs() << "[ERROR] Differential PTA slicing requires the ILA slicer\n";
            return false;
        }

        const ValueFlowSlice& preCandidate =
            multiStageSlicer->getPreCandidateSlice();
        if (preCandidateSolveNodeIds.empty() && !preCandidate.svfgNodes.empty())
        {
            SVFUtil::errs() << "[ERROR] Execution-closed VFG'_pre is unavailable\n";
            return false;
        }
        const NodeBS& preCandidateIds = preCandidateSolveNodeIds;
        const SlicedSVFGView mainScope(preSVFG, preCandidateIds);

        // The base SVFG is stable. Replace only the pre-analysis TVF overlay
        // with edges derived from the context-sensitive sliced main ILA.
        const MTASVFGBuilder::ThreadVFBuildConfig mainConfig =
            MTASVFGBuilder::ThreadVFBuildConfig::mainPhase(
                mainScope, &selectedThreadVFCandidates);
        {
            ScopedPhaseTimer timer("Replace Pre-TVF with Main-TVF overlay");
            preSVFGBuilder->replaceThreadAwareOverlay(
                slicedMHP.get(), slicedLockAnalysis.get(), mainConfig);
        }
        if (isMTAStatEnabled())
            SVFUtil::outs() << "[Main-TVF] "
                            << preSVFGBuilder->getThreadAwareEdgeCount()
                            << " interference edges over VFG'_pre\n";

        ValueFlowSlice finalSlice;
        {
            ScopedPhaseTimer timer("PTA Slicing over refined main VFG");
            finalSlice = multiStageSlicer->runPTASlicing(vulnerableStatements, preSVFG);
        }
        ptaSlicedNodes = finalSlice.icfgNodes;
        finalSVFGNodeIds = finalSlice.nodeIds();

        NodeBS outsideCandidate = finalSVFGNodeIds;
        outsideCandidate.intersectWithComplement(preCandidateIds);
        if (!outsideCandidate.empty())
        {
            SVFUtil::errs() << "[ERROR] Initial FSPTA slice escapes VFG'_pre by "
                            << outsideCandidate.count() << " SVFG nodes\n";
            return false;
        }

        {
            ScopedPhaseTimer timer("Build FSMPTA execution dependency closure");
            finalSVFGNodeIds =
                FSMPTA<const SlicedSVFGView*>::buildExecutionDependencyClosure(
                    preSVFG, preAndersen, finalSVFGNodeIds);
        }

        outsideCandidate = finalSVFGNodeIds;
        outsideCandidate.intersectWithComplement(preCandidateIds);
        if (!outsideCandidate.empty())
        {
            SVFUtil::errs() << "[ERROR] FSMPTA execution closure escapes VFG'_pre by "
                            << outsideCandidate.count() << " SVFG nodes\n";
            return false;
        }

        // Report the exact execution-closed solve set, rather than the smaller
        // pre-closure dependency seed shown by the old implementation.
        ptaSlicedNodes = collectICFGNodes(preSVFG, finalSVFGNodeIds);

        if (isMTAStatEnabled())
        {
            SVFUtil::outs() << "[FSPTA Slice] " << finalSlice.svfgNodes.size()
                            << " / " << preCandidate.svfgNodes.size() << " / "
                            << preCandidateIds.count()
                            << " SVFG nodes (final / pre-candidate / execution-closed), "
                            << finalSVFGNodeIds.count()
                            << " execution-closure solve nodes\n";
            SVFUtil::outs() << "PTA sliced to " << ptaSlicedNodes.size()
                            << " nodes\n";
        }
    }

    if (isMTAStatEnabled())
        reportPTASliceStatistics(ptaSlicedNodes);

    // Slicers are no longer needed. The builder and its one stable base SVFG
    // remain alive because FSMPTA solves that graph directly.
    multiStageSlicer.reset();
    singleSlicer.reset();

    // Step 5: solve the exact final slice on the stable base SVFG plus Main-TVF.
    SVFUtil::outs() << "[Main FSMPTA] Reusing BaseSVFG; Main-TVF comes from the sliced main ILA\n";
    if (!FSMPTA<const SlicedSVFGView*>::supportsCurrentConfiguration())
    {
        SVFUtil::errs() << "[ERROR] Unsupported FSMPTA mapping/clustering configuration\n";
        return false;
    }
    {
        ScopedPhaseTimer timer("Flow-Sensitive FSAM Analysis");
        slicedSVFGView = std::make_unique<SlicedSVFGView>(
                             preSVFG, finalSVFGNodeIds);
        auto solver = std::make_unique<FSMPTA<const SlicedSVFGView*>>(
                          *preAndersen, *preSVFG, slicedSVFGView.get());
        solver->analyze();
        if (dumpDot)
        {
            solver->getSVFG()->dump("mta_svfg");
            slicedSVFGView->dump("sliced_svfg");
        }
        mainFSMPTA = std::move(solver);
    }

    return true;
}

// Final Race Detection using sliced analysis results
bool SlicedMTA::runFinalRaceDetection()
{
    SVFUtil::outs() << "\n=== Final Race Detection ===\n";

    if (!hasThreadFunctions)
    {
        SVFUtil::outs() << "[SKIP] No thread functions found\n";
        return true;
    }
    if (racePairs.empty())
    {
        SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
        SVFUtil::outs() << "Race pairs (pre-analysis): 0\n";
        SVFUtil::outs() << "Race pairs (sliced graph): 0\n";
        SVFUtil::outs() << "Race statements (sliced graph): 0\n";
        if (isMTAStatEnabled())
        {
            const RaceDigests digests = computeRaceDigests(racePairs);
            SVFUtil::outs() << "[MSLI-RQ] mode=MSli alarms=0 pairs=0"
                            << " alarm-digest=" << digests.alarm
                            << " pair-digest=" << digests.pair << "\n";
        }
        SVFUtil::outs() << "\nNo race pairs detected in sliced graph.\n";
        return true;
    }
    if (mtaSlicedView == nullptr)
    {
        SVFUtil::errs() << "[ERROR] MTA sliced view not available\n";
        return false;
    }
    if (slicedMHP == nullptr || slicedLockAnalysis == nullptr)
    {
        SVFUtil::errs() << "[ERROR] Sliced MHP or LockAnalysis not available\n";
        return false;
    }
    if (getMainPTA() == nullptr)
    {
        SVFUtil::errs() << "[ERROR] Main flow-sensitive pointer analysis not available\n";
        return false;
    }

    std::set<RacePair> detectedPairs;
    {
        ScopedPhaseTimer timer("Final Race Detection");
        detectedPairs = detectRacePairsOnSlicedGraph(
                            racePairs,        // Refine only pre-analysis candidates
                            getMainPTA(),     // Use flow-sensitive FSAM points-to
                            slicedMHP.get(), slicedLockAnalysis.get());
    }

    // Distinct racy statements (the endpoints of the race pairs) -- a stabler,
    // smaller-to-report metric than the pair count.
    std::set<const SVFStmt*> racyStmts;
    for (const RacePair& pair : detectedPairs)
    {
        racyStmts.insert(pair.stmt1);
        racyStmts.insert(pair.stmt2);
    }

    SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
    SVFUtil::outs() << "Race pairs (pre-analysis): " << racePairs.size() << "\n";
    SVFUtil::outs() << "Race pairs (sliced graph): " << detectedPairs.size() << "\n";
    SVFUtil::outs() << "Race statements (sliced graph): " << racyStmts.size() << "\n";
    // Machine-readable line for the artifact's `msli` table generator: the race
    // statements reported after slicing (the preservation metric).
    if (isMTAStatEnabled())
    {
        const RaceDigests digests = computeRaceDigests(detectedPairs);
        SVFUtil::outs() << "[MSLI-RQ] mode=MSli alarms=" << racyStmts.size()
                        << " pairs=" << detectedPairs.size()
                        << " alarm-digest=" << digests.alarm
                        << " pair-digest=" << digests.pair << "\n";
    }

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

// No-slice A/B baseline: run the same analysis as the sliced path over the whole
// program. SlicedTCT is retained here to preserve its main-context construction;
// MHP and LockAnalysis consume the original full graphs directly.
bool SlicedMTA::runWholeProgramDetection()
{
    SVFUtil::outs() << "\n=== Whole-program FSAM Race Detection (no slicing) ===\n";
    if (!hasThreadFunctions || racePairs.empty())
    {
        SVFUtil::outs() << "[SKIP] No thread functions / race pairs in pre-analysis\n";
        return true;
    }

    // SlicedTCT currently consumes the sliced-view representation even for the
    // full baseline. Time that construction explicitly so the A/B phase table
    // accounts for it instead of leaving it in unattributed wall time.
    {
        ScopedPhaseTimer timer("Build Whole-program View");
        std::set<const ICFGNode*> allNodes;
        for (ICFG::iterator it = svfir->getICFG()->begin(),
                eit = svfir->getICFG()->end(); it != eit; ++it)
            allNodes.insert(it->second);
        ptaSlicedView = std::make_unique<SlicedSVFIRView>(
                            svfir, *threadCallGraph, svfir->getICFG(), allNodes);
    }

    {
        ScopedPhaseTimer timer("Whole-program TCT/MHP/Lock");
        {
            ScopedPhaseTimer phaseTimer("Whole-program Thread Create Tree");
            slicedTCT = SlicedTCT::create(
                            *preAndersen, *ptaSlicedView, mainContextDepth);
        }
        CallGraph* fullCallGraph = threadCallGraph;
        {
            ScopedPhaseTimer phaseTimer(
                "Whole-program ILA: construct MHP/ForkJoin");
            slicedMHP = MHP::create(
                            slicedTCT.get(), svfir->getICFG(), fullCallGraph,
                            MHP::StateRepresentation::QuerySummaries);
        }
        {
            ScopedPhaseTimer phaseTimer("Whole-program ILA: MHP propagation");
            slicedMHP->analyze(svfir->getICFG(), fullCallGraph);
        }
        {
            ScopedPhaseTimer phaseTimer("Whole-program ILA: Lock analysis");
            slicedLockAnalysis =
                std::make_unique<LockAnalysis>(slicedTCT.get());
            slicedLockAnalysis->analyze(svfir->getICFG(), fullCallGraph);
        }
    }

    const MTASVFGBuilder::ThreadVFBuildConfig mainConfig =
        MTASVFGBuilder::ThreadVFBuildConfig::wholeProgram();
    {
        ScopedPhaseTimer timer("Whole-program Replace Pre-TVF with Main-TVF overlay");
        preSVFGBuilder->replaceThreadAwareOverlay(
            slicedMHP.get(), slicedLockAnalysis.get(), mainConfig);
    }
    if (isMTAStatEnabled())
        SVFUtil::outs() << "[Main-TVF] "
                        << preSVFGBuilder->getThreadAwareEdgeCount()
                        << " interference edges over the whole BaseSVFG\n";

    {
        ScopedPhaseTimer timer("Whole-program Flow-Sensitive FSMPTA Solve");
        auto solver = std::make_unique<FSMPTA<SVFG*>>(
                          *preAndersen, *preSVFG, preSVFG);
        solver->analyze();
        mainFSMPTA = std::move(solver);
    }

    std::set<RacePair> detectedPairs;
    {
        ScopedPhaseTimer timer("Final Race Detection (whole program)");
        detectedPairs = detectRacePairsOnSlicedGraph(
                            racePairs, getMainPTA(), slicedMHP.get(),
                            slicedLockAnalysis.get());
    }

    std::set<const SVFStmt*> racyStmts;
    for (const RacePair& pair : detectedPairs)
    {
        racyStmts.insert(pair.stmt1);
        racyStmts.insert(pair.stmt2);
    }

    SVFUtil::outs() << "\n=== Race Detection Summary ===\n";
    SVFUtil::outs() << "Race pairs (pre-analysis): " << racePairs.size() << "\n";
    SVFUtil::outs() << "Race pairs (whole program): " << detectedPairs.size() << "\n";
    SVFUtil::outs() << "Race statements (whole program): " << racyStmts.size() << "\n";
    if (isMTAStatEnabled())
    {
        const RaceDigests digests = computeRaceDigests(detectedPairs);
        SVFUtil::outs() << "[MSLI-RQ] mode=FSAM alarms=" << racyStmts.size()
                        << " pairs=" << detectedPairs.size()
                        << " alarm-digest=" << digests.alarm
                        << " pair-digest=" << digests.pair << "\n";
    }
    return true;
}

bool SlicedMTA::runOnModule(SVFIR* pag, AndersenWaveDiff& preAnalysis)
{
    if (svfir != nullptr || pag == nullptr || preAnalysis.getPAG() != pag)
    {
        SVFUtil::errs() << "[ERROR] SlicedMTA is single-use and requires a "
                        << "matching SVFIR and Andersen pre-analysis\n";
        return false;
    }
    svfir = pag;
    preAndersen = &preAnalysis;

    SVFUtil::outs() << "[Config] Slicing: "
                    << (Options::MTAEnableSlicing() ? "enabled" : "disabled")
                    << "\n";

    if (isMTAStatEnabled())
        reportOriginalStatistics(svfir);

    // The pre-analysis is context-insensitive in BOTH modes (the sliced run and
    // the FSAM baseline must share an identical pre-analysis substrate); the
    // main phase then runs at the configured context depth.
    // TCT context bounds are explicit constructor inputs; no process-global
    // option is mutated while the pipeline is running.
    const bool preOk = runPreAnalysis();
    if (!preOk)
        return false;

    if (Options::MTAEnableSlicing())
    {
        if (!runMTASlicingAndAnalysis()) return false;
        if (!runPTASlicingAndAnalysis()) return false;
        if (!runFinalRaceDetection()) return false;
    }
    else
    {
        if (!runWholeProgramDetection()) return false;
    }

    SVFUtil::outs() << "\n=== Analysis Complete ===\n";
    return true;
}

//===----------------------------------------------------------------------===//
// Race detection for the SlicedMTA pipeline (final detection + whole-program
// baseline), driven by the sliced/FSAM analyses. hasThreadFunctions is a generic
// helper on the base MTA detector; detectRacePairsOnSlicedGraph is the pipeline's
// sliced-graph screen and stays on SlicedMTA.
//===----------------------------------------------------------------------===//

// Whether any thread (fork-target) function is reachable via a fork edge.
bool MTA::hasThreadFunctions(CallGraph* callGraph)
{
    for (CallGraph::iterator it = callGraph->begin(), eit = callGraph->end();
            it != eit; ++it)
    {
        const CallGraphNode* node = it->second;
        for (const CallGraphEdge* edge : node->getOutEdges())
        {
            if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge &&
                    edge->getDstNode()->getFunction() != nullptr)
            {
                return true;
            }
        }
    }
    return false;
}

// Detect race pairs on the sliced graph using sliced analysis results.
std::set<SlicedMTA::RacePair> SlicedMTA::detectRacePairsOnSlicedGraph(
    const std::set<RacePair>& preAnalysisRacePairs,
    BVDataPTAImpl* slicedPTA,
    MHP* slicedMHP,
    LockAnalysis* slicedLockAnalysis)
{

    std::set<RacePair> filteredRacePairs;

    // MSli's main phase refines the alarms produced by the conservative
    // pre-analysis. Main ILA and FSMPTA are recomputed independently on their
    // slices; only the candidate universe comes from pre-analysis.
    for (const RacePair& pair : preAnalysisRacePairs)
    {
        const ICFGNode* node1 = pair.stmt1->getICFGNode();
        const ICFGNode* node2 = pair.stmt2->getICFGNode();

        if (!slicedMHP->mayHappenInParallelCache(node1, node2))
            continue;

        if (slicedLockAnalysis->isProtectedByCommonLock(node1, node2))
            continue;

        PointsTo pts1, pts2;
        if (const LoadStmt* ldStmt1 =
                    SVFUtil::dyn_cast<LoadStmt>(pair.stmt1))
        {
            pts1 = slicedPTA->getPts(ldStmt1->getRHSVarID());
        }
        else if (const StoreStmt* stStmt1 =
                     SVFUtil::dyn_cast<StoreStmt>(pair.stmt1))
        {
            pts1 = slicedPTA->getPts(stStmt1->getLHSVarID());
        }
        else
        {
            continue;
        }

        if (const LoadStmt* ldStmt2 =
                    SVFUtil::dyn_cast<LoadStmt>(pair.stmt2))
        {
            pts2 = slicedPTA->getPts(ldStmt2->getRHSVarID());
        }
        else if (const StoreStmt* stStmt2 =
                     SVFUtil::dyn_cast<StoreStmt>(pair.stmt2))
        {
            pts2 = slicedPTA->getPts(stStmt2->getLHSVarID());
        }
        else
        {
            continue;
        }

        // Check if points-to sets still intersect
        if (pts1.intersects(pts2))
            filteredRacePairs.insert(pair);
    }

    return filteredRacePairs;
}
