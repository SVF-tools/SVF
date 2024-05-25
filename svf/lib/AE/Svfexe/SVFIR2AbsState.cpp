//===- SVFIR2AbsState.cpp -- SVF IR Translation to Interval Domain-----//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
 * SVFIR2AbsState.cpp
 *
 *  Created on: Aug 7, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 *
 */

#include "AE/Svfexe/SVFIR2AbsState.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

AbstractValue SVF::SVFIR2AbsState::globalNulladdrs = AddressValue();

/**
 * This function, getRangeLimitFromType, calculates the lower and upper bounds of
 * a numeric range for a given SVFType. It is used to determine the possible value
 * range of integer types. If the type is an SVFIntegerType, it calculates the bounds
 * based on the size and signedness of the type. The calculated bounds are returned
 * as an IntervalValue representing the lower (lb) and upper (ub) limits of the range.
 *
 * @param type   The SVFType for which to calculate the value range.
 *
 * @return       An IntervalValue representing the lower and upper bounds of the range.
 */
IntervalValue SVFIR2AbsState::getRangeLimitFromType(const SVFType* type)
{
    if (const SVFIntegerType* intType = SVFUtil::dyn_cast<SVFIntegerType>(type))
    {
        u32_t bits = type->getByteSize() * 8;
        s64_t ub = 0;
        s64_t lb = 0;
        if (bits >= 32)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<s32_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<s32_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u32_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u32_t>::min());
            }
        }
        else if (bits == 16)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<s16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<s16_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u16_t>::min());
            }
        }
        else if (bits == 8)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<int8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<int8_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u_int8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u_int8_t>::min());
            }
        }
        return IntervalValue(lb, ub);
    }
    else if (SVFUtil::isa<SVFOtherType>(type))
    {
        // handle other type like float double, set s32_t as the range
        s64_t ub = static_cast<s64_t>(std::numeric_limits<s32_t>::max());
        s64_t lb = static_cast<s64_t>(std::numeric_limits<s32_t>::min());
        return IntervalValue(lb, ub);
    }
    else
    {
        return IntervalValue::top();
        // other types, return top interval
    }
}

IntervalValue SVFIR2AbsState::getZExtValue(const AbstractState& as, const SVFVar* var)
{
    const SVFType* type = var->getType();
    if (SVFUtil::isa<SVFIntegerType>(type))
    {
        u32_t bits = type->getByteSize() * 8;
        if (as[var->getId()].getInterval().is_numeral())
        {
            if (bits == 8)
            {
                int8_t signed_i8_value = as[var->getId()].getInterval().getIntNumeral();
                u32_t unsigned_value = static_cast<uint8_t>(signed_i8_value);
                return IntervalValue(unsigned_value, unsigned_value);
            }
            else if (bits == 16)
            {
                s16_t signed_i16_value = as[var->getId()].getInterval().getIntNumeral();
                u32_t unsigned_value = static_cast<u16_t>(signed_i16_value);
                return IntervalValue(unsigned_value, unsigned_value);
            }
            else if (bits == 32)
            {
                s32_t signed_i32_value = as[var->getId()].getInterval().getIntNumeral();
                u32_t unsigned_value = static_cast<u32_t>(signed_i32_value);
                return IntervalValue(unsigned_value, unsigned_value);
            }
            else if (bits == 64)
            {
                s64_t signed_i64_value = as[var->getId()].getInterval().getIntNumeral();
                return IntervalValue((s64_t)signed_i64_value, (s64_t)signed_i64_value);
                // we only support i64 at most
            }
            else
            {
                assert(false && "cannot support int type other than u8/16/32/64");
            }
        }
        else
        {
            return IntervalValue::top(); // TODO: may have better solution
        }
    }
    return IntervalValue::top(); // TODO: may have better solution
}

IntervalValue SVFIR2AbsState::getSExtValue(const AbstractState& as, const SVFVar* var)
{
    return as[var->getId()].getInterval();
}

IntervalValue SVFIR2AbsState::getFPToSIntValue(const AbstractState& as, const SVF::SVFVar* var)
{
    if (as[var->getId()].getInterval().is_real())
    {
        // get the float value of ub and lb
        double float_lb = as[var->getId()].getInterval().lb().getRealNumeral();
        double float_ub = as[var->getId()].getInterval().ub().getRealNumeral();
        // get the int value of ub and lb
        s64_t int_lb = static_cast<s64_t>(float_lb);
        s64_t int_ub = static_cast<s64_t>(float_ub);
        return IntervalValue(int_lb, int_ub);
    }
    else
    {
        return getSExtValue(as, var);
    }
}

