//===- AbstractStateManager.cpp -- AE state-access implementations ---//
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
//
// Representation-independent GEP and type helpers shared by Box-backed
// dense, semi-sparse, and full-sparse abstract execution.
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "SVFIR/SVFIR.h"

using namespace SVF;

const AbstractDomain::AbstractState* AbstractInterpretation::
    getScalarAbstractState(const FunObjVar*) const
{
    return nullptr;
}

const AbstractDomain::AbstractState* AbstractInterpretation::
    getScalarAbstractState(const ValVar*) const
{
    return nullptr;
}

void AbstractInterpretation::finalizeAbstractState(const ICFGNode*) {}

IntervalValue AbstractInterpretation::getGepElementIndex(const GepStmt* gep)
{
    const ICFGNode* node = gep->getICFGNode();
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantOffset());

    IntervalValue res(0);
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const ValVar* var = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* type = gep->getOffsetVarAndGepTypePairVec()[i].second;

        s64_t idxLb, idxUb;
        if (const ConstIntValVar* constInt =
                SVFUtil::dyn_cast<ConstIntValVar>(var))
            idxLb = idxUb = constInt->getSExtValue();
        else
        {
            IntervalValue idxItv = getAbsValue(var, node).getInterval();
            if (idxItv.isBottom())
                idxLb = idxUb = 0;
            else
            {
                idxLb = idxItv.lb().getIntNumeral();
                idxUb = idxItv.ub().getIntNumeral();
            }
        }

        if (SVFUtil::isa<SVFPointerType>(type))
        {
            u32_t elemNum = gep->getAccessPath().getElementNum(
                gep->getAccessPath().gepSrcPointeeType());
            idxLb = (double)Options::MaxFieldLimit() / elemNum < idxLb
                        ? Options::MaxFieldLimit()
                        : idxLb * elemNum;
            idxUb = (double)Options::MaxFieldLimit() / elemNum < idxUb
                        ? Options::MaxFieldLimit()
                        : idxUb * elemNum;
        }
        else
        {
            if (Options::ModelArrays())
            {
                const std::vector<u32_t>& so =
                    PAG::getPAG()->getTypeInfo(type)->getFlattenedElemIdxVec();
                if (so.empty() || idxUb >= (APOffset)so.size() || idxLb < 0)
                    idxLb = idxUb = 0;
                else
                {
                    idxLb = PAG::getPAG()->getFlattenedElemIdx(type, idxLb);
                    idxUb = PAG::getPAG()->getFlattenedElemIdx(type, idxUb);
                }
            }
            else
                idxLb = idxUb = 0;
        }
        res = res + IntervalValue(idxLb, idxUb);
    }
    res.meet_with(IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit()));
    if (res.isBottom())
        res = IntervalValue(0);
    return res;
}

IntervalValue AbstractInterpretation::getGepByteOffset(const GepStmt* gep)
{
    const ICFGNode* node = gep->getICFGNode();
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantByteOffset());

    IntervalValue res(0);
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const ValVar* idxOperandVar =
            gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* idxOperandType =
            gep->getOffsetVarAndGepTypePairVec()[i].second;

        if (SVFUtil::isa<SVFArrayType>(idxOperandType) ||
            SVFUtil::isa<SVFPointerType>(idxOperandType))
        {
            u32_t elemByteSize = 1;
            if (const SVFArrayType* arrOperandType =
                    SVFUtil::dyn_cast<SVFArrayType>(idxOperandType))
                elemByteSize =
                    arrOperandType->getTypeOfElement()->getByteSize();
            else if (SVFUtil::isa<SVFPointerType>(idxOperandType))
                elemByteSize =
                    gep->getAccessPath().gepSrcPointeeType()->getByteSize();
            else
                assert(false && "idxOperandType must be ArrType or PtrType");

            if (const ConstIntValVar* op =
                    SVFUtil::dyn_cast<ConstIntValVar>(idxOperandVar))
            {
                s64_t lb = (double)Options::MaxFieldLimit() / elemByteSize >=
                                   op->getSExtValue()
                               ? op->getSExtValue() * elemByteSize
                               : Options::MaxFieldLimit();
                res = res + IntervalValue(lb, lb);
            }
            else
            {
                IntervalValue idxVal =
                    getAbsValue(idxOperandVar, node).getInterval();
                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                {
                    s64_t ub =
                        (idxVal.ub().getIntNumeral() < 0) ? 0
                        : (double)Options::MaxFieldLimit() / elemByteSize >=
                                idxVal.ub().getIntNumeral()
                            ? elemByteSize * idxVal.ub().getIntNumeral()
                            : Options::MaxFieldLimit();
                    s64_t lb =
                        (idxVal.lb().getIntNumeral() < 0) ? 0
                        : (double)Options::MaxFieldLimit() / elemByteSize >=
                                idxVal.lb().getIntNumeral()
                            ? elemByteSize * idxVal.lb().getIntNumeral()
                            : Options::MaxFieldLimit();
                    res = res + IntervalValue(lb, ub);
                }
            }
        }
        else if (const SVFStructType* structOperandType =
                     SVFUtil::dyn_cast<SVFStructType>(idxOperandType))
        {
            res = res + IntervalValue(gep->getAccessPath().getStructFieldOffset(
                            idxOperandVar, structOperandType));
        }
        else
        {
            assert(false && "gep type pair only support arr/ptr/struct");
        }
    }
    return res;
}

