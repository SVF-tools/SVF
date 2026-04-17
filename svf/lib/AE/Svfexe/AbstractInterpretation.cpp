//===- AbstractExecution.cpp -- Abstract Execution---------------------------------//
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
//  Created on: Jan 10, 2024
//      Author: Xiao Cheng, Jiawei Wang
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "Util/WorkList.h"
#include "Graphs/CallGraph.h"
#include "WPA/Andersen.h"
#include <cmath>
#include <deque>

using namespace SVF;
using namespace SVFUtil;


void AbstractInterpretation::runOnModule(ICFG *_icfg)
{
    stat->startClk();
    icfg = _icfg;
    svfir = PAG::getPAG();
    // Run Andersen's pointer analysis and build WTO
    preAnalysis = new PreAnalysis(svfir, icfg);
    callGraph = preAnalysis->getCallGraph();
    icfg->updateCallGraph(callGraph);
    preAnalysis->initWTO();
    preAnalysis->initCycleValVars();

    svfStateMgr = new AbstractStateManager(svfir, preAnalysis->getPointerAnalysis());
    utils = new AbsExtAPI(svfStateMgr);

    /// collect checkpoint
    utils->collectCheckPoint();

    analyse();
    utils->checkPointAllSet();
    stat->endClk();
    stat->finializeStat();
    if (Options::PStat())
        stat->performStat();
    for (auto& detector: detectors)
        detector->reportBug();
}

AbstractInterpretation::AbstractInterpretation()
{
    stat = new AEStat(this);
}


void AbstractInterpretation::propagateObjVarAbsVal(const ObjVar* var, const ICFGNode* defSite)
{
    const AbstractValue& val = getAbsValue(var, defSite);
    for (const ICFGNode* useSite : svfStateMgr->getUseSitesOfObjVar(var, defSite))
    {
        updateAbsValue(var, val, useSite);
    }
}


/// Destructor
AbstractInterpretation::~AbstractInterpretation()
{
    delete utils;
    delete stat;
    delete preAnalysis;
    delete svfStateMgr;
}

/// Collect entry point functions for analysis.
/// Entry points are functions without callers (no incoming edges in CallGraph).
/// Uses a deque to allow efficient insertion at front for prioritizing main()
std::deque<const FunObjVar*> AbstractInterpretation::collectProgEntryFuns()
{
    std::deque<const FunObjVar*> entryFunctions;

    for (auto it = callGraph->begin(); it != callGraph->end(); ++it)
    {
        const CallGraphNode* cgNode = it->second;
        const FunObjVar* fun = cgNode->getFunction();

        // Skip declarations
        if (fun->isDeclaration())
            continue;

        // Entry points are functions without callers (no incoming edges)
        if (cgNode->getInEdges().empty())
        {
            // If main exists, put it first for priority using deque's push_front
            if (fun->getName() == "main")
            {
                entryFunctions.push_front(fun);
            }
            else
            {
                entryFunctions.push_back(fun);
            }
        }
    }

    return entryFunctions;
}


/// Program entry - analyze from all entry points (multi-entry analysis is the default)
void AbstractInterpretation::analyse()
{
    // Always use multi-entry analysis from all entry points
    analyzeFromAllProgEntries();
}

/// Analyze all entry points (functions without callers) - for whole-program analysis.
/// Abstract state is shared across entry points so that functions analyzed from
/// earlier entries are not re-analyzed from scratch.
void AbstractInterpretation::analyzeFromAllProgEntries()
{
    // Collect all entry point functions
    std::deque<const FunObjVar*> entryFunctions = collectProgEntryFuns();

    if (entryFunctions.empty())
    {
        assert(false && "No entry functions found for analysis");
        return;
    }
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    for (const FunObjVar* entryFun : entryFunctions)
    {
        const ICFGNode* funEntry = icfg->getFunEntryICFGNode(entryFun);
        handleFunction(funEntry);
    }
}

/// handle global node
/// Initializes the abstract state for the global ICFG node and processes all global statements.
/// This includes setting up the null pointer and black hole pointer (blkPtr).
/// BlkPtr is initialized to point to the BlackHole object, representing
/// an unknown memory location that cannot be statically resolved.
void AbstractInterpretation::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    // Global init is one of the few legitimate direct-mutation sites:
    // updateAbsState filters out ValVars in semi-sparse mode, but NullPtr/
    // BlkPtr have no SVFVar so we cannot route them through updateAbsValue.
    // Use the manager's operator[] (auto-creates the entry if absent).
    AbstractState& init = (*svfStateMgr)[node];
    init = AbstractState();
    // TODO: we cannot find right SVFVar for NullPtr, so we use init[NullPtr]
    // directly. Same for BlkPtr below.
    init[IRGraph::NullPtr] = AddressValue();

    // Global Node, we just need to handle addr, load, store, copy and gep
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }

    // BlkPtr represents a pointer whose target is statically unknown (e.g., from
    // int2ptr casts, external function returns, or unmodeled instructions like
    // AtomicCmpXchg). It should be an address pointing to the BlackHole object
    // (ID=2), NOT an interval top.
    //
    // History: this was originally set to IntervalValue::top() as a quick fix when
    // the analysis crashed on programs containing uninitialized BlkPtr. However,
    // BlkPtr is semantically a *pointer* (address domain), not a numeric value
    // (interval domain). Setting it to interval top broke cross-domain consistency:
    // the interval domain and address domain gave contradictory information for the
    // same variable. The correct representation is an AddressValue containing the
    // BlackHole virtual address, which means "points to unknown memory".
    (*svfStateMgr)[node][PAG::getPAG()->getBlkPtr()] = AddressValue(BlackHoleObjAddr);
}

