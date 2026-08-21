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
 */

#include "MTA/MTASlicer.h"
#include "MTA/TCT.h"
#include "SVFIR/SVFIR.h"
#include "Util/SVFUtil.h"
#include "Util/CxtStmt.h"
#include "Util/ThreadAPI.h"
#include <deque>
#include <cassert>
#include "Graphs/ICFGEdge.h"
#include "Graphs/ICFGNode.h"
#include "Graphs/CallGraph.h"
#include "SVFIR/SVFStatements.h"
#include "SVFIR/SVFVariables.h"
#include <queue>
#include "Graphs/SVFG.h"
#include "Graphs/VFGNode.h"
#include "Graphs/VFGEdge.h"

using namespace SVF;

namespace SVF
{

//===----------------------------------------------------------------------===//
// SlicedTCT - TCT rebuilt over the sliced ThreadCallGraph view.
//===----------------------------------------------------------------------===//

std::unique_ptr<SlicedTCT> SlicedTCT::create(
    PointerAnalysis& pointerAnalysis, const SlicedSVFIRView& slicedView,
    u32_t contextLimit)
{
    std::unique_ptr<SlicedTCT> tct(
        new SlicedTCT(pointerAnalysis, slicedView, contextLimit));
    tct->build();
    return tct;
}

SlicedTCT::SlicedTCT(PointerAnalysis& pointerAnalysis,
                     const SlicedSVFIRView& slicedView, u32_t contextLimit)
    : TCT(&pointerAnalysis, contextLimit),
      tcgView(slicedView.getThreadCallGraph())
{
    assert(tcgView != nullptr && "SlicedTCT requires a sliced thread call graph");
}

void SlicedTCT::build()
{
    markRelProcs();
    collectLoopInfoForJoin();
    collectEntryFunInCallGraph();

    for (const FunObjVar* entry : entryFuncSet)
    {
        if (!isCandidateFun(entry))
            continue;
        CallStrCxt context;
        CxtThreadProc parent(-1, context, nullptr);
        TCTNode* root = getOrCreateTCTNode(
                            context, createDummyForkSite(), parent, entry);
        pushToCTPWorkList(CxtThreadProc(root->getId(), context, entry));
    }

    while (!ctpList.empty())
    {
        CxtThreadProc process = popFromCTPWorkList();
        CallGraphNode* node = tcg->getCallGraphNode(process.getProc());
        if (!isCandidateFun(node->getFunction()) || !isKeptNode(node))
            continue;

        std::vector<const CallGraphEdge*> outEdges;
        tcgView->getOutEdgesOf(node, outEdges);
        for (const CallGraphEdge* edge : outEdges)
        {
            std::vector<const CallICFGNode*> directCalls;
            std::vector<const CallICFGNode*> indirectCalls;
            tcgView->getDirectCallsOf(edge, directCalls);
            tcgView->getIndirectCallsOf(edge, indirectCalls);
            for (const CallICFGNode* call : directCalls)
                handleCallRelation(process, edge, call);
            for (const CallICFGNode* call : indirectCalls)
                handleCallRelation(process, edge, call);
        }
    }

    collectMultiForkedThreads();

    if (Options::TCTDotGraph())
    {
        print();
        dump("tct");
    }
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
        markRelProcs(svfun);

        const CallICFGNode* callNode = SVFUtil::cast<CallICFGNode>(forkSite);
        std::vector<const CallGraphEdge*> forkEdges;
        GenericGraphTraits<const SlicedThreadCallGraphView*>::getForkEdges(
            tcgView, callNode, forkEdges);
        for (const CallGraphEdge* edge : forkEdges)
        {
            candidateFuncSet.insert(edge->getDstNode()->getFunction());
        }
    }

    // Get kept join sites from sliced view
    std::vector<const ICFGNode*> keptJoinSites;
    getKeptJoinSites(keptJoinSites);

    for (const ICFGNode* joinSite : keptJoinSites)
    {
        const FunObjVar* svfun = joinSite->getFun();
        markRelProcs(svfun);
    }

