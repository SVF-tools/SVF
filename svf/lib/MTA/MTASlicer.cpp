//===- MTASlicer.cpp -- Multi-stage on-demand program slicers -------------===//
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
 * MTASlicer.cpp
 *
 *      Author: Jiawei Yang
 *
 * MTASlicerBase + MultiStageSlicer + SingleSlicer -- the program slicers of
 * "Multi-Stage On-Demand Program Slicing for Modular Analysis of Multi-Threaded
 * Programs" (ISSTA 2026).
 */

#include "MTA/MTASlicer.h"
#include "MTA/TCT.h"
#include "SVFIR/SVFIR.h"
#include "Util/SVFUtil.h"
#include "Util/CxtStmt.h"
#include "Util/ThreadAPI.h"
#include <deque>
#include <algorithm>
#include <cassert>
#include "Graphs/ICFGEdge.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/CallGraph.h"
#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFVariables.h"
#include <fstream>
#include <sstream>
#include <queue>
#include <unordered_map>
#include <iostream>
#include <cctype>
#include "Graphs/SVFG.h"
#include "Graphs/VFGNode.h"
#include "Graphs/VFGEdge.h"

using namespace SVF;

namespace SVF
{

//===----------------------------------------------------------------------===//
// SlicedTCT - TCT rebuilt over the sliced ThreadCallGraph view.
//===----------------------------------------------------------------------===//

SlicedTCT::SlicedTCT(PointerAnalysis* p, const SlicedSVFIRView* slicedView, u32_t maxContextLen)
    : TCT(p),
      tcgView(slicedView != nullptr && slicedView->getThreadCallGraph() != nullptr ? slicedView->getThreadCallGraph() : nullptr),
      maxContextLen(maxContextLen)
{
    // Base class TCT(p) constructor called build() before tcgView was initialized.
    // Now that tcgView is initialized, rebuild using sliced view.
    if (tcgView != nullptr)
    {
        build();
    }
}

void SlicedTCT::build()
{
    if (tcgView == nullptr)
    {
        TCT::build();
        return;
    }

    // The base TCT(p) constructor already built the whole-program TCT. Reset it to
    // a clean graph so the slice rebuilds from scratch (addGNode reissues ids from
    // 0; stale nodes left behind would later fail the MHP getCxtOfCxtThread lookup).
    reset();

    // Call base class build() which will call our overridden methods
    // (markRelProcs, collectLoopInfoForJoin, collectEntryFunInCallGraph)
    // that filter using sliced view, then build the TCT tree
    TCT::build();
}

void SlicedTCT::markRelProcs()
{
    if (tcgView == nullptr)
    {
        TCT::markRelProcs();
        return;
    }

    // Get kept fork sites from sliced view
    std::vector<const ICFGNode*> keptForkSites;
    getKeptForkSites(keptForkSites);

    for (const ICFGNode* forkSite : keptForkSites)
    {
        // Get function from fork site
        const FunObjVar* svfun = forkSite->getFun();
        TCT::markRelProcs(svfun);

        // Get fork edges from sliced view (use tcgView to get out edges)
        const CallICFGNode* callNode = SVFUtil::cast<CallICFGNode>(forkSite);
        const FunObjVar* callerFun = callNode->getFun();
        CallGraphNode* callerNode = tcg->getCallGraphNode(callerFun);

        if (!isKeptNode(callerNode))
            continue;

        // Use sliced view to get out edges (which filters by kept edges)
        std::vector<const CallGraphEdge*> outEdges;
        tcgView->getOutEdgesOf(callerNode, outEdges);

        for (const CallGraphEdge* edge : outEdges)
        {
            // Check if this is a fork edge
            if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge)
            {
                const CallGraphNode* forkeeNode = edge->getDstNode();
                if (isKeptNode(forkeeNode))
                {
                    // Check if this edge corresponds to the fork site
                    const CallGraphEdge::CallInstSet& directCalls = edge->getDirectCalls();
                    const CallGraphEdge::CallInstSet& indirectCalls = edge->getIndirectCalls();

                    if (directCalls.count(callNode) > 0 || indirectCalls.count(callNode) > 0)
                    {
                        const FunObjVar* forkeeFun = forkeeNode->getFunction();
                        candidateFuncSet.insert(forkeeFun);
                    }
                }
            }
        }
    }

    // Get kept join sites from sliced view
    std::vector<const ICFGNode*> keptJoinSites;
    getKeptJoinSites(keptJoinSites);

    for (const ICFGNode* joinSite : keptJoinSites)
    {
        const FunObjVar* svfun = joinSite->getFun();
        TCT::markRelProcs(svfun);
    }

