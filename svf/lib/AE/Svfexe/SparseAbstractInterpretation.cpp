//===- SparseAbstractInterpretation.cpp -- Sparse Abstract Execution----//
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
//===---------------------------------------------------------------------===//

#include "AE/Svfexe/SparseAbstractInterpretation.h"
#include "AE/Svfexe/PreAnalysis.h"
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"

using namespace SVF;

// =====================================================================
//  Semi-sparse state-access overrides (used by both SemiSparse and
//  FullSparse subclasses; the latter further restricts ValVar reads).
// =====================================================================

void SemiSparseAbstractInterpretation::updateAbsState(
    const ICFGNode* node, const AbstractState& state)
{
    // Only replace ObjVar state.  ValVars live at their def-sites and
    // must not be overwritten when the predecessor's state is merged in.
    abstractTrace[node].updateAddrStateOnly(state);
}

void SemiSparseAbstractInterpretation::joinStates(
    AbstractState& dst, const AbstractState& src)
{
    // ValVars live at def-sites in semi-sparse mode; they don't flow
    // through state merges.  Snapshot dst's ValVar map, perform the full
    // join, then restore the snapshot — leaving only ObjVars and freed
    // addrs merged.
    auto saved = dst.getVarToVal();
    dst.joinWith(src);
    dst.clearValVars();
    for (const auto& [id, val] : saved)
        dst[id] = val;
}

const AbstractValue& SemiSparseAbstractInterpretation::getAbsValue(
    const ValVar* var, const ICFGNode* node)
{
    u32_t id = var->getId();
    AbstractState& as = abstractTrace[node];

    // LLVM IR literal constants (Int / FP / NullPtr / Data) are modelled
    // as ValVars but have no ICFGNode — `var->getICFGNode()` returns
    // null, so the def-site fallback below can't reach them.  Resolve
    // them by SVFVar kind directly.  In dense this is unnecessary
    // because joinStates copies their global-init entries forward and
    // the in-map lookup at the current node finds them.
    if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
    {
        as[id] = IntervalValue(constInt->getSExtValue(), constInt->getSExtValue());
        return as[id];
    }
    else if (const ConstFPValVar* constFP = SVFUtil::dyn_cast<ConstFPValVar>(var))
    {
        as[id] = IntervalValue(constFP->getFPValue(), constFP->getFPValue());
        return as[id];
    }
    else if (SVFUtil::isa<ConstNullPtrValVar>(var))
    {
        as[id] = AddressValue();
        return as[id];
    }
    else if (SVFUtil::isa<ConstDataValVar>(var))
    {
        as[id] = IntervalValue::top();
        return as[id];
    }

    // Pull from def-site; for call results, the value lives at RetICFGNode.
    const ICFGNode* defNode = var->getICFGNode();
    if (defNode && hasAbsState(defNode))
    {
        const auto& varMap = getAbsState(defNode).getVarToVal();
        if (varMap.count(id))
            return getAbsState(defNode)[id];
    }
    
    // If this is a call result, try the call's RetICFGNode as well.
    if (const CallICFGNode* callNode =
                defNode ? SVFUtil::dyn_cast<CallICFGNode>(defNode) : nullptr)
    {
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        if (hasAbsState(retNode))
        {
            const auto& retMap = getAbsState(retNode).getVarToVal();
            if (retMap.count(id))
                return getAbsState(retNode)[id];
        }
    }

    as[id] = IntervalValue::top();
    return as[id];
}

bool SemiSparseAbstractInterpretation::hasAbsValue(
    const ValVar* var, const ICFGNode*) const
{
    if (SVFUtil::isa<ConstIntValVar>(var) || SVFUtil::isa<ConstFPValVar>(var) ||
            SVFUtil::isa<ConstNullPtrValVar>(var) || SVFUtil::isa<ConstDataValVar>(var))
        return true;

    u32_t id = var->getId();
    const ICFGNode* defNode = var->getICFGNode();
    if (!defNode)
        return false;

    auto dit = abstractTrace.find(defNode);
    if (dit != abstractTrace.end() && dit->second.getVarToVal().count(id))
        return true;
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(defNode))
    {
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        auto rit = abstractTrace.find(retNode);
        if (rit != abstractTrace.end() && rit->second.getVarToVal().count(id))
            return true;
    }
    return false;
}

void SemiSparseAbstractInterpretation::updateAbsValue(
    const ValVar* var, const AbstractValue& val, const ICFGNode* node)
{
    // Write to the var's def-site so getAbsValue stays consistent.
    const ICFGNode* defNode = var->getICFGNode();
    abstractTrace[defNode ? defNode : node][var->getId()] = val;
}

