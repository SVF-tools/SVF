//===- IntervalExeState.cpp----Interval Domain-------------------------//
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
 * AbstractExeState.cpp
 *
 *  Created on: Jul 9, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#include "AE/Core/AbstractState.h"
#include "Util/SVFUtil.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

bool AbstractState::equals(const AbstractState&other) const
{
    return *this == other;
}

u32_t AbstractState::hash() const
{
    size_t h = getVarToVal().size() * 2;
    Hash<u32_t> hf;
    for (const auto &t: getVarToVal())
    {
        h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    size_t h2 = getLocToVal().size() * 2;
    for (const auto &t: getLocToVal())
    {
        h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }
    Hash<std::pair<u32_t, u32_t>> pairH;
    return pairH({h, h2});
}

AbstractState AbstractState::widening(const AbstractState& other)
{
    // widen interval
    AbstractState es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._addrToAbsVal.begin(); it != es._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._addrToAbsVal.find(key) != other._addrToAbsVal.end())
            if (it->second.isInterval() && other._addrToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._addrToAbsVal.at(key).getInterval());
    }
    return es;
}

AbstractState AbstractState::narrowing(const AbstractState& other)
{
    AbstractState es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().narrow_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._addrToAbsVal.begin(); it != es._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._addrToAbsVal.find(key) != other._addrToAbsVal.end())
            if (it->second.isInterval() && other._addrToAbsVal.at(key).isInterval())
                it->second.getInterval().narrow_with(other._addrToAbsVal.at(key).getInterval());
    }
    return es;

}

/// domain join with other, important! other widen this.
void AbstractState::joinWith(const AbstractState& other)
{
    for (auto it = other._varToAbsVal.begin(); it != other._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAbsVal.find(key);
        if (oit != _varToAbsVal.end())
        {
            oit->second.join_with(it->second);
        }
        else
        {
            _varToAbsVal.emplace(key, it->second);
        }
    }
    for (auto it = other._addrToAbsVal.begin(); it != other._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _addrToAbsVal.find(key);
        if (oit != _addrToAbsVal.end())
        {
            oit->second.join_with(it->second);
        }
        else
        {
            _addrToAbsVal.emplace(key, it->second);
        }
    }
    _freedAddrs.insert(other._freedAddrs.begin(), other._freedAddrs.end());
}

/// domain meet with other, important! other widen this.
void AbstractState::meetWith(const AbstractState& other)
{
    for (auto it = other._varToAbsVal.begin(); it != other._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAbsVal.find(key);
        if (oit != _varToAbsVal.end())
        {
            oit->second.meet_with(it->second);
        }
    }
    for (auto it = other._addrToAbsVal.begin(); it != other._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _addrToAbsVal.find(key);
        if (oit != _addrToAbsVal.end())
        {
            oit->second.meet_with(it->second);
        }
    }
    Set<NodeID> intersection;
    std::set_intersection(_freedAddrs.begin(), _freedAddrs.end(),
                          other._freedAddrs.begin(), other._freedAddrs.end(),
                          std::inserter(intersection, intersection.begin()));
    _freedAddrs = std::move(intersection);
}

