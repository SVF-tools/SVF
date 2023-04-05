//===- LocationSet.h -- Location set of abstract object-----------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * @file: LocationSet.h
 * @author: yesen
 * @date: 26 Sep 2014
 *
 * LICENSE
 *
 */


#ifndef LOCATIONSET_H_
#define LOCATIONSET_H_


#include "SVFIR/SVFValue.h"


namespace SVF
{

class SVFVar;


/*
* Location set represents a set of locations in a memory block with following offsets:
*     { offset + \sum_{i=0}^N (stride_i * j_i) | 0 \leq j_i < M_i }
* where N is the size of number-stride pair vector, M_i (stride_i) is i-th number (stride)
* in the number-stride pair vector.
*/
class LocationSet
{
    friend class SymbolTableInfo;
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    enum LSRelation
    {
        NonOverlap, Overlap, Subset, Superset, Same
    };

    typedef std::pair<const SVFVar*, const SVFType*> VarAndGepTypePair;
    typedef std::vector<VarAndGepTypePair> OffsetVarAndGepTypePairs;

    /// Constructor
    LocationSet(s32_t o = 0) : fldIdx(o) {}

    /// Copy Constructor
    LocationSet(const LocationSet& ls)
        : fldIdx(ls.fldIdx),
          offsetVarAndGepTypePairs(ls.getOffsetVarAndGepTypePairVec())
    {
    }

    ~LocationSet() {}

    /// Overload operators
    //@{
    LocationSet operator+(const LocationSet& rhs) const;
    bool operator<(const LocationSet& rhs) const;
    inline const LocationSet& operator=(const LocationSet& rhs)
    {
        fldIdx = rhs.fldIdx;
        offsetVarAndGepTypePairs = rhs.getOffsetVarAndGepTypePairVec();
        return *this;
    }
    inline bool operator==(const LocationSet& rhs) const
    {
        return this->fldIdx == rhs.fldIdx &&
               this->offsetVarAndGepTypePairs == rhs.offsetVarAndGepTypePairs;
    }
    //@}

    /// Get methods
    //@{
    inline s32_t getConstantFieldIdx() const
    {
        return fldIdx;
    }
    inline void setFldIdx(s32_t idx)
    {
        fldIdx = idx;
    }
    inline const OffsetVarAndGepTypePairs& getOffsetVarAndGepTypePairVec() const
    {
        return offsetVarAndGepTypePairs;
    }
    //@}

    /// Return accumulated constant offset given OffsetVarVec
    s32_t computeConstantOffset() const;

    /// Return element number of a type.
    u32_t getElementNum(const SVFType* type) const;


    bool addOffsetVarAndGepTypePair(const SVFVar* var, const SVFType* gepIterType);

    /// Return TRUE if this is a constant location set.
    bool isConstantOffset() const;

    /// Return TRUE if we share any location in common with RHS
    inline bool intersects(const LocationSet& RHS) const
    {
        return computeAllLocations().intersects(RHS.computeAllLocations());
    }

    /// Dump location set
    std::string dump() const;

private:

    /// Check relations of two location sets
    LSRelation checkRelation(const LocationSet& LHS, const LocationSet& RHS);

    /// Compute all possible locations according to offset and number-stride pairs.
    NodeBS computeAllLocations() const;

    s32_t fldIdx;	///< Accumulated Constant Offsets
    OffsetVarAndGepTypePairs offsetVarAndGepTypePairs;	///< a vector of actual offset in the form of <SVF Var, iterator type>s
};

} // End namespace SVF

template <> struct std::hash<SVF::LocationSet>
{
    size_t operator()(const SVF::LocationSet &ls) const
    {
        SVF::Hash<std::pair<SVF::NodeID, SVF::NodeID>> h;
        std::hash<SVF::LocationSet::OffsetVarAndGepTypePairs> v;
        return h(std::make_pair(ls.getConstantFieldIdx(), v(ls.getOffsetVarAndGepTypePairVec())));
    }
};

#endif /* LOCATIONSET_H_ */
