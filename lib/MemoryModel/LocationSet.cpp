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
bool LocationSet::addOffsetValue(const Value* offsetVal, const Type* type)
{
    offsetValues.push_back(std::make_pair(offsetVal,type));
    return true;
}

/// Return true if all offset values are constants
bool LocationSet::isConstantOffset() const
{
    for(auto it : offsetValues){
        if(SVFUtil::isa<ConstantInt>(it.first) == false)
            return false;
    }
    return true;
}

/// Return element number of a type
/// (1) StructType or Array, return flatterned number elements.
/// (2) PointerType, return the element number of the pointee 
/// (3) non-pointer SingleValueType, return 1
u32_t LocationSet::getElementNum(const Type* type) const{

    if(SVFUtil::isa<ArrayType>(type) || SVFUtil::isa<StructType>(type))
    {
        return SymbolTableInfo::SymbolInfo()->getNumOfFlattenElements(type);
    }
    else if (type->isSingleValueType())
    {
        /// This is a pointer arithmic
        if(const PointerType* pty = SVFUtil::dyn_cast<PointerType>(type))
            return getElementNum(pty->getElementType());
        else
            return 1;
    }
    else{
        SVFUtil::outs() << "GepIter Type" << type2String(type) << "\n";
        assert(false && "What other types for this gep?");
        abort();
    }
}

/// Return accumulated constant offset
/// 
/// "value" is the offset variable (must be a constant)
/// "type" is the location where we want to compute offset
/// Given a vector: [(value1,type1), (value2,type2), (value3,type3)]
/// totalConstOffset = flattenOffset(value1,type1) * flattenOffset(type2,type2) + flattenOffset(type3,type3)
/// For a pointer type (e.g., t1 is PointerType), we will retrieve the pointee type and times the offset, i.e., getElementNum(t1) X off1

/// For example,
// struct inner{ int rollNumber; float percentage;};
// struct Student { struct inner rollNumber; char studentName[10][3];}
// char x = studentRecord[1].studentName[3][2];

/// %5 = getelementptr inbounds %struct.Student, %struct.Student* %4, i64 1
///     value1: i64 1 type1: %struct.Student*  
///     accumulateConstantOffset = 32
/// %6 = getelementptr inbounds %struct.Student, %struct.Student* %5, i32 0, i32 1 
///     value1: i32 0  type1: %struct.Student* 
///     value2: i32 1  type2: %struct.Student = type { %struct.inner, [10 x [3 x i8]] }  
///     accumulateConstantOffset = 2
/// %7 = getelementptr inbounds [10 x [3 x i8]], [10 x [3 x i8]]* %6, i64 0, i64 3 
///     value1: i64 0  type1: [10 x [3 x i8]]*
///     value2: i64 3  type2: [10 x [3 x i8]]       
///     accumulateConstantOffset = 9
/// %8 = getelementptr inbounds [3 x i8], [3 x i8]* %7, i64 0, i64 2 
///     value1: i64 0  type1: [3 x i8]*
///     value2: i64 2  type2: [3 x i8]
///     accumulateConstantOffset = 2
s32_t LocationSet::accumulateConstantOffset() const{
    
    assert(isConstantOffset() && "not a constant offset");

    if(offsetValues.empty())
        return accumulateConstantFieldIdx();

    s32_t totalConstOffset = 0;
    for(int i = offsetValues.size() - 1; i >= 0; i--){
        const Value* value = offsetValues[i].first;
        const Type* type = offsetValues[i].second;
        const ConstantInt *op = SVFUtil::dyn_cast<ConstantInt>(value);
        assert(op && "not a constant offset?");
        if(type==nullptr){
            totalConstOffset += op->getSExtValue();
            continue;
        }

        if(const PointerType* pty = SVFUtil::dyn_cast<PointerType>(type))
            totalConstOffset += op->getSExtValue() * getElementNum(pty->getElementType());
        else{
            s32_t offset = op->getSExtValue();
            u32_t flattenOffset = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(type, offset); 
            totalConstOffset += flattenOffset;
        }
    }
    return totalConstOffset;
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

LocationSet LocationSet::operator+ (const LocationSet& rhs) const
{
    LocationSet ls(rhs);
    ls.fldIdx += accumulateConstantFieldIdx();
    OffsetValueVec::const_iterator it = getOffsetValueVec().begin();
    OffsetValueVec::const_iterator eit = getOffsetValueVec().end();
    for (; it != eit; ++it)
        ls.addOffsetValue(it->first, it->second);

    return ls;
}


bool LocationSet::operator< (const LocationSet& rhs) const
{
    if (fldIdx != rhs.fldIdx)
        return (fldIdx < rhs.fldIdx);
    else
    {
        const OffsetValueVec& pairVec = getOffsetValueVec();
        const OffsetValueVec& rhsPairVec = rhs.getOffsetValueVec();
        if (pairVec.size() != rhsPairVec.size())
            return (pairVec.size() < rhsPairVec.size());
        else
        {
            OffsetValueVec::const_iterator it = pairVec.begin();
            OffsetValueVec::const_iterator rhsIt = rhsPairVec.begin();
            for (; it != pairVec.end() && rhsIt != rhsPairVec.end(); ++it, ++rhsIt)
            {
                return (*it) < (*rhsIt);
            }

            return false;
        }
    }
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
        rawstr << " (value: " << value2String(it->first) << " type: " << it->second << ")";
    }
    rawstr << " }\n";
    return rawstr.str();
}
