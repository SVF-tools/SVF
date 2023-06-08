//===- SVFIR2ConsExeState.cpp ----SVFIR2ConsExeState-------------------------//
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

//
// Created by jiawei and xiao on 6/1/23.
//


#include "AbstractExecution/SVFIR2ConsExeState.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;
using namespace std;

SVFIR2ConsExeState::~SVFIR2ConsExeState()
{
    ConsExeState::globalConsES._varToVal.clear();
    ConsExeState::globalConsES._varToVAddrs.clear();
    ConsExeState::globalConsES._locToVal.clear();
    ConsExeState::globalConsES._locToVAddrs.clear();
}
/// Translator for llvm ir
//{%
/*!
 * https://llvm.org/docs/LangRef.html#alloca-instruction
 * @param addr
 * @return
 */
void SVFIR2ConsExeState::translateAddr(const AddrStmt *addr)
{
    initSVFVar(addr->getRHSVarID());
    if (inVarToValTable(addr->getRHSVarID()))
    {
        _es->globalConsES._varToVal[addr->getLHSVarID()] = _es->globalConsES._varToVal[addr->getRHSVarID()];
    }
    else if (inVarToAddrsTable(addr->getRHSVarID()))
    {
        _es->globalConsES._varToVAddrs[addr->getLHSVarID()] = _es->globalConsES._varToVAddrs[addr->getRHSVarID()];
    }
    else
    {
        assert(false && "not number or virtual addrs?");
    }
}

/*!
 * https://llvm.org/docs/LangRef.html#binary-operations
 * @param binary
 * @return
 */
void SVFIR2ConsExeState::translateBinary(const BinaryOPStmt *binary)
{
    u32_t op0 = binary->getOpVarID(0);
    u32_t op1 = binary->getOpVarID(1);
    u32_t res = binary->getResID();
    // rhs is not initialized
    if (!_es->inVarToVal(op0) || !_es->inVarToVal(op1))
        return;

    SingleAbsValue &lhs = (*_es)[op0], &rhs = (*_es)[op1];
    SingleAbsValue resVal;
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    case BinaryOPStmt::FAdd:
        resVal = (lhs + rhs).simplify();
        break;
    case BinaryOPStmt::Sub:
    case BinaryOPStmt::FSub:
        resVal = (lhs - rhs).simplify();
        break;
    case BinaryOPStmt::Mul:
    case BinaryOPStmt::FMul:
        resVal = (lhs * rhs).simplify();
        break;
    case BinaryOPStmt::SDiv:
    case BinaryOPStmt::FDiv:
    case BinaryOPStmt::UDiv:
        resVal = (lhs / rhs).simplify();
        break;
    case BinaryOPStmt::SRem:
    case BinaryOPStmt::FRem:
    case BinaryOPStmt::URem:
        resVal = (lhs % rhs).simplify();
        break;
    case BinaryOPStmt::Xor:
        resVal = (lhs ^ rhs).simplify();
        break;
    case BinaryOPStmt::And:
        resVal = (lhs & rhs).simplify();
        break;
    case BinaryOPStmt::Or:
        resVal = (lhs | rhs).simplify();
        break;
    case BinaryOPStmt::AShr:
        resVal = ashr(lhs, rhs).simplify();
        break;
    case BinaryOPStmt::Shl:
        resVal = shl(lhs, rhs).simplify();
        break;
    case BinaryOPStmt::LShr:
        resVal = lshr(lhs, rhs).simplify();
        break;
    default:
    {
        assert(false && "undefined binary");
    }
    }
    (*_es)[res] = resVal;
}

/*!
 * https://llvm.org/docs/LangRef.html#icmp-instruction
 * @param cmp
 * @return
 */
