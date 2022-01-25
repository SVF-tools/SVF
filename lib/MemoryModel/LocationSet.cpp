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
/// (1) StructType, return flatterned number elements, i.e., the index of the last element (getFlattenedOffsetVec().back()) plus one.
/// (2) ArrayType, return number of elements
/// (3) PointerType, return the element number of the pointee 
/// (4) non-pointer SingleValueType, return 1
u32_t LocationSet::getElementNum(const Type* type) const{
    u32_t sz = 1;
    if(const ArrayType* aty = SVFUtil::dyn_cast<ArrayType>(type))
    {
        /// handle nested arrays
        const Type* innerTy = aty;
        while (const ArrayType* arr = SVFUtil::dyn_cast<ArrayType>(innerTy))
        {
            sz *= arr->getNumElements();
            innerTy = arr->getElementType();
        }
    }
    else if (const StructType *sty = SVFUtil::dyn_cast<StructType>(type) )
    {
        const vector<u32_t> &so = SymbolTableInfo::SymbolInfo()->getFlattenedOffsetVec(sty);
        sz = so.back() + 1;
    }
    else if (type->isSingleValueType())
    {
        /// This is a pointer arithmic
        if(const PointerType* pty = SVFUtil::dyn_cast<PointerType>(type) ){
            sz = getElementNum(pty->getElementType());
        }
    }
    else{
        SVFUtil::outs() << "GepIter Type" << *type << "\n";
        assert(false && "What other types for this gep?");
    }
    return sz;
}

/// Return accumulated constant offset
/// 
/// "value" is the offset variable (must be a constant)
/// "type" is the location where we want to compute offset
/// e.g., %3 = getelementptr inbounds [10 x i32], [10 x i32]* %1, i64 0, i64 5 
/// offsetValues[0] value: i64 0, type: [10 x i32]*
/// offsetValues[1] value: i64 5, type: [10 x i32]
/// 
/// Given a vector: [(v1,t1), (v2,t2), (v3,t3)]
/// totalConstOffset = v1 * sz(t2) + v2 * sz(t3) + v3 * 1
/// If the vector only has one element (one gep operand), then it must be a pointer arithmetic and type must be a PointerType
/// totalConstOffset = v1 * sz(t1) 
s64_t LocationSet::accumulateConstantOffset() const{
    
    assert(isConstantOffset() && "not a constant offset");

    s64_t totalConstOffset = 0;
    u32_t sz = 1;
    for(int i = offsetValues.size() - 1; i >= 0; i--){
        const Value* value = offsetValues[i].first;
        const Type* type = offsetValues[i].second;
        const ConstantInt *op = SVFUtil::dyn_cast<ConstantInt>(value);
        assert(op && "not a constant offset?");
        /// if this gep only has one operand, the gepIterType must be the pointer type, and we will need to retrieve size of its elementType.
        if(offsetValues.size()==1){
            assert(SVFUtil::isa<PointerType>(type) && "If gep has only one operand, its gepIterType must be PointerType!");
            sz = getElementNum(type);
        }
        totalConstOffset += op->getSExtValue() * sz; 
        sz *= getElementNum(type);
        SVFUtil::outs() << "Value: " << value2String(value) << " type: " << *type << "\n";
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