/// Pull-based state merge: for each predecessor that has an abstract state,
/// copy its state, apply branch refinement for conditional IntraCFGEdges,
/// and join all feasible states into getAbsState(node).
/// The join semantics are sparsity-aware (see AbstractState::joinWith).
/// Returns true if at least one predecessor contributed state.
bool AbstractInterpretation::mergeStatesFromPredecessors(const ICFGNode* node)
{
    // Collect all feasible predecessor states, then merge at the end.
    AbstractState merged;
    bool hasFeasiblePred = false;

    for (auto& edge : node->getInEdges())
    {
        const ICFGNode* pred = edge->getSrcNode();
        if (!hasAbsState(pred))
            continue;

        if (const IntraCFGEdge* intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
        {
            if (intraCfgEdge->getCondition())
            {
                AbstractState predState = getAbsState(pred);
                if (isBranchFeasible(intraCfgEdge, predState))
                {
                    merged.joinWith(predState);
                    hasFeasiblePred = true;
                }
            }
            else
            {
                merged.joinWith(getAbsState(pred));
                hasFeasiblePred = true;
            }
        }
        else if (SVFUtil::isa<CallCFGEdge>(edge))
        {
            merged.joinWith(getAbsState(pred));
            hasFeasiblePred = true;
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            switch (Options::HandleRecur())
            {
            case TOP:
                merged.joinWith(getAbsState(pred));
                hasFeasiblePred = true;
                break;
            case WIDEN_ONLY:
            case WIDEN_NARROW:
            {
                const RetICFGNode* returnSite = SVFUtil::dyn_cast<RetICFGNode>(node);
                const CallICFGNode* callSite = returnSite->getCallICFGNode();
                if (hasAbsState(callSite))
                {
                    merged.joinWith(getAbsState(pred));
                    hasFeasiblePred = true;
                }
                break;
            }
            }
        }
    }

    if (!hasFeasiblePred)
        return false;

    updateAbsState(node, merged);

    return true;
}


/// Given a cmp operand, walk its SSA def edge to find the LoadStmt that
/// produced it. This lets us trace back to the ObjVar in memory so that
/// branch narrowing can refine the stored value.
///
/// Example: for `%cmp = icmp sgt %a, 5` where `%a = load i32, ptr %p`,
/// calling findBackingLoad(%a) returns the LoadStmt, and we can then
/// narrow the ObjVar behind %p.
///
/// Follows one level of CopyStmt (e.g., zext/sext) if the load is not
/// directly on the cmp operand. Returns nullptr if no load is found.
static const LoadStmt* findBackingLoad(const SVFVar* var)
{
    if (var->getInEdges().empty())
        return nullptr;
    SVFStmt* inStmt = *var->getInEdges().begin();
    if (const LoadStmt* ls = SVFUtil::dyn_cast<LoadStmt>(inStmt))
        return ls;
    if (const CopyStmt* cs = SVFUtil::dyn_cast<CopyStmt>(inStmt))
    {
        const SVFVar* src = cs->getRHSVar();
        if (!src->getInEdges().empty())
            return SVFUtil::dyn_cast<LoadStmt>(*src->getInEdges().begin());
    }
    return nullptr;
}

/// Compute the interval constraint on one cmp operand given the predicate,
/// branch direction (succ), which side it is on, and the other operand's
/// interval. Returns top if no useful narrowing is possible.
///
/// Called from isCmpBranchFeasible for each non-constant operand that has a
/// backing load. Given a branch condition like:
///
///   %cmp = icmp sgt %a, 5       ;  a > 5
///   br i1 %cmp, label %T, %F
///
/// On the true branch (succ=1), operand %a (isLHS=true) is constrained to
/// [6, +inf). On the false branch (succ=0), %a is constrained to (-inf, 5].
/// The result is used to narrow the ObjVar behind %a's load.
static IntervalValue computeCmpConstraint(s32_t predicate, s64_t succ,
        bool isLHS, const IntervalValue& self,
        const IntervalValue& other)
{
    // Normalize: always reason from the LHS perspective.
    // If we are the RHS operand, swap the predicate direction.
    if (!isLHS)
    {
        // a > b from b's perspective: b < a
        static const Map<s32_t, s32_t> swapPred =
        {
            {CmpStmt::ICMP_EQ,  CmpStmt::ICMP_EQ},
            {CmpStmt::ICMP_NE,  CmpStmt::ICMP_NE},
            {CmpStmt::ICMP_SGT, CmpStmt::ICMP_SLT},
            {CmpStmt::ICMP_SGE, CmpStmt::ICMP_SLE},
            {CmpStmt::ICMP_SLT, CmpStmt::ICMP_SGT},
            {CmpStmt::ICMP_SLE, CmpStmt::ICMP_SGE},
            {CmpStmt::ICMP_UGT, CmpStmt::ICMP_ULT},
            {CmpStmt::ICMP_UGE, CmpStmt::ICMP_ULE},
            {CmpStmt::ICMP_ULT, CmpStmt::ICMP_UGT},
            {CmpStmt::ICMP_ULE, CmpStmt::ICMP_UGE},
            {CmpStmt::FCMP_OEQ, CmpStmt::FCMP_OEQ},
            {CmpStmt::FCMP_UEQ, CmpStmt::FCMP_UEQ},
            {CmpStmt::FCMP_OGT, CmpStmt::FCMP_OLT},
            {CmpStmt::FCMP_OGE, CmpStmt::FCMP_OLE},
            {CmpStmt::FCMP_OLT, CmpStmt::FCMP_OGT},
            {CmpStmt::FCMP_OLE, CmpStmt::FCMP_OGE},
            {CmpStmt::FCMP_UGT, CmpStmt::FCMP_ULT},
            {CmpStmt::FCMP_UGE, CmpStmt::FCMP_ULE},
            {CmpStmt::FCMP_ULT, CmpStmt::FCMP_UGT},
            {CmpStmt::FCMP_ULE, CmpStmt::FCMP_UGE},
            {CmpStmt::FCMP_ONE, CmpStmt::FCMP_ONE},
            {CmpStmt::FCMP_UNE, CmpStmt::FCMP_UNE},
        };
        auto it = swapPred.find(predicate);
        if (it == swapPred.end()) return IntervalValue::top();
        predicate = it->second;
    }

    // If false branch, negate the predicate.
    if (succ == 0)
    {
        static const Map<s32_t, s32_t> negPred =
        {
            {CmpStmt::ICMP_EQ,  CmpStmt::ICMP_NE},
            {CmpStmt::ICMP_NE,  CmpStmt::ICMP_EQ},
            {CmpStmt::ICMP_SGT, CmpStmt::ICMP_SLE},
            {CmpStmt::ICMP_SGE, CmpStmt::ICMP_SLT},
            {CmpStmt::ICMP_SLT, CmpStmt::ICMP_SGE},
            {CmpStmt::ICMP_SLE, CmpStmt::ICMP_SGT},
            {CmpStmt::ICMP_UGT, CmpStmt::ICMP_ULE},
            {CmpStmt::ICMP_UGE, CmpStmt::ICMP_ULT},
            {CmpStmt::ICMP_ULT, CmpStmt::ICMP_UGE},
            {CmpStmt::ICMP_ULE, CmpStmt::ICMP_UGT},
            {CmpStmt::FCMP_OEQ, CmpStmt::FCMP_ONE},
            {CmpStmt::FCMP_UEQ, CmpStmt::FCMP_UNE},
            {CmpStmt::FCMP_OGT, CmpStmt::FCMP_OLE},
            {CmpStmt::FCMP_OGE, CmpStmt::FCMP_OLT},
            {CmpStmt::FCMP_OLT, CmpStmt::FCMP_OGE},
            {CmpStmt::FCMP_OLE, CmpStmt::FCMP_OGT},
            {CmpStmt::FCMP_UGT, CmpStmt::FCMP_ULE},
            {CmpStmt::FCMP_UGE, CmpStmt::FCMP_ULT},
            {CmpStmt::FCMP_ULT, CmpStmt::FCMP_UGE},
            {CmpStmt::FCMP_ULE, CmpStmt::FCMP_UGT},
            {CmpStmt::FCMP_ONE, CmpStmt::FCMP_OEQ},
            {CmpStmt::FCMP_UNE, CmpStmt::FCMP_UEQ},
        };
        auto it = negPred.find(predicate);
        if (it == negPred.end()) return IntervalValue::top();
        predicate = it->second;
    }

    // Now compute the constraint on LHS given: LHS <predicate> other
    IntervalValue result = self;
    switch (predicate)
    {
    case CmpStmt::ICMP_EQ:
    case CmpStmt::FCMP_OEQ:
    case CmpStmt::FCMP_UEQ:
        result.meet_with(other);
        break;
    case CmpStmt::ICMP_NE:
    case CmpStmt::FCMP_ONE:
    case CmpStmt::FCMP_UNE:
    case CmpStmt::FCMP_FALSE:
    case CmpStmt::FCMP_TRUE:
        return IntervalValue::top(); // no useful narrowing
    case CmpStmt::ICMP_UGT:
    case CmpStmt::ICMP_SGT:
    case CmpStmt::FCMP_OGT:
    case CmpStmt::FCMP_UGT:
        result.meet_with(IntervalValue(other.lb() + 1, IntervalValue::plus_infinity()));
        break;
    case CmpStmt::ICMP_UGE:
    case CmpStmt::ICMP_SGE:
    case CmpStmt::FCMP_OGE:
    case CmpStmt::FCMP_UGE:
        result.meet_with(IntervalValue(other.lb(), IntervalValue::plus_infinity()));
        break;
    case CmpStmt::ICMP_ULT:
    case CmpStmt::ICMP_SLT:
    case CmpStmt::FCMP_OLT:
    case CmpStmt::FCMP_ULT:
        result.meet_with(IntervalValue(IntervalValue::minus_infinity(), other.ub() - 1));
        break;
    case CmpStmt::ICMP_ULE:
    case CmpStmt::ICMP_SLE:
    case CmpStmt::FCMP_OLE:
    case CmpStmt::FCMP_ULE:
        result.meet_with(IntervalValue(IntervalValue::minus_infinity(), other.ub()));
        break;
    default:
        return IntervalValue::top();
    }
    return result;
}

bool AbstractInterpretation::isCmpBranchFeasible(const IntraCFGEdge* edge,
        AbstractState& as)
{
    const ICFGNode* pred = edge->getSrcNode();
    s64_t succ = edge->getSuccessorCondValue();
    const CmpStmt* cmpStmt = SVFUtil::cast<CmpStmt>(
                                 *edge->getCondition()->getInEdges().begin());
    s32_t predicate = cmpStmt->getPredicate();

    if (cmpStmt->getOpVarID(0) == IRGraph::NullPtr ||
            cmpStmt->getOpVarID(1) == IRGraph::NullPtr)
        return true;

    AbstractValue opVal[2] =
    {
        getAbsValue(cmpStmt->getOpVar(0), pred),
        getAbsValue(cmpStmt->getOpVar(1), pred)
    };
    if (opVal[0].isAddr() || opVal[1].isAddr())
        return true;

    // Feasibility check: cmp result must be compatible with branch successor
    IntervalValue resVal = getAbsValue(cmpStmt->getRes(), pred).getInterval();
    resVal.meet_with(IntervalValue((s64_t)succ, succ));
    if (resVal.isBottom())
        return false;

    // For each operand: if it is the variable side (non-constant), has a
    // backing load, and the branch imposes a useful constraint, narrow the
    // ObjVar behind the load.
    for (int i = 0; i < 2; i++)
    {
        if (opVal[i].getInterval().is_numeral() || !opVal[1-i].getInterval().is_numeral())
            continue; // skip constant operands and var-vs-var

        if (const LoadStmt* load = findBackingLoad(cmpStmt->getOpVar(i)))
        {
            IntervalValue narrowed = computeCmpConstraint(
                                         predicate, succ, i == 0,
                                         opVal[i].getInterval(), opVal[1-i].getInterval());

            if (!narrowed.isTop())
            {
                const AbstractValue& ptrVal = getAbsValue(load->getRHSVar(), pred);
                if (ptrVal.isAddr())
                {
                    for (const auto& addr : ptrVal.getAddrs())
                    {
                        NodeID objId = as.getIDFromAddr(addr);
                        if (as.inAddrToValTable(objId))
                            as.load(addr).meet_with(narrowed);
                    }
                }
            }
        }
    }
    return true;
}

bool AbstractInterpretation::isSwitchBranchFeasible(const IntraCFGEdge* edge,
        AbstractState& as)
{
    const ICFGNode* pred = edge->getSrcNode();
    s64_t succ = edge->getSuccessorCondValue();
    const SVFVar* var = edge->getCondition();

    if (!as.inVarToValTable(var->getId()) && !as.inVarToAddrsTable(var->getId()))
        as[var->getId()] = getAbsValue(var, pred);
    IntervalValue& switch_cond = as[var->getId()].getInterval();
    switch_cond.meet_with(IntervalValue(succ, succ));
    if (switch_cond.isBottom())
        return false;

    FIFOWorkList<const SVFStmt*> stmtList;
    for (SVFStmt* stmt : var->getInEdges())
        stmtList.push(stmt);
    while (!stmtList.empty())
    {
        const SVFStmt* stmt = stmtList.pop();
        if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            const AbstractValue& ptrVal = getAbsValue(load->getRHSVar(), pred);
            if (ptrVal.isAddr())
            {
                for (const auto& addr : ptrVal.getAddrs())
                {
                    NodeID objId = as.getIDFromAddr(addr);
                    if (as.inAddrToValTable(objId))
                        as.load(addr).meet_with(switch_cond);
                }
            }
        }
    }
    return true;
}