void SVFIR2ConsExeState::translateCmp(const CmpStmt *cmp)
{
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    u32_t res = cmp->getResID();
    if (inVarToValTable(op0) && inVarToValTable(op1))
    {
        SingleAbsValue &lhs = (*_es)[op0], &rhs = (*_es)[op1];
        SingleAbsValue resVal;
        auto predicate = cmp->getPredicate();
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
            resVal = ite(lhs == rhs, 1, 0).simplify();
            break;
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
            resVal = ite(lhs != rhs, 1, 0).simplify();
            break;
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
            resVal = ite(lhs > rhs, 1, 0).simplify();
            break;
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
            resVal = ite(lhs >= rhs, 1, 0).simplify();
            break;
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
            resVal = ite(lhs < rhs, 1, 0).simplify();
            break;
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
            resVal = ite(lhs <= rhs, 1, 0).simplify();
            break;
        case CmpStmt::FCMP_FALSE:
            resVal = 0;
            break;
        case CmpStmt::FCMP_TRUE:
            resVal = 1;
            break;
        default:
        {
            assert(false && "undefined compare");
        }
        }
        (*_es)[res] = resVal;
    }
    else if (inVarToAddrsTable(op0) && inVarToAddrsTable(op1))
    {

        VAddrs &lhs = _es->getVAddrs(op0), &rhs = _es->getVAddrs(op1);
        if (lhs.size() == 1)
        {
            (*_es)[res] = SingleAbsValue::topConstant();
            return;
        }
        if (rhs.size() == 1)
        {
            (*_es)[res] = SingleAbsValue::topConstant();
            return;
        }
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
                (*_es)[res] = lhs == rhs;
            }
            else
            {
                if (lhs.hasIntersect(rhs))
                {
                    (*_es)[res] = SingleAbsValue::topConstant();
                }
                else
                {
                    (*_es)[res] = 0;
                }
            }
            return;
        }
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                (*_es)[res] = lhs != rhs;
            }
            else
            {
                if (lhs.hasIntersect(rhs))
                {
                    (*_es)[res] = SingleAbsValue::topConstant();
                }
                else
                {
                    (*_es)[res] = 1;
                }
            }
            return;
        }
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                (*_es)[res] = *lhs.begin() > *rhs.begin();
            }
            else
            {
                (*_es)[res] = SingleAbsValue::topConstant();
            }
            return;
        }
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                (*_es)[res] = *lhs.begin() >= *rhs.begin();
            }
            else
            {
                (*_es)[res] = SingleAbsValue::topConstant();
            }
            return;
        }
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                (*_es)[res] = *lhs.begin() < *rhs.begin();
            }
            else
            {
                (*_es)[res] = SingleAbsValue::topConstant();
            }
            return;
        }
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
        {
            if (lhs.size() == 1 && rhs.size() == 1)
            {
                (*_es)[res] = *lhs.begin() <= *rhs.begin();
            }
            else
            {
                (*_es)[res] = SingleAbsValue::topConstant();
            }
            return;
        }
        case CmpStmt::FCMP_FALSE:
            (*_es)[res] = 0;
            return;
        case CmpStmt::FCMP_TRUE:
            (*_es)[res] = 1;
            return;
        default:
        {
            assert(false && "undefined compare");
        }
        }
    }
    else if (inVarToAddrsTable(op0))
    {
        (*_es)[res] = SingleAbsValue::topConstant();
    }
    else if (inVarToAddrsTable(op1))
    {
        (*_es)[res] = SingleAbsValue::topConstant();
    }
}

/*!
 * https://llvm.org/docs/LangRef.html#load-instruction
 * @param load
 * @return
 */
void SVFIR2ConsExeState::translateLoad(const LoadStmt *load)
{
    u32_t rhs = load->getRHSVarID();
    u32_t lhs = load->getLHSVarID();
    if (inVarToAddrsTable(rhs))
    {
        const VAddrs &addrs = _es->getVAddrs(rhs);
        for (const auto &addr: addrs)
        {
            assert(isVirtualMemAddress(addr) && "not addr?");
            u32_t objId = getInternalID(addr);
            if (load->getLHSVar()->getType() && load->getLHSVar()->getType()->isPointerTy())
            {
                if (inLocToAddrsTable(objId))
                {
                    _es->getVAddrs(lhs).setBottom();
                }
                break;
            }
            else if (!load->getLHSVar()->getType() && inLocToAddrsTable(objId))
            {
                _es->getVAddrs(lhs).setBottom();
                break;
            }
            else if (inLocToValTable(objId))
            {
                (*_es)[lhs] = SingleAbsValue::bottomConstant();
                break;
            }
        }

        for (const auto &addr: addrs)
        {
            assert(isVirtualMemAddress(addr) && "not addr?");
            u32_t objId = getInternalID(addr);
            if (load->getLHSVar()->getType() && load->getLHSVar()->getType()->isPointerTy())
            {
                if (inLocToAddrsTable(objId))
                {
                    if (!inVarToAddrsTable(lhs))
                    {
                        _es->getVAddrs(lhs) = _es->loadVAddrs(addr);
                    }
                    else
                    {
                        _es->getVAddrs(lhs).join_with(_es->loadVAddrs(addr));
                    }
                }
            }
            else if (!load->getLHSVar()->getType() && inLocToAddrsTable(objId))
            {
                if (!inVarToAddrsTable(lhs))
                {
                    _es->getVAddrs(lhs) = _es->loadVAddrs(addr);
                }
                else
                {
                    _es->getVAddrs(lhs).join_with(_es->loadVAddrs(addr));
                }
            }
            else if (inLocToValTable(objId))
            {
                if (!inVarToValTable(lhs))
                {
                    (*_es)[lhs] = _es->load(SingleAbsValue(getVirtualMemAddress(objId)));
                }
                else
                {
                    (*_es)[lhs].join_with(_es->load(SingleAbsValue(getVirtualMemAddress(objId))));
                }
            }
        }
    }
}

