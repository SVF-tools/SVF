//===- SVFIR2ItvExeState.cpp -- SVF IR Translation to Interval Domain-----//
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
 * SVFIR2ItvExeState.cpp
 *
 *  Created on: Aug 7, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 *
 */

#include "AbstractExecution/SVFIR2ItvExeState.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

SVF::SVFIR2ItvExeState::Addrs SVF::SVFIR2ItvExeState::globalNulladdrs =
    AddressValue();

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
IntervalValue SVFIR2ItvExeState::getRangeLimitFromType(const SVFType* type)
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
                ub = static_cast<s64_t>(std::numeric_limits<int16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<int16_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<uint16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<uint16_t>::min());
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
        assert(false && "cannot support");
    }
}


void SVFIR2ItvExeState::applySummary(IntervalExeState &es)
{
    for (const auto &item: es._varToItvVal)
    {
        _es._varToItvVal[item.first] = item.second;
    }
    for (const auto &item: es._locToItvVal)
    {
        _es._locToItvVal[item.first] = item.second;
    }
    for (const auto &item: es._varToAddrs)
    {
        _es._varToAddrs[item.first] = item.second;
    }
    for (const auto &item: es._locToAddrs)
    {
        _es._locToAddrs[item.first] = item.second;
    }
}

void SVFIR2ItvExeState::moveToGlobal()
{
    for (const auto &it: _es._varToItvVal)
    {
        IntervalExeState::globalES._varToItvVal.insert(it);
    }
    for (const auto &it: _es._locToItvVal)
    {
        IntervalExeState::globalES._locToItvVal.insert(it);
    }
    for (const auto &_varToVAddr: _es._varToAddrs)
    {
        IntervalExeState::globalES._varToAddrs.insert(_varToVAddr);
    }
    for (const auto &_locToVAddr: _es._locToAddrs)
    {
        IntervalExeState::globalES._locToAddrs.insert(_locToVAddr);
    }

    _es._varToItvVal.clear();
    IntervalExeState::globalES._varToItvVal.erase(PAG::getPAG()->getBlkPtr());
    _es._varToItvVal[PAG::getPAG()->getBlkPtr()] = IntervalValue::top();
    _es._locToItvVal.clear();
    _es._varToAddrs.clear();
    _es._locToAddrs.clear();
}

void SVFIR2ItvExeState::widenAddrs(IntervalExeState &lhs, const IntervalExeState &rhs)
{
    for (const auto &rhsItem: rhs._varToAddrs)
    {
        auto lhsIter = lhs._varToAddrs.find(rhsItem.first);
        if (lhsIter != lhs._varToAddrs.end())
        {
            for (const auto &addr: rhsItem.second)
            {
                if (!lhsIter->second.contains(addr))
                {
                    for (s32_t i = 0; i < (s32_t) Options::MaxFieldLimit(); i++)
                    {
                        lhsIter->second.join_with(getGepObjAddress(getInternalID(addr), i));
                    }
                }
            }
        }
    }
    for (const auto &rhsItem: rhs._locToAddrs)
    {
        auto lhsIter = lhs._locToAddrs.find(rhsItem.first);
        if (lhsIter != lhs._locToAddrs.end())
        {
            for (const auto &addr: rhsItem.second)
            {
                if (!lhsIter->second.contains(addr))
                {
                    for (s32_t i = 0; i < (s32_t) Options::MaxFieldLimit(); i++)
                    {
                        lhsIter->second.join_with(getGepObjAddress(getInternalID(addr), i));
                    }
                }
            }
        }
    }
}

