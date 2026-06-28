//===- Slicer.cpp -- Multi-stage on-demand program slicers ----------------===//
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
 * Slicer.cpp
 *
 *      Author: Jiawei Yang
 *
 * SlicerBase + MTASlicer + PTASlicer + SingleSlicer.
 */

#include "MTA/Slicer.h"
#include "MTA/SlicedView.h"
#include <MTA/TCT.h>
#include <SVFIR/SVFIR.h>
#include <Util/SVFUtil.h>
#include <Util/CxtStmt.h>
#include <Util/ThreadAPI.h>
#include <deque>
#include <algorithm>
#include <cassert>
#include <Graphs/ICFGEdge.h>
#include <Graphs/ICFGNode.h>
#include <Graphs/CallGraph.h>
#include <SVFIR/SVFStatements.h>
#include <SVFIR/SVFVariables.h>
#include <fstream>
#include <sstream>
#include <queue>
#include <unordered_map>
#include <iostream>
#include <cctype>
#include <Graphs/SVFG.h>
#include <Graphs/VFGNode.h>
#include <Graphs/VFGEdge.h>

using namespace SVF;

namespace SVF
{

//===----------------------------------------------------------------------===//
// SlicerBase
//===----------------------------------------------------------------------===//

SlicerBase::SlicerBase(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                       LockAnalysis* lockAnalysis)
    : svfIr(svfIr), pta(pta), mhp(mhp), lockAnalysis(lockAnalysis) {
    callGraph = pta->getCallGraph();
}

SlicerBase::~SlicerBase() {
    // No dynamic memory to clean up - all pointers are managed externally
}

// Helper: Get lock set for an ICFG node
std::set<const ICFGNode*> SlicerBase::getLockSet(const ICFGNode* node) {
    std::set<const ICFGNode*> allLockSites;

    // Get intra-procedural locks
    if (lockAnalysis->isInsideIntraLock(node) &&
        !lockAnalysis->isInsideCondIntraLock(node)) {
        const LockAnalysis::InstSet& intraLocks = lockAnalysis->getIntraLockSet(node);
        for (const ICFGNode* lockSite : intraLocks) {
            allLockSites.insert(lockSite);
        }
    }

    // Get context-sensitive locks
    if (lockAnalysis->hasCxtStmtFromInst(node)) {
        const LockAnalysis::CxtStmtSet& cxtStmts = lockAnalysis->getCxtStmtsFromInst(node);
        for (const CxtStmt& cxtStmt : cxtStmts) {
            if (lockAnalysis->hasCxtLockfromCxtStmt(cxtStmt)) {
                const LockAnalysis::CxtLockSet& cxtLocks =
                    lockAnalysis->getCxtLockfromCxtStmt(cxtStmt);
                for (const LockAnalysis::CxtLock& cxtLock : cxtLocks) {
                    allLockSites.insert(cxtLock.getStmt());
                }
            }
        }
    }

    return allLockSites;
}

// Helper: Get TCTNode set from ICFGNode
std::set<const TCTNode*> SlicerBase::getTCTNodeSetFromNode(const ICFGNode* node) {
    std::set<const TCTNode*> tctNodeSet;

    if (mhp->hasThreadStmtSet(node)) {
        for (const CxtThreadStmt& cts : mhp->getThreadStmtSet(node)) {
            if (mhp->getTCT()->hasGNode(cts.getTid())) {
                tctNodeSet.insert(mhp->getTCT()->getTCTNode(cts.getTid()));
            }
        }
    }

    return tctNodeSet;
}

// Helper: Get dependent thread create statements
std::set<const SVFStmt*> SlicerBase::getDependentThreadCreate(const SVFStmt* stmt) {
    std::set<const SVFStmt*> forkSiteStmts;

    const ICFGNode* icfgNode = stmt->getICFGNode();
    std::set<const TCTNode*> tctNodeSet = getTCTNodeSetFromNode(icfgNode);

    for (const TCTNode* tctNode : tctNodeSet) {
        const CxtThread& cxtThread = tctNode->getCxtThread();
        const ICFGNode* forkSite = cxtThread.getThread();
        if (forkSite != nullptr) {
            const ICFGNode::SVFStmtList& svfStmts = forkSite->getSVFStmts();
            if (!svfStmts.empty()) {
                forkSiteStmts.insert(svfStmts.front());
            }
        }
    }

    return forkSiteStmts;
}

// Data-dependence slice over the thread-aware SVFG (VFG_pre), at SVFG-node
// granularity: the value-flow nodes reachable backward from the seeds. The
// value-flow edges already capture direct (top-level), indirect (address-taken
// / MemSSA), and thread-aware (interference) data dependence.
std::set<const SVFGNode*> SlicerBase::computeDataDependenceSVFGNodes(
    const std::set<const SVFStmt*>& seeds, SVF::SVFG* vfg) {

    assert(vfg != nullptr && "data-dependence slice requires the thread-aware VFG_pre");

    std::set<const SVFGNode*> visited;
    std::deque<const SVFGNode*> worklist;
    auto seed = [&](const SVFGNode* n) {
        if (n != nullptr && visited.insert(n).second)
            worklist.push_back(n);
    };

    // Seed from the value-flow nodes of the given (e.g. race target) statements.
    // VFG_pre is a pointer-only SVFG, so a load/store of a NON-pointer value (the
    // usual case -- a race on an int/float field) has no statement node. For those
    // we must still preserve the points-to of the dereferenced address pointer, or
    // the sliced flow-sensitive solve sees an empty slice for it, computes empty
    // points-to, and drops the race (a soundness bug). So additionally seed from the
    // definition of each load/store's address pointer (always a pointer, hence
    // always in the pointer-only SVFG); its backward closure keeps the pointer's
    // def chain regardless of the value type.
    for (const SVFStmt* stmt : seeds)
    {
        if (vfg->hasStmtVFGNode(stmt))
            seed(vfg->getStmtVFGNode(stmt));

        NodeID addrPtr = 0;
        if (const LoadStmt* ld = SVFUtil::dyn_cast<LoadStmt>(stmt))
            addrPtr = ld->getRHSVarID();
        else if (const StoreStmt* st = SVFUtil::dyn_cast<StoreStmt>(stmt))
            addrPtr = st->getLHSVarID();
        if (addrPtr != 0)
        {
            // getDefSVFGNode takes a ValVar (the address pointer is a top-level
            // value variable).
            const ValVar* ptrNode = SVFUtil::dyn_cast<ValVar>(svfIr->getGNode(addrPtr));
            if (ptrNode != nullptr && vfg->hasDefSVFGNode(ptrNode))
                seed(vfg->getDefSVFGNode(ptrNode));
        }
    }

    // Backward over every value-flow edge.
    while (!worklist.empty()) {
        const SVFGNode* n = worklist.front();
        worklist.pop_front();
        for (const VFGEdge* e : n->getInEdges())
            seed(e->getSrcNode());
    }

    return visited;
}

// Project retained VFG nodes (plus the seeds) onto their ICFG nodes.
std::set<const ICFGNode*> SlicerBase::svfgNodesToICFGNodes(
    const std::set<const SVFGNode*>& nodes, const std::set<const SVFStmt*>& seeds) {
    std::set<const ICFGNode*> result;
    for (const SVFGNode* n : nodes)
        if (const StmtVFGNode* s = SVFUtil::dyn_cast<StmtVFGNode>(n))
            if (s->getICFGNode() != nullptr)
                result.insert(s->getICFGNode());
    for (const SVFStmt* stmt : seeds)
        result.insert(stmt->getICFGNode());
    return result;
}

std::set<const ICFGNode*> SlicerBase::sliceDataDependenceOverVFG(
    const std::set<const SVFStmt*>& seeds, SVF::SVFG* vfg) {
    return svfgNodesToICFGNodes(computeDataDependenceSVFGNodes(seeds, vfg), seeds);
}

// Helper: Collect pthread-related statements (create and join)
std::set<const CallICFGNode*> SlicerBase::collectPthreadStatements(
    const std::set<const SVFStmt*>& vulnerableStmts) {
    std::set<const CallICFGNode*> pthreadCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map pthread_create nodes to their corresponding pthread_join nodes
    std::set<const SVFStmt*> pthreadCreateStmts;

    // First pass: collect all pthread_create nodes
    for (const SVFStmt* stmt : vulnerableStmts) {
        std::set<const SVFStmt*> forkSiteStmts = getDependentThreadCreate(stmt);

        for (const SVFStmt* forkSiteStmt : forkSiteStmts) {
            const ICFGNode* forkSiteNode = forkSiteStmt->getICFGNode();
            const CallICFGNode* forkCallNode = SVFUtil::dyn_cast<CallICFGNode>(forkSiteNode);
            if (forkCallNode != nullptr && threadAPI->isTDFork(forkCallNode)) {
                pthreadCallNodes.insert(forkCallNode);
                pthreadCreateStmts.insert(forkSiteStmt);
            }
        }
    }

    // Second pass: find corresponding pthread_join nodes
    ICFG* icfg = svfIr->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDJoin(callNode)) {
            const SVFVar* joinThread = threadAPI->getJoinedThread(callNode);
            if (joinThread != nullptr) {
                for (auto& createStmt : pthreadCreateStmts) {
                    const ICFGNode* createNode = createStmt->getICFGNode();
                    const CallICFGNode* createCallNode = SVFUtil::dyn_cast<CallICFGNode>(createNode);
                    if (createCallNode != nullptr) {
                        const SVFVar* forkedThread = threadAPI->getForkedThread(createCallNode);
                        if (forkedThread != nullptr && threadAPI->isAliasedForkJoin(pta, forkedThread, joinThread)) {
                                pthreadCallNodes.insert(callNode);
                        }
                    }
                }
            }
        }
    }

    return pthreadCallNodes;
}