    if(getMakredProcs().empty())
        SVFUtil::writeWrnMsg("We didn't recognize any fork site, this is single thread program?");
}

void SlicedTCT::markRelProcs(const FunObjVar* fun)
{
    // Call base class implementation
    TCT::markRelProcs(fun);
}

void SlicedTCT::collectLoopInfoForJoin()
{
    if (tcgView == nullptr)
    {
        TCT::collectLoopInfoForJoin();
        return;
    }

    // Get kept join sites from sliced view
    std::vector<const ICFGNode*> keptJoinSites;
    getKeptJoinSites(keptJoinSites);

    for(const ICFGNode* join : keptJoinSites)
    {
        const FunObjVar* svffun = join->getFun();
        const SVFBasicBlock* svfbb = join->getBB();

        if(svffun->hasLoopInfo(svfbb))
        {
            const LoopBBs& lp = svffun->getLoopInfo(svfbb);
            if(!lp.empty() && isJoinMustExecutedInLoop(lp,join))
            {
                joinSiteToLoopMap[join] = lp;
            }
        }

        if(isInRecursion(join))
        {
            inRecurJoinSites.insert(join);
        }
    }
}

void SlicedTCT::collectEntryFunInCallGraph()
{
    if (tcgView == nullptr)
    {
        TCT::collectEntryFunInCallGraph();
        return;
    }

    // Use sliced CallGraph view to collect entry functions
    // Only collect functions that are kept in sliced view
    const OrderedSet<const CallGraphNode*>& keptNodes = tcgView->getKeptNodes();

    for (const CallGraphNode* node : keptNodes)
    {
        const FunObjVar* fun = node->getFunction();
        if (SVFUtil::isExtCall(fun))
            continue;

        // Check if this node has no incoming edges in the sliced view
        std::vector<const CallGraphEdge*> inEdges;
        tcgView->getInEdgesOf(node, inEdges);
        if (inEdges.empty())
        {
            entryFuncSet.insert(fun);
        }
    }

    assert(!getEntryProcs().empty() && "Can't find any function in module!");
}

void SlicedTCT::handleCallRelation(CxtThreadProc& ctp, const CallGraphEdge* cgEdge, const CallICFGNode* cs)
{
    // Check if the call site and callee are kept in sliced view
    if (!isKeptEdge(cgEdge))
        return;

    const CallGraphNode* dstNode = cgEdge->getDstNode();
    if (!isKeptNode(dstNode))
        return;

    // Call base class implementation
    TCT::handleCallRelation(ctp, cgEdge, cs);
}

void SlicedTCT::pushCxt(CallStrCxt& cxt, const CallICFGNode* call, const FunObjVar* callee)
{
    const FunObjVar* caller = call->getFun();
    CallSiteID csId = tcg->getCallSiteID(call, callee);

    if(inSameCallGraphSCC(tcg->getCallGraphNode(caller), tcg->getCallGraphNode(callee)) == false)
    {
        cxt.push_back(csId);
        // Use custom maxContextLen if set, otherwise use Options::MaxContextLen()
        u32_t maxLen = (maxContextLen > 0) ? maxContextLen : Options::MaxContextLen();
        if (cxt.size() > maxLen)
            cxt.erase(cxt.begin());
        if (cxt.size() > MaxCxtSize)
            MaxCxtSize = cxt.size();
        DBOUT(DMTA, dumpCxt(cxt));
    }
}

bool SlicedTCT::isKeptNode(const CallGraphNode* node) const
{
    if (tcgView == nullptr)
        return true;
    return tcgView->isKeptNode(node);
}

bool SlicedTCT::isKeptEdge(const CallGraphEdge* edge) const
{
    if (tcgView == nullptr)
        return true;
    const CallGraph::CallGraphEdgeSet& keptEdges = tcgView->getKeptEdges();
    return keptEdges.find(const_cast<CallGraphEdge*>(edge)) != keptEdges.end();
}