/*!
 * https://llvm.org/docs/LangRef.html#store-instruction
 * @param store
 * @return
 */
void SVFIR2ConsExeState::translateStore(const StoreStmt *store)
{
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();
    if (inVarToAddrsTable(lhs))
    {
        if (inVarToValTable(rhs))
        {
            const VAddrs &addrs = _es->getVAddrs(lhs);
            for (const auto &addr: addrs)
            {
                assert(isVirtualMemAddress(addr) && "not addr?");
                _es->store(SingleAbsValue(addr), (*_es)[rhs]);
            }
        }
        else if (inVarToAddrsTable(rhs))
        {
            const VAddrs &addrs = _es->getVAddrs(lhs);
            for (const auto &addr: addrs)
            {
                assert(isVirtualMemAddress(addr) && "not addr?");
                _es->storeVAddrs(addr, _es->getVAddrs(rhs));
            }
        }
    }
}

/*!
 * https://llvm.org/docs/LangRef.html#conversion-operations
 * @param copy
 * @return
 */
void SVFIR2ConsExeState::translateCopy(const CopyStmt *copy)
{
    u32_t lhs = copy->getLHSVarID();
    u32_t rhs = copy->getRHSVarID();
    if (PAG::getPAG()->isBlkPtr(lhs))
    {
        _es->globalConsES._varToVal[lhs] = SingleAbsValue::topConstant();
    }
    else
    {
        if (inVarToValTable(rhs))
        {
            (*_es)[lhs] = (*_es)[rhs];
        }
        else if (inVarToAddrsTable(rhs))
        {
            _es->getVAddrs(lhs) = _es->getVAddrs(rhs);
        }
    }
}

/*!
 * https://llvm.org/docs/LangRef.html#call-instruction
 * @param callPE
 * @return
 */
void SVFIR2ConsExeState::translateCall(const CallPE *callPE)
{
    NodeID lhs = callPE->getLHSVarID();
    NodeID rhs = callPE->getRHSVarID();
    if (inVarToValTable(rhs))
    {
        (*_es)[lhs] = (*_es)[rhs];
    }
    else if (inVarToAddrsTable(rhs))
    {
        _es->getVAddrs(lhs) = _es->getVAddrs(rhs);
    }
}

void SVFIR2ConsExeState::translateRet(const RetPE *retPE)
{
    NodeID lhs = retPE->getLHSVarID();
    NodeID rhs = retPE->getRHSVarID();
    if (inVarToValTable(rhs))
    {
        (*_es)[lhs] = (*_es)[rhs];
    }
    else if (inVarToAddrsTable(rhs))
    {
        _es->getVAddrs(lhs) = _es->getVAddrs(rhs);
    }
}

/*!
 * https://llvm.org/docs/LangRef.html#getelementptr-instruction
 * @param gep
 * @return
 */
