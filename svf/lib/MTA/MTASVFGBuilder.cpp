//===- MTASVFGBuilder.cpp -- Thread-aware SVFG builder for FSAM ---------===//
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
 * MTASVFGBuilder.cpp
 *
 *      Author: Jiawei Yang
 */

#include "MTA/MTASVFGBuilder.h"
#include "Graphs/SlicedGraphs.h"
#include "MSSA/MemSSA.h"
#include "MSSA/MemPartition.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Util/SVFUtil.h"
#include "Util/Options.h"
#include "Graphs/ThreadCallGraph.h"

using namespace SVF;
using namespace SVFUtil;

namespace
{
/// Thread-aware MRGenerator mixin: layers the FSAM fork/join mod-ref effects on
/// top of any base partition strategy (Distinct / IntraDisjoint / InterDisjoint).
/// This must happen during MemorySSA mod/ref construction because callsite
/// MU/CHI nodes, and therefore ActualIN/ActualOUT SVFG nodes, are created from
/// these mod/ref sets.
template <class BaseMRG>
class ThreadMRG : public BaseMRG
{
public:
    ThreadMRG(BVDataPTAImpl* pta, bool pointerOnly)
        : BaseMRG(pta, pointerOnly)
    {
    }

protected:
    /// Thread fork as a call WITHOUT a return: forward the spawnee's *ref* set to
    /// the fork site (ActualIN -> FormalIN, FSAM's thread-oblivious value flow).
    /// We do NOT add the spawnee's *mod* set -- a fork has no return, so its
    /// writes flow back via the thread-aware interference edges instead.
    void refineCallsiteModRef(NodeBS& mod, NodeBS& ref,
                              const CallICFGNode* cs, const FunObjVar* callee) override
    {
        if (const ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(this->getCallGraph()))
            if (tcg->hasThreadForkEdge(cs))
            {
                ref = this->getRefSideEffectOfFunction(callee);
                // A fork is a call WITHOUT a return: the spawnee's writes must not
                // be applied as a mod here (which would kill the spawner's own
                // value flow past the fork). They reach later reads via the
                // thread-aware interference edges instead.
                mod.clear();
            }
    }

    /// Thread join: the joined spawnee's exit writes become visible to the
    /// spawner. For each site that joins this start routine, add the spawnee's
    /// MOD set to the join callsite (creating an ActualOUT there -- a "return
    /// without a forward"). MOD only, never REF. Join edges are not real
    /// call-graph edges, so we reach them via getJoinSites.
    void propagateAdditionalModRef(CallGraphNode* callGraphNode,
                                   MRGenerator::WorkList& worklist) override
    {
        ThreadCallGraph* tcg = SVFUtil::dyn_cast<ThreadCallGraph>(this->getCallGraph());
        if (tcg == nullptr)
            return;
        ThreadCallGraph::InstSet joinSites;
        tcg->getJoinSites(callGraphNode, joinSites);
        if (joinSites.empty())
            return;
        const NodeBS& spawneeMod = this->getModSideEffectOfFunction(callGraphNode->getFunction());
        for (const CallICFGNode* callSite : joinSites)
            // A join exposes the joined thread's writes at the join point. Those
            // writes are not necessarily reachable from pthread_join's handle
            // argument, so use the MOD set as computed for the start routine.
            if (this->addUnfilteredModSideEffectOfCallSite(callSite, spawneeMod))
                worklist.push(this->getCallGraph()->getCallGraphNode(
                                  callSite->getCaller())->getId());
    }
};
} // anonymous namespace

// Build a thread-aware MRGenerator wrapping the configured partition strategy, so
// the MemSSA mod-ref generation carries the FSAM fork/join side effects.
std::unique_ptr<MRGenerator> MTASVFGBuilder::createMRGenerator(BVDataPTAImpl* pta, bool ptrOnlyMSSA)
{
    switch (Options::MemPar())
    {
    case MemSSA::MemPartition::Distinct:
        return std::make_unique<ThreadMRG<DistinctMRG>>(pta, ptrOnlyMSSA);
    case MemSSA::MemPartition::IntraDisjoint:
        return std::make_unique<ThreadMRG<IntraDisjointMRG>>(pta, ptrOnlyMSSA);
    case MemSSA::MemPartition::InterDisjoint:
        return std::make_unique<ThreadMRG<InterDisjointMRG>>(pta, ptrOnlyMSSA);
    default:
        assert(false && "unrecognised memory partition strategy");
        return nullptr;
    }
}

