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

static unsigned wouldbev = 0;
void FlowSensitivePlaceholder::initialize(SVFModule* svfModule)
{
    FlowSensitive::initialize(svfModule);
    precolour();
    colour();

    unsigned v = 0;
    for (DenseMap<NodeID, Version>::value_type nv : versions) {
        v += nv.second;
    }

    printf("versions: %u, would be AT sets: %u\n", v, wouldbev);

    exit(0);
}

void FlowSensitivePlaceholder::precolour(void)
{

}

void FlowSensitivePlaceholder::colour(void)
{

}

/// Returns a new version for o.
Version FlowSensitivePlaceholder::newVersion(NodeID o)
{
    ++versions[o];
    return versions[o];
}

bool FlowSensitivePlaceholder::hasVersion(NodeID l, NodeID o, enum VersionType v) const
{
    // Choose which map we are checking.
    const DenseMap<NodeID, DenseMap<NodeID, Version>> &m = v == CONSUME ? consume : yield;
    const DenseMap<NodeID, Version> &ml = m.lookup(l);
    return ml.find(o) != ml.end();
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