void SVFIR2ConsExeState::translateGep(const GepStmt *gep, bool isGlobal)
{
    u32_t rhs = gep->getRHSVarID();
    u32_t lhs = gep->getLHSVarID();
    // rhs is not initialized
    if (!inVarToAddrsTable(rhs)) return;
    const VAddrs &rhsVal = _es->getVAddrs(rhs);
    if (rhsVal.empty()) return;
    std::pair<s32_t, s32_t> offset = getGepOffset(gep);
    if (offset.first == -1 && offset.second == -1) return;
    if (!isVirtualMemAddress(*rhsVal.begin()))
    {
        return;
    }
    VAddrs gepAddrs;
    s32_t ub = offset.second;
    if (offset.second > (s32_t) Options::MaxFieldLimit() - 1)
    {
        ub = Options::MaxFieldLimit() - 1;
    }
    for (s32_t i = offset.first; i <= ub; i++)
    {
        gepAddrs.join_with(getGepObjAddress(rhs, i));
    }
    if (gepAddrs.empty()) return;
    _es->getVAddrs(lhs) = gepAddrs;
    return;
}

/*!
 * https://llvm.org/docs/LangRef.html#select-instruction
 * @param select
 * @return
 */
void SVFIR2ConsExeState::translateSelect(const SelectStmt *select)
{
    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();
    _es->applySelect(res, cond, tval, fval);
}

/*!
 * https://llvm.org/docs/LangRef.html#i-phi
 * @param phi
 * @return
 */
void SVFIR2ConsExeState::translatePhi(const PhiStmt *phi)
{
    u32_t res = phi->getResID();
    std::vector<u32_t> ops;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        ops.push_back(phi->getOpVarID(i));
    }
    _es->applyPhi(res, ops);
}

//%}
//%}

/*!
 * Return the expr of gep object given a base and offset
 * @param base
 * @param offset
 * @return
 */
SVFIR2ConsExeState::VAddrs SVFIR2ConsExeState::getGepObjAddress(u32_t base, s32_t offset)
{
    const VAddrs &addrs = _es->getVAddrs(base);
    VAddrs ret;
    for (const auto &addr: addrs)
    {
        int64_t baseObj = getInternalID(addr);
        if (baseObj == 0)
        {
            ret.insert(getVirtualMemAddress(0));
            continue;
        }
        assert(SVFUtil::isa<ObjVar>(PAG::getPAG()->getGNode(baseObj)) && "Fail to get the base object address!");
        NodeID gepObj = PAG::getPAG()->getGepObjVar(baseObj, offset);
        initSVFVar(gepObj);
        if (offset == 0 && baseObj != gepObj) _es->getVAddrs(gepObj) = _es->getVAddrs(base);
        ret.insert(getVirtualMemAddress(gepObj));
    }
    return ret;
}

/*!
 * Return the offset expression of a GepStmt
 * @param gep
 * @return
 */
std::pair<s32_t, s32_t> SVFIR2ConsExeState::getGepOffset(const SVF::GepStmt *gep)
{
    /// for instant constant index, e.g.  gep arr, 1
    if (gep->getOffsetVarAndGepTypePairVec().empty())
        return std::make_pair(gep->getConstantFieldIdx(), gep->getConstantFieldIdx());

    s32_t totalOffset = 0;
    /// default value of MaxFieldLimit is 512
    u32_t maxFieldLimit = Options::MaxFieldLimit() - 1;
    /// for variable index and nested indexes, e.g. 1) gep arr, idx  2) gep arr idx0, idx1
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const SVFValue *value = gep->getOffsetVarAndGepTypePairVec()[i].first->getValue();
        const SVFType *type = gep->getOffsetVarAndGepTypePairVec()[i].second;
        const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(value);
        s32_t offset = 0;
        /// offset is constant but stored in variable
        if (op)
        {
            offset = op->getSExtValue();
        }
        /// offset is variable, the concrete value isn't sure util runtime, and maybe not a concrete value.
        /// e.g.
        else
        {
            u32_t idx = PAG::getPAG()->getValueNode(value);
            if (!inVarToValTable(idx)) return std::make_pair(-1, -1);
            if ((*_es)[idx].isBottom() || (*_es)[idx].isTop() || (*_es)[idx].isSym())
            {
                return std::make_pair(0, (s32_t) Options::MaxFieldLimit());
            }

            // if idxVal is a concrete value
            offset = ConsExeState::z3Expr2NumValue((*_es)[idx]);
        }
        if (type == nullptr)
        {
            if ((long long) (totalOffset + offset) > maxFieldLimit)
            {
                totalOffset = maxFieldLimit;
            }
            else
            {
                totalOffset += offset;
            }
            continue;
        }

        if (const SVFPointerType *pty = SVFUtil::dyn_cast<SVFPointerType>(type))
        {
            offset = offset * gep->getAccessPath().getElementNum(pty->getPtrElementType());
        }
        else
        {
            const std::vector<u32_t> &so = SymbolTableInfo::SymbolInfo()->getTypeInfo(
                                               type)->getFlattenedElemIdxVec();
            if (so.empty() || (u32_t) offset >= so.size())
                return std::make_pair(-1, -1);
            offset = SymbolTableInfo::SymbolInfo()->getFlattenedElemIdx(type, offset);
        }
        if ((long long) (totalOffset + offset) > maxFieldLimit)
        {
            totalOffset = maxFieldLimit;
        }
        else
        {
            totalOffset += offset;
        }
    }
    std::pair<s32_t, s32_t> offSetPair;
    offSetPair.first = totalOffset;
    offSetPair.second = totalOffset;
    return offSetPair;
}


