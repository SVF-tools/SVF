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
 *
 * Implements the FSAM thread-aware value-flow construction (CGO'16, §3.3).
 */

#include "MTA/MTASVFGBuilder.h"
#include "MSSA/MemSSA.h"
#include "MSSA/MemPartition.h"
#include "MemoryModel/PointerAnalysisImpl.h"
#include "Util/SVFUtil.h"
#include "Util/Options.h"
#include "Graphs/ThreadCallGraph.h"

using namespace SVF;
using namespace SVFUtil;

u32_t MTASVFGBuilder::numOfNewSVFGEdges = 0;

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
    ThreadMRG(BVDataPTAImpl* p, bool ptrOnly) : BaseMRG(p, ptrOnly) {}

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
        ThreadCallGraph::InstSet joinsites;
        tcg->getJoinSites(callGraphNode, joinsites);
        if (joinsites.empty())
            return;
        const NodeBS& spawneeMod = this->getModSideEffectOfFunction(callGraphNode->getFunction());
        for (const CallICFGNode* cs : joinsites)
            // A join exposes the joined thread's writes at the join point. Those
            // writes are not necessarily reachable from pthread_join's handle
            // argument, so use the MOD set as computed for the start routine.
            if (this->addUnfilteredModSideEffectOfCallSite(cs, spawneeMod))
                worklist.push(this->getCallGraph()->getCallGraphNode(cs->getCaller())->getId());
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

/*!
 * Mirror of SVFG::addInterIndirectVFRetEdge over the public SVFG API, so the
 * join edges can be added from the builder without modifying core SVFG.
 */
void MTASVFGBuilder::addJoinRetEdge(const FormalOUTSVFGNode* formalOut,
                                    const ActualOUTSVFGNode* actualOut, CallSiteID csId)
{
    NodeBS cpts = formalOut->getPointsTo();
    const NodeBS& dpts = actualOut->getPointsTo();
    if (!cpts.intersects(dpts))
        return;
    cpts &= dpts;

    SVFGNode* src = svfg->getSVFGNode(formalOut->getId());
    SVFGNode* dst = svfg->getSVFGNode(actualOut->getId());
    if (SVFGEdge* edge = svfg->hasInterVFGEdge(src, dst, SVFGEdge::RetIndVF, csId))
    {
        SVFUtil::cast<RetIndSVFGEdge>(edge)->addPointsTo(cpts);
    }
    else
    {
        RetIndSVFGEdge* retEdge = new RetIndSVFGEdge(src, dst, csId);
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

        ThreadCallGraph::InstSet joinsites;
        tcg->getJoinSites(tcg->getCallGraphNode(formalOut->getFun()), joinsites);
        for (const CallICFGNode* cs : joinsites)
        {
            if (!mssa->hasCHI(cs))
                continue;
            SVFG::ActualOUTSVFGNodeSet& actualOuts = svfg->getActualOUTSVFGNodes(cs);
            for (NodeID aoId : actualOuts)
            {
                const ActualOUTSVFGNode* actualOut =
                    SVFUtil::cast<ActualOUTSVFGNode>(svfg->getSVFGNode(aoId));
                addJoinRetEdge(formalOut, actualOut, svfg->getCallSiteID(cs, formalOut->getFun()));
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
        const SVFGNode* snode = it->second;
        if (SVFUtil::isa<LoadSVFGNode>(snode))
        {
            const StmtSVFGNode* node = SVFUtil::cast<StmtSVFGNode>(snode);
            if (node->getICFGNode())
                ldnodeSet.insert(node);
        }
        else if (SVFUtil::isa<StoreSVFGNode>(snode))
        {
            const StmtSVFGNode* node = SVFUtil::cast<StmtSVFGNode>(snode);
            if (node->getICFGNode())
                stnodeSet.insert(node);
        }
    }
}

/*!
 * Add (or merge into) a thread-MHP indirect value-flow edge src -> dst.
 */
SVFGEdge* MTASVFGBuilder::addTDEdge(NodeID srcId, NodeID dstId, const PointsTo& pts)
{
    SVFGNode* srcNode = svfg->getSVFGNode(srcId);
    SVFGNode* dstNode = svfg->getSVFGNode(dstId);

    if (SVFGEdge* edge = svfg->hasThreadVFGEdge(srcNode, dstNode, SVFGEdge::TheadMHPIndirectVF))
    {
        assert(SVFUtil::isa<IndirectSVFGEdge>(edge) && "should be an indirect value-flow edge!");
        return (SVFUtil::cast<IndirectSVFGEdge>(edge)->addPointsTo(pts.toNodeBS()) ? edge : nullptr);
    }
    else
    {
        numOfNewSVFGEdges++;
        ThreadMHPIndSVFGEdge* indirectEdge = new ThreadMHPIndSVFGEdge(srcNode, dstNode);
        indirectEdge->addPointsTo(pts.toNodeBS());
        return (svfg->addSVFGEdge(indirectEdge) ? indirectEdge : nullptr);
    }
}

/*!
 * Backward reachable store SVFG nodes via indirect value flow (lock-span head test).
 */
MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getPrevNodes(const StmtSVFGNode* n)
{
    if (prevset.find(n) != prevset.end())
        return prevset[n];

    SVFGNodeIDSet prev;
    Set<const SVFGNode*> worklist;
    Set<const SVFGNode*> visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n->InEdgeBegin(); iter != n->InEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge())
            worklist.insert(edge->getSrcNode());
    }

    while (!worklist.empty())
    {
        const SVFGNode* node = *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode>(node))
            prev.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->InEdgeBegin(); iter != node->InEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() && visited.find(edge->getSrcNode()) == visited.end())
                    worklist.insert(edge->getSrcNode());
            }
        }
    }
    prevset[n] = prev;
    return prev;
}

