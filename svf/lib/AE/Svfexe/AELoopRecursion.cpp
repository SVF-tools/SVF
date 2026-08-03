//===- AELoopRecursion.cpp -- Loop / recursion handling for AE ---------//
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
// Loop and recursion handling factored out of AbstractInterpretation.cpp.
// Contains:
//   * The widen/narrow fixpoint driver (handleLoopOrRecursion)
//   * The dense base cycle helpers (getFullCycleHeadState /
//     widenCycleState / narrowCycleState — semi-sparse overrides live in
//     SparseAbstractInterpretation.cpp)
//   * Recursion-specific helpers (isRecursiveFun, isRecursiveCallSite,
//     skipRecursiveCall, skipRecursionWithTop, shouldApplyNarrowing)
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "AE/Svfexe/AEWTO.h"
#include "SVFIR/SVFIR.h"
#include "WPA/Andersen.h"
#include "Util/Options.h"
#include <cstdlib>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace SVF;
using namespace SVFUtil;

namespace
{
static std::string aeTmpPath(const char* name)
{
    const char* prefix = std::getenv("AE_TMP_PREFIX");
    if (prefix == nullptr || *prefix == '\0')
        return std::string("/tmp/") + name;
    return std::string("/tmp/") + prefix + "_" + name;
}

static unsigned long spinProbeEnvUL(const char* name, unsigned long fallback)
{
    const char* raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0')
        return fallback;

    char* end = nullptr;
    unsigned long value = std::strtoul(raw, &end, 10);
    return (end == raw) ? fallback : value;
}

static unsigned long spinProbeLoopDumpEvery()
{
    static const unsigned long every = spinProbeEnvUL("SPINPROBE_LOOP_DUMP_EVERY", 1000);
    return every == 0 ? 1000 : every;
}

static bool hotCycleThrottleEnabled()
{
    static const bool enabled = (std::getenv("AE_HOT_CYCLE_THROTTLE") != nullptr);
    return enabled;
}

static unsigned long hotCycleThrottleThreshold()
{
    static const unsigned long threshold =
        spinProbeEnvUL("AE_HOT_CYCLE_THRESHOLD", 128);
    return threshold == 0 ? 1 : threshold;
}

static unsigned long hotCycleThrottleBodyThreshold()
{
    static const unsigned long threshold =
        spinProbeEnvUL("AE_HOT_CYCLE_BODY_THRESHOLD", 0);
    return threshold;
}

static unsigned long hotCycleThrottleDumpEvery()
{
    static const unsigned long every =
        spinProbeEnvUL("AE_HOT_CYCLE_DUMP_EVERY", 100);
    return every == 0 ? 100 : every;
}

static bool hotCyclePhiTopEnabled()
{
    static const bool enabled = (std::getenv("AE_HOT_CYCLE_PHI_TOP") != nullptr);
    return enabled;
}

static bool isPhiOnlyNode(const ICFGNode* node)
{
    if (node->getSVFStmts().size() != 1)
        return false;
    return SVFUtil::isa<PhiStmt>(*node->getSVFStmts().begin());
}

static void collectCycleNodes(const ICFGCycleWTO* cycle,
                              std::vector<const ICFGNode*>& nodes)
{
    nodes.push_back(cycle->head()->getICFGNode());
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton =
                SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            nodes.push_back(singleton->getICFGNode());
        }
        else if (const ICFGCycleWTO* subCycle =
                     SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            collectCycleNodes(subCycle, nodes);
        }
    }
}
}

// =====================================================================
//  Recursion helpers
// =====================================================================

/// Check if a function is recursive (part of a call graph SCC)
bool AbstractInterpretation::isRecursiveFun(const FunObjVar* fun)
{
    return preAnalysis->getPointerAnalysis()->isInRecursion(fun);
}

