//===- MemPartition.cpp -- Memory region partition----------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * @file: DisMRGenerator.cpp
 * @author: yesen
 * @date: 07/12/2013
 * @version: 1.0
 *
 * @section LICENSE
 *
 * @section DESCRIPTION
 *
 */

#include "MSSA/MemPartition.h"

using namespace SVF;

/**
 * Create distinct memory regions.
 */
void DistinctMRG::partitionMRs()
{
    for(FunToPointsToMap::iterator it = getFunToPointsToList().begin(), eit = getFunToPointsToList().end();
            it!=eit; ++it)
    {
        const SVFFunction* fun = it->first;
        /// Collect all points-to target in a function scope.
        PointsTo mergePts;
        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit)
        {
            const PointsTo& pts = *cit;
            mergePts |= pts;
        }
        createDistinctMR(fun, mergePts);
    }
}

/**
 * Create memory regions for each points-to target.
 * 1. collect all points-to targets in a function scope.
 * 2. create memory region for each point-to target.
 */
void DistinctMRG::createDistinctMR(const SVFFunction* func, const PointsTo& pts)
{
    /// Create memory regions for each points-to target.

    PointsTo::iterator ptsIt = pts.begin();
    PointsTo::iterator ptsEit = pts.end();
    for (; ptsIt != ptsEit; ++ptsIt)
    {
        NodeID id = *ptsIt;
        // create new conditional points-to set with this single element.
        PointsTo newPts;
        newPts.set(id);

        // set the rep cpts as itself.
        cptsToRepCPtsMap[newPts] = newPts;

        // add memory region for this points-to target.
        createMR(func, newPts);
    }
}

/**
 * Get memory regions to be inserted at a load statement.
 * @param cpts The conditional points-to set of load statement.
 * @param fun The function being analyzed.
 * @param mrs Memory region set contains all possible target memory regions.
 */
void DistinctMRG::getMRsForLoad(MRSet& mrs, const PointsTo& pts, const SVFFunction*)
{
    /// Get memory regions for each points-to element in cpts.

    PointsTo::iterator ptsIt = pts.begin();
    PointsTo::iterator ptsEit = pts.end();
    for (; ptsIt != ptsEit; ++ptsIt)
    {
        NodeID id = *ptsIt;
        // create new conditional points-to set with this single element.
        PointsTo newPts;
        newPts.set(id);

        MemRegion mr(newPts);
        MRSet::iterator mit = memRegSet.find(&mr);
        assert(mit!=memRegSet.end() && "memory region not found!!");
        mrs.insert(*mit);
    }
}

/**
 * Get memory regions to be inserted at a load statement.
 * Just process as getMRsForLoad().
 */
void DistinctMRG::getMRsForCallSiteRef(MRSet& aliasMRs, const PointsTo& cpts, const SVFFunction* fun)
{
    getMRsForLoad(aliasMRs, cpts, fun);
}

/*-----------------------------------------------------*/

void IntraDisjointMRG::partitionMRs()
{
    for(FunToPointsToMap::iterator it = getFunToPointsToList().begin(),
            eit = getFunToPointsToList().end(); it!=eit; ++it)
    {
        const SVFFunction* fun = it->first;

        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end();
                cit!=ecit; ++cit)
        {
            const PointsTo& cpts = *cit;

            PointsToList& inters = getIntersList(fun);
            computeIntersections(cpts, inters);
        }

        /// Create memory regions.
        const PointsToList& inters = getIntersList(fun);
        for (PointsToList::const_iterator interIt = inters.begin(), interEit = inters.end();
                interIt != interEit; ++interIt)
        {
            const PointsTo& inter = *interIt;
            createDisjointMR(fun, inter);
        }
    }
}

/**
 * Compute intersections between cpts and computed cpts intersections before.
 */