// Helper: Collect mutex-related statements (lock and unlock)
std::set<const CallICFGNode*> SlicerBase::collectMutexStatements(
    const std::set<const SVFStmt*>& vulnerableStmts) {
    std::set<const CallICFGNode*> mutexCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map mutex_lock nodes to their corresponding mutex_unlock nodes
    std::set<const CallICFGNode*> mutexLockCallNodes;

    // First pass: collect all mutex_lock nodes from lock sets
    for (const SVFStmt* stmt : vulnerableStmts) {
        std::set<const ICFGNode*> lockSet = getLockSet(stmt->getICFGNode());
        for (const ICFGNode* lockNode : lockSet) {
            const CallICFGNode* lockCallNode = SVFUtil::dyn_cast<CallICFGNode>(lockNode);
            if (lockCallNode != nullptr && threadAPI->isTDAcquire(lockCallNode)) {
                mutexCallNodes.insert(lockCallNode);
                mutexLockCallNodes.insert(lockCallNode);
            }
        }
    }

    // Second pass: find corresponding mutex_unlock nodes
    ICFG* icfg = svfIr->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it) {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDRelease(callNode)) {
            const SVFVar* unlockVar = threadAPI->getLockVal(callNode);
            if (unlockVar != nullptr) {
                for (auto lockCallNode : mutexLockCallNodes) {
                    if (lockCallNode != nullptr) {
                        const SVFVar* lockVar = threadAPI->getLockVal(lockCallNode);
                        if (lockVar != nullptr && pta->alias(unlockVar->getId(), lockVar->getId())) {
                            mutexCallNodes.insert(callNode);
                        }
                    }
                }
            }
        }
    }

    return mutexCallNodes;
}

