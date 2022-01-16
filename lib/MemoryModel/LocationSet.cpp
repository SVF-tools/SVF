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
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Add offset value to vector offsetValues
 */
bool LocationSet::addOffsetValue(const Value* offsetVal)
{
    for(const Value* val : offsetValues){
        if(val==offsetVal)
            return false;
    }
    offsetValues.push_back(offsetVal);
    return true;
}

/// Return TRUE if all offset values are constants
bool LocationSet::isConstantOffset() const
{
    for(const Value* val : offsetValues){
        if(SVFUtil::isa<ConstantInt>(val) == false)
            return false;
    }
    return true;
}

/*!
 * Compute all possible locations according to offset and number-stride pairs.
 */
NodeBS LocationSet::computeAllLocations() const
{
    NodeBS result;
    result.set(accumulateConstantFieldIdx());
    return result;
}

SVF::LocationSet::LSRelation LocationSet::checkRelation(const LocationSet& LHS, const LocationSet& RHS)
{
    NodeBS lhsLocations = LHS.computeAllLocations();
    NodeBS rhsLocations = RHS.computeAllLocations();
    if (lhsLocations.intersects(rhsLocations))
    {
        if (lhsLocations == rhsLocations)
            return Same;
        else if (lhsLocations.contains(rhsLocations))
            return Superset;
        else if (rhsLocations.contains(lhsLocations))
            return Subset;
        else
            return Overlap;
    }
    else
    {
        return NonOverlap;
    }
}

/// Dump location set
std::string LocationSet::dump() const
{
    std::string str;
    raw_string_ostream rawstr(str);

    rawstr << "LocationSet\tField_Index: " << accumulateConstantFieldIdx();
    rawstr << ",\tNum-Stride: {";
    const OffsetValueVec& vec = getOffsetValueVec();
    OffsetValueVec::const_iterator it = vec.begin();
    OffsetValueVec::const_iterator eit = vec.end();
    for (; it != eit; ++it)
    {
        rawstr << " (" << value2String(*it) << ")";
    }
    rawstr << " }\n";
    return rawstr.str();
}