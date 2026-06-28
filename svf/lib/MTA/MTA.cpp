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

    tcg->dump("tcg");

    mhp = computeMHP(tct.get());
    lsa = computeLocksets(tct.get());

    if(Options::RaceCheck())
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
        NodeID o = worklist.front();
        worklist.pop_front();
        auto add = [&](NodeID x) {
            if (!ptsClosure.test(x)) { ptsClosure.set(x); worklist.push_back(x); }
        };

        for (NodeID t : pta->getPts(o))                  // points-to
            add(t);

        if (pag->getBaseObject(o) != nullptr)            // containment (object nodes only)
            for (NodeID f : pta->getAllFieldsObjVars(pta->getBaseObjVarID(o)))
                add(f);
    }

    return ptsClosure;
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

    // Lock signature: a string capturing every input isProtectedByCommonLock
    // reads, so equal signatures => identical lock relation against any partner
    // ("U" = holds no lock). Lets C4 be decided once per class pair.
    auto cxtSig = [](const CallStrCxt& c) -> std::string {
        std::string s; for (u32_t x : c) { s += std::to_string(x); s += '.'; } return s;
    };
    auto lockSig = [&](const ICFGNode* n) -> std::string {
        if (!lockAnalysis->isProtectedByCommonLock(n, n)) return "U";   // holds no lock
        std::string s = "L";
        s += lockAnalysis->isInsideIntraLock(n) ? 'i' : '.';
        s += lockAnalysis->isInsideCondIntraLock(n) ? 'c' : '.';
        s += "[I";
        if (lockAnalysis->hasIntraLockSet(n)) {   // else: cond-only / cxt-only locked
            std::vector<NodeID> ls;
            for (const ICFGNode* l : lockAnalysis->getIntraLockSet(n)) ls.push_back(l->getId());
            std::sort(ls.begin(), ls.end());
            for (NodeID id : ls) { s += ':'; s += std::to_string(id); }
        }
        s += ']';
        if (lockAnalysis->hasCxtStmtFromInst(n)) {
            std::vector<std::string> parts;  // one per context of n: (context, held locks)
            for (const CxtStmt& cts : lockAnalysis->getCxtStmtsFromInst(n)) {
                std::string part = cxtSig(cts.getContext()) + "{";
                if (lockAnalysis->hasCxtLockfromCxtStmt(cts)) {
                    std::vector<std::string> locks;
                    for (const CxtStmt& cl : lockAnalysis->getCxtLockfromCxtStmt(cts))
                        locks.push_back(std::to_string(cl.getStmt()->getId()) + "@" + cxtSig(cl.getContext()));
                    std::sort(locks.begin(), locks.end());
                    for (const std::string& l : locks) { part += l; part += ';'; }
                }
                parts.push_back(part + "}");
            }
            std::sort(parts.begin(), parts.end());
            s += "[X"; for (const std::string& p : parts) s += p; s += ']';
        }
        return s;
    };

    // One occurrence per (statement, thread instance), indexed by object (C1 ->
    // "same bucket") as collected, so the points-to set is consumed straight into
    // the buckets and never stored per occurrence.
    struct Occ { const SVFStmt* stmt; const ICFGNode* node; bool isStore;
                 NodeID tid; NodeBS interleav; std::string lsig; };
    std::vector<Occ> occs;
    Map<NodeID, std::vector<size_t>> objToOcc;
    for (const auto& item : *callGraph) {
        const FunObjVar* F = item.second->getFunction();
        if (!F || !F->hasBasicBlock()) continue;
        for (auto bbit : *F)
            for (const ICFGNode* node : bbit.second->getICFGNodeList()) {
                if (!mhp->hasThreadStmtSet(node)) continue;          // screen 1: concurrent?
                for (const SVFStmt* stmt : svfIr->getSVFStmtList(node)) {
                    NodeID var; bool isStore;
                    if (auto* l = SVFUtil::dyn_cast<LoadStmt>(stmt)) { var=l->getRHSVarID(); isStore=false; }
                    else if (auto* s = SVFUtil::dyn_cast<StoreStmt>(stmt)) { var=s->getLHSVarID(); isStore=true; }
                    else continue;
                    PointsTo p = pta->getPts(var);
                    p &= escSet;                                     // screen 2: touches shared object?
                    if (p.empty()) continue;
                    std::string lsig = lockSig(node);
                    const size_t base = occs.size();
                    for (const CxtThreadStmt& cts : mhp->getThreadStmtSet(node))
                        occs.push_back({stmt, node, isStore, cts.getTid(),
                                        mhp->getInterleavingThreads(cts), lsig});
                    for (NodeID o : p)
                        for (size_t k = base; k < occs.size(); ++k)
                            objToOcc[o].push_back(k);
                }
            }
    }

    // C3: distinct threads must mutually interleave; the same thread self-races
    // only when it is multiforked (>1 dynamic instance).
    auto raceMHP = [&](const Occ& a, const Occ& b) -> bool {
        if (a.tid != b.tid) return a.interleav.test(b.tid) && b.interleav.test(a.tid);
        return mhp->getTCT()->getTCTNode(a.tid)->isMultiforked();
    };
    auto commit = [&](const Occ& a, const Occ& b) {
        const SVFStmt* s1 = a.stmt; const SVFStmt* s2 = b.stmt;
        if (s2 < s1) std::swap(s1, s2);
        outRacePairs.insert(RacePair(s1, s2));
    };
    // Within each object, occurrences sharing the race predicate's inputs (tid,
    // interleaving, isStore, lock sig) race the same partners, so collapse into a
    // class and judge C2/C3/C4 once per class pair -- O(classes^2) not O(occ^2).
    for (const auto& kv : objToOcc) {
        struct Cls { bool isStore, locked; size_t rep; std::vector<size_t> members; };
        std::vector<Cls> classes;
        Map<std::string, size_t> keyToClass;
        for (size_t idx : kv.second) {
            const Occ& o = occs[idx];
            std::string key;
            key += std::to_string(o.tid);
            key += o.isStore ? 'S' : 'L';
            key += '|'; key += o.lsig;            // lock signature (sound merge key)
            key += '|';
            for (NodeID t : o.interleav) { key += std::to_string(t); key += ','; }
            auto it = keyToClass.find(key);
            if (it == keyToClass.end()) {
                keyToClass[key] = classes.size();
                classes.push_back({o.isStore, o.lsig != "U", idx, {idx}});
            } else
                classes[it->second].members.push_back(idx);
        }
        for (size_t a = 0; a < classes.size(); ++a)
            for (size_t b = a; b < classes.size(); ++b) {
                Cls& ca = classes[a]; Cls& cb = classes[b];
                if (!ca.isStore && !cb.isStore) continue;             // C2
                const Occ& ra = occs[ca.rep]; const Occ& rb = occs[cb.rep];
                if (!raceMHP(ra, rb)) continue;                       // C3
                // C4 + emit. Lock relation is uniform per class (the reps decide
                // it), so emit a statement-COVERING set -- every racy member as an
                // endpoint -- preserving the racy-statement set with O(|members|)
                // pairs. A class with itself also has self-pairs, checked per member.
                if (a != b) {
                    if (ca.locked && cb.locked &&
                        lockAnalysis->isProtectedByCommonLock(ra.node, rb.node))
                        continue;
                    for (size_t ia : ca.members) commit(occs[ia], occs[cb.members[0]]);
                    for (size_t ib : cb.members) commit(occs[ca.members[0]], occs[ib]);
                } else {
                    const std::vector<size_t>& M = ca.members;
                    // Do two DISTINCT members race? (uniform lock relation by lock sig)
                    bool crossRaces = M.size() >= 2 &&
                        (!ca.locked ||
                         !lockAnalysis->isProtectedByCommonLock(occs[M[0]].node, occs[M[1]].node));
                    if (crossRaces) {
                        for (size_t i = 0; i < M.size(); ++i)
                            commit(occs[M[i]], occs[M[i == 0 ? 1 : 0]]);
                    } else {
                        // No cross race; a member still self-races (two instances of
                        // one multiforked statement) when it is not self-locked.
                        for (size_t idx : M)
                            if (!ca.locked ||
                                !lockAnalysis->isProtectedByCommonLock(occs[idx].node, occs[idx].node))
                                commit(occs[idx], occs[idx]);
                    }
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

