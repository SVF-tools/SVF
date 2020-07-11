//===- FlowSensitivePlaceholder.cpp -- Sparse flow-sensitive pointer analysis------------//

/*
 * FlowSensitivePlaceholder.cpp
 *
 *  Created on: Jun 26, 2020
 *      Author: Mohamad Barbar
 */

#include "WPA/Andersen.h"
#include "WPA/FlowSensitivePlaceholder.h"
#include <iostream>

void FlowSensitivePlaceholder::initialize(SVFModule* svfModule)
{
    FlowSensitive::initialize(svfModule);

    vPtD = getVDFPTDataTy();
    assert(vPtD && "FSPH::initialize: Expected VDFPTData");

    double start = stat->getClk();
    precolour();
    double end = stat->getClk();
    double prec = (end - start) / TIMEINTERVAL;

    start = stat->getClk();
    colour();
    end = stat->getClk();
    double col = (end - start) / TIMEINTERVAL;

    printf("precolour: %fs, colour: %fs\n", prec, col);

    vPtD->setConsume(consume);
    vPtD->setYield(yield);
}

void FlowSensitivePlaceholder::finalize()
{
    printf("DONE! TODO"); fflush(stdout);
    exit(0);
}

void FlowSensitivePlaceholder::precolour(void)
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
                yield[l][o] = newVersion(o);
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
                    consume[l][o] = newVersion(o);
                }

                vWorklist.push(l);
            }
        }
    }
}

void FlowSensitivePlaceholder::colour(void) {
    unsigned loops = 0;
    while (!vWorklist.empty()) {
        NodeID l = vWorklist.pop();
        const SVFGNode *sl = svfg->getSVFGNode(l);

        for (const SVFGEdge *e : sl->getOutEdges()) {
            const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
            if (!ie) continue;
            for (NodeID o : ie->getPointsTo()) {
                NodeID lp = ie->getDstNode()->getId();
                if (meld(consume[lp][o], yield[l][o])) {
                    const SVFGNode *slp = svfg->getSVFGNode(lp);
                    // No need to do anything for delta/store because their yield is set and static.
                    if (!SVFUtil::isa<StoreSVFGNode>(slp) && !delta(lp)) {
                        yield[lp][o] = consume[lp][o];
                        vWorklist.push(lp);
                    }
                }
            }
        }
    }
}

bool FlowSensitivePlaceholder::meld(Version &v1, Version &v2)
{
    // Meld operator is union of bit vectors.
    return v1 |= v2;
}

bool FlowSensitivePlaceholder::delta(NodeID l) const
{
    const SVFGNode *s = svfg->getSVFGNode(l);
    // fsph-TODO: double check.
    return SVFUtil::isa<FormalINSVFGNode>(s) || SVFUtil::isa<ActualOUTSVFGNode>(s);
}

/// Returns a new version for o.
Version FlowSensitivePlaceholder::newVersion(NodeID o)
{
    Version nv;
    nv.set(++versions[o]);
    return nv;
}

bool FlowSensitivePlaceholder::hasVersion(NodeID l, NodeID o, enum VersionType v) const
{
    // Choose which map we are checking.
    const DenseMap<NodeID, DenseMap<NodeID, Version>> &m = v == CONSUME ? consume : yield;
    const DenseMap<NodeID, Version> &ml = m.lookup(l);
    return ml.find(o) != ml.end();
}

void FlowSensitivePlaceholder::determineReliance(void)
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

        if (SVFUtil::isa<LoadSVFGNode>(sn))
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

void FlowSensitivePlaceholder::propagateVersion(NodeID o, Version v)
{
    for (Version r : versionReliance[o][v])
    {
        if (vPtD->updateATVersion(o, r, v))
        {
            propagateVersion(o, r);
            for (NodeID s : stmtReliance[o][r])
            {
                pushIntoWorklist(s);
            }
        }
    }
}

void FlowSensitivePlaceholder::processNode(NodeID n)
{
    SVFGNode* sn = svfg->getSVFGNode(n);
    if (processSVFGNode(sn)) propagate(&sn);
}

bool FlowSensitivePlaceholder::processLoad(const LoadSVFGNode* load)
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

bool FlowSensitivePlaceholder::processStore(const StoreSVFGNode* store)
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

    if (vPtD->propWithinLoc(l, isSU, singleton, changedObjects))
    {
        changed = true;
        for (NodeID o : changedObjects)
        {
            // The yielded version changed because propWithinLoc went c -> y.
            propagateVersion(o, yield[l][o]);
        }
    }

    double updateEnd = stat->getClk();
    updateTime += (updateEnd - updateStart) / TIMEINTERVAL;

    return changed;
}
