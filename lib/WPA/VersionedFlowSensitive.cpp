//===- VersionedFlowSensitive.cpp -- Sparse flow-sensitive pointer analysis------------//

/*
 * VersionedFlowSensitive.cpp
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#include "WPA/Andersen.h"
#include "WPA/VersionedFlowSensitive.h"
#include <iostream>

void VersionedFlowSensitive::initialize()
{
    FlowSensitive::initialize();

    vPtD = getVDFPTDataTy();
    assert(vPtD && "VFS::initialize: Expected VDFPTData");

    double start = stat->getClk();
    precolour();
    double end = stat->getClk();
    double prec = (end - start) / TIMEINTERVAL;

    start = stat->getClk();
    colour();
    end = stat->getClk();
    double col = (end - start) / TIMEINTERVAL;

    start = stat->getClk();
    mapMeldVersions(meldConsume, consume);
    mapMeldVersions(meldYield, yield);
    end = stat->getClk();
    double map = (end - start) / TIMEINTERVAL;

    printf("precolour: %fs, colour: %fs, map: %fs\n", prec, col, map);
    //exit(0);

    determineReliance();

    vPtD->setConsume(&consume);
    vPtD->setYield(&yield);
}

void VersionedFlowSensitive::finalize()
{
    printf("DONE! TODO"); fflush(stdout);
    exit(0);
}

void VersionedFlowSensitive::precolour(void)
{
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        NodeID l = it->first;
        const SVFGNode *sn = it->second;

        if (const StoreSVFGNode *stn = SVFUtil::dyn_cast<StoreSVFGNode>(sn))
        {
            NodeID p = stn->getPAGDstNodeID();
            for (NodeID o : ander->getPts(p))
            {
                meldYield[l][o] = newMeldVersion(o);
            }

            vWorklist.push(l);
        }
        else if (delta(l))
        {
            // If l may change at runtime (new incoming edges), it's unknown whether
            // a new consume version is required, so we give it one in case.
            for (const SVFGEdge *e : sn->getOutEdges())
            {
                const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
                if (!ie) continue;
                for (NodeID o : ie->getPointsTo())
                {
                    meldConsume[l][o] = newMeldVersion(o);
                    // It's yield will be the same; deltas are mutually exclusive with stores.
                    meldYield[l][o] = meldConsume[l][o];
                }

                vWorklist.push(l);
            }
        }
    }
}

void VersionedFlowSensitive::colour(void) {
    unsigned loops = 0;
    while (!vWorklist.empty()) {
        NodeID l = vWorklist.pop();
        const SVFGNode *sl = svfg->getSVFGNode(l);

        // Propagate l's y to lp's c for all l --o--> lp.
        for (const SVFGEdge *e : sl->getOutEdges()) {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;
            for (NodeID o : ie->getPointsTo()) {
                NodeID lp = ie->getDstNode()->getId();
                if (meld(meldConsume[lp][o], meldYield[l][o])) {
                    const SVFGNode *slp = svfg->getSVFGNode(lp);
                    // No need to do anything for store because their yield is set and static.
                    if (!SVFUtil::isa<StoreSVFGNode>(slp)) {
                        meldYield[lp][o] = meldConsume[lp][o];
                        vWorklist.push(lp);
                    }
                }
            }
        }
    }
}

bool VersionedFlowSensitive::meld(MeldVersion &mv1, MeldVersion &mv2)
{
    // Meld operator is union of bit vectors.
    return mv1 |= mv2;
}


void VersionedFlowSensitive::mapMeldVersions(DenseMap<NodeID, DenseMap<NodeID, MeldVersion>> &from,
                                             DenseMap<NodeID, DenseMap<NodeID, Version>> &to)
{
    // We want to uniquely map MeldVersions (SparseBitVectors) to a Version (unsigned integer).
    // mvv keeps track, and curVersion is used to generate new Versions.
    static DenseMap<MeldVersion, Version> mvv;
    static Version curVersion = 0;

    for (DenseMap<NodeID, DenseMap<NodeID, MeldVersion>>::value_type &lomv : from)
    {
        NodeID l = lomv.first;
        DenseMap<NodeID, Version> &tol = to[l];
        for (DenseMap<NodeID, MeldVersion>::value_type &omv : lomv.second)
        {
            NodeID o = omv.first;
            MeldVersion &mv = omv.second;

            DenseMap<MeldVersion, Version>::const_iterator foundVersion = mvv.find(mv);
            Version v = foundVersion == mvv.end() ? mvv[mv] = ++curVersion : foundVersion->second;
            tol[o] = v;
        }
    }

    // Don't need the from anymore.
    from.clear();
}

bool VersionedFlowSensitive::delta(NodeID l) const
{
    const SVFGNode *s = svfg->getSVFGNode(l);
    // vfs-TODO: double check.
    return SVFUtil::isa<FormalINSVFGNode>(s) || SVFUtil::isa<ActualOUTSVFGNode>(s);
}

/// Returns a new version for o.
MeldVersion VersionedFlowSensitive::newMeldVersion(NodeID o)
{
    MeldVersion nv;
    nv.set(++meldVersions[o]);
    return nv;
}

bool VersionedFlowSensitive::hasVersion(NodeID l, NodeID o, enum VersionType v) const
{
    // Choose which map we are checking.
    const DenseMap<NodeID, DenseMap<NodeID, Version>> &m = v == CONSUME ? consume : yield;
    const DenseMap<NodeID, Version> &ml = m.lookup(l);
    return ml.find(o) != ml.end();
}

void VersionedFlowSensitive::determineReliance(void)
{
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        NodeID l = it->first;
        const SVFGNode *sn = it->second;
        for (const SVFGEdge *e : sn->getOutEdges())
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;
            for (NodeID o : ie->getPointsTo())
            {
                // Given l --o--> lp, c at lp relies on y at l.
                NodeID lp = ie->getDstNode()->getId();
                Version &y = yield[l][o];
                Version &cp = consume[lp][o];
                if (cp != y)
                {
                    versionReliance[o][y].insert(cp);
                }
            }
        }

        if (SVFUtil::isa<LoadSVFGNode>(sn) || SVFUtil::isa<StoreSVFGNode>(sn))
        {
            for (DenseMap<NodeID, Version>::value_type &ov : consume[l])
            {
                NodeID o = ov.first;
                Version &v = ov.second;
                stmtReliance[o][v].insert(l);
            }
        }
    }
}

void VersionedFlowSensitive::propagateVersion(NodeID o, Version v)
{
    for (Version r : versionReliance[o][v])
    {
        if (vPtD->updateATVersion(o, r, v))
        {
            propagateVersion(o, r);
        }
    }

    for (NodeID s : stmtReliance[o][v])
    {
        pushIntoWorklist(s);
    }
}

void VersionedFlowSensitive::processNode(NodeID n)
{
    SVFGNode* sn = svfg->getSVFGNode(n);
    if (processSVFGNode(sn)) propagate(&sn);
}

void VersionedFlowSensitive::updateConnectedNodes(const SVFGEdgeSetTy& newEdges)
{
    for (const SVFGEdge *e : newEdges)
    {
        const SVFGNode *src = e->getSrcNode();
        const SVFGNode *dst = e->getDstNode();
        NodeID srcId = src->getId();
        NodeID dstId = dst->getId();

        if (SVFUtil::isa<PHISVFGNode>(dst))
        {
            pushIntoWorklist(dstId);
        }
        else if (SVFUtil::isa<FormalINSVFGNode>(dst) || SVFUtil::isa<ActualOUTSVFGNode>(dst))
        {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            assert(ie && "VFS::updateConnectedNodes: given direct edge?");
            bool changed = false;

            const PointsTo &ept = ie->getPointsTo();
            // For every o, such that src --o--> dst, we need to set up reliance (and propagate).
            for (NodeID o : ept)
            {
                // Nothing to propagate.
                if (yield[srcId].find(o) == yield[srcId].end()) continue;

                Version &srcY = yield[srcId][o];
                Version &dstC = consume[dstId][o];
                versionReliance[o][srcY].insert(dstC);
                propagateVersion(o, srcY);
            }

            if (changed) pushIntoWorklist(dst->getId());
        }
    }
}

bool VersionedFlowSensitive::processLoad(const LoadSVFGNode* load)
{
    double start = stat->getClk();

    bool changed = false;

    NodeID l = load->getId();
    NodeID p = load->getPAGDstNodeID();
    NodeID q = load->getPAGSrcNodeID();

    const PointsTo& qpt = getPts(q);
    for (NodeID o : qpt)
    {
        if (pag->isConstantObj(o) || pag->isNonPointerObj(o)) continue;

        if (vPtD->unionTLFromAT(l, p, o))
        {
            changed = true;
        }

        if (isFIObjNode(o))
        {
            /// If o is a field-insensitive node, we should also get all field nodes'
            /// points-to sets and pass them to p.
            const NodeBS& fields = getAllFieldsObjNode(o);
            for (NodeID of : fields)
            {
                if (vPtD->unionTLFromAT(l, p, of))
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
    double start = stat->getClk();
    bool changed = false;
    // The version for these objects would be y_l(o).
    NodeBS changedObjects;

    if (!qpt.empty())
    {
        for (NodeID o : ppt)
        {
            if (pag->isConstantObj(o) || pag->isNonPointerObj(o)) continue;

            if (vPtD->unionATFromTL(l, q, o))
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

    changed = vPtD->propWithinLoc(l, isSU, singleton, changedObjects) || changed;

    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    // TODO: time.
    for (NodeID o : changedObjects)
    {
        propagateVersion(o, yield[l][o]);
    }

    return changed;
}
