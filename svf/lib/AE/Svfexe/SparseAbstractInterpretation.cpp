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

using namespace SVF;

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

FullSparseAbstractInterpretation::FunctionInputSignature
FullSparseAbstractInterpretation::collectFunctionInputs(
    const ICFGNode* funEntry)
{
    FunctionInputSignature signature;

    // Match updateStateOnCall: each CallPE summarizes the actual scalar values
    // that define one formal parameter at this context-insensitive entry.
    for (const SVFStmt* stmt : funEntry->getSVFStmts())
    {
        if (const auto* call = SVFUtil::dyn_cast<CallPE>(stmt))
        {
            AbstractValue value;
            for (u32_t i = 0; i < call->getOpVarNum(); ++i)
            {
                const ICFGNode* actualNode = call->getOpCallICFGNode(i);
                if (hasAbsState(actualNode))
                    value.join_with(getAbsValue(call->getOpVar(i), actualNode));
            }
            signature.scalarAndFreedInputs[call->getResID()] = value;
        }
    }

    // Freed-address reachability enters a function through ordinary ICFG
    // call edges rather than packed-Obj routes.
    for (const ICFGEdge* edge : funEntry->getInEdges())
    {
        const ICFGNode* source = edge->getSrcNode();
        if (hasAbsState(source))
            mergeFreedAddrs(signature.scalarAndFreedInputs,
                            getAbsState(source));
    }

    // Preserve route identity: two call sites may carry different values for
    // the same packed base Obj and must remain separate signature entries.
    for (const SparseObjGraph::Edge* edge :
            objRoutes->getFunctionInputs(funEntry->getFun()))
    {
        AbstractState input;
        const SparseObjGraph::Node* sourceNode = edge->getSrcNode();
        const ICFGNode* source = sourceNode->getICFGNode();
        if (hasAbsState(source))
            for (NodeID base : edge->getBases())
                joinStates(input, getAbsState(source),
                           svfir->getAllFieldsObjVars(
                               svfir->getBaseObject(base)));
        signature.objInputs.push_back(std::move(input));
    }
    return signature;
}

bool FullSparseAbstractInterpretation::buildReusableFunctionSubgraph(
    const FunObjVar* fun, Set<const FunObjVar*>& visited,
    std::vector<const FunObjVar*>& functions)
{
    // A recursive SCC is entered only through a call from outside the SCC;
    // its internal calls are skipped and its body is one interprocedural WTO.
    // When recursive reuse is enabled, the closure therefore contains every
    // SCC member and compares their inputs like any other callee.
    if (!allowRecursiveReuse() &&
            preAnalysis->getPointerAnalysis()->isInRecursion(fun))
        return false;
    if (!visited.insert(fun).second)
        return true;

    functions.push_back(fun);
    const CallGraphNode* node =
        preAnalysis->getCallGraph()->getCallGraphNode(fun);
    for (const CallGraphEdge* edge : node->getOutEdges())
    {
        const FunObjVar* callee = edge->getDstNode()->getFunction();
        if (!callee->isDeclaration() &&
                !buildReusableFunctionSubgraph(callee, visited, functions))
            return false;
    }
    return true;
}

const std::vector<const FunObjVar*>*
FullSparseAbstractInterpretation::getReusableFunctionSubgraph(
    const FunObjVar* fun)
{
    if (nonReusableFunctions.count(fun))
        return nullptr;

    const auto existing = reusableFunctionSubgraphs.find(fun);
    if (existing != reusableFunctionSubgraphs.end())
        return &existing->second;

    Set<const FunObjVar*> visited;
    std::vector<const FunObjVar*> functions;
    if (!buildReusableFunctionSubgraph(fun, visited, functions))
    {
        nonReusableFunctions.insert(fun);
        return nullptr;
    }
    return &reusableFunctionSubgraphs.emplace(fun, std::move(functions))
                .first->second;
}

FullSparseAbstractInterpretation::FunctionSubgraphSignature
FullSparseAbstractInterpretation::collectFunctionSubgraphInputs(
    const std::vector<const FunObjVar*>& functions)
{
    FunctionSubgraphSignature signature;
    signature.reserve(functions.size());
    for (const FunObjVar* dependency : functions)
        signature.push_back(functionInputVersion(dependency));
    return signature;
}

u64_t FullSparseAbstractInterpretation::functionInputVersion(
    const FunObjVar* fun)
{
    // Equal versions imply equal inputs: the version changes whenever a newly
    // collected signature differs from the stored one. A->B->A yields a new
    // version, which only costs a reuse miss.
    FunctionInputSignature current =
        collectFunctionInputs(svfir->getICFG()->getFunEntryICFGNode(fun));
    auto it = latestFunctionInputs.find(fun);
    if (it == latestFunctionInputs.end())
    {
        latestFunctionInputs.emplace(fun, std::make_pair(std::move(current), u64_t(1)));
        return 1;
    }
    if (!(it->second.first == current))
    {
        it->second.first = std::move(current);
        ++it->second.second;
    }
    return it->second.second;
}

