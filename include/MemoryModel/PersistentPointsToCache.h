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

#include <iomanip>
#include <iostream>
#include <vector>

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
    typedef Map<Data, PointsToID> PTSToIDMap;
    typedef std::function<Data(const Data &, const Data &)> DataOp;
    // TODO: an unordered pair type may be better.
    typedef Map<std::pair<PointsToID, PointsToID>, PointsToID> OpCache;

    static PointsToID emptyPointsToId(void) { return 0; };

public:
    PersistentPointsToCache(void) : idCounter(1)
    {
        idToPts.push_back(new Data());
        ptsToId[Data()] = emptyPointsToId();

        initStats();
    }

    /// Clear the cache.
    void clear()
    {
      for (const Data *d : idToPts) delete d;
      idToPts.clear();
      ptsToId.clear();

      unionCache.clear();
      complementCache.clear();
      intersectionCache.clear();
    }

    /// Resets the cache removing everything except the emptyData it was initialised with.
    void reset(void)
    {
        clear();

        // Put the empty data back in.
        ptsToId[Data()] = emptyPointsToId();
        idToPts.push_back(new Data());

        idCounter = 1;
        // Cache is empty...
        initStats();
    }

    /// Remaps all points-to sets stored in the cache to the current mapping.
    void remapAllPts(void)
    {
        for (Data *d : idToPts) d->checkAndRemap();

        // Rebuild ptsToId from idToPts.
        ptsToId.clear();
        for (PointsToID i = 0; i < idToPts.size(); ++i) ptsToId[*idToPts[i]] = i;
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
        idToPts.push_back(new Data(pts));
        ptsToId[pts] = id;

        return id;
    }

    /// Returns the points-to set which id represents. id must be stored in the cache.
    const Data &getActualPts(PointsToID id) const
    {
        // Check if the points-to set for ID has already been stored.
        assert(idToPts.size() > id && "PPTC::getActualPts: points-to set not stored!");
        return *idToPts.at(id);
    }

    /// Unions lhs and rhs and returns their union's ID.
    PointsToID unionPts(PointsToID lhs, PointsToID rhs)
    {
        static const DataOp unionOp = [](const Data &lhs, const Data &rhs) { return lhs | rhs; };

        ++totalUnions;

        // Order operands so we don't perform x U y and y U x separately.
        std::pair<PointsToID, PointsToID> operands = std::minmax(lhs, rhs);

        // Property cases.
        // EMPTY_SET U x
        if (operands.first == emptyPointsToId())
        {
            ++propertyUnions;
            return operands.second;
        }

        // x U x
        if (operands.first == operands.second)
        {
            ++propertyUnions;
            return operands.first;
        }

        bool opPerformed = false;
        PointsToID result = opPts(lhs, rhs, unionOp, unionCache, true, opPerformed);

        if (opPerformed)
        {
            ++uniqueUnions;

            // We can use lhs/rhs here rather than our ordered operands,
            // because the operation was commutative.

            // if x U y = z, then x U z = z,
            if (lhs != result)
            {
                unionCache[std::minmax(lhs, result)] = result;
                ++preemptiveUnions;
                ++totalUnions;
            }

            // and y U z = z.
            if (rhs != result)
            {
                unionCache[std::minmax(rhs, result)] = result;
                ++preemptiveUnions;
                ++totalUnions;
            }
        } else ++lookupUnions;

        return result;
    }

    /// Relatively complements lhs and rhs (lhs \ rhs) and returns it's ID.
    PointsToID complementPts(PointsToID lhs, PointsToID rhs)
    {
        static const DataOp complementOp = [](const Data &lhs, const Data &rhs) { return lhs - rhs; };

        ++totalComplements;

        // Property cases.
        // x - x
        if (lhs == rhs)
        {
            ++propertyComplements;
            return emptyPointsToId();
        }

        // x - EMPTY_SET
        if (rhs == emptyPointsToId())
        {
            ++propertyComplements;
            return lhs;
        }

        // EMPTY_SET - x
        if (lhs == emptyPointsToId())
        {
            ++propertyComplements;
            return emptyPointsToId();
        }

        bool opPerformed = false;
        const PointsToID result = opPts(lhs, rhs, complementOp, complementCache, false, opPerformed);

        if (opPerformed)
        {
            ++uniqueComplements;

            // We performed lhs - rhs = result, so...
            if (result != emptyPointsToId())
            {
                // result AND rhs = EMPTY_SET,
                intersectionCache[std::minmax(result, rhs)] = emptyPointsToId();
                ++preemptiveIntersections;
                ++totalIntersections;

                // and result AND lhs = result,
                intersectionCache[std::minmax(result, lhs)] = result;
                ++preemptiveIntersections;
                ++totalIntersections;

                // and result - rhs = result.
                complementCache[std::make_pair(result, rhs)] = result;
                ++preemptiveComplements;
                ++totalComplements;
            }
        } else ++lookupComplements;

        return result;
    }

    /// Intersects lhs and rhs (lhs AND rhs) and returns the intersection's ID.
    PointsToID intersectPts(PointsToID lhs, PointsToID rhs)
    {
        static const DataOp intersectionOp = [](const Data &lhs, const Data &rhs) { return lhs & rhs; };

        ++totalIntersections;

        // Order operands so we don't perform x U y and y U x separately.
        std::pair<PointsToID, PointsToID> operands = std::minmax(lhs, rhs);

        // Property cases.
        // EMPTY_SET & x
        if (operands.first == emptyPointsToId())
        {
            ++propertyIntersections;
            return emptyPointsToId();
        }

        // x & x
        if (operands.first == operands.second)
        {
            ++propertyIntersections;
            return operands.first;
        }

        bool opPerformed = false;
        const PointsToID result = opPts(lhs, rhs, intersectionOp, intersectionCache, true, opPerformed);
        if (opPerformed)
        {
            ++uniqueIntersections;

            // When the result is empty, we won't be adding anything of substance.
            if (result != emptyPointsToId())
            {
                // We performed lhs AND rhs = result, so...
                // result AND rhs = result,
                if (result != rhs)
                {
                    intersectionCache[std::minmax(result, rhs)] = result;
                    ++preemptiveIntersections;
                    ++totalIntersections;
                }

                // and result AND lhs = result,
                if (result != lhs)
                {
                    intersectionCache[std::minmax(result, lhs)] = result;
                    ++preemptiveIntersections;
                    ++totalIntersections;
                }

                // Also (thanks reviewer #2)
                // result U lhs = result,
                if (result != emptyPointsToId() && result != lhs)
                {
                    unionCache[std::minmax(lhs, result)] = lhs;
                    ++preemptiveUnions;
                    ++totalUnions;
                }

                // And result U rhs = rhs.
                if (result != emptyPointsToId() && result != rhs)
                {
                    unionCache[std::minmax(rhs, result)] = rhs;
                    ++preemptiveUnions;
                    ++totalUnions;
                }
            }
        } else ++lookupIntersections;

        return result;
    }

    /// Print statistics on operations and points-to set numbers.
    void printStats(const std::string subtitle) const
    {
        static const unsigned fieldWidth = 25;
        SVFUtil::outs().flags(std::ios::left);

        SVFUtil::outs() << "****Persistent Points-To Cache Statistics: " << subtitle << "****\n";

        SVFUtil::outs() << std::setw(fieldWidth) << "UniquePointsToSets"      << idToPts.size()          << "\n";

        SVFUtil::outs() << std::setw(fieldWidth) << "TotalUnions"             << totalUnions             << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "PropertyUnions"          << propertyUnions          << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "UniqueUnions"            << uniqueUnions            << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "LookupUnions"            << lookupUnions            << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "PreemptiveUnions"        << preemptiveUnions        << "\n";

        SVFUtil::outs() << std::setw(fieldWidth) << "TotalComplements"        << totalComplements        << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "PropertyComplements"     << propertyComplements     << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "UniqueComplements"       << uniqueComplements       << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "LookupComplements"       << lookupComplements       << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "PreemptiveComplements"   << preemptiveComplements   << "\n";

        SVFUtil::outs() << std::setw(fieldWidth) << "TotalIntersections"      << totalIntersections      << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "PropertyIntersections"   << propertyIntersections   << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "UniqueIntersections"     << uniqueIntersections     << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "LookupIntersections"     << lookupIntersections     << "\n";
        SVFUtil::outs() << std::setw(fieldWidth) << "PreemptiveIntersections" << preemptiveIntersections << "\n";

        SVFUtil::outs().flush();
    }

    /// Returns all points-to sets stored by this cache as keys to a map.
    /// Values are all 1. We use the map to be more compatible with getAllPts
    /// in the various PTDatas. Performance is a non-issue (for now) since this
    /// is just used for evaluation's sake.
    Map<Data, unsigned> getAllPts(void)
    {
        Map<Data, unsigned> allPts;
        for (const Data *d : idToPts) allPts[*d] = 1;
        return allPts;
    }

    // TODO: ref count API for garbage collection.

