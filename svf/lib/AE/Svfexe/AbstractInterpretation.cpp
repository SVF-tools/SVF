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
    utils = new AbsExtAPI();

    // Run Andersen's pointer analysis and build WTO
    preAnalysis = new PreAnalysis(svfir, icfg);
    callGraph = preAnalysis->getCallGraph();
    icfg->updateCallGraph(callGraph);
    preAnalysis->initWTO();

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

/// Destructor
AbstractInterpretation::~AbstractInterpretation()
{
    delete stat;
    delete preAnalysis;
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
    globalState = AbstractState();
    globalState[IRGraph::NullPtr] = AddressValue();

    // Global Node, we just need to handle addr, load, store, copy and gep
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt, globalState);
    }

    // BlkPtr represents a pointer whose target is statically unknown (e.g., from
    // int2ptr casts, external function returns, or unmodeled instructions like
    // AtomicCmpXchg). It should be an address pointing to the BlackHole object
    // (ID=2), NOT an interval top.
    globalState[PAG::getPAG()->getBlkPtr()] =
        AddressValue(BlackHoleObjAddr);
}

/// Propagate the post-state of a node along each outgoing IntraCFGEdge.
/// For conditional branches, the state is refined via isBranchFeasible.
/// The result is written to edgeAbsStates (overwrite, not join).
/// Downstream nodes will merge these edge states via mergeInEdges().
/// Call/return edges are handled by handleFunCall, not here.
void AbstractInterpretation::propagateToSuccessor(const ICFGNode* node, AbstractState& as)
{
    for (auto& edge : node->getOutEdges())
    {
        if (const IntraCFGEdge* intraEdge =
                    SVFUtil::dyn_cast<IntraCFGEdge>(edge))
        {
            AbstractState state = as;
            if (intraEdge->getCondition())
            {
                if (!isBranchFeasible(intraEdge, state))
                    continue;  // infeasible branch, do not propagate
                // state has been refined in-place by isBranchFeasible
            }
            edgeAbsStates[intraEdge] = state;
        }
        // CallCFGEdge/RetCFGEdge are handled by handleFunCall
    }
}

/// Merge all incoming edge states from edgeAbsStates into a fresh AbstractState.
/// Edge states are NOT erased — they persist for phi reads and loop iterations.
AbstractState AbstractInterpretation::mergeInEdges(const ICFGNode* node)
{
    AbstractState merged;
    for (auto& edge : node->getInEdges())
    {
        auto it = edgeAbsStates.find(edge);
        if (it == edgeAbsStates.end())
            continue;
        merged.joinWith(it->second);
    }
    return merged;
}