    if(getMakredProcs().empty())
        SVFUtil::writeWrnMsg("We didn't recognize any fork site, this is single thread program?");
}

void SlicedTCT::markRelProcs(const FunObjVar* fun)
{
    const CallGraphNode* start = tcg->getCallGraphNode(fun);
    if (!isKeptNode(start))
        return;

    FIFOWorkList<const CallGraphNode*> worklist;
    PTACGNodeSet visited;
    worklist.push(start);
    visited.insert(start);
    while (!worklist.empty())
    {
        const CallGraphNode* node = worklist.pop();
        candidateFuncSet.insert(node->getFunction());
        std::vector<const CallGraphEdge*> inEdges;
        tcgView->getInEdgesOf(node, inEdges);
        for (const CallGraphEdge* edge : inEdges)
        {
            const CallGraphNode* caller = edge->getSrcNode();
            if (visited.insert(caller).second)
                worklist.push(caller);
        }
    }
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

    // A removed caller must not turn its callee into a new program root.
    // Start only from original roots that remain in the sliced view.
    const OrderedSet<const CallGraphNode*>& keptNodes = tcgView->getKeptNodes();

    for (const CallGraphNode* node : keptNodes)
    {
        const FunObjVar* fun = node->getFunction();
        if (SVFUtil::isExtCall(fun))
            continue;

        if (!node->hasIncomingEdge())
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
    return tcgView->isKeptEdge(edge);
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

        std::vector<const CallGraphEdge*> forkEdges;
        GenericGraphTraits<const SlicedThreadCallGraphView*>::getForkEdges(
            tcgView, callNode, forkEdges);
        if (!forkEdges.empty())
            out.push_back(forkSite);
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

        std::vector<const CallGraphEdge*> joinEdges;
        GenericGraphTraits<const SlicedThreadCallGraphView*>::getJoinEdges(
            tcgView, callNode, joinEdges);
        if (!joinEdges.empty())
            out.push_back(joinSite);
    }
}

//===----------------------------------------------------------------------===//
// MTASlicerBase
//===----------------------------------------------------------------------===//

MTASlicerBase::MTASlicerBase(SVFIR* svfir, AndersenBase* pta, MHP* mhp,
                             LockAnalysis* lockAnalysis, SVFG* svfg)
    : svfir(svfir), pta(pta), mhp(mhp), lockAnalysis(lockAnalysis), svfg(svfg)
{
    callGraph = pta->getCallGraph();
}