bool AbstractInterpretation::isBranchFeasible(const IntraCFGEdge* edge,
        AbstractState& as)
{
    const SVFVar* cmpVar = edge->getCondition();
    assert(!cmpVar->getInEdges().empty() && "branch condition has no defining edge?");
    if (SVFUtil::isa<CmpStmt>(*cmpVar->getInEdges().begin()))
        return isCmpBranchFeasible(edge, as);
    return isSwitchBranchFeasible(edge, as);
}

/**
 * Handle an ICFG node: execute statements on the current abstract state.
 * The node's pre-state must already be in getAbsState(node) (set by
 * mergeStatesFromPredecessors, or by handleGlobalNode for the global node).
 * Returns true if the abstract state has changed, false if fixpoint reached or unreachable.
 */
bool AbstractInterpretation::handleICFGNode(const ICFGNode* node)
{
    // Check reachability: pre-state must have been propagated by predecessors
    bool isFunEntry = SVFUtil::isa<FunEntryICFGNode>(node);
    if (!hasAbsState(node))
    {
        if (isFunEntry)
        {
            // Entry point with no callers: inherit from global node
            const ICFGNode* globalNode = icfg->getGlobalICFGNode();
            if (hasAbsState(globalNode))
                updateAbsState(node, getAbsState(globalNode));
            else
                updateAbsState(node, AbstractState());
        }
        else
        {
            return false;  // unreachable node
        }
    }

    // Store the previous state for fixpoint detection
    AbstractState prevState = getAbsState(node);

    stat->getBlockTrace()++;
    stat->getICFGNodeTrace()++;

    // Handle SVF statements
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }

    // Handle call sites
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        handleCallSite(callNode);
    }

    // Run detectors
    for (auto& detector: detectors)
        detector->detect(node);
    stat->countStateSize();

    // Track this node as analyzed (for coverage statistics across all entry points)
    allAnalyzedNodes.insert(node);

    if (getAbsState(node) == prevState)
        return false;

    return true;
}