/*!
 * Forward reachable store/load SVFG nodes via indirect value flow (lock-span tail test).
 */
MTASVFGBuilder::SVFGNodeIDSet MTASVFGBuilder::getSuccNodes(const StmtSVFGNode* n)
{
    if (succset.find(n) != succset.end())
        return succset[n];

    SVFGNodeIDSet succ;
    Set<const SVFGNode*> worklist;
    Set<const SVFGNode*> visited;

    for (SVFGEdge::SVFGEdgeSetTy::iterator iter = n->OutEdgeBegin(); iter != n->OutEdgeEnd(); ++iter)
    {
        SVFGEdge* edge = *iter;
        if (edge->isIndirectVFGEdge())
            worklist.insert(edge->getDstNode());
    }

    while (!worklist.empty())
    {
        const SVFGNode* node = *worklist.begin();
        worklist.erase(worklist.begin());
        visited.insert(node);
        if (SVFUtil::isa<StoreSVFGNode, LoadSVFGNode>(node))
            succ.set(node->getId());
        else
        {
            for (SVFGEdge::SVFGEdgeSetTy::iterator iter = node->OutEdgeBegin(); iter != node->OutEdgeEnd(); ++iter)
            {
                SVFGEdge* edge = *iter;
                if (edge->isIndirectVFGEdge() && visited.find(edge->getDstNode()) == visited.end())
                    worklist.insert(edge->getDstNode());
            }
        }
    }
    succset[n] = succ;
    return succ;
}

/*!
 * Whether, for all lock spans n belongs to, n is the first write (span head).
 */
bool MTASVFGBuilder::isHeadOfSpan(const StmtSVFGNode* n)
{
    if (headmap.find(n) != headmap.end())
        return headmap[n];

    SVFGNodeIDSet prev = getPrevNodes(n);
    for (NodeID id : prev)
    {
        const StmtSVFGNode* prevNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(id));
        if (prevNode && lockana->isInSameSpan(prevNode->getICFGNode(), n->getICFGNode()))
        {
            headmap[n] = false;
            return false;
        }
    }
    headmap[n] = true;
    return true;
}