IntervalValue SVFIR2AbsState::getFPToUIntValue(const AbstractState& as, const SVF::SVFVar* var)
{
    if (as[var->getId()].getInterval().is_real())
    {
        // get the float value of ub and lb
        double float_lb = as[var->getId()].getInterval().lb().getRealNumeral();
        double float_ub = as[var->getId()].getInterval().ub().getRealNumeral();
        // get the int value of ub and lb
        u64_t int_lb = static_cast<u64_t>(float_lb);
        u64_t int_ub = static_cast<u64_t>(float_ub);
        return IntervalValue(int_lb, int_ub);
    }
    else
    {
        return getZExtValue(as, var);
    }
}

IntervalValue SVFIR2AbsState::getSIntToFPValue(const AbstractState& as, const SVF::SVFVar* var)
{
    // get the sint value of ub and lb
    s64_t sint_lb = as[var->getId()].getInterval().lb().getIntNumeral();
    s64_t sint_ub = as[var->getId()].getInterval().ub().getIntNumeral();
    // get the float value of ub and lb
    double float_lb = static_cast<double>(sint_lb);
    double float_ub = static_cast<double>(sint_ub);
    return IntervalValue(float_lb, float_ub);
}

IntervalValue SVFIR2AbsState::getUIntToFPValue(const AbstractState& as, const SVF::SVFVar* var)
{
    // get the uint value of ub and lb
    u64_t uint_lb = as[var->getId()].getInterval().lb().getIntNumeral();
    u64_t uint_ub = as[var->getId()].getInterval().ub().getIntNumeral();
    // get the float value of ub and lb
    double float_lb = static_cast<double>(uint_lb);
    double float_ub = static_cast<double>(uint_ub);
    return IntervalValue(float_lb, float_ub);
}

IntervalValue SVFIR2AbsState::getTruncValue(const AbstractState& as, const SVF::SVFVar* var, const SVFType* dstType)
{
    const IntervalValue& itv = as[var->getId()].getInterval();
    if(itv.isBottom()) return itv;
    // get the value of ub and lb
    s64_t int_lb = itv.lb().getIntNumeral();
    s64_t int_ub = itv.ub().getIntNumeral();
    // get dst type
    u32_t dst_bits = dstType->getByteSize() * 8;
    if (dst_bits == 8)
    {
        // get the signed value of ub and lb
        int8_t s8_lb = static_cast<int8_t>(int_lb);
        int8_t s8_ub = static_cast<int8_t>(int_ub);
        if (s8_lb > s8_ub)
        {
            // return range of s8
            return IntervalValue::top();
        }
        return IntervalValue(s8_lb, s8_ub);
    }
    else if (dst_bits == 16)
    {
        // get the signed value of ub and lb
        s16_t s16_lb = static_cast<s16_t>(int_lb);
        s16_t s16_ub = static_cast<s16_t>(int_ub);
        if (s16_lb > s16_ub)
        {
            // return range of s16
            return IntervalValue::top();
        }
        return IntervalValue(s16_lb, s16_ub);
    }
    else if (dst_bits == 32)
    {
        // get the signed value of ub and lb
        s32_t s32_lb = static_cast<s32_t>(int_lb);
        s32_t s32_ub = static_cast<s32_t>(int_ub);
        if (s32_lb > s32_ub)
        {
            // return range of s32
            return IntervalValue::top();
        }
        return IntervalValue(s32_lb, s32_ub);
    }
    else
    {
        assert(false && "cannot support dst int type other than u8/16/32");
    }
}

IntervalValue SVFIR2AbsState::getFPTruncValue(const AbstractState& as, const SVF::SVFVar* var, const SVFType* dstType)
{
    // TODO: now we do not really handle fptrunc
    return as[var->getId()].getInterval();
}

