#include "MemoryModel/AbstractPointsToDS.h"

namespace SVF
{

void insertKey(const NodeID &key, NodeBS &keySet)
{
    keySet.set(key);
}

void insertKey(const CondVar<ContextCond> &key, Set<CondVar<ContextCond>> &keySet)
{
    keySet.insert(key);
}

void insertKey(const VersionedVar &key, Set<VersionedVar> &keySet)
{
    keySet.insert(key);
}

};
