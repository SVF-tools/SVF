//===- VersionedFlowSensitive.cpp -- Versioned flow-sensitive pointer analysis------------//

/*
 * VersionedFlowSensitive.cpp
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#include "WPA/Andersen.h"
#include "WPA/VersionedFlowSensitive.h"
#include "Util/Options.h"
#include "MemoryModel/PointsTo.h"
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>

using namespace SVF;

const Version VersionedFlowSensitive::invalidVersion = 0;
VersionedFlowSensitive *VersionedFlowSensitive::vfspta = nullptr;

VersionedVar VersionedFlowSensitive::atKey(NodeID var, Version version)
{
    assert(version != invalidVersion && "VersionedFlowSensitive::atKey: trying to use an invalid version!");
    return std::make_pair(var, version);
}

VersionedFlowSensitive::VersionedFlowSensitive(SVFIR *_pag, PTATY type)
    : FlowSensitive(_pag, type)
{
    numPrelabeledNodes = numPrelabelVersions = 0;
    prelabelingTime = meldLabelingTime = versionPropTime = 0.0;
    // We'll grab vPtD in initialize.

    for (SVFIR::const_iterator it = pag->begin(); it != pag->end(); ++it)
    {
        if (SVFUtil::isa<ObjVar>(it->second)) equivalentObject[it->first] = it->first;
    }

    assert(!Options::OPTSVFG && "VFS: -opt-svfg not currently supported with VFS.");
}

void VersionedFlowSensitive::initialize()
{
    FlowSensitive::initialize();
    // Overwrite the stat FlowSensitive::initialize gave us.
    delete stat;
    stat = new VersionedFlowSensitiveStat(this);

    vPtD = getVersionedPTDataTy();

    buildIsStoreLoadMaps();
    buildDeltaMaps();
    consume.resize(svfg->getTotalNodeNum());
    yield.resize(svfg->getTotalNodeNum());

    prelabel();
    meldLabel();

    removeAllIndirectSVFGEdges();
}

void VersionedFlowSensitive::finalize()
{
    FlowSensitive::finalize();
    // vPtD->dumpPTData();
    // dumpReliances();
    // dumpLocVersionMaps();
}

void VersionedFlowSensitive::prelabel(void)
{
    double start = stat->getClk(true);
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        NodeID l = it->first;
        const SVFGNode *ln = it->second;

        if (const StoreSVFGNode *stn = SVFUtil::dyn_cast<StoreSVFGNode>(ln))
        {
            // l: *p = q.
            // If p points to o (Andersen's), l yields a new version for o.
            NodeID p = stn->getPAGDstNodeID();
            for (NodeID o : ander->getPts(p))
            {
                prelabeledObjects.insert(o);
            }

            vWorklist.push(l);

            if (ander->getPts(p).count() != 0) ++numPrelabeledNodes;
        }
        else if (delta(l))
        {
            // The outgoing edges are not only what will later be propagated. SVFGOPT may
            // move around nodes such that there can be an MRSVFGNode with no incoming or
            // outgoing edges which will be added at runtime. In essence, we can no
            // longer rely on the outgoing edges of a delta node when SVFGOPT is enabled.
            const MRSVFGNode *mr = SVFUtil::dyn_cast<MRSVFGNode>(ln);
            if (mr != nullptr)
            {
                for (const NodeID o : mr->getPointsTo())
                {
                    prelabeledObjects.insert(o);
                }

                // Push into worklist because its consume == its yield.
                vWorklist.push(l);
                if (mr->getPointsTo().count() != 0) ++numPrelabeledNodes;
            }
        }
    }

    double end = stat->getClk(true);
    prelabelingTime = (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::meldLabel(void)
{
    double start = stat->getClk(true);

    assert(Options::VersioningThreads > 0 && "VFS::meldLabel: number of versioning threads must be > 0!");

    // Nodes which have at least one object on them given a prelabel.
    std::vector<const SVFGNode *> prelabeledNodes;
    // Fast query for the above.
    std::vector<bool> isPrelabeled(svfg->getTotalNodeNum(), false);
    while (!vWorklist.empty())
    {
        const NodeID n = vWorklist.pop();
        prelabeledNodes.push_back(svfg->getSVFGNode(n));
        isPrelabeled[n] = true;
    }

    // Delta, delta source, store, and load nodes, which require versions during
    // solving, unlike other nodes with which we can make do with the reliance map.
    std::vector<NodeID> nodesWhichNeedVersions;
    for (SVFG::const_iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        const NodeID n = it->first;
        if (delta(n) || deltaSource(n) || isStore(n) || isLoad(n)) nodesWhichNeedVersions.push_back(n);
    }

    std::mutex *versionMutexes = new std::mutex[nodesWhichNeedVersions.size()];

    // Map of footprints to the canonical object "owning" the footprint.
    Map<std::vector<const IndirectSVFGEdge *>, NodeID> footprintOwner;

    std::queue<NodeID> objectQueue;
    for (const NodeID o : prelabeledObjects)
    {
        // "Touch" maps with o so we don't need to lock on them.
        versionReliance[o];
        stmtReliance[o];
        objectQueue.push(o);
    }

    std::mutex objectQueueMutex;
    std::mutex footprintOwnerMutex;

    auto meldVersionWorker = [this, &footprintOwner, &objectQueue,
                              &objectQueueMutex, &footprintOwnerMutex, &versionMutexes,
                              &prelabeledNodes, &isPrelabeled, &nodesWhichNeedVersions]
                             (const unsigned thread)
    {
        while (true)
        {
            NodeID o;
            { std::lock_guard<std::mutex> guard(objectQueueMutex);
                // No more objects? Done.
                if (objectQueue.empty()) return;
                o = objectQueue.front();
                objectQueue.pop();
            }

            // 1. Compute the SCCs for the nodes on the graph overlay of o.
            // For starting nodes, we only need those which did prelabeling for o specifically.
            // TODO: maybe we should move this to prelabel with a map (o -> starting nodes).
            std::vector<const SVFGNode *> osStartingNodes;
            for (const SVFGNode *sn : prelabeledNodes)
            {
                if (const StoreSVFGNode *store = SVFUtil::dyn_cast<StoreSVFGNode>(sn))
                {
                    const NodeID p = store->getPAGDstNodeID();
                    if (this->ander->getPts(p).test(o)) osStartingNodes.push_back(sn);
                }
                else if (delta(sn->getId()))
                {
                    const MRSVFGNode *mr = SVFUtil::dyn_cast<MRSVFGNode>(sn);
                    if (mr != nullptr)
                    {
                        if (mr->getPointsTo().test(o)) osStartingNodes.push_back(sn);
                    }
                }
            }

            std::vector<int> partOf;
            std::vector<const IndirectSVFGEdge *> footprint;
            unsigned numSCCs = SCC::detectSCCs(this, this->svfg, o, osStartingNodes, partOf, footprint);

            // 2. Skip any further processing of a footprint we have seen before.
            { std::lock_guard<std::mutex> guard(footprintOwnerMutex);
                const Map<std::vector<const IndirectSVFGEdge *>, NodeID>::const_iterator canonOwner
                    = footprintOwner.find(footprint);
                if (canonOwner == footprintOwner.end())
                {
                    this->equivalentObject[o] = o;
                    footprintOwner[footprint] = o;
                }
                else
                {
                    this->equivalentObject[o] = canonOwner->second;
                    // Same version and stmt reliance as the canonical. During solving we cannot just reuse
                    // the canonical object's reliance because it may change due to on-the-fly call graph
                    // construction. Something like copy-on-write could be good... probably negligible.
                    this->versionReliance.at(o) = this->versionReliance.at(canonOwner->second);
                    this->stmtReliance.at(o) = this->stmtReliance.at(canonOwner->second);
                    continue;
                }
            }

            // 3. a. Initialise the MeldVersion of prelabeled nodes (SCCs).
            //    b. Initialise a todo list of all the nodes we need to version,
            //       sorted according to topological order.
            // We will use a map of sccs to meld versions for what is consumed.
            std::vector<MeldVersion> sccToMeldVersion(numSCCs);
            // At stores, what is consumed is different to what is yielded, so we
            // maintain that separately.
            Map<NodeID, MeldVersion> storesYieldedMeldVersion;
            // SVFG nodes of interest -- those part of an SCC from the starting nodes.
            std::vector<NodeID> todoList;
            unsigned bit = 0;
            for (NodeID n = 0; n < partOf.size(); ++n)
            {
                if (partOf[n] == -1) continue;

                if (isPrelabeled[n])
                {
                    if (this->isStore(n)) storesYieldedMeldVersion[n].set(bit);
                    else sccToMeldVersion[partOf[n]].set(bit);
                    ++bit;
                }

                todoList.push_back(n);
            }

            // Sort topologically so each nodes is only visited once.
            auto cmp = [&partOf](const NodeID a, const NodeID b)
            {
                return partOf[a] > partOf[b];
            };
            std::sort(todoList.begin(), todoList.end(), cmp);

            // 4. a. Do meld versioning.
            //    b. Determine SCC reliances.
            //    c. Build a footprint for o (all edges which it is found on).
            //    d. Determine which SCCs belong to stores.

            // sccReliance[x] = { y_1, y_2, ... } if there exists an edge from a node
            // in SCC x to SCC y_i.
            std::vector<Set<int>> sccReliance(numSCCs);
            // Maps SCC to the store it corresponds to or -1 if it doesn't. TODO: unsigned vs signed -- nasty.
            std::vector<int> storeSCC(numSCCs, -1);
            for (size_t i = 0; i < todoList.size(); ++i)
            {
                const NodeID n = todoList[i];
                const SVFGNode *sn = this->svfg->getSVFGNode(n);
                const bool nIsStore = this->isStore(n);

                int nSCC = partOf[n];
                if (nIsStore) storeSCC[nSCC] = n;

                // Given n -> m, the yielded version of n will be melded into m.
                // For stores, that is in storesYieldedMeldVersion, otherwise, consume == yield and
                // we can just use sccToMeldVersion.
                const MeldVersion &nMV = nIsStore ? storesYieldedMeldVersion[n] : sccToMeldVersion[nSCC];
                for (const SVFGEdge *e : sn->getOutEdges())
                {
                    const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
                    if (!ie) continue;

                    const NodeID m = ie->getDstNode()->getId();
                    // Ignoreedges which don't involve o.
                    if (!ie->getPointsTo().test(o)) continue;

                    int mSCC = partOf[m];

                    // There is an edge from the SCC n belongs to to that m belongs to.
                    sccReliance[nSCC].insert(mSCC);

                    // Ignore edges to delta nodes (prelabeled consume).
                    // No point propagating when n's SCC == m's SCC (same meld version there)
                    // except when it is a store, because we are actually propagating n's yielded
                    // into m's consumed. Store nodes are in their own SCCs, so it is a self
                    // loop on a store node.
                    if (!this->delta(m) && (nSCC != mSCC || nIsStore))
                    {
                        sccToMeldVersion[mSCC] |= nMV;
                    }
                }
            }

            // 5. Transform meld versions belonging to SCCs into versions.
            Map<MeldVersion, Version> mvv;
            std::vector<Version> sccToVersion(numSCCs, invalidVersion);
            Version curVersion = 0;
            for (u32_t scc = 0; scc < sccToMeldVersion.size(); ++scc)
            {
                const MeldVersion &mv = sccToMeldVersion[scc];
                Map<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
                Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
                sccToVersion[scc] = v;
            }

            sccToMeldVersion.clear();

            // Same for storesYieldedMeldVersion.
            Map<NodeID, Version> storesYieldedVersion;
            for (auto const& nmv : storesYieldedMeldVersion)
            {
                const NodeID n = nmv.first;
                const MeldVersion &mv = nmv.second;

                Map<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
                Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
                storesYieldedVersion[n] = v;
            }

            storesYieldedMeldVersion.clear();

            mvv.clear();

            // 6. From SCC reliance, determine version reliances.
            Map<Version, std::vector<Version>> &osVersionReliance = this->versionReliance.at(o);
            for (u32_t scc = 0; scc < numSCCs; ++scc)
            {
                if (sccReliance[scc].empty()) continue;

                // Some consume relies on a yield. When it's a store, we need to pick whether to
                // use the consume or yield unlike when it is not because they are the same.
                const Version version
                    = storeSCC[scc] != -1 ? storesYieldedVersion[storeSCC[scc]] : sccToVersion[scc];

                std::vector<Version> &reliantVersions = osVersionReliance[version];
                for (const int reliantSCC : sccReliance[scc])
                {
                    const Version reliantVersion = sccToVersion[reliantSCC];
                    if (version != reliantVersion)
                    {
                        // sccReliance is a set, no need to worry about duplicates.
                        reliantVersions.push_back(reliantVersion);
                    }
                }
            }

            // 7. a. Save versions for nodes which need them.
            //    b. Fill in stmtReliance.
            // TODO: maybe randomise iteration order for less contention? Needs profiling.
            Map<Version, NodeBS> &osStmtReliance = this->stmtReliance.at(o);
            for (size_t i = 0; i < nodesWhichNeedVersions.size(); ++i)
            {
                const NodeID n = nodesWhichNeedVersions[i];
                std::mutex &mutex = versionMutexes[i];

                const int scc = partOf[n];
                if (scc == -1) continue;

                std::lock_guard<std::mutex> guard(mutex);

                const Version c = sccToVersion[scc];
                if (c != invalidVersion)
                {
                    this->setConsume(n, o, c);
                    if (this->isStore(n) || this->isLoad(n)) osStmtReliance[c].set(n);
                }

                if (this->isStore(n))
                {
                    const Map<NodeID, Version>::const_iterator yIt = storesYieldedVersion.find(n);
                    if (yIt != storesYieldedVersion.end()) this->setYield(n, o, yIt->second);
                }
            }
        }
    };

    std::vector<std::thread> workers;
    for (unsigned i = 0; i < Options::VersioningThreads; ++i) workers.push_back(std::thread(meldVersionWorker, i));
    for (std::thread &worker : workers) worker.join();

    delete[] versionMutexes;

    double end = stat->getClk(true);
    meldLabelingTime = (end - start) / TIMEINTERVAL;
}

bool VersionedFlowSensitive::meld(MeldVersion &mv1, const MeldVersion &mv2)
{
    // Meld operator is union of bit vectors.
    return mv1 |= mv2;
}

bool VersionedFlowSensitive::delta(const NodeID l) const
{
    assert(l < deltaMap.size() && "VFS::delta: deltaMap is missing SVFG nodes!");
    return deltaMap[l];
}

bool VersionedFlowSensitive::deltaSource(const NodeID l) const
{
    assert(l < deltaSourceMap.size() && "VFS::delta: deltaSourceMap is missing SVFG nodes!");
    return deltaSourceMap[l];
}

void VersionedFlowSensitive::buildIsStoreLoadMaps(void)
{
    isStoreMap.resize(svfg->getTotalNodeNum(), false);
    isLoadMap.resize(svfg->getTotalNodeNum(), false);
    for (SVFG::const_iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        if (SVFUtil::isa<StoreSVFGNode>(it->second)) isStoreMap[it->first] = true;
        else if (SVFUtil::isa<LoadSVFGNode>(it->second)) isLoadMap[it->first] = true;
    }
}

bool VersionedFlowSensitive::isStore(const NodeID l) const
{
    assert(l < isStoreMap.size() && "VFS::isStore: isStoreMap is missing SVFG nodes!");
    return isStoreMap[l];
}

bool VersionedFlowSensitive::isLoad(const NodeID l) const
{
    assert(l < isLoadMap.size() && "VFS::isLoad: isLoadMap is missing SVFG nodes!");
    return isLoadMap[l];
}

void VersionedFlowSensitive::buildDeltaMaps(void)
{
    deltaMap.resize(svfg->getTotalNodeNum(), false);

    // Call block nodes corresponding to all delta nodes.
    Set<const CallICFGNode *> deltaCBNs;

    for (SVFG::const_iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        const NodeID l = it->first;
        const SVFGNode *s = it->second;

        // Cases:
        //  * Function entry: can get new incoming indirect edges through ind. callsites.
        //  * Callsite returns: can get new incoming indirect edges if the callsite is indirect.
        //  * Otherwise: static.
        bool isDelta = false;
        if (const SVFFunction *fn = svfg->isFunEntrySVFGNode(s))
        {
            PTACallGraphEdge::CallInstSet callsites;
            /// use pre-analysis call graph to approximate all potential callsites
            ander->getPTACallGraph()->getIndCallSitesInvokingCallee(fn, callsites);
            isDelta = !callsites.empty();

            if (isDelta)
            {
                // TODO: could we use deltaCBNs in the call above, avoiding this loop?
                for (const CallICFGNode *cbn : callsites) deltaCBNs.insert(cbn);
            }
        }
        else if (const CallICFGNode *cbn = svfg->isCallSiteRetSVFGNode(s))
        {
            isDelta = cbn->isIndirectCall();
            if (isDelta) deltaCBNs.insert(cbn);
        }

        deltaMap[l] = isDelta;
    }

    deltaSourceMap.resize(svfg->getTotalNodeNum(), false);

    for (SVFG::const_iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        const NodeID l = it->first;
        const SVFGNode *s = it->second;

        if (const CallICFGNode *cbn = SVFUtil::dyn_cast<CallICFGNode>(s->getICFGNode()))
        {
            if (deltaCBNs.find(cbn) != deltaCBNs.end()) deltaSourceMap[l] = true;
        }

        // TODO: this is an over-approximation but it sound, marking every formal out as
        //       a delta-source.
        if (SVFUtil::isa<FormalOUTSVFGNode>(s)) deltaSourceMap[l] = true;
    }
}

void VersionedFlowSensitive::removeAllIndirectSVFGEdges(void)
{
    for (SVFG::iterator nodeIt = svfg->begin(); nodeIt != svfg->end(); ++nodeIt)
    {
        SVFGNode *sn = nodeIt->second;

        const SVFGEdgeSetTy &inEdges = sn->getInEdges();
        std::vector<SVFGEdge *> toDeleteFromIn;
        for (SVFGEdge *e : inEdges)
        {
            if (SVFUtil::isa<IndirectSVFGEdge>(e)) toDeleteFromIn.push_back(e);
        }

        for (SVFGEdge *e : toDeleteFromIn) svfg->removeSVFGEdge(e);

        // Only need to iterate over incoming edges for each node because edges
        // will be deleted from in/out through removeSVFGEdge.
    }

    setGraph(svfg);
}

void VersionedFlowSensitive::propagateVersion(NodeID o, Version v)
{
    double start = stat->getClk();

    const std::vector<Version> &reliantVersions = getReliantVersions(o, v);
    for (Version r : reliantVersions)
    {
        propagateVersion(o, v, r, false);
    }

    double end = stat->getClk();
    versionPropTime += (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::propagateVersion(const NodeID o, const Version v, const Version vp, bool time/*=true*/)
{
    double start = time ? stat->getClk() : 0.0;

    const VersionedVar srcVar = atKey(o, v);
    const VersionedVar dstVar = atKey(o, vp);
    if (vPtD->unionPts(dstVar, srcVar))
    {
        // o:vp has changed.
        // Add the dummy propagation node to tell the solver to propagate it later.
        const DummyVersionPropSVFGNode *dvp = nullptr;
        VarToPropNodeMap::const_iterator dvpIt = versionedVarToPropNode.find(dstVar);
        if (dvpIt == versionedVarToPropNode.end())
        {
            dvp = svfg->addDummyVersionPropSVFGNode(o, vp);
            versionedVarToPropNode[dstVar] = dvp;
        } else dvp = dvpIt->second;

        assert(dvp != nullptr && "VFS::propagateVersion: propagation dummy node not found?");
        pushIntoWorklist(dvp->getId());

        // Notify nodes which rely on o:vp that it changed.
        for (NodeID s : getStmtReliance(o, vp)) pushIntoWorklist(s);
    }

    double end = time ? stat->getClk() : 0.0;
    if (time) versionPropTime += (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::processNode(NodeID n)
{
    SVFGNode* sn = svfg->getSVFGNode(n);
    // Handle DummyVersPropSVFGNode here so we don't have to override the long
    // processSVFGNode. We also don't call propagate based on its result.
    if (const DummyVersionPropSVFGNode *dvp = SVFUtil::dyn_cast<DummyVersionPropSVFGNode>(sn))
    {
        propagateVersion(dvp->getObject(), dvp->getVersion());
    }
    else if (processSVFGNode(sn))
    {
        propagate(&sn);
    }
}

void VersionedFlowSensitive::updateConnectedNodes(const SVFGEdgeSetTy& newEdges)
{
    for (const SVFGEdge *e : newEdges)
    {
        SVFGNode *dstNode = e->getDstNode();
        NodeID src = e->getSrcNode()->getId();
        NodeID dst = dstNode->getId();

        if (SVFUtil::isa<PHISVFGNode>(dstNode)
            || SVFUtil::isa<FormalParmSVFGNode>(dstNode)
            || SVFUtil::isa<ActualRetSVFGNode>(dstNode))
        {
            pushIntoWorklist(dst);
        }
        else
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            assert(ie != nullptr && "VFS::updateConnectedNodes: given direct edge?");

            assert(delta(dst) && "VFS::updateConnectedNodes: new edges should be to delta nodes!");
            assert(deltaSource(src) && "VFS::updateConnectedNodes: new indirect edges should be from delta source nodes!");

            const NodeBS &ept = ie->getPointsTo();
            // For every o, such that src --o--> dst, we need to set up reliance (and propagate).
            for (const NodeID o : ept)
            {
                Version srcY = getYield(src, o);
                if (srcY == invalidVersion) continue;
                Version dstC = getConsume(dst, o);
                if (dstC == invalidVersion) continue;

                std::vector<Version> &versionsRelyingOnSrcY = getReliantVersions(o, srcY);
                if (std::find(versionsRelyingOnSrcY.begin(), versionsRelyingOnSrcY.end(), dstC) == versionsRelyingOnSrcY.end())
                {
                    versionsRelyingOnSrcY.push_back(dstC);
                    propagateVersion(o, srcY, dstC);
                }
            }
        }
    }
}

bool VersionedFlowSensitive::processLoad(const LoadSVFGNode* load)
{
    double start = stat->getClk();

    bool changed = false;

    // l: p = *q
    NodeID l = load->getId();
    NodeID p = load->getPAGDstNodeID();
    NodeID q = load->getPAGSrcNodeID();

    const PointsTo& qpt = getPts(q);
    for (NodeID o : qpt)
    {
        if (pag->isConstantObj(o) || pag->isNonPointerObj(o)) continue;

        const Version c = getConsume(l, o);
        if (c != invalidVersion && vPtD->unionPts(p, atKey(o, c)))
        {
            changed = true;
        }

        if (isFieldInsensitive(o))
        {
            /// If o is a field-insensitive object, we should also get all field nodes'
            /// points-to sets and pass them to p.
            const NodeBS& fields = getAllFieldsObjVars(o);
            for (NodeID of : fields)
            {
                const Version c = getConsume(l, of);
                if (c != invalidVersion && vPtD->unionPts(p, atKey(of, c)))
                {
                    changed = true;
                }
            }
        }
    }

    double end = stat->getClk();
    loadTime += (end - start) / TIMEINTERVAL;
    return changed;
}

bool VersionedFlowSensitive::processStore(const StoreSVFGNode* store)
{
    NodeID p = store->getPAGDstNodeID();
    const PointsTo &ppt = getPts(p);

    if (ppt.empty()) return false;

    NodeID q = store->getPAGSrcNodeID();
    const PointsTo &qpt = getPts(q);

    NodeID l = store->getId();
    // l: *p = q

    double start = stat->getClk();
    bool changed = false;
    // The version for these objects would be y_l(o).
    NodeBS changedObjects;

    if (!qpt.empty())
    {
        for (NodeID o : ppt)
        {
            if (pag->isConstantObj(o) || pag->isNonPointerObj(o)) continue;

            const Version y = getYield(l, o);
            if (y != invalidVersion && vPtD->unionPts(atKey(o, y), q))
            {
                changed = true;
                changedObjects.set(o);
            }
        }
    }

    double end = stat->getClk();
    storeTime += (end - start) / TIMEINTERVAL;

    double updateStart = stat->getClk();

    NodeID singleton = 0;
    bool isSU = isStrongUpdate(store, singleton);
    if (isSU) svfgHasSU.set(l);
    else svfgHasSU.reset(l);

    // For all objects, perform pts(o:y) = pts(o:y) U pts(o:c) at loc,
    // except when a strong update is taking place.
    for (const ObjToVersionMap::value_type &oc : consume[l])
    {
        const NodeID o = oc.first;
        const Version c = oc.second;

        // Strong-updated; don't propagate.
        if (isSU && o == singleton) continue;

        const Version y = getYield(l, o);
        if (y != invalidVersion && vPtD->unionPts(atKey(o, y), atKey(o, c)))
        {
            changed = true;
            changedObjects.set(o);
        }
    }

    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    // Changed objects need to be propagated. Time here should be inconsequential
    // *except* for time taken for propagateVersion, which will time itself.
    if (!changedObjects.empty())
    {
        for (const NodeID o : changedObjects)
        {
            // Definitely has a yielded version (came from prelabelling) as these are
            // the changed objects which must've been pointed to in Andersen's too.
            const Version y = getYield(l, o);
            propagateVersion(o, y);

            // Some o/v pairs changed: statements need to know.
            for (NodeID s : getStmtReliance(o, y)) pushIntoWorklist(s);
        }
    }

    return changed;
}

void VersionedFlowSensitive::cluster(void)
{
    std::vector<std::pair<unsigned, unsigned>> keys;
    for (SVFIR::iterator pit = pag->begin(); pit != pag->end(); ++pit)
    {
        unsigned occ = 1;
        unsigned v = pit->first;
        if (Options::PredictPtOcc && pag->getObject(v) != nullptr) occ = stmtReliance[v].size() + 1;
        assert(occ != 0);
        keys.push_back(std::make_pair(v, occ));
    }

    PointsTo::MappingPtr nodeMapping =
        std::make_shared<std::vector<NodeID>>(NodeIDAllocator::Clusterer::cluster(ander, keys, candidateMappings, "aux-ander"));
    PointsTo::MappingPtr reverseNodeMapping =
        std::make_shared<std::vector<NodeID>>(NodeIDAllocator::Clusterer::getReverseNodeMapping(*nodeMapping));

    PointsTo::setCurrentBestNodeMapping(nodeMapping, reverseNodeMapping);
}

Version VersionedFlowSensitive::getVersion(const NodeID l, const NodeID o, const LocVersionMap &lvm) const
{
    const Map<NodeID, NodeID>::const_iterator canonObjectIt = equivalentObject.find(o);
    const NodeID op = canonObjectIt == equivalentObject.end() ? o : canonObjectIt->second;

    const ObjToVersionMap &ovm = lvm[l];
    const ObjToVersionMap::const_iterator foundVersion = ovm.find(op);
    return foundVersion == ovm.end() ? invalidVersion : foundVersion->second;
}

Version VersionedFlowSensitive::getConsume(const NodeID l, const NodeID o) const
{
    return getVersion(l, o, consume);
}

Version VersionedFlowSensitive::getYield(const NodeID l, const NodeID o) const
{
    // Non-store: consume == yield.
    if (isStore(l)) return getVersion(l, o, yield);
    else return getVersion(l, o, consume);
}

void VersionedFlowSensitive::setVersion(const NodeID l, const NodeID o, const Version v, LocVersionMap &lvm)
{
    ObjToVersionMap &ovm = lvm[l];
    ovm[o] = v;
}

void VersionedFlowSensitive::setConsume(const NodeID l, const NodeID o, const Version v)
{
    setVersion(l, o, v, consume);
}

void VersionedFlowSensitive::setYield(const NodeID l, const NodeID o, const Version v)
{
    // Non-store: consume == yield.
    if (isStore(l)) setVersion(l, o, v, yield);
    else setVersion(l, o, v, consume);
}

std::vector<Version> &VersionedFlowSensitive::getReliantVersions(const NodeID o, const Version v)
{
    return versionReliance[o][v];
}

NodeBS &VersionedFlowSensitive::getStmtReliance(const NodeID o, const Version v)
{
    return stmtReliance[o][v];
}

void VersionedFlowSensitive::dumpReliances(void) const
{
    SVFUtil::outs() << "# Version reliances\n";
    for (const Map<NodeID, Map<Version, std::vector<Version>>>::value_type &ovrv : versionReliance)
    {
        NodeID o = ovrv.first;
        SVFUtil::outs() << "  Object " << o << "\n";
        for (const Map<Version, std::vector<Version>>::value_type& vrv : ovrv.second)
        {
            Version v = vrv.first;
            SVFUtil::outs() << "    Version " << v << " is a reliance for: ";

            bool first = true;
            for (Version rv : vrv.second)
            {
                if (!first)
                {
                    SVFUtil::outs() << ", ";
                }

                SVFUtil::outs() << rv;
                first = false;
            }

            SVFUtil::outs() << "\n";
        }
    }

    SVFUtil::outs() << "# Statement reliances\n";
    for (const Map<NodeID, Map<Version, NodeBS>>::value_type &ovss : stmtReliance)
    {
        NodeID o = ovss.first;
        SVFUtil::outs() << "  Object " << o << "\n";

        for (const Map<Version, NodeBS>::value_type &vss : ovss.second)
        {
            Version v = vss.first;
            SVFUtil::outs() << "    Version " << v << " is a reliance for statements: ";

            const NodeBS &ss = vss.second;
            bool first = true;
            for (NodeID s : ss)
            {
                if (!first)
                {
                    SVFUtil::outs() << ", ";
                }

                SVFUtil::outs() << s;
                first = false;
            }

            SVFUtil::outs() << "\n";
        }
    }
}

void VersionedFlowSensitive::dumpLocVersionMaps(void) const
{
    SVFUtil::outs() << "# LocVersion Maps\n";
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        const NodeID loc = it->first;
        bool locPrinted = false;
        for (const LocVersionMap *lvm : { &consume, &yield })
        {
            if (lvm->at(loc).empty()) continue;
            if (!locPrinted)
            {
                SVFUtil::outs() << "  " << "SVFG node " << loc << "\n";
                locPrinted = true;
            }

            SVFUtil::outs() << "    " << (lvm == &consume ? "Consume " : "Yield   ") << ": ";

            bool first = true;
            for (const ObjToVersionMap::value_type &ov : lvm->at(loc))
            {
                const NodeID o = ov.first;
                const Version v = ov.second;
                SVFUtil::outs() << (first ? "" : ", ") << "<" << o << ", " << v << ">";
                first = false;
            }

            SVFUtil::outs() << "\n";
        }
    }

}