void SVFIR2AbsState::widenAddrs(AbstractState& as, AbstractState&lhs, const AbstractState&rhs)
{
    for (const auto &rhsItem: rhs._varToAbsVal)
    {
        auto lhsIter = lhs._varToAbsVal.find(rhsItem.first);
        if (lhsIter != lhs._varToAbsVal.end())
        {
            if (rhsItem.second.isAddr())
            {
                for (const auto &addr: rhsItem.second.getAddrs())
                {
                    if (!lhsIter->second.getAddrs().contains(addr))
                    {
                        for (s32_t i = 0; i < (s32_t) Options::MaxFieldLimit(); i++)
                        {
                            lhsIter->second.join_with(getGepObjAddress(as, getInternalID(addr), i));
                        }
                    }
                }
            }
        }
    }
    for (const auto &rhsItem: rhs._addrToAbsVal)
    {
        auto lhsIter = lhs._addrToAbsVal.find(rhsItem.first);
        if (lhsIter != lhs._addrToAbsVal.end())
        {
            if (rhsItem.second.isAddr())
            {
                for (const auto& addr : rhsItem.second.getAddrs())
                {
                    if (!lhsIter->second.getAddrs().contains(addr))
                    {
                        for (s32_t i = 0; i < (s32_t)Options::MaxFieldLimit();
                                i++)
                        {
                            lhsIter->second.join_with(
                                getGepObjAddress(as, getInternalID(addr), i));
                        }
                    }
                }
            }
        }
    }
}

void SVFIR2AbsState::narrowAddrs(AbstractState& as, AbstractState&lhs, const AbstractState&rhs)
{
    for (const auto &rhsItem: rhs._varToAbsVal)
    {
        auto lhsIter = lhs._varToAbsVal.find(rhsItem.first);
        if (lhsIter != lhs._varToAbsVal.end())
        {
            if (lhsIter->second.isAddr())
            {
                for (const auto &addr: lhsIter->second.getAddrs())
                {
                    if (!rhsItem.second.getAddrs().contains(addr))
                    {
                        lhsIter->second = rhsItem.second;
                        break;
                    }
                }
            }
        }
    }
    for (const auto &rhsItem: rhs._addrToAbsVal)
    {
        auto lhsIter = lhs._addrToAbsVal.find(rhsItem.first);
        if (lhsIter != lhs._addrToAbsVal.end())
        {
            if (lhsIter->second.isAddr())
            {
                for (const auto& addr : lhsIter->second.getAddrs())
                {
                    if (!rhsItem.second.getAddrs().contains(addr))
                    {
                        lhsIter->second = rhsItem.second;
                        break;
                    }
                }
            }
        }
    }
}

AddressValue SVFIR2AbsState::getGepObjAddress(AbstractState& as, u32_t pointer, APOffset offset)
{
    AbstractValue addrs = getAddrs(as, pointer);
    AddressValue ret = AddressValue();
    for (const auto &addr: addrs.getAddrs())
    {
        s64_t baseObj = getInternalID(addr);
        assert(SVFUtil::isa<ObjVar>(_svfir->getGNode(baseObj)) && "Fail to get the base object address!");
        NodeID gepObj = _svfir->getGepObjVar(baseObj, offset);
        as[gepObj] = AddressValue(AbstractState::getVirtualMemAddress(gepObj));
        ret.insert(AbstractState::getVirtualMemAddress(gepObj));
    }
    return ret;
}

/**
 * This function, getByteOffset, calculates the byte offset for a given GepStmt.
 *
 * @param gep   The GepStmt representing the GetElementPtr instruction.
 *
 * @return      The calculated byte offset as an IntervalValue.
 *
 * It is byte offset rather than flatten index.
 * e.g. %var2 = getelementptr inbounds %struct.OuterStruct, %struct.OuterStruct* %var0, i64 0, i32 2, i32 0, i64 %var1
* %struct.OuterStruct = type { i32, i32, %struct.InnerStruct }
* %struct.InnerStruct = type { [2 x i32] }
* there are 4 GepTypePairs (<0, %struct.OuterStruct*>, <2, %struct.OuterStruct>, <0, %struct.InnerStruct>, <%var1, [2xi32]>)
* this function process arr/ptr subtype by calculating elemByteSize * indexOperand
 *   and process struct subtype by calculating the byte offset from beginning to the field of struct
* e.g. for 0th pair <0, %struct.OuterStruct*>, it is 0* ptrSize(%struct.OuterStruct*) = 0 bytes
*      for 1st pair <2, %struct.OuterStruct>, it is 2nd field in %struct.OuterStruct = 8 bytes
*      for 2nd pair <0, %struct.InnerStruct>, it is 0th field in %struct.InnerStruct = 0 bytes
*      for 3rd pair <%var1, [2xi32]>, it is %var1'th element in array [2xi32] = 4bytes * %var1
*      ----
*  Therefore the final byteoffset is [8+4*var1.lb(), 8+4*var1.ub()]
 *
 */