/**
 * Handle a function using worklist algorithm guided by WTO order.
 * All top-level WTO components are pushed into the worklist upfront,
 * so the traversal order is exactly the WTO order — each node is
 * visited once, and cycles are handled as whole components.
 */
void AbstractInterpretation::handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller)
{
    auto it = preAnalysis->getFuncToWTO().find(funEntry->getFun());
    if (it == preAnalysis->getFuncToWTO().end())
        return;

    // Push all top-level WTO components into the worklist in WTO order
    FIFOWorkList<const ICFGWTOComp*> worklist(it->second->getWTOComponents());

    while (!worklist.empty())
    {
        const ICFGWTOComp* comp = worklist.pop();

        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            const ICFGNode* node = singleton->getICFGNode();
            if (mergeStatesFromPredecessors(node))
                handleICFGNode(node);
        }
        else if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            if (mergeStatesFromPredecessors(cycle->head()->getICFGNode()))
                handleLoopOrRecursion(cycle, caller);
        }
    }
}


void AbstractInterpretation::handleCallSite(const ICFGNode* node)
{
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        if (isExtCall(callNode))
        {
            handleExtCall(callNode);
        }
        else
        {
            // Handle both direct and indirect calls uniformly
            handleFunCall(callNode);
        }
    }
    else
        assert (false && "it is not call node");
}

bool AbstractInterpretation::isExtCall(const CallICFGNode *callNode)
{
    return SVFUtil::isExtCall(callNode->getCalledFunction());
}

void AbstractInterpretation::handleExtCall(const CallICFGNode *callNode)
{
    utils->handleExtAPI(callNode);
    for (auto& detector : detectors)
    {
        detector->handleStubFunctions(callNode);
    }
}

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
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() &&
                    !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
                updateAbsValue(retPE->getLHSVar(), IntervalValue::top(), callNode);
        }
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
                            AbstractState& as = svfStateMgr->getAbstractState(callNode);
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

