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
#include "Graphs/SparseObjGraph.h"
#include "SVFIR/SVFIR.h"
#include "Graphs/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"

#include <cstdlib>
#include <optional>

using namespace SVF;

/// Compare one value a node reads with the value it read at its last
/// visit (index i), or record it when save is set. A null value is a value
/// that is not there (no state, or no entry).
static bool matchValue(std::vector<std::optional<AbstractValue>>& last,
                       u32_t& i, bool save, const AbstractValue* value)
{
    if (save)
    {
        last.push_back(value ? std::optional<AbstractValue>(*value) : std::nullopt);
        return true;
    }
    if (i >= last.size())
        return false;
    const std::optional<AbstractValue>& old = last[i++];
    return value ? old && old->equals(*value) : !old;
}

/// The freed addresses of a predecessor are compared like a value.
static bool matchFreed(std::vector<std::optional<AbstractValue>>& last,
                       u32_t& i, bool save, const AbstractState* pred)
{
    if (save)
    {
        last.push_back(pred ? std::optional<AbstractValue>(
                           AddressValue(pred->getFreedAddrs())) : std::nullopt);
        return true;
    }
    if (i >= last.size())
        return false;
    const std::optional<AbstractValue>& old = last[i++];
    return pred ? old && old->addrs.getVals() == pred->getFreedAddrs() : !old;
}

// FullSparse Obj inputs follow prebuilt routes. Shared SSA def-site access
// and WTO gather/scatter retain their existing implementations.

// =====================================================================
//  Full-sparse — class lifecycle + SVFG construction.
// =====================================================================

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation() = default;

void FullSparseAbstractInterpretation::buildSVFG()
{
    // AE reads Obj values only through the route graph, which copies what it
    // needs from the SVFG. The builder, and with it the SVFG and its MemSSA,
    // is destroyed on return.
    SVFGBuilder svfgBuilder(true);
    svfgBuilder.buildFullSVFG(preAnalysis->getPointerAnalysis());
    objRoutes = std::make_unique<SparseObjGraph>(
                    *svfgBuilder.getSVFG(), *svfir->getICFG(),
                    *preAnalysis->getPointerAnalysis(), *svfir);
}

void FullSparseAbstractInterpretation::mergeFreedAddrs(AbstractState& dst,
        const AbstractState& src)
{
    for (NodeID addr : src.getFreedAddrs())
        dst.addToFreedAddrs(addr);
}

void FullSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src, const NodeBS& objs)
{
    for (NodeID obj : objs)
    {
        const u32_t addr = AbstractState::getVirtualMemAddress(obj);
        const auto value = src.getLocToVal().find(src.getIDFromAddr(addr));
        if (value != src.getLocToVal().end())
        {
            if (dst.getLocToVal().count(dst.getIDFromAddr(addr)))
                dst.load(addr).join_with(value->second);
            else
                dst.store(addr, value->second);
        }
    }
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    // Inputs unchanged since the last visit: merging and handling the node
    // again would reproduce its current state.
    if (canSkip(node))
    {
        // The overflow detector keeps the latest offset of each GEP Obj,
        // which another GEP may have overwritten since the last visit.
        runDetectors(node);
        return false;
    }
    // Record what this visit reads. A cycle head is recorded with its cycle.
    if (!isCycleHead(node))
        matchInputs(node, {node}, true);

    const auto& graphEdges = objRoutes->getInputs(node);
    AbstractState previous;
    bool hasPrevious = false;
    for (const SparseObjGraph::Edge* edge : graphEdges)
    {
        if (edge->getSrcNode()->getICFGNode() == node && hasAbsState(node))
        {
            previous = getAbsState(node);
            hasPrevious = true;
            break;
        }
    }

    // The ICFG merge determines reachability and propagates freed addresses.
    const bool reachable = AbstractInterpretation::mergeStatesFromPredecessors(node);
    if (reachable)
    {
        AbstractState merged = getAbsState(node);
        for (const SparseObjGraph::Edge* edge : graphEdges)
        {
            const SparseObjGraph::Node* sourceNode = edge->getSrcNode();
            const ICFGNode* source = sourceNode->getICFGNode();
            const AbstractState* sourceState = source == node
                                               ? (hasPrevious ? &previous : nullptr)
                                               : (hasAbsState(source)
                                                  ? &getAbsState(source) : nullptr);
            if (!sourceState)
                continue;

            const AbstractState* input = sourceState;
            AbstractState refined;
            if (sourceNode->branch)
            {
                refined = *sourceState;
                if (!isBranchEdgeFeasible(sourceNode->branch, refined) ||
                        !applyBranchRefinement(sourceNode->branch, refined))
                    continue;
                input = &refined;
            }
            for (NodeID base : edge->getBases())
                joinStates(merged, *input, svfir->getAllFieldsObjVars(
                               svfir->getBaseObject(base)));
        }
        updateAbsState(node, merged);
    }
    return reachable;
}