/*!
 * Build the stock (thread-oblivious) SVFG, add the FSAM join-related def-use
 * edges (relocated out of core SVFG.cpp), then add thread-aware MHP edges.
 */
void MTASVFGBuilder::buildSVFG()
{
    svfg->buildSVFG();
    connectThreadJoinEdges();
    connectMHPEdges(svfg->getMSSA()->getPTA());
}

void MTASVFGBuilder::clearThreadAwareOverlay()
{
    for (SVFGEdge* edge : threadAwareEdges)
        svfg->removeSVFGEdge(edge);
    threadAwareEdges.clear();
}

void MTASVFGBuilder::replaceThreadAwareOverlay(
    MHP* mainMHP, LockAnalysis* mainLockAnalysis,
    const ThreadVFBuildConfig& config)
{
    assert(svfg != nullptr && "base SVFG must be built before replacing its overlay");
    assert(mainMHP != nullptr && mainLockAnalysis != nullptr &&
           "thread-aware overlay requires main ILA results");

    clearThreadAwareOverlay();
    mhp = mainMHP;
    lockAnalysis = mainLockAnalysis;
    overlayScope = config.scope;
    overlayCandidates = config.candidates;
    recordThreadVFQueries = false;
    labelInterferenceEdges = true;

    storeNodes.clear();
    loadNodes.clear();
    threadVFQueryMap.clear();
    predecessorCache.clear();
    successorCache.clear();
    spanHeadCache.clear();
    spanTailCache.clear();

    connectMHPEdges(svfg->getMSSA()->getPTA());
    overlayScope = nullptr;
    overlayCandidates = nullptr;
}

/*!
 * Mirror of SVFG::addInterIndirectVFRetEdge over the public SVFG API, so the
 * join edges can be added from the builder without modifying core SVFG.
 */
void MTASVFGBuilder::addJoinRetEdge(const FormalOUTSVFGNode* formalOut,
                                    const ActualOUTSVFGNode* actualOut,
                                    CallSiteID callSiteId)
{
    NodeBS cpts = formalOut->getPointsTo();
    const NodeBS& dpts = actualOut->getPointsTo();
    if (!cpts.intersects(dpts))
        return;
    cpts &= dpts;

    SVFGNode* src = svfg->getSVFGNode(formalOut->getId());
    SVFGNode* dst = svfg->getSVFGNode(actualOut->getId());
    if (SVFGEdge* edge = svfg->hasInterVFGEdge(
                             src, dst, SVFGEdge::RetIndVF, callSiteId))
    {
        SVFUtil::cast<RetIndSVFGEdge>(edge)->addPointsTo(cpts);
    }
    else
    {
        RetIndSVFGEdge* retEdge = new RetIndSVFGEdge(src, dst, callSiteId);
        retEdge->addPointsTo(cpts);
        svfg->addSVFGEdge(retEdge);
    }
}

/*!
 * FSAM join-related def-use (the "return" half of treating a join as a call
 * without a forward): for every FormalOUT (a start routine's exit def), connect
 * it to the ActualOUT at each site that joins that routine. Relocated here from
 * core SVFG::connectIndirectSVFGEdges so the stock SVFG stays unmodified.
 */
void MTASVFGBuilder::connectThreadJoinEdges()
{
    ThreadCallGraph* tcg =
        SVFUtil::dyn_cast<ThreadCallGraph>(svfg->getMSSA()->getPTA()->getCallGraph());
    if (tcg == nullptr)
        return;

    MemSSA* mssa = svfg->getMSSA();
    for (SVFG::const_iterator it = svfg->begin(), eit = svfg->end(); it != eit; ++it)
    {
        const FormalOUTSVFGNode* formalOut = SVFUtil::dyn_cast<FormalOUTSVFGNode>(it->second);
        if (formalOut == nullptr)
            continue;

        ThreadCallGraph::InstSet joinSites;
        tcg->getJoinSites(tcg->getCallGraphNode(formalOut->getFun()), joinSites);
        for (const CallICFGNode* callSite : joinSites)
        {
            if (!mssa->hasCHI(callSite))
                continue;
            SVFG::ActualOUTSVFGNodeSet& actualOuts =
                svfg->getActualOUTSVFGNodes(callSite);
            for (NodeID actualOutId : actualOuts)
            {
                const ActualOUTSVFGNode* actualOut =
                    SVFUtil::cast<ActualOUTSVFGNode>(
                        svfg->getSVFGNode(actualOutId));
                addJoinRetEdge(
                    formalOut, actualOut,
                    svfg->getCallSiteID(callSite, formalOut->getFun()));
            }
        }
    }
}