// Helper: Collect common pthread and mutex statements (shared by PTA and MTA slicing)
std::pair<std::set<const CallICFGNode*>, std::set<const CallICFGNode*>>
SlicerBase::collectCommonThreadStatements(const std::set<const SVFStmt*>& vulnerableStatements) {
    // Step 1: Collect pthread-related statements, i.e., pthread_create and pthread_join
    std::set<const CallICFGNode*> pthreadCallNodes = collectPthreadStatements(vulnerableStatements);

    // Step 2: Collect mutex-related statements
    std::set<const CallICFGNode*> mutexCallNodes = collectMutexStatements(vulnerableStatements);

    return std::make_pair(pthreadCallNodes, mutexCallNodes);
}

// Build backward ICFG node set from vulnerable nodes
std::set<const ICFGNode*> SlicerBase::buildBackwardICFGNodeSet(
    const std::set<const ICFGNode*>& vulnerableNodes) {
    std::set<const ICFGNode*> backwardICFGNodeSet;
    std::deque<const ICFGNode*> worklist;

    // Initialize with vulnerable nodes
    for (const ICFGNode* node : vulnerableNodes) {
        backwardICFGNodeSet.insert(node);
        worklist.push_back(node);
    }

    // Traverse backward along ICFG edges
    while (!worklist.empty()) {
        const ICFGNode* currICFGNode = worklist.front();
        worklist.pop_front();

        for (const ICFGEdge* inEdge : currICFGNode->getInEdges()) {
            const ICFGNode* srcNode = inEdge->getSrcNode();
            if (backwardICFGNodeSet.find(srcNode) == backwardICFGNodeSet.end()) {
                backwardICFGNodeSet.insert(srcNode);
                worklist.push_back(srcNode);
            }
        }
    }

    return backwardICFGNodeSet;
}