// =====================================================================
//  Full-sparse — SVFG-backed def/use; ValVar reads stubbed.
// =====================================================================

void FullSparseAbstractInterpretation::initFromPTA(AndersenWaveDiff* pta)
{
    SemiSparseAbstractInterpretation::initFromPTA(pta);
    SVFGBuilder memSSA(true);
    svfg = memSSA.buildFullSVFG(pta);
}

// TODO(full-sparse): route ValVar reads through the SVFG's reaching-def
// query.  Stub until that lands so misuse fails loudly instead of
// silently inheriting semi-sparse semantics.
const AbstractValue& FullSparseAbstractInterpretation::getAbsValue(
    const ValVar*, const ICFGNode*)
{
    assert(false && "FullSparseAbstractInterpretation::getAbsValue not implemented");
    abort();
}

bool FullSparseAbstractInterpretation::hasAbsValue(
    const ValVar*, const ICFGNode*) const
{
    assert(false && "FullSparseAbstractInterpretation::hasAbsValue not implemented");
    abort();
}

Set<const ICFGNode*> FullSparseAbstractInterpretation::getUseSitesOfObjVar(
    const ObjVar* obj, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    return svfg->getUseSitesOfObjVar(obj, node);
}

Set<const ICFGNode*> FullSparseAbstractInterpretation::getUseSitesOfValVar(
    const ValVar* var) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    return svfg->getUseSitesOfValVar(var);
}

const ICFGNode* FullSparseAbstractInterpretation::getDefSiteOfValVar(
    const ValVar* var) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    return svfg->getDefSiteOfValVar(var);
}

const ICFGNode* FullSparseAbstractInterpretation::getDefSiteOfObjVar(
    const ObjVar* obj, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    return svfg->getDefSiteOfObjVar(obj, node);
}

// =====================================================================
//  SemiSparseAbstractInterpretation — cycle helpers (sparse-shape)
// =====================================================================

void SemiSparseAbstractInterpretation::initFromPTA(AndersenWaveDiff*)
{
    preAnalysis->initCycleValVars();
}

AbstractState SemiSparseAbstractInterpretation::getFullCycleHeadState(
    const ICFGCycleWTO* cycle)
{
    // Start from the dense snapshot (ObjVars + any ValVars that happen to
    // be cached at cycle_head's trace entry).
    AbstractState snap = AbstractInterpretation::getFullCycleHeadState(cycle);

    const Set<const ValVar*>& valVars = preAnalysis->getCycleValVars(cycle);
    if (valVars.empty())
        return snap;  // no cycle ValVars known: nothing to pull

    // Drop stale ValVar entries and pull each cycle ValVar from its
    // def-site.  ValVars without a genuine stored value are skipped to
    // avoid getAbsValue's top-fallback contaminating body def-sites on
    // the subsequent widen/narrow scatter.
    snap.clearValVars();
    for (const ValVar* v : valVars)
    {
        const ICFGNode* defSite = v->getICFGNode();
        if (!defSite || !hasAbsValue(v, defSite))
            continue;
        snap[v->getId()] = getAbsValue(v, defSite);
    }
    return snap;
}

bool SemiSparseAbstractInterpretation::widenCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    // Base widens, writes trace[cycle_head], and returns fixpoint bool.
    bool fixpoint = AbstractInterpretation::widenCycleState(prev, cur, cycle);

    // Scatter the widened ValVars back to their def-sites so body nodes
    // observe the widened values on the next iteration.  Matches the
    // pre-refactor semantics: scatter unconditionally, including at
    // widening fixpoint (see the narrowing-starts-with-stale-body issue
    // fixed by always writing widened state back).
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    const AbstractState& next = abstractTrace[cycle_head];
    for (const auto& [id, val] : next.getVarToVal())
        updateAbsValue(svfir->getSVFVar(id), val, cycle_head);
    return fixpoint;
}

bool SemiSparseAbstractInterpretation::narrowCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    // Delegate to base.  It returns true on the two non-scatter cases
    // (narrowing disabled, or narrow fixpoint); we preserve the original
    // "skip scatter at fixpoint" semantics by bailing early here.
    bool fixpoint = AbstractInterpretation::narrowCycleState(prev, cur, cycle);
    if (fixpoint)
        return true;

    // Non-fixpoint: base wrote the narrowed state to trace.  Scatter the
    // narrowed ValVars back to def-sites.
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    const AbstractState& next = abstractTrace[cycle_head];
    for (const auto& [id, val] : next.getVarToVal())
        updateAbsValue(svfir->getSVFVar(id), val, cycle_head);
    return false;
}