void SVFIR2ItvExeState::narrowAddrs(IntervalExeState &lhs, const IntervalExeState &rhs)
{
    for (const auto &rhsItem: rhs._varToAddrs)
    {
        auto lhsIter = lhs._varToAddrs.find(rhsItem.first);
        if (lhsIter != lhs._varToAddrs.end())
        {
            for (const auto &addr: lhsIter->second)
            {
                if (!rhsItem.second.contains(addr))
                {
                    lhsIter->second = rhsItem.second;
                    break;
                }
            }
        }
    }
    for (const auto &rhsItem: rhs._locToAddrs)
    {
        auto lhsIter = lhs._locToAddrs.find(rhsItem.first);
        if (lhsIter != lhs._locToAddrs.end())
        {
            for (const auto &addr: lhsIter->second)
            {
                if (!rhsItem.second.contains(addr))
                {
                    lhsIter->second = rhsItem.second;
                    break;
                }
            }
        }
    }
}

SVFIR2ItvExeState::Addrs SVFIR2ItvExeState::getGepObjAddress(u32_t pointer, APOffset offset)
{
    assert(!getAddrs(pointer).empty());
    Addrs &addrs = getAddrs(pointer);
    Addrs ret;
    for (const auto &addr: addrs)
    {
        int64_t baseObj = getInternalID(addr);
        if (baseObj == 0)
        {
            ret.insert(getVirtualMemAddress(0));
            continue;
        }
        assert(SVFUtil::isa<ObjVar>(_svfir->getGNode(baseObj)) && "Fail to get the base object address!");
        NodeID gepObj = _svfir->getGepObjVar(baseObj, offset);
        initSVFVar(gepObj);
        ret.insert(getVirtualMemAddress(gepObj));
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
IntervalValue SVFIR2ItvExeState::getByteOffset(const GepStmt *gep)
{
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantByteOffset());
    IntervalValue res(0); // Initialize the result interval 'res' to 0.
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
                IntervalValue idxVal = _es[idx];
                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                {
                    // if lb or ub is negative number, set 0.
                    // if lb or ub is positive number, guarantee lb/ub * elemByteSize <= MaxFieldLimit
                    s64_t ub = (idxVal.ub().getNumeral() < 0) ? 0 :
                               (double)Options::MaxFieldLimit() /
                               elemByteSize >= idxVal.ub().getNumeral() ? elemByteSize * idxVal.ub().getNumeral(): Options::MaxFieldLimit();
                    s64_t lb = (idxVal.lb().getNumeral() < 0) ? 0 :
                               ((double)Options::MaxFieldLimit() /
                                elemByteSize >= idxVal.lb().getNumeral()) ? elemByteSize * idxVal.lb().getNumeral() : Options::MaxFieldLimit();
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
 * This function, getItvOfFlattenedElemIndex, calculates the offset range as a pair
 * of APOffset values for a given GepStmt.
 *
 * @param gep   The GepStmt representing the GetElementPtr instruction.
 *
 * @return      A pair of APOffset values representing the offset range.
 */
IntervalValue SVFIR2ItvExeState::getItvOfFlattenedElemIndex(const GepStmt *gep)
{
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantOffset());
    IntervalValue res(0);
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
            IntervalValue idxItv = _es[_svfir->getValueNode(value)];
            if (idxItv.isBottom())
                idxLb = idxUb = 0;
            else
            {
                idxLb = idxItv.lb().getNumeral();
                idxUb = idxItv.ub().getNumeral();
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


/*!
 * Init Z3Expr for ObjVar
 * @param objVar
 * @param valExprIdToCondValPairMap
 */
void SVFIR2ItvExeState::initObjVar(const ObjVar *objVar, u32_t varId)
{

    if (objVar->hasValue())
    {
        const MemObj *obj = objVar->getMemObj();
        /// constant data
        if (obj->isConstDataOrConstGlobal() || obj->isConstantArray() || obj->isConstantStruct())
        {
            if (const SVFConstantInt *consInt = SVFUtil::dyn_cast<SVFConstantInt>(obj->getValue()))
            {
                double numeral = (double)consInt->getSExtValue();
                IntervalExeState::globalES[varId] = IntervalValue(numeral, numeral);
            }
            else if (const SVFConstantFP* consFP = SVFUtil::dyn_cast<SVFConstantFP>(obj->getValue()))
                IntervalExeState::globalES[varId] = IntervalValue(consFP->getFPValue(), consFP->getFPValue());
            else if (SVFUtil::isa<SVFConstantNullPtr>(obj->getValue()))
                IntervalExeState::globalES[varId] = IntervalValue(0, 0);
            else if (SVFUtil::isa<SVFGlobalValue>(obj->getValue()))
                IntervalExeState::globalES.getAddrs(varId).insert(getVirtualMemAddress(varId));
            else if (obj->isConstantArray() || obj->isConstantStruct())
                IntervalExeState::globalES[varId] = IntervalValue::top();
            else
                IntervalExeState::globalES[varId] = IntervalValue::top();
        }
        else
            IntervalExeState::globalES.getAddrs(varId).insert(getVirtualMemAddress(varId));
    }
    else
        IntervalExeState::globalES.getAddrs(varId).insert(getVirtualMemAddress(varId));
}

void SVFIR2ItvExeState::initSVFVar(u32_t varId)
{
    if (inVarToValTable(varId) || _es.inVarToAddrsTable(varId)) return;
    SVFIR *svfir = PAG::getPAG();
    SVFVar *svfVar = svfir->getGNode(varId);
    // write objvar into cache instead of exestate
    if (const ObjVar *objVar = dyn_cast<ObjVar>(svfVar))
    {
        initObjVar(objVar, varId);
        return;
    }
    else
    {
        assert(false && "not an obj var?");
    }
}


void SVFIR2ItvExeState::translateAddr(const AddrStmt *addr)
{
    initSVFVar(addr->getRHSVarID());
    if (inVarToValTable(addr->getRHSVarID()))
    {
        // if addr RHS is integerType(i8 i32 etc), value should be limited.
        if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        {
            IntervalExeState::globalES[addr->getRHSVarID()].meet_with(
                getRangeLimitFromType(addr->getRHSVar()->getType()));
        }
        IntervalExeState::globalES[addr->getLHSVarID()] = IntervalExeState::globalES[addr->getRHSVarID()];

    }
    else if (inVarToAddrsTable(addr->getRHSVarID()))
    {
        IntervalExeState::globalES.getAddrs(addr->getLHSVarID()) = IntervalExeState::globalES.getAddrs(
                    addr->getRHSVarID());
    }
    else
    {
        assert(false && "not number or virtual addrs?");
    }
}


void SVFIR2ItvExeState::translateBinary(const BinaryOPStmt *binary)
{
    u32_t op0 = binary->getOpVarID(0);
    u32_t op1 = binary->getOpVarID(1);
    u32_t res = binary->getResID();
    if (!inVarToValTable(op0)) _es[op0] = IntervalValue::top();
    if (!inVarToValTable(op1)) _es[op1] = IntervalValue::top();
    if (inVarToValTable(op0) && inVarToValTable(op1))
    {
        IntervalValue &lhs = _es[op0], &rhs = _es[op1];
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
        {
            assert(false && "undefined binary: ");
        }
        }
        _es[res] = resVal;
    }
}

void SVFIR2ItvExeState::translateCmp(const CmpStmt *cmp)
{
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    u32_t res = cmp->getResID();
    if (inVarToValTable(op0) && inVarToValTable(op1))
    {
        IntervalValue resVal;
        IntervalValue &lhs = _es[op0], &rhs = _es[op1];
        auto predicate = cmp->getPredicate();
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
            resVal = (lhs == rhs);
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
        _es[res] = resVal;
    }
    else if (inVarToAddrsTable(op0) && inVarToAddrsTable(op1))
    {
        IntervalValue resVal;
        Addrs &lhs = getAddrs(op0), &rhs = getAddrs(op1);
        assert(!lhs.empty() && !rhs.empty() && "empty address?");
        auto predicate = cmp->getPredicate();
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                resVal = IntervalValue(lhs.equals(rhs));
            }
            else
            {
                if (lhs.hasIntersect(rhs))
                {
                    resVal = IntervalValue::top();
                }
                else
                {
                    resVal = IntervalValue(0);
                }
            }
            break;
        }
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                resVal = IntervalValue(!lhs.equals(rhs));
            }
            else
            {
                if (lhs.hasIntersect(rhs))
                {
                    resVal = IntervalValue::top();
                }
                else
                {
                    resVal = IntervalValue(1);
                }
            }
            break;
        }
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                resVal = IntervalValue(*lhs.begin() > *rhs.begin());
            }
            else
            {
                resVal = IntervalValue::top();
            }
            break;
        }
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                resVal = IntervalValue(*lhs.begin() >= *rhs.begin());
            }
            else
            {
                resVal = IntervalValue::top();
            }
            break;
        }
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                resVal = IntervalValue(*lhs.begin() < *rhs.begin());
            }
            else
            {
                resVal = IntervalValue::top();
            }
            break;
        }
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                resVal = IntervalValue(*lhs.begin() <= *rhs.begin());
            }
            else
            {
                resVal = IntervalValue::top();
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
        _es[res] = resVal;
    }
}