bool AbstractInterpretation::isCmpBranchFeasible(const CmpStmt* cmpStmt, s64_t succ,
        AbstractState& as)
{
    AbstractState new_es = as;
    // get cmp stmt's op0, op1, and predicate
    NodeID op0 = cmpStmt->getOpVarID(0);
    NodeID op1 = cmpStmt->getOpVarID(1);
    NodeID res_id = cmpStmt->getResID();
    s32_t predicate = cmpStmt->getPredicate();
    // if op0 or op1 is nullptr, no need to change value, just copy the state
    if (op0 == IRGraph::NullPtr || op1 == IRGraph::NullPtr)
    {
        as = new_es;
        return true;
    }
    // if op0 or op1 is undefined, return;
    // skip address compare
    if (new_es.inVarToAddrsTable(op0) || new_es.inVarToAddrsTable(op1))
    {
        as = new_es;
        return true;
    }
    const LoadStmt *load_op0 = nullptr;
    const LoadStmt *load_op1 = nullptr;
    // get '%1 = load i32 s', and load inst may not exist
    const SVFVar* loadVar0 = getSVFVar(op0);
    if (!loadVar0->getInEdges().empty())
    {
        SVFStmt *loadVar0InStmt = *loadVar0->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt))
        {
            load_op0 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar0InStmt))
        {
            loadVar0 = getSVFVar(copyStmt->getRHSVarID());
            if (!loadVar0->getInEdges().empty())
            {
                SVFStmt *loadVar0InStmt2 = *loadVar0->getInEdges().begin();
                if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt2))
                {
                    load_op0 = loadStmt;
                }
            }
        }
    }

    const SVFVar* loadVar1 = getSVFVar(op1);
    if (!loadVar1->getInEdges().empty())
    {
        SVFStmt *loadVar1InStmt = *loadVar1->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt))
        {
            load_op1 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar1InStmt))
        {
            loadVar1 = getSVFVar(copyStmt->getRHSVarID());
            if (!loadVar1->getInEdges().empty())
            {
                SVFStmt *loadVar1InStmt2 = *loadVar1->getInEdges().begin();
                if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt2))
                {
                    load_op1 = loadStmt;
                }
            }
        }
    }
    // for const X const, we may get concrete resVal instantly
    // for var X const, we may get [0,1] if the intersection of var and const is not empty set
    IntervalValue resVal = new_es[res_id].getInterval();
    resVal.meet_with(IntervalValue((s64_t) succ, succ));
    // If Var X const generates bottom value, it means this branch path is not feasible.
    if (resVal.isBottom())
    {
        return false;
    }

    bool b0 = new_es[op0].getInterval().is_numeral();
    bool b1 = new_es[op1].getInterval().is_numeral();

    // if const X var, we should reverse op0 and op1.
    if (b0 && !b1)
    {
        std::swap(op0, op1);
        std::swap(load_op0, load_op1);
        predicate = _switch_lhsrhs_predicate[predicate];
    }
    else
    {
        // if var X var, we cannot preset the branch condition to infer the intervals of var0,var1
        if (!b0 && !b1)
        {
            as = new_es;
            return true;
        }
        // if const X const, we can instantly get the resVal
        else if (b0 && b1)
        {
            as = new_es;
            return true;
        }
    }
    // if cmp is 'var X const == false', we should reverse predicate 'var X' const == true'
    // X' is reverse predicate of X
    if (succ == 0)
    {
        predicate = _reverse_predicate[predicate];
    }
    else {}
    // change interval range according to the compare predicate
    AddressValue addrs;
    if(load_op0 && new_es.inVarToAddrsTable(load_op0->getRHSVarID()))
        addrs = new_es[load_op0->getRHSVarID()].getAddrs();

    IntervalValue &lhs = new_es[op0].getInterval(), &rhs = new_es[op1].getInterval();
    switch (predicate)
    {
    case CmpStmt::Predicate::ICMP_EQ:
    case CmpStmt::Predicate::FCMP_OEQ:
    case CmpStmt::Predicate::FCMP_UEQ:
    {
        // Var == Const, so [var.lb, var.ub].meet_with(const)
        lhs.meet_with(rhs);
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(rhs);
            }
        }
        break;
    }
    case CmpStmt::Predicate::ICMP_NE:
    case CmpStmt::Predicate::FCMP_ONE:
    case CmpStmt::Predicate::FCMP_UNE:
        // Compliment set
        break;
    case CmpStmt::Predicate::ICMP_UGT:
    case CmpStmt::Predicate::ICMP_SGT:
    case CmpStmt::Predicate::FCMP_OGT:
    case CmpStmt::Predicate::FCMP_UGT:
        // Var > Const, so [var.lb, var.ub].meet_with([Const+1, +INF])
        lhs.meet_with(IntervalValue(rhs.lb() + 1, IntervalValue::plus_infinity()));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(rhs.lb() + 1, IntervalValue::plus_infinity()));
            }
        }
        break;
    case CmpStmt::Predicate::ICMP_UGE:
    case CmpStmt::Predicate::ICMP_SGE:
    case CmpStmt::Predicate::FCMP_OGE:
    case CmpStmt::Predicate::FCMP_UGE:
    {
        // Var >= Const, so [var.lb, var.ub].meet_with([Const, +INF])
        lhs.meet_with(IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
            }
        }
        break;
    }
    case CmpStmt::Predicate::ICMP_ULT:
    case CmpStmt::Predicate::ICMP_SLT:
    case CmpStmt::Predicate::FCMP_OLT:
    case CmpStmt::Predicate::FCMP_ULT:
    {
        // Var < Const, so [var.lb, var.ub].meet_with([-INF, const.ub-1])
        lhs.meet_with(IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
            }
        }
        break;
    }
    case CmpStmt::Predicate::ICMP_ULE:
    case CmpStmt::Predicate::ICMP_SLE:
    case CmpStmt::Predicate::FCMP_OLE:
    case CmpStmt::Predicate::FCMP_ULE:
    {
        // Var <= Const, so [var.lb, var.ub].meet_with([-INF, const.ub])
        lhs.meet_with(IntervalValue(IntervalValue::minus_infinity(), rhs.ub()));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(IntervalValue::minus_infinity(), rhs.ub()));
            }
        }
        break;
    }
    case CmpStmt::Predicate::FCMP_FALSE:
        break;
    case CmpStmt::Predicate::FCMP_TRUE:
        break;
    default:
        assert(false && "implement this part");
        abort();
    }
    as = new_es;
    return true;
}

bool AbstractInterpretation::isSwitchBranchFeasible(const SVFVar* var, s64_t succ,
        AbstractState& as)
{
    AbstractState new_es = as;
    IntervalValue& switch_cond = new_es[var->getId()].getInterval();
    s64_t value = succ;
    FIFOWorkList<const SVFStmt*> workList;
    for (SVFStmt *cmpVarInStmt: var->getInEdges())
    {
        workList.push(cmpVarInStmt);
    }
    switch_cond.meet_with(IntervalValue(value, value));
    if (switch_cond.isBottom())
    {
        return false;
    }
    while(!workList.empty())
    {
        const SVFStmt* stmt = workList.pop();
        if (SVFUtil::isa<CopyStmt>(stmt))
        {
            IntervalValue& copy_cond = new_es[var->getId()].getInterval();
            copy_cond.meet_with(IntervalValue(value, value));
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            if (new_es.inVarToAddrsTable(load->getRHSVarID()))
            {
                AddressValue &addrs = new_es[load->getRHSVarID()].getAddrs();
                for (const auto &addr: addrs)
                {
                    NodeID objId = new_es.getIDFromAddr(addr);
                    if (new_es.inAddrToValTable(objId))
                    {
                        new_es.load(addr).meet_with(switch_cond);
                    }
                }
            }
        }
    }
    as = new_es;
    return true;
}

