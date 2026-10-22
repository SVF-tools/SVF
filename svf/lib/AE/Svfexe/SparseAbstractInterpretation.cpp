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

// FullSparse input assembly is kept together below. Shared SSA def-site
// access and WTO gather/scatter retain their existing implementations.

// =====================================================================
//  Full-sparse — class lifecycle + SVFG construction.
// =====================================================================

FullSparseAbstractInterpretation::~FullSparseAbstractInterpretation() = default;

void FullSparseAbstractInterpretation::buildSVFG()
{
    svfgBuilder = std::make_unique<SVFGBuilder>(true);
    svfgBuilder->buildFullSVFG(preAnalysis->getPointerAnalysis());
}

// FullSparse keeps the established GEP/freed-address compatibility path.
// Base Obj values and top-level inputs are assembled from direct SVFG edges.
void FullSparseAbstractInterpretation::joinStates(AbstractState& dst,
        const AbstractState& src)
{
    for (const auto& [id, val] : src.getLocToVal())
    {
        if (SVFUtil::isa<GepObjVar>(svfir->getSVFVar(id)))
        {
            const u32_t addr = AbstractState::getVirtualMemAddress(id);
            if (dst.getLocToVal().count(id))
                dst.load(addr).join_with(val);
            else
                dst.store(addr, val);
        }
    }
    for (NodeID addr : src.getFreedAddrs())
        dst.addToFreedAddrs(addr);
}

bool FullSparseAbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    // An empty SVFG input is not proof of unreachability. Reuse the existing
    // control gate and GEP/freed-address compatibility merge, not a new solver.
    const bool reachable =
        AbstractInterpretation::mergeStatesFromPredecessors(node);
    if (reachable)
    {
        AbstractState merged = getAbsState(node);
        for (const VFGNode* target : node->getVFGNodes())
        {
            for (auto it = target->InEdgeBegin(); it != target->InEdgeEnd(); ++it)
            {
                const VFGEdge* edge = *it;
                const ICFGNode* source = edge->getSrcNode()->getICFGNode();
                if (source == node)
                {
                    // Dependencies inside one ICFG node are handled by its
                    // statement execution, not by reading its old output.
                }
                else if (!source || !hasAbsState(source))
                {
                    // This direct predecessor has no available output yet.
                }
                else
                {
                    // Step 1. Read this direct and indirect predecessor's current edge values.
                    AbstractState input = collectValueFlowInput(edge);
                    // Step 2. JOIN the contributions of feasible, refined paths.
                    input = filterValueFlowInputByPath(edge, input);
                    // Step 3. Merge this contribution, never the whole source state.
                    for (const auto& [id, value] : input.getVarToVal())
                        merged[id].join_with(value);
                    for (const auto& [obj, value] : input.getLocToVal())
                    {
                        const u32_t addr = AbstractState::getVirtualMemAddress(obj);
                        if (merged.getLocToVal().count(obj))
                            merged.load(addr).join_with(value);
                        else
                            merged.store(addr, value);
                    }
                }
            }
        }
        updateAbsState(node, merged);
        // The shared semi-sparse setter replaces Obj state only. Copy
        // the collected SSA inputs here without changing their canonical
        // def-site reads or the WTO gather/scatter convention.
        for (const auto& [id, value] : merged.getVarToVal())
            getAbsState(node)[id] = value;
    }
    return reachable;
}