/*!
 * Collect all store/load SVFG nodes.
 */
void MTASVFGBuilder::collectLoadStoreSVFGNodes()
{
    for (SVFG::const_iterator it = svfg->begin(), eit = svfg->end(); it != eit; ++it)
    {
        const SVFGNode* svfgNode = it->second;
        const bool isLoad = SVFUtil::isa<LoadSVFGNode>(svfgNode);
        if (!isLoad && !SVFUtil::isa<StoreSVFGNode>(svfgNode))
            continue;
        const StmtSVFGNode* node = SVFUtil::cast<StmtSVFGNode>(svfgNode);
        if (node->getICFGNode() == nullptr)
            continue;
        if (!isInOverlayScope(node))
            continue;
        if (isLoad)
            loadNodes.insert(node);
        else
            storeNodes.insert(node);
    }
}

bool MTASVFGBuilder::isInOverlayScope(const SVFGNode* node) const
{
    return overlayScope == nullptr || overlayScope->isKeptNode(node);
}

/*!
 * Add (or merge into) a thread-MHP indirect value-flow edge src -> dst.
 */
SVFGEdge* MTASVFGBuilder::addTDEdge(NodeID srcId, NodeID dstId, const PointsTo& pts)
{
    SVFGNode* srcNode = svfg->getSVFGNode(srcId);
    SVFGNode* dstNode = svfg->getSVFGNode(dstId);
    assert(isInOverlayScope(srcNode) && isInOverlayScope(dstNode) &&
           "thread-aware overlay edge escaped its construction scope");

    // VFG_pre (sliced-only) mode: keep the edge for connectivity but omit its
    // points-to label -- no slice consumer reads it.
    if (!labelInterferenceEdges)
    {
        if (SVFGEdge* edge = svfg->hasThreadVFGEdge(srcNode, dstNode, SVFGEdge::TheadMHPIndirectVF))
            return edge;
        ThreadMHPIndSVFGEdge* indirectEdge = new ThreadMHPIndSVFGEdge(srcNode, dstNode);
        if (svfg->addSVFGEdge(indirectEdge))
        {
            threadAwareEdges.insert(indirectEdge);
            return indirectEdge;
        }
        return nullptr;
    }

    if (SVFGEdge* edge = svfg->hasThreadVFGEdge(srcNode, dstNode, SVFGEdge::TheadMHPIndirectVF))
    {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "should be an indirect value-flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(pts.toNodeBS()) ? edge : nullptr);
    }
    else
    {
        ThreadMHPIndSVFGEdge* indirectEdge = new ThreadMHPIndSVFGEdge(srcNode, dstNode);
        indirectEdge->addPointsTo(pts.toNodeBS());
        if (svfg->addSVFGEdge(indirectEdge))
        {
            threadAwareEdges.insert(indirectEdge);
            return indirectEdge;
        }
        return nullptr;
    }
}

/*!
 * Backward reachable store SVFG nodes via indirect value flow (lock-span head test).
 */
MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getPredecessorNodes(
    const StmtSVFGNode* node)
{
    const auto found = predecessorCache.find(node);
    if (found != predecessorCache.end())
        return found->second;

    SVFGNodeIDSet predecessors;
    Set<const SVFGNode*> worklist;
    Set<const SVFGNode*> visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->InEdgeBegin();
            iter != node->InEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge() && !edge->isThreadMHPIndirectVFGEdge() &&
                isInOverlayScope(edge->getSrcNode()))
            worklist.insert(edge->getSrcNode());
    }

    while (!worklist.empty())
    {
        const SVFGNode* node = *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode>(node))
            predecessors.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->InEdgeBegin(); iter != node->InEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() &&
                        !edge->isThreadMHPIndirectVFGEdge() &&
                        isInOverlayScope(edge->getSrcNode()) &&
                        visited.find(edge->getSrcNode()) == visited.end())
                    worklist.insert(edge->getSrcNode());
            }
        }
    }
    predecessorCache[node] = predecessors;
    return predecessors;
}

