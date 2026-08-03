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
// State-access bodies factored out of AbstractInterpretation.cpp /
// SparseAbstractInterpretation.cpp.  Class declarations stay in their
// respective headers; this file just hosts the implementations of the
// methods that used to live on the (now-folded) AbstractStateManager —
// trace lookup, value get/has/update, GEP / load / store helpers, and
// def/use queries — for dense and semi-sparse.  Full-sparse stubs and
// SVFG-backed overrides live in SparseAbstractInterpretation.cpp.
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "AE/Svfexe/SparseAbstractInterpretation.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include <cstdlib>
#include <cstring>
#include <limits>

using namespace SVF;

// =====================================================================
//  Dense (AbstractInterpretation) — direct trace lookup; sparse
//  subclasses override the virtuals below.
// =====================================================================

AbstractState& AbstractInterpretation::getAbsState(const ICFGNode* node)
{
    if (abstractTrace.count(node) == 0)
    {
        static const bool allowMissing = []()
        {
            const char* env = std::getenv("AE_ALLOW_MISSING_STATE");
            if (env == nullptr || *env == '\0')
                return false;
            return std::strcmp(env, "0") != 0 &&
                   std::strcmp(env, "false") != 0 &&
                   std::strcmp(env, "off") != 0 &&
                   std::strcmp(env, "no") != 0;
        }();
        if (allowMissing)
        {
            abstractTrace[node] = AbstractState();
            return abstractTrace[node];
        }
        assert(false && "No preAbsTrace for this node");
        abort();
    }
    return abstractTrace[node];
}

void AbstractInterpretation::updateAbsState(const ICFGNode* node, const AbstractState& state)
{
    abstractTrace[node] = state;
}

void AbstractInterpretation::joinStates(AbstractState& dst, const AbstractState& src)
{
    dst.joinWith(src);
}

bool AbstractInterpretation::hasAbsState(const ICFGNode* node)
{
    return abstractTrace.count(node) != 0;
}

/// Dense base: direct trace lookup, with a top sentinel for genuinely
/// missing entries (e.g. function parameters like argc, never written
/// before first read).  Sparse subclasses override with a def-site
/// resolution chain.
///
/// The "in map" check is a raw map.count — NOT inVarToValTable /
/// inVarToAddrsTable, which gate on isInterval / isAddr.  SVF
/// canonically represents uninit and null-pointer shapes as
/// (interval=bottom ∧ addrs=∅); those predicates would falsely report
/// such an entry as "not present", and the top fallback below would
/// then clobber the very signal NullptrDerefDetector::isUninit keys off.
const AbstractValue& AbstractInterpretation::getAbsValue(const ValVar* var, const ICFGNode* node)
{
    u32_t id = var->getId();
    AbstractState& as = abstractTrace[node];
    if (as.getVarToVal().count(id))
        return as[id];
    as[id] = IntervalValue::top();
    return as[id];
}

const AbstractValue& AbstractInterpretation::getAbsValue(const ObjVar* var, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    return as.load(addr);
}

const AbstractValue& AbstractInterpretation::getAbsValue(const SVFVar* var, const ICFGNode* node)
{
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        return getAbsValue(objVar, node);
    if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        return getAbsValue(valVar, node);
    assert(false && "Unknown SVFVar kind");
    abort();
}

/// Dense base: direct existence check at `node`.  Mirrors the simplified
/// getAbsValue lookup — uses raw map.contains rather than
/// inVar*Table predicates, which would falsely report neutral
/// (interval=bottom ∧ addrs=∅) entries as "not present".
bool AbstractInterpretation::hasAbsValue(const ValVar* var, const ICFGNode* node) const
{
    auto it = abstractTrace.find(node);
    if (it == abstractTrace.end())
        return false;
    return it->second.getVarToVal().count(var->getId()) != 0;
}

bool AbstractInterpretation::hasAbsValue(const ObjVar* var, const ICFGNode* node) const
{
    auto it = abstractTrace.find(node);
    if (it == abstractTrace.end())
        return false;
    return it->second.getLocToVal().count(var->getId()) != 0;
}

bool AbstractInterpretation::hasAbsValue(const SVFVar* var, const ICFGNode* node) const
{
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        return hasAbsValue(objVar, node);
    if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        return hasAbsValue(valVar, node);
    return false;
}