/// TOP mode for recursive calls: skip the function body entirely and
/// conservatively set all reachable stores and the return value to TOP.
void AbstractInterpretation::skipRecursionWithTop(const CallICFGNode *callNode)
{
    const RetICFGNode *retNode = callNode->getRetICFGNode();

    // 1. Set return value to TOP
    for (const SVFStmt* stmt : retNode->getSVFStmts())
    {
        const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt);
        if (retPE == nullptr ||
                retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
            continue;

        AbstractValue topValue(IntervalValue::top());
        if (retPE->getLHSVar()->isPointer())
            topValue.getAddrs().insert(BlackHoleObjAddr);
        updateAbsValue(retPE->getLHSVar(), topValue, callNode);
    }

    // 2. Set all stores in callee's reachable BBs to TOP
    if (retNode->getOutEdges().size() > 1)
    {
        updateAbsState(retNode, getAbsState(callNode));
        return;
    }
    for (const SVFBasicBlock* bb : callNode->getCalledFunction()->getReachableBBs())
    {
        for (const ICFGNode* node : bb->getICFGNodeList())
        {
            for (const SVFStmt* stmt : node->getSVFStmts())
            {
                if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
                {
                    const SVFVar* rhsVar = store->getRHSVar();
                    if (!rhsVar->isPointer() && !rhsVar->isConstDataOrAggDataButNotNullPtr())
                    {
                        const AbstractValue& addrs = getAbsValue(store->getLHSVar(), callNode);
                        if (addrs.isAddr())
                        {
                            AbstractState& as = getAbsState(callNode);
                            for (const auto& addr : addrs.getAddrs())
                                as.store(addr, IntervalValue::top());
                        }
                    }
                }
            }
        }
    }

    // 3. Copy callNode's state to retNode
    updateAbsState(retNode, getAbsState(callNode));
}

/// Check if caller and callee are in the same CallGraph SCC (i.e. a recursive callsite)
bool AbstractInterpretation::isRecursiveCallSite(const CallICFGNode* callNode,
        const FunObjVar* callee)
{
    const FunObjVar* caller = callNode->getCaller();
    return preAnalysis->getPointerAnalysis()->inSameCallGraphSCC(caller, callee);
}

/// Skip recursive callsites (within SCC); entry calls from outside SCC are not skipped
bool AbstractInterpretation::skipRecursiveCall(const CallICFGNode* callNode)
{
    const FunObjVar* callee = getCallee(callNode);
    if (!callee)
        return false;

    // Non-recursive function: never skip, always inline
    if (!isRecursiveFun(callee))
        return false;

    // For recursive functions, skip only recursive callsites (within same SCC).
    // Entry calls (from outside SCC) are not skipped - they are inlined so that
    // handleLoopOrRecursion() can analyze the function body.
    // This applies uniformly to all modes (TOP/WIDEN_ONLY/WIDEN_NARROW).
    return isRecursiveCallSite(callNode, callee);
}

/// Check if narrowing should be applied: always for regular loops, mode-dependent for recursion
bool AbstractInterpretation::shouldApplyNarrowing(const FunObjVar* fun)
{
    // Non-recursive functions (regular loops): always apply narrowing
    if (!isRecursiveFun(fun))
        return true;

    // Recursive functions: WIDEN_NARROW applies narrowing, WIDEN_ONLY does not
    // TOP mode exits early in handleLoopOrRecursion, so should not reach here
    switch (Options::HandleRecur())
    {
    case TOP:
        assert(false && "TOP mode should not reach narrowing phase for recursive functions");
        return false;
    case WIDEN_ONLY:
        return false;  // Skip narrowing for recursive functions
    case WIDEN_NARROW:
        return true;   // Apply narrowing for recursive functions
    default:
        assert(false && "Unknown recursion handling mode");
        return false;
    }
}

// =====================================================================
//  Cycle state helpers (dense base)
//
//  Dense default: trace[cycle_head] is the authoritative primary
//  storage, so the snapshot / write-back are trivial.
//  SemiSparseAbstractInterpretation overrides these to additionally
//  pull/scatter cycle ValVars from/to their def-sites.
// =====================================================================

AbstractState AbstractInterpretation::getFullCycleHeadState(const ICFGCycleWTO* cycle)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    AbstractState snap;
    if (hasAbsState(cycle_head))
        snap = getAbsState(cycle_head);
    return snap;
}

