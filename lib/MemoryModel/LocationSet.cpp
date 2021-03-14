//===- LocationSet.cpp -- Location set for modeling abstract memory object----//
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
 * @file: LocationSet.cpp
 * @author: yesen
 * @date: 26 Sep 2014
 *
 * LICENSE
 *
 */

#include "Util/Options.h"
#include "MemoryModel/LocationSet.h"
#include "MemoryModel/MemModel.h"

using namespace SVF;


/*!
 * Add element num and stride pair
 */
void LocationSet::addElemNumStridePair(const NodePair& pair)
{
    /// The pair will not be added if any number of a stride is zero,
    /// because they will not have effect on the locations represented by this LocationSet.
    if (pair.first == 0 || pair.second == 0)
        return;

    if (Options::SingleStride)
    {
        if (numStridePair.empty())
            numStridePair.push_back(std::make_pair(StInfo::getMaxFieldLimit(),pair.second));
        else
        {
            /// Find the GCD stride
            NodeID existStride = (*numStridePair.begin()).second;
            NodeID newStride = gcd(pair.second, existStride);
            if (newStride != existStride)
            {
                numStridePair.pop_back();
                numStridePair.push_back(std::make_pair(StInfo::getMaxFieldLimit(),newStride));
            }
        }
    }
    else
    {
        numStridePair.push_back(pair);
    }
}


/*!
 * Return TRUE if it successfully increases any index by 1
 */
bool LocationSet::increaseIfNotReachUpperBound(std::vector<NodeID>& indices,
        const ElemNumStridePairVec& pairVec) const
{
    assert(indices.size() == pairVec.size() && "vector size not match");

    /// Check if all indices reach upper bound
    bool reachUpperBound = true;
    for (u32_t i = 0; i < indices.size(); i++)
    {
        assert(pairVec[i].first > 0 && "number must be greater than 0");
        if (indices[i] < (pairVec[i].first - 1))
            reachUpperBound = false;
    }

    /// Increase index if not reach upper bound
    bool increased = false;
    if (reachUpperBound == false)
    {
        u32_t i = 0;
        while (increased == false)
        {
            if (indices[i] < (pairVec[i].first - 1))
            {
                indices[i] += 1;
                increased = true;
            }
            else
            {
                indices[i] = 0;
                i++;
            }
        }
    }

    return increased;
}


/*!
 * Compute all possible locations according to offset and number-stride pairs.
 */
PointsTo LocationSet::computeAllLocations() const
{

    PointsTo result;
    result.set(getOffset());

    if (isConstantOffset() == false)
    {
        const ElemNumStridePairVec& lhsVec = getNumStridePair();
        std::vector<NodeID> indices;
        u32_t size = lhsVec.size();
        while (size)
        {
            indices.push_back(0);
            size--;
        }

        do
        {
            u32_t i = 0;
            NodeID ofst = getOffset();
            while (i < lhsVec.size())
            {
                ofst += (lhsVec[i].second * indices[i]);
                i++;
            }

            result.set(ofst);

        }
        while (increaseIfNotReachUpperBound(indices, lhsVec));
    }

    return result;
}