IntervalValue SVFIR2AbsState::getByteOffset(const AbstractState& as, const GepStmt *gep)
{
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantByteOffset());
    IntervalValue res = IntervalValue(0); // Initialize the result interval 'res' to 0.
    // Loop through the offsetVarAndGepTypePairVec in reverse order.
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const SVFVar* idxOperandVar =
            gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* idxOperandType =
            gep->getOffsetVarAndGepTypePairVec()[i].second;
        // calculating Array/Ptr by elemByteSize * indexOperand
        if (SVFUtil::isa<SVFArrayType>(idxOperandType) || SVFUtil::isa<SVFPointerType>(idxOperandType))
        {
            u32_t elemByteSize = 1;
            if (const SVFArrayType* arrOperandType = SVFUtil::dyn_cast<SVFArrayType>(idxOperandType))
                elemByteSize = arrOperandType->getTypeOfElement()->getByteSize();
            else if (SVFUtil::isa<SVFPointerType>(idxOperandType))
                elemByteSize = gep->getAccessPath().gepSrcPointeeType()->getByteSize();
            else
                assert(false && "idxOperandType must be ArrType or PtrType");
            if (const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(idxOperandVar->getValue()))
            {
                s64_t lb = (double)Options::MaxFieldLimit() / elemByteSize >= op->getSExtValue() ?op->getSExtValue() * elemByteSize
                           : Options::MaxFieldLimit();
                res = res + IntervalValue(lb, lb);
            }
            else
            {
                u32_t idx = _svfir->getValueNode(idxOperandVar->getValue());
                IntervalValue idxVal = as[idx].getInterval();
                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                {
                    // if lb or ub is negative number, set 0.
                    // if lb or ub is positive number, guarantee lb/ub * elemByteSize <= MaxFieldLimit
                    s64_t ub = (idxVal.ub().getIntNumeral() < 0) ? 0 :
                               (double)Options::MaxFieldLimit() /
                               elemByteSize >= idxVal.ub().getIntNumeral() ? elemByteSize * idxVal.ub().getIntNumeral(): Options::MaxFieldLimit();
                    s64_t lb = (idxVal.lb().getIntNumeral() < 0) ? 0 :
                               ((double)Options::MaxFieldLimit() /
                                elemByteSize >= idxVal.lb().getIntNumeral()) ? elemByteSize * idxVal.lb().getIntNumeral() : Options::MaxFieldLimit();
                    res = res + IntervalValue(lb, ub);
                }
            }
        }
        // Process struct subtype by calculating the byte offset from beginning to the field of struct
        else if (const SVFStructType* structOperandType = SVFUtil::dyn_cast<SVFStructType>(idxOperandType))
        {
            res = res + IntervalValue(gep->getAccessPath().getStructFieldOffset(
                                          idxOperandVar, structOperandType));
        }
        else
        {
            assert(false && "gep type pair only support arr/ptr/struct");
        }
    }
    return res; // Return the resulting byte offset as an IntervalValue.
}

/**
 * This function, getElementIndex, calculates the offset range as a pair
 * of APOffset values for a given GepStmt.
 *
 * @param gep   The GepStmt representing the GetElementPtr instruction.
 *
 * @return      A pair of APOffset values representing the offset range.
 */