void SlicedTCT::getKeptForkSites(std::vector<const ICFGNode*>& out) const
{
    out.clear();
    if (tcgView == nullptr)
    {
        // Fall back to original
        for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
        {
            out.push_back(*it);
        }
        return;
    }

    // Get all fork sites from original ThreadCallGraph, but filter by:
    // 1. The function containing the fork site is kept
    // 2. There exists a kept fork edge from this fork site
    for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->forksitesBegin(), eit = tcg->forksitesEnd(); it != eit; ++it)
    {
        const ICFGNode* forkSite = *it;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(forkSite);
        if (callNode == nullptr)
            continue;

        // Check if the function containing the fork site is kept
        const FunObjVar* fun = forkSite->getFun();
        CallGraphNode* cgNode = tcg->getCallGraphNode(fun);
        if (!isKeptNode(cgNode))
            continue;

        // Check if there's a kept fork edge from this fork site
        // Use sliced view to get out edges
        std::vector<const CallGraphEdge*> outEdges;
        tcgView->getOutEdgesOf(cgNode, outEdges);

        bool hasKeptForkEdge = false;
        for (const CallGraphEdge* edge : outEdges)
        {
            if (edge->getEdgeKind() == CallGraphEdge::TDForkEdge)
            {
                // Check if this edge corresponds to the fork site
                const CallGraphEdge::CallInstSet& directCalls = edge->getDirectCalls();
                const CallGraphEdge::CallInstSet& indirectCalls = edge->getIndirectCalls();

                if (directCalls.count(callNode) > 0 || indirectCalls.count(callNode) > 0)
                {
                    // Check if the destination is kept
                    const CallGraphNode* dstNode = edge->getDstNode();
                    if (isKeptNode(dstNode))
                    {
                        hasKeptForkEdge = true;
                        break;
                    }
                }
            }
        }

        if (hasKeptForkEdge)
        {
            out.push_back(forkSite);
        }
    }
}

void SlicedTCT::getKeptJoinSites(std::vector<const ICFGNode*>& out) const
{
    out.clear();
    if (tcgView == nullptr)
    {
        // Fall back to original
        for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->joinsitesBegin(), eit = tcg->joinsitesEnd(); it != eit; ++it)
        {
            out.push_back(*it);
        }
        return;
    }

    // Get all join sites from original ThreadCallGraph, but filter by:
    // 1. The function containing the join site is kept
    // 2. There exists a kept join edge to this join site
    for (ThreadCallGraph::CallSiteSet::const_iterator it = tcg->joinsitesBegin(), eit = tcg->joinsitesEnd(); it != eit; ++it)
    {
        const ICFGNode* joinSite = *it;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(joinSite);
        if (callNode == nullptr)
            continue;

        // Check if the function containing the join site is kept
        const FunObjVar* fun = joinSite->getFun();
        CallGraphNode* cgNode = tcg->getCallGraphNode(fun);
        if (!isKeptNode(cgNode))
            continue;

        // Check if there's a kept join edge to this join site
        // Use sliced view to get in edges
        std::vector<const CallGraphEdge*> inEdges;
        tcgView->getInEdgesOf(cgNode, inEdges);

        bool hasKeptJoinEdge = false;
        for (const CallGraphEdge* edge : inEdges)
        {
            if (edge->getEdgeKind() == CallGraphEdge::TDJoinEdge)
            {
                // Check if this edge corresponds to the join site
                const CallGraphEdge::CallInstSet& directCalls = edge->getDirectCalls();
                const CallGraphEdge::CallInstSet& indirectCalls = edge->getIndirectCalls();

                if (directCalls.count(callNode) > 0 || indirectCalls.count(callNode) > 0)
                {
                    // Check if the source is kept
                    const CallGraphNode* srcNode = edge->getSrcNode();
                    if (isKeptNode(srcNode))
                    {
                        hasKeptJoinEdge = true;
                        break;
                    }
                }
            }
        }

        if (hasKeptJoinEdge)
        {
            out.push_back(joinSite);
        }
    }
}

//===----------------------------------------------------------------------===//
// MTASlicerBase
//===----------------------------------------------------------------------===//

MTASlicerBase::MTASlicerBase(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                             LockAnalysis* lockAnalysis, SVFG* vfg)
    : svfIr(svfIr), pta(pta), mhp(mhp), lockAnalysis(lockAnalysis), vfg(vfg)
{
    callGraph = pta->getCallGraph();
}

MTASlicerBase::~MTASlicerBase()
{
    // No dynamic memory to clean up - all pointers are managed externally
}

// Helper: Get lock set for an ICFG node
OrderedSet<const ICFGNode*> MTASlicerBase::getLockSet(const ICFGNode* node)
{
    OrderedSet<const ICFGNode*> allLockSites;

    // Get intra-procedural locks
    if (lockAnalysis->isInsideIntraLock(node) &&
            !lockAnalysis->isInsideCondIntraLock(node))
    {
        const LockAnalysis::InstSet& intraLocks = lockAnalysis->getIntraLockSet(node);
        for (const ICFGNode* lockSite : intraLocks)
        {
            allLockSites.insert(lockSite);
        }
    }

    // Get context-sensitive locks
    if (lockAnalysis->hasCxtStmtFromInst(node))
    {
        const LockAnalysis::CxtStmtSet& cxtStmts = lockAnalysis->getCxtStmtsFromInst(node);
        for (const CxtStmt& cxtStmt : cxtStmts)
        {
            if (lockAnalysis->hasCxtLockfromCxtStmt(cxtStmt))
            {
                const LockAnalysis::CxtLockSet& cxtLocks =
                    lockAnalysis->getCxtLockfromCxtStmt(cxtStmt);
                for (const LockAnalysis::CxtLock& cxtLock : cxtLocks)
                {
                    allLockSites.insert(cxtLock.getStmt());
                }
            }
        }
    }

    return allLockSites;
}