/// Add the vars that stmt reads. Its LHS is added too; matchInputs drops
/// the ValVars that the node itself defines.
static void addReadVars(const SVFStmt* stmt, OrderedSet<const SVFVar*>& vars)
{
    vars.insert(stmt->getSrcNode());
    vars.insert(stmt->getDstNode());
    if (const MultiOpndStmt* multi = SVFUtil::dyn_cast<MultiOpndStmt>(stmt))
        for (u32_t k = 0; k < multi->getOpVarNum(); ++k)
            vars.insert(multi->getOpVar(k));
    if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        for (const auto& offset : gep->getOffsetVarAndGepTypePairVec())
            vars.insert(offset.first);
    if (const BranchStmt* branch = SVFUtil::dyn_cast<BranchStmt>(stmt))
        vars.insert(branch->getCondition());
    if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        vars.insert(select->getCondition());
}

/// The statement that defines var, if any.
static const SVFStmt* getDefStmt(const SVFVar* var)
{
    return var->getInEdges().empty() ? nullptr : *var->getInEdges().begin();
}

bool FullSparseAbstractInterpretation::matchInputs(
    const ICFGNode* key, const std::vector<const ICFGNode*>& nodes, bool save)
{
    std::vector<std::optional<AbstractValue>>& last = preAbsTrace[key];
    if (save)
        last.clear();
    u32_t i = 0;
    // Only inputs whose source lies outside nodes are gathered.
    const Set<const ICFGNode*> inside(nodes.begin(), nodes.end());
    // Branch edges whose condition this visit checks or refines with.
    OrderedSet<const IntraCFGEdge*> branches;

    // (1) ICFG predecessors: reachability and freed addresses.
    for (const ICFGNode* node : nodes)
        for (const ICFGEdge* edge : node->getInEdges())
        {
            const ICFGNode* pred = edge->getSrcNode();
            const IntraCFGEdge* intra = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
            if (intra && intra->getCondition())
                branches.insert(intra);
            if (inside.count(pred))
                continue;
            if (!matchFreed(last, i, save, hasAbsState(pred) ? &getAbsState(pred) : nullptr))
                return false;
        }

    // (2) ObjGraph predecessors: the fields of the base Objs on each route.
    for (const ICFGNode* node : nodes)
        for (const SparseObjGraph::Edge* edge : objRoutes->getInputs(node))
        {
            const ICFGNode* source = edge->getSrcNode()->getICFGNode();
            if (edge->getSrcNode()->branch)
                branches.insert(edge->getSrcNode()->branch);
            if (inside.count(source))
                continue;
            const AbstractState* state = hasAbsState(source) ? &getAbsState(source) : nullptr;
            for (NodeID base : edge->getBases())
                for (NodeID obj : svfir->getAllFieldsObjVars(svfir->getBaseObject(base)))
                {
                    const AbstractValue* read = nullptr;
                    if (state)
                    {
                        const auto value = state->getLocToVal().find(state->getIDFromAddr(
                                               AbstractState::getVirtualMemAddress(obj)));
                        if (value != state->getLocToVal().end())
                            read = &value->second;
                    }
                    if (!matchValue(last, i, save, read))
                        return false;
                }
        }

    // (3) ValVars, read at their def-sites. They come from the SVFStmts:
    OrderedSet<const SVFVar*> vars;
    for (const ICFGNode* node : nodes)
    {
        //   (a) the statements of node;
        for (const SVFStmt* stmt : node->getSVFStmts())
            addReadVars(stmt, vars);
        //   (b) the actual parameters of a call, which an external call reads
        //       without a CallPE;
        if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
            for (const ValVar* actual : call->getActualParms())
                vars.insert(actual);
    }
    //   (c) for each branch edge of (1) and (2), the statements that define
    //       its condition: the comparison, and the copy and load behind each
    //       of its operands.
    for (const IntraCFGEdge* edge : branches)
    {
        vars.insert(edge->getCondition());
        const SVFStmt* cmp = getDefStmt(edge->getCondition());
        if (!cmp || !SVFUtil::isa<CmpStmt>(cmp))
            continue;
        addReadVars(cmp, vars);
        for (const SVFVar* operand : {SVFUtil::cast<CmpStmt>(cmp)->getOpVar(0),
                                      SVFUtil::cast<CmpStmt>(cmp)->getOpVar(1)})
        {
            const SVFStmt* def = getDefStmt(operand);
            if (def && SVFUtil::isa<CopyStmt>(def))
            {
                addReadVars(def, vars);
                def = getDefStmt(def->getSrcNode());
            }
            if (def && SVFUtil::isa<LoadStmt>(def))
                addReadVars(def, vars);
        }
    }
    for (const SVFVar* var : vars)
    {
        const ValVar* value = var ? SVFUtil::dyn_cast<ValVar>(var) : nullptr;
        // A ValVar defined inside nodes is their output, not their input.
        if (!value || inside.count(getICFGNode(value)))
            continue;
        const ICFGNode* def = getICFGNode(value);
        const AbstractValue* read = nullptr;
        if (hasAbsState(def))
        {
            const auto it = getAbsState(def).getVarToVal().find(value->getId());
            if (it != getAbsState(def).getVarToVal().end())
                read = &it->second;
        }
        if (!matchValue(last, i, save, read))
            return false;
    }
    return save || i == last.size();
}