void AbstractInterpretation::updateAbsValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node)
{
    abstractTrace[node][var->getId()] = val;
}

void AbstractInterpretation::updateAbsValue(const ObjVar* var, const AbstractValue& val, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    as.store(addr, val);
}

void AbstractInterpretation::updateAbsValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node)
{
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        updateAbsValue(objVar, val, node);
    else if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        updateAbsValue(valVar, val, node);
    else
        assert(false && "Unknown SVFVar kind");
}

void AbstractInterpretation::getAbsState(const Set<const ValVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    for (const ValVar* var : vars)
    {
        u32_t id = var->getId();
        result[id] = as[id];
    }
}

void AbstractInterpretation::getAbsState(const Set<const ObjVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    for (const ObjVar* var : vars)
    {
        u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
        result.store(addr, as.load(addr));
    }
}

void AbstractInterpretation::getAbsState(const Set<const SVFVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    for (const SVFVar* var : vars)
    {
        if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        {
            u32_t id = valVar->getId();
            result[id] = as[id];
        }
        else if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        {
            u32_t addr = AbstractState::getVirtualMemAddress(objVar->getId());
            result.store(addr, as.load(addr));
        }
    }
}

IntervalValue AbstractInterpretation::getGepElementIndex(const GepStmt* gep)
{
    const ICFGNode* node = gep->getICFGNode();
    // Byte-addressed struct GEPs carry both a physical byte offset (for bound
    // checks) and a layout-derived field index (for abstract memory state).
    // Recomputing the latter from the raw i8 index can collapse distinct
    // fields at MaxFieldLimit, e.g. byte offsets 80 and 368 both becoming 32.
    if (gep->getAccessPath().hasResolvedFieldIndex())
        return IntervalValue(
                   (s64_t)gep->getAccessPath().getConstantStructFldIdx());
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantOffset());

    IntervalValue res(0);
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const ValVar* var = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* type = gep->getOffsetVarAndGepTypePairVec()[i].second;

        s64_t idxLb, idxUb;
        if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
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
            u32_t elemNum = gep->getAccessPath().getElementNum(gep->getAccessPath().gepSrcPointeeType());
            idxLb = (double)Options::MaxFieldLimit() / elemNum < idxLb ? Options::MaxFieldLimit() : idxLb * elemNum;
            idxUb = (double)Options::MaxFieldLimit() / elemNum < idxUb ? Options::MaxFieldLimit() : idxUb * elemNum;
        }
        else
        {
            if (Options::ModelArrays())
            {
                const std::vector<u32_t>& so = PAG::getPAG()->getTypeInfo(type)->getFlattenedElemIdxVec();
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
        const ValVar* idxOperandVar = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* idxOperandType = gep->getOffsetVarAndGepTypePairVec()[i].second;

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
                res = res +
                      IntervalValue(op->getSExtValue()) * IntervalValue(elemByteSize);
            }
            else
            {
                IntervalValue idxVal = getAbsValue(idxOperandVar, node).getInterval();
                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                    res = res + idxVal * IntervalValue(elemByteSize);
            }
        }
        else if (const SVFStructType* structOperandType = SVFUtil::dyn_cast<SVFStructType>(idxOperandType))
        {
            res = res + IntervalValue(gep->getAccessPath().getStructFieldOffset(idxOperandVar, structOperandType));
        }
        else
        {
            assert(false && "gep type pair only support arr/ptr/struct");
        }
    }
    return res;
}

AddressValue AbstractInterpretation::getGepObjAddrs(const ValVar* pointer, IntervalValue offset)
{
    const ICFGNode* node = pointer->getICFGNode();
    AddressValue gepAddrs;
    AbstractState& as = getAbsState(node);
    APOffset lb = offset.lb().getIntNumeral() < Options::MaxFieldLimit() ? offset.lb().getIntNumeral()
                  : Options::MaxFieldLimit();
    APOffset ub = offset.ub().getIntNumeral() < Options::MaxFieldLimit() ? offset.ub().getIntNumeral()
                  : Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
    {
        const AbstractValue& addrs = getAbsValue(pointer, node);
        for (const auto& addr : addrs.getAddrs())
        {
            s64_t baseObj = as.getIDFromAddr(addr);
            assert(SVFUtil::isa<ObjVar>(svfir->getSVFVar(baseObj)) && "Fail to get the base object address!");
            NodeID gepObj = svfir->getGepObjVar(baseObj, i);
            as[gepObj] = AddressValue(AbstractState::getVirtualMemAddress(gepObj));
            gepAddrs.insert(AbstractState::getVirtualMemAddress(gepObj));
        }
    }
    return gepAddrs;
}

