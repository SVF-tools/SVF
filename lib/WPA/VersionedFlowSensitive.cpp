//===- VersionedFlowSensitive.cpp -- Versioned flow-sensitive pointer analysis------------//

/*
 * VersionedFlowSensitive.cpp
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#include "WPA/Andersen.h"
#include "WPA/VersionedFlowSensitive.h"
#include <iostream>

using namespace SVF;

const Version VersionedFlowSensitive::invalidVersion = 0;
VersionedFlowSensitive *VersionedFlowSensitive::vfspta = nullptr;

VersionedVar VersionedFlowSensitive::atKey(NodeID var, Version version)
{
    assert(version != invalidVersion && "VersionedFlowSensitive::atKey: trying to use an invalid version!");
    return std::make_pair(var, version);
}

VersionedFlowSensitive::VersionedFlowSensitive(PAG *_pag, PTATY type)
    : FlowSensitive(_pag, type)
{
    numPrelabeledNodes = numPrelabelVersions = 0;
    relianceTime = prelabelingTime = meldLabelingTime = meldMappingTime = versionPropTime = 0.0;
    // We'll grab vPtD in initialize.
}

void VersionedFlowSensitive::initialize()
{
    FlowSensitive::initialize();
    // Overwrite the stat FlowSensitive::initialize gave us.
    delete stat;
    stat = new VersionedFlowSensitiveStat(this);

    vPtD = getVersionedPTDataTy();

    prelabel();
    meldLabel();
    mapMeldVersions();

    determineReliance();
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
        const SVFGNode *sn = it->second;

        if (const StoreSVFGNode *stn = SVFUtil::dyn_cast<StoreSVFGNode>(sn))
        {
            // l: *p = q.
            // If p points to o (Andersen's), l yields a new version for o.
            NodeID p = stn->getPAGDstNodeID();
            for (NodeID o : ander->getPts(p))
            {
                meldYield[l][o] = newMeldVersion(o);
            }

            vWorklist.push(l);

            if (ander->getPts(p).count() != 0)
            {
                ++numPrelabeledNodes;
            }
        }
        else if (delta(l))
        {
            // The outgoing edges are not only what will later be propagated. SVFGOPT may
            // move around nodes such that there can be an MRSVFGNode with no incoming or
            // outgoing edges which will be added at runtime. In essence, we can no
            // longer rely on the outgoing edges of a delta node when SVFGOPT is enabled.
            const MRSVFGNode *mr = SVFUtil::dyn_cast<MRSVFGNode>(sn);
            if (mr != nullptr)
            {
                for (const NodeID o : mr->getPointsTo())
                {
                    meldConsume[l][o] = newMeldVersion(o);
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

void VersionedFlowSensitive::meldLabel(void) {
    double start = stat->getClk(true);

    while (!vWorklist.empty()) {
        NodeID l = vWorklist.pop();
        const SVFGNode *sl = svfg->getSVFGNode(l);

        // Propagate l's y to lp's c for all l --o--> lp.
        for (const SVFGEdge *e : sl->getOutEdges()) {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;

            NodeID lp = ie->getDstNode()->getId();
            // Delta nodes had c set already and they are permanent.
            if (delta(lp)) continue;

            bool lpIsStore = SVFUtil::isa<StoreSVFGNode>(svfg->getSVFGNode(lp));
            // Consume and yield are the same for non-stores, so ignore them.
            if (l == lp && !lpIsStore) continue;

            // For stores, yield != consume, otherwise they are the same.
            ObjToMeldVersionMap &myl = SVFUtil::isa<StoreSVFGNode>(sl) ? meldYield[l]
                                                                       : meldConsume[l];
            ObjToMeldVersionMap &mclp = meldConsume[lp];
            bool yieldChanged = false;
            for (NodeID o : ie->getPointsTo()) {
                if (myl.find(o) == myl.end()) continue;
                // Yield == consume for non-stores, so when consume is updated, so is yield.
                // For stores, yield was already set, and it's static.
                yieldChanged = (meld(mclp[o], myl[o]) && !lpIsStore) || yieldChanged;
            }

            if (yieldChanged) vWorklist.push(lp);
        }
    }

    double end = stat->getClk(true);
    meldLabelingTime = (end - start) / TIMEINTERVAL;
}

bool VersionedFlowSensitive::meld(MeldVersion &mv1, MeldVersion &mv2)
{
    // Meld operator is union of bit vectors.
    return mv1 |= mv2;
}


void VersionedFlowSensitive::mapMeldVersions(void)
{
    double start = stat->getClk(true);

    // We want to uniquely map MeldVersions (SparseBitVectors) to a Version (unsigned integer).
    // mvv keeps track, and curVersion is used to generate new Versions.
    Map<MeldVersion, Version> mvv;
    Version curVersion = 1;

    // meldConsume -> consume.
    for (LocMeldVersionMap::value_type &lomv : meldConsume)
    {
        NodeID l = lomv.first;
        bool isStore = SVFUtil::isa<StoreSVFGNode>(svfg->getSVFGNode(l));
        ObjToVersionMap &consumel = consume[l];
        ObjToVersionMap &yieldl = yield[l];
        for (ObjToMeldVersionMap::value_type &omv : lomv.second)
        {
            NodeID o = omv.first;
            MeldVersion &mv = omv.second;

            Map<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
            // If a mapping for foudnVersion exists, use it, otherwise create a new Version,
            // keep track of it, and use that.
            Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
            consumel[o] = v;
            // At non-stores, consume == yield.
            if (!isStore) {
                yieldl[o] = v;
            }
        }
    }

    // meldYield -> yield.
    for (LocMeldVersionMap::value_type &lomv : meldYield)
    {
        NodeID l = lomv.first;
        ObjToVersionMap &yieldl = yield[l];
        for (ObjToMeldVersionMap::value_type &omv : lomv.second)
        {
            NodeID o = omv.first;
            MeldVersion &mv = omv.second;

            Map<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
            Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
            yieldl[o] = v;
        }
    }

    // No longer necessary.
    meldConsume.clear();
    meldYield.clear();

    double end = stat->getClk(true);
    meldMappingTime += (end - start) / TIMEINTERVAL;
}

bool VersionedFlowSensitive::delta(NodeID l) const
{
    // Whether a node is a delta node or not. Decent boon to performance.
    static Map<NodeID, bool> deltaCache;

    Map<NodeID, bool>::const_iterator isDeltaIt = deltaCache.find(l);
    if (isDeltaIt != deltaCache.end()) return isDeltaIt->second;

    const SVFGNode *s = svfg->getSVFGNode(l);
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
    }
    else if (const CallBlockNode *cbn = svfg->isCallSiteRetSVFGNode(s))
    {
        isDelta = cbn->isIndirectCall();
    }

    deltaCache[l] = isDelta;
    return isDelta;
}


VersionedFlowSensitive::MeldVersion VersionedFlowSensitive::newMeldVersion(NodeID o)
{
    ++numPrelabelVersions;
    MeldVersion nv;
    nv.set(++meldVersions[o]);
    return nv;
}

bool VersionedFlowSensitive::hasVersion(NodeID l, NodeID o, enum VersionType v) const
{
    // Choose which map we are checking.
    const LocVersionMap &m = v == CONSUME ? consume : yield;
    const LocVersionMap::const_iterator ml = m.find(l);
    if (ml == m.end()) return false;
    return ml->second.find(o) != ml->second.end();
}

void VersionedFlowSensitive::determineReliance(void)
{
    double start = stat->getClk(true);
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        NodeID l = it->first;
        const SVFGNode *sn = it->second;
        for (const SVFGEdge *e : sn->getOutEdges())
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;

            ObjToVersionMap &yieldl = yield[l];
            for (NodeID o : ie->getPointsTo())
            {
                // Given l --o--> lp, c(o) at lp relies on y(o) at l.
                NodeID lp = ie->getDstNode()->getId();

                if (!hasVersion(l, o, YIELD)) continue;
                Version &y = yieldl[o];

                if (!hasVersion(lp, o, CONSUME)) continue;
                Version &cp = consume[lp][o];
                if (cp != y)
                {
                    versionReliance[o][y].insert(cp);
                }
            }
        }

        // When an object/version points-to set changes, these nodes need to know.
        if (SVFUtil::isa<LoadSVFGNode>(sn) || SVFUtil::isa<StoreSVFGNode>(sn))
        {
            for (ObjToVersionMap::value_type &ov : consume[l])
            {
                NodeID o = ov.first;
                Version &v = ov.second;
                stmtReliance[o][v].set(l);
            }
        }
    }

    double end = stat->getClk(true);
    relianceTime = (end - start) / TIMEINTERVAL;
}

void VersionedFlowSensitive::propagateVersion(NodeID o, Version v)
{
    double start = stat->getClk();

    Map<Version, Set<Version>>::iterator relyingVersions = versionReliance[o].find(v);
    if (relyingVersions != versionReliance[o].end())
    {
        const VersionedVar srcVar = atKey(o, v);
        for (Version r : relyingVersions->second)
        {
            const VersionedVar dstVar = atKey(o, r);
            if (vPtD->unionPts(dstVar, srcVar))
            {
                // o/r has changed.
                // Add the dummy node to propagate it.
                VarToPropNodeMap::const_iterator dvpIt = versionedVarToPropNode.find(dstVar);
                const DummyVersionPropSVFGNode *dvp = nullptr;
                if (dvpIt == versionedVarToPropNode.end())
                {
                    dvp = svfg->addDummyVersionPropSVFGNode(o, r);
                    versionedVarToPropNode[dstVar] = dvp;
                } else dvp = dvpIt->second;

                assert(dvp != nullptr && "VFS::propagateVersion: propagation dummy node not found?");
                pushIntoWorklist(dvp->getId());

                // Notify nodes which rely on o/r that it changed.
                for (NodeID s : stmtReliance[o][r])
                {
                    pushIntoWorklist(s);
                }
            }
        }
    }

    double end = stat->getClk();
    versionPropTime += (end - start) / TIMEINTERVAL;
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
    else if (processSVFGNode(sn)) propagate(&sn);
}

void VersionedFlowSensitive::updateConnectedNodes(const SVFGEdgeSetTy& newEdges)
{
    for (const SVFGEdge *e : newEdges)
    {
        SVFGNode *dstNode = e->getDstNode();
        NodeID src = e->getSrcNode()->getId();
        NodeID dst = dstNode->getId();

        assert(delta(dst) && "VFS::updateConnectedNodes: new edges should be to delta nodes!");

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

            const PointsTo &ept = ie->getPointsTo();
            // For every o, such that src --o--> dst, we need to set up reliance (and propagate).
            for (NodeID o : ept)
            {
                if (!hasVersion(src, o, YIELD)) continue;
                Version &srcY = yield[src][o];
                if (!hasVersion(dst, o, CONSUME)) continue;
                Version &dstC = consume[dst][o];

                Set<Version> &versionsRelyingOnSrcY = versionReliance[o][srcY];
                if (versionsRelyingOnSrcY.find(dstC) == versionsRelyingOnSrcY.end())
                {
                    versionsRelyingOnSrcY.insert(dstC);
                    propagateVersion(o, srcY);
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

        if (hasVersion(l, o, CONSUME) && vPtD->unionPts(p, atKey(o, consume[l][o])))
        {
            changed = true;
        }

        if (isFieldInsensitive(o))
        {
            /// If o is a field-insensitive object, we should also get all field nodes'
            /// points-to sets and pass them to p.
            const NodeBS& fields = getAllFieldsObjNode(o);
            for (NodeID of : fields)
            {
                if (hasVersion(l, of, CONSUME) && vPtD->unionPts(p, atKey(of, consume[l][of])))
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

            if (hasVersion(l, o, YIELD) && vPtD->unionPts(atKey(o, yield[l][o]), q))
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
    ObjToVersionMap &yieldL = yield[l];
    ObjToVersionMap &consumeL = consume[l];
    for (ObjToVersionMap::value_type &oc : consumeL)
    {
        NodeID o = oc.first;
        Version c = oc.second;

        // Strong-updated; don't propagate.
        if (isSU && o == singleton) continue;

        if (!hasVersion(l, o, YIELD)) continue;
        Version y = yieldL[o];
        if (vPtD->unionPts(atKey(o, y), atKey(o, c)))
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
        ObjToVersionMap &yieldl = yield[l];
        for (NodeID o : changedObjects)
        {
            // Definitely has a yielded version (came from prelabelling) as these are
            // the changed objects which must've been pointed to in Andersen's too.
            const Version y = yieldl[o];
            propagateVersion(o, y);

            // Some o/v pairs changed: statements need to know.
            for (NodeID s : stmtReliance[o][y])
            {
                pushIntoWorklist(s);
            }
        }
    }

    return changed;
}

void VersionedFlowSensitive::dumpReliances(void) const
{
    SVFUtil::outs() << "# Version reliances\n";
    for (const Map<NodeID, Map<Version, Set<Version>>>::value_type ovrv : versionReliance)
    {
        NodeID o = ovrv.first;
        SVFUtil::outs() << "  Object " << o << "\n";
        for (const Map<Version, Set<Version>>::value_type vrv : ovrv.second)
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
            if (lvm->find(loc) == lvm->end() || lvm->at(loc).empty()) continue;
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