// getGepObjAddrs
AddressValue AbstractState::getGepObjAddrs(u32_t pointer, IntervalValue offset)
{
    AddressValue gepAddrs;
    APOffset lb = offset.lb().getIntNumeral() < Options::MaxFieldLimit() ? offset.lb().getIntNumeral()
                  : Options::MaxFieldLimit();
    APOffset ub = offset.ub().getIntNumeral() < Options::MaxFieldLimit() ? offset.ub().getIntNumeral()
                  : Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
    {
        AbstractValue addrs = (*this)[pointer];
        for (const auto& addr : addrs.getAddrs())
        {
            s64_t baseObj = getIDFromAddr(addr);
            assert(SVFUtil::isa<ObjVar>(PAG::getPAG()->getGNode(baseObj)) && "Fail to get the base object address!");
            NodeID gepObj = PAG::getPAG()->getGepObjVar(baseObj, i);
            (*this)[gepObj] = AddressValue(AbstractState::getVirtualMemAddress(gepObj));
            gepAddrs.insert(AbstractState::getVirtualMemAddress(gepObj));
        }
    }

    return gepAddrs;
}
// initObjVar
void AbstractState::initObjVar(ObjVar* objVar)
{
    NodeID varId = objVar->getId();

    // Check if the object variable has an associated value

    const BaseObjVar* obj = PAG::getPAG()->getBaseObject(objVar->getId());

    // Handle constant data, arrays, and structures
    if (obj->isConstDataOrConstGlobal() || obj->isConstantArray() || obj->isConstantStruct())
    {
        if (const ConstIntObjVar* consInt = SVFUtil::dyn_cast<ConstIntObjVar>(objVar))
        {
            s64_t numeral = consInt->getSExtValue();
            (*this)[varId] = IntervalValue(numeral, numeral);
        }
        else if (const ConstFPObjVar* consFP = SVFUtil::dyn_cast<ConstFPObjVar>(objVar))
        {
            (*this)[varId] = IntervalValue(consFP->getFPValue(), consFP->getFPValue());
        }
        else if (SVFUtil::isa<ConstNullPtrObjVar>(objVar))
        {
            (*this)[varId] = IntervalValue(0, 0);
        }
        else if (SVFUtil::isa<GlobalObjVar>(objVar))
        {
            (*this)[varId] = AddressValue(AbstractState::getVirtualMemAddress(varId));
        }
        else if (obj->isConstantArray() || obj->isConstantStruct())
        {
            (*this)[varId] = IntervalValue::top();
        }
        else
        {
            (*this)[varId] = IntervalValue::top();
        }
    }
    // Handle non-constant memory objects
    else
    {
        (*this)[varId] = AddressValue(AbstractState::getVirtualMemAddress(varId));
    }
    return;
}