bool FullSparseAbstractInterpretation::reuseFunctionResult(
    const ICFGNode* funEntry)
{
    if (!Options::AEReuse())
        return false;

    const FunObjVar* fun = funEntry->getFun();
    const std::vector<const FunObjVar*>* subgraph =
        getReusableFunctionSubgraph(fun);
    if (!subgraph)
        return false;

    // Checking only fun's direct inputs is insufficient: wrapper() may have
    // no Obj input itself while a transitive callee reads a changed global.
    const auto previous = lastFunctionInputs.find(fun);
    bool stable = false;
    if (previous != lastFunctionInputs.end() && completedFunctions.count(fun))
    {
        stable = previous->second == collectFunctionSubgraphInputs(*subgraph);
    }
    if (stable)
        return true;

    // The next complete analysis replaces the cached signature in
    // recordFunctionResult. Until then this result is not reusable.
    completedFunctions.erase(fun);
    return false;
}

void FullSparseAbstractInterpretation::recordFunctionResult(
    const ICFGNode* funEntry)
{
    if (!Options::AEReuse())
        return;

    const FunObjVar* fun = funEntry->getFun();
    if (const std::vector<const FunObjVar*>* subgraph =
            getReusableFunctionSubgraph(fun))
    {
        // Record after the body completes so internal call-site inputs and
        // the result describe the same fixpoint, not the pre-analysis state.
        lastFunctionInputs[fun] = collectFunctionSubgraphInputs(*subgraph);
        completedFunctions.insert(fun);
    }
}

bool FullSparseAbstractInterpretation::allowRecursiveReuse() const
{
    // TOP recursion handling writes the caller's state while skipping the
    // SCC, so a skipped SCC would lose that effect.
    static const bool enabled = Options::AEReuse() &&
                                Options::HandleRecur() != TOP;
    return enabled;
}

const FullSparseAbstractInterpretation::CycleRegion*
FullSparseAbstractInterpretation::getCycleRegion(const ICFGCycleWTO* cycle)
{
    if (nonReusableCycles.count(cycle))
        return nullptr;
    const auto existing = cycleRegions.find(cycle);
    if (existing != cycleRegions.end())
        return &existing->second;

    // Loops of a recursive function lie inside the SCC's interprocedural WTO.
    // They are covered by SCC-level function reuse, not by cycle reuse.
    const FunObjVar* fun = cycle->head()->getICFGNode()->getFun();
    if (!fun || preAnalysis->getPointerAnalysis()->isInRecursion(fun))
    {
        nonReusableCycles.insert(cycle);
        return nullptr;
    }

    CycleRegion region;
    collectCycleNodes(cycle, region.nodes);
    if (!collectCycleCallees(region) || region.closure.count(fun))
    {
        nonReusableCycles.insert(cycle);
        return nullptr;
    }
    collectCycleExternalInputs(region);
    return &cycleRegions.emplace(cycle, std::move(region)).first->second;
}

void FullSparseAbstractInterpretation::collectCycleNodes(
    const ICFGCycleWTO* cycle, Set<const ICFGNode*>& nodes) const
{
    nodes.insert(cycle->head()->getICFGNode());
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton =
                    SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            nodes.insert(singleton->getICFGNode());
        else if (const ICFGCycleWTO* subCycle =
                     SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            collectCycleNodes(subCycle, nodes);
    }
}

bool FullSparseAbstractInterpretation::collectCycleCallees(CycleRegion& region)
{
    // The call closure is analyzed from inside the cycle, so its nodes belong
    // to the region. Its own boundary inputs are compared per function.
    for (const ICFGNode* node : region.nodes)
    {
        const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node);
        if (!call || SVFUtil::isExtCall(call->getCalledFunction()))
            continue;

        std::vector<const FunObjVar*> callees;
        if (const FunObjVar* callee = call->getCalledFunction())
            callees.push_back(callee);
        else if (preAnalysis->getCallGraph()->hasIndCSCallees(call))
            for (const FunObjVar* callee :
                    preAnalysis->getCallGraph()->getIndCSCallees(call))
                callees.push_back(callee);

        for (const FunObjVar* callee : callees)
        {
            if (callee->isDeclaration())
                continue;
            if (!buildReusableFunctionSubgraph(callee, region.closure,
                                               region.functions))
                return false;
        }
    }
    return true;
}

void FullSparseAbstractInterpretation::addExternalValVar(
    const SVFVar* var, CycleRegion& region, Set<const ValVar*>& visited)
{
    // A ValVar is an external input when it is read in the cycle and its
    // def-site lies outside the region. Constants at the global node never
    // change after initialization and are omitted.
    const ValVar* value = var ? SVFUtil::dyn_cast<ValVar>(var) : nullptr;
    if (!value || !visited.insert(value).second)
        return;
    const ICFGNode* defSite = getICFGNode(value);
    if (defSite != svfir->getICFG()->getGlobalICFGNode() &&
            !region.contains(defSite))
        region.externalValVars.push_back(value);
}

