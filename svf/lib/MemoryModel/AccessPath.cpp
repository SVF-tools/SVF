//===- AccessPath.cpp -- Location set for modeling abstract memory object----//
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
 * @file: AccessPath.cpp
 * @author: yesen
 * @date: 26 Sep 2014
 *
 * LICENSE
 *
 */

#include "Util/Options.h"
#include "MemoryModel/AccessPath.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Add offset value to vector offsetVarAndGepTypePairs
 */
bool AccessPath::addOffsetVarAndGepTypePair(const SVFVar* var, const SVFType* gepIterType)
{
    idxOperandPairs.emplace_back(var, gepIterType);
    return true;
}

/// Return true if all offset values are constants
bool AccessPath::isConstantOffset() const
{
    for(auto it : idxOperandPairs)
    {
        if(SVFUtil::isa<ConstIntValVar>(it.first) == false)
            return false;
    }
    return true;
}

/// Return element number of a type
/// (1) StructType or Array, return flattened number elements.
/// (2) Pointer type, return max field limit
/// (3) Non-pointer SingleValueType or Function Type, return 1
u32_t AccessPath::getElementNum(const SVFType* type) const
{
    if (SVFUtil::isa<SVFArrayType, SVFStructType>(type))
    {
        return PAG::getPAG()->getNumOfFlattenElements(type);
    }
    else if (type->isPointerTy())
    {
        // if type is a pointer, should be like:
        // %2 = getelementptr inbounds i32*, i32** %1, ...
        // where gepSrcPointee is of pointer type (i32*).
        // this can be transformed to:
        // %2 = getelementptr inbounds [N x i32], [N x i32]* %1, ...
        // However, we do not know N without context information. int** implies non-contiguous blocks of memory
        // In this case, we conservatively return max field limit
        return Options::MaxFieldLimit();
    }
    else if (type->isSingleValueType() || SVFUtil::isa<SVFFunctionType>(type))
    {
        return 1;
    }
    else
    {
        SVFUtil::outs() << "GepIter Type" << *type << "\n";
        assert(false && "What other types for this gep?");
        abort();
    }
}


/// Return byte offset from the beginning of the structure to the field where it is located for struct type
///
// e.g. idxOperandVar: i32 2  idxOperandType: %struct.Student = type { i32, [i8 x 12], i32 }
//  we accumulate field 0 (i32) byte size (4 Bytes), and field 1 ([i8x12]) byte size (12 Bytes)
//  then the return byte offset is 16 Bytes.
u32_t AccessPath::getStructFieldOffset(const SVFVar* idxOperandVar, const SVFStructType* idxOperandType) const
{
    u32_t structByteOffset = 0;
    if (const ConstIntValVar*op = SVFUtil::dyn_cast<ConstIntValVar>(idxOperandVar))
    {
        for (u32_t structField = 0; structField < (u32_t) op->getSExtValue(); ++structField)
        {
            u32_t flattenIdx = idxOperandType->getTypeInfo()->getFlattenedFieldIdxVec()[structField];
            structByteOffset += idxOperandType->getTypeInfo()->getOriginalElemType(flattenIdx)->getByteSize();
        }
        return structByteOffset;
    }
    else
    {
        assert(false && "struct type can only pair with constant idx");
        abort();
    }
}