void VersionedFlowSensitive::dumpMeldVersion(MeldVersion &v)
{
    SVFUtil::outs() << "[ ";
    bool first = true;
    for (unsigned e : v)
    {
        if (!first)
        {
            SVFUtil::outs() << ", ";
        }

        SVFUtil::outs() << e;
        first = false;
    }

    SVFUtil::outs() << " ]";
}

unsigned VersionedFlowSensitive::SCC::detectSCCs(VersionedFlowSensitive *vfs,
                                                 const SVFG *svfg, const NodeID object,
                                                 const std::vector<const SVFGNode *> &startingNodes,
                                                 std::vector<int> &partOf,
                                                 std::vector<const IndirectSVFGEdge *> &footprint)
{
    partOf.resize(svfg->getTotalNodeNum());
    std::fill(partOf.begin(), partOf.end(), -1);
    footprint.clear();

    std::vector<NodeData> nodeData(svfg->getTotalNodeNum(), { -1, -1, false});
    std::stack<const SVFGNode *> stack;

    int index = 0;
    int currentSCC = 0;

    for (const SVFGNode *v : startingNodes)
    {
        if (nodeData[v->getId()].index == -1)
        {
            visit(vfs, object, partOf, footprint, nodeData, stack, index, currentSCC, v);
        }
    }

    // Make sure footprints with the same edges pass ==/hash the same.
    std::sort(footprint.begin(), footprint.end());

    return currentSCC;
}