/// Get callee function: directly for direct calls, via pointer analysis for indirect calls
const FunObjVar* AbstractInterpretation::getCallee(const CallICFGNode* callNode)
{
    // Direct call: get callee directly from call node
    if (const FunObjVar* callee = callNode->getCalledFunction())
        return callee;

    // Indirect call: resolve callee through pointer analysis
    const auto callsiteMaps = svfir->getIndirectCallsites();
    auto it = callsiteMaps.find(callNode);
    if (it == callsiteMaps.end())
        return nullptr;

    NodeID call_id = it->second;
    if (!hasAbsState(callNode))
        return nullptr;

    const AbstractValue& Addrs = getAbsValue(svfir->getSVFVar(call_id), callNode);
    if (!Addrs.isAddr() || Addrs.getAddrs().empty())
        return nullptr;

    NodeID addr = *Addrs.getAddrs().begin();
    const SVFVar* func_var = getSVFVar(getAbsState(callNode).getIDFromAddr(addr));
    return SVFUtil::dyn_cast<FunObjVar>(func_var);
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
/// Handle direct or indirect call: get callee(s), process function body, set return state.
///
/// For direct calls, the callee is known statically.
/// For indirect calls, the previous implementation resolved callees from the abstract
/// state's address domain, which only picked the first address and missed other targets.
/// Since the abstract state's address domain is not an over-approximation for function
/// pointers (it may be uninitialized or incomplete), we now use Andersen's pointer
/// analysis results from the pre-computed call graph, which soundly resolves all
/// possible indirect call targets.
void AbstractInterpretation::handleFunCall(const CallICFGNode *callNode)
{
    if (skipRecursiveCall(callNode))
        return;

    // Direct call: callee is known
    if (const FunObjVar* callee = callNode->getCalledFunction())
    {
        const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
        handleFunction(calleeEntry, callNode);
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        updateAbsState(retNode, getAbsState(callNode));
        return;
    }

    // Indirect call: use Andersen's call graph to get all resolved callees.
    const RetICFGNode* retNode = callNode->getRetICFGNode();
    if (callGraph->hasIndCSCallees(callNode))
    {
        const auto& callees = callGraph->getIndCSCallees(callNode);
        for (const FunObjVar* callee : callees)
        {
            if (callee->isDeclaration())
                continue;
            const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
            handleFunction(calleeEntry, callNode);
        }
    }
    // Resume return node from caller's state (context-insensitive)
    updateAbsState(retNode, getAbsState(callNode));
}

/// Handle WTO cycle (loop or recursive function) using widening/narrowing iteration.
///
/// Widening is applied at the cycle head to ensure termination of the analysis.
/// The cycle head's abstract state is iteratively updated until a fixpoint is reached.
///
/// == What is being widened ==
/// The abstract state at the cycle head node, which includes:
/// - Variable values (intervals) that may change across loop iterations
/// - For example, a loop counter `i` starting at 0 and incrementing each iteration
///
/// == Regular loops (non-recursive functions) ==
/// All modes (TOP/WIDEN_ONLY/WIDEN_NARROW) behave the same for regular loops:
/// 1. Widening phase: Iterate until the cycle head state stabilizes
///    Example: for(i=0; i<100; i++) -> i widens to [0, +inf]
/// 2. Narrowing phase: Refine the over-approximation from widening
///    Example: [0, +inf] narrows to [0, 100] using loop condition
///
/// == Recursive function cycles ==
/// Behavior depends on Options::HandleRecur():
///
/// - TOP mode:
///     Does not iterate. Calls skipRecursionWithTop() to set all stores and
///     return value to TOP immediately. This is the most conservative but fastest.
///     Example:
///       int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
///       factorial(5) -> returns [-inf, +inf]
///
/// - WIDEN_ONLY mode:
///     Widening phase only, no narrowing for recursive functions.
///     The recursive function body is analyzed with widening until fixpoint.
///     Example:
///       int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
///       factorial(5) -> returns [10000, +inf] (widened upper bound)
///
/// - WIDEN_NARROW mode:
///     Both widening and narrowing phases for recursive functions.
///     After widening reaches fixpoint, narrowing refines the result.
///     Example:
///       int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
///       factorial(5) -> returns [10000, 10000] (precise after narrowing)

// ---------------------------------------------------------------------------
// Semi-sparse cycle helpers
// ---------------------------------------------------------------------------
//
// In semi-sparse mode, ValVars live at their def-sites and do not flow
// through the cycle-head merge. The around-merge widening at cycle_head
// therefore cannot observe ValVar growth across iterations (e.g. an
// ArgValVar that a recursive CallPE keeps overwriting at the FunEntry).
//
// To fix that, handleLoopOrRecursion runs an extra cross-iter widening on
// a snapshot that pulls every cycle ValVar into cycle_head's state. The set
// of ValVar IDs per cycle is precomputed once after PreAnalysis::initWTO()
// (initCycleValVars), bottom-up so nested cycles are handled before their
// enclosing cycle.

// --- Cycle state helpers (dense/sparse unified) ---

AbstractState AbstractInterpretation::getFullCycleHeadState(const ICFGCycleWTO* cycle)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    AbstractState snap;
    if (hasAbsState(cycle_head))
        snap = getAbsState(cycle_head);

    if (Options::AESparsity() == AESparsity::SemiSparse)
    {
        const Set<const ValVar*>& valVars = preAnalysis->getCycleValVars(cycle);
        if (valVars.empty())
            return snap;  // dense path / no cycle ValVars: snap is already complete

        // Semi-sparse: drop any stale ValVar entries cached at cycle_head and
        // pull each cycle ValVar from its def-site.  ValVars without a stored
        // value are skipped to avoid the top-fallback contamination.
        snap.clearValVars();
        for (const ValVar* v : valVars)
        {
            const ICFGNode* defSite = v->getICFGNode();
            if (!defSite || !hasAbsValue(v, defSite)) continue;
            snap[v->getId()] = getAbsValue(v, defSite);
        }
        return snap;
    }
    else
    {
        return snap;
    }
}


bool AbstractInterpretation::widenCycleState(
    const AbstractState& prev, const AbstractState& cur, const ICFGCycleWTO* cycle)
{
    AbstractState prev_copy = prev;
    AbstractState next = prev_copy.widening(cur);
    // Always write back (even at fixpoint) so cycle_head's trace holds the
    // widened state for the upcoming narrowing phase.
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    svfStateMgr->getTrace()[cycle_head] = next;
    if (Options::AESparsity() == AESparsity::SemiSparse)
    {
        for (const auto& [id, val] : next.getVarToVal())
        {
            updateAbsValue(svfir->getSVFVar(id), val, cycle_head);
        }
    }
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
    svfStateMgr->getTrace()[cycle_head] = next;
    if (Options::AESparsity() == AESparsity::SemiSparse)
    {
        for (const auto& [id, val] : next.getVarToVal())
        {
            updateAbsValue(svfir->getSVFVar(id), val, cycle_head);
        }
    }
    return false;
}


void AbstractInterpretation::handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();

    // TOP mode for recursive function cycles: set all stores and return value to TOP
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
            // getFullCycleHeadState handles dense (returns trace[cycle_head])
            // and semi-sparse (collects ValVars from def-sites) uniformly.
            AbstractState prev = getFullCycleHeadState(cycle);

            if (mergeStatesFromPredecessors(cycle_head))
                handleICFGNode(cycle_head);
            AbstractState cur = getFullCycleHeadState(cycle);

            if (increasing)
            {
                if (widenCycleState(prev, cur, cycle))
                {
                    increasing = false;
                    continue;
                }
            }
            else
            {
                if (narrowCycleState(prev, cur, cycle))
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
            if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            {
                const ICFGNode* node = singleton->getICFGNode();
                if (mergeStatesFromPredecessors(node))
                    handleICFGNode(node);
            }
            else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                if (mergeStatesFromPredecessors(subCycle->head()->getICFGNode()))
                    handleLoopOrRecursion(subCycle, caller);
            }
        }
    }
}