bool AbstractInterpretation::isBranchFeasible(const IntraCFGEdge* intraEdge,
        AbstractState& as)
{
    const SVFVar *cmpVar = intraEdge->getCondition();
    if (cmpVar->getInEdges().empty())
    {
        return isSwitchBranchFeasible(cmpVar,
                                      intraEdge->getSuccessorCondValue(), as);
    }
    else
    {
        assert(!cmpVar->getInEdges().empty() &&
               "no in edges?");
        SVFStmt *cmpVarInStmt = *cmpVar->getInEdges().begin();
        if (const CmpStmt *cmpStmt = SVFUtil::dyn_cast<CmpStmt>(cmpVarInStmt))
        {
            return isCmpBranchFeasible(cmpStmt,
                                       intraEdge->getSuccessorCondValue(), as);
        }
        else
        {
            return isSwitchBranchFeasible(
                       cmpVar, intraEdge->getSuccessorCondValue(), as);
        }
    }
    return true;
}

/**
 * Handle an ICFG node: execute statements on the given abstract state.
 * The state is modified in-place.
 */
void AbstractInterpretation::handleICFGNode(const ICFGNode* node, AbstractState& as)
{
    stat->getBlockTrace()++;
    stat->getICFGNodeTrace()++;

    // Handle SVF statements
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt, as);
    }

    // Handle call sites
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        handleCallSite(callNode, as);
    }

    // Run detectors
    for (auto& detector: detectors)
        detector->detect(as, node);
    stat->countStateSize();

    // Track this node as analyzed (for coverage statistics across all entry points)
    allAnalyzedNodes.insert(node);
}

/**
 * Handle a function using worklist algorithm guided by WTO order.
 * Returns the exit state of the function.
 */
AbstractState AbstractInterpretation::handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller)
{
    auto it = preAnalysis->getFuncToWTO().find(funEntry->getFun());
    if (it == preAnalysis->getFuncToWTO().end())
        return AbstractState();

    AbstractState exitState;

    // Push all top-level WTO components into the worklist in WTO order
    FIFOWorkList<const ICFGWTOComp*> worklist(it->second->getWTOComponents());

    while (!worklist.empty())
    {
        const ICFGWTOComp* comp = worklist.pop();

        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            const ICFGNode* node = singleton->getICFGNode();
            AbstractState as = mergeInEdges(node);

            // Function entry with no incoming state: inherit from globalState
            if (as == AbstractState() && SVFUtil::isa<FunEntryICFGNode>(node))
                as = globalState;

            // Skip unreachable nodes
            if (as == AbstractState())
                continue;

            handleICFGNode(node, as);
            propagateToSuccessor(node, as);

            // Capture exit state
            if (SVFUtil::isa<FunExitICFGNode>(node))
                exitState = as;
        }
        else if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            handleLoopOrRecursion(cycle, caller);
            // For recursive function cycles, FunExitICFGNode is inside the cycle.
            // After convergence, capture the exit state by merging FunExitICFGNode's
            // in-edges and executing its statements (to get the post-state).
            const ICFGNode* funExit = icfg->getFunExitICFGNode(funEntry->getFun());
            AbstractState exitAs = mergeInEdges(funExit);
            if (!(exitAs == AbstractState()))
            {
                handleICFGNode(funExit, exitAs);
                exitState = exitAs;
            }
        }
    }

    return exitState;
}


void AbstractInterpretation::handleCallSite(const ICFGNode* node, AbstractState& as)
{
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        if (isExtCall(callNode))
        {
            handleExtCall(callNode, as);
        }
        else
        {
            // Handle both direct and indirect calls uniformly
            handleFunCall(callNode, as);
        }
    }
    else
        assert (false && "it is not call node");
}

bool AbstractInterpretation::isExtCall(const CallICFGNode *callNode)
{
    return SVFUtil::isExtCall(callNode->getCalledFunction());
}

void AbstractInterpretation::handleExtCall(const CallICFGNode *callNode, AbstractState& as)
{
    utils->handleExtAPI(callNode, as);
    for (auto& detector : detectors)
    {
        detector->handleStubFunctions(callNode, as);
    }
}

/// Check if a function is recursive (part of a call graph SCC)
bool AbstractInterpretation::isRecursiveFun(const FunObjVar* fun)
{
    return preAnalysis->getPointerAnalysis()->isInRecursion(fun);
}

