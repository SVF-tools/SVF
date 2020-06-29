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
    distributeVersions();
    exit(0);
}

void FlowSensitivePlaceholder::distributeVersions(void)
{
    double start = stat->getClk();
    for (SVFG::iterator it = svfg->begin(); it != svfg->end(); ++it)
    {
        const SVFGNode *s = it->second;
        if (s->getInEdges().size() == 0 && s->getOutEdges().size() == 0) continue;
        setVersions(s);
    }
    double end = stat->getClk();
    printf("distr time: %.6f\n", (end - start) / TIMEINTERVAL);
}

void FlowSensitivePlaceholder::setVersions(const SVFGNode *s)
{
    // fsph-TODO: deal with delta!
    // Why did we recurse into this call? For <l, o>.
    static DenseSet<std::pair<NodeID, NodeID>> recurseReasons;
    static NodeBS versionsSet;
    if (versionsSet.test(s->getId())) return;

    NodeID l = s->getId();
    DenseMap<NodeID, Version> consumel = consume[l];
    DenseMap<NodeID, Version> yieldl = yield[l];

    // fsph-TODO: is it possible to have l'-o->l and l'-o->l in TWO edges, both from l'?
    // Set consumed version.
    // object -> source. Not a set because we only care about objects with a single source.
    DenseMap<NodeID, NodeID> sources;
    PointsTo multiplyIncomingObjects;
    PointsTo allIncomingObjects;
    for (const SVFGEdge *e : s->getInEdges())
    {
        if (const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e))
        {
            // Specifically don't use ref. to not modify.
            const PointsTo &ept = ie->getPointsTo();
            // A multiply incoming object is any object which is already in the every growing list
            // of all incoming objects, hence, ept & allIncomingObjects.
            multiplyIncomingObjects |= ept & allIncomingObjects;
            allIncomingObjects |= ept;

            for (NodeID o : ept)
            {
                sources[o] = ie->getSrcNode()->getId();
            }
        }
    }

    for (NodeID o : multiplyIncomingObjects)
    {
        // [UNION].
        consumel[o] = newVersion(o);
    }

    // Everything that occurs only once relies on its predecessor, unless it is its own pred.
    PointsTo singlyIncomingObjects = allIncomingObjects - multiplyIncomingObjects;
    for (NodeID o : singlyIncomingObjects)
    {
        assert(sources.find(o) != sources.end() && "FSPH: no source set for incoming object.");
        NodeID lp = sources[o];

        if (lp == l)
        {
            // [SELF-LOOP].
            consumel[o] = newVersion(o);
        }
        else
        {
            if (!hasVersion(lp, o, YIELD))
            {
                std::pair<NodeID, NodeID> reason = std::make_pair(lp, o);
                if (recurseReasons.find(reason) == recurseReasons.end())
                {
                    recurseReasons.insert(reason);
                    setVersions(svfg->getSVFGNode(lp));
                    recurseReasons.erase(reason);
                }
                else
                {
                    // [???]: fsph-TODO: may be unnecessary depending on "store-like" nodes.
                    yield[lp][o] = newVersion(o);
                }
            }

            // [PROP-YIELDED].
            consumel[o] = yield[lp][o];
        }
    }

    // Set yielded version.
    if (const StoreSVFGNode *store = SVFUtil::dyn_cast<StoreSVFGNode>(s))
    {
        NodeID p = store->getPAGDstNodeID();
        for (NodeID o : ander->getPts(p))
        {
            // [WRITE].
            yieldl[o] = newVersion(o);
        }
    }
    else
    {
        for (const SVFGEdge *e : s->getOutEdges())
        {
            if (const IndirectSVFGEdge *ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(e))
            {
                const PointsTo &ept = ie->getPointsTo();
                for (NodeID o : ept)
                {
                    // [PROP-UNWRITTEN]/[PROP-CONSUMED].
                    yieldl[o] = consumel[o];
                }
            }
        }
    }

    versionsSet.set(s->getId());
}

/// Returns a new version for o.
Version FlowSensitivePlaceholder::newVersion(NodeID o)
{
    ++versions[o];
    return versions[o];
}

bool FlowSensitivePlaceholder::hasVersion(NodeID l, NodeID o, enum VersionType v)
{
    // Choose which map we are checking.
    DenseMap<NodeID, DenseMap<NodeID, Version>> &m = v == CONSUME ? consume : yield;
    return m[l].find(o) != m[l].end();
}