/*!
 * Whether, for all lock spans n belongs to, n is the last write (span tail).
 */
bool MTASVFGBuilder::isTailOfSpan(const StmtSVFGNode* n)
{
    assert(SVFUtil::isa<StoreSVFGNode>(n) && "tail test only for store nodes");

    if (tailmap.find(n) != tailmap.end())
        return tailmap[n];

    SVFGNodeIDSet succ = getSuccNodes(n);
    for (NodeID id : succ)
    {
        const SVFGNode* sn = svfg->getSVFGNode(id);
        if (SVFUtil::isa<LoadSVFGNode>(sn))
            continue;
        const StmtSVFGNode* succNode = SVFUtil::dyn_cast<StmtSVFGNode>(sn);
        if (succNode && lockana->isInSameSpan(succNode->getICFGNode(), n->getICFGNode()))
        {
            tailmap[n] = false;
            return false;
        }
    }
    tailmap[n] = true;
    return true;
}

/*!
 * Record the per-edge [THREAD-VF] query for one candidate pair s --o--> sp (see
 * getThreadVFQueryMap for the rule): the endpoints, plus -- under a common lock --
 * the in-span Succ_spl(s) / Pred_spl'(sp) witnesses. Enumerated fully (the
 * tail/head boolean tests short-circuit; source extraction must not).
 */
void MTASVFGBuilder::recordThreadVFSource(const StmtSVFGNode* s, const StmtSVFGNode* sp, bool commonLock)
{
    // Per-edge Query set. The endpoints are NOT duplicated into the value --
    // they are recoverable from the map key -- so the value holds only the
    // additional in-span witnesses below (empty for the common lock-free case).
    std::set<const ICFGNode*>& query = threadVFQueryMap[{s, sp}];

    if (!commonLock)
        return;

    // Succ_spl(s) = { x in s's span | x is a store, s --o--> x } (tail witnesses).
    for (NodeID id : getSuccNodes(s))
    {
        const SVFGNode* sn = svfg->getSVFGNode(id);
        if (!SVFUtil::isa<StoreSVFGNode>(sn))
            continue;
        const StmtSVFGNode* succNode = SVFUtil::cast<StmtSVFGNode>(sn);
        if (lockana->isInSameSpan(succNode->getICFGNode(), s->getICFGNode()))
            query.insert(succNode->getICFGNode());
    }

    // Pred_spl'(sp) = { x in sp's span | x --o--> sp } (head witnesses).
    for (NodeID id : getPrevNodes(sp))
    {
        const StmtSVFGNode* prevNode = SVFUtil::dyn_cast<StmtSVFGNode>(svfg->getSVFGNode(id));
        if (prevNode && lockana->isInSameSpan(prevNode->getICFGNode(), sp->getICFGNode()))
            query.insert(prevNode->getICFGNode());
    }
}

/*!
 * Store -> Load interference: add a thread-aware def-use edge if the store may
 * happen in parallel with and may alias the load, unless excluded by a common
 * lock (then only when the store is a span tail and the load a span head).
 */
void MTASVFGBuilder::handleStoreLoad(const StmtSVFGNode* n1, const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    const ICFGNode* i1 = n1->getICFGNode();
    const ICFGNode* i2 = n2->getICFGNode();

    if (!mhp->mayHappenInParallel(i1, i2))
        return;

    // No alias() re-check: the bucketed candidate generator only pairs accesses
    // whose raw points-to sets share an object, so the intersection below is
    // non-empty by construction and alias() could never answer NoAlias here.
    PointsTo pts = pta->getPts(n1->getDstNodeID());
    pts &= pta->getPts(n2->getSrcNodeID());

    // [THREAD-VF] source extraction runs for every candidate pair (both the
    // pairs that survive and the ones the lock test prunes), so the sliced ILA
    // can re-derive whether the edge holds.
    bool commonLock = lockana->isProtectedByCommonLock(i1, i2);
    recordThreadVFSource(n1, n2, commonLock);

    if (commonLock)
    {
        if (isTailOfSpan(n1) && isHeadOfSpan(n2))
            addTDEdge(n1->getId(), n2->getId(), pts);
    }
    else
    {
        addTDEdge(n1->getId(), n2->getId(), pts);
    }
}