// Perform dual slicing (temporal slicing): filter statements based on control flow and parallel execution
std::set<const ICFGNode*> SlicerBase::runDualSlicing(
    const std::set<const ICFGNode*>& slicedNodes) {
    std::set<const ICFGNode*> dualSlicedNodes;

    // Build backward ICFG node set
    std::set<const ICFGNode*> backwardICFGNodeSet = buildBackwardICFGNodeSet(slicedNodes);

    // Perform control slicing
    for (const ICFGNode* stmtICFGNode : slicedNodes) {
        // Check if the ICFG node is in backward_icfg_node_set
        if (backwardICFGNodeSet.find(stmtICFGNode) != backwardICFGNodeSet.end()) {
            dualSlicedNodes.insert(stmtICFGNode);
        } else {
            // Check if it may happen in parallel with any vulnerable node
            for (const ICFGNode* bugICFGNode : slicedNodes) {
                if (mhp->mayHappenInParallelCache(stmtICFGNode, bugICFGNode)) {
                    dualSlicedNodes.insert(stmtICFGNode);
                    break;
                }
            }
        }
    }

    return dualSlicedNodes;
}

// Call-dependence expansion (used by MTASlicer).
std::set<const ICFGNode*> SlicerBase::expandCallDependence(
    const std::set<const ICFGNode*>& nodes) {

    // Determine keptFunctions from the given nodes
    std::set<const FunObjVar*> keptFunctions;
    for (const ICFGNode* node : nodes) {
        if (node != nullptr && node->getFun() != nullptr) {
            keptFunctions.insert(node->getFun());
        }
    }

    // Build ancestor closure (upward traversal in call graph)
    std::queue<const FunObjVar*> worklistFuncs;
    for (const FunObjVar* fun : keptFunctions) {
        worklistFuncs.push(fun);
    }

    std::unordered_map<const FunObjVar*, const CallGraphNode*> fun2Node;
    for (auto it = callGraph->begin(), eit = callGraph->end(); it != eit; ++it) {
        const CallGraphNode* node = it->second;
        if (node && node->getFunction()) {
            fun2Node[node->getFunction()] = node;
        }
    }

    std::set<const FunObjVar*> visitedFuncs = keptFunctions;
    while (!worklistFuncs.empty()) {
        const FunObjVar* target = worklistFuncs.front();
        worklistFuncs.pop();
        auto nodeIt = fun2Node.find(target);
        if (nodeIt == fun2Node.end()) continue;

        const CallGraphNode* node = nodeIt->second;
        for (const CallGraphEdge* inEdge : node->getInEdges()) {
            if (inEdge == nullptr) continue;
            const CallGraphNode* callerNode = inEdge->getSrcNode();
            if (callerNode && callerNode->getFunction()) {
                const FunObjVar* callerFun = callerNode->getFunction();
                if (visitedFuncs.find(callerFun) == visitedFuncs.end()) {
                    keptFunctions.insert(callerFun);
                    visitedFuncs.insert(callerFun);
                    worklistFuncs.push(callerFun);
                }
            }
        }
    }

    // For each keptFunction, add call/ret nodes and entry/exit nodes
    ICFG* icfg = svfIr->getICFG();
    std::set<const ICFGNode*> expandedNodes = nodes;
    for (const FunObjVar* fun : keptFunctions) {
        if (!fun) continue;

        // Add function entry/exit nodes
        if (fun->hasBasicBlock()) {
            if (FunEntryICFGNode* entry = icfg->getFunEntryICFGNode(fun)) {
                expandedNodes.insert(entry);
            }
            if (FunExitICFGNode* exit = icfg->getFunExitICFGNode(fun)) {
                expandedNodes.insert(exit);
            }
        }

        // Find all call/ret nodes that call this function
        auto funNodeIt = fun2Node.find(fun);
        if (funNodeIt != fun2Node.end()) {
            const CallGraphNode* calleeNode = funNodeIt->second;

            // Traverse all edges that call this function
            for (const CallGraphEdge* inEdge : calleeNode->getInEdges()) {
                if (inEdge == nullptr) continue;

                const CallGraphEdge::CallInstSet &directCalls = inEdge->getDirectCalls();
                const CallGraphEdge::CallInstSet &indirectCalls = inEdge->getIndirectCalls();

                for (const CallICFGNode* callNode : directCalls) {
                    if (callNode != nullptr) {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr) {
                            expandedNodes.insert(retNode);
                        }
                    }
                }

                for (const CallICFGNode* callNode : indirectCalls) {
                    if (callNode != nullptr) {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr) {
                            expandedNodes.insert(retNode);
                        }
                    }
                }
            }
        }
    }

    return expandedNodes;
}