// getElementIndex
IntervalValue AbstractState::getElementIndex(const GepStmt* gep)
{
    // If the GEP statement has a constant offset, return it directly as the interval value
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantOffset());

    IntervalValue res(0);
    // Iterate over the list of offset variable and type pairs in reverse order
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        AccessPath::IdxOperandPair IdxVarAndType = gep->getOffsetVarAndGepTypePairVec()[i];
        const SVFVar* var = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* type = IdxVarAndType.second;

        // Variables to store the lower and upper bounds of the index value
        s64_t idxLb;
        s64_t idxUb;

        // Determine the lower and upper bounds based on whether the value is a constant
        if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
            idxLb = idxUb = constInt->getSExtValue();
        else
        {
            IntervalValue idxItv = (*this)[var->getId()].getInterval();
            if (idxItv.isBottom())
                idxLb = idxUb = 0;
            else
            {
                idxLb = idxItv.lb().getIntNumeral();
                idxUb = idxItv.ub().getIntNumeral();
            }
        }

        // Adjust the bounds if the type is a pointer
        if (SVFUtil::isa<SVFPointerType>(type))
        {
            u32_t elemNum = gep->getAccessPath().getElementNum(gep->getAccessPath().gepSrcPointeeType());
            idxLb = (double)Options::MaxFieldLimit() / elemNum < idxLb ? Options::MaxFieldLimit() : idxLb * elemNum;
            idxUb = (double)Options::MaxFieldLimit() / elemNum < idxUb ? Options::MaxFieldLimit() : idxUb * elemNum;
        }
        // Adjust the bounds for array or struct types using the symbol table info
        else
        {
            if (Options::ModelArrays())
            {
                const std::vector<u32_t>& so = PAG::getPAG()->getTypeInfo(type)->getFlattenedElemIdxVec();
                if (so.empty() || idxUb >= (APOffset)so.size() || idxLb < 0)
                {
                    idxLb = idxUb = 0;
                }
                else
                {
                    idxLb = PAG::getPAG()->getFlattenedElemIdx(type, idxLb);
                    idxUb = PAG::getPAG()->getFlattenedElemIdx(type, idxUb);
                }
            }
            else
                idxLb = idxUb = 0;
        }

        // Add the calculated interval to the result
        res = res + IntervalValue(idxLb, idxUb);
    }

    // Ensure the result is within the bounds of [0, MaxFieldLimit]
    res.meet_with(IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit()));
    if (res.isBottom())
    {
        res = IntervalValue(0);
    }
    return res;
}
// getByteOffset
IntervalValue AbstractState::getByteOffset(const GepStmt* gep)
{
    // If the GEP statement has a constant byte offset, return it directly as the interval value
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantByteOffset());

    IntervalValue res(0); // Initialize the result interval 'res' to 0.

    // Loop through the offsetVarAndGepTypePairVec in reverse order.
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const SVFVar* idxOperandVar = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* idxOperandType = gep->getOffsetVarAndGepTypePairVec()[i].second;

        // Calculate the byte offset for array or pointer types
        if (SVFUtil::isa<SVFArrayType>(idxOperandType) || SVFUtil::isa<SVFPointerType>(idxOperandType))
        {
            u32_t elemByteSize = 1;
            if (const SVFArrayType* arrOperandType = SVFUtil::dyn_cast<SVFArrayType>(idxOperandType))
                elemByteSize = arrOperandType->getTypeOfElement()->getByteSize();
            else if (SVFUtil::isa<SVFPointerType>(idxOperandType))
                elemByteSize = gep->getAccessPath().gepSrcPointeeType()->getByteSize();
            else
                assert(false && "idxOperandType must be ArrType or PtrType");

            if (const ConstIntValVar* op = SVFUtil::dyn_cast<ConstIntValVar>(idxOperandVar))
            {
                // Calculate the lower bound (lb) of the interval value
                s64_t lb = (double)Options::MaxFieldLimit() / elemByteSize >= op->getSExtValue()
                           ? op->getSExtValue() * elemByteSize
                           : Options::MaxFieldLimit();
                res = res + IntervalValue(lb, lb);
            }
            else
            {
                IntervalValue idxVal = (*this)[idxOperandVar->getId()].getInterval();

                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                {
                    // Ensure the bounds are non-negative and within the field limit
                    s64_t ub = (idxVal.ub().getIntNumeral() < 0) ? 0
                               : (double)Options::MaxFieldLimit() / elemByteSize >= idxVal.ub().getIntNumeral()
                               ? elemByteSize * idxVal.ub().getIntNumeral()
                               : Options::MaxFieldLimit();
                    s64_t lb = (idxVal.lb().getIntNumeral() < 0) ? 0
                               : (double)Options::MaxFieldLimit() / elemByteSize >= idxVal.lb().getIntNumeral()
                               ? elemByteSize * idxVal.lb().getIntNumeral()
                               : Options::MaxFieldLimit();
                    res = res + IntervalValue(lb, ub);
                }
            }
        }
        // Process struct subtypes by calculating the byte offset from the beginning to the field of the struct
        else if (const SVFStructType* structOperandType = SVFUtil::dyn_cast<SVFStructType>(idxOperandType))
        {
            res = res + IntervalValue(gep->getAccessPath().getStructFieldOffset(idxOperandVar, structOperandType));
        }
        else
        {
            assert(false && "gep type pair only support arr/ptr/struct");
        }
    }
    return res; // Return the resulting byte offset as an IntervalValue.
}

AbstractValue AbstractState::loadValue(NodeID varId)
{
    AbstractValue res;
    for (auto addr : (*this)[varId].getAddrs())
    {
        res.join_with(load(addr)); // q = *p
    }
    return res;
}
// storeValue
void AbstractState::storeValue(NodeID varId, AbstractValue val)
{
    for (auto addr : (*this)[varId].getAddrs())
    {
        store(addr, val); // *p = q
    }
}

