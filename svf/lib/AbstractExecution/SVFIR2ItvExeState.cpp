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
    for (const auto &item: es._varToVAddrs)
    {
        _es._varToVAddrs[item.first] = item.second;
    }
    for (const auto &item: es._locToVAddrs)
    {
        _es._locToVAddrs[item.first] = item.second;
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
    for (const auto &_varToVAddr: _es._varToVAddrs)
    {
        IntervalExeState::globalES._varToVAddrs.insert(_varToVAddr);
    }
    for (const auto &_locToVAddr: _es._locToVAddrs)
    {
        IntervalExeState::globalES._locToVAddrs.insert(_locToVAddr);
    }

    _es._varToItvVal.clear();
    IntervalExeState::globalES._varToItvVal.erase(PAG::getPAG()->getBlkPtr());
    _es._varToItvVal[PAG::getPAG()->getBlkPtr()] = IntervalValue::top();
    _es._locToItvVal.clear();
    _es._varToVAddrs.clear();
    _es._locToVAddrs.clear();
}

void SVFIR2ItvExeState::widenVAddrs(IntervalExeState &lhs, const IntervalExeState &rhs)
{
    for (const auto &rhsItem: rhs._varToVAddrs)
    {
        auto lhsIter = lhs._varToVAddrs.find(rhsItem.first);
        if (lhsIter != lhs._varToVAddrs.end())
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
    for (const auto &rhsItem: rhs._locToVAddrs)
    {
        auto lhsIter = lhs._locToVAddrs.find(rhsItem.first);
        if (lhsIter != lhs._locToVAddrs.end())
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

void SVFIR2ItvExeState::narrowVAddrs(IntervalExeState &lhs, const IntervalExeState &rhs)
{
    for (const auto &rhsItem: rhs._varToVAddrs)
    {
        auto lhsIter = lhs._varToVAddrs.find(rhsItem.first);
        if (lhsIter != lhs._varToVAddrs.end())
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
    for (const auto &rhsItem: rhs._locToVAddrs)
    {
        auto lhsIter = lhs._locToVAddrs.find(rhsItem.first);
        if (lhsIter != lhs._locToVAddrs.end())
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

SVFIR2ItvExeState::VAddrs SVFIR2ItvExeState::getGepObjAddress(u32_t pointer, s32_t offset)
{
    assert(!getVAddrs(pointer).empty());
    VAddrs &addrs = getVAddrs(pointer);
    VAddrs ret;
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

std::pair<s32_t, s32_t> SVFIR2ItvExeState::getGepOffset(const GepStmt *gep)
{
    /// for instant constant index, e.g.  gep arr, 1
    if (gep->getOffsetVarAndGepTypePairVec().empty())
        return std::make_pair(gep->getConstantFieldIdx(), gep->getConstantFieldIdx());

    s32_t totalOffsetLb = 0;
    s32_t totalOffsetUb = 0;
    /// default value of MaxFieldLimit is 512
    u32_t maxFieldLimit = Options::MaxFieldLimit() - 1;
    /// for variable index and nested indexes, e.g. 1) gep arr, idx  2) gep arr idx0, idx1
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const SVFValue *value = gep->getOffsetVarAndGepTypePairVec()[i].first->getValue();
        const SVFType *type = gep->getOffsetVarAndGepTypePairVec()[i].second;
        const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(value);
        s32_t offsetLb = 0;
        s32_t offsetUb = 0;
        /// offset is constant but stored in variable
        if (op)
        {
            offsetLb = offsetUb = op->getSExtValue();
        }
        /// offset is variable, the concrete value isn't sure util runtime, and maybe not a concrete value.
        /// e.g.
        else
        {
            u32_t idx = _svfir->getValueNode(value);
            if (!inVarToIValTable(idx)) return std::make_pair(-1, -1);
            IntervalValue &idxVal = _es[idx];
            if (idxVal.isBottom() || idxVal.isTop()) return std::make_pair(0, (s32_t)Options::MaxFieldLimit());
            // if idxVal is a concrete value
            if (idxVal.is_numeral())
            {
                offsetLb = offsetUb = idxVal.lb().getNumeral();
            }
            else
            {
                // if inxVal is an interval. we should make sure that idxVal.lb>0 && idxVal.ub<MaxFieldLimit
                offsetLb = idxVal.lb().getNumeral() < 0 ? 0 : idxVal.lb().getNumeral();
                offsetLb = idxVal.lb().getNumeral() > maxFieldLimit ? maxFieldLimit : offsetLb;
                offsetUb = idxVal.ub().getNumeral() < 0 ? 0 : idxVal.ub().getNumeral();
                offsetUb = idxVal.ub().getNumeral() > maxFieldLimit ? maxFieldLimit : offsetUb;
            }
        }
        if (type == nullptr)
        {
            if ((long long) (totalOffsetLb + offsetLb) > maxFieldLimit)
            {
                totalOffsetLb = maxFieldLimit;
            }
            else
            {
                totalOffsetLb += offsetLb;
            }

            if ((long long) (totalOffsetUb + offsetUb) > maxFieldLimit)
            {
                totalOffsetUb = maxFieldLimit;
            }
            else
            {
                totalOffsetUb += offsetUb;
            }
            continue;
        }

        if (const SVFPointerType *pty = SVFUtil::dyn_cast<SVFPointerType>(type))
        {
            offsetLb = offsetLb * gep->getAccessPath().getElementNum(pty->getPtrElementType());
            offsetUb = offsetUb * gep->getAccessPath().getElementNum(pty->getPtrElementType());

        }
        else
        {
            const std::vector<u32_t> &so = SymbolTableInfo::SymbolInfo()->getTypeInfo(type)->getFlattenedElemIdxVec();
            if (so.empty() || (u32_t) offsetUb >= so.size() || (u32_t) offsetLb >= so.size())
                return std::make_pair(-1, -1);
            offsetLb = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(type, offsetLb);
            offsetUb = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(type, offsetUb);
        }
        if ((long long) (totalOffsetLb + offsetLb) > maxFieldLimit)
        {
            totalOffsetLb = maxFieldLimit;
        }
        else
        {
            totalOffsetLb += offsetLb;
        }
        if ((long long) (totalOffsetUb + offsetUb) > maxFieldLimit)
        {
            totalOffsetUb = maxFieldLimit;
        }
        else
        {
            totalOffsetUb += offsetUb;
        }
    }
    std::pair<s32_t, s32_t> offSetPair;
    offSetPair.first = totalOffsetLb;
    offSetPair.second = totalOffsetUb;
    return offSetPair;
}

/*!
 * Init Z3Expr for ValVar
 * @param valVar
 * @param valExprIdToCondValPairMap
 */
void SVFIR2ItvExeState::initValVar(const ValVar *valVar, u32_t varId)
{

    SVFIR *svfir = PAG::getPAG();

    if (const SVFType *type = valVar->getType())
    {
        // TODO:miss floatpointerty, voidty, labelty, matadataty
        if (type->getKind() == SVFType::SVFIntegerTy ||
                type->getKind() == SVFType::SVFPointerTy ||
                type->getKind() == SVFType::SVFFunctionTy ||
                type->getKind() == SVFType::SVFStructTy ||
                type->getKind() == SVFType::SVFArrayTy)
            // continue with null expression
            _es[varId] = IntervalValue::top();
        else
        {

            SVFUtil::errs() << valVar->getValue()->toString() << "\n" << " type: " << *type << "\n";
            assert(false && "what other types we have");
        }
    }
    else
    {
        if (svfir->getNullPtr() == valVar->getId())
            _es[varId] = IntervalValue(0, 0);
        else
            _es[varId] = IntervalValue::top();
        assert(SVFUtil::isa<DummyValVar>(valVar) && "not a DummValVar if it has no type?");
    }
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
            {
                IntervalExeState::globalES[varId] = IntervalValue(consFP->getFPValue(), consFP->getFPValue());
            }
            else if (SVFUtil::isa<SVFConstantNullPtr>(obj->getValue()))
            {
                IntervalExeState::globalES[varId] = IntervalValue(0, 0);
            }
            else if (SVFUtil::isa<SVFGlobalValue>(obj->getValue()))
            {
                IntervalExeState::globalES.getVAddrs(varId).insert(getVirtualMemAddress(varId));
            }
            else if (obj->isConstantArray() || obj->isConstantStruct())
            {
                IntervalExeState::globalES[varId] = IntervalValue::top();
            }
            else
            {
                IntervalExeState::globalES[varId] = IntervalValue::top();
            }
        }
        else
        {
            IntervalExeState::globalES.getVAddrs(varId).insert(getVirtualMemAddress(varId));
        }
    }
    else
    {
        IntervalExeState::globalES.getVAddrs(varId).insert(getVirtualMemAddress(varId));
    }
}

void SVFIR2ItvExeState::initSVFVar(u32_t varId)
{
    if (inVarToIValTable(varId) || _es.inVarToAddrsTable(varId)) return;
    SVFIR *svfir = PAG::getPAG();
    SVFVar *svfVar = svfir->getGNode(varId);
    // write objvar into cache instead of exestate
    if (const ObjVar *objVar = dyn_cast<ObjVar>(svfVar))
    {
        initObjVar(objVar, varId);
        return;
    }
    else if (const ValVar *valVar = dyn_cast<ValVar>(svfVar))
    {
        initValVar(valVar, varId);
        return;
    }
    else
    {
        initValVar(valVar, varId);
        return;
    }
}


void SVFIR2ItvExeState::translateAddr(const AddrStmt *addr)
{
    initSVFVar(addr->getRHSVarID());
    if (inVarToIValTable(addr->getRHSVarID()))
    {
        IntervalExeState::globalES[addr->getLHSVarID()] = IntervalExeState::globalES[addr->getRHSVarID()];
    }
    else if (inVarToAddrsTable(addr->getRHSVarID()))
    {
        IntervalExeState::globalES.getVAddrs(addr->getLHSVarID()) = IntervalExeState::globalES.getVAddrs(
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
    if (!inVarToIValTable(op0)) _es[op0] = IntervalValue::top();
    if (!inVarToIValTable(op1)) _es[op1] = IntervalValue::top();
    if (inVarToIValTable(op0) && inVarToIValTable(op1))
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
    if (inVarToIValTable(op0) && inVarToIValTable(op1))
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
        VAddrs &lhs = getVAddrs(op0), &rhs = getVAddrs(op1);
//        if (lhs.empty() || rhs.empty()) {
//            outs() << "empty address cmp?\n";
//            return;
//        }
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
        VAddrs &addrs = getVAddrs(rhs);
        assert(!addrs.empty());
        for (const auto &addr: addrs)
        {
            u32_t objId = getInternalID(addr);
            if (inLocToIValTable(objId))
                _es[lhs] = IntervalValue::bottom();
            else if (inLocToAddrsTable(objId))
                _es.getVAddrs(lhs).setBottom();
            break;
        }
        for (const auto &addr: addrs)
        {
            u32_t objId = getInternalID(addr);
            if (inLocToIValTable(objId))
            {
                if (!inVarToIValTable(lhs))
                {
                    _es[lhs] = _es.load(addr);
                }
                else
                {
                    _es[lhs].join_with(_es.load(addr));
                }
            }
            else if (inLocToAddrsTable(objId))
            {
                if (!inVarToAddrsTable(lhs))
                {
                    _es.getVAddrs(lhs) = _es.loadVAddrs(addr);
                }
                else
                {
                    _es.getVAddrs(lhs).join_with(_es.loadVAddrs(addr));
                }
            }
        }
    }
}

void SVFIR2ItvExeState::translateStore(const StoreStmt *store)
{
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();
    if (inVarToAddrsTable(lhs))
    {
        if (inVarToIValTable(rhs))
        {
            assert(!getVAddrs(lhs).empty());
            VAddrs &addrs = getVAddrs(lhs);
            for (const auto &addr: addrs)
            {
                _es.store(addr, _es[rhs]);
            }
        }
        else if (inVarToAddrsTable(rhs))
        {
            assert(!getVAddrs(lhs).empty());
            VAddrs &addrs = getVAddrs(lhs);
            for (const auto &addr: addrs)
            {
                assert(!getVAddrs(rhs).empty());
                _es.storeVAddrs(addr, getVAddrs(rhs));
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
        if (inVarToIValTable(rhs))
        {
            _es[lhs] = _es[rhs];
        }
        else if (inVarToAddrsTable(rhs))
        {
            assert(!getVAddrs(rhs).empty());
            _es.getVAddrs(lhs) = getVAddrs(rhs);
        }
    }
}

void SVFIR2ItvExeState::translateGep(const GepStmt *gep)
{
    u32_t rhs = gep->getRHSVarID();
    u32_t lhs = gep->getLHSVarID();
    if (!inVarToAddrsTable(rhs)) return;
    assert(!getVAddrs(rhs).empty());
    VAddrs &rhsVal = getVAddrs(rhs);
    if (rhsVal.empty()) return;
    std::pair<s32_t, s32_t> offsetPair = getGepOffset(gep);
    if (offsetPair.first == -1 && offsetPair.second == -1) return;
    if (!isVirtualMemAddress(*rhsVal.begin()))
    {
        return;
    }
    else
    {
        VAddrs gepAddrs;
        s32_t ub = offsetPair.second;
        if (offsetPair.second > (s32_t) Options::MaxFieldLimit() - 1)
        {
            ub = Options::MaxFieldLimit() - 1;
        }
        for (s32_t i = offsetPair.first; i <= ub; i++)
        {
            gepAddrs.join_with(getGepObjAddress(rhs, i));
        }
        if(gepAddrs.empty()) return;
        _es.getVAddrs(lhs) = gepAddrs;
        return;
    }
}

void SVFIR2ItvExeState::translateSelect(const SelectStmt *select)
{
    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();
    if (inVarToIValTable(tval) && inVarToIValTable(fval) && inVarToIValTable(cond))
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
    else if (inVarToAddrsTable(tval) && inVarToAddrsTable(fval) && inVarToIValTable(cond))
    {
        if (_es[cond].is_numeral())
        {
            assert(!getVAddrs(fval).empty());
            assert(!getVAddrs(tval).empty());
            _es.getVAddrs(res) = _es[cond].is_zero() ? getVAddrs(fval) : getVAddrs(tval);
        }
    }
}

void SVFIR2ItvExeState::translatePhi(const PhiStmt *phi)
{
    u32_t res = phi->getResID();
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        NodeID curId = phi->getOpVarID(i);
        if (inVarToIValTable(curId))
        {
            const IntervalValue &cur = _es[curId];
            if (!inVarToIValTable(res))
            {
                _es[res] = cur;
            }
            else
            {
                _es[res].join_with(cur);
            }
        }
        else if (inVarToAddrsTable(curId))
        {
            assert(!getVAddrs(curId).empty());
            const VAddrs &cur = getVAddrs(curId);
            if (!inVarToAddrsTable(res))
            {
                _es.getVAddrs(res) = cur;
            }
            else
            {
                _es.getVAddrs(res).join_with(cur);
            }
        }
    }
}


void SVFIR2ItvExeState::translateCall(const CallPE *callPE)
{
    NodeID lhs = callPE->getLHSVarID();
    NodeID rhs = callPE->getRHSVarID();
    if (inVarToIValTable(rhs))
    {
        _es[lhs] = _es[rhs];
    }
    else if (inVarToAddrsTable(rhs))
    {
        assert(!getVAddrs(rhs).empty());
        _es.getVAddrs(lhs) = getVAddrs(rhs);
    }
}

void SVFIR2ItvExeState::translateRet(const RetPE *retPE)
{
    NodeID lhs = retPE->getLHSVarID();
    NodeID rhs = retPE->getRHSVarID();
    if (inVarToIValTable(rhs))
    {
        _es[lhs] = _es[rhs];
    }
    else if (inVarToAddrsTable(rhs))
    {
        assert(!getVAddrs(rhs).empty());
        _es.getVAddrs(lhs) = getVAddrs(rhs);
    }
}