/// Handle recursive call in TOP mode: set all stores and return value to TOP
void AbstractInterpretation::handleRecursiveCall(const CallICFGNode *callNode, AbstractState& as)
{
    setTopToObjInRecursion(callNode, as);
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() &&
                    !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
            {
                as[retPE->getLHSVarID()] = IntervalValue::top();
            }
        }
    }
    // Write the resulting state to any RetCFGEdge leading to retNode
    // so the return site can pick it up via mergeInEdges.
    for (auto& edge : retNode->getInEdges())
    {
        if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            edgeAbsStates[edge] = as;
            break;
        }
    }
}

/// Check if caller and callee are in the same CallGraph SCC (i.e. a recursive callsite)
bool AbstractInterpretation::isRecursiveCallSite(const CallICFGNode* callNode,
        const FunObjVar* callee)
{
    const FunObjVar* caller = callNode->getCaller();
    return preAnalysis->getPointerAnalysis()->inSameCallGraphSCC(caller, callee);
}

/// Get callee function: directly for direct calls, via pointer analysis for indirect calls
const FunObjVar* AbstractInterpretation::getCallee(const CallICFGNode* callNode, AbstractState& as)
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
    if (!as.inVarToAddrsTable(call_id))
        return nullptr;

    AbstractValue Addrs = as[call_id];
    if (Addrs.getAddrs().empty())
        return nullptr;

    NodeID addr = *Addrs.getAddrs().begin();
    const SVFVar* func_var = getSVFVar(as.getIDFromAddr(addr));
    return SVFUtil::dyn_cast<FunObjVar>(func_var);
}

/// Skip recursive callsites (within SCC); entry calls from outside SCC are not skipped
bool AbstractInterpretation::skipRecursiveCall(const CallICFGNode* callNode, AbstractState& as)
{
    const FunObjVar* callee = getCallee(callNode, as);
    if (!callee)
        return false;

    // Non-recursive function: never skip, always inline
    if (!isRecursiveFun(callee))
        return false;

    // For recursive functions, skip only recursive callsites (within same SCC).
    return isRecursiveCallSite(callNode, callee);
}

/// Check if narrowing should be applied: always for regular loops, mode-dependent for recursion
bool AbstractInterpretation::shouldApplyNarrowing(const FunObjVar* fun)
{
    // Non-recursive functions (regular loops): always apply narrowing
    if (!isRecursiveFun(fun))
        return true;

    // Recursive functions: WIDEN_NARROW applies narrowing, WIDEN_ONLY does not
    switch (Options::HandleRecur())
    {
    case TOP:
        assert(false && "TOP mode should not reach narrowing phase for recursive functions");
        return false;
    case WIDEN_ONLY:
        return false;
    case WIDEN_NARROW:
        return true;
    default:
        assert(false && "Unknown recursion handling mode");
        return false;
    }
}

/// Handle direct or indirect call: get callee(s), process function body, set return state.
void AbstractInterpretation::handleFunCall(const CallICFGNode *callNode, AbstractState& as)
{
    // Skip recursive callsites (within SCC); entry calls are not skipped.
    if (skipRecursiveCall(callNode, as))
    {
        const FunObjVar* callee = getCallee(callNode, as);
        const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
        const ICFGNode* calleeExit = icfg->getFunExitICFGNode(callee);
        // Write to the CallCFGEdge as a back-edge contribution.
        ICFGEdge* callEdge = icfg->getICFGEdge(callNode, calleeEntry, ICFGEdge::CallCF);
        assert(callEdge && "CallCFGEdge must exist for recursive call");
        edgeAbsStates[callEdge] = as;
        // Also write to the RetCFGEdge so the return site picks up the state.
        // The IntraCFGEdge from CallNode to RetNode was removed for inter-procedural
        // calls, so without this the return site would have no incoming state.
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        ICFGEdge* retEdge = icfg->getICFGEdge(calleeExit, retNode, ICFGEdge::RetCF);
        if (retEdge)
            edgeAbsStates[retEdge] = as;
        return;
    }

    // Direct call: callee is known
    if (const FunObjVar* callee = callNode->getCalledFunction())
    {
        const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
        const ICFGNode* calleeExit = icfg->getFunExitICFGNode(callee);
        // Write caller's state to CallCFGEdge so CallPE can copy parameters
        ICFGEdge* callEdge = icfg->getICFGEdge(callNode, calleeEntry, ICFGEdge::CallCF);
        if (callEdge)
            edgeAbsStates[callEdge] = as;
        AbstractState exitState = handleFunction(calleeEntry, callNode);
        // Write exit state to RetCFGEdge so return site can pick it up
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        ICFGEdge* retEdge = icfg->getICFGEdge(calleeExit, retNode, ICFGEdge::RetCF);
        if (retEdge)
        {
            if (!(exitState == AbstractState()))
                edgeAbsStates[retEdge] = exitState;
            else
                edgeAbsStates[retEdge] = as; // fallback to caller state
        }
        return;
    }

    // Indirect call: use Andersen's call graph to get all resolved callees.
    const RetICFGNode* retNode = callNode->getRetICFGNode();
    if (callGraph->hasIndCSCallees(callNode))
    {
        const auto& callees = callGraph->getIndCSCallees(callNode);
        bool firstCallee = true;
        for (const FunObjVar* callee : callees)
        {
            if (callee->isDeclaration())
                continue;
            const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
            const ICFGNode* calleeExit = icfg->getFunExitICFGNode(callee);
            // Write caller's state to CallCFGEdge
            ICFGEdge* callEdge = icfg->getICFGEdge(callNode, calleeEntry, ICFGEdge::CallCF);
            if (callEdge)
                edgeAbsStates[callEdge] = as;
            AbstractState exitState = handleFunction(calleeEntry, callNode);

            ICFGEdge* retEdge = icfg->getICFGEdge(calleeExit, retNode, ICFGEdge::RetCF);
            if (retEdge)
            {
                if (!(exitState == AbstractState()))
                {
                    if (firstCallee)
                    {
                        edgeAbsStates[retEdge] = exitState;
                        firstCallee = false;
                    }
                    else
                        edgeAbsStates[retEdge].joinWith(exitState);
                }
            }
        }
        // If no callee was processed, fall back to caller's state
        if (firstCallee)
        {
            // Find any RetCFGEdge to retNode and write caller state
            for (auto& edge : retNode->getInEdges())
            {
                if (SVFUtil::isa<RetCFGEdge>(edge))
                {
                    edgeAbsStates[edge] = as;
                    break;
                }
            }
        }
    }
    else
    {
        // No callees resolved; fall through — the IntraCFGEdge from callNode
        // to retNode (if it exists) already carries the caller's state via
        // propagateToSuccessor. If no such edge, write to any RetCFGEdge.
        for (auto& edge : retNode->getInEdges())
        {
            if (SVFUtil::isa<RetCFGEdge>(edge))
            {
                edgeAbsStates[edge] = as;
                break;
            }
        }
    }
}

