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

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation()
{
    delete svfg;
}

void FullSparseAbstractInterpretation::buildSVFG()
{
    SVFGBuilder memSSA(true);
    svfg = memSSA.buildFullSVFG(preAnalysis->getPointerAnalysis());
}

// =====================================================================
//  Full-sparse — state-access overrides.
//
//  ValVar reads are stubbed until SVFG-backed resolution lands;
//  def/use queries route through the SVFG.
// =====================================================================

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