void AbstractState::printAbstractState() const
{
    SVFUtil::outs() << "-----------Var and Value-----------\n";
    u32_t fieldWidth = 20;
    SVFUtil::outs().flags(std::ios::left);
    std::vector<std::pair<u32_t, AbstractValue>> varToAbsValVec(_varToAbsVal.begin(), _varToAbsVal.end());
    std::sort(varToAbsValVec.begin(), varToAbsValVec.end(), [](const auto &a, const auto &b)
    {
        return a.first < b.first;
    });
    for (const auto &item: varToAbsValVec)
    {
        SVFUtil::outs() << std::left << std::setw(fieldWidth) << ("Var" + std::to_string(item.first));
        if (item.second.isInterval())
        {
            SVFUtil::outs() << " Value: " << item.second.getInterval().toString() << "\n";
        }
        else if (item.second.isAddr())
        {
            SVFUtil::outs() << " Value: {";
            u32_t i = 0;
            for (const auto& addr: item.second.getAddrs())
            {
                ++i;
                if (i < item.second.getAddrs().size())
                {
                    SVFUtil::outs() << "0x" << std::hex << addr << ", ";
                }
                else
                {
                    SVFUtil::outs() << "0x" << std::hex << addr;
                }
            }
            SVFUtil::outs() << "}\n";
        }
        else
        {
            SVFUtil::outs() << " Value: ⊥\n";
        }
    }

    std::vector<std::pair<u32_t, AbstractValue>> addrToAbsValVec(_addrToAbsVal.begin(), _addrToAbsVal.end());
    std::sort(addrToAbsValVec.begin(), addrToAbsValVec.end(), [](const auto &a, const auto &b)
    {
        return a.first < b.first;
    });

    for (const auto& item: addrToAbsValVec)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << AbstractState::getVirtualMemAddress(item.first);
        SVFUtil::outs() << std::left << std::setw(fieldWidth) << oss.str();
        if (item.second.isInterval())
        {
            SVFUtil::outs() << " Value: " << item.second.getInterval().toString() << "\n";
        }
        else if (item.second.isAddr())
        {
            SVFUtil::outs() << " Value: {";
            u32_t i = 0;
            for (const auto& addr: item.second.getAddrs())
            {
                ++i;
                if (i < item.second.getAddrs().size())
                {
                    SVFUtil::outs() << "0x" << std::hex << addr << ", ";
                }
                else
                {
                    SVFUtil::outs() << "0x" << std::hex << addr;
                }
            }
            SVFUtil::outs() << "}\n";
        }
        else
        {
            SVFUtil::outs() << " Value: ⊥\n";
        }
    }
    SVFUtil::outs() << "-----------------------------------------\n";
}

const SVFType* AbstractState::getPointeeElement(NodeID id)
{
    SVFIR* svfir = PAG::getPAG();
    if (inVarToAddrsTable(id))
    {
        const AbstractValue& addrs = (*this)[id];
        for (auto addr: addrs.getAddrs())
        {
            NodeID addr_id = getIDFromAddr(addr);
            if (addr_id == 0) // nullptr skip
                continue;
            return svfir->getBaseObject(addr_id)->getType();
        }
    }
    else
    {
        // do nothing if no record in addrs table.
    }
    return nullptr;
}

u32_t AbstractState::getAllocaInstByteSize(const AddrStmt *addr)
{
    if (const ObjVar* objvar = SVFUtil::dyn_cast<ObjVar>(addr->getRHSVar()))
    {
        if (PAG::getPAG()->getBaseObject(objvar->getId())->isConstantByteSize())
        {
            u32_t sz = PAG::getPAG()->getBaseObject(objvar->getId())->getByteSizeOfObj();
            return sz;
        }

        else
        {
            const std::vector<SVFVar*>& sizes = addr->getArrSize();
            // Default element size is set to 1.
            u32_t elementSize = 1;
            u64_t res = elementSize;
            for (const SVFVar* value: sizes)
            {
                if (!inVarToValTable(value->getId()))
                {
                    (*this)[value->getId()] = IntervalValue(Options::MaxFieldLimit());
                }
                IntervalValue itv =
                    (*this)[value->getId()].getInterval();
                res = res * itv.ub().getIntNumeral() > Options::MaxFieldLimit()? Options::MaxFieldLimit(): res * itv.ub().getIntNumeral();
            }
            return (u32_t)res;
        }
    }
    assert (false && "Addr rhs value is not ObjVar");
    abort();
}