void SVFIR2ConsExeState::initValVar(const ValVar *valVar, u32_t varId)
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
            (*_es)[varId] = SingleAbsValue::topConstant();
        else
        {

            SVFUtil::errs() << valVar->getValue()->toString() << "\n" << " type: " << type->toString() << "\n";
            assert(false && "what other types we have");
        }
    }
    else
    {
        if (svfir->getNullPtr() == valVar->getId())
            (*_es)[varId] = 0;
        else
            (*_es)[varId] = SingleAbsValue::topConstant();
        assert(SVFUtil::isa<DummyValVar>(valVar) && "not a DummValVar if it has no type?");
    }
}

/*!
 * Init Z3Expr for ObjVar
 * @param objVar
 * @param valExprIdToCondValPairMap
 */
void SVFIR2ConsExeState::initObjVar(const ObjVar *objVar, u32_t varId)
{
    if (objVar->hasValue())
    {
        const MemObj *obj = objVar->getMemObj();
        /// constant data
        if (obj->isConstDataOrAggData() || obj->isConstantArray() || obj->isConstantStruct())
        {
            if (const SVFConstantInt *consInt = SVFUtil::dyn_cast<SVFConstantInt>(obj->getValue()))
            {
                _es->globalConsES._varToVal[varId] = consInt->getSExtValue();
            }
            else if (const SVFConstantFP *consFP = SVFUtil::dyn_cast<SVFConstantFP>(obj->getValue()))
                _es->globalConsES._varToVal[varId] = consFP->getFPValue();
            else if (SVFUtil::isa<SVFConstantNullPtr>(obj->getValue()))
                _es->globalConsES._varToVal[varId] = 0;
            else if (SVFUtil::isa<SVFGlobalValue>(obj->getValue()))
                _es->globalConsES._varToVAddrs[varId] = getVirtualMemAddress(varId);
            else if (obj->isConstDataOrAggData())
            {
                // TODO
                //                assert(false && "implement this part");
                _es->globalConsES._varToVal[varId] = SingleAbsValue::topConstant();
            }
            else
            {
                _es->globalConsES._varToVal[varId] = SingleAbsValue::topConstant();
            }
        }
        else
        {
            _es->globalConsES._varToVAddrs[varId] = getVirtualMemAddress(varId);
        }
    }
    else
    {
        _es->globalConsES._varToVAddrs[varId] = getVirtualMemAddress(varId);
    }
}


void SVFIR2ConsExeState::initSVFVar(u32_t varId)
{
    if (_es->inVarToVal(varId)) return;
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

void SVFIR2ConsExeState::moveToGlobal()
{
    for (const auto &it: _es->_varToVal)
    {
        ConsExeState::globalConsES._varToVal.insert(it);
    }
    for (const auto &it: _es->_locToVal)
    {
        ConsExeState::globalConsES._locToVal.insert(it);
    }
    for (const auto &it: _es->_varToVAddrs)
    {
        ConsExeState::globalConsES._varToVAddrs.insert(it);
    }
    for (const auto &it: _es->_locToVAddrs)
    {
        ConsExeState::globalConsES._locToVAddrs.insert(it);
    }
    ConsExeState::globalConsES._varToVAddrs[0] = ConsExeState::getVirtualMemAddress(0);
    _es->_varToVal.clear();
    _es->_locToVal.clear();
    _es->_varToVAddrs.clear();
    _es->_locToVAddrs.clear();
}