// Step 1 helper function - collect predecessors' local vars including top-level and address-taken
AbstractState FullSparseAbstractInterpretation::collectValueFlowInput(
    const VFGEdge* edge)
{
    AbstractState input;
    const VFGNode* source = edge->getSrcNode();
    const ICFGNode* sourceICFG = source->getICFGNode();
    const auto* indirect = SVFUtil::dyn_cast<IndirectSVFGEdge>(edge);
    if (indirect)
    {
        // indirect edge (address-taken vars)
        for (NodeID id : indirect->getPointsTo())
        {
            const BaseObjVar* base = svfir->getBaseObject(id);
            for (NodeID obj : svfir->getAllFieldsObjVars(base))
            {
                const auto* var = svfir->getObjVar(obj);
                if (AbstractInterpretation::hasAbsValue(var, sourceICFG))
                    input.store(AbstractState::getVirtualMemAddress(obj),
                                AbstractInterpretation::getAbsValue(var, sourceICFG));
            }
        }
    }
    else
    {
        // direct edge (top-level vars)
        for (NodeID id : source->getDefSVFVars())
        {
            const auto* var = SVFUtil::dyn_cast<ValVar>(svfir->getSVFVar(id));
            if (var && hasAbsValue(var, sourceICFG))
                input[id] = getAbsValue(var, sourceICFG);
        }
    }
    return input;
}