IntervalValue SVFIR2AbsState::getElementIndex(const AbstractState& as, const GepStmt *gep)
{
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantOffset());
    IntervalValue res = IntervalValue(0);
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        AccessPath::IdxOperandPair IdxVarAndType =
            gep->getOffsetVarAndGepTypePairVec()[i];
        const SVFValue *value =
            gep->getOffsetVarAndGepTypePairVec()[i].first->getValue();
        const SVFType *type = IdxVarAndType.second;
        // idxLb/Ub is the flattened offset generated by the current OffsetVarAndGepTypePair
        s64_t idxLb;
        s64_t idxUb;
        // get lb and ub of the index value
        if (const SVFConstantInt* constInt = SVFUtil::dyn_cast<SVFConstantInt>(value))
            idxLb = idxUb = constInt->getSExtValue();
        else
        {
            IntervalValue idxItv = as[_svfir->getValueNode(value)].getInterval();
            if (idxItv.isBottom())
                idxLb = idxUb = 0;
            else
            {
                idxLb = idxItv.lb().getIntNumeral();
                idxUb = idxItv.ub().getIntNumeral();
            }
        }
        // for pointer type, flattened index = elemNum * idx
        if (SVFUtil::isa<SVFPointerType>(type))
        {
            u32_t elemNum = gep->getAccessPath().getElementNum(gep->getAccessPath().gepSrcPointeeType());
            idxLb = (double)Options::MaxFieldLimit() / elemNum < idxLb? Options::MaxFieldLimit(): idxLb * elemNum;
            idxUb = (double)Options::MaxFieldLimit() / elemNum < idxUb? Options::MaxFieldLimit(): idxUb * elemNum;
        }
        // for array or struct, get flattened index from SymbolTable Info
        else
        {
            if(Options::ModelArrays())
            {
                const std::vector<u32_t>& so = SymbolTableInfo::SymbolInfo()
                                               ->getTypeInfo(type)
                                               ->getFlattenedElemIdxVec();
                if (so.empty() || idxUb >= (APOffset)so.size() || idxLb < 0)
                {
                    idxLb = idxUb = 0;
                }
                else
                {
                    idxLb = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(
                                type, idxLb);
                    idxUb = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(
                                type, idxUb);
                }
            }
            else
                idxLb = idxUb = 0;
        }
        res = res + IntervalValue(idxLb, idxUb);
    }
    res.meet_with(IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit()));
    if (res.isBottom())
    {
        res = IntervalValue(0);
    }
    return res;
}


void SVFIR2AbsState::initObjVar(AbstractState& as, const ObjVar* var)
{
    NodeID varId = var->getId();
    if (var->hasValue())
    {
        const MemObj *obj = var->getMemObj();
        /// constant data
        if (obj->isConstDataOrConstGlobal() || obj->isConstantArray() || obj->isConstantStruct())
        {
            if (const SVFConstantInt *consInt = SVFUtil::dyn_cast<SVFConstantInt>(obj->getValue()))
            {
                s64_t numeral = consInt->getSExtValue();
                as[varId] = IntervalValue(numeral, numeral);
            }
            else if (const SVFConstantFP* consFP = SVFUtil::dyn_cast<SVFConstantFP>(obj->getValue()))
                as[varId] = IntervalValue(consFP->getFPValue(), consFP->getFPValue());
            else if (SVFUtil::isa<SVFConstantNullPtr>(obj->getValue()))
                as[varId] = IntervalValue(0, 0);
            else if (SVFUtil::isa<SVFGlobalValue>(obj->getValue()))
            {
                as[varId] = AddressValue(getVirtualMemAddress(varId));
            }

            else if (obj->isConstantArray() || obj->isConstantStruct())
                as[varId] = IntervalValue::top();
            else
                as[varId] = IntervalValue::top();
        }
        else
            as[varId] = AddressValue(getVirtualMemAddress(varId));
    }
    else
        as[varId] = AddressValue(getVirtualMemAddress(varId));
}


void SVFIR2AbsState::handleAddr(AbstractState& as, const AddrStmt *addr)
{
    initObjVar(as, SVFUtil::cast<ObjVar>(addr->getRHSVar()));
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        as[addr->getRHSVarID()].getInterval().meet_with(getRangeLimitFromType(addr->getRHSVar()->getType()));
    as[addr->getLHSVarID()] = as[addr->getRHSVarID()];
}


void SVFIR2AbsState::handleBinary(AbstractState& as, const BinaryOPStmt *binary)
{
    u32_t op0 = binary->getOpVarID(0);
    u32_t op1 = binary->getOpVarID(1);
    u32_t res = binary->getResID();
    if (!inVarToValTable(as, op0)) as[op0] = IntervalValue::top();
    if (!inVarToValTable(as, op1)) as[op1] = IntervalValue::top();
    IntervalValue &lhs = as[op0].getInterval(), &rhs = as[op1].getInterval();
    IntervalValue resVal;
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    case BinaryOPStmt::FAdd:
        resVal = (lhs + rhs);
        break;
    case BinaryOPStmt::Sub:
    case BinaryOPStmt::FSub:
        resVal = (lhs - rhs);
        break;
    case BinaryOPStmt::Mul:
    case BinaryOPStmt::FMul:
        resVal = (lhs * rhs);
        break;
    case BinaryOPStmt::SDiv:
    case BinaryOPStmt::FDiv:
    case BinaryOPStmt::UDiv:
        resVal = (lhs / rhs);
        break;
    case BinaryOPStmt::SRem:
    case BinaryOPStmt::FRem:
    case BinaryOPStmt::URem:
        resVal = (lhs % rhs);
        break;
    case BinaryOPStmt::Xor:
        resVal = (lhs ^ rhs);
        break;
    case BinaryOPStmt::And:
        resVal = (lhs & rhs);
        break;
    case BinaryOPStmt::Or:
        resVal = (lhs | rhs);
        break;
    case BinaryOPStmt::AShr:
        resVal = (lhs >> rhs);
        break;
    case BinaryOPStmt::Shl:
        resVal = (lhs << rhs);
        break;
    case BinaryOPStmt::LShr:
        resVal = (lhs >> rhs);
        break;
    default:
        assert(false && "undefined binary: ");
    }
    as[res] = resVal;
}

