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
#include "WPA/Andersen.h"
#include "Util/SVFUtil.h"
#include <deque>
#include <set>
#include <string>
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

// Serialise a calling context into a stable string key.
std::string MTA::contextSignature(const CallStrCxt& context) {
    std::string sig;
    for (u32_t callSite : context) { sig += std::to_string(callSite); sig += '.'; }
    return sig;
}

// Lock signature of a node: equal signatures => identical common-lock relation
// against any partner ("U" = holds no lock), so C4 is decided once per class pair.
std::string MTA::lockSignature(LockAnalysis* lockAnalysis, const ICFGNode* node) {
    if (!lockAnalysis->isProtectedByCommonLock(node, node)) return "U";
    std::string sig = "L";
    sig += lockAnalysis->isInsideIntraLock(node) ? 'i' : '.';
    sig += lockAnalysis->isInsideCondIntraLock(node) ? 'c' : '.';
    sig += "[I";
    if (lockAnalysis->hasIntraLockSet(node)) {   // else: cond-only / cxt-only locked
        std::vector<NodeID> intraLocks;
        for (const ICFGNode* lock : lockAnalysis->getIntraLockSet(node))
            intraLocks.push_back(lock->getId());
        std::sort(intraLocks.begin(), intraLocks.end());
        for (NodeID id : intraLocks) { sig += ':'; sig += std::to_string(id); }
    }
    sig += ']';
    if (lockAnalysis->hasCxtStmtFromInst(node)) {
        std::vector<std::string> perContext;   // one entry per context of node
        for (const CxtStmt& cxtStmt : lockAnalysis->getCxtStmtsFromInst(node)) {
            std::string entry = contextSignature(cxtStmt.getContext()) + "{";
            if (lockAnalysis->hasCxtLockfromCxtStmt(cxtStmt)) {
                std::vector<std::string> heldLocks;
                for (const CxtStmt& heldLock : lockAnalysis->getCxtLockfromCxtStmt(cxtStmt))
                    heldLocks.push_back(std::to_string(heldLock.getStmt()->getId()) + "@"
                                        + contextSignature(heldLock.getContext()));
                std::sort(heldLocks.begin(), heldLocks.end());
                for (const std::string& lock : heldLocks) { entry += lock; entry += ';'; }
            }
            perContext.push_back(entry + "}");
        }
        std::sort(perContext.begin(), perContext.end());
        sig += "[X"; for (const std::string& entry : perContext) sig += entry; sig += ']';
    }
    return sig;
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
                    std::string sig = lockSignature(lockAnalysis, node);
                    const size_t firstNew = occurrences.size();
                    for (const CxtThreadStmt& threadStmt : mhp->getThreadStmtSet(node))
                        occurrences.push_back({stmt, node, isStore, threadStmt.getTid(),
                                               mhp->getInterleavingThreads(threadStmt), sig});
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
        std::vector<RaceClass> classes;
        Map<std::string, size_t> keyToClass;
        for (size_t occIdx : objectAndOccs.second) {
            const RaceOccurrence& occ = occurrences[occIdx];
            std::string key;
            key += std::to_string(occ.tid);
            key += occ.isStore ? 'S' : 'L';
            key += '|'; key += occ.lockSig;       // lock signature (sound merge key)
            key += '|';
            for (NodeID interleaved : occ.interleav) { key += std::to_string(interleaved); key += ','; }
            auto found = keyToClass.find(key);
            if (found == keyToClass.end()) {
                keyToClass[key] = classes.size();
                classes.push_back({occ.isStore, occ.lockSig != "U", occIdx, {occIdx}});
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