// Helper: Get TCTNode set from ICFGNode
OrderedSet<const TCTNode*> MTASlicerBase::getTCTNodeSetFromNode(const ICFGNode* node)
{
    OrderedSet<const TCTNode*> tctNodeSet;

    if (mhp->hasThreadStmtSet(node))
    {
        for (const CxtThreadStmt& cts : mhp->getThreadStmtSet(node))
        {
            if (mhp->getTCT()->hasGNode(cts.getTid()))
            {
                tctNodeSet.insert(mhp->getTCT()->getTCTNode(cts.getTid()));
            }
        }
    }

    return tctNodeSet;
}

// Helper: Get dependent thread create statements
OrderedSet<const SVFStmt*> MTASlicerBase::getDependentThreadCreate(const SVFStmt* stmt)
{
    OrderedSet<const SVFStmt*> forkSiteStmts;

    const ICFGNode* icfgNode = stmt->getICFGNode();
    OrderedSet<const TCTNode*> tctNodeSet = getTCTNodeSetFromNode(icfgNode);

    for (const TCTNode* tctNode : tctNodeSet)
    {
        const CxtThread& cxtThread = tctNode->getCxtThread();
        const ICFGNode* forkSite = cxtThread.getThread();
        if (forkSite != nullptr)
        {
            const ICFGNode::SVFStmtList& svfStmts = forkSite->getSVFStmts();
            if (!svfStmts.empty())
            {
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
OrderedSet<const SVFGNode*> MTASlicerBase::computeDataDependenceSVFGNodes(
    const OrderedSet<const SVFStmt*>& seeds, SVFG* vfg)
{

    assert(vfg != nullptr && "data-dependence slice requires the thread-aware VFG_pre");

    OrderedSet<const SVFGNode*> visited;
    std::deque<const SVFGNode*> worklist;
    auto seed = [&](const SVFGNode* n)
    {
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
    while (!worklist.empty())
    {
        const SVFGNode* n = worklist.front();
        worklist.pop_front();
        for (const VFGEdge* e : n->getInEdges())
            seed(e->getSrcNode());
    }

    return visited;
}

// Project retained VFG nodes (plus the seeds) onto their ICFG nodes.
OrderedSet<const ICFGNode*> MTASlicerBase::svfgNodesToICFGNodes(
    const OrderedSet<const SVFGNode*>& nodes, const OrderedSet<const SVFStmt*>& seeds)
{
    OrderedSet<const ICFGNode*> result;
    for (const SVFGNode* n : nodes)
        if (const StmtVFGNode* s = SVFUtil::dyn_cast<StmtVFGNode>(n))
            if (s->getICFGNode() != nullptr)
                result.insert(s->getICFGNode());
    for (const SVFStmt* stmt : seeds)
        result.insert(stmt->getICFGNode());
    return result;
}

OrderedSet<const ICFGNode*> MTASlicerBase::sliceDataDependenceOverVFG(
    const OrderedSet<const SVFStmt*>& seeds, SVFG* vfg)
{
    return svfgNodesToICFGNodes(computeDataDependenceSVFGNodes(seeds, vfg), seeds);
}

// Helper: Collect pthread-related statements (create and join)
OrderedSet<const CallICFGNode*> MTASlicerBase::collectPthreadStatements(
    const OrderedSet<const SVFStmt*>& vulnerableStmts)
{
    OrderedSet<const CallICFGNode*> pthreadCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map pthread_create nodes to their corresponding pthread_join nodes
    OrderedSet<const SVFStmt*> pthreadCreateStmts;

    // First pass: collect all pthread_create nodes
    for (const SVFStmt* stmt : vulnerableStmts)
    {
        OrderedSet<const SVFStmt*> forkSiteStmts = getDependentThreadCreate(stmt);

        for (const SVFStmt* forkSiteStmt : forkSiteStmts)
        {
            const ICFGNode* forkSiteNode = forkSiteStmt->getICFGNode();
            const CallICFGNode* forkCallNode = SVFUtil::dyn_cast<CallICFGNode>(forkSiteNode);
            if (forkCallNode != nullptr && threadAPI->isTDFork(forkCallNode))
            {
                pthreadCallNodes.insert(forkCallNode);
                pthreadCreateStmts.insert(forkSiteStmt);
            }
        }
    }

    // Second pass: find corresponding pthread_join nodes
    ICFG* icfg = svfIr->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDJoin(callNode))
        {
            const SVFVar* joinThread = threadAPI->getJoinedThread(callNode);
            if (joinThread != nullptr)
            {
                for (auto& createStmt : pthreadCreateStmts)
                {
                    const ICFGNode* createNode = createStmt->getICFGNode();
                    const CallICFGNode* createCallNode = SVFUtil::dyn_cast<CallICFGNode>(createNode);
                    if (createCallNode != nullptr)
                    {
                        const SVFVar* forkedThread = threadAPI->getForkedThread(createCallNode);
                        if (forkedThread != nullptr && threadAPI->isAliasedForkJoin(pta, forkedThread, joinThread))
                        {
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
OrderedSet<const CallICFGNode*> MTASlicerBase::collectMutexStatements(
    const OrderedSet<const SVFStmt*>& vulnerableStmts)
{
    OrderedSet<const CallICFGNode*> mutexCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map mutex_lock nodes to their corresponding mutex_unlock nodes
    OrderedSet<const CallICFGNode*> mutexLockCallNodes;

    // First pass: collect all mutex_lock nodes from lock sets
    for (const SVFStmt* stmt : vulnerableStmts)
    {
        OrderedSet<const ICFGNode*> lockSet = getLockSet(stmt->getICFGNode());
        for (const ICFGNode* lockNode : lockSet)
        {
            const CallICFGNode* lockCallNode = SVFUtil::dyn_cast<CallICFGNode>(lockNode);
            if (lockCallNode != nullptr && threadAPI->isTDAcquire(lockCallNode))
            {
                mutexCallNodes.insert(lockCallNode);
                mutexLockCallNodes.insert(lockCallNode);
            }
        }
    }

    // Second pass: find corresponding mutex_unlock nodes
    ICFG* icfg = svfIr->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end(); it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDRelease(callNode))
        {
            const SVFVar* unlockVar = threadAPI->getLockVal(callNode);
            if (unlockVar != nullptr)
            {
                for (auto lockCallNode : mutexLockCallNodes)
                {
                    if (lockCallNode != nullptr)
                    {
                        const SVFVar* lockVar = threadAPI->getLockVal(lockCallNode);
                        if (lockVar != nullptr && pta->alias(unlockVar->getId(), lockVar->getId()))
                        {
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
std::pair<OrderedSet<const CallICFGNode*>, OrderedSet<const CallICFGNode*>>
        MTASlicerBase::collectCommonThreadStatements(const OrderedSet<const SVFStmt*>& vulnerableStatements)
{
    // Step 1: Collect pthread-related statements, i.e., pthread_create and pthread_join
    OrderedSet<const CallICFGNode*> pthreadCallNodes = collectPthreadStatements(vulnerableStatements);

    // Step 2: Collect mutex-related statements
    OrderedSet<const CallICFGNode*> mutexCallNodes = collectMutexStatements(vulnerableStatements);

    return std::make_pair(pthreadCallNodes, mutexCallNodes);
}

// Keep the control-flow marker nodes the (sliced) lock analysis depends on:
// every lock/unlock-bearing function's entry and exit nodes.
//
// LockAnalysis classifies an intra lock as *partial* (conditional) by reaching
// the function entry node from the unlock along a lock-free backward path
// (intraBackwardTraverse: `entryInst == I` -> return false), and bails the
// forward span at the function exit node (`exitInst == I`). These checks are
// node-identity tests against the entry/exit markers. The data-dependence slice
// does not otherwise retain those markers, so on the sliced view the backward
// walk can never match the entry node and the lock is mis-classified as total --
// which makes isProtectedByCommonCILock report a spurious common lock and drops
// a real race (a query-preservation violation). Bridging preserves reachability
// *to* the kept markers, so once they are retained the sliced lock analysis
// reproduces the whole-program classification. Markers used: entry block
// front (cxt-lock start) and back (intra backward marker), and exit block back
// (intra forward marker).
static void collectLockFunctionMarkers(
    const OrderedSet<const CallICFGNode*>& mutexCallNodes,
    OrderedSet<const ICFGNode*>& sliceResult)
{
    for (const CallICFGNode* mutexCallNode : mutexCallNodes)
    {
        const FunObjVar* fun = mutexCallNode->getFun();
        if (fun == nullptr)
            continue;
        if (const SVFBasicBlock* entry = fun->getEntryBlock())
        {
            sliceResult.insert(entry->front());
            sliceResult.insert(entry->back());
        }
        if (const SVFBasicBlock* exit = fun->getExitBB())
            sliceResult.insert(exit->back());
    }
}

// Build backward ICFG node set from vulnerable nodes
OrderedSet<const ICFGNode*> MTASlicerBase::buildBackwardICFGNodeSet(
    const OrderedSet<const ICFGNode*>& vulnerableNodes)
{
    OrderedSet<const ICFGNode*> backwardICFGNodeSet;
    std::deque<const ICFGNode*> worklist;

    // Initialize with vulnerable nodes
    for (const ICFGNode* node : vulnerableNodes)
    {
        backwardICFGNodeSet.insert(node);
        worklist.push_back(node);
    }

    // Traverse backward along ICFG edges
    while (!worklist.empty())
    {
        const ICFGNode* currICFGNode = worklist.front();
        worklist.pop_front();

        for (const ICFGEdge* inEdge : currICFGNode->getInEdges())
        {
            const ICFGNode* srcNode = inEdge->getSrcNode();
            if (backwardICFGNodeSet.find(srcNode) == backwardICFGNodeSet.end())
            {
                backwardICFGNodeSet.insert(srcNode);
                worklist.push_back(srcNode);
            }
        }
    }

    return backwardICFGNodeSet;
}

// Perform dual slicing (temporal slicing): filter statements based on control flow and parallel execution
OrderedSet<const ICFGNode*> MTASlicerBase::runDualSlicing(
    const OrderedSet<const ICFGNode*>& slicedNodes)
{
    OrderedSet<const ICFGNode*> dualSlicedNodes;

    // Build backward ICFG node set
    OrderedSet<const ICFGNode*> backwardICFGNodeSet = buildBackwardICFGNodeSet(slicedNodes);

    // Perform control slicing
    for (const ICFGNode* stmtICFGNode : slicedNodes)
    {
        // Check if the ICFG node is in backward_icfg_node_set
        if (backwardICFGNodeSet.find(stmtICFGNode) != backwardICFGNodeSet.end())
        {
            dualSlicedNodes.insert(stmtICFGNode);
        }
        else
        {
            // Check if it may happen in parallel with any vulnerable node
            for (const ICFGNode* bugICFGNode : slicedNodes)
            {
                if (mhp->mayHappenInParallelCache(stmtICFGNode, bugICFGNode))
                {
                    dualSlicedNodes.insert(stmtICFGNode);
                    break;
                }
            }
        }
    }

    return dualSlicedNodes;
}

// Call-dependence expansion (used by MultiStageSlicer).
OrderedSet<const ICFGNode*> MTASlicerBase::expandCallDependence(
    const OrderedSet<const ICFGNode*>& nodes)
{

    // Determine keptFunctions from the given nodes
    OrderedSet<const FunObjVar*> keptFunctions;
    for (const ICFGNode* node : nodes)
    {
        if (node != nullptr && node->getFun() != nullptr)
        {
            keptFunctions.insert(node->getFun());
        }
    }

    // Build ancestor closure (upward traversal in call graph)
    std::queue<const FunObjVar*> worklistFuncs;
    for (const FunObjVar* fun : keptFunctions)
    {
        worklistFuncs.push(fun);
    }

    Map<const FunObjVar*, const CallGraphNode*> fun2Node;
    for (auto it = callGraph->begin(), eit = callGraph->end(); it != eit; ++it)
    {
        const CallGraphNode* node = it->second;
        if (node && node->getFunction())
        {
            fun2Node[node->getFunction()] = node;
        }
    }

    OrderedSet<const FunObjVar*> visitedFuncs = keptFunctions;
    while (!worklistFuncs.empty())
    {
        const FunObjVar* target = worklistFuncs.front();
        worklistFuncs.pop();
        auto nodeIt = fun2Node.find(target);
        if (nodeIt == fun2Node.end()) continue;

        const CallGraphNode* node = nodeIt->second;
        for (const CallGraphEdge* inEdge : node->getInEdges())
        {
            if (inEdge == nullptr) continue;
            const CallGraphNode* callerNode = inEdge->getSrcNode();
            if (callerNode && callerNode->getFunction())
            {
                const FunObjVar* callerFun = callerNode->getFunction();
                if (visitedFuncs.find(callerFun) == visitedFuncs.end())
                {
                    keptFunctions.insert(callerFun);
                    visitedFuncs.insert(callerFun);
                    worklistFuncs.push(callerFun);
                }
            }
        }
    }

    // For each keptFunction, add call/ret nodes and entry/exit nodes
    ICFG* icfg = svfIr->getICFG();
    OrderedSet<const ICFGNode*> expandedNodes = nodes;
    for (const FunObjVar* fun : keptFunctions)
    {
        if (!fun) continue;

        // Add function entry/exit nodes
        if (fun->hasBasicBlock())
        {
            if (FunEntryICFGNode* entry = icfg->getFunEntryICFGNode(fun))
            {
                expandedNodes.insert(entry);
            }
            if (FunExitICFGNode* exit = icfg->getFunExitICFGNode(fun))
            {
                expandedNodes.insert(exit);
            }
        }

        // Find all call/ret nodes that call this function
        auto funNodeIt = fun2Node.find(fun);
        if (funNodeIt != fun2Node.end())
        {
            const CallGraphNode* calleeNode = funNodeIt->second;

            // Traverse all edges that call this function
            for (const CallGraphEdge* inEdge : calleeNode->getInEdges())
            {
                if (inEdge == nullptr) continue;

                const CallGraphEdge::CallInstSet &directCalls = inEdge->getDirectCalls();
                const CallGraphEdge::CallInstSet &indirectCalls = inEdge->getIndirectCalls();

                for (const CallICFGNode* callNode : directCalls)
                {
                    if (callNode != nullptr)
                    {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr)
                        {
                            expandedNodes.insert(retNode);
                        }
                    }
                }

                for (const CallICFGNode* callNode : indirectCalls)
                {
                    if (callNode != nullptr)
                    {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr)
                        {
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
// MultiStageSlicer
//===----------------------------------------------------------------------===//

MultiStageSlicer::MultiStageSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                                   LockAnalysis* lockAnalysis, SVFG* vfg)
    : MTASlicerBase(svfIr, pta, mhp, lockAnalysis, vfg)
{
}

// Perform slicing for MTA (includes function expansion for IRView)
OrderedSet<const ICFGNode*> MultiStageSlicer::runILASlicing(
    const OrderedSet<const SVFStmt*>& vulnerableStatements,
    const OrderedSet<const ICFGNode*>& threadVFSources)
{

    // Step 1: Collect common pthread and mutex statements
    auto commonStmts = collectCommonThreadStatements(vulnerableStatements);
    const OrderedSet<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const OrderedSet<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Step 2: Form initial slice result (before function expansion). The ILA
    // slicing sources are the [INIT] race statements plus the [THREAD-VF]
    // sources (MSli §4.2) extracted while building VFG_pre: the endpoints and
    // in-span lock witnesses queried during main-phase value-flow construction.
    OrderedSet<const ICFGNode*> initialSliceResult;
    initialSliceResult.insert(pthreadCallNodes.begin(), pthreadCallNodes.end());
    for (const CallICFGNode* pthreadCallNode : pthreadCallNodes)
    {
        initialSliceResult.insert(pthreadCallNode->getRetICFGNode());
    }
    initialSliceResult.insert(mutexCallNodes.begin(), mutexCallNodes.end());
    for (const CallICFGNode* mutexCallNode : mutexCallNodes)
    {
        initialSliceResult.insert(mutexCallNode->getRetICFGNode());
    }
    collectLockFunctionMarkers(mutexCallNodes, initialSliceResult);
    for (const SVFStmt* stmt: vulnerableStatements)
    {
        initialSliceResult.insert(stmt->getICFGNode());
    }
    initialSliceResult.insert(threadVFSources.begin(), threadVFSources.end());

    // Retain loop-exit entry anchors. MHP seeds post-join interleaving directly at
    // the loop-exit block entry of each join in a loop; keeping those entries makes
    // the seed land on a kept node, so no runtime seed projection is needed. Reuses
    // the shared FunObjVar::getExitBlocksOfLoop.
    ThreadAPI* threadAPI = mhp->getThreadCallGraph()->getThreadAPI();
    for (const CallICFGNode* c : pthreadCallNodes)
    {
        if (!threadAPI->isTDJoin(c) || c->getBB() == nullptr) continue;
        std::vector<const SVFBasicBlock*> exitbbs;
        c->getFun()->getExitBlocksOfLoop(c->getBB(), exitbbs);
        for (const SVFBasicBlock* eb : exitbbs)
            if (!eb->getICFGNodeList().empty())
                initialSliceResult.insert(eb->front());
    }

    // Step 3: Perform dual slicing (temporal slicing)
    OrderedSet<const ICFGNode*> dualSlicedNodes = runDualSlicing(initialSliceResult);

    // Step 4: Expand keptNodes to include call/ret nodes and function entry/exit
    // nodes (call dependence).
    return expandCallDependence(dualSlicedNodes);
}

//===----------------------------------------------------------------------===//
// MultiStageSlicer -- stage 2 (FSPTA data-dependence slice)
//===----------------------------------------------------------------------===//

// The FSPTA data-dependence slice at SVFG-node granularity (memoised so the
// backward closure over VFG_pre is computed once and shared with ILA slicing).
const OrderedSet<const SVFGNode*>& MultiStageSlicer::getRetainedSVFGNodes(
    const OrderedSet<const SVFStmt*>& vulnerableStatements)
{
    if (!retainedComputed)
    {
        retainedSVFGNodes = computeDataDependenceSVFGNodes(vulnerableStatements, vfg);
        retainedComputed = true;
    }
    return retainedSVFGNodes;
}

// Perform slicing for pointer analysis (returns only node set, no IRView needed)
OrderedSet<const ICFGNode*> MultiStageSlicer::runPTASlicing(
    const OrderedSet<const SVFStmt*>& vulnerableStatements)
{

    // Step 1: paper-faithful (§4.3) data-dependence slice over the thread-aware
    // SVFG built once in pre-analysis (reusing the memoised SVFG-node closure).
    OrderedSet<const ICFGNode*> initialSliceResult =
        svfgNodesToICFGNodes(getRetainedSVFGNodes(vulnerableStatements), vulnerableStatements);

    // Retain the lock/unlock-function control-flow markers the sliced lock
    // analysis depends on (see collectLockFunctionMarkers); the data-dependence
    // SVFG slice does not otherwise keep them.
    collectLockFunctionMarkers(collectMutexStatements(vulnerableStatements), initialSliceResult);

    // Step 2: Perform dual slicing (temporal slicing)
    OrderedSet<const ICFGNode*> dualSlicedNodes = runDualSlicing(initialSliceResult);

    return dualSlicedNodes;
}

//===----------------------------------------------------------------------===//
// SingleSlicer
//===----------------------------------------------------------------------===//

SingleSlicer::SingleSlicer(SVFIR* svfIr, AndersenBase* pta, MHP* mhp,
                           LockAnalysis* lockAnalysis, SVFG* vfg)
    : MTASlicerBase(svfIr, pta, mhp, lockAnalysis, vfg)
{
}

// Single-pass slice (the baseline of MSli §3/§5.4): the transitive closure of
// the target statements under the COMBINED dependence graph -- synchronization,
// data, and call dependence -- yielding one slice shared by ILA and FSPTA.
OrderedSet<const ICFGNode*> SingleSlicer::runSlicing(
    const OrderedSet<const SVFStmt*>& vulnerableStatements)
{

    // Step 1: Synchronization dependence -- the relevant pthread (fork/join) and
    // mutex (lock/unlock) statements for the targets.
    auto commonStmts = collectCommonThreadStatements(vulnerableStatements);
    const OrderedSet<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const OrderedSet<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Step 2: Seed the working set with the synchronization statements (and their
    // return nodes) and the target statements themselves.
    OrderedSet<const ICFGNode*> currentNodes;
    currentNodes.insert(pthreadCallNodes.begin(), pthreadCallNodes.end());
    for (const CallICFGNode* pthreadCallNode : pthreadCallNodes)
        currentNodes.insert(pthreadCallNode->getRetICFGNode());
    currentNodes.insert(mutexCallNodes.begin(), mutexCallNodes.end());
    for (const CallICFGNode* mutexCallNode : mutexCallNodes)
        currentNodes.insert(mutexCallNode->getRetICFGNode());
    collectLockFunctionMarkers(mutexCallNodes, currentNodes);
    for (const SVFStmt* stmt : vulnerableStatements)
        currentNodes.insert(stmt->getICFGNode());

    // Step 3: Close over data dependence (the thread-aware VFG_pre value flow --
    // direct + indirect + interference, the same model the FSPTA stage uses) and call
    // dependence (function expansion), alternately, until the node set converges.
    bool changed = true;
    int iteration = 0;
    const int maxIterations = 100; // safety bound against non-convergence
    while (changed && iteration < maxIterations)
    {
        iteration++;
        OrderedSet<const ICFGNode*> previousNodes = currentNodes;

        OrderedSet<const SVFStmt*> currentStatements;
        for (const ICFGNode* node : currentNodes)
        {
            const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
            currentStatements.insert(stmts.begin(), stmts.end());
        }

        OrderedSet<const ICFGNode*> dataDepNodes =
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

} // End namespace SVF