/// The nodes of cycle and its nested cycles, in WTO order.
static void collectCycleNodes(const ICFGCycleWTO* cycle,
                              std::vector<const ICFGNode*>& nodes)
{
    nodes.push_back(cycle->head()->getICFGNode());
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGCycleWTO* sub = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            collectCycleNodes(sub, nodes);
        else
            nodes.push_back(SVFUtil::cast<ICFGSingletonWTO>(comp)->getICFGNode());
    }
}

bool FullSparseAbstractInterpretation::isCycleHead(const ICFGNode* node) const
{
    const auto wto = preAnalysis->getFuncToWTO().find(node->getFun());
    return wto != preAnalysis->getFuncToWTO().end() && wto->second->isHead(node);
}

bool FullSparseAbstractInterpretation::canSkip(const ICFGNode* node)
{
    if (!Options::AEReuse())
        return false;
    const auto running = runningCycles.find(node->getFun());
    if (running != runningCycles.end() && running->second > 0)
        return false;
    if (SVFUtil::isa<RetICFGNode>(node))
        return false;
    const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node);
    if (call && !SVFUtil::isExtCall(call->getCalledFunction()))
        return false;
    if (isCycleHead(node))
        return false;
    return preAbsTrace.count(node) && matchInputs(node, {node}, false);
}