void AbstractInterpretation::handleSVFStatement(const SVFStmt *stmt)
{
    if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
    {
        updateStateOnAddr(addr);
    }
    else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
    {
        updateStateOnBinary(binary);
    }
    else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
    {
        updateStateOnCmp(cmp);
    }
    else if (SVFUtil::isa<UnaryOPStmt>(stmt))
    {
    }
    else if (SVFUtil::isa<BranchStmt>(stmt))
    {
        // branch stmt is handled in hasBranchES
    }
    else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt))
    {
        updateStateOnLoad(load);
    }
    else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
    {
        updateStateOnStore(store);
    }
    else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
    {
        updateStateOnCopy(copy);
    }
    else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        updateStateOnGep(gep);
    }
    else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt))
    {
        updateStateOnSelect(select);
    }
    else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
    {
        updateStateOnPhi(phi);
    }
    else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt))
    {
        // To handle Call Edge
        updateStateOnCall(callPE);
    }
    else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt))
    {
        updateStateOnRet(retPE);
    }
    else
        assert(false && "implement this part");
    // NullPtr should not be changed by any statement. If the entry is missing
    // (not yet auto-inserted) we treat that as "unchanged" — only check the
    // entry if it actually exists.
    {
        const auto& vmap = getAbsState(stmt->getICFGNode()).getVarToVal();
        auto it = vmap.find(IRGraph::NullPtr);
        assert(it == vmap.end() ||
               (!it->second.isInterval() && !it->second.isAddr()));
    }
}

void AbstractInterpretation::updateStateOnGep(const GepStmt *gep)
{
    const ICFGNode* node = gep->getICFGNode();
    IntervalValue offsetPair = svfStateMgr->getGepElementIndex(gep);
    AddressValue gepAddrs = svfStateMgr->getGepObjAddrs(SVFUtil::cast<ValVar>(gep->getRHSVar()), offsetPair);
    updateAbsValue(gep->getLHSVar(), gepAddrs, node);
}

void AbstractInterpretation::updateStateOnSelect(const SelectStmt *select)
{
    const ICFGNode* node = select->getICFGNode();
    const AbstractValue& condVal = getAbsValue(select->getCondition(), node);
    const AbstractValue& tVal = getAbsValue(select->getTrueValue(), node);
    const AbstractValue& fVal = getAbsValue(select->getFalseValue(), node);
    AbstractValue resVal;
    if (condVal.getInterval().is_numeral())
    {
        resVal = condVal.getInterval().is_zero() ? fVal : tVal;
    }
    else
    {
        resVal = tVal;
        resVal.join_with(fVal);
    }
    updateAbsValue(select->getRes(), resVal, node);
}

void AbstractInterpretation::updateStateOnPhi(const PhiStmt *phi)
{
    const ICFGNode* icfgNode = phi->getICFGNode();
    AbstractValue rhs;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = phi->getOpICFGNode(i);
        if (hasAbsState(opICFGNode))
        {
            AbstractState tmpState = getAbsState(opICFGNode);
            const AbstractValue& opVal = getAbsValue(phi->getOpVar(i), opICFGNode);
            const ICFGEdge* edge = icfg->getICFGEdge(opICFGNode, icfgNode, ICFGEdge::IntraCF);
            if (edge)
            {
                const IntraCFGEdge* intraEdge = SVFUtil::cast<IntraCFGEdge>(edge);
                if (intraEdge->getCondition())
                {
                    if (isBranchFeasible(intraEdge, tmpState))
                        rhs.join_with(opVal);
                }
                else
                    rhs.join_with(opVal);
            }
            else
            {
                rhs.join_with(opVal);
            }
        }
    }
    updateAbsValue(phi->getRes(), rhs, icfgNode);
}


/// Handle CallPE: phi-like merging of actual parameters from all call sites
/// into the formal parameter at FunEntryICFGNode (e.g., formal = join(actual1@cs1, actual2@cs2, ...))
void AbstractInterpretation::updateStateOnCall(const CallPE *callPE)
{
    const ICFGNode* node = callPE->getICFGNode();
    const SVFVar* res = callPE->getRes();
    AbstractValue rhs;
    for (u32_t i = 0; i < callPE->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = callPE->getOpCallICFGNode(i);
        if (hasAbsState(opICFGNode))
        {
            const AbstractValue& opVal = getAbsValue(callPE->getOpVar(i), opICFGNode);
            rhs.join_with(opVal);
        }
    }
    updateAbsValue(res, rhs, node);
}

void AbstractInterpretation::updateStateOnRet(const RetPE *retPE)
{
    const ICFGNode* node = retPE->getICFGNode();
    const AbstractValue& rhsVal = getAbsValue(retPE->getRHSVar(), node);
    updateAbsValue(retPE->getLHSVar(), rhsVal, node);
}


void AbstractInterpretation::updateStateOnAddr(const AddrStmt *addr)
{
    const ICFGNode* node = addr->getICFGNode();
    // initObjVar mutates _varToAbsVal/_addrToAbsVal directly, so we need
    // mutable access; route via the manager.
    AbstractState& as = svfStateMgr->getAbstractState(node);
    as.initObjVar(SVFUtil::cast<ObjVar>(addr->getRHSVar()));
    // AddrStmt: lhs(ValVar) = &rhs(ObjVar).
    // as[rhsId] stores the ObjVar's virtual address in _varToVal,
    // NOT the object contents. So we must use as[] directly for ObjVar.
    u32_t rhsId = addr->getRHSVarID();
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        as[rhsId].getInterval().meet_with(utils->getRangeLimitFromType(addr->getRHSVar()->getType()));
    // LHS is a ValVar (pointer), write through the API
    updateAbsValue(addr->getLHSVar(), as[rhsId], node);
}