/// Handle WTO cycle (loop or recursive function) using widening/narrowing iteration.
void AbstractInterpretation::collectBodyNodes(const ICFGCycleWTO* cycle, Set<const ICFGNode*>& bodyNodes)
{
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* s = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            bodyNodes.insert(s->getICFGNode());
        else if (const ICFGCycleWTO* sc = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            bodyNodes.insert(sc->head()->getICFGNode());
            collectBodyNodes(sc, bodyNodes);
        }
    }
}

void AbstractInterpretation::handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();

    // TOP mode for recursive function cycles: set all stores and return value to TOP
    if (Options::HandleRecur() == TOP && isRecursiveFun(cycle_head->getFun()))
    {
        if (caller)
        {
            // Merge in-edges to get the caller state at cycle head
            AbstractState callerAs = mergeInEdges(cycle_head);
            if (callerAs == AbstractState())
                callerAs = globalState;
            handleRecursiveCall(caller, callerAs);
        }
        return;
    }

    // Collect all body nodes recursively (including sub-cycle internals).
    Set<const ICFGNode*> bodyNodes;
    collectBodyNodes(cycle, bodyNodes);

    // Identify back-edges: edges from within cycle to cycle head
    Set<const ICFGEdge*> backEdges;
    for (auto& edge : cycle_head->getInEdges())
    {
        const ICFGNode* src = edge->getSrcNode();
        if (src == cycle_head || bodyNodes.count(src))
            backEdges.insert(edge);
    }

    // Iterate until fixpoint with widening/narrowing on the cycle head.
    bool increasing = true;
    u32_t widen_delay = Options::WidenDelay();
    AbstractState prev_head_pre = mergeInEdges(cycle_head);
    if (prev_head_pre == AbstractState() && SVFUtil::isa<FunEntryICFGNode>(cycle_head))
        prev_head_pre = globalState;

    for (u32_t cur_iter = 0;; cur_iter++)
    {
        // Build cycle head's in-state: merge all in-edges (external + back-edges)
        AbstractState head_pre = mergeInEdges(cycle_head);
        if (head_pre == AbstractState() && SVFUtil::isa<FunEntryICFGNode>(cycle_head))
            head_pre = globalState;

        // Apply widening or narrowing
        if (cur_iter >= widen_delay)
        {
            if (increasing)
            {
                AbstractState widened = prev_head_pre.widening(head_pre);
                if (widened == prev_head_pre)
                    increasing = false;
                head_pre = widened;
            }
            else
            {
                if (!shouldApplyNarrowing(cycle_head->getFun()))
                    break;
                AbstractState narrowed = prev_head_pre.narrowing(head_pre);
                if (narrowed == prev_head_pre)
                    break;
                head_pre = narrowed;
            }
        }
        prev_head_pre = head_pre;

        // Clear body node in-edge states to prevent stale states from previous iteration.
        // Must happen BEFORE head propagation so fresh edges from head are not erased.
        for (const ICFGNode* bodyNode : bodyNodes)
        {
            for (auto& edge : bodyNode->getInEdges())
                edgeAbsStates.erase(edge);
        }

        // Execute head and propagate to out-edges
        handleICFGNode(cycle_head, head_pre);
        propagateToSuccessor(cycle_head, head_pre);

        // Process cycle body components
        for (const ICFGWTOComp* comp : cycle->getWTOComponents())
        {
            if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            {
                const ICFGNode* bodyNode = singleton->getICFGNode();
                AbstractState bodyAs = mergeInEdges(bodyNode);
                if (bodyAs == AbstractState())
                    continue;
                handleICFGNode(bodyNode, bodyAs);
                propagateToSuccessor(bodyNode, bodyAs);
                // For FunExitICFGNode within a recursive cycle, also propagate
                // along RetCFGEdges to RetICFGNodes within the cycle.
                // propagateToSuccessor only handles IntraCFGEdges, but within
                // a recursive cycle the RetCFGEdge is an intra-cycle edge.
                if (SVFUtil::isa<FunExitICFGNode>(bodyNode))
                {
                    for (auto& edge : bodyNode->getOutEdges())
                    {
                        if (SVFUtil::isa<RetCFGEdge>(edge) &&
                            bodyNodes.count(edge->getDstNode()))
                        {
                            edgeAbsStates[edge] = bodyAs;
                        }
                    }
                }
            }
            else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                handleLoopOrRecursion(subCycle, caller);
            }
        }
        // Back-edges to cycle_head now have updated states in edgeAbsStates.
        // Next iteration will read them when building head's in-state.
    }
    // After convergence, out-edges of cycle nodes already hold converged states
    // in edgeAbsStates. Downstream nodes will pick them up via mergeInEdges.
}