// Helper: Get lock set for an ICFG node
OrderedSet<const ICFGNode*> MTASlicerBase::getLockSet(const ICFGNode* node)
{
    OrderedSet<const ICFGNode*> allLockSites;

    // Synchronization dependence is based on may-lock spans. Retain both the
    // unconditional locks used to prove mutual exclusion and conditional locks
    // whose spans can affect the sliced lock analysis' classification.
    if (lockAnalysis->hasIntraLockSet(node))
    {
        const LockAnalysis::InstSet& intraLocks = lockAnalysis->getIntraLockSet(node);
        for (const ICFGNode* lockSite : intraLocks)
        {
            allLockSites.insert(lockSite);
        }
    }
    if (lockAnalysis->isInsideCondIntraLock(node))
    {
        const LockAnalysis::InstSet& conditionalLocks =
            lockAnalysis->getCondIntraLockSet(node);
        for (const ICFGNode* lockSite : conditionalLocks)
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
            if (lockAnalysis->hasCxtLockFromCxtStmt(cxtStmt))
            {
                const LockAnalysis::CxtLockSet& cxtLocks =
                    lockAnalysis->getCxtLockFromCxtStmt(cxtStmt);
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
OrderedSet<const TCTNode*> MTASlicerBase::getTCTNodeSetFromNode(
    const ICFGNode* node)
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

// Helper: Get dependent thread-create sites for an ICFG source node.
OrderedSet<const CallICFGNode*> MTASlicerBase::getDependentThreadCreate(
    const ICFGNode* node)
{
    OrderedSet<const CallICFGNode*> forkSites;
    OrderedSet<const TCTNode*> tctNodeSet = getTCTNodeSetFromNode(node);
    ThreadAPI* threadAPI = mhp->getThreadCallGraph()->getThreadAPI();

    TCT* tct = mhp->getTCT();
    for (const TCTNode* tctNode : tctNodeSet)
    {
        NodeBS dependentThreads = tct->getAncestorThreads(tctNode->getId());
        dependentThreads.set(tctNode->getId());
        for (NodeID tid : dependentThreads)
        {
            const ICFGNode* forkSite =
                tct->getTCTNode(tid)->getCxtThread().getThread();
            const CallICFGNode* forkCall =
                SVFUtil::dyn_cast<CallICFGNode>(forkSite);
            if (forkCall != nullptr && threadAPI->isTDFork(forkCall))
                forkSites.insert(forkCall);
        }
    }

    return forkSites;
}

// Data-dependence slice over the thread-aware SVFG (VFG_pre), at SVFG-node
// granularity: the value-flow nodes reachable backward from the seeds. The
// value-flow edges already capture direct (top-level), indirect (address-taken
// / MemSSA), and thread-aware (interference) data dependence.
OrderedSet<const SVFGNode*> MTASlicerBase::computeDataDependenceSVFGNodes(
    const OrderedSet<const SVFStmt*>& seeds, SVFG* svfg)
{

    assert(svfg != nullptr && "data-dependence slice requires the thread-aware VFG_pre");

    OrderedSet<const SVFGNode*> visited;
    std::deque<const SVFGNode*> worklist;

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
        if (svfg->hasStmtVFGNode(stmt))
            enqueueSVFGNode(svfg->getStmtVFGNode(stmt), visited, worklist);

        NodeID addrPtr = 0;
        if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
            addrPtr = load->getRHSVarID();
        else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
            addrPtr = store->getLHSVarID();
        if (addrPtr != 0)
        {
            // getDefSVFGNode takes a ValVar (the address pointer is a top-level
            // value variable).
            const ValVar* ptrNode = SVFUtil::dyn_cast<ValVar>(svfir->getGNode(addrPtr));
            if (ptrNode != nullptr && svfg->hasDefSVFGNode(ptrNode))
                enqueueSVFGNode(svfg->getDefSVFGNode(ptrNode), visited, worklist);
        }
    }

    // Backward over every value-flow edge.
    while (!worklist.empty())
    {
        const SVFGNode* node = worklist.front();
        worklist.pop_front();
        for (const VFGEdge* edge : node->getInEdges())
            enqueueSVFGNode(edge->getSrcNode(), visited, worklist);
    }

    return visited;
}

void MTASlicerBase::enqueueSVFGNode(
    const SVFGNode* node, OrderedSet<const SVFGNode*>& visited,
    std::deque<const SVFGNode*>& worklist)
{
    if (node != nullptr && visited.insert(node).second)
        worklist.push_back(node);
}

// Project retained VFG nodes (plus the seeds) onto their ICFG nodes.
OrderedSet<const ICFGNode*> MTASlicerBase::svfgNodesToICFGNodes(
    const OrderedSet<const SVFGNode*>& nodes,
    const OrderedSet<const SVFStmt*>& seeds)
{
    OrderedSet<const ICFGNode*> result;
    for (const SVFGNode* node : nodes)
        if (const StmtVFGNode* statementNode =
                SVFUtil::dyn_cast<StmtVFGNode>(node))
            if (statementNode->getICFGNode() != nullptr)
                result.insert(statementNode->getICFGNode());
    for (const SVFStmt* stmt : seeds)
        if (stmt != nullptr && stmt->getICFGNode() != nullptr)
            result.insert(stmt->getICFGNode());
    return result;
}

OrderedSet<const ICFGNode*> MTASlicerBase::sliceDataDependenceOverVFG(
    const OrderedSet<const SVFStmt*>& seeds, SVFG* svfg)
{
    return svfgNodesToICFGNodes(
        computeDataDependenceSVFGNodes(seeds, svfg), seeds);
}

// Helper: Collect pthread-related statements (create and join)
OrderedSet<const CallICFGNode*> MTASlicerBase::collectPthreadStatements(
    const OrderedSet<const ICFGNode*>& sourceNodes)
{
    OrderedSet<const CallICFGNode*> pthreadCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map pthread_create nodes to their corresponding pthread_join nodes
    OrderedSet<const CallICFGNode*> pthreadCreateNodes;

    // First pass: collect all pthread_create nodes
    for (const ICFGNode* sourceNode : sourceNodes)
    {
        OrderedSet<const CallICFGNode*> forkSites =
            getDependentThreadCreate(sourceNode);
        for (const CallICFGNode* forkCallNode : forkSites)
        {
            pthreadCallNodes.insert(forkCallNode);
            pthreadCreateNodes.insert(forkCallNode);
        }
    }

    // Second pass: find corresponding pthread_join nodes
    ICFG* icfg = svfir->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end();
         it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDJoin(callNode))
        {
            const SVFVar* joinThread = threadAPI->getJoinedThread(callNode);
            if (joinThread != nullptr)
            {
                for (const CallICFGNode* createCallNode : pthreadCreateNodes)
                {
                    const SVFVar* forkedThread = threadAPI->getForkedThread(createCallNode);
                    if (forkedThread != nullptr &&
                        threadAPI->isAliasedForkJoin(
                            pta, forkedThread, joinThread))
                    {
                        pthreadCallNodes.insert(callNode);
                    }
                }
            }
        }
    }

    return pthreadCallNodes;
}