void SVFIR2AbsState::handleCmp(AbstractState& as, const CmpStmt *cmp)
{
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    if (!inVarToValTable(as, op0)) as[op0] = IntervalValue::top();
    if (!inVarToValTable(as, op1)) as[op1] = IntervalValue::top();
    u32_t res = cmp->getResID();
    if (inVarToValTable(as, op0) && inVarToValTable(as, op1))
    {
        IntervalValue resVal;
        IntervalValue &lhs = as[op0].getInterval(), &rhs = as[op1].getInterval();
        //AbstractValue
        auto predicate = cmp->getPredicate();
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
            resVal = (lhs == rhs);
            // resVal = (lhs.getInterval() == rhs.getInterval());
            break;
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
            resVal = (lhs != rhs);
            break;
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
            resVal = (lhs > rhs);
            break;
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
            resVal = (lhs >= rhs);
            break;
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
            resVal = (lhs < rhs);
            break;
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
            resVal = (lhs <= rhs);
            break;
        case CmpStmt::FCMP_FALSE:
            resVal = IntervalValue(0, 0);
            break;
        case CmpStmt::FCMP_TRUE:
            resVal = IntervalValue(1, 1);
            break;
        default:
        {
            assert(false && "undefined compare: ");
        }
        }
        as[res] = resVal;
    }
    else if (inVarToAddrsTable(as, op0) && inVarToAddrsTable(as, op1))
    {
        IntervalValue resVal;
        AbstractValue &lhs = getAddrs(as, op0), &rhs = getAddrs(as, op1);
        auto predicate = cmp->getPredicate();
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
        {
            if (lhs.getAddrs().hasIntersect(rhs.getAddrs()))
            {
                resVal = IntervalValue(0, 1);
            }
            else if (lhs.getAddrs().empty() && rhs.getAddrs().empty())
            {
                resVal = IntervalValue(1, 1);
            }
            else
            {
                resVal = IntervalValue(0, 0);
            }
            break;
        }
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
        {
            if (lhs.getAddrs().hasIntersect(rhs.getAddrs()))
            {
                resVal = IntervalValue(0, 1);
            }
            else if (lhs.getAddrs().empty() && rhs.getAddrs().empty())
            {
                resVal = IntervalValue(0, 0);
            }
            else
            {
                resVal = IntervalValue(1, 1);
            }
            break;
        }
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
        {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1)
            {
                resVal = IntervalValue(*lhs.getAddrs().begin() > *rhs.getAddrs().begin());
            }
            else
            {
                resVal = IntervalValue(0, 1);
            }
            break;
        }
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
        {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1)
            {
                resVal = IntervalValue(*lhs.getAddrs().begin() >= *rhs.getAddrs().begin());
            }
            else
            {
                resVal = IntervalValue(0, 1);
            }
            break;
        }
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
        {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1)
            {
                resVal = IntervalValue(*lhs.getAddrs().begin() < *rhs.getAddrs().begin());
            }
            else
            {
                resVal = IntervalValue(0, 1);
            }
            break;
        }
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
        {
            if (lhs.getAddrs().size() == 1 && rhs.getAddrs().size() == 1)
            {
                resVal = IntervalValue(*lhs.getAddrs().begin() <= *rhs.getAddrs().begin());
            }
            else
            {
                resVal = IntervalValue(0, 1);
            }
            break;
        }
        case CmpStmt::FCMP_FALSE:
            resVal = IntervalValue(0, 0);
            break;
        case CmpStmt::FCMP_TRUE:
            resVal = IntervalValue(1, 1);
            break;
        default:
        {
            assert(false && "undefined compare: ");
        }
        }
        as[res] = resVal;
    }
}