void AbstractInterpretation::updateStateOnBinary(const BinaryOPStmt *binary)
{
    const ICFGNode* node = binary->getICFGNode();
    // Treat bottom (uninitialized) operands as top for soundness
    const AbstractValue& op0Val = getAbsValue(binary->getOpVar(0), node);
    const AbstractValue& op1Val = getAbsValue(binary->getOpVar(1), node);
    IntervalValue lhs = op0Val.getInterval().isBottom() ? IntervalValue::top() : op0Val.getInterval();
    IntervalValue rhs = op1Val.getInterval().isBottom() ? IntervalValue::top() : op1Val.getInterval();
    IntervalValue resVal;
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    case BinaryOPStmt::FAdd:
        resVal = (lhs + rhs);
        break;
    case BinaryOPStmt::Sub:
    case BinaryOPStmt::FSub:
        resVal = (lhs - rhs);
        break;
    case BinaryOPStmt::Mul:
    case BinaryOPStmt::FMul:
        resVal = (lhs * rhs);
        break;
    case BinaryOPStmt::SDiv:
    case BinaryOPStmt::FDiv:
    case BinaryOPStmt::UDiv:
        resVal = (lhs / rhs);
        break;
    case BinaryOPStmt::SRem:
    case BinaryOPStmt::FRem:
    case BinaryOPStmt::URem:
        resVal = (lhs % rhs);
        break;
    case BinaryOPStmt::Xor:
        resVal = (lhs ^ rhs);
        break;
    case BinaryOPStmt::And:
        resVal = (lhs & rhs);
        break;
    case BinaryOPStmt::Or:
        resVal = (lhs | rhs);
        break;
    case BinaryOPStmt::AShr:
        resVal = (lhs >> rhs);
        break;
    case BinaryOPStmt::Shl:
        resVal = (lhs << rhs);
        break;
    case BinaryOPStmt::LShr:
        resVal = (lhs >> rhs);
        break;
    default:
        assert(false && "undefined binary: ");
    }
    updateAbsValue(binary->getRes(), resVal, node);
}

