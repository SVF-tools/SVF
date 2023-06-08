//===- AccessPath.h -- Location set of abstract object-----------------------//
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
 * @file: AccessPath.h
 * @author: yesen
 * @date: 26 Sep 2014
 *
 * LICENSE
 *
 */


#ifndef AccessPath_H_
#define AccessPath_H_


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
class AccessPath
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
    AccessPath(APOffset o = 0) : fldIdx(o) {}

    /// Copy Constructor
    AccessPath(const AccessPath& ap)
        : fldIdx(ap.fldIdx),
          offsetVarAndGepTypePairs(ap.getOffsetVarAndGepTypePairVec())
    {
    }

    ~AccessPath() {}

    /// Overload operators
    //@{
    AccessPath operator+(const AccessPath& rhs) const;
    bool operator<(const AccessPath& rhs) const;
    inline const AccessPath& operator=(const AccessPath& rhs)
    {
        fldIdx = rhs.fldIdx;
        offsetVarAndGepTypePairs = rhs.getOffsetVarAndGepTypePairVec();
        return *this;
    }
    inline bool operator==(const AccessPath& rhs) const
    {
        return this->fldIdx == rhs.fldIdx &&
               this->offsetVarAndGepTypePairs == rhs.offsetVarAndGepTypePairs;
    }
    //@}

    /// Get methods
    //@{
    inline APOffset getConstantFieldIdx() const
    {
        return fldIdx;
    }
    inline void setFldIdx(APOffset idx)
    {
        fldIdx = idx;
    }
    inline const OffsetVarAndGepTypePairs& getOffsetVarAndGepTypePairVec() const
    {
        return offsetVarAndGepTypePairs;
    }
    //@}

    /// Return accumulated constant offset given OffsetVarVec
    APOffset computeConstantOffset() const;

    /// Return element number of a type.
    u32_t getElementNum(const SVFType* type) const;


    bool addOffsetVarAndGepTypePair(const SVFVar* var, const SVFType* gepIterType);

    /// Return TRUE if this is a constant location set.
    bool isConstantOffset() const;

    /// Return TRUE if we share any location in common with RHS
    inline bool intersects(const AccessPath& RHS) const
    {
        return computeAllLocations().intersects(RHS.computeAllLocations());
    }

    /// Dump location set
    std::string dump() const;

private:

    /// Check relations of two location sets
    LSRelation checkRelation(const AccessPath& LHS, const AccessPath& RHS);

    /// Compute all possible locations according to offset and number-stride pairs.
    NodeBS computeAllLocations() const;

    APOffset fldIdx;	///< Accumulated Constant Offsets
    OffsetVarAndGepTypePairs offsetVarAndGepTypePairs;	///< a vector of actual offset in the form of <SVF Var, iterator type>s
};

} // End namespace SVF

template <> struct std::hash<SVF::AccessPath>
{
    size_t operator()(const SVF::AccessPath &ap) const
    {
        SVF::Hash<std::pair<SVF::NodeID, SVF::NodeID>> h;
        std::hash<SVF::AccessPath::OffsetVarAndGepTypePairs> v;
        return h(std::make_pair(ap.getConstantFieldIdx(),
                                v(ap.getOffsetVarAndGepTypePairVec())));
    }
};

#endif /* AccessPath_H_ */
