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
            for (const SVFGEdge *e : sn->getOutEdges())
            {
                const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e);
                if (!ie) continue;
                for (NodeID o : ie->getPointsTo())
                {
                    yield[l][o] = newVersion(o);
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
                NodeID lp = ie->getDstNode()->getId();
                Version &y = yield[l][o];
                Version &yp = yield[lp][o];
                if (yp != y)
                {
                    versionReliance[o][&y].insert(&yp);
                }
            }
        }

        if (SVFUtil::isa<LoadSVFGNode>(sn))
        {
            for (DenseMap<NodeID, Version>::value_type &ov : consume[l])
            {
                NodeID o = ov.first;
                Version &v = ov.second;
                stmtReliance[{o, &v}].insert(l);
            }
        }
    }
}

bool FlowSensitivePlaceholder::processLoad(const LoadSVFGNode* load)
{
    // fsph-TODO!
    return false;
}

bool FlowSensitivePlaceholder::processStore(const StoreSVFGNode* store)
{
    // fsph-TODO!
    return false;
}