//===----------------------------------------------------------------------===//
// MTASlicer
//===----------------------------------------------------------------------===//

MTASlicer::MTASlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                     LockAnalysis* lockAnalysis)
    : SlicerBase(svfIr, pta, mhp, lockAnalysis) {
}

// Perform slicing for MTA (includes function expansion for IRView)
std::set<const ICFGNode*> MTASlicer::runSlicing(
    const std::set<const SVFStmt*>& vulnerableStatements,
    const std::set<const ICFGNode*>& threadVFSources) {

    // Step 1: Collect common pthread and mutex statements
    auto commonStmts = collectCommonThreadStatements(vulnerableStatements);
    const std::set<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const std::set<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Step 2: Form initial slice result (before function expansion). The ILA
    // slicing sources are the [INIT] race statements plus the [THREAD-VF]
    // sources (MSli §4.2) extracted while building VFG_pre: the endpoints and
    // in-span lock witnesses queried during main-phase value-flow construction.
    std::set<const ICFGNode*> initialSliceResult;
    initialSliceResult.insert(pthreadCallNodes.begin(), pthreadCallNodes.end());
    for (const CallICFGNode* pthreadCallNode : pthreadCallNodes) {
        initialSliceResult.insert(pthreadCallNode->getRetICFGNode());
    }
    initialSliceResult.insert(mutexCallNodes.begin(), mutexCallNodes.end());
    for (const CallICFGNode* mutexCallNode : mutexCallNodes) {
        initialSliceResult.insert(mutexCallNode->getRetICFGNode());
    }
    for (const SVFStmt* stmt: vulnerableStatements) {
        initialSliceResult.insert(stmt->getICFGNode());
    }
    initialSliceResult.insert(threadVFSources.begin(), threadVFSources.end());

    // Step 3: Perform dual slicing (temporal slicing)
    std::set<const ICFGNode*> dualSlicedNodes = runDualSlicing(initialSliceResult);

    // Step 4: Expand keptNodes to include call/ret nodes and function entry/exit
    // nodes (call dependence).
    return expandCallDependence(dualSlicedNodes);
}

//===----------------------------------------------------------------------===//
// PTASlicer
//===----------------------------------------------------------------------===//

PTASlicer::PTASlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                     LockAnalysis* lockAnalysis, SVF::SVFG* vfg)
    : SlicerBase(svfIr, pta, mhp, lockAnalysis), vfg(vfg) {
}