void VersionedFlowSensitive::SCC::visit(VersionedFlowSensitive *vfs,
                                        const NodeID object,
                                        std::vector<int> &partOf,
                                        std::vector<const IndirectSVFGEdge *> &footprint,
                                        std::vector<NodeData> &nodeData,
                                        std::stack<const SVFGNode *> &stack,
                                        int &index,
                                        int &currentSCC,
                                        const SVFGNode *v)
{
    const NodeID vId = v->getId();

    nodeData[vId].index = index;
    nodeData[vId].lowlink = index;
    ++index;

    stack.push(v);
    nodeData[vId].onStack = true;

    for (const SVFGEdge *e : v->getOutEdges())
    {
        const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
        if (!ie) continue;

        const SVFGNode *w = ie->getDstNode();
        const NodeID wId = w->getId();

        // If object is not part of the edge, there is no edge from v to w.
        if (!ie->getPointsTo().test(object)) continue;

        // Even if we don't count edges to stores and deltas for SCCs' sake, they
        // are relevant to the footprint as a propagation still occurs over such edges.
        footprint.push_back(ie);

        // Ignore edges to delta nodes because they are prelabeled so cannot
        // be part of the SCC v is in (already in nodesTodo from the prelabeled set).
        // Similarly, store nodes.
        if (vfs->delta(wId) || vfs->isStore(wId)) continue;

        if (nodeData[wId].index == -1)
        {
            visit(vfs, object, partOf, footprint, nodeData, stack, index, currentSCC, w);
            nodeData[vId].lowlink = std::min(nodeData[vId].lowlink, nodeData[wId].lowlink);
        }
        else if (nodeData[wId].onStack)
        {
            nodeData[vId].lowlink = std::min(nodeData[vId].lowlink, nodeData[wId].index);
        }
    }

    if (nodeData[vId].lowlink == nodeData[vId].index)
    {
        const SVFGNode *w = nullptr;
        do
        {
            w = stack.top();
            stack.pop();
            const NodeID wId = w->getId();
            nodeData[wId].onStack = false;
            partOf[wId] = currentSCC;
        } while (w != v);

        // For the next SCC.
        ++currentSCC;
    }
}
