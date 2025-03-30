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
    friend class SVFIRWriter;
    friend class SVFIRReader;

public:
    enum LSRelation
    {
        NonOverlap, Overlap, Subset, Superset, Same
    };

    typedef std::pair<const SVFVar*, const SVFType*> IdxOperandPair;
    typedef std::vector<IdxOperandPair> IdxOperandPairs;

    /// Constructor
    AccessPath(APOffset o = 0, const SVFType* srcTy = nullptr) : fldIdx(o), gepPointeeType(srcTy) {}

    /// Copy Constructor
    AccessPath(const AccessPath& ap)
        : fldIdx(ap.fldIdx),
          idxOperandPairs(ap.getIdxOperandPairVec()),
          gepPointeeType(ap.gepSrcPointeeType())
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
        idxOperandPairs = rhs.getIdxOperandPairVec();
        gepPointeeType = rhs.gepPointeeType;
        return *this;
    }
    inline bool operator==(const AccessPath& rhs) const
    {
        return this->fldIdx == rhs.fldIdx &&
               this->idxOperandPairs == rhs.idxOperandPairs && this->gepPointeeType == rhs.gepPointeeType;
    }
    //@}

    /// Get methods
    //@{
    inline APOffset getConstantStructFldIdx() const
    {
        return fldIdx;
    }
    inline void setFldIdx(APOffset idx)
    {
        fldIdx = idx;
    }
    inline const IdxOperandPairs& getIdxOperandPairVec() const
    {
        return idxOperandPairs;
    }
    inline const SVFType* gepSrcPointeeType() const
    {
        return gepPointeeType;
    }
    //@}

    /**
     * Computes the total constant byte offset of an access path.
     * This function iterates over the offset-variable-type pairs in reverse order,
     * accumulating the total byte offset for constant offsets. For each pair,
     * it retrieves the corresponding SVFValue and determines the type of offset
     * (whether it's an array, pointer, or structure). If the offset corresponds
     * to a structure, it further resolves the actual element type based on the
     * offset value. It then multiplies the offset value by the size of the type
     * to compute the byte offset. This is used to handle composite types where
     * offsets are derived from the type's internal structure, such as arrays
     * or structures with fields of various types and sizes. The function asserts
     * that the access path must have a constant offset, and it is intended to be
     * used when the offset is known to be constant at compile time.
     *
     * @return APOffset representing the computed total constant byte offset.
     */
    /// e.g. GepStmt* gep = [i32*4], 2
    /// APOffset byteOffset = gep->accumulateConstantByteOffset();
    /// byteOffset should be 8 since i32 is 4 bytes and index is 2.
    APOffset computeConstantByteOffset() const;

    /// Return accumulated constant offset given OffsetVarVec
    /// compard to computeConstantByteOffset, it is field offset rather than byte offset
    /// e.g. GepStmt* gep = [i32*4], 2
    /// APOffset byteOffset = gep->computeConstantOffset();
    /// byteOffset should be 2 since it is field offset.
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

    /// Return byte offset from the beginning of the structure to the field where it is located for struct type
    u32_t getStructFieldOffset(const SVFVar* idxOperandVar, const SVFStructType* idxOperandType) const;

    /// Dump location set
    std::string dump() const;

    inline void addIdxOperandPair(std::pair<const SVFVar*, const SVFType*> pair)
    {
        idxOperandPairs.push_back(pair);
    }

private:

    /// Check relations of two location sets
    LSRelation checkRelation(const AccessPath& LHS, const AccessPath& RHS);

    /// Compute all possible locations according to offset and number-stride pairs.
    NodeBS computeAllLocations() const;

    APOffset fldIdx;	///< Accumulated Constant Offsets
    IdxOperandPairs idxOperandPairs;	///< a vector of actual offset in the form of <SVF Var, iterator type>
    const SVFType* gepPointeeType;   /// source element type in gep instruction,
    /// e.g., %f1 = getelementptr inbounds %struct.MyStruct, %struct.MyStruct* %arrayidx, i32 0, i32 0
    /// the source element type is %struct.MyStruct
};

} // End namespace SVF

template <> struct std::hash<SVF::AccessPath>
{
    size_t operator()(const SVF::AccessPath &ap) const
    {
        SVF::Hash<std::pair<SVF::NodeID, SVF::NodeID>> h;
        std::hash<SVF::AccessPath::IdxOperandPairs> v;
        return h(std::make_pair(ap.getConstantStructFldIdx(),
                                v(ap.getIdxOperandPairVec())));
    }
};

#endif /* AccessPath_H_ */