// The FSPTA data-dependence slice at SVFG-node granularity (memoised so the
// backward closure over VFG_pre is computed once and shared with ILA slicing).
const std::set<const SVFGNode*>& PTASlicer::getRetainedSVFGNodes(
    const std::set<const SVFStmt*>& vulnerableStatements) {
    if (!retainedComputed) {
        retainedSVFGNodes = computeDataDependenceSVFGNodes(vulnerableStatements, vfg);
        retainedComputed = true;
    }
    return retainedSVFGNodes;
}

// Perform slicing for pointer analysis (returns only node set, no IRView needed)
std::set<const ICFGNode*> PTASlicer::runSlicing(
    const std::set<const SVFStmt*>& vulnerableStatements) {

    // Step 1: paper-faithful (§4.3) data-dependence slice over the thread-aware
    // SVFG built once in pre-analysis (reusing the memoised SVFG-node closure).
    std::set<const ICFGNode*> initialSliceResult =
        svfgNodesToICFGNodes(getRetainedSVFGNodes(vulnerableStatements), vulnerableStatements);

    // Step 2: Perform dual slicing (temporal slicing)
    std::set<const ICFGNode*> dualSlicedNodes = runDualSlicing(initialSliceResult);

    return dualSlicedNodes;
}

//===----------------------------------------------------------------------===//
// SingleSlicer
//===----------------------------------------------------------------------===//

SingleSlicer::SingleSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                           LockAnalysis* lockAnalysis, SVF::SVFG* vfg)
    : SlicerBase(svfIr, pta, mhp, lockAnalysis), vfg(vfg) {
}

// Single-pass slice (the baseline of MSli §3/§5.4): the transitive closure of
// the target statements under the COMBINED dependence graph -- synchronization,
// data, and call dependence -- yielding one slice shared by ILA and FSPTA.
std::set<const ICFGNode*> SingleSlicer::runSlicing(
    const std::set<const SVFStmt*>& vulnerableStatements) {

    // Step 1: Synchronization dependence -- the relevant pthread (fork/join) and
    // mutex (lock/unlock) statements for the targets.
    auto commonStmts = collectCommonThreadStatements(vulnerableStatements);
    const std::set<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const std::set<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Step 2: Seed the working set with the synchronization statements (and their
    // return nodes) and the target statements themselves.
    std::set<const ICFGNode*> currentNodes;
    currentNodes.insert(pthreadCallNodes.begin(), pthreadCallNodes.end());
    for (const CallICFGNode* pthreadCallNode : pthreadCallNodes)
        currentNodes.insert(pthreadCallNode->getRetICFGNode());
    currentNodes.insert(mutexCallNodes.begin(), mutexCallNodes.end());
    for (const CallICFGNode* mutexCallNode : mutexCallNodes)
        currentNodes.insert(mutexCallNode->getRetICFGNode());
    for (const SVFStmt* stmt : vulnerableStatements)
        currentNodes.insert(stmt->getICFGNode());

    // Step 3: Close over data dependence (the thread-aware VFG_pre value flow --
    // direct + indirect + interference, the same model PTASlicer uses) and call
    // dependence (function expansion), alternately, until the node set converges.
    bool changed = true;
    int iteration = 0;
    const int maxIterations = 100; // safety bound against non-convergence
    while (changed && iteration < maxIterations) {
        iteration++;
        std::set<const ICFGNode*> previousNodes = currentNodes;

        std::set<const SVFStmt*> currentStatements;
        for (const ICFGNode* node : currentNodes) {
            const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
            currentStatements.insert(stmts.begin(), stmts.end());
        }

        std::set<const ICFGNode*> dataDepNodes =
            sliceDataDependenceOverVFG(currentStatements, vfg);
        currentNodes.insert(dataDepNodes.begin(), dataDepNodes.end());

        currentNodes = expandCallDependence(currentNodes);

        changed = (currentNodes != previousNodes);
    }

    if (iteration >= maxIterations)
        SVFUtil::writeWrnMsg("SingleSlicer reached max iterations, may not have converged");

    // Step 4: One dual-slicing (temporal) pass at the end.
    return runDualSlicing(currentNodes);
}

} // namespace SVF
