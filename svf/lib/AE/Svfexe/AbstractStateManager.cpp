//===- AbstractStateManager.cpp -- State management for abstract execution -//
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
//  Created on: Mar 2026
//      Author: Jiawei Wang
//
#include "AE/Svfexe/AbstractStateManager.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"

using namespace SVF;

AbstractStateManager::AbstractStateManager(SVFIR* svfir, AndersenWaveDiff* pta)
    : svfir(svfir), svfg(nullptr)
{
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse)
    {
        SVFGBuilder memSSA(true);
        svfg = memSSA.buildFullSVFG(pta);
    }
}

AbstractStateManager::~AbstractStateManager()
{
    delete svfg;
}

// ===----------------------------------------------------------------------===//
//  State Access
// ===----------------------------------------------------------------------===//

AbstractState& AbstractStateManager::getAbstractState(const ICFGNode* node)
{
    if (abstractTrace.count(node) == 0)
    {
        assert(false && "No preAbsTrace for this node");
        abort();
    }
    return abstractTrace[node];
}

void AbstractStateManager::updateAbstractState(const ICFGNode* node, const AbstractState& state)
{
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::SemiSparse)
    {
        // Semi-sparse: only replace ObjVar state. ValVars live at their
        // def-sites and must not be overwritten by state replacement.
        abstractTrace[node].updateAddrStateOnly(state);
    }
    else
    {
        abstractTrace[node] = state;
    }
}

bool AbstractStateManager::hasAbstractState(const ICFGNode* node)
{
    return abstractTrace.count(node) != 0;
}

// ===----------------------------------------------------------------------===//
//  Abstract Value Access — ValVar
// ===----------------------------------------------------------------------===//

const AbstractValue& AbstractStateManager::getAbstractValue(const ValVar* var, const ICFGNode* node)
{
    u32_t id = var->getId();
    bool semiSparse = Options::AESparsity() == AbstractInterpretation::AESparsity::SemiSparse;

    // Constants: store into current node's state and return
    AbstractState& as = abstractTrace[node];
    if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
    {
        as[id] = IntervalValue(constInt->getSExtValue(), constInt->getSExtValue());
        return as[id];
    }
    if (const ConstFPValVar* constFP = SVFUtil::dyn_cast<ConstFPValVar>(var))
    {
        as[id] = IntervalValue(constFP->getFPValue(), constFP->getFPValue());
        return as[id];
    }
    if (SVFUtil::isa<ConstNullPtrValVar>(var))
    {
        as[id] = AddressValue();
        return as[id];
    }
    if (SVFUtil::isa<ConstDataValVar>(var))
    {
        as[id] = IntervalValue::top();
        return as[id];
    }

    // Dense mode: try current node's state first.
    // If absent, fall through to the def-site lookup below — matching
    // upstream behaviour where getAbstractValue always reads from the
    // def-site.  Returning top or bottom here would be wrong: top loses
    // concrete values like NULL addresses; bottom misjudges branch
    // feasibility for uninitialised variables like argc.
    if (!semiSparse)
    {
        if (as.inVarToValTable(id) || as.inVarToAddrsTable(id))
            return as[id];
    }

    // Semi-sparse mode: pull from def-site first, then check current state
    const ICFGNode* defNode = var->getICFGNode();
    if (defNode && hasAbstractState(defNode))
    {
        const auto& varMap = getAbstractState(defNode).getVarToVal();
        if (varMap.count(id))
            return getAbstractState(defNode)[id];
    }

    // Fallback for call-result ValVars: their getICFGNode() returns
    // CallICFGNode but the value is written by RetPE at RetICFGNode.
    if (const CallICFGNode* callNode =
                defNode ? SVFUtil::dyn_cast<CallICFGNode>(defNode) : nullptr)
    {
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        if (hasAbstractState(retNode))
        {
            const auto& retMap = getAbstractState(retNode).getVarToVal();
            if (retMap.count(id))
                return getAbstractState(retNode)[id];
        }
    }

    as[id] = IntervalValue::top();
    return as[id];
}

// ===----------------------------------------------------------------------===//
//  Abstract Value Access — ObjVar / SVFVar dispatch
// ===----------------------------------------------------------------------===//

const AbstractValue& AbstractStateManager::getAbstractValue(const ObjVar* var, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    return as.load(addr);
}

const AbstractValue& AbstractStateManager::getAbstractValue(const SVFVar* var, const ICFGNode* node)
{
    // Check ObjVar first since ObjVar inherits from ValVar
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        return getAbstractValue(objVar, node);
    if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        return getAbstractValue(valVar, node);
    assert(false && "Unknown SVFVar kind");
    abort();
}