void SVFIR2ItvExeState::translateLoad(const LoadStmt *load)
{
    u32_t rhs = load->getRHSVarID();
    u32_t lhs = load->getLHSVarID();
    if (inVarToAddrsTable(rhs))
    {
        Addrs &addrs = getAddrs(rhs);
        assert(!addrs.empty());
        IntervalValue rhsItv = IntervalValue::bottom();
        AddressValue rhsAddr;
        bool isVal = false, isAddr = false;
        for (const auto &addr: addrs)
        {
            u32_t objId = getInternalID(addr);
            if (inLocToValTable(objId))
            {
                rhsItv.join_with(_es.load(addr));
                isVal = true;
            }
            else if (inLocToAddrsTable(objId))
            {
                rhsAddr.join_with(_es.loadAddrs(addr));
                isAddr = true;
            }
        }
        if (isVal)
        {
            // lhs var is an integer
            _es[lhs] = rhsItv;
        }
        else if (isAddr)
        {
            // lhs var is an address
            _es.getAddrs(lhs) = rhsAddr;
        }
        else
        {
            // rhs not initialized, e.g., absence of a store before load
        }
    }
}

void SVFIR2ItvExeState::translateStore(const StoreStmt *store)
{
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();
    if (inVarToAddrsTable(lhs))
    {
        if (inVarToValTable(rhs))
        {
            assert(!getAddrs(lhs).empty());
            Addrs &addrs = getAddrs(lhs);
            for (const auto &addr: addrs)
            {
                _es.store(addr, _es[rhs]);
            }
        }
        else if (inVarToAddrsTable(rhs))
        {
            assert(!getAddrs(lhs).empty());
            Addrs &addrs = getAddrs(lhs);
            for (const auto &addr: addrs)
            {
                assert(!getAddrs(rhs).empty());
                _es.storeAddrs(addr, getAddrs(rhs));
            }

        }
    }
}