void AbstractInterpretation::handleSVFStatement(const SVFStmt *stmt, AbstractState& as)
{
    if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
    {
        updateStateOnAddr(addr, as);
    }
    else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
    {
        updateStateOnBinary(binary, as);
    }
    else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
    {
        updateStateOnCmp(cmp, as);
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
        updateStateOnLoad(load, as);
    }
    else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
    {
        updateStateOnStore(store, as);
    }
    else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
    {
        updateStateOnCopy(copy, as);
    }
    else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        updateStateOnGep(gep, as);
    }
    else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt))
    {
        updateStateOnSelect(select, as);
    }
    else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
    {
        updateStateOnPhi(phi, as);
    }
    else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt))
    {
        // To handle Call Edge
        updateStateOnCall(callPE, as);
    }
    else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt))
    {
        updateStateOnRet(retPE, as);
    }
    else
        assert(false && "implement this part");
    // NullPtr is index 0, it should not be changed
    assert(!as[IRGraph::NullPtr].isInterval() &&
           !as[IRGraph::NullPtr].isAddr());
}

/// Set all store values in a recursive function to TOP (used in TOP mode)
void AbstractInterpretation::setTopToObjInRecursion(const CallICFGNode *callNode, AbstractState& as)
{
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() && !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
                as[retPE->getLHSVarID()] = IntervalValue::top();
        }
    }
    if (!retNode->getOutEdges().empty())
    {
        if (retNode->getOutEdges().size() == 1)
        {

        }
        else
        {
            return;
        }
    }
    FIFOWorkList<const SVFBasicBlock *> blkWorkList;
    FIFOWorkList<const ICFGNode *> instWorklist;
    for (const SVFBasicBlock * bb: callNode->getCalledFunction()->getReachableBBs())
    {
        for (const ICFGNode* node: bb->getICFGNodeList())
        {
            for (const SVFStmt *stmt: node->getSVFStmts())
            {
                if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
                {
                    const SVFVar *rhsVar = store->getRHSVar();
                    u32_t lhs = store->getLHSVarID();
                    if (as.inVarToAddrsTable(lhs))
                    {
                        if (!rhsVar->isPointer() && !rhsVar->isConstDataOrAggDataButNotNullPtr())
                        {
                            const AbstractValue &addrs = as[lhs];
                            for (const auto &addr: addrs.getAddrs())
                            {
                                as.store(addr, IntervalValue::top());
                            }
                        }
                    }
                }
            }
        }
    }
}

void AbstractInterpretation::updateStateOnGep(const GepStmt *gep, AbstractState& as)
{
    u32_t rhs = gep->getRHSVarID();
    u32_t lhs = gep->getLHSVarID();
    IntervalValue offsetPair = as.getElementIndex(gep);
    AbstractValue gepAddrs;
    APOffset lb = offsetPair.lb().getIntNumeral() < Options::MaxFieldLimit()?
                  offsetPair.lb().getIntNumeral(): Options::MaxFieldLimit();
    APOffset ub = offsetPair.ub().getIntNumeral() < Options::MaxFieldLimit()?
                  offsetPair.ub().getIntNumeral(): Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
        gepAddrs.join_with(as.getGepObjAddrs(rhs,IntervalValue(i)));
    as[lhs] = gepAddrs;
}