// ===----------------------------------------------------------------------===//
//  hasAbstractValue — side-effect-free existence check
//
//  Mirrors the lookup chain in getAbstractValue but stops short of the
//  top-fallback.  Returns false when getAbstractValue would only be able
//  to return top as a default.
// ===----------------------------------------------------------------------===//

bool AbstractStateManager::hasAbstractValue(const ValVar* var, const ICFGNode* node) const
{
    // Constants are always "present" (their value is intrinsic).
    if (SVFUtil::isa<ConstIntValVar>(var) || SVFUtil::isa<ConstFPValVar>(var) ||
            SVFUtil::isa<ConstNullPtrValVar>(var) || SVFUtil::isa<ConstDataValVar>(var))
        return true;

    u32_t id = var->getId();
    bool semiSparse = Options::AESparsity() == AbstractInterpretation::AESparsity::SemiSparse;

    // Dense mode: stored at the current node.
    if (!semiSparse)
    {
        auto it = abstractTrace.find(node);
        if (it != abstractTrace.end() &&
                (it->second.inVarToValTable(id) || it->second.inVarToAddrsTable(id)))
            return true;
    }

    // Semi-sparse (and dense fall-through): check the def-site.
    const ICFGNode* defNode = var->getICFGNode();
    if (defNode)
    {
        auto it = abstractTrace.find(defNode);
        if (it != abstractTrace.end() && it->second.getVarToVal().count(id))
            return true;

        // Fallback for call-result ValVars: value lives at the RetICFGNode.
        if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(defNode))
        {
            const RetICFGNode* retNode = callNode->getRetICFGNode();
            auto rit = abstractTrace.find(retNode);
            if (rit != abstractTrace.end() && rit->second.getVarToVal().count(id))
                return true;
        }
    }
    return false;
}

bool AbstractStateManager::hasAbstractValue(const ObjVar* var, const ICFGNode* node) const
{
    auto it = abstractTrace.find(node);
    if (it == abstractTrace.end())
        return false;
    u32_t objId = var->getId();
    return it->second.getLocToVal().count(objId) != 0;
}

bool AbstractStateManager::hasAbstractValue(const SVFVar* var, const ICFGNode* node) const
{
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        return hasAbstractValue(objVar, node);
    if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        return hasAbstractValue(valVar, node);
    return false;
}

// ===----------------------------------------------------------------------===//
//  Update Abstract Value
// ===----------------------------------------------------------------------===//

void AbstractStateManager::updateAbstractValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node)
{
    // In semi-sparse mode, write to the var's def-site so that
    // getAbstractValue (which reads from def-site) stays consistent.
    const ICFGNode* target = node;
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::SemiSparse)
    {
        const ICFGNode* defNode = var->getICFGNode();
        if (defNode)
            target = defNode;
    }
    abstractTrace[target][var->getId()] = val;
}

void AbstractStateManager::updateAbstractValue(const ObjVar* var, const AbstractValue& val, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    as.store(addr, val);
}

void AbstractStateManager::updateAbstractValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node)
{
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        updateAbstractValue(objVar, val, node);
    else if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        updateAbstractValue(valVar, val, node);
    else
        assert(false && "Unknown SVFVar kind");
}

// ===----------------------------------------------------------------------===//
//  Filtered State Access
// ===----------------------------------------------------------------------===//

void AbstractStateManager::getAbstractState(const Set<const ValVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    for (const ValVar* var : vars)
    {
        u32_t id = var->getId();
        result[id] = as[id];
    }
}

void AbstractStateManager::getAbstractState(const Set<const ObjVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    for (const ObjVar* var : vars)
    {
        u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
        result.store(addr, as.load(addr));
    }
}