void AbstractInterpretation::updateStateOnCmp(const CmpStmt *cmp)
{
    const ICFGNode* node = cmp->getICFGNode();
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    const AbstractValue& op0Val = getAbsValue(cmp->getOpVar(0), node);
    const AbstractValue& op1Val = getAbsValue(cmp->getOpVar(1), node);

    // if it is address
    if (op0Val.isAddr() && op1Val.isAddr())
    {
        IntervalValue resVal;
        const AddressValue& addrOp0 = op0Val.getAddrs();
        const AddressValue& addrOp1 = op1Val.getAddrs();
        if (addrOp0.equals(addrOp1))
        {
            resVal = IntervalValue(1, 1);
        }
        else if (addrOp0.hasIntersect(addrOp1))
        {
            resVal = IntervalValue(0, 1);
        }
        else
        {
            resVal = IntervalValue(0, 0);
        }
        updateAbsValue(cmp->getRes(), resVal, node);
    }
    // if op0 or op1 is nullptr, compare abstractValue instead of touching addr or interval
    else if (op0 == IRGraph::NullPtr || op1 == IRGraph::NullPtr)
    {
        IntervalValue resVal = (op0Val.equals(op1Val)) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        updateAbsValue(cmp->getRes(), resVal, node);
    }
    else
    {
        {
            IntervalValue resVal;
            if (op0Val.isInterval() && op1Val.isInterval())
            {
                // Treat bottom (uninitialized) operands as top for soundness
                IntervalValue lhs = op0Val.getInterval().isBottom() ? IntervalValue::top() : op0Val.getInterval(),
                              rhs = op1Val.getInterval().isBottom() ? IntervalValue::top() : op1Val.getInterval();
                // AbstractValue
                auto predicate = cmp->getPredicate();
                switch (predicate)
                {
                case CmpStmt::ICMP_EQ:
                case CmpStmt::FCMP_OEQ:
                case CmpStmt::FCMP_UEQ:
                    resVal = (lhs == rhs);
                    // resVal = (lhs.getInterval() == rhs.getInterval());
                    break;
                case CmpStmt::ICMP_NE:
                case CmpStmt::FCMP_ONE:
                case CmpStmt::FCMP_UNE:
                    resVal = (lhs != rhs);
                    break;
                case CmpStmt::ICMP_UGT:
                case CmpStmt::ICMP_SGT:
                case CmpStmt::FCMP_OGT:
                case CmpStmt::FCMP_UGT:
                    resVal = (lhs > rhs);
                    break;
                case CmpStmt::ICMP_UGE:
                case CmpStmt::ICMP_SGE:
                case CmpStmt::FCMP_OGE:
                case CmpStmt::FCMP_UGE:
                    resVal = (lhs >= rhs);
                    break;
                case CmpStmt::ICMP_ULT:
                case CmpStmt::ICMP_SLT:
                case CmpStmt::FCMP_OLT:
                case CmpStmt::FCMP_ULT:
                    resVal = (lhs < rhs);
                    break;
                case CmpStmt::ICMP_ULE:
                case CmpStmt::ICMP_SLE:
                case CmpStmt::FCMP_OLE:
                case CmpStmt::FCMP_ULE:
                    resVal = (lhs <= rhs);
                    break;
                case CmpStmt::FCMP_FALSE:
                    resVal = IntervalValue(0, 0);
                    break;
                case CmpStmt::FCMP_TRUE:
                    resVal = IntervalValue(1, 1);
                    break;
                case CmpStmt::FCMP_ORD:
                case CmpStmt::FCMP_UNO:
                    // FCMP_ORD: true if both operands are not NaN
                    // FCMP_UNO: true if either operand is NaN
                    // Conservatively return [0, 1] since we don't track NaN
                    resVal = IntervalValue(0, 1);
                    break;
                default:
                    assert(false && "undefined compare: ");
                }
                updateAbsValue(cmp->getRes(), resVal, node);
            }
            else if (op0Val.isAddr() && op1Val.isAddr())
            {
                const AddressValue& lhs = op0Val.getAddrs();
                const AddressValue& rhs = op1Val.getAddrs();
                auto predicate = cmp->getPredicate();
                switch (predicate)
                {
                case CmpStmt::ICMP_EQ:
                case CmpStmt::FCMP_OEQ:
                case CmpStmt::FCMP_UEQ:
                {
                    if (lhs.hasIntersect(rhs))
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    else if (lhs.empty() && rhs.empty())
                    {
                        resVal = IntervalValue(1, 1);
                    }
                    else
                    {
                        resVal = IntervalValue(0, 0);
                    }
                    break;
                }
                case CmpStmt::ICMP_NE:
                case CmpStmt::FCMP_ONE:
                case CmpStmt::FCMP_UNE:
                {
                    if (lhs.hasIntersect(rhs))
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    else if (lhs.empty() && rhs.empty())
                    {
                        resVal = IntervalValue(0, 0);
                    }
                    else
                    {
                        resVal = IntervalValue(1, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_UGT:
                case CmpStmt::ICMP_SGT:
                case CmpStmt::FCMP_OGT:
                case CmpStmt::FCMP_UGT:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() > *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_UGE:
                case CmpStmt::ICMP_SGE:
                case CmpStmt::FCMP_OGE:
                case CmpStmt::FCMP_UGE:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() >= *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_ULT:
                case CmpStmt::ICMP_SLT:
                case CmpStmt::FCMP_OLT:
                case CmpStmt::FCMP_ULT:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() < *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_ULE:
                case CmpStmt::ICMP_SLE:
                case CmpStmt::FCMP_OLE:
                case CmpStmt::FCMP_ULE:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() <= *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::FCMP_FALSE:
                    resVal = IntervalValue(0, 0);
                    break;
                case CmpStmt::FCMP_TRUE:
                    resVal = IntervalValue(1, 1);
                    break;
                case CmpStmt::FCMP_ORD:
                case CmpStmt::FCMP_UNO:
                    // FCMP_ORD: true if both operands are not NaN
                    // FCMP_UNO: true if either operand is NaN
                    // Conservatively return [0, 1] since we don't track NaN
                    resVal = IntervalValue(0, 1);
                    break;
                default:
                    assert(false && "undefined compare: ");
                }
                updateAbsValue(cmp->getRes(), resVal, node);
            }
        }
    }
}

void AbstractInterpretation::updateStateOnLoad(const LoadStmt *load)
{
    const ICFGNode* node = load->getICFGNode();
    updateAbsValue(load->getLHSVar(),
                   svfStateMgr->loadValue(SVFUtil::cast<ValVar>(load->getRHSVar()), node), node);
}

void AbstractInterpretation::updateStateOnStore(const StoreStmt *store)
{
    const ICFGNode* node = store->getICFGNode();
    svfStateMgr->storeValue(SVFUtil::cast<ValVar>(store->getLHSVar()),
                            getAbsValue(store->getRHSVar(), node), node);
}

void AbstractInterpretation::updateStateOnCopy(const CopyStmt *copy)
{
    const ICFGNode* node = copy->getICFGNode();
    const SVFVar* lhsVar = copy->getLHSVar();
    const SVFVar* rhsVar = copy->getRHSVar();

    auto getZExtValue = [&](const SVFVar* var)
    {
        const SVFType* type = var->getType();
        if (SVFUtil::isa<SVFIntegerType>(type))
        {
            u32_t bits = type->getByteSize() * 8;
            const AbstractValue& val = getAbsValue(var, node);
            if (val.getInterval().is_numeral())
            {
                if (bits == 8)
                {
                    int8_t signed_i8_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<uint8_t>(signed_i8_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 16)
                {
                    s16_t signed_i16_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u16_t>(signed_i16_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 32)
                {
                    s32_t signed_i32_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u32_t>(signed_i32_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 64)
                {
                    s64_t signed_i64_value = val.getInterval().getIntNumeral();
                    return IntervalValue((s64_t)signed_i64_value, (s64_t)signed_i64_value);
                }
                else
                    assert(false && "cannot support int type other than u8/16/32/64");
            }
            else
            {
                return IntervalValue::top();
            }
        }
        return IntervalValue::top();
    };

    auto getTruncValue = [&](const SVFVar* var, const SVFType* dstType)
    {
        const IntervalValue& itv = getAbsValue(var, node).getInterval();
        if(itv.isBottom()) return itv;
        s64_t int_lb = itv.lb().getIntNumeral();
        s64_t int_ub = itv.ub().getIntNumeral();
        u32_t dst_bits = dstType->getByteSize() * 8;
        if (dst_bits == 8)
        {
            int8_t s8_lb = static_cast<int8_t>(int_lb);
            int8_t s8_ub = static_cast<int8_t>(int_ub);
            if (s8_lb > s8_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s8_lb, s8_ub);
        }
        else if (dst_bits == 16)
        {
            s16_t s16_lb = static_cast<s16_t>(int_lb);
            s16_t s16_ub = static_cast<s16_t>(int_ub);
            if (s16_lb > s16_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s16_lb, s16_ub);
        }
        else if (dst_bits == 32)
        {
            s32_t s32_lb = static_cast<s32_t>(int_lb);
            s32_t s32_ub = static_cast<s32_t>(int_ub);
            if (s32_lb > s32_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s32_lb, s32_ub);
        }
        else
        {
            assert(false && "cannot support dst int type other than u8/16/32");
            abort();
        }
    };

    const AbstractValue& rhsVal = getAbsValue(rhsVar, node);

    if (copy->getCopyKind() == CopyStmt::COPYVAL)
    {
        updateAbsValue(lhsVar, rhsVal, node);
    }
    else if (copy->getCopyKind() == CopyStmt::ZEXT)
    {
        updateAbsValue(lhsVar, getZExtValue(rhsVar), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SEXT)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOSI)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOUI)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SITOFP)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::UITOFP)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::TRUNC)
    {
        updateAbsValue(lhsVar, getTruncValue(rhsVar, lhsVar->getType()), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTRUNC)
    {
        updateAbsValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::INTTOPTR)
    {
        //insert nullptr
    }
    else if (copy->getCopyKind() == CopyStmt::PTRTOINT)
    {
        updateAbsValue(lhsVar, IntervalValue::top(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::BITCAST)
    {
        if (rhsVal.isAddr())
            updateAbsValue(lhsVar, rhsVal, node);
    }
    else
        assert(false && "undefined copy kind");
}