AddressValue AbstractInterpretation::getGepObjAddrs(const ValVar* pointer,
                                                    IntervalValue offset)
{
    const ICFGNode* node = pointer->getICFGNode();
    AddressValue gepAddrs;
    APOffset lb = offset.lb().getIntNumeral() < Options::MaxFieldLimit()
                      ? offset.lb().getIntNumeral()
                      : Options::MaxFieldLimit();
    APOffset ub = offset.ub().getIntNumeral() < Options::MaxFieldLimit()
                      ? offset.ub().getIntNumeral()
                      : Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
    {
        const AbstractValue& addrs = getAbsValue(pointer, node);
        for (const auto& addr : addrs.getAddrs())
        {
            s64_t baseObj = objectIdFromAddress(addr);
            assert(SVFUtil::isa<ObjVar>(svfir->getSVFVar(baseObj)) &&
                   "Fail to get the base object address!");
            NodeID gepObj = svfir->getGepObjVar(baseObj, i);
            gepAddrs.insert(AddressValue::getVirtualMemAddress(gepObj));
        }
    }
    return gepAddrs;
}

AbstractValue AbstractInterpretation::loadValue(const ValVar* pointer,
                                                const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    AbstractValue res;
    for (auto addr : ptrVal.getAddrs())
    {
        res.join_with(getMemoryValue(addr, node));
    }
    return res;
}

void AbstractInterpretation::storeValue(const ValVar* pointer,
                                        const AbstractValue& val,
                                        const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    for (auto addr : ptrVal.getAddrs())
        updateMemoryValue(addr, val, node);
}

const SVFType* AbstractInterpretation::getPointeeElement(const ObjVar* var,
                                                         const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(var, node);
    if (!ptrVal.isAddr())
        return nullptr;
    for (auto addr : ptrVal.getAddrs())
    {
        NodeID objId = objectIdFromAddress(addr);
        if (objId == 0)
            continue;
        return svfir->getBaseObject(objId)->getType();
    }
    return nullptr;
}

u32_t AbstractInterpretation::getAllocaInstByteSize(const AddrStmt* addr)
{
    const ICFGNode* node = addr->getICFGNode();
    if (const ObjVar* objvar = SVFUtil::dyn_cast<ObjVar>(addr->getRHSVar()))
    {
        if (svfir->getBaseObject(objvar->getId())->isConstantByteSize())
        {
            return svfir->getBaseObject(objvar->getId())->getByteSizeOfObj();
        }
        else
        {
            const std::vector<SVFVar*>& sizes = addr->getArrSize();
            u32_t elementSize = 1;
            u64_t res = elementSize;
            for (const SVFVar* value : sizes)
            {
                const AbstractValue& sizeVal = getAbsValue(value, node);
                IntervalValue itv = sizeVal.getInterval();
                if (itv.isBottom())
                    itv = IntervalValue(Options::MaxFieldLimit());
                res = res * itv.ub().getIntNumeral() > Options::MaxFieldLimit()
                          ? Options::MaxFieldLimit()
                          : res * itv.ub().getIntNumeral();
            }
            return (u32_t)res;
        }
    }
    assert(false && "Addr rhs value is not ObjVar");
    abort();
}