void AbstractStateManager::getAbstractState(const Set<const SVFVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
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

// ===----------------------------------------------------------------------===//
//  GEP Helpers
// ===----------------------------------------------------------------------===//

IntervalValue AbstractStateManager::getGepElementIndex(const GepStmt* gep)
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
        if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
            idxLb = idxUb = constInt->getSExtValue();
        else
        {
            IntervalValue idxItv = getAbstractValue(var, node).getInterval();
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

IntervalValue AbstractStateManager::getGepByteOffset(const GepStmt* gep)
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
                s64_t lb = (double)Options::MaxFieldLimit() / elemByteSize >= op->getSExtValue()
                           ? op->getSExtValue() * elemByteSize
                           : Options::MaxFieldLimit();
                res = res + IntervalValue(lb, lb);
            }
            else
            {
                IntervalValue idxVal = getAbstractValue(idxOperandVar, node).getInterval();
                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                {
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

AddressValue AbstractStateManager::getGepObjAddrs(const ValVar* pointer, IntervalValue offset)
{
    const ICFGNode* node = pointer->getICFGNode();
    AddressValue gepAddrs;
    AbstractState& as = getAbstractState(node);
    APOffset lb = offset.lb().getIntNumeral() < Options::MaxFieldLimit() ? offset.lb().getIntNumeral()
                  : Options::MaxFieldLimit();
    APOffset ub = offset.ub().getIntNumeral() < Options::MaxFieldLimit() ? offset.ub().getIntNumeral()
                  : Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
    {
        const AbstractValue& addrs = getAbstractValue(pointer, node);
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

// ===----------------------------------------------------------------------===//
//  Load / Store through pointer
// ===----------------------------------------------------------------------===//

AbstractValue AbstractStateManager::loadValue(const ValVar* pointer, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbstractValue(pointer, node);
    AbstractState& as = getAbstractState(node);
    AbstractValue res;
    for (auto addr : ptrVal.getAddrs())
        res.join_with(as.load(addr));
    return res;
}

void AbstractStateManager::storeValue(const ValVar* pointer, const AbstractValue& val, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbstractValue(pointer, node);
    AbstractState& as = getAbstractState(node);
    for (auto addr : ptrVal.getAddrs())
        as.store(addr, val);
}

// ===----------------------------------------------------------------------===//
//  Type / Size Helpers
// ===----------------------------------------------------------------------===//

const SVFType* AbstractStateManager::getPointeeElement(const ObjVar* var, const ICFGNode* node)
{
    const AbstractValue& ptrVal = getAbstractValue(var, node);
    if (!ptrVal.isAddr())
        return nullptr;
    for (auto addr : ptrVal.getAddrs())
    {
        NodeID objId = getAbstractState(node).getIDFromAddr(addr);
        if (objId == 0)
            continue;
        return svfir->getBaseObject(objId)->getType();
    }
    return nullptr;
}

u32_t AbstractStateManager::getAllocaInstByteSize(const AddrStmt* addr)
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
                const AbstractValue& sizeVal = getAbstractValue(value, node);
                IntervalValue itv = sizeVal.getInterval();
                if (itv.isBottom())
                    itv = IntervalValue(Options::MaxFieldLimit());
                res = res * itv.ub().getIntNumeral() > Options::MaxFieldLimit()
                      ? Options::MaxFieldLimit() : res * itv.ub().getIntNumeral();
            }
            return (u32_t)res;
        }
    }
    assert(false && "Addr rhs value is not ObjVar");
    abort();
}

// ===----------------------------------------------------------------------===//
//  Def/Use site queries
// ===----------------------------------------------------------------------===//

Set<const ICFGNode*> AbstractStateManager::getUseSitesOfObjVar(const ObjVar* obj, const ICFGNode* node) const
{
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse)
    {
        assert(svfg && "SVFG is not built for sparse AE");
        return svfg->getUseSitesOfObjVar(obj, node);
    }
    Set<const ICFGNode*> succs;
    for (const auto* edge : node->getOutEdges())
        succs.insert(edge->getDstNode());
    return succs;
}

Set<const ICFGNode*> AbstractStateManager::getUseSitesOfValVar(const ValVar* var) const
{
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse)
    {
        assert(svfg && "SVFG is not built for sparse AE");
        return svfg->getUseSitesOfValVar(var);
    }
    Set<const ICFGNode*> succs;
    if (const ICFGNode* node = var->getICFGNode())
    {
        for (const auto* edge : node->getOutEdges())
            succs.insert(edge->getDstNode());
    }
    return succs;
}

const ICFGNode* AbstractStateManager::getDefSiteOfValVar(const ValVar* var) const
{
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse)
    {
        assert(svfg && "SVFG is not built for sparse AE");
        return svfg->getDefSiteOfValVar(var);
    }
    return var->getICFGNode();
}

const ICFGNode* AbstractStateManager::getDefSiteOfObjVar(const ObjVar* obj, const ICFGNode* node) const
{
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse)
    {
        assert(svfg && "SVFG is not built for sparse AE");
        return svfg->getDefSiteOfObjVar(obj, node);
    }
    for (const auto* edge : node->getInEdges())
        return edge->getSrcNode();
    return nullptr;
}

