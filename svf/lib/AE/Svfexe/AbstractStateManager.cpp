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
        svfgBuilder = std::make_unique<SVFGBuilder>(true);
        svfg = svfgBuilder->buildFullSVFG(pta);
    }
}

AbstractStateManager::~AbstractStateManager()
{
    // svfg is owned by svfgBuilder's unique_ptr; no explicit delete needed.
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
    u32_t sparsity = Options::AESparsity();
    if (sparsity == AbstractInterpretation::AESparsity::SemiSparse ||
            sparsity == AbstractInterpretation::AESparsity::Sparse)
    {
        // Semi / Full sparse: only replace ObjVar state. ValVars live at
        // their def-sites and must not be overwritten by state replacement.
        // (In Full Sparse we still copy the merged ObjVar side for now —
        // Phase 1b will gate this for memory reduction.)
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
    bool denseOnly = Options::AESparsity() == AbstractInterpretation::AESparsity::Dense;

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
    //
    // Sparse modes (SemiSparse / Sparse) skip this check: merge doesn't
    // populate ValVars at non-def-site nodes, and reading a stale
    // transfer-produced value at the current node would ignore the
    // cycle-widened value at the def-site.
    if (denseOnly)
    {
        if (as.inVarToValTable(id) || as.inVarToAddrsTable(id))
            return as[id];
    }

    // Sparse modes: pull from def-site first, then check current state
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
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    if (Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse)
    {
        // Full-sparse: _addrToAbsVal merge is gated in joinWith; only the
        // def-anchor's own ICFG holds the obj's value (written by the
        // per-kind transfer or store/ExtAPI handler). Route the read to
        // that anchor via getDefSiteOfObjVar, then compose the
        // path-refinement overlay (if any) via meet.
        AbstractState& hereState = getAbstractState(node);

        // Locate the base value.
        const AbstractValue* base = nullptr;
        // Fast-path: value already cached at this node (store/ExtAPI wrote
        // here, or a previous materialize landed here).
        if (hereState.getLocToVal().count(var->getId()) != 0)
        {
            base = &hereState.load(addr);
        }
        else
        {
            const ICFGNode* defICFG = getDefSiteOfObjVar(var, node);
            if (defICFG && hasAbstractState(defICFG))
            {
                AbstractState& defState = getAbstractState(defICFG);
                if (defState.getLocToVal().count(var->getId()) != 0)
                    base = &defState.load(addr);
            }
        }

        // Overlay compose: if any branch refinement reached this node for
        // this obj, meet it into the base.  No overlay entry = no-op.
        auto ait = pathRefinedAt.find(node);
        if (ait != pathRefinedAt.end())
        {
            auto oit = ait->second.find(var->getId());
            if (oit != ait->second.end())
            {
                AbstractValue composed = base ? *base : AbstractValue();
                composed.meet_with(oit->second);
                // Materialize at hereState so the returned reference is
                // stable and repeated reads at the same node are O(1).
                hereState.store(addr, composed);
                return hereState.load(addr);
            }
        }

        if (base)
            return *base;
        // No reaching def + no overlay: top via local slot.
        return hereState.load(addr);
    }
    AbstractState& as = getAbstractState(node);
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
    // In sparse modes (SemiSparse / Sparse), write to the var's def-site so
    // that getAbstractValue (which reads from def-site) stays consistent.
    // This matters especially for widen/narrow scatter calls that pass
    // cycle_head as `node` but expect the value to land on body def-sites.
    const ICFGNode* target = node;
    u32_t s = Options::AESparsity();
    if (s == AbstractInterpretation::AESparsity::SemiSparse ||
            s == AbstractInterpretation::AESparsity::Sparse)
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
    bool sparse =
        Options::AESparsity() == AbstractInterpretation::AESparsity::Sparse;
    for (auto addr : ptrVal.getAddrs())
    {
        if (sparse)
        {
            // Route each ObjVar read through getAbstractValue(ObjVar*),
            // which in Sparse mode redirects to the obj's MSSA def-anchor
            // trace instead of the local one (which joinWith no longer
            // fills).  Non-ObjVar addresses (null/blackhole) still read
            // locally via as.load.
            u32_t objId = as.getIDFromAddr(addr);
            const SVFVar* v = svfir->getGNode(objId);
            if (const ObjVar* obj = SVFUtil::dyn_cast<ObjVar>(v))
            {
                res.join_with(getAbstractValue(obj, node));
                continue;
            }
        }
        res.join_with(as.load(addr));
    }
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

// ===----------------------------------------------------------------------===//
//  MSSA def-anchor transfers (full-sparse Phase A)
//
//  Each helper walks SVFG only through its public graph surface
//  (InEdgeBegin/End + IndirectSVFGEdge::getPointsTo + type casts) and fills
//  trace[N]._addrToAbsVal[obj] for every obj served by a def-anchor hosted
//  at N. See plan/plan-per-kind-transfers.md §"Core mechanism" for the
//  unified pattern, and §"Mechanism / MU-vs-CHI" for the two-hop rule used
//  by FormalIN and ActualOUT (their direct upstream is a MU that does not
//  itself hold a value; recurse one more indirect-edge hop to the CHI).
//
//  Two-hop FormalIN/ActualOUT chains verified empirically via
//  `wpa -ander -svfg -dump-vfg` on a minimal interproc test — see
//  /tmp/mssa-phi-test/case_interproc.c during plan design.
// ===----------------------------------------------------------------------===//

namespace
{
/// Collect the set of ObjVar IDs an MRSVFGNode-like node services.
/// All memory-side SVFG nodes used by the transfers expose an MRVer.
template <class MRNodeT>
inline NodeBS getMRNodeObjIds(const MRNodeT* mrNode)
{
    return mrNode->getMRVer()->getMR()->getPointsTo();
}

/// Unified "meet over upstream def-anchors" pattern. Given a worklist of
/// upstream SVFG nodes (already filtered to those actually serving `objId`),
/// read each one's ICFG-anchored trace value and meet them.
/// Returns (meet, hasAny): hasAny=false means no upstream had a value yet.
inline std::pair<AbstractValue, bool> meetFromUpstreams(
    const std::vector<const VFGNode*>& upstreams,
    u32_t objId,
    const Map<const ICFGNode*, AbstractState>& trace)
{
    AbstractValue meet;
    bool hasAny = false;
    for (const VFGNode* U : upstreams)
    {
        auto it = trace.find(U->getICFGNode());
        if (it == trace.end()) continue;
        const auto& map = it->second.getLocToVal();
        auto mit = map.find(objId);
        if (mit == map.end()) continue;
        if (!hasAny)
        {
            meet = mit->second;
            hasAny = true;
        }
        else
        {
            meet.join_with(mit->second);
        }
    }
    return {meet, hasAny};
}

/// Commit `meet` into the target node's trace for `objId` **only if the
/// slot is currently empty**. This preserves strict transfer additivity:
///   - Phase A (merge intact): when merge has already written a value —
///     potentially refined by isCmpBranchFeasible's pred-state narrowing,
///     which lives on edges and is NOT visible to upstream def-anchor
///     reads — transfer defers to merge. Transfer only fills slots merge
///     left empty (rare in Phase A; not a meaningful code path).
///   - Phase B (merge gated): the slot is always empty when the transfer
///     runs, so transfer becomes the primary filler.
/// Merging with the existing value would risk widening away merge's
/// branch-refined precision (transfer pulls un-refined upstream values).
inline void commitTransferValue(AbstractState& target,
                                u32_t objId,
                                const AbstractValue& meet)
{
    const auto& map = target.getLocToVal();
    if (map.find(objId) != map.end())
        return;
    target.store(AbstractState::getVirtualMemAddress(objId), meet);
}
}

void AbstractStateManager::runMSSAPHITransferAt(const ICFGNode* N)
{
    if (Options::AESparsity() != AbstractInterpretation::AESparsity::Sparse)
        return;
    for (const VFGNode* vn : N->getVFGNodes())
    {
        const MSSAPHISVFGNode* phi = SVFUtil::dyn_cast<MSSAPHISVFGNode>(vn);
        if (!phi) continue;
        // MSSAPHISVFGNode exposes MRVer via getResVer(); only the
        // IntraMSSAPHISVFGNode subclass aliases it as getMRVer.
        NodeBS objIds = phi->getResVer()->getMR()->getPointsTo();
        // One-hop: PHI operand defs arrive as indirect in-edges on the PHI
        // itself (SVFG's connectIndirectSVFGEdges wires them). Each in-edge
        // source is a real CHI-holding SVFG node (Store / nested MSSAPHI /
        // FormalIN / ActualOUT / FormalOUT of inner calls).
        std::vector<const VFGNode*> upstreams;
        for (auto it = phi->InEdgeBegin(), eit = phi->InEdgeEnd(); it != eit; ++it)
        {
            const IndirectSVFGEdge* ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(*it);
            if (!ie) continue;
            // Edge pts must cover at least one of our objIds; we still
            // per-obj filter inside the loop below.
            upstreams.push_back(ie->getSrcNode());
        }
        AbstractState& here = abstractTrace[N];
        for (NodeID objId : objIds)
        {
            // Filter upstreams to those whose edge carries this specific obj.
            std::vector<const VFGNode*> filtered;
            for (auto it = phi->InEdgeBegin(), eit = phi->InEdgeEnd(); it != eit; ++it)
            {
                const IndirectSVFGEdge* ie = SVFUtil::dyn_cast<IndirectSVFGEdge>(*it);
                if (ie && ie->getPointsTo().test(objId))
                    filtered.push_back(ie->getSrcNode());
            }
            auto [meet, ok] = meetFromUpstreams(filtered, objId, abstractTrace);
            if (ok)
                commitTransferValue(here, objId, meet);
        }
    }
}

void AbstractStateManager::runFormalINTransferAt(const ICFGNode* N)
{
    if (Options::AESparsity() != AbstractInterpretation::AESparsity::Sparse)
        return;
    if (!SVFUtil::isa<FunEntryICFGNode>(N))
        return;
    for (const VFGNode* vn : N->getVFGNodes())
    {
        const FormalINSVFGNode* fin = SVFUtil::dyn_cast<FormalINSVFGNode>(vn);
        if (!fin) continue;
        NodeBS objIds = getMRNodeObjIds(fin);
        AbstractState& here = abstractTrace[N];
        for (NodeID objId : objIds)
        {
            // Two-hop: fin <-CallIndEdge- ActualIN (Mu, no value) <-IndEdge- caller CHI.
            // Collect the caller-side CHIs by walking one more hop past
            // each ActualIN.
            std::vector<const VFGNode*> upstreams;
            for (auto it = fin->InEdgeBegin(), eit = fin->InEdgeEnd(); it != eit; ++it)
            {
                const CallIndSVFGEdge* ce = SVFUtil::dyn_cast<CallIndSVFGEdge>(*it);
                if (!ce || !ce->getPointsTo().test(objId)) continue;
                const VFGNode* ain = ce->getSrcNode();
                for (auto it2 = ain->InEdgeBegin(), eit2 = ain->InEdgeEnd();
                        it2 != eit2; ++it2)
                {
                    const IndirectSVFGEdge* ie2 = SVFUtil::dyn_cast<IndirectSVFGEdge>(*it2);
                    if (ie2 && ie2->getPointsTo().test(objId))
                        upstreams.push_back(ie2->getSrcNode());
                }
            }
            auto [meet, ok] = meetFromUpstreams(upstreams, objId, abstractTrace);
            if (!ok)
            {
                // Fallback: program entry / orphan — seed from globalICFGNode.
                const ICFGNode* g = svfir->getICFG()->getGlobalICFGNode();
                auto git = abstractTrace.find(g);
                if (git != abstractTrace.end())
                {
                    const auto& gmap = git->second.getLocToVal();
                    auto mit = gmap.find(objId);
                    if (mit != gmap.end())
                    {
                        meet = mit->second;
                        ok = true;
                    }
                }
            }
            if (ok)
                commitTransferValue(here, objId, meet);
        }
    }
}

void AbstractStateManager::runActualOUTTransferAt(const ICFGNode* N)
{
    if (Options::AESparsity() != AbstractInterpretation::AESparsity::Sparse)
        return;
    // APOUT VFG nodes are hosted at the RetICFGNode (not the CallICFGNode).
    // Driver calls this hook when visiting the retNode; sanity-gate on both
    // the retNode shape and the matching callNode not being ExtAPI.
    const RetICFGNode* retNode = SVFUtil::dyn_cast<RetICFGNode>(N);
    if (!retNode)
        return;
    const CallICFGNode* callNode = retNode->getCallICFGNode();
    if (!callNode || SVFUtil::isExtCall(callNode))
        return;
    for (const VFGNode* vn : retNode->getVFGNodes())
    {
        const ActualOUTSVFGNode* aout = SVFUtil::dyn_cast<ActualOUTSVFGNode>(vn);
        if (!aout) continue;
        NodeBS objIds = getMRNodeObjIds(aout);
        AbstractState& here = abstractTrace[retNode];
        for (NodeID objId : objIds)
        {
            // Two-hop: aout <-RetIndEdge- FormalOUT (RetMu, no value)
            //              <-IndEdge- callee-side CHI (Store / MSSAPHI / ...).
            std::vector<const VFGNode*> upstreams;
            for (auto it = aout->InEdgeBegin(), eit = aout->InEdgeEnd(); it != eit; ++it)
            {
                const RetIndSVFGEdge* re = SVFUtil::dyn_cast<RetIndSVFGEdge>(*it);
                if (!re || !re->getPointsTo().test(objId)) continue;
                const VFGNode* fout = re->getSrcNode();
                for (auto it2 = fout->InEdgeBegin(), eit2 = fout->InEdgeEnd();
                        it2 != eit2; ++it2)
                {
                    const IndirectSVFGEdge* ie2 = SVFUtil::dyn_cast<IndirectSVFGEdge>(*it2);
                    if (ie2 && ie2->getPointsTo().test(objId))
                        upstreams.push_back(ie2->getSrcNode());
                }
            }
            auto [meet, ok] = meetFromUpstreams(upstreams, objId, abstractTrace);
            if (ok)
                commitTransferValue(here, objId, meet);
        }
    }
}