/*!
 * Store -> Store interference (symmetric): add thread-aware def-use edges in
 * both directions, with the same lock-span pruning as store/load.
 */
void MTASVFGBuilder::handleStoreStore(const StmtSVFGNode* n1, const StmtSVFGNode* n2, PointerAnalysis* pta)
{
    const ICFGNode* i1 = n1->getICFGNode();
    const ICFGNode* i2 = n2->getICFGNode();

    if (!mhp->mayHappenInParallel(i1, i2))
        return;

    // No alias() re-check: see handleStoreLoad -- bucketing already guarantees a
    // shared raw object.
    PointsTo pts = pta->getPts(n1->getDstNodeID());
    pts &= pta->getPts(n2->getDstNodeID());

    // Both directions are candidate thread-aware edges; extract sources for each.
    bool commonLock = lockana->isProtectedByCommonLock(i1, i2);
    recordThreadVFSource(n1, n2, commonLock);
    recordThreadVFSource(n2, n1, commonLock);

    if (commonLock)
    {
        if (isTailOfSpan(n1) && isHeadOfSpan(n2))
            addTDEdge(n1->getId(), n2->getId(), pts);
        if (isTailOfSpan(n2) && isHeadOfSpan(n1))
            addTDEdge(n2->getId(), n1->getId(), pts);
    }
    else
    {
        addTDEdge(n1->getId(), n2->getId(), pts);
        addTDEdge(n2->getId(), n1->getId(), pts);
    }
}

/*!
 * For every MHP store/load and store/store pair, add the thread-aware
 * (interference) value-flow edges.
 */
void MTASVFGBuilder::connectMHPEdges(PointerAnalysis* pta)
{
    collectLoadStoreSVFGNodes();

    // Inverted access index (object -> access-node bitset): unioning the
    // bitsets per store visits each may-alias pair exactly once, no dedup tables.
    Map<NodeID, SVFGNodeIDSet> objToStoreIds;
    Map<NodeID, SVFGNodeIDSet> objToLoadIds;
    for (const StmtSVFGNode* store : stnodeSet)
        for (NodeID obj : pta->getPts(store->getDstNodeID()))
            objToStoreIds[obj].set(store->getId());
    for (const StmtSVFGNode* load : ldnodeSet)
        for (NodeID obj : pta->getPts(load->getSrcNodeID()))
            objToLoadIds[obj].set(load->getId());

    for (const StmtSVFGNode* store : stnodeSet)
    {
        SVFGNodeIDSet candLoads, candStores;
        for (NodeID obj : pta->getPts(store->getDstNodeID()))
        {
            candStores |= objToStoreIds[obj];
            Map<NodeID, SVFGNodeIDSet>::const_iterator lIt = objToLoadIds.find(obj);
            if (lIt != objToLoadIds.end())
                candLoads |= lIt->second;
        }

        for (NodeID loadId : candLoads)
            handleStoreLoad(store, SVFUtil::cast<StmtSVFGNode>(svfg->getSVFGNode(loadId)), pta);

        // Visit each unordered store pair once: only partners with a larger id.
        const NodeID storeId = store->getId();
        for (NodeID otherId : candStores)
        {
            if (otherId <= storeId)
                continue;
            handleStoreStore(store, SVFUtil::cast<StmtSVFGNode>(svfg->getSVFGNode(otherId)), pta);
        }
    }
}