/*!
 * Forward reachable store/load SVFG nodes via indirect value flow (lock-span tail test).
 */
MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getSuccessorNodes(
    const StmtSVFGNode* node)
{
    const auto found = successorCache.find(node);
    if (found != successorCache.end())
        return found->second;

    SVFGNodeIDSet successors;
    Set<const SVFGNode*> worklist;
    Set<const SVFGNode*> visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->OutEdgeBegin();
            iter != node->OutEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge() && !edge->isThreadMHPIndirectVFGEdge() &&
                isInOverlayScope(edge->getDstNode()))
            worklist.insert(edge->getDstNode());
    }

    while (!worklist.empty())
    {
        const SVFGNode* node = *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(node))
            successors.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->OutEdgeBegin(); iter != node->OutEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() &&
                        !edge->isThreadMHPIndirectVFGEdge() &&
                        isInOverlayScope(edge->getDstNode()) &&
                        visited.find(edge->getDstNode()) == visited.end())
                    worklist.insert(edge->getDstNode());
            }
        }
    }
    successorCache[node] = successors;
    return successors;
}

/*!
 * Whether, for all lock spans n belongs to, n is the first write (span head).
 */
bool MTASVFGBuilder::isHeadOfSpan(const StmtSVFGNode* node)
{
    const auto found = spanHeadCache.find(node);
    if (found != spanHeadCache.end())
        return found->second;

    const SVFGNodeIDSet predecessors = getPredecessorNodes(node);
    for (NodeID id : predecessors)
    {
        const StmtSVFGNode* prevNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(id));
        if (prevNode != nullptr && lockAnalysis->isInSameSpan(
                    prevNode->getICFGNode(), node->getICFGNode()))
        {
            spanHeadCache[node] = false;
            return false;
        }
    }
    spanHeadCache[node] = true;
    return true;
}

/*!
 * Whether, for all lock spans n belongs to, n is the last write (span tail).
 */
bool MTASVFGBuilder::isTailOfSpan(const StmtSVFGNode* node)
{
    assert(SVFUtil::isa<StoreSVFGNode>(node) &&
           "tail test only for store nodes");

    const auto found = spanTailCache.find(node);
    if (found != spanTailCache.end())
        return found->second;

    const SVFGNodeIDSet successors = getSuccessorNodes(node);
    for (NodeID id : successors)
    {
        const SVFGNode* successor = svfg->getSVFGNode(id);
        if (SVFUtil::isa<LoadSVFGNode>(successor))
            continue;
        const StmtSVFGNode* successorStatement =
            SVFUtil::dyn_cast<StmtSVFGNode>(successor);
        if (successorStatement != nullptr && lockAnalysis->isInSameSpan(
                    successorStatement->getICFGNode(), node->getICFGNode()))
        {
            spanTailCache[node] = false;
            return false;
        }
    }
    spanTailCache[node] = true;
    return true;
}

/*!
 * Record the per-edge [THREAD-VF] query for one candidate pair s --o--> s' (see
 * getThreadVFQueryMap for the rule): the endpoints, plus -- under a common lock --
 * the in-span Succ_spl(s) / Pred_spl'(s') witnesses. Enumerated fully (the
 * tail/head boolean tests short-circuit; source extraction must not).
 */