// Step 2: filter one SVFG input D -> N along ICFG paths.
// D/N label the source/target ICFG nodes; B labels a branch node.
// Snippets below are simplified C. Interval annotations describe the already
// computed AE values, not assignments executed by this search. For narrowing
// examples, x is a memory Obj loaded through a pointer with one known target.
AbstractState FullSparseAbstractInterpretation::filterValueFlowInputByPath(
    const VFGEdge* edge, const AbstractState& input)
{
    const ICFGNode* source = edge->getSrcNode()->getICFGNode();
    const ICFGNode* target = edge->getDstNode()->getICFGNode();
    NodeBS objs;
    if (const auto* indirect = SVFUtil::dyn_cast<IndirectSVFGEdge>(edge))
    {
        for (NodeID obj : indirect->getPointsTo())
            objs |= svfir->getAllFieldsObjVars(svfir->getBaseObject(obj));
    }
    AbstractState result;
    if (input.getVarToVal().empty() && input.getLocToVal().empty() && objs.empty())
    {
        // No values or queried Objs. An indirect edge still identifies its
        // base/sub Objs even when D has not produced their values yet.
    }
    else if (!edge->isIntraVFGEdge() || source->getFun() != target->getFun() ||
             !source->getFun())
    {
        // Call input:
        //   f(&x);                      // D: ActualIn in caller
        //   void f(int *p) { ... }      // N: FormalIn at callee entry
        // Call output:
        //   void f(int *p) { *p = 7; }  // D: FormalOut at callee exit
        //   f(&x);                     // N: ActualOut at caller return point
        // Recursive input (same function, but a non-intra SVFG edge):
        //   void f(int *p) { f(p); }    // D: call ActualIn -> N: entry FormalIn
        // Global initialization:
        //   int g = 7;                 // D: global node, no owning function
        //   void f() { use(g); }       // N
        // All retain D's input; no cross-procedural feasibility is proved.
        result = input;
    }
    else
    {
        // int x = input();   // D: x=[0,10]
        // ...
        // if (x > 5) {       // B
        //     ...
        //     use(x);        // N
        // }
        // Same function: search D -> B -> ... -> N, carrying x=[6,10]
        // on B's true edge. More branches give D -> B1 -> ... -> Bk -> N.
        // incoming is temporary to this query; no statements are re-executed.
        Map<const ICFGNode*, AbstractState> incoming;
        FIFOWorkList<const ICFGNode*> worklist;
        incoming[source] = input;
        worklist.push(source);
        while (!worklist.empty())
        {
            const ICFGNode* current = worklist.pop();
            if (current == target)
            {
                // int x = read_int();  // D: x=[-10,10]
                // if (x > 0) {        // B: true edge carries x=[1,10]
                //     use(x);         // N: current == target
                // }
                // JOIN incoming[N] into this D -> N query's result. Stop at
                // N: do not inspect or follow any of N's outgoing edges.
                result.joinWith(incoming[current]);
            }
            else
            {
                // int a[2] = {0,1} // D: a[0] = [0,0], a[1] = [1,1]
                // a[1] = 2         // S: a[1] = [2,2]
                // use(a[1])        // N: if an input carries D's old a[1],
                // its D -> S -> N path reuses S's result when strongly updated.
                // An input from S already has a[1]=2; S itself is not replayed.

                AbstractState output = incoming[current];
                if (current != source && hasAbsState(current))
                {
                    const AbstractState& state = getAbsState(current);
                    for (const SVFStmt* stmt : current->getSVFStmts())
                    {
                        if (const auto* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
                        {
                            const AddressValue& targets =
                                getAbsValue(store->getLHSVar(), current).getAddrs();
                            const bool strongUpdate = isStrongUpdate(targets, state);
                            for (NodeID obj : objs)
                            {
                                const u32_t addr = AbstractState::getVirtualMemAddress(obj);
                                if (targets.contains(addr) &&
                                        state.getIDFromAddr(addr) == obj && state.getLocToVal().count(obj))
                                {
                                    // D: a[9]=9; p=a+9; S: *p=10; N: use(a[9]).
                                    // Static GEP mapping may leave D -> N, not S -> N.
                                    // At S, AE identifies the carried sub-Obj a[9].
                                    // Reuse S's computed value; do not execute S again.
                                    if (strongUpdate)
                                        output.store(addr, state.load(addr));
                                    else
                                        output.load(addr).join_with(state.load(addr));
                                }
                            }
                        }
                    }
                }
                std::vector<std::pair<const ICFGNode*, const IntraCFGEdge*>> successors;
                if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(current))
                {
                    if (current != source && SVFUtil::isExtCall(call->getCalledFunction()) &&
                            hasAbsState(current))
                    {
                        // D: x=0; C: a model writes x=5; N: use(x).
                        // Read the queried Objs from C's already computed
                        // output, including x=5 even when input contains x=0.
                        // This is a projection of C's output, not proof that
                        // C wrote every present Obj. A joined output may be
                        // less precise than this query's path-specific values.
                        const AbstractState& state = getAbsState(current);
                        for (NodeID obj : objs)
                        {
                            const u32_t addr = AbstractState::getVirtualMemAddress(obj);
                            if (state.getIDFromAddr(addr) == obj && state.getLocToVal().count(obj))
                            {
                                output.store(addr, state.load(addr));
                            }
                        }
                    }
                    // int x = input();   // D
                    // f();               // CallICFGNode
                    // ...                // matching RetICFGNode is in caller
                    // use(x);            // N
                    // Search: D -> call f -> caller return point -> ... -> N.
                    // Do not enter f. External calls reuse queried Obj outputs
                    // above; ordinary callee results come from their SVFG inputs.
                    // Whether f can return is not proved here (also conservative
                    // for abort()).
                    successors.emplace_back(call->getRetICFGNode(), nullptr);
                }
                else
                {
                    // int x = 1;         // D
                    // int y = 2;         // ordinary node: keep carried x=1
                    // x = 9;             // S: still keep D's carried x=1
                    // use(x);            // N: may also receive x=9 from S
                    // Follow intra edges only. Do not execute y=2 or x=9;
                    // do not infer that S kills D's contribution.
                    for (const ICFGEdge* out : current->getOutEdges())
                    {
                        if (const auto* intra = SVFUtil::dyn_cast<IntraCFGEdge>(out))
                            successors.emplace_back(intra->getDstNode(), intra);
                    }
                }

                for (const auto& [next, branch] : successors)
                {
                    AbstractState candidate = output;
                    bool feasible = true;
                    if (!next || next->getFun() != source->getFun())
                    {
                        // Structural boundary (not a C-level branch test):
                        //   D -> call f -> [missing caller return point]
                        //   D[in f] -> next[in g] -> ... -> N[in f]
                        // next is missing or outside D's function. Do not
                        // extend this intra-procedural search through that hop.
                        feasible = false;
                    }
                    else if (!branch || !branch->getCondition() || !hasAbsState(current))
                    {
                        // Unconditional edge:
                        //   int x = input();  // D
                        //   int y = 2;        // M: no branch condition
                        //   use(x);           // N
                        // Caller-side hop:
                        //   D: ...; f(); ...; use(x);  // N
                        // Branch state not available yet:
                        //   D: ...; if (c) { ...; use(x); }  // B -> N
                        //   State[B] is absent in this iteration.
                        // Keep the input unchanged: none proves the hop false.
                    }
                    else
                    {
                        // int x = input();   // D
                        // if (x > 5) {       // B: an AE state already exists
                        //     use(x);        // N
                        // }
                        // Read B's existing Boolean result to check its edge;
                        // narrow only this path's copy of D's input below.
                        AbstractState branchState = getAbsState(current);
                        if (!isBranchEdgeFeasible(branch, branchState))
                            // int x = 0;        // D
                            // if (x > 5) {      // B: condition=[0,0]
                            //     use(x);       // N: true edge requires 1
                            // }
                            // [0,0] cannot take the true edge: reject this hop.
                            feasible = false;
                        else
                            // int x = input();  // D: x=[0,10]
                            // if (x > 5) {      // B: condition=[0,1]
                            //     use(x);       // N: keep true, carry x=[6,10]
                            // }
                            // Nested constraints can reject a carried input:
                            // if (x <= 5) {     // B1: candidate x=[0,5]
                            //     if (x > 5) { use(x); }  // B2 -> N
                            // }
                            // Even if State[B2] still admits true, intersecting
                            // candidate [0,5] with [6,10] gives bottom. Other
                            // unsupported constraints conservatively keep input.
                            feasible = applyBranchRefinement(branch, candidate);
                    }

                    // The two feasible-arrival cases below use one diamond:
                    //   int x = read_int();         // D: x=[-10,10]
                    //   if (x > 0) { then_work(); }  // B: true carries [1,10]
                    //   else { else_work(); }       // false carries [-10,0]
                    //   work();                     // M (= next), merge point
                    //   use(x);                     // N
                    //
                    //                D -> B
                    //             true /   \ false
                    //            [1,10]   [-10,0]
                    //                  \ /
                    //                   M -> N
                    // "First" means search arrival order, not program order.
                    // Either route can arrive first; the final JOIN is the same.
                    if (!feasible)
                    {
                        // int x = 0;        // D
                        // if (x > 5) { ...; }
                        // use(x);           // N is after the if
                        // Drop the true route only; the false route can still
                        // reach N. Other SVFG predecessors are not rejected.
                    }
                    else if (!incoming.count(next))
                    {
                        // Diamond above, assuming the true route arrives first:
                        //   incoming[M] absent, candidate=[1,10]
                        //   -> save incoming[M]=[1,10] and queue M.
                        // The false route has not contributed yet. This is
                        // the same initialization used for any first visit.
                        incoming[next] = candidate;
                        worklist.push(next);
                    }
                    else
                    {
                        // Same diamond, now the false route arrives at M:
                        //   incoming[M]=[1,10], candidate=[-10,0]
                        //   -> JOIN gives [-10,10]. M need not have been
                        //   processed yet; its first input is already saved.
                        // Both routes started at D, not two different SVFG defs.
                        AbstractState joined = incoming[next];
                        joined.joinWith(candidate);
                        // Another repeated-arrival pattern is a loop back edge:
                        //   int x = read_int();  // D: x=[-10,10]
                        //   while (again()) {}  // M: head / back edge
                        //   use(x);             // N
                        // Existing [-10,10] JOIN back-edge [-10,10] is unchanged:
                        // skip the if below and do not queue M again.
                        if (joined != incoming[next])
                        {
                            // Diamond: [1,10] -> [-10,10] enlarged M's input.
                            // Queue M so the added values can also reach N.
                            incoming[next] = std::move(joined);
                            worklist.push(next);
                        }
                    }
                }
            }
        }
    }
    return result;
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