bool AbstractInterpretation::widenCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    AbstractState prev_copy = prev;
    AbstractState next = prev_copy.widening(cur);
    // Always write back (even at fixpoint) so cycle_head's trace holds the
    // widened state for the upcoming narrowing phase.
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    abstractTrace[cycle_head] = next;
    return next == prev;
}

bool AbstractInterpretation::narrowCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    if (!shouldApplyNarrowing(cycle_head->getFun()))
        return true;
    AbstractState prev_copy = prev;
    AbstractState next = prev_copy.narrowing(cur);
    if (next == prev)
        return true;  // fixpoint
    abstractTrace[cycle_head] = next;
    return false;
}

bool AbstractInterpretation::applyHotCycleThrottle(const ICFGCycleWTO* cycle,
        bool force)
{
    if (!hotCycleThrottleEnabled())
        return false;

    if (!force)
    {
        const bool hotByInvocations =
            spinCycleInvocations[cycle] > hotCycleThrottleThreshold();
        const unsigned long bodyThreshold = hotCycleThrottleBodyThreshold();
        const bool hotByBody =
            bodyThreshold != 0 && spinCycleBodyNodeExecs[cycle] >= bodyThreshold;
        if (!hotByInvocations && !hotByBody)
            return false;
    }

    std::vector<const ICFGNode*> nodes;
    collectCycleNodes(cycle, nodes);
    std::sort(nodes.begin(), nodes.end());
    nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());

    const AbstractValue topValue(IntervalValue::top());
    unsigned long varTops = 0;
    unsigned long locTops = 0;
    unsigned long nodeTouches = 0;

    for (const ICFGNode* node : nodes)
    {
        auto stateIt = abstractTrace.find(node);
        if (stateIt == abstractTrace.end())
            continue;

        AbstractState& state = stateIt->second;
        bool touched = false;

        std::vector<u32_t> varIds;
        for (const auto& kv : state.getVarToVal())
        {
            if (kv.second.isInterval())
                varIds.push_back(kv.first);
        }
        for (u32_t id : varIds)
        {
            state[id] = topValue;
            ++varTops;
            touched = true;
        }

        std::vector<u32_t> locIds;
        for (const auto& kv : state.getLocToVal())
        {
            if (kv.second.isInterval())
                locIds.push_back(kv.first);
        }
        for (u32_t id : locIds)
        {
            state.store(AbstractState::getVirtualMemAddress(id), topValue);
            ++locTops;
            touched = true;
        }

        if (touched)
            ++nodeTouches;
    }

    ++hotCycleThrottleHits[cycle];
    hotCycleThrottleVarTops[cycle] += varTops;
    hotCycleThrottleLocTops[cycle] += locTops;
    hotCycleThrottleNodeTouches[cycle] += nodeTouches;
    hotCycleThrottleFuns.insert(cycle->head()->getICFGNode()->getFun());
    ++hotCycleThrottleTotalHits;
    hotCycleThrottleTotalVarTops += varTops;
    hotCycleThrottleTotalLocTops += locTops;

    if (hotCycleThrottleTotalHits == 1 ||
            hotCycleThrottleTotalHits % hotCycleThrottleDumpEvery() == 0)
        dumpHotCycleThrottleStats("live");

    return true;
}