/// Return accumulated constant offset
///
/// "value" is the offset variable (must be a constant)
/// "type" is the location where we want to compute offset
/// Given a vector and elem byte size: [(value1,type1), (value2,type2), (value3,type3)], bytesize
/// totalConstByteOffset = ByteOffset(value1,type1) * ByteOffset(value2,type2) + ByteOffset(value3,type3)
/// For a pointer type (e.g., t1 is PointerType), we will retrieve the pointee type and times the offset, i.e., getElementNum(t1) X off1
APOffset AccessPath::computeConstantByteOffset() const
{
    assert(isConstantOffset() && "not a constant offset");

    APOffset totalConstOffset = 0;
    for(int i = idxOperandPairs.size() - 1; i >= 0; i--)
    {
        /// For example, there is struct DEST{int a, char b[10], int c[5]}
        /// (1) %c = getelementptr inbounds %struct.DEST, %struct.DEST* %arr, i32 0, i32 2
        //  (2) %arrayidx = getelementptr inbounds [10 x i8], [10 x i8]* %b, i64 0, i64 8
        const SVFVar* var = idxOperandPairs[i].first;
        /// for (1) offsetVarAndGepTypePairs.size()  = 2
        ///     i = 0, type: %struct.DEST*, PtrType, op = 0
        ///     i = 1, type: %struct.DEST, StructType, op = 2
        /// for (2) offsetVarAndGepTypePairs.size()  = 2
        ///     i = 0, type: [10 x i8]*, PtrType, op = 0
        ///     i = 1, type: [10 x i8], ArrType, op = 8
        const SVFType* type = idxOperandPairs[i].second;
        /// if offsetVarAndGepTypePairs[i].second is nullptr, it means
        ///   GepStmt comes from external API, this GepStmt is assigned in SVFIRExtAPI.cpp
        ///   at SVFIRBuilder::getBaseTypeAndFlattenedFields ls.addOffsetVarAndGepTypePair()
        assert(type && "this GepStmt comes from ExternalAPI cannot call this api");
        const SVFType* type2 = type;
        if (const SVFArrayType* arrType = SVFUtil::dyn_cast<SVFArrayType>(type))
        {
            /// for (2) i = 1, arrType: [10 x i8], type2 = i8
            type2 = arrType->getTypeOfElement();
        }
        else if (SVFUtil::isa<SVFPointerType>(type))
        {
            /// for (1) i = 0, ptrType: %struct.DEST*, type2: %struct.DEST
            /// for (2) i = 0, ptrType: [10 x i8]*, type2 = [10 x i8]
            type2 = gepSrcPointeeType();
        }

        const ConstIntValVar* op = SVFUtil::dyn_cast<ConstIntValVar>(var);
        if (const SVFStructType* structType = SVFUtil::dyn_cast<SVFStructType>(type))
        {
            /// for (1) structType: %struct.DEST
            ///   structField = 0, flattenIdx = 0, type2: int
            ///   structField = 1, flattenIdx = 1, type2: char[10]
            ///   structField = 2, flattenIdx = 11, type2: int[5]
            for (u32_t structField = 0; structField < (u32_t)op->getSExtValue(); ++structField)
            {
                u32_t flattenIdx = structType->getTypeInfo()->getFlattenedFieldIdxVec()[structField];
                type2 = structType->getTypeInfo()->getOriginalElemType(flattenIdx);
                totalConstOffset += type2->getByteSize();
            }
        }
        else
        {
            /// for (2) i = 0, op: 0, type: [10 x i8]*(Ptr), type2: [10 x i8](Arr)
            ///         i = 1, op: 8, type: [10 x i8](Arr), type2: i8
            totalConstOffset += op->getSExtValue() * type2->getByteSize();
        }
    }
    totalConstOffset = Options::MaxFieldLimit() > totalConstOffset? totalConstOffset: Options::MaxFieldLimit();
    return totalConstOffset;
}

/// Return accumulated constant offset
///
/// "value" is the offset variable (must be a constant)
/// "type" is the location where we want to compute offset
/// Given a vector: [(value1,type1), (value2,type2), (value3,type3)]
/// totalConstOffset = flattenOffset(value1,type1) * flattenOffset(value2,type2) + flattenOffset(value3,type3)
/// For a pointer type (e.g., t1 is PointerType), we will retrieve the pointee type and times the offset, i.e., getElementNum(t1) X off1

/// For example,
// struct inner{ int rollNumber; float percentage;};
// struct Student { struct inner rollNumber; char studentName[10][3];}
// char x = studentRecord[1].studentName[3][2];