void MTASVFGBuilder::recordThreadVFSource(
    const StmtSVFGNode* source, const StmtSVFGNode* destination,
    bool commonLock)
{
    // Per-edge Query set. The endpoints are NOT duplicated into the value --
    // they are recoverable from the map key -- so the value holds only the
    // additional in-span witnesses below (empty for the common lock-free case).
    Set<const ICFGNode*>& query =
        threadVFQueryMap[ {source, destination}];

    if (!commonLock)
        return;

    // Succ_spl(s) = { x in s's span | x is a store, s --o--> x }.
    for (NodeID id : getSuccessorNodes(source))
    {
        const SVFGNode* successor = svfg->getSVFGNode(id);
        if (!SVFUtil::isa<StoreSVFGNode>(successor))
            continue;
        const StmtSVFGNode* successorStatement =
            SVFUtil::cast<StmtSVFGNode>(successor);
        if (lockAnalysis->isInSameSpan(
                    successorStatement->getICFGNode(), source->getICFGNode()))
            query.insert(successorStatement->getICFGNode());
    }

    // Pred_spl'(s') = { x in s' span | x --o--> s' }.
    for (NodeID id : getPredecessorNodes(destination))
    {
        const StmtSVFGNode* prevNode =
            SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(id));
        if (prevNode != nullptr && lockAnalysis->isInSameSpan(
                    prevNode->getICFGNode(), destination->getICFGNode()))
            query.insert(prevNode->getICFGNode());
    }
}

/*!
 * Store -> Load interference: add a thread-aware def-use edge if the store may
 * happen in parallel with and may alias the load, unless excluded by a common
 * lock (then only when the store is a span tail and the load a span head).
 */
void MTASVFGBuilder::handleStoreLoad(
    const StmtSVFGNode* store, const StmtSVFGNode* load,
    PointerAnalysis* pta)
{
    const ICFGNode* storeNode = store->getICFGNode();
    const ICFGNode* loadNode = load->getICFGNode();

    const bool mayParallel = mhp->mayHappenInParallel(storeNode, loadNode);
    if (!mayParallel)
        return;

    // No alias() re-check: the bucketed candidate generator only pairs accesses
    // whose raw points-to sets share an object, so the intersection below is
    // non-empty by construction and alias() could never answer NoAlias here.
    // The label is only needed when the edge will be solved (main FSMPTA); in
    // VFG_pre (sliced-only) mode skip the intersection -- it is never read.
    PointsTo pts;
    if (labelInterferenceEdges)
    {
        pts = pta->getPts(store->getDstNodeID());
        pts &= pta->getPts(load->getSrcNodeID());
    }

    // [THREAD-VF] source extraction runs for every candidate pair (both the
    // pairs that survive and the ones the lock test prunes), so the sliced ILA
    // can re-derive whether the edge holds.
    const bool commonLock =
        lockAnalysis->isProtectedByCommonLock(storeNode, loadNode);
    if (recordThreadVFQueries)
        recordThreadVFSource(store, load, commonLock);

    if (commonLock)
    {
        if (isTailOfSpan(store) && isHeadOfSpan(load))
            addTDEdge(store->getId(), load->getId(), pts);
    }
    else
    {
        addTDEdge(store->getId(), load->getId(), pts);
    }
}

/*!
 * Store -> Store interference (symmetric): add thread-aware def-use edges in
 * both directions, with the same lock-span pruning as store/load.
 */
void MTASVFGBuilder::handleStoreStore(
    const StmtSVFGNode* firstStore, const StmtSVFGNode* secondStore,
    PointerAnalysis* pta)
{
    const ICFGNode* firstNode = firstStore->getICFGNode();
    const ICFGNode* secondNode = secondStore->getICFGNode();

    const bool mayParallel = mhp->mayHappenInParallel(firstNode, secondNode);
    if (!mayParallel)
        return;

    // No alias() re-check: see handleStoreLoad -- bucketing already guarantees a
    // shared raw object. Skip the label intersection in VFG_pre (sliced-only) mode.
    PointsTo pts;
    if (labelInterferenceEdges)
    {
        pts = pta->getPts(firstStore->getDstNodeID());
        pts &= pta->getPts(secondStore->getDstNodeID());
    }

    // Both directions are candidate thread-aware edges; extract sources for each.
    const bool commonLock =
        lockAnalysis->isProtectedByCommonLock(firstNode, secondNode);
    if (recordThreadVFQueries)
    {
        recordThreadVFSource(firstStore, secondStore, commonLock);
        recordThreadVFSource(secondStore, firstStore, commonLock);
    }

    if (commonLock)
    {
        if (isTailOfSpan(firstStore) && isHeadOfSpan(secondStore))
            addTDEdge(firstStore->getId(), secondStore->getId(), pts);
        if (isTailOfSpan(secondStore) && isHeadOfSpan(firstStore))
            addTDEdge(secondStore->getId(), firstStore->getId(), pts);
    }
    else
    {
        addTDEdge(firstStore->getId(), secondStore->getId(), pts);
        addTDEdge(secondStore->getId(), firstStore->getId(), pts);
    }
}