void AbstractInterpretation::updateStateOnSelect(const SelectStmt *select, AbstractState& as)
{
    u32_t res = select->getResID();
    u32_t tval = select->getTrueValue()->getId();
    u32_t fval = select->getFalseValue()->getId();
    u32_t cond = select->getCondition()->getId();
    if (as[cond].getInterval().is_numeral())
    {
        as[res] = as[cond].getInterval().is_zero() ? as[fval] : as[tval];
    }
    else
    {
        as[res] = as[tval];
        as[res].join_with(as[fval]);
    }
}

void AbstractInterpretation::updateStateOnPhi(const PhiStmt *phi, AbstractState& as)
{
    const ICFGNode* icfgNode = phi->getICFGNode();
    u32_t res = phi->getResID();
    AbstractValue rhs;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        NodeID curId = phi->getOpVarID(i);
        const ICFGNode* opICFGNode = phi->getOpICFGNode(i);
        const ICFGEdge* edge = icfg->getICFGEdge(opICFGNode, icfgNode, ICFGEdge::IntraCF);
        if (edge)
        {
            auto it = edgeAbsStates.find(edge);
            if (it != edgeAbsStates.end())
            {
                // Edge state already has branch refinement applied by propagateToSuccessor,
                // which is semantically correct for phi.
                rhs.join_with(it->second[curId]);
            }
        }
    }
    as[res] = rhs;
}


void AbstractInterpretation::updateStateOnCall(const CallPE *callPE, AbstractState& as)
{
    NodeID lhs = callPE->getLHSVarID();
    NodeID rhs = callPE->getRHSVarID();
    as[lhs] = as[rhs];
}

void AbstractInterpretation::updateStateOnRet(const RetPE *retPE, AbstractState& as)
{
    NodeID lhs = retPE->getLHSVarID();
    NodeID rhs = retPE->getRHSVarID();
    as[lhs] = as[rhs];
}


void AbstractInterpretation::updateStateOnAddr(const AddrStmt *addr, AbstractState& as)
{
    as.initObjVar(SVFUtil::cast<ObjVar>(addr->getRHSVar()));
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        as[addr->getRHSVarID()].getInterval().meet_with(utils->getRangeLimitFromType(addr->getRHSVar()->getType()));
    as[addr->getLHSVarID()] = as[addr->getRHSVarID()];
}


void AbstractInterpretation::updateStateOnBinary(const BinaryOPStmt *binary, AbstractState& as)
{
    u32_t op0 = binary->getOpVarID(0);
    u32_t op1 = binary->getOpVarID(1);
    u32_t res = binary->getResID();
    if (!as.inVarToValTable(op0)) as[op0] = IntervalValue::top();
    if (!as.inVarToValTable(op1)) as[op1] = IntervalValue::top();
    IntervalValue &lhs = as[op0].getInterval(), &rhs = as[op1].getInterval();
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
    as[res] = resVal;
}

void AbstractInterpretation::updateStateOnCmp(const CmpStmt *cmp, AbstractState& as)
{
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    // if it is address
    if (as.inVarToAddrsTable(op0) && as.inVarToAddrsTable(op1))
    {
        IntervalValue resVal;
        AddressValue addrOp0 = as[op0].getAddrs();
        AddressValue addrOp1 = as[op1].getAddrs();
        u32_t res = cmp->getResID();
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
        as[res] = resVal;
    }
    // if op0 or op1 is nullptr, compare abstractValue instead of touching addr or interval
    else if (op0 == IRGraph::NullPtr || op1 == IRGraph::NullPtr)
    {
        u32_t res = cmp->getResID();
        IntervalValue resVal = (as[op0].equals(as[op1])) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        as[res] = resVal;
    }
    else
    {
        if (!as.inVarToValTable(op0))
            as[op0] = IntervalValue::top();
        if (!as.inVarToValTable(op1))
            as[op1] = IntervalValue::top();
        u32_t res = cmp->getResID();
        if (as.inVarToValTable(op0) && as.inVarToValTable(op1))
        {
            IntervalValue resVal;
            if (as[op0].isInterval() && as[op1].isInterval())
            {
                IntervalValue &lhs = as[op0].getInterval(),
                               &rhs = as[op1].getInterval();
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
                as[res] = resVal;
            }
            else if (as[op0].isAddr() && as[op1].isAddr())
            {
                AddressValue &lhs = as[op0].getAddrs(),
                              &rhs = as[op1].getAddrs();
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
                as[res] = resVal;
            }
        }
    }
}

void AbstractInterpretation::updateStateOnLoad(const LoadStmt *load, AbstractState& as)
{
    u32_t rhs = load->getRHSVarID();
    u32_t lhs = load->getLHSVarID();
    as[lhs] = as.loadValue(rhs);
}