void SVFIR2ItvExeState::translateCopy(const CopyStmt *copy)
{
    u32_t lhs = copy->getLHSVarID();
    u32_t rhs = copy->getRHSVarID();
    if (PAG::getPAG()->isBlkPtr(lhs))
    {
        _es[lhs] = IntervalValue::top();
    }
    else
    {
        if (inVarToValTable(rhs))
        {
            _es[lhs] = _es[rhs];
            // if copy LHS is integerType(i8 i32 etc), value should be limited.
            // this branch can handle bitcast from higher bits integer to
            // lower bits integer. e.g. bitcast i32 to i8
            if (copy->getLHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
            {
                _es[lhs].meet_with(
                    getRangeLimitFromType(copy->getLHSVar()->getType()));
            }
        }
        else if (inVarToAddrsTable(rhs))
        {
            assert(!getAddrs(rhs).empty());
            _es.getAddrs(lhs) = getAddrs(rhs);
        }
    }
}

void SVFIR2ItvExeState::translateGep(const GepStmt *gep)
{
    u32_t rhs = gep->getRHSVarID();
    u32_t lhs = gep->getLHSVarID();
    if (!inVarToAddrsTable(rhs)) return;
    assert(!getAddrs(rhs).empty());
    Addrs &rhsVal = getAddrs(rhs);
    if (rhsVal.empty()) return;
    IntervalValue offsetPair = getItvOfFlattenedElemIndex(gep);
    if (!isVirtualMemAddress(*rhsVal.begin()))
        return;
    else
    {
        Addrs gepAddrs;
        APOffset lb = offsetPair.lb().getNumeral() < Options::MaxFieldLimit()?
                      offsetPair.lb().getNumeral(): Options::MaxFieldLimit();
        APOffset ub = offsetPair.ub().getNumeral() < Options::MaxFieldLimit()?
                      offsetPair.ub().getNumeral(): Options::MaxFieldLimit();
        for (APOffset i = lb; i <= ub; i++)
            gepAddrs.join_with(getGepObjAddress(rhs, i));
        if(gepAddrs.empty()) return;
        _es.getAddrs(lhs) = gepAddrs;
        return;
    }
}

void SVFIR2ItvExeState::translateSelect(const SelectStmt *select)
{
    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();
    if (inVarToValTable(tval) && inVarToValTable(fval) && inVarToValTable(cond))
    {
        if (_es[cond].is_numeral())
        {
            _es[res] = _es[cond].is_zero() ? _es[fval] : _es[tval];
        }
        else
        {
            _es[res] = _es[cond];
        }
    }
    else if (inVarToAddrsTable(tval) && inVarToAddrsTable(fval) && inVarToValTable(cond))
    {
        if (_es[cond].is_numeral())
        {
            assert(!getAddrs(fval).empty());
            assert(!getAddrs(tval).empty());
            _es.getAddrs(res) = _es[cond].is_zero() ? getAddrs(fval) : getAddrs(tval);
        }
    }
}

void SVFIR2ItvExeState::translatePhi(const PhiStmt *phi)
{
    u32_t res = phi->getResID();
    IntervalValue rhsItv = IntervalValue::bottom();
    AddressValue rhsAddr;
    bool isVal = false, isAddr = false;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        NodeID curId = phi->getOpVarID(i);
        if (inVarToValTable(curId))
        {
            rhsItv.join_with(_es[curId]);
            isVal = true;
        }
        else if (inVarToAddrsTable(curId))
        {
            assert(!getAddrs(curId).empty());
            rhsAddr.join_with(getAddrs(curId));
            isAddr = true;
        }
    }
    if (isVal)
    {
        // res var is an integer
        _es[res] = rhsItv;
    }
    else if (isAddr)
    {
        // res var is an address
        _es.getAddrs(res) = rhsAddr;
    }
}


void SVFIR2ItvExeState::translateCall(const CallPE *callPE)
{
    NodeID lhs = callPE->getLHSVarID();
    NodeID rhs = callPE->getRHSVarID();
    if (inVarToValTable(rhs))
    {
        _es[lhs] = _es[rhs];
    }
    else if (inVarToAddrsTable(rhs))
    {
        assert(!getAddrs(rhs).empty());
        _es.getAddrs(lhs) = getAddrs(rhs);
    }
}

void SVFIR2ItvExeState::translateRet(const RetPE *retPE)
{
    NodeID lhs = retPE->getLHSVarID();
    NodeID rhs = retPE->getRHSVarID();
    if (inVarToValTable(rhs))
    {
        _es[lhs] = _es[rhs];
    }
    else if (inVarToAddrsTable(rhs))
    {
        assert(!getAddrs(rhs).empty());
        _es.getAddrs(lhs) = getAddrs(rhs);
    }
}