// Helper: Collect mutex-related statements (lock and unlock)
OrderedSet<const CallICFGNode*> MTASlicerBase::collectMutexStatements(
    const OrderedSet<const ICFGNode*>& sourceNodes)
{
    OrderedSet<const CallICFGNode*> mutexCallNodes;

    ThreadCallGraph* tcg = mhp->getThreadCallGraph();
    ThreadAPI* threadAPI = tcg->getThreadAPI();

    // Map mutex_lock nodes to their corresponding mutex_unlock nodes
    OrderedSet<const CallICFGNode*> mutexLockCallNodes;

    // First pass: collect all mutex_lock nodes from lock sets
    for (const ICFGNode* sourceNode : sourceNodes)
    {
        OrderedSet<const ICFGNode*> lockSet = getLockSet(sourceNode);
        for (const ICFGNode* lockNode : lockSet)
        {
            const CallICFGNode* lockCallNode =
                SVFUtil::dyn_cast<CallICFGNode>(lockNode);
            if (lockCallNode != nullptr &&
                threadAPI->isTDAcquire(lockCallNode))
            {
                mutexCallNodes.insert(lockCallNode);
                mutexLockCallNodes.insert(lockCallNode);
            }
        }
    }

    // Second pass: find corresponding mutex_unlock nodes
    ICFG* icfg = svfir->getICFG();
    for (ICFG::iterator it = icfg->begin(), eit = icfg->end();
         it != eit; ++it)
    {
        const ICFGNode* node = it->second;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (callNode != nullptr && threadAPI->isTDRelease(callNode))
        {
            const SVFVar* unlockVar = threadAPI->getLockVal(callNode);
            if (unlockVar != nullptr)
            {
                for (const CallICFGNode* lockCallNode : mutexLockCallNodes)
                {
                    if (lockCallNode != nullptr)
                    {
                        const SVFVar* lockVar = threadAPI->getLockVal(lockCallNode);
                        if (lockVar != nullptr &&
                            pta->alias(unlockVar->getId(), lockVar->getId()))
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
MTASlicerBase::collectCommonThreadStatements(
    const OrderedSet<const ICFGNode*>& sourceNodes)
{
    // Step 1: Collect pthread-related statements, i.e., pthread_create and pthread_join
    OrderedSet<const CallICFGNode*> pthreadCallNodes =
        collectPthreadStatements(sourceNodes);

    // Step 2: Collect mutex-related statements
    OrderedSet<const CallICFGNode*> mutexCallNodes =
        collectMutexStatements(sourceNodes);

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
void MTASlicerBase::addSynchronizationDependencies(
    const OrderedSet<const CallICFGNode*>& pthreadCallNodes,
    const OrderedSet<const CallICFGNode*>& mutexCallNodes,
    OrderedSet<const ICFGNode*>& sliceResult)
{
    for (const CallICFGNode* callNode : pthreadCallNodes)
    {
        sliceResult.insert(callNode);
        if (callNode->getRetICFGNode() != nullptr)
            sliceResult.insert(callNode->getRetICFGNode());
    }
    for (const CallICFGNode* callNode : mutexCallNodes)
    {
        sliceResult.insert(callNode);
        if (callNode->getRetICFGNode() != nullptr)
            sliceResult.insert(callNode->getRetICFGNode());
    }

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

    ThreadAPI* threadAPI = mhp->getThreadCallGraph()->getThreadAPI();
    for (const CallICFGNode* callNode : pthreadCallNodes)
    {
        if (!threadAPI->isTDJoin(callNode) || callNode->getBB() == nullptr)
            continue;
        std::vector<const SVFBasicBlock*> exitBlocks;
        callNode->getFun()->getExitBlocksOfLoop(callNode->getBB(), exitBlocks);
        for (const SVFBasicBlock* exitBlock : exitBlocks)
            if (!exitBlock->getICFGNodeList().empty())
                sliceResult.insert(exitBlock->front());
    }
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
    std::queue<const FunObjVar*> functionWorklist;
    for (const FunObjVar* fun : keptFunctions)
        functionWorklist.push(fun);

    Map<const FunObjVar*, const CallGraphNode*> functionToNode;
    for (auto it = callGraph->begin(), eit = callGraph->end();
         it != eit; ++it)
    {
        const CallGraphNode* node = it->second;
        if (node != nullptr && node->getFunction() != nullptr)
            functionToNode[node->getFunction()] = node;
    }

    OrderedSet<const FunObjVar*> visitedFunctions = keptFunctions;
    while (!functionWorklist.empty())
    {
        const FunObjVar* target = functionWorklist.front();
        functionWorklist.pop();
        const auto nodeIt = functionToNode.find(target);
        if (nodeIt == functionToNode.end())
            continue;

        const CallGraphNode* node = nodeIt->second;
        for (const CallGraphEdge* inEdge : node->getInEdges())
        {
            if (inEdge == nullptr)
                continue;
            const CallGraphNode* callerNode = inEdge->getSrcNode();
            if (callerNode != nullptr && callerNode->getFunction() != nullptr)
            {
                const FunObjVar* callerFun = callerNode->getFunction();
                if (visitedFunctions.find(callerFun) == visitedFunctions.end())
                {
                    keptFunctions.insert(callerFun);
                    visitedFunctions.insert(callerFun);
                    functionWorklist.push(callerFun);
                }
            }
        }
    }

    // For each keptFunction, add call/ret nodes and entry/exit nodes
    ICFG* icfg = svfir->getICFG();
    OrderedSet<const ICFGNode*> expandedNodes = nodes;
    for (const FunObjVar* fun : keptFunctions)
    {
        if (fun == nullptr)
            continue;

        // Add function entry/exit nodes
        if (fun->hasBasicBlock())
        {
            if (FunEntryICFGNode* entry = icfg->getFunEntryICFGNode(fun))
                expandedNodes.insert(entry);
            if (FunExitICFGNode* exit = icfg->getFunExitICFGNode(fun))
                expandedNodes.insert(exit);
        }

        // Find all call/ret nodes that call this function
        const auto funNodeIt = functionToNode.find(fun);
        if (funNodeIt != functionToNode.end())
        {
            const CallGraphNode* calleeNode = funNodeIt->second;

            // Traverse all edges that call this function
            for (const CallGraphEdge* inEdge : calleeNode->getInEdges())
            {
                if (inEdge == nullptr)
                    continue;

                const CallGraphEdge::CallInstSet& directCalls =
                    inEdge->getDirectCalls();
                const CallGraphEdge::CallInstSet& indirectCalls =
                    inEdge->getIndirectCalls();

                for (const CallICFGNode* callNode : directCalls)
                {
                    if (callNode != nullptr)
                    {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr)
                            expandedNodes.insert(retNode);
                    }
                }

                for (const CallICFGNode* callNode : indirectCalls)
                {
                    if (callNode != nullptr)
                    {
                        expandedNodes.insert(callNode);
                        const RetICFGNode* retNode = callNode->getRetICFGNode();
                        if (retNode != nullptr)
                            expandedNodes.insert(retNode);
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

MultiStageSlicer::MultiStageSlicer(SVFIR* svfir, AndersenBase* pta, MHP* mhp,
                                   LockAnalysis* lockAnalysis, SVFG* svfg)
    : MTASlicerBase(svfir, pta, mhp, lockAnalysis, svfg)
{
}

// Perform slicing for MTA (includes function expansion for IRView)
OrderedSet<const ICFGNode*> MultiStageSlicer::runILASlicing(
    const OrderedSet<const SVFStmt*>& vulnerableStatements,
    const OrderedSet<const ICFGNode*>& threadVFSources)
{

    // Step 1: Form the complete ILA source set first. MSli section 4.2 defines
    // V_ILA as [INIT] union [THREAD-VF], then closes every source over its
    // synchronization dependences. Keep this set at ICFG granularity: some
    // THREAD-VF call/marker nodes have no attached SVF statement.
    OrderedSet<const ICFGNode*> ilaSourceNodes = threadVFSources;
    for (const SVFStmt* stmt : vulnerableStatements)
        ilaSourceNodes.insert(stmt->getICFGNode());

    // Step 2: synchronization-dependence closure of the complete source set.
    auto commonStmts = collectCommonThreadStatements(ilaSourceNodes);
    const OrderedSet<const CallICFGNode*>& pthreadCallNodes = commonStmts.first;
    const OrderedSet<const CallICFGNode*>& mutexCallNodes = commonStmts.second;

    // Form V_ILA_sync before function expansion: all [INIT] and [THREAD-VF]
    // sources plus the synchronization primitives on which they depend.
    OrderedSet<const ICFGNode*> initialSliceResult;
    addSynchronizationDependencies(
        pthreadCallNodes, mutexCallNodes, initialSliceResult);
    for (const SVFStmt* stmt : vulnerableStatements)
    {
        initialSliceResult.insert(stmt->getICFGNode());
    }
    initialSliceResult.insert(threadVFSources.begin(), threadVFSources.end());

    // Step 3: Expand keptNodes to include call/ret nodes and function entry/exit
    // nodes (call dependence).
    OrderedSet<const ICFGNode*> finalSlice = expandCallDependence(initialSliceResult);

    // Slicing invariant: no relevant synchronization primitive may be contracted
    // into a bridge edge. Return nodes are retained because the sliced ICFG
    // represents external synchronization calls as paired call/return nodes.
    for (const CallICFGNode* callNode : pthreadCallNodes)
    {
        assert(finalSlice.count(callNode) && finalSlice.count(callNode->getRetICFGNode()) &&
               "ILA slice dropped a fork/join synchronization dependence");
        (void)callNode;
    }
    for (const CallICFGNode* callNode : mutexCallNodes)
    {
        assert(finalSlice.count(callNode) && finalSlice.count(callNode->getRetICFGNode()) &&
               "ILA slice dropped a lock/unlock synchronization dependence");
        (void)callNode;
    }
    return finalSlice;
}

//===----------------------------------------------------------------------===//
// MultiStageSlicer -- stage 2 (FSPTA data-dependence slice)
//===----------------------------------------------------------------------===//

// Candidate value-flow slice over VFG_pre. It is used only to select the
// THREAD-VF queries and to scope the refined main overlay; it is not reused as
// the final FSPTA slice.
const ValueFlowSlice& MultiStageSlicer::getPreCandidateSlice(
    const OrderedSet<const SVFStmt*>& vulnerableStatements)
{
    if (!preCandidateComputed || preCandidateSeeds != vulnerableStatements)
    {
        preCandidateSlice = ValueFlowSlice{};
        preCandidateSeeds = vulnerableStatements;
        preCandidateSlice.svfgNodes =
            computeDataDependenceSVFGNodes(vulnerableStatements, svfg);
        preCandidateSlice.icfgNodes =
            svfgNodesToICFGNodes(preCandidateSlice.svfgNodes, vulnerableStatements);
        preCandidateComputed = true;
    }
    return preCandidateSlice;
}

// Final FSPTA data-dependence slice over the refined main value-flow graph.
ValueFlowSlice MultiStageSlicer::runPTASlicing(
    const OrderedSet<const SVFStmt*>& vulnerableStatements,
    SVFG* refinedMainVFG)
{
    ValueFlowSlice result;
    result.svfgNodes =
        computeDataDependenceSVFGNodes(vulnerableStatements, refinedMainVFG);
    result.icfgNodes =
        svfgNodesToICFGNodes(result.svfgNodes, vulnerableStatements);
    return result;
}

//===----------------------------------------------------------------------===//
// SingleSlicer
//===----------------------------------------------------------------------===//

SingleSlicer::SingleSlicer(SVFIR* svfir, AndersenBase* pta, MHP* mhp,
                           LockAnalysis* lockAnalysis, SVFG* svfg)
    : MTASlicerBase(svfir, pta, mhp, lockAnalysis, svfg)
{
}

// Single-pass slice (the baseline of MSli §3/§5.4): the transitive closure of
// the target statements under the COMBINED dependence graph -- synchronization,
// data, and call dependence -- yielding one slice shared by ILA and FSPTA.
ValueFlowSlice SingleSlicer::runSlicing(
    const OrderedSet<const SVFStmt*>& vulnerableStatements)
{

    OrderedSet<const ICFGNode*> sourceNodes;
    for (const SVFStmt* stmt : vulnerableStatements)
        if (stmt != nullptr && stmt->getICFGNode() != nullptr)
            sourceNodes.insert(stmt->getICFGNode());

    OrderedSet<const ICFGNode*> currentNodes = sourceNodes;
    OrderedSet<const SVFGNode*> dataSliceNodes;

    // Step 3: Close over data dependence (the thread-aware VFG_pre value flow --
    // direct + indirect + interference, the same model the FSPTA stage uses) and call
    // dependence (function expansion), alternately, until the node set converges.
    while (true)
    {
        OrderedSet<const ICFGNode*> previousNodes = currentNodes;

        auto commonStmts = collectCommonThreadStatements(currentNodes);
        addSynchronizationDependencies(
            commonStmts.first, commonStmts.second, currentNodes);

        OrderedSet<const SVFStmt*> currentStatements;
        for (const ICFGNode* node : currentNodes)
        {
            const ICFGNode::SVFStmtList& stmts = node->getSVFStmts();
            currentStatements.insert(stmts.begin(), stmts.end());
        }

        dataSliceNodes =
            computeDataDependenceSVFGNodes(currentStatements, svfg);
        OrderedSet<const ICFGNode*> dataDepNodes =
            svfgNodesToICFGNodes(dataSliceNodes, currentStatements);
        currentNodes.insert(dataDepNodes.begin(), dataDepNodes.end());

        currentNodes = expandCallDependence(currentNodes);

        if (currentNodes == previousNodes)
            break;
    }
    ValueFlowSlice result;
    result.svfgNodes = std::move(dataSliceNodes);
    result.icfgNodes = std::move(currentNodes);
    return result;
}

} // End namespace SVF