/// %5 = getelementptr inbounds %struct.Student, %struct.Student* %4, i64 1
///     value1: i64 1 type1: %struct.Student*
///     computeConstantOffset = 32
/// %6 = getelementptr inbounds %struct.Student, %struct.Student* %5, i32 0, i32 1
///     value1: i32 0  type1: %struct.Student*
///     value2: i32 1  type2: %struct.Student = type { %struct.inner, [10 x [3 x i8]] }
///     computeConstantOffset = 2
/// %7 = getelementptr inbounds [10 x [3 x i8]], [10 x [3 x i8]]* %6, i64 0, i64 3
///     value1: i64 0  type1: [10 x [3 x i8]]*
///     value2: i64 3  type2: [10 x [3 x i8]]
///     computeConstantOffset = 9
/// %8 = getelementptr inbounds [3 x i8], [3 x i8]* %7, i64 0, i64 2
///     value1: i64 0  type1: [3 x i8]*
///     value2: i64 2  type2: [3 x i8]
///     computeConstantOffset = 2
APOffset AccessPath::computeConstantOffset() const
{

    assert(isConstantOffset() && "not a constant offset");

    APOffset totalConstOffset = 0;
    //After the model-const and model-array options are turned on,
    // the gepstmt offset generated by the array on the global
    // node will be saved in getConstantStructFldIdx
    if (idxOperandPairs.size() == 0)
        return getConstantStructFldIdx();
    for(int i = idxOperandPairs.size() - 1; i >= 0; i--)
    {
        const SVFVar* var = idxOperandPairs[i].first;
        const SVFType* type = idxOperandPairs[i].second;
        assert(SVFUtil::isa<ConstIntValVar>(var) && "not a constant offset?");
        s64_t constOffset = SVFUtil::dyn_cast<ConstIntValVar>(var)->getSExtValue();

        if(type==nullptr)
        {
            totalConstOffset += constOffset;
            continue;
        }

        if(SVFUtil::isa<SVFPointerType>(type))
            totalConstOffset += constOffset * getElementNum(gepPointeeType);
        else
        {
            APOffset offset = constOffset;
            if (offset >= 0)
            {
                const std::vector<u32_t>& so = PAG::getPAG()->getTypeInfo(type)->getFlattenedElemIdxVec();
                // if offset is larger than the size of getFlattenedElemIdxVec (overflow)
                // set offset the last index of getFlattenedElemIdxVec to avoid assertion
                if (offset >= (APOffset)so.size())
                {
                    SVFUtil::errs() << "It is an overflow access, hence it is the last idx\n";
                    offset = so.size() - 1;
                }
                else
                {

                }

                u32_t flattenOffset =
                    PAG::getPAG()->getFlattenedElemIdx(type,
                                                       offset);
                totalConstOffset += flattenOffset;
            }
        }
    }
    return totalConstOffset;
}
/*!
 * Compute all possible locations according to offset and number-stride pairs.
 */
NodeBS AccessPath::computeAllLocations() const
{
    NodeBS result;
    result.set(getConstantStructFldIdx());
    return result;
}

AccessPath AccessPath::operator+(const AccessPath& rhs) const
{
    assert(gepPointeeType == rhs.gepSrcPointeeType() && "source element type not match");
    AccessPath ap(rhs);
    ap.fldIdx += getConstantStructFldIdx();
    for (auto &p : ap.getIdxOperandPairVec())
        ap.addOffsetVarAndGepTypePair(p.first, p.second);

    return ap;
}

bool AccessPath::operator< (const AccessPath& rhs) const
{
    if (fldIdx != rhs.fldIdx)
        return (fldIdx < rhs.fldIdx);
    else
    {
        const IdxOperandPairs& pairVec = getIdxOperandPairVec();
        const IdxOperandPairs& rhsPairVec = rhs.getIdxOperandPairVec();
        if (pairVec.size() != rhsPairVec.size())
            return (pairVec.size() < rhsPairVec.size());
        else
        {
            IdxOperandPairs::const_iterator it = pairVec.begin();
            IdxOperandPairs::const_iterator rhsIt = rhsPairVec.begin();
            for (; it != pairVec.end() && rhsIt != rhsPairVec.end(); ++it, ++rhsIt)
            {
                return (*it) < (*rhsIt);
            }

            return false;
        }
    }
}

SVF::AccessPath::LSRelation AccessPath::checkRelation(const AccessPath& LHS, const AccessPath& RHS)
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
std::string AccessPath::dump() const
{
    std::string str;
    std::stringstream rawstr(str);

    rawstr << "AccessPath\tField_Index: " << getConstantStructFldIdx();
    rawstr << ",\tNum-Stride: {";
    const IdxOperandPairs& vec = getIdxOperandPairVec();
    IdxOperandPairs::const_iterator it = vec.begin();
    IdxOperandPairs::const_iterator eit = vec.end();
    for (; it != eit; ++it)
    {
        const SVFType* ty = it->second;
        rawstr << " (Svf var: " << it->first->toString() << ", Iter type: " << *ty << ")";
    }
    rawstr << " }\n";
    return rawstr.str();
}
