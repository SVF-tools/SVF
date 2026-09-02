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
//   * Recursion-specific helpers (isRecursiveFun, isRecursiveCallSite,
//     skipRecursiveCall, skipRecursionWithTop, shouldApplyNarrowing)
//

#include "AE/Svfexe/AEWTO.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "WPA/Andersen.h"

using namespace SVF;
using namespace SVFUtil;

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
void AbstractInterpretation::skipRecursionWithTop(const CallICFGNode* callNode)
{
    const RetICFGNode* retNode = callNode->getRetICFGNode();

    // 1. Set return value to TOP
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE* retPE =
                SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() &&
                !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
                updateAbsValue(retPE->getLHSVar(), IntervalValue::top(),
                               callNode);
        }
    }

    // 2. Set all stores in callee's reachable BBs to TOP
    if (retNode->getOutEdges().size() > 1)
    {
        copyAbstractState(callNode, retNode);
        return;
    }
    for (const SVFBasicBlock* bb :
         callNode->getCalledFunction()->getReachableBBs())
    {
        for (const ICFGNode* node : bb->getICFGNodeList())
        {
            for (const SVFStmt* stmt : node->getSVFStmts())
            {
                if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
                {
                    const SVFVar* rhsVar = store->getRHSVar();
                    if (!rhsVar->isPointer() &&
                        !rhsVar->isConstDataOrAggDataButNotNullPtr())
                    {
                        const AbstractValue& addrs =
                            getAbsValue(store->getLHSVar(), callNode);
                        if (addrs.isAddr())
                        {
                            for (const auto& addr : addrs.getAddrs())
                            {
                                updateMemoryValue(addr, IntervalValue::top(),
                                                  callNode);
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Copy callNode's state to retNode
    copyAbstractState(callNode, retNode);
}

/// Check if caller and callee are in the same CallGraph SCC (i.e. a recursive
/// callsite)
bool AbstractInterpretation::isRecursiveCallSite(const CallICFGNode* callNode,
                                                 const FunObjVar* callee)
{
    const FunObjVar* caller = callNode->getCaller();
    return preAnalysis->getPointerAnalysis()->inSameCallGraphSCC(caller,
                                                                 callee);
}

/// Skip recursive callsites (within SCC); entry calls from outside SCC are not
/// skipped
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

/// Check if narrowing should be applied: always for regular loops,
/// mode-dependent for recursion
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
        assert(false && "TOP mode should not reach narrowing phase for "
                        "recursive functions");
        return false;
    case WIDEN_ONLY:
        return false; // Skip narrowing for recursive functions
    case WIDEN_NARROW:
        return true; // Apply narrowing for recursive functions
    default:
        assert(false && "Unknown recursion handling mode");
        return false;
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
//  Native sparse implementations gather them into the cycle head
//  snapshot and scatter them back after each widen/narrow step so the
//  fixpoint can observe ValVar growth across iterations.
// =====================================================================

void AbstractInterpretation::handleLoopOrRecursion(const ICFGCycleWTO* cycle,
                                                   const CallICFGNode* caller)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();

    // TOP mode for recursive function cycles: set all stores and return value
    // to TOP
    if (Options::HandleRecur() == TOP && isRecursiveFun(cycle_head->getFun()))
    {
        if (caller)
            skipRecursionWithTop(caller);
        return;
    }

    // Iterate until fixpoint with widening/narrowing on the cycle head.
    bool increasing = true;
    u32_t widen_delay = Options::WidenDelay();
    for (u32_t cur_iter = 0;; cur_iter++)
    {
        if (cur_iter >= widen_delay)
        {
            // cloneCycleHeadState handles dense (returns trace[cycle_head])
            // and semi-sparse (collects ValVars from def-sites) uniformly.
            std::unique_ptr<AbstractDomain::AbstractState> previous =
                cloneCycleHeadState(cycle);

            if (mergeStatesFromPredecessors(cycle_head))
                handleICFGNode(cycle_head);
            std::unique_ptr<AbstractDomain::AbstractState> current =
                cloneCycleHeadState(cycle);

            if (increasing)
            {
                const bool stateFixpoint =
                    widenCycleState(*previous, *current, cycle);
                if (stateFixpoint)
                {
                    increasing = false;
                    continue;
                }
            }
            else
            {
                const bool stateFixpoint =
                    narrowCycleState(*previous, *current, cycle);
                if (stateFixpoint)
                    break;
            }
        }
        else
        {
            // Before widen_delay: process cycle head with gated pattern
            if (mergeStatesFromPredecessors(cycle_head))
                handleICFGNode(cycle_head);
        }

        // Process cycle body components (each with gated merge+handle)
        for (const ICFGWTOComp* comp : cycle->getWTOComponents())
        {
            if (const ICFGSingletonWTO* singleton =
                    SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            {
                const ICFGNode* node = singleton->getICFGNode();
                if (mergeStatesFromPredecessors(node))
                    handleICFGNode(node);
            }
            else if (const ICFGCycleWTO* subCycle =
                         SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                if (mergeStatesFromPredecessors(
                        subCycle->head()->getICFGNode()))
                    handleLoopOrRecursion(subCycle, caller);
            }
        }
    }
}