void FullSparseAbstractInterpretation::collectCycleExternalInputs(
    CycleRegion& region)
{
    Set<const ValVar*> visited;
    for (const ICFGNode* node : region.nodes)
    {
        for (const SVFStmt* stmt : node->getSVFStmts())
        {
            addExternalValVar(stmt->getSrcNode(), region, visited);
            addExternalValVar(stmt->getDstNode(), region, visited);
            if (const MultiOpndStmt* multi = SVFUtil::dyn_cast<MultiOpndStmt>(stmt))
                for (u32_t i = 0; i < multi->getOpVarNum(); ++i)
                    addExternalValVar(multi->getOpVar(i), region, visited);
            if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
                for (const auto& offset : gep->getOffsetVarAndGepTypePairVec())
                    addExternalValVar(offset.first, region, visited);
            if (const BranchStmt* branch = SVFUtil::dyn_cast<BranchStmt>(stmt))
                addExternalValVar(branch->getCondition(), region, visited);
            if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
                addExternalValVar(select->getCondition(), region, visited);
        }
        if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
            for (const ValVar* actual : call->getActualParms())
                addExternalValVar(actual, region, visited);
        if (const RetICFGNode* ret = SVFUtil::dyn_cast<RetICFGNode>(node))
            addExternalValVar(ret->getActualRet(), region, visited);

        for (const ICFGEdge* edge : node->getInEdges())
        {
            if (const IntraCFGEdge* intra = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
                addExternalValVar(intra->getCondition(), region, visited);
            if (!region.contains(edge->getSrcNode()))
                region.externalICFGEdges.push_back(edge);
        }

        for (const SparseObjGraph::Edge* edge : objRoutes->getInputs(node))
        {
            const SparseObjGraph::Node* source = edge->getSrcNode();
            if (source->branch)
                addExternalValVar(source->branch->getCondition(), region, visited);
            if (!region.contains(source->getICFGNode()))
                region.externalObjRoutes.push_back(edge);
        }
    }
}

FullSparseAbstractInterpretation::CycleInputSignature
FullSparseAbstractInterpretation::collectCycleInputs(
    const ICFGCycleWTO* cycle, const CycleRegion& region)
{
    CycleInputSignature signature;
    signature.functions = collectFunctionSubgraphInputs(region.functions);

    for (const ValVar* var : region.externalValVars)
    {
        const ICFGNode* defSite = getICFGNode(var);
        if (hasAbsValue(var, defSite))
            signature.scalars[var->getId()] = getAbsValue(var, defSite);
    }

    // An enclosing cycle's widening and narrowing scatter values to the
    // def-sites of this cycle's ValVars without running this cycle.
    for (const ValVar* var : preAnalysis->getCycleValVars(cycle))
    {
        const ICFGNode* defSite = getICFGNode(var);
        if (hasAbsValue(var, defSite))
            signature.cycleValVars[var->getId()] = getAbsValue(var, defSite);
    }

    for (const SparseObjGraph::Edge* edge : region.externalObjRoutes)
    {
        AbstractState input;
        const SparseObjGraph::Node* sourceNode = edge->getSrcNode();
        const ICFGNode* source = sourceNode->getICFGNode();
        if (hasAbsState(source))
            for (NodeID base : edge->getBases())
                joinStates(input, getAbsState(source),
                           svfir->getAllFieldsObjVars(
                               svfir->getBaseObject(base)));
        signature.objInputs.push_back(std::move(input));
    }

    // Reproduce the ICFG merge decision. A branch condition may depend on
    // loaded Obj values, so feasibility is recorded rather than inferred.
    for (const ICFGEdge* edge : region.externalICFGEdges)
    {
        const ICFGNode* source = edge->getSrcNode();
        bool feasible = false;
        if (hasAbsState(source))
        {
            const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
            if (intra && intra->getCondition())
            {
                AbstractState state = getAbsState(source);
                feasible = isBranchEdgeFeasible(intra, state);
                if (feasible)
                    applyBranchRefinement(intra, state);
                if (feasible)
                    mergeFreedAddrs(signature.freed, state);
            }
            else
            {
                feasible = true;
                mergeFreedAddrs(signature.freed, getAbsState(source));
            }
        }
        signature.feasibleEdges.push_back(feasible);
    }
    return signature;
}

bool FullSparseAbstractInterpretation::reuseCycleResult(
    const ICFGCycleWTO* cycle)
{
    if (!Options::AEReuse())
        return false;

    const CycleRegion* region = getCycleRegion(cycle);
    if (!region)
        return false;

    const auto previous = lastCycleInputs.find(cycle);
    bool stable = false;
    if (previous != lastCycleInputs.end() && completedCycles.count(cycle))
    {
        stable = previous->second == collectCycleInputs(cycle, *region);
    }
    if (stable)
        return true;
    completedCycles.erase(cycle);
    return false;
}

void FullSparseAbstractInterpretation::recordCycleResult(
    const ICFGCycleWTO* cycle)
{
    if (!Options::AEReuse())
        return;
    if (const CycleRegion* region = getCycleRegion(cycle))
    {
        lastCycleInputs[cycle] = collectCycleInputs(cycle, *region);
        completedCycles.insert(cycle);
    }
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