private:
    PointsToID newPointsToId(void)
    {
        // Make sure we don't overflow.
        assert(idCounter != emptyPointsToId() && "PPTC::newPointsToId: PointsToIDs exhausted! Try a larger type.");
        return idCounter++;
    }

    /// Performs dataOp on lhs and rhs, checking the opCache first and updating it afterwards.
    /// commutative indicates whether the operation in question is commutative or not.
    /// opPerformed is set to true if the operation was *not* cached and thus performed, false otherwise.
    inline PointsToID opPts(PointsToID lhs, PointsToID rhs, const DataOp &dataOp, OpCache &opCache,
                            bool commutative, bool &opPerformed)
    {
        std::pair<PointsToID, PointsToID> operands;
        // If we're commutative, we want to always perform the same operation: x op y.
        // Performing x op y sometimes and y op x other times is a waste of time.
        if (commutative) operands = std::minmax(lhs, rhs);
        else operands = std::make_pair(lhs, rhs);

        // Check if we have performed this operation
        OpCache::const_iterator foundResult = opCache.find(operands);
        if (foundResult != opCache.end()) return foundResult->second;

        opPerformed = true;

        const Data &lhsPts = getActualPts(lhs);
        const Data &rhsPts = getActualPts(rhs);

        Data result = dataOp(lhsPts, rhsPts);

        PointsToID resultId;
        // Intern points-to set: check if result already exists.
        typename PTSToIDMap::const_iterator foundId = ptsToId.find(result);
        if (foundId != ptsToId.end()) resultId = foundId->second;
        else
        {
            resultId = newPointsToId();
            idToPts.push_back(new Data(result));
            ptsToId[result] = resultId;
        }

        // Cache the result, for hash-consing.
        opCache[operands] = resultId;

        return resultId;
    }

    /// Initialises statistics variables to 0.
    inline void initStats(void)
    {

        totalUnions              = 0;
        uniqueUnions             = 0;
        propertyUnions           = 0;
        lookupUnions             = 0;
        preemptiveUnions         = 0;
        totalComplements         = 0;
        uniqueComplements        = 0;
        propertyComplements      = 0;
        lookupComplements        = 0;
        preemptiveComplements    = 0;
        totalIntersections       = 0;
        uniqueIntersections      = 0;
        propertyIntersections    = 0;
        lookupIntersections      = 0;
        preemptiveIntersections  = 0;
    }

private:
    /// Maps points-to IDs (indices) to their corresponding points-to set.
    /// Reverse of idToPts.
    /// Elements are only added through push_back, so the number of elements
    /// stored is the size of the vector.
    /// Not const so we can remap.
    std::vector<Data *> idToPts;
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

    // Statistics:
    u64_t totalUnions;
    u64_t uniqueUnions;
    u64_t propertyUnions;
    u64_t lookupUnions;
    u64_t preemptiveUnions;
    u64_t totalComplements;
    u64_t uniqueComplements;
    u64_t propertyComplements;
    u64_t lookupComplements;
    u64_t preemptiveComplements;
    u64_t totalIntersections;
    u64_t uniqueIntersections;
    u64_t propertyIntersections;
    u64_t lookupIntersections;
    u64_t preemptiveIntersections;
};

} // End namespace SVF

#endif /* PERSISTENT_POINTS_TO_H_ */