void AbstractInterpretation::updateStateOnStore(const StoreStmt *store, AbstractState& as)
{
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();
    as.storeValue(lhs, as[rhs]);
}

void AbstractInterpretation::updateStateOnCopy(const CopyStmt *copy, AbstractState& as)
{
    auto getZExtValue = [](AbstractState& as, const SVFVar* var)
    {
        const SVFType* type = var->getType();
        if (SVFUtil::isa<SVFIntegerType>(type))
        {
            u32_t bits = type->getByteSize() * 8;
            if (as[var->getId()].getInterval().is_numeral())
            {
                if (bits == 8)
                {
                    int8_t signed_i8_value = as[var->getId()].getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<uint8_t>(signed_i8_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 16)
                {
                    s16_t signed_i16_value = as[var->getId()].getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u16_t>(signed_i16_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 32)
                {
                    s32_t signed_i32_value = as[var->getId()].getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u32_t>(signed_i32_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 64)
                {
                    s64_t signed_i64_value = as[var->getId()].getInterval().getIntNumeral();
                    return IntervalValue((s64_t)signed_i64_value, (s64_t)signed_i64_value);
                    // we only support i64 at most
                }
                else
                    assert(false && "cannot support int type other than u8/16/32/64");
            }
            else
            {
                return IntervalValue::top(); // TODO: may have better solution
            }
        }
        return IntervalValue::top(); // TODO: may have better solution
    };

    auto getTruncValue = [this](const AbstractState& as, const SVFVar* var,
                             const SVFType* dstType)
    {
        const IntervalValue& itv = as[var->getId()].getInterval();
        if(itv.isBottom()) return itv;
        // get the value of ub and lb
        s64_t int_lb = itv.lb().getIntNumeral();
        s64_t int_ub = itv.ub().getIntNumeral();
        // get dst type
        u32_t dst_bits = dstType->getByteSize() * 8;
        if (dst_bits == 8)
        {
            // get the signed value of ub and lb
            int8_t s8_lb = static_cast<int8_t>(int_lb);
            int8_t s8_ub = static_cast<int8_t>(int_ub);
            if (s8_lb > s8_ub)
            {
                // return range of s8
                return utils->getRangeLimitFromType(dstType);
            }
            return IntervalValue(s8_lb, s8_ub);
        }
        else if (dst_bits == 16)
        {
            // get the signed value of ub and lb
            s16_t s16_lb = static_cast<s16_t>(int_lb);
            s16_t s16_ub = static_cast<s16_t>(int_ub);
            if (s16_lb > s16_ub)
            {
                // return range of s16
                return utils->getRangeLimitFromType(dstType);
            }
            return IntervalValue(s16_lb, s16_ub);
        }
        else if (dst_bits == 32)
        {
            // get the signed value of ub and lb
            s32_t s32_lb = static_cast<s32_t>(int_lb);
            s32_t s32_ub = static_cast<s32_t>(int_ub);
            if (s32_lb > s32_ub)
            {
                // return range of s32
                return utils->getRangeLimitFromType(dstType);
            }
            return IntervalValue(s32_lb, s32_ub);
        }
        else
        {
            assert(false && "cannot support dst int type other than u8/16/32");
            abort();
        }
    };

    u32_t lhs = copy->getLHSVarID();
    u32_t rhs = copy->getRHSVarID();

    if (copy->getCopyKind() == CopyStmt::COPYVAL)
    {
        as[lhs] = as[rhs];
    }
    else if (copy->getCopyKind() == CopyStmt::ZEXT)
    {
        as[lhs] = getZExtValue(as, copy->getRHSVar());
    }
    else if (copy->getCopyKind() == CopyStmt::SEXT)
    {
        as[lhs] = as[rhs].getInterval();
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOSI)
    {
        as[lhs] = as[rhs].getInterval();
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOUI)
    {
        as[lhs] = as[rhs].getInterval();
    }
    else if (copy->getCopyKind() == CopyStmt::SITOFP)
    {
        as[lhs] = as[rhs].getInterval();
    }
    else if (copy->getCopyKind() == CopyStmt::UITOFP)
    {
        as[lhs] = as[rhs].getInterval();
    }
    else if (copy->getCopyKind() == CopyStmt::TRUNC)
    {
        as[lhs] = getTruncValue(as, copy->getRHSVar(), copy->getLHSVar()->getType());
    }
    else if (copy->getCopyKind() == CopyStmt::FPTRUNC)
    {
        as[lhs] = as[rhs].getInterval();
    }
    else if (copy->getCopyKind() == CopyStmt::INTTOPTR)
    {
        //insert nullptr
    }
    else if (copy->getCopyKind() == CopyStmt::PTRTOINT)
    {
        as[lhs] = IntervalValue::top();
    }
    else if (copy->getCopyKind() == CopyStmt::BITCAST)
    {
        if (as[rhs].isAddr())
        {
            as[lhs] = as[rhs];
        }
        else
        {
            // do nothing
        }
    }
    else
        assert(false && "undefined copy kind");
}
