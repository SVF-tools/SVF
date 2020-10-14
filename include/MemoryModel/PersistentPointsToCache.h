//===- PersistentPointsToCache.h -- Persistent points-to sets ----------------//

/*
 * PersistentPointsToCache.h
 *
 *  Persistent, hash-consed points-to sets
 *
 *  Created on: Sep 28, 2020
 *      Author: Mohamad Barbar
 */

#ifndef PERSISTENT_POINTS_TO_H_
#define PERSISTENT_POINTS_TO_H_

#include "Util/SVFBasicTypes.h"

namespace SVF
{

/// Persistent points-to set store. Can be used as a backing for points-to data structures like
/// PointsToDS and PointsToDFDS. Hides points-to sets and union operations from users and hands
/// out PointsToIDs.
/// Points-to sets are interned, and union operations are lazy and hash-consed.
template <typename Data>
class PersistentPointsToCache
{
public:
    typedef Map<PointsToID, Data> IDToPTSMap;
    typedef Map<Data, PointsToID> PTSToIDMap;
    typedef std::function<Data(const Data &, const Data &)> DataOp;
    // TODO: an unordered pair type may be better.
    typedef Map<std::pair<PointsToID, PointsToID>, PointsToID> OpCache;

    static PointsToID emptyPointsToId(void) { return 0; };

public:
    PersistentPointsToCache(const Data &emptyData) : idCounter(1)
    {
        idToPts[emptyPointsToId()] = emptyData;
        ptsToId[emptyData] = emptyPointsToId();
    }

    /// If pts is not in the PersistentPointsToCache, inserts it, assigns an ID, and returns
    /// that ID. If it is, then the ID is returned.
    PointsToID emplacePts(const Data &pts)
    {
        // Is it already in the cache?
        typename PTSToIDMap::const_iterator foundId = ptsToId.find(pts);
        if (foundId != ptsToId.end()) return foundId->second;

        // Otherwise, insert it.
        PointsToID id = newPointsToId();
        idToPts[id] = pts;
        ptsToId[pts] = id;

        return id;
    }

    /// Returns the points-to set which id represents. id must be stored in the cache.
    const Data &getActualPts(PointsToID id) const
    {
        // Check if the points-to set for ID has already been stored.
        typename IDToPTSMap::const_iterator foundPts = idToPts.find(id);
        assert(foundPts != idToPts.end() && "PPTC::getActualPts: points-to set not stored!");
        return foundPts->second;
    }

    /// Unions lhs and rhs and returns their union's ID.
    PointsToID unionPts(PointsToID lhs, PointsToID rhs)
    {
        static const DataOp unionOp = [](const Data &lhs, const Data &rhs) { return lhs | rhs; };
        std::pair<PointsToID, PointsToID> desiredUnion = std::minmax(lhs, rhs);

        // Trivial cases.
        // EMPTY_SET U x
        if (desiredUnion.first == emptyPointsToId()) return desiredUnion.second;
        // x U x
        if (desiredUnion.first == desiredUnion.second) return desiredUnion.first;

        return opPts(lhs, rhs, unionOp, unionCache, true);
    }

    /// Relatively complements lhs and rhs (lhs \ rhs) and returns it's ID.
    PointsToID complementPts(PointsToID lhs, PointsToID rhs)
    {
        static const DataOp complementOp = [](const Data &lhs, const Data &rhs) { return lhs - rhs; };

        // Trivial cases.
        // x - x
        if (lhs == rhs) return emptyPointsToId();
        // x - EMPTY_SET
        if (rhs == emptyPointsToId()) return lhs;
        // EMPTY_SET - x
        if (lhs == emptyPointsToId()) return emptyPointsToId();

        return opPts(lhs, rhs, complementOp, complementCache, false);
    }

    /// Intersects lhs and rhs (lhs AND rhs) and returns the intersection's ID.
    PointsToID intersectPts(PointsToID lhs, PointsToID rhs)
    {
        static const DataOp intersectionOp = [](const Data &lhs, const Data &rhs) { return lhs & rhs; };
        std::pair<PointsToID, PointsToID> operands = std::minmax(lhs, rhs);

        // Trivial cases.
        // EMPTY_SET & x
        if (operands.first == emptyPointsToId()) return emptyPointsToId();
        // x & x
        if (operands.first == operands.second) return operands.first;

        return opPts(lhs, rhs, intersectionOp, intersectionCache, true);
    }

    // TODO: ref count API for garbage collection.

private:
    PointsToID newPointsToId(void)
    {
        ++idCounter;
        // Make sure we don't overflow.
        assert(idCounter != emptyPointsToId() && "PPTC::newPointsToId: PointsToIDs exhausted! Try a larger type.");
        return idCounter;
    }

    /// Performs dataOp on lhs and rhs, checking the opCache first and updating it afterwards.
    /// commutative indicates whether the operation in question is commutative or not.
    inline PointsToID opPts(PointsToID lhs, PointsToID rhs, const DataOp &dataOp, OpCache &opCache, bool commutative)
    {
        std::pair<PointsToID, PointsToID> operands;
        // If we're commutative, we want to always perform the same operation: x op y.
        // Performing x op y sometimes and y op x other times is a waste of time.
        if (commutative) operands = std::minmax(lhs, rhs);
        else operands = std::make_pair(lhs, rhs);

        // Check if we have performed this operation
        OpCache::const_iterator foundResult = opCache.find(operands);
        if (foundResult != opCache.end()) return foundResult->second;

        const Data &lhsPts = getActualPts(lhs);
        const Data &rhsPts = getActualPts(rhs);

        Data result = dataOp(lhsPts, rhsPts);

        PointsToID resultId;
        // Intern points-to set: check if actualResult already exists.
        typename PTSToIDMap::const_iterator foundId = ptsToId.find(result);
        if (foundId != ptsToId.end()) resultId = foundId->second;
        else
        {
            resultId = newPointsToId();
            idToPts[resultId] = result;
            ptsToId[result] = resultId;
        }

        // Cache the complement, for hash-consing.
        opCache[operands] = resultId;

        return resultId;
    }

private:
    /// Maps points-to IDs to their corresponding points-to set.
    /// Reverse of idToPts.
    IDToPTSMap idToPts;
    /// Maps points-to sets to their corresponding ID.
    PTSToIDMap ptsToId;

    /// Maps two IDs to their union. Keys must be sorted.
    OpCache unionCache;
    /// Maps two IDs to their relative complement.
    OpCache complementCache;
    /// Maps two IDs to their intersection. Keys must be sorted.
    OpCache intersectionCache;

    /// Used to generate new PointsToIDs. Any non-zero is valid.
    PointsToID idCounter;
};

} // End namespace SVF

#endif /* PERSISTENT_POINTS_TO_H_ */