void AbstractInterpretation::dumpHotCycleThrottleStats(const char* reason) const
{
    std::ofstream of(aeTmpPath("svf_hotcycle_throttle.tsv"));
    of << "# reason=" << reason
       << "\tenabled=" << (hotCycleThrottleEnabled() ? 1 : 0)
       << "\tthreshold=" << hotCycleThrottleThreshold()
       << "\tbody_threshold=" << hotCycleThrottleBodyThreshold()
       << "\ttotal_hits=" << hotCycleThrottleTotalHits
       << "\ttotal_var_tops=" << hotCycleThrottleTotalVarTops
       << "\ttotal_loc_tops=" << hotCycleThrottleTotalLocTops
       << "\n";
    of << "function\thead_node_id\thits\tinvocations\titerations"
       << "\tbody_node_execs\tsubcycle_calls\tvar_tops\tloc_tops\tnode_touches\n";

    auto countOf = [](const Map<const ICFGCycleWTO*, unsigned long>& m,
                      const ICFGCycleWTO* c) -> unsigned long {
        auto it = m.find(c);
        return it == m.end() ? 0 : it->second;
    };

    std::vector<const ICFGCycleWTO*> cycles;
    cycles.reserve(hotCycleThrottleHits.size());
    for (const auto& kv : hotCycleThrottleHits)
        cycles.push_back(kv.first);
    std::sort(cycles.begin(), cycles.end(),
              [&](const ICFGCycleWTO* lhs, const ICFGCycleWTO* rhs) {
                  const unsigned long lh = countOf(hotCycleThrottleHits, lhs);
                  const unsigned long rh = countOf(hotCycleThrottleHits, rhs);
                  if (lh != rh)
                      return lh > rh;
                  return lhs->head()->getICFGNode()->getId() <
                         rhs->head()->getICFGNode()->getId();
              });

    for (const ICFGCycleWTO* cycle : cycles)
    {
        const ICFGNode* head = cycle->head()->getICFGNode();
        of << head->getFun()->getName()
           << "\t" << head->getId()
           << "\t" << countOf(hotCycleThrottleHits, cycle)
           << "\t" << countOf(spinCycleInvocations, cycle)
           << "\t" << countOf(spinCycleIterations, cycle)
           << "\t" << countOf(spinCycleBodyNodeExecs, cycle)
           << "\t" << countOf(spinCycleSubcycleCalls, cycle)
           << "\t" << countOf(hotCycleThrottleVarTops, cycle)
           << "\t" << countOf(hotCycleThrottleLocTops, cycle)
           << "\t" << countOf(hotCycleThrottleNodeTouches, cycle)
           << "\n";
    }
}

// =====================================================================
//  Cycle / recursion driver
//
//  Handle a WTO cycle (loop or recursive function) using widening /
//  narrowing iteration.  Widening at cycle head ensures termination.
//
//  == What is being widened ==
//  The abstract state at the cycle head node, which includes:
//  - Variable values (intervals) that may change across loop iterations
//  - For example, a loop counter `i` starting at 0 and incrementing
//    each iteration
//
//  == Regular loops (non-recursive functions) ==
//  All modes (TOP/WIDEN_ONLY/WIDEN_NARROW) behave the same for regular
//  loops:
//   1. Widening phase: iterate until the cycle head state stabilizes
//      Example: for(i=0; i<100; i++)  ->  i widens to [0, +inf]
//   2. Narrowing phase: refine the over-approximation from widening
//      Example: [0, +inf] narrows to [0, 100] using loop condition
//
//  == Recursive function cycles ==
//  Behavior depends on Options::HandleRecur():
//
//  - TOP:           skip body entirely, set return + reachable stores
//                   to TOP (most conservative, fastest)
//  - WIDEN_ONLY:    widening only, no narrowing
//                     factorial(5) -> [10000, +inf]
//  - WIDEN_NARROW:  widening + narrowing
//                     factorial(5) -> [10000, 10000]
//
//  == Semi-sparse note ==
//  In semi-sparse mode ValVars live at their def-sites and do not flow
//  through cycle_head's merge.  The cycle helpers in
//  SparseAbstractInterpretation.cpp gather them into the cycle_head
//  snapshot and scatter them back after each widen/narrow step so the
//  fixpoint can observe ValVar growth across iterations.
// =====================================================================

