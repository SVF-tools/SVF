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

    static const PointsToID emptyPointsToId = 0;

public:
    PersistentPointsToCache(void) : idCounter(1) { }

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
        return *foundPts;
    }

    /// Unions id1 and id2 and returns their union's ID.
    PointsToID unionPts(PointsToID id1, PointsToID id2)
    {
        std::pair<PointsToID, PointsToID> desiredUnion = std::minmax(id1, id2);

        // Check if we have performed this union before.
        Map<std::pair<PointsToID, PointsToID>, PointsToID>::const_iterator foundResult = unionCache.find(desiredUnion);
        if (foundResult != unionCache.end()) return foundResult->second;

        const Data &pts1 = getActualPts(id1);
        const Data &pts2 = getActualPts(id2);

        // TODO: may be faster to unravel | to sometimes avoid hashing a points-to set.
        Data actualUnion = pts1 | pts2;

        PointsToID unionId = 0;
        // Intern points-to set: check if actualUnion already exists.
        typename PTSToIDMap::const_iterator foundId = ptsToId.find(actualUnion);
        if (foundId != ptsToId.end()) unionId = *foundId;
        else
        {
            unionId = newPointsToId();
            idToPts[unionId] = actualUnion;
            ptsToId[actualUnion] = unionId;
        }

        // Cache the union, for hash-consing.
        unionCache[std::minmax(id1, id2)] = unionId;

        return unionId;
    }

    // TODO: ref count API for garbage collection.

private:
    PointsToID newPointsToId(void)
    {
        ++idCounter;
        // Make sure we don't overflow.
        assert(idCounter != emptyPointsToId && "PPTC::newPointsToId: PointsToIDs exhausted! Try a larger type.");
        return idCounter;
    }

private:
    /// Maps points-to IDs to their corresponding points-to set.
    /// Reverse of idToPts.
    IDToPTSMap idToPts;
    /// Maps points-to sets to their corresponding ID.
    PTSToIDMap ptsToId;

    // TODO: an unordered pair type may be better.
    /// Maps two IDs to their union. Keys must be sorted.
    Map<std::pair<PointsToID, PointsToID>, PointsToID> unionCache;

    /// Used to generate new PointsToIDs. Any non-zero is valid.
    PointsToID idCounter;
};

} // End namespace SVF

#endif /* PERSISTENT_POINTS_TO_H_ */