AbstractValue AbstractInterpretation::loadValue(const ValVar* pointer, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    AbstractState& as = getAbsState(node);
    AbstractValue res;
    for (auto addr : ptrVal.getAddrs())
    {
        res.join_with(
            getAbsValue(svfir->getSVFVar(as.getIDFromAddr(addr)), node));
    }
    return res;
}

void AbstractInterpretation::storeValue(const ValVar* pointer, const AbstractValue& val, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(pointer, node);
    AbstractState& as = getAbsState(node);
    for (auto addr : ptrVal.getAddrs())
        updateAbsValue(svfir->getSVFVar(as.getIDFromAddr(addr)), val, node);
}

AbstractValue AbstractInterpretation::loadAddressValue(u32_t addr,
        const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    return getAbsValue(svfir->getSVFVar(as.getIDFromAddr(addr)), node);
}

void AbstractInterpretation::storeAddressValue(u32_t addr,
        const AbstractValue& val,
        const ICFGNode* node)
{
    AbstractState& as = getAbsState(node);
    updateAbsValue(svfir->getSVFVar(as.getIDFromAddr(addr)), val, node);
}

const SVFType* AbstractInterpretation::getPointeeElement(const ObjVar* var, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbsValue(var, node);
    if (!ptrVal.isAddr())
        return nullptr;
    for (auto addr : ptrVal.getAddrs())
    {
        NodeID objId = getAbsState(node).getIDFromAddr(addr);
        if (objId == 0)
            continue;
        return svfir->getBaseObject(objId)->getType();
    }
    return nullptr;
}

IntervalValue AbstractInterpretation::getAllocaInstByteSizeInterval(const AddrStmt* addr)
{
    const ICFGNode* node = addr->getICFGNode();
    if (const ObjVar* objvar = SVFUtil::dyn_cast<ObjVar>(addr->getRHSVar()))
    {
        const BaseObjVar* baseObj = svfir->getBaseObject(objvar->getId());
        if (baseObj->isConstantByteSize())
        {
            return IntervalValue(baseObj->getByteSizeOfObj());
        }
        else
        {
            const std::vector<SVFVar*>& sizes = addr->getArrSize();
            if (sizes.empty())
                return IntervalValue::bottom();

            // Heap allocation arguments are already byte counts. A dynamic
            // alloca argument is an element count and needs the element size.
            IntervalValue res((s64_t)(baseObj->isStack()
                                      ? std::max(1U, baseObj->getType()->getByteSize())
                                      : 1U));
            for (const SVFVar* value : sizes)
            {
                const AbstractValue& sizeVal = getAbsValue(value, node);
                IntervalValue count = sizeVal.getInterval();
                if (count.isBottom())
                {
                    // This is a reachable allocation with an unknown dynamic
                    // count.  Treat it as an unbounded non-negative size; reusing
                    // a previous concrete invocation would be unsound.
                    res = IntervalValue(
                              (s64_t)0, IntervalValue::plus_infinity());
                    break;
                }
                count.meet_with(IntervalValue(
                                    (s64_t)0, IntervalValue::plus_infinity()));
                if (count.isBottom())
                    return IntervalValue::bottom();
                res = res * count;
            }

            auto summaryIt = allocationByteSizeSummary.find(addr);
            if (summaryIt == allocationByteSizeSummary.end())
                summaryIt = allocationByteSizeSummary.emplace(addr, res).first;
            else
                summaryIt->second.join_with(res);
            return summaryIt->second;
        }
    }
    assert(false && "Addr rhs value is not ObjVar");
    abort();
}

u32_t AbstractInterpretation::getAllocaInstByteSize(const AddrStmt* addr)
{
    const IntervalValue size = getAllocaInstByteSizeInterval(addr);
    if (size.isBottom() || size.ub().is_plus_infinity())
        return 0;

    const s64_t upper = size.ub().getIntNumeral();
    if (upper <= 0 || (u64_t)upper > std::numeric_limits<u32_t>::max())
        return 0;
    return (u32_t)upper;
}