void IntraDisjointMRG::computeIntersections(const PointsTo& cpts, PointsToList& inters)
{
    if (inters.find(cpts) != inters.end())
    {
        // Skip this cpts if it is already in the map.
        return;
    }
    else if (cpts.count() == 1)
    {
        // If this cpts has only one element, it will not intersect with any cpts in inters,
        // just add it into intersection set.
        inters.insert(cpts);
        return;
    }
    else
    {
        PointsToList toBeDeleted;
        PointsToList newInters;

        PointsTo cpts_copy = cpts;	// make a copy since cpts may be changed.

        // check intersections with existing cpts in subSetMap
        for (PointsToList::const_iterator interIt = inters.begin(), interEit = inters.end();
                interIt != interEit; ++interIt)
        {
            const PointsTo& inter = *interIt;

            if (cpts_copy.intersects(inter))
            {
                // compute intersection between cpts and inter
                PointsTo new_inter = inter;
                new_inter &= cpts_copy;

                // remove old intersection and add new one if possible
                if (new_inter != inter)
                {
                    toBeDeleted.insert(inter);
                    newInters.insert(new_inter);

                    // compute complement after intersection
                    PointsTo complement = inter;
                    complement.intersectWithComplement(new_inter);
                    if (complement.empty() == false)
                    {
                        newInters.insert(complement);
                    }
                }

                cpts_copy.intersectWithComplement(new_inter);

                if (cpts_copy.empty())
                    break;
            }
        }

        // remove old intersections
        for (PointsToList::const_iterator it = toBeDeleted.begin(), eit = toBeDeleted.end();
                it != eit; ++it)
        {
            const PointsTo& temp_cpts = *it;
            inters.erase(temp_cpts);
        }

        // add new intersections
        for (PointsToList::const_iterator it = newInters.begin(), eit = newInters.end();
                it != eit; ++it)
        {
            const PointsTo& temp_cpts = *it;
            inters.insert(temp_cpts);
        }

        // add remaining set into inters
        if (cpts_copy.empty() == false)
            inters.insert(cpts_copy);
    }
}

/**
 * Create memory regions for each points-to target.
 */
void IntraDisjointMRG::createDisjointMR(const SVFFunction* func, const PointsTo& cpts)
{
    // set the rep cpts as itself.
    cptsToRepCPtsMap[cpts] = cpts;

    // add memory region for this points-to target.
    createMR(func, cpts);
}

void IntraDisjointMRG::getMRsForLoadFromInterList(MRSet& mrs, const PointsTo& cpts, const PointsToList& inters)
{
    PointsToList::const_iterator it = inters.begin();
    PointsToList::const_iterator eit = inters.end();
    for (; it != eit; ++it)
    {
        const PointsTo& inter = *it;
        if (cpts.contains(inter))
        {
            MemRegion mr(inter);
            MRSet::iterator mit = memRegSet.find(&mr);
            assert(mit!=memRegSet.end() && "memory region not found!!");
            mrs.insert(*mit);
        }
    }
}

/**
 * Get memory regions to be inserted at a load statement.
 * Just process as getMRsForLoad().
 */
void IntraDisjointMRG::getMRsForCallSiteRef(MRSet& aliasMRs, const PointsTo& cpts, const SVFFunction* fun)
{
    getMRsForLoad(aliasMRs, cpts, fun);
}

/*-----------------------------------------------------*/

void InterDisjointMRG::partitionMRs()
{
    /// Generate disjoint cpts.
    for(FunToPointsToMap::iterator it = getFunToPointsToList().begin(),
            eit = getFunToPointsToList().end(); it!=eit; ++it)
    {
        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end();
                cit!=ecit; ++cit)
        {
            const PointsTo& cpts = *cit;

            computeIntersections(cpts, inters);
        }
    }

    /// Create memory regions.
    for(FunToPointsToMap::iterator it = getFunToPointsToList().begin(),
            eit = getFunToPointsToList().end(); it!=eit; ++it)
    {
        const SVFFunction* fun = it->first;

        for(PointsToList::iterator cit = it->second.begin(), ecit = it->second.end();
                cit!=ecit; ++cit)
        {
            const PointsTo& cpts = *cit;

            for (PointsToList::const_iterator interIt = inters.begin(), interEit = inters.end();
                    interIt != interEit; ++interIt)
            {
                const PointsTo& inter = *interIt;
                if (cpts.contains(inter))
                    createDisjointMR(fun, inter);
            }
        }
    }
}