void FullSparseAbstractInterpretation::handleLoopOrRecursion(
    const ICFGCycleWTO* cycle, const CallICFGNode* caller)
{
    const ICFGNode* head = cycle->head()->getICFGNode();
    std::vector<const ICFGNode*> nodes;
    collectCycleNodes(cycle, nodes);
    // Gather the inputs of the cycle from outside it, at its entry.
    if (Options::AEReuse() && preAbsTrace.count(head) &&
            matchInputs(head, nodes, false))
    {
        // Restore the states of its last run, which an enclosing cycle may
        // have overwritten since.
        for (const auto& output : cycleOutputs[head])
            abstractTrace[output.first] = output.second;
        for (const ICFGNode* node : nodes)
            if (hasAbsState(node))
                runDetectors(node);
        return;
    }
    ++runningCycles[head->getFun()];
    SemiSparseAbstractInterpretation::handleLoopOrRecursion(cycle, caller);
    --runningCycles[head->getFun()];
    // Recorded after the run: an input from a callee of the cycle is the
    // value the run left there.
    matchInputs(head, nodes, true);
    std::vector<std::pair<const ICFGNode*, AbstractState>>& outputs = cycleOutputs[head];
    outputs.clear();
    for (const ICFGNode* node : nodes)
        if (hasAbsState(node))
            outputs.emplace_back(node, getAbsState(node));
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

// =====================================================================
//  Shared SemiSparse/FullSparse state-access overrides. Both use the
//  established ValVar def-site reads and writes below.
// =====================================================================

void SemiSparseAbstractInterpretation::updateAbsState(
    const ICFGNode* node, const AbstractState& state)
{
    // Only replace ObjVar state.  ValVars live at their def-sites and
    // must not be overwritten when the predecessor's state is merged in.
    abstractTrace[node].updateAddrStateOnly(state);
}

void SemiSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src)
{
    // ValVars live at def-sites in semi-sparse mode; they don't flow
    // through state merges.  Iterate src's ObjVar (_addrToAbsVal) entries
    // directly and join into dst, leaving dst's ValVar map untouched.
    // _freedAddrs (used by the null-deref detector) also rides along
    // ICFG edges — there is no SVFG-level encoding of free events.
    for (const auto& [id, val] : src.getLocToVal())
    {
        u32_t addr = AbstractState::getVirtualMemAddress(id);
        if (dst.getLocToVal().count(id))
            dst.load(addr).join_with(val);
        else
            dst.store(addr, val);
    }
    for (NodeID a : src.getFreedAddrs())
        dst.addToFreedAddrs(a);
}

const ICFGNode* SemiSparseAbstractInterpretation::getICFGNode(
    const ValVar* var) const
{
    // const ValVars are all defined in global node
    if (!var->getICFGNode())
    {
        return svfir->getICFG()->getGlobalICFGNode();
    }
    // for return value of callsite, use the ret-site as def-site
    else if (SVFUtil::isa<CallICFGNode>(var->getICFGNode()) &&
             SVFUtil::isa<RetValPN>(var))
    {
        return SVFUtil::dyn_cast<CallICFGNode>(var->getICFGNode())
               ->getRetICFGNode();
    }
    // for other ValVars, use their def-site as the node to query abstract
    // value.
    else
    {
        return var->getICFGNode();
    }
}

void SemiSparseAbstractInterpretation::updateAbsValue(const ValVar* var,
        const AbstractValue& val,
        const ICFGNode* node)
{
    // Write to the var's def-site so getAbsValue stays consistent.
    const ICFGNode* defNode = var->getICFGNode();
    abstractTrace[defNode ? defNode : node][var->getId()] = val;
}

const AbstractValue& SemiSparseAbstractInterpretation::getAbsValue(
    const ValVar* var, const ICFGNode* node)
{
    // Read from the var's def-site (where updateAbsValue wrote it).
    return AbstractInterpretation::getAbsValue(var, getICFGNode(var));
}

bool SemiSparseAbstractInterpretation::hasAbsValue(const ValVar* var,
        const ICFGNode* node) const
{
    return AbstractInterpretation::hasAbsValue(var, getICFGNode(var));
}