void SVFIR2AbsState::handleLoad(AbstractState& as, const LoadStmt *load)
{
    u32_t rhs = load->getRHSVarID();
    u32_t lhs = load->getLHSVarID();
    AbstractValue &addrs = as[rhs];
    AbstractValue rhsVal; // interval::bottom Address::bottom
    // AbstractValue absRhs
    for (const auto &addr: addrs.getAddrs())
        rhsVal.join_with(as.load(addr));
    as[lhs] = rhsVal;
}

void SVFIR2AbsState::handleStore(AbstractState& as, const StoreStmt *store)
{
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();

    for (const auto &addr: as[lhs].getAddrs())
    {
        as.store(addr, as[rhs]);
    }
}

void SVFIR2AbsState::handleCopy(AbstractState& as, const CopyStmt *copy)
{
    u32_t lhs = copy->getLHSVarID();
    u32_t rhs = copy->getRHSVarID();

    if (copy->getCopyKind() == CopyStmt::COPYVAL)
    {
        as[lhs] = as[rhs];
    }
    else if (copy->getCopyKind() == CopyStmt::ZEXT)
    {
        as[lhs] = getZExtValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::SEXT)
    {
        as[lhs] = getSExtValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOSI)
    {
        as[lhs] = getFPToSIntValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOUI)
    {
        as[lhs] = getFPToUIntValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::SITOFP)
    {
        as[lhs] = getSIntToFPValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::UITOFP)
    {
        as[lhs] = getUIntToFPValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::TRUNC)
    {
        as[lhs] = getTruncValue(as, copy->getRHSVar(), copy->getLHSVar()->getType());
    }
    else if (copy->getCopyKind() == CopyStmt::FPTRUNC)
    {
        as[lhs] = getFPTruncValue(as, copy->getRHSVar(), copy->getLHSVar()->getType());
    }
    else if (copy->getCopyKind() == CopyStmt::INTTOPTR)
    {
        //insert nullptr
    }
    else if (copy->getCopyKind() == CopyStmt::PTRTOINT)
    {
        as[lhs] = IntervalValue::top();
    }
    else if (copy->getCopyKind() == CopyStmt::BITCAST)
    {
        if (as[rhs].isAddr())
        {
            as[lhs] = as[rhs];
        }
        else
        {
            // do nothing
        }
    }
    else
    {
        assert(false && "undefined copy kind");
        abort();
    }
}

void SVFIR2AbsState::handleGep(AbstractState& as, const GepStmt *gep)
{
    u32_t rhs = gep->getRHSVarID();
    u32_t lhs = gep->getLHSVarID();
    IntervalValue offsetPair = getElementIndex(as, gep);
    AbstractValue gepAddrs;
    APOffset lb = offsetPair.lb().getIntNumeral() < Options::MaxFieldLimit()?
                  offsetPair.lb().getIntNumeral(): Options::MaxFieldLimit();
    APOffset ub = offsetPair.ub().getIntNumeral() < Options::MaxFieldLimit()?
                  offsetPair.ub().getIntNumeral(): Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
        gepAddrs.join_with(getGepObjAddress(as, rhs, i));
    as[lhs] = gepAddrs;
}

void SVFIR2AbsState::handleSelect(AbstractState& as, const SelectStmt *select)
{
    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();
    if (as[cond].getInterval().is_numeral())
    {
        as[res] = as[cond].getInterval().is_zero() ? as[fval] : as[tval];
    }
    else
    {
        as[res] = as[tval];
        as[res].join_with(as[fval]);
    }
}

void SVFIR2AbsState::handlePhi(AbstractState& as, const PhiStmt *phi)
{
    u32_t res = phi->getResID();
    AbstractValue rhs;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        NodeID curId = phi->getOpVarID(i);
        rhs.join_with(as[curId]);
    }
    as[res] = rhs;
}


void SVFIR2AbsState::handleCall(AbstractState& as, const CallPE *callPE)
{
    NodeID lhs = callPE->getLHSVarID();
    NodeID rhs = callPE->getRHSVarID();
    as[lhs] = as[rhs];
}

void SVFIR2AbsState::handleRet(AbstractState& as, const RetPE *retPE)
{
    NodeID lhs = retPE->getLHSVarID();
    NodeID rhs = retPE->getRHSVarID();
    as[lhs] = as[rhs];
}