/*!
 * For every MHP store/load and store/store pair, add the thread-aware
 * (interference) value-flow edges.
 */
void MTASVFGBuilder::connectMHPEdges(PointerAnalysis* pta)
{
    if (overlayCandidates != nullptr)
    {
        // The pre-analysis candidate set is a conservative directed universe.
        // Traverse it directly so main overlay construction is proportional to
        // retained candidates instead of re-enumerating every alias pair.
        OrderedSet<ThreadVFCandidate> processedStorePairs;
        for (const ThreadVFCandidate& candidate : *overlayCandidates)
        {
            const StmtSVFGNode* src = SVFUtil::dyn_cast<StmtSVFGNode>(
                                          svfg->getSVFGNode(
                                              candidate.sourceNodeId));
            const StmtSVFGNode* dst = SVFUtil::dyn_cast<StmtSVFGNode>(
                                          svfg->getSVFGNode(
                                              candidate.destinationNodeId));
            assert(src != nullptr && dst != nullptr &&
                   "thread-aware candidates must reference statement nodes");
            assert(SVFUtil::isa<StoreSVFGNode>(src) &&
                   "thread-aware candidate source must be a store");
            assert(isInOverlayScope(src) && isInOverlayScope(dst) &&
                   "thread-aware candidate escaped its selected scope");

            if (SVFUtil::isa<LoadSVFGNode>(dst))
                handleStoreLoad(src, dst, pta);
            else if (SVFUtil::isa<StoreSVFGNode>(dst))
            {
                const ThreadVFCandidate canonicalPair =
                    candidate.sourceNodeId < candidate.destinationNodeId
                    ? candidate
                    : ThreadVFCandidate(candidate.destinationNodeId,
                                        candidate.sourceNodeId);
                if (processedStorePairs.insert(canonicalPair).second)
                    // Main ILA re-decides the unordered pair once; the handler
                    // emits whichever directed edges pass the lock-span rules.
                    handleStoreStore(src, dst, pta);
            }
        }
        return;
    }

    collectLoadStoreSVFGNodes();

    // Inverted access index (object -> access-node bitset): unioning the
    // bitsets per store visits each may-alias pair exactly once, no dedup tables.
    Map<NodeID, SVFGNodeIDSet> objectToStoreIds;
    Map<NodeID, SVFGNodeIDSet> objectToLoadIds;
    for (const StmtSVFGNode* store : storeNodes)
        for (NodeID objectId : pta->getPts(store->getDstNodeID()))
            objectToStoreIds[objectId].set(store->getId());
    for (const StmtSVFGNode* load : loadNodes)
        for (NodeID objectId : pta->getPts(load->getSrcNodeID()))
            objectToLoadIds[objectId].set(load->getId());

    for (const StmtSVFGNode* store : storeNodes)
    {
        SVFGNodeIDSet candidateLoads;
        SVFGNodeIDSet candidateStores;
        for (NodeID objectId : pta->getPts(store->getDstNodeID()))
        {
            candidateStores |= objectToStoreIds[objectId];
            const auto loads = objectToLoadIds.find(objectId);
            if (loads != objectToLoadIds.end())
                candidateLoads |= loads->second;
        }

        for (NodeID loadId : candidateLoads)
        {
            const StmtSVFGNode* load =
                SVFUtil::cast<StmtSVFGNode>(svfg->getSVFGNode(loadId));
            handleStoreLoad(store, load, pta);
        }

        // Visit each unordered store pair once: only partners with a larger id.
        const NodeID storeId = store->getId();
        for (NodeID otherId : candidateStores)
        {
            if (otherId <= storeId)
                continue;
            const StmtSVFGNode* other =
                SVFUtil::cast<StmtSVFGNode>(svfg->getSVFGNode(otherId));
            handleStoreStore(store, other, pta);
        }
    }
}
