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
#include "AE/Svfexe/AEWTO.h"
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"

using namespace SVF;

// SemiSparse state-access overrides (get/has/updateAbsValue,
// updateAbsState, joinStates) live in AbstractStateManager.cpp; the
// FullSparse-specific overrides — including the SVFG-backed def/use
// queries and the ValVar stubs — live below alongside the rest of
// FullSparse so the whole subclass stays in one file.

// =====================================================================
//  Full-sparse — class lifecycle + SVFG construction.
// =====================================================================

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation() = default;

void FullSparseAbstractInterpretation::buildSVFG()
{
    svfgBuilder = std::make_unique<SVFGBuilder>(true);
    svfg = svfgBuilder->buildFullSVFG(preAnalysis->getPointerAnalysis());
}

// =====================================================================
//  Full-sparse — Phase 1 state-access overrides.
//
//  ValVar reads route to the SVFG-reaching def-site; ObjVar reads walk
//  indirect SVFG in-edges (with MSSAPHI passthrough) to collect
//  reaching defs and join their trace entries.  Writes still go to
//  `node`'s trace via the inherited base implementation; that's the
//  def-site for AddrStmt / StoreStmt under the dense write semantics
//  Phase 1 preserves.
// =====================================================================

const AbstractValue& FullSparseAbstractInterpretation::getAbsValue(
    const ValVar* var, const ICFGNode* node)
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    // ValVars without an SVFG def-site (constants, function-entry
    // parameters before any def) fall back to the semi-sparse path:
    // their declaring ICFGNode is the canonical home.
    if (!svfg->hasDefSVFGNode(var))
        return SemiSparseAbstractInterpretation::getAbsValue(var, node);
    const SVFGNode* defNode = svfg->getDefSiteOfValVar(var);
    return AbstractInterpretation::getAbsValue(var, defNode->getICFGNode());
}

bool FullSparseAbstractInterpretation::hasAbsValue(
    const ValVar* var, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    if (!svfg->hasDefSVFGNode(var))
        return SemiSparseAbstractInterpretation::hasAbsValue(var, node);
    const SVFGNode* defNode = svfg->getDefSiteOfValVar(var);
    return AbstractInterpretation::hasAbsValue(var, defNode->getICFGNode());
}

const AbstractValue& FullSparseAbstractInterpretation::getAbsValue(
    const ObjVar* var, const ICFGNode* node)
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    // SVFG::getDefSiteOfObjVar already walks through MSSAPHI and the
    // four inter-procedural relay nodes, so the returned set is a flat
    // collection of StoreSVFGNodes.  Join their trace entries for `var`.
    AbstractValue meet;  // bottom
    for (const VFGNode* vNode : node->getVFGNodes())
    {
        const SVFGNode* svfgVNode = SVFUtil::dyn_cast<SVFGNode>(vNode);
        if (!svfgVNode)
            continue;
        for (const SVFGNode* d : svfg->getDefSiteOfObjVar(var, svfgVNode))
        {
            const ICFGNode* defICFG = d->getICFGNode();
            if (defICFG && AbstractInterpretation::hasAbsValue(var, defICFG))
                meet.join_with(AbstractInterpretation::getAbsValue(var, defICFG));
        }
    }
    _objReadBuf = meet;
    return _objReadBuf;
}

bool FullSparseAbstractInterpretation::hasAbsValue(
    const ObjVar* var, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    for (const VFGNode* vNode : node->getVFGNodes())
    {
        const SVFGNode* svfgVNode = SVFUtil::dyn_cast<SVFGNode>(vNode);
        if (!svfgVNode)
            continue;
        for (const SVFGNode* d : svfg->getDefSiteOfObjVar(var, svfgVNode))
        {
            const ICFGNode* defICFG = d->getICFGNode();
            if (defICFG && AbstractInterpretation::hasAbsValue(var, defICFG))
                return true;
        }
    }
    return false;
}

const Set<const ICFGNode*> FullSparseAbstractInterpretation::getUseSitesOfValVar(
    const ValVar* var) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    Set<const ICFGNode*> useSites;
    for(const SVFGNode* svfgNode : svfg->getUseSitesOfValVar(var))
    {
        useSites.insert(svfgNode->getICFGNode());
    }
    return useSites;
}

const ICFGNode* FullSparseAbstractInterpretation::getDefSiteOfValVar(
    const ValVar* var) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    return svfg->getDefSiteOfValVar(var)->getICFGNode();
}

const Set<const ICFGNode*> FullSparseAbstractInterpretation::getDefSiteOfObjVar(
    const ObjVar* obj, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    Set<const ICFGNode*> defSites;
    for(auto* vNode : node->getVFGNodes())
    {
        for(const SVFGNode* svfgNode : svfg->getDefSiteOfObjVar(obj, vNode))
        {
            defSites.insert(svfgNode->getICFGNode());
        }
    }
    return defSites;
}

const Set<const ICFGNode*> FullSparseAbstractInterpretation::getUseSitesOfObjVar(
    const ObjVar* obj, const ICFGNode* node) const
{
    assert(svfg && "SVFG is not built for full-sparse AE");
    Set<const ICFGNode*> useSites;
    for(auto* vNode : node->getVFGNodes())
    {
        for(const SVFGNode* svfgNode : svfg->getUseSitesOfObjVar(obj, vNode))
        {
            useSites.insert(svfgNode->getICFGNode());
        }
    }
    return useSites;
}
// =====================================================================
//  Semi-sparse cycle helpers (sparse-shape gather / scatter).
// =====================================================================

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