void AbstractInterpretation::handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    ++spinCycleInvocations[cycle];
    spinActiveCycle = cycle;
    spinActiveNode = cycle_head;

    // TOP mode for recursive function cycles: set all stores and return value to TOP
    if (Options::HandleRecur() == TOP && isRecursiveFun(cycle_head->getFun()))
    {
        if (caller)
            skipRecursionWithTop(caller);
        return;
    }

    if (applyEntryBudgetCycleTop(cycle))
    {
        dumpSpinProbe("entry-budget-cycle");
        return;
    }

    // EXPERIMENT (loop memoization): if this cycle's head input state (cycle
    // vars collected at the head) is identical to its previous invocation, the
    // widen/narrow fixpoint would reproduce the same result, so skip it.
    ++loopMemoTotal;
    AbstractState memoSnap = getFullCycleHeadState(cycle);
    auto memoIt = cycleInputCache.find(cycle);
    if (memoIt != cycleInputCache.end() && memoIt->second == memoSnap &&
            cycleInputVer[cycle] == gepOverlayVersion)
    {
        ++loopMemoHits;
        return;
    }
    cycleInputCache[cycle] = memoSnap;
    cycleInputVer[cycle] = gepOverlayVersion;

    if (applyHotCycleThrottle(cycle))
        return;

    if (hotCyclePhiTopEnabled() &&
            hotCycleThrottleFuns.find(cycle_head->getFun()) != hotCycleThrottleFuns.end() &&
            isPhiOnlyNode(cycle_head))
    {
        if (applyHotCycleThrottle(cycle, true))
        {
            dumpSpinProbe("hot-cycle-phi-only");
            return;
        }
    }

    // Iterate until fixpoint with widening/narrowing on the cycle head.
    bool increasing = true;
    u32_t widen_delay = Options::WidenDelay();
    for (u32_t cur_iter = 0;; cur_iter++)
    {
        spinActiveCycle = cycle;
        spinActiveNode = cycle_head;
        spinActiveIter = cur_iter;
        ++spinGlobalLoopIters;
        ++spinCycleIterations[cycle];
        if (applyEntryBudgetCycleTop(cycle))
        {
            dumpSpinProbe("entry-budget-cycle");
            return;
        }
        if (spinGlobalLoopIters % spinProbeLoopDumpEvery() == 0)
            dumpSpinProbe("loop");

        if (cur_iter >= widen_delay)
        {
            // getFullCycleHeadState handles dense (returns trace[cycle_head])
            // and semi-sparse (collects ValVars from def-sites) uniformly.
            AbstractState prev = getFullCycleHeadState(cycle);

            if (mergeStatesFromPredecessors(cycle_head))
            {
                ++spinCycleHeadExecs[cycle];
                handleICFGNode(cycle_head);
                if (applyHotCycleThrottle(cycle))
                {
                    dumpSpinProbe("hot-cycle-head");
                    return;
                }
            }
            AbstractState cur = getFullCycleHeadState(cycle);

            if (increasing)
            {
                if (widenCycleState(prev, cur, cycle))
                {
                    ++spinCycleWidenFixpoints[cycle];
                    increasing = false;
                    continue;
                }
            }
            else
            {
                if (narrowCycleState(prev, cur, cycle))
                {
                    ++spinCycleNarrowFixpoints[cycle];
                    dumpSpinProbe("cycle-fixpoint");
                    break;
                }
            }
        }
        else
        {
            // Before widen_delay: process cycle head with gated pattern
            if (mergeStatesFromPredecessors(cycle_head))
            {
                ++spinCycleHeadExecs[cycle];
                handleICFGNode(cycle_head);
                if (applyHotCycleThrottle(cycle))
                {
                    dumpSpinProbe("hot-cycle-head");
                    return;
                }
            }
        }

        // Process cycle body components (each with gated merge+handle)
        for (const ICFGWTOComp* comp : cycle->getWTOComponents())
        {
            if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            {
                const ICFGNode* node = singleton->getICFGNode();
                if (mergeStatesFromPredecessors(node))
                {
                    ++spinCycleBodyNodeExecs[cycle];
                    handleICFGNode(node);
                    if (applyHotCycleThrottle(cycle))
                    {
                        dumpSpinProbe("hot-cycle-mid");
                        return;
                    }
                }
            }
            else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                if (mergeStatesFromPredecessors(subCycle->head()->getICFGNode()))
                {
                    ++spinCycleSubcycleCalls[cycle];
                    handleLoopOrRecursion(subCycle, caller);
                    spinActiveCycle = cycle;
                    spinActiveNode = cycle_head;
                    if (applyHotCycleThrottle(cycle))
                    {
                        dumpSpinProbe("hot-cycle-mid");
                        return;
                    }
                }
            }
        }
    }
    dumpSpinProbe("cycle-exit");
}
