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

using namespace SVF;
using namespace SVFUtil;
using namespace z3;


void AbstractInterpretation::runOnModule(ICFG *_icfg)
{
    stat->startClk();
    icfg = _icfg;
    svfir = PAG::getPAG();
    utils = new AbsExtAPI(abstractTrace);

    /// collect checkpoint
    collectCheckPoint();

    analyse();
    checkPointAllSet();
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
    for (auto it: funcToWTO)
        delete it.second;
}

/**
 * @brief Recursively collect cycle heads from nested WTO components
 *
 * This helper function traverses the WTO component tree and builds the cycleHeadToCycle
 * map, which maps each cycle head node to its corresponding ICFGCycleWTO object.
 * This enables efficient O(1) lookup of cycles during analysis.
 */
void AbstractInterpretation::collectCycleHeads(const std::list<const ICFGWTOComp*>& comps)
{
    for (const ICFGWTOComp* comp : comps)
    {
        if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            cycleHeadToCycle[cycle->head()->getICFGNode()] = cycle;
            // Recursively collect nested cycle heads
            collectCycleHeads(cycle->getWTOComponents());
        }
    }
}

void AbstractInterpretation::initWTO()
{
    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(svfir);
    // Detect if the call graph has cycles by finding its strongly connected components (SCC)
    Andersen::CallGraphSCC* callGraphScc = ander->getCallGraphSCC();
    callGraphScc->find();
    CallGraph* callGraph = ander->getCallGraph();

    // Iterate through the call graph
    for (auto it = callGraph->begin(); it != callGraph->end(); it++)
    {
        // Check if the current function is part of a cycle
        if (callGraphScc->isInCycle(it->second->getId()))
            recursiveFuns.insert(it->second->getFunction()); // Mark the function as recursive

        // Calculate ICFGWTO for each function/recursion
        const FunObjVar *fun = it->second->getFunction();
        if (fun->isDeclaration())
            continue;

        NodeID repNodeId = callGraphScc->repNode(it->second->getId());
        auto cgSCCNodes = callGraphScc->subNodes(repNodeId);

        // Identify if this node is an SCC entry (nodes who have incoming edges
        // from nodes outside the SCC). Also identify non-recursive callsites.
        bool isEntry = false;
        if (it->second->getInEdges().empty())
            isEntry = true;
        for (auto inEdge: it->second->getInEdges())
        {
            NodeID srcNodeId = inEdge->getSrcID();
            if (!cgSCCNodes.test(srcNodeId))
            {
                isEntry = true;
                const CallICFGNode *callSite = nullptr;
                if (inEdge->isDirectCallEdge())
                    callSite = *(inEdge->getDirectCalls().begin());
                else if (inEdge->isIndirectCallEdge())
                    callSite = *(inEdge->getIndirectCalls().begin());
                else
                    assert(false && "CallGraphEdge must "
                           "be either direct or indirect!");

                nonRecursiveCallSites.insert(
                {callSite, inEdge->getDstNode()->getFunction()->getId()});
            }
        }

        // Compute IWTO for the function partition entered from each partition entry
        if (isEntry)
        {
            Set<const FunObjVar*> funcScc;
            for (const auto& node: cgSCCNodes)
            {
                funcScc.insert(callGraph->getGNode(node)->getFunction());
            }
            ICFGWTO* iwto = new ICFGWTO(icfg->getFunEntryICFGNode(fun), funcScc);
            iwto->init();
            funcToWTO[it->second->getFunction()] = iwto;
        }
    }

    // Build cycleHeadToCycle map for all functions
    // This maps cycle head nodes to their corresponding WTO cycles for efficient lookup
    for (auto& [func, wto] : funcToWTO)
    {
        collectCycleHeads(wto->getWTOComponents());
    }
}

/// Program entry
void AbstractInterpretation::analyse()
{
    initWTO();
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    getAbsStateFromTrace(
        icfg->getGlobalICFGNode())[PAG::getPAG()->getBlkPtr()] = IntervalValue::top();
    if (const CallGraphNode* cgn = svfir->getCallGraph()->getCallGraphNode("main"))
    {
        // Use worklist-based function handling instead of recursive WTO component handling
        const ICFGNode* mainEntry = icfg->getFunEntryICFGNode(cgn->getFunction());
        handleFunction(mainEntry);
    }
}

/// handle global node
void AbstractInterpretation::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    abstractTrace[node] = AbstractState();
    abstractTrace[node][IRGraph::NullPtr] = AddressValue();
    // Global Node, we just need to handle addr, load, store, copy and gep
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }
}

/// get execution state by merging states of predecessor blocks
/// Scenario 1: preblock -----(intraEdge)----> block, join the preES of inEdges
/// Scenario 2: preblock -----(callEdge)----> block
bool AbstractInterpretation::mergeStatesFromPredecessors(const ICFGNode * icfgNode)
{
    std::vector<AbstractState> workList;
    AbstractState preAs;
    for (auto& edge: icfgNode->getInEdges())
    {
        if (abstractTrace.find(edge->getSrcNode()) != abstractTrace.end())
        {

            if (const IntraCFGEdge *intraCfgEdge =
                        SVFUtil::dyn_cast<IntraCFGEdge>(edge))
            {
                AbstractState tmpEs = abstractTrace[edge->getSrcNode()];
                if (intraCfgEdge->getCondition())
                {
                    if (isBranchFeasible(intraCfgEdge, tmpEs))
                    {
                        workList.push_back(tmpEs);
                    }
                    else
                    {
                        // do nothing
                    }
                }
                else
                {
                    workList.push_back(tmpEs);
                }
            }
            else if (const CallCFGEdge *callCfgEdge =
                         SVFUtil::dyn_cast<CallCFGEdge>(edge))
            {

                // context insensitive implementation
                workList.push_back(
                    abstractTrace[callCfgEdge->getSrcNode()]);
            }
            else if (const RetCFGEdge *retCfgEdge =
                         SVFUtil::dyn_cast<RetCFGEdge>(edge))
            {
                // Only include return edge if the corresponding callsite was processed
                // (skipped recursive callsites in WIDEN_ONLY/WIDEN_NARROW won't have state)
                const RetICFGNode* retNode = SVFUtil::dyn_cast<RetICFGNode>(icfgNode);
                if (hasAbsStateFromTrace(retNode->getCallICFGNode()))
                {
                    workList.push_back(abstractTrace[retCfgEdge->getSrcNode()]);
                }
            }
            else
                assert(false && "Unhandled ICFGEdge type encountered!");
        }
    }
    if (workList.size() == 0)
    {
        return false;
    }
    else
    {
        while (!workList.empty())
        {
            preAs.joinWith(workList.back());
            workList.pop_back();
        }
        // Has ES on the in edges - Feasible block
        // update post as
        abstractTrace[icfgNode] = preAs;
        return true;
    }
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
    SVFVar* loadVar0 = svfir->getGNode(op0);
    if (!loadVar0->getInEdges().empty())
    {
        SVFStmt *loadVar0InStmt = *loadVar0->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt))
        {
            load_op0 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar0InStmt))
        {
            loadVar0 = svfir->getGNode(copyStmt->getRHSVarID());
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

    SVFVar* loadVar1 = svfir->getGNode(op1);
    if (!loadVar1->getInEdges().empty())
    {
        SVFStmt *loadVar1InStmt = *loadVar1->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt))
        {
            load_op1 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar1InStmt))
        {
            loadVar1 = svfir->getGNode(copyStmt->getRHSVarID());
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
/// handle instructions in svf basic blocks
void AbstractInterpretation::handleSingletonWTO(const ICFGSingletonWTO *icfgSingletonWto)
{
    const ICFGNode* node = icfgSingletonWto->getICFGNode();
    stat->getBlockTrace()++;

    std::deque<const ICFGNode*> worklist;

    stat->getICFGNodeTrace()++;
    // handle SVF Stmt
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }
    // inlining the callee by calling handleFunc for the callee function
    if (const CallICFGNode* callnode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        handleCallSite(callnode);
    }
    for (auto& detector: detectors)
        detector->detect(getAbsStateFromTrace(node), node);
    stat->countStateSize();
}

/**
 * Handle an ICFG node by merging states from predecessors and processing statements
 * Returns true if the abstract state has changed, false if fixpoint reached or infeasible
 */
bool AbstractInterpretation::handleICFGNode(const ICFGNode* node)
{
    // Store the previous state for fixpoint detection
    AbstractState prevState;
    bool hadPrevState = hasAbsStateFromTrace(node);
    if (hadPrevState)
        prevState = abstractTrace[node];

    // For function entry nodes, initialize state from caller or global
    bool isFunEntry = SVFUtil::isa<FunEntryICFGNode>(node);
    if (isFunEntry)
    {
        // Try to merge from predecessors first (handles call edges)
        if (!mergeStatesFromPredecessors(node))
        {
            // No predecessors with state - initialize from caller or global
            if (!callSiteStack.empty())
            {
                // Get state from the most recent call site
                const CallICFGNode* caller = callSiteStack.back();
                if (hasAbsStateFromTrace(caller))
                {
                    abstractTrace[node] = abstractTrace[caller];
                }
                else
                {
                    abstractTrace[node] = AbstractState();
                }
            }
            else
            {
                // This is the main function entry, inherit from global node
                const ICFGNode* globalNode = icfg->getGlobalICFGNode();
                if (hasAbsStateFromTrace(globalNode))
                {
                    abstractTrace[node] = abstractTrace[globalNode];
                }
                else
                {
                    abstractTrace[node] = AbstractState();
                }
            }
        }
    }
    else
    {
        // Merge states from predecessors
        if (!mergeStatesFromPredecessors(node))
            return false;
    }

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
        detector->detect(getAbsStateFromTrace(node), node);
    stat->countStateSize();

    // Check if state changed (for fixpoint detection)
    // For entry nodes on first visit, always return true to process successors
    if (isFunEntry && !hadPrevState)
        return true;

    if (abstractTrace[node] == prevState)
        return false;

    return true;
}

/**
 * Get the next nodes of a node within the same function
 * For CallICFGNode, includes shortcut to RetICFGNode
 */
std::vector<const ICFGNode*> AbstractInterpretation::getNextNodes(const ICFGNode* node) const
{
    std::vector<const ICFGNode*> outEdges;
    for (const ICFGEdge* edge : node->getOutEdges())
    {
        const ICFGNode* dst = edge->getDstNode();
        // Only nodes inside the same function are included
        if (dst->getFun() == node->getFun())
        {
            outEdges.push_back(dst);
        }
    }
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        // Shortcut to the RetICFGNode
        const ICFGNode* retNode = callNode->getRetICFGNode();
        outEdges.push_back(retNode);
    }
    return outEdges;
}

/**
 * Get the next nodes outside a cycle
 * Inner cycles are skipped since their next nodes cannot be outside the outer cycle
 */
std::vector<const ICFGNode*> AbstractInterpretation::getNextNodesOfCycle(const ICFGCycleWTO* cycle) const
{
    Set<const ICFGNode*> cycleNodes;
    // Insert the head of the cycle and all component nodes
    cycleNodes.insert(cycle->head()->getICFGNode());
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            cycleNodes.insert(singleton->getICFGNode());
        }
        else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            cycleNodes.insert(subCycle->head()->getICFGNode());
        }
    }

    std::vector<const ICFGNode*> outEdges;

    // Check head's successors
    std::vector<const ICFGNode*> nextNodes = getNextNodes(cycle->head()->getICFGNode());
    for (const ICFGNode* nextNode : nextNodes)
    {
        // Only nodes that point outside the cycle are included
        if (cycleNodes.find(nextNode) == cycleNodes.end())
        {
            outEdges.push_back(nextNode);
        }
    }

    // Check each component's successors
    for (const ICFGWTOComp* comp : cycle->getWTOComponents())
    {
        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            std::vector<const ICFGNode*> compNextNodes = getNextNodes(singleton->getICFGNode());
            for (const ICFGNode* nextNode : compNextNodes)
            {
                if (cycleNodes.find(nextNode) == cycleNodes.end())
                {
                    outEdges.push_back(nextNode);
                }
            }
        }
        // Skip inner cycles - they are handled within the outer cycle
    }
    return outEdges;
}

/**
 * Handle a function using worklist algorithm
 * This replaces the recursive WTO component handling with explicit worklist iteration
 */
void AbstractInterpretation::handleFunction(const ICFGNode* funEntry)
{
    FIFOWorkList<const ICFGNode*> worklist;
    worklist.push(funEntry);

    while (!worklist.empty())
    {
        const ICFGNode* node = worklist.pop();

        // Check if this node is a cycle head
        if (cycleHeadToCycle.find(node) != cycleHeadToCycle.end())
        {
            const ICFGCycleWTO* cycle = cycleHeadToCycle[node];
            handleICFGCycle(cycle);

            // Push nodes outside the cycle to the worklist
            std::vector<const ICFGNode*> cycleNextNodes = getNextNodesOfCycle(cycle);
            for (const ICFGNode* nextNode : cycleNextNodes)
            {
                worklist.push(nextNode);
            }
        }
        else
        {
            // Handle regular node
            if (!handleICFGNode(node))
            {
                // Fixpoint reached or infeasible, skip successors
                continue;
            }

            // Push successor nodes to the worklist
            std::vector<const ICFGNode*> nextNodes = getNextNodes(node);
            for (const ICFGNode* nextNode : nextNodes)
            {
                worklist.push(nextNode);
            }
        }
    }
}


void AbstractInterpretation::handleCallSite(const ICFGNode* node)
{
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        if (isExtCall(callNode))
        {
            extCallPass(callNode);
        }
        else if (isDirectCall(callNode))
        {
            directCallFunPass(callNode);
        }
        else if (isIndirectCall(callNode))
        {
            indirectCallFunPass(callNode);
        }
        else
            assert(false && "implement this part");
    }
    else
        assert (false && "it is not call node");
}

bool AbstractInterpretation::isExtCall(const CallICFGNode *callNode)
{
    return SVFUtil::isExtCall(callNode->getCalledFunction());
}

void AbstractInterpretation::extCallPass(const CallICFGNode *callNode)
{
    callSiteStack.push_back(callNode);
    utils->handleExtAPI(callNode);
    for (auto& detector : detectors)
    {
        detector->handleStubFunctions(callNode);
    }
    callSiteStack.pop_back();
}

bool AbstractInterpretation::isRecursiveFun(const FunObjVar* fun)
{
    return recursiveFuns.find(fun) != recursiveFuns.end();
}

bool AbstractInterpretation::isRecursiveCall(const CallICFGNode *callNode)
{
    const FunObjVar *callfun = callNode->getCalledFunction();
    if (!callfun)
        return false;
    else
        return isRecursiveFun(callfun);
}

void AbstractInterpretation::recursiveCallPass(const CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);
    setRecursiveCallStoresToTop(callNode);
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
    abstractTrace[retNode] = as;
}

bool AbstractInterpretation::isRecursiveCallSite(const CallICFGNode* callNode,
        const FunObjVar* callee)
{
    return nonRecursiveCallSites.find({callNode, callee->getId()}) ==
           nonRecursiveCallSites.end();
}

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
    if (!hasAbsStateFromTrace(callNode))
        return nullptr;

    AbstractState& as = getAbsStateFromTrace(callNode);
    if (!as.inVarToAddrsTable(call_id))
        return nullptr;

    AbstractValue Addrs = as[call_id];
    if (Addrs.getAddrs().empty())
        return nullptr;

    NodeID addr = *Addrs.getAddrs().begin();
    SVFVar* func_var = svfir->getGNode(as.getIDFromAddr(addr));
    return SVFUtil::dyn_cast<FunObjVar>(func_var);
}

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
    // handleICFGCycle() can analyze the function body.
    // This applies uniformly to all modes (TOP/WIDEN_ONLY/WIDEN_NARROW).
    return isRecursiveCallSite(callNode, callee);
}

bool AbstractInterpretation::shouldApplyNarrowing(const FunObjVar* fun)
{
    // Non-recursive functions (regular loops): always apply narrowing
    if (!isRecursiveFun(fun))
        return true;

    // Recursive functions: depends on mode
    // Note: TOP mode handles recursive cycles separately in handleICFGCycle,
    // so this should not be reached for recursive functions in TOP mode.
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


bool AbstractInterpretation::isDirectCall(const CallICFGNode *callNode)
{
    const FunObjVar *callfun =callNode->getCalledFunction();
    if (!callfun)
        return false;
    else
        return !callfun->isDeclaration();
}
void AbstractInterpretation::directCallFunPass(const CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);

    abstractTrace[callNode] = as;

    // Skip recursive call if applicable (returns true if skipped)
    if (skipRecursiveCall(callNode))
        return;

    const FunObjVar *calleeFun = callNode->getCalledFunction();
    callSiteStack.push_back(callNode);

    // Use worklist-based function handling instead of recursive WTO component handling
    const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(calleeFun);
    handleFunction(calleeEntry);

    callSiteStack.pop_back();
    // handle Ret node
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    // resume ES to callnode
    abstractTrace[retNode] = abstractTrace[callNode];
}

bool AbstractInterpretation::isIndirectCall(const CallICFGNode *callNode)
{
    const auto callsiteMaps = svfir->getIndirectCallsites();
    return callsiteMaps.find(callNode) != callsiteMaps.end();
}

void AbstractInterpretation::indirectCallFunPass(const CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);

    // Skip recursive call if applicable (returns true if skipped)
    if (skipRecursiveCall(callNode))
        return;

    const FunObjVar* callee = getCallee(callNode);
    if (!callee)
        return;

    callSiteStack.push_back(callNode);
    abstractTrace[callNode] = as;

    // Use worklist-based function handling instead of recursive WTO component handling
    const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
    handleFunction(calleeEntry);

    callSiteStack.pop_back();
    // handle Ret node
    const RetICFGNode* retNode = callNode->getRetICFGNode();
    abstractTrace[retNode] = abstractTrace[callNode];
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
///     Does not iterate. Calls recursiveCallPass() to set all stores and
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
void AbstractInterpretation::handleICFGCycle(const ICFGCycleWTO* cycle)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();

    // TOP mode for recursive function cycles: use recursiveCallPass to set
    // all stores and return value to TOP, maintaining original semantics
    if (Options::HandleRecur() == TOP && isRecursiveFun(cycle_head->getFun()))
    {
        // Get the call node from callSiteStack (the call that entered this function)
        if (!callSiteStack.empty())
        {
            const CallICFGNode* callNode = callSiteStack.back();
            recursiveCallPass(callNode);
        }
        return;
    }

    // WIDEN_ONLY / WIDEN_NARROW modes (and regular loops): iterate until fixpoint
    bool increasing = true;
    u32_t widen_delay = Options::WidenDelay();

    for (u32_t cur_iter = 0;; cur_iter++)
    {
        // Get the abstract state before processing the cycle head
        AbstractState prev_head_state;
        if (hasAbsStateFromTrace(cycle_head))
            prev_head_state = abstractTrace[cycle_head];

        // Process the cycle head node
        handleICFGNode(cycle_head);
        AbstractState cur_head_state = abstractTrace[cycle_head];

        // Start widening or narrowing if cur_iter >= widen delay threshold
        if (cur_iter >= widen_delay)
        {
            if (increasing)
            {
                // Apply widening operator
                abstractTrace[cycle_head] = prev_head_state.widening(cur_head_state);

                if (abstractTrace[cycle_head] == prev_head_state)
                {
                    // Widening fixpoint reached, switch to narrowing phase
                    increasing = false;
                    continue;
                }
            }
            else
            {
                // Narrowing phase - check if narrowing should be applied
                if (!shouldApplyNarrowing(cycle_head->getFun()))
                {
                    break;
                }

                // Apply narrowing
                abstractTrace[cycle_head] = prev_head_state.narrowing(cur_head_state);
                if (abstractTrace[cycle_head] == prev_head_state)
                {
                    // Narrowing fixpoint reached, exit loop
                    break;
                }
            }
        }

        // Process cycle body components
        for (const ICFGWTOComp* comp : cycle->getWTOComponents())
        {
            if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            {
                handleICFGNode(singleton->getICFGNode());
            }
            else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                // Handle nested cycle recursively
                handleICFGCycle(subCycle);
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
    // NullPtr is index 0, it should not be changed
    assert(!getAbsStateFromTrace(stmt->getICFGNode())[IRGraph::NullPtr].isInterval() &&
           !getAbsStateFromTrace(stmt->getICFGNode())[IRGraph::NullPtr].isAddr());
}

void AbstractInterpretation::setRecursiveCallStoresToTop(const CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            AbstractState as;
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

// count the size of memory map
void AEStat::countStateSize()
{
    if (count == 0)
    {
        generalNumMap["ES_Var_AVG_Num"] = 0;
        generalNumMap["ES_Loc_AVG_Num"] = 0;
        generalNumMap["ES_Var_Addr_AVG_Num"] = 0;
        generalNumMap["ES_Loc_Addr_AVG_Num"] = 0;
    }
    ++count;
}

void AEStat::finializeStat()
{
    memUsage = getMemUsage();
    if (count > 0)
    {
        generalNumMap["ES_Var_AVG_Num"] /= count;
        generalNumMap["ES_Loc_AVG_Num"] /= count;
        generalNumMap["ES_Var_Addr_AVG_Num"] /= count;
        generalNumMap["ES_Loc_Addr_AVG_Num"] /= count;
    }
    generalNumMap["SVF_STMT_NUM"] = count;
    generalNumMap["ICFG_Node_Num"] = _ae->svfir->getICFG()->nodeNum;
    u32_t callSiteNum = 0;
    u32_t extCallSiteNum = 0;
    Set<const FunObjVar *> funs;
    for (const auto &it: *_ae->svfir->getICFG())
    {
        if (it.second->getFun())
        {
            funs.insert(it.second->getFun());
        }
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(it.second))
        {
            if (!isExtCall(callNode))
            {
                callSiteNum++;
            }
            else
            {
                extCallSiteNum++;
            }
        }
    }
    generalNumMap["Func_Num"] = funs.size();
    generalNumMap["EXT_CallSite_Num"] = extCallSiteNum;
    generalNumMap["NonEXT_CallSite_Num"] = callSiteNum;
    timeStatMap["Total_Time(sec)"] = (double)(endTime - startTime) / TIMEINTERVAL;

}

void AEStat::performStat()
{
    std::string fullName(_ae->moduleName);
    std::string name;
    std::string moduleName;
    if (fullName.find('/') == std::string::npos)
    {
        std::string name = fullName;
        moduleName = name.substr(0, fullName.find('.'));
    }
    else
    {
        std::string name = fullName.substr(fullName.find('/'), fullName.size());
        moduleName = name.substr(0, fullName.find('.'));
    }

    SVFUtil::outs() << "\n************************\n";
    SVFUtil::outs() << "################ (program : " << moduleName << ")###############\n";
    SVFUtil::outs().flags(std::ios::left);
    unsigned field_width = 30;
    for (NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it != eit; ++it)
    {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "-------------------------------------------------------\n";
    for (TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it != eit; ++it)
    {
        // format out put with width 20 space
        SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "Memory usage: " << memUsage << "\n";

    SVFUtil::outs() << "#######################################################" << std::endl;
    SVFUtil::outs().flush();
}

void AbstractInterpretation::collectCheckPoint()
{
    // traverse every ICFGNode
    Set<std::string> ae_checkpoint_names = {"svf_assert"};
    Set<std::string> buf_checkpoint_names = {"UNSAFE_BUFACCESS", "SAFE_BUFACCESS"};
    Set<std::string> nullptr_checkpoint_names = {"UNSAFE_LOAD", "SAFE_LOAD"};

    for (auto it = svfir->getICFG()->begin(); it != svfir->getICFG()->end(); ++it)
    {
        const ICFGNode* node = it->second;
        if (const CallICFGNode *call = SVFUtil::dyn_cast<CallICFGNode>(node))
        {
            if (const FunObjVar *fun = call->getCalledFunction())
            {
                if (ae_checkpoint_names.find(fun->getName()) !=
                        ae_checkpoint_names.end())
                {
                    checkpoints.insert(call);
                }
                if (Options::BufferOverflowCheck())
                {
                    if (buf_checkpoint_names.find(fun->getName()) !=
                            buf_checkpoint_names.end())
                    {
                        checkpoints.insert(call);
                    }
                }
                if (Options::NullDerefCheck())
                {
                    if (nullptr_checkpoint_names.find(fun->getName()) !=
                            nullptr_checkpoint_names.end())
                    {
                        checkpoints.insert(call);
                    }
                }
            }
        }
    }
}

void AbstractInterpretation::checkPointAllSet()
{
    if (checkpoints.size() == 0)
    {
        return;
    }
    else
    {
        SVFUtil::errs() << SVFUtil::errMsg("At least one svf_assert has not been checked!!") << "\n";
        for (const CallICFGNode* call: checkpoints)
            SVFUtil::errs() << call->toString() + "\n";
        assert(false);
    }

}

void AbstractInterpretation::updateStateOnGep(const GepStmt *gep)
{
    AbstractState& as = getAbsStateFromTrace(gep->getICFGNode());
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

void AbstractInterpretation::updateStateOnSelect(const SelectStmt *select)
{
    AbstractState& as = getAbsStateFromTrace(select->getICFGNode());
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

void AbstractInterpretation::updateStateOnPhi(const PhiStmt *phi)
{
    const ICFGNode* icfgNode = phi->getICFGNode();
    AbstractState& as = getAbsStateFromTrace(icfgNode);
    u32_t res = phi->getResID();
    AbstractValue rhs;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        NodeID curId = phi->getOpVarID(i);
        const ICFGNode* opICFGNode = phi->getOpICFGNode(i);
        if (hasAbsStateFromTrace(opICFGNode))
        {
            AbstractState tmpEs = abstractTrace[opICFGNode];
            AbstractState& opAs = getAbsStateFromTrace(opICFGNode);
            const ICFGEdge* edge =  icfg->getICFGEdge(opICFGNode, icfgNode, ICFGEdge::IntraCF);
            // if IntraEdge, check the condition, if it is feasible, join the value
            // if IntraEdge but not conditional edge, join the value
            // if not IntraEdge, join the value
            if (edge)
            {
                const IntraCFGEdge* intraEdge = SVFUtil::cast<IntraCFGEdge>(edge);
                if (intraEdge->getCondition())
                {
                    if (isBranchFeasible(intraEdge, tmpEs))
                        rhs.join_with(opAs[curId]);
                }
                else
                    rhs.join_with(opAs[curId]);
            }
            else
            {
                rhs.join_with(opAs[curId]);
            }
        }
    }
    as[res] = rhs;
}


void AbstractInterpretation::updateStateOnCall(const CallPE *callPE)
{
    AbstractState& as = getAbsStateFromTrace(callPE->getICFGNode());
    NodeID lhs = callPE->getLHSVarID();
    NodeID rhs = callPE->getRHSVarID();
    as[lhs] = as[rhs];
}

void AbstractInterpretation::updateStateOnRet(const RetPE *retPE)
{
    AbstractState& as = getAbsStateFromTrace(retPE->getICFGNode());
    NodeID lhs = retPE->getLHSVarID();
    NodeID rhs = retPE->getRHSVarID();
    as[lhs] = as[rhs];
}


void AbstractInterpretation::updateStateOnAddr(const AddrStmt *addr)
{
    AbstractState& as = getAbsStateFromTrace(addr->getICFGNode());
    as.initObjVar(SVFUtil::cast<ObjVar>(addr->getRHSVar()));
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        as[addr->getRHSVarID()].getInterval().meet_with(utils->getRangeLimitFromType(addr->getRHSVar()->getType()));
    as[addr->getLHSVarID()] = as[addr->getRHSVarID()];
}


void AbstractInterpretation::updateStateOnBinary(const BinaryOPStmt *binary)
{
    /// Find the comparison predicates in "class BinaryOPStmt:OpCode" under SVF/svf/include/SVFIR/SVFStatements.h
    /// You are only required to handle integer predicates, including Add, FAdd, Sub, FSub, Mul, FMul, SDiv, FDiv, UDiv,
    /// SRem, FRem, URem, Xor, And, Or, AShr, Shl, LShr
    const ICFGNode* node = binary->getICFGNode();
    AbstractState& as = getAbsStateFromTrace(node);
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

void AbstractInterpretation::updateStateOnCmp(const CmpStmt *cmp)
{
    AbstractState& as = getAbsStateFromTrace(cmp->getICFGNode());
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
                default:
                    assert(false && "undefined compare: ");
                }
                as[res] = resVal;
            }
        }
    }
}

void AbstractInterpretation::updateStateOnLoad(const LoadStmt *load)
{
    AbstractState& as = getAbsStateFromTrace(load->getICFGNode());
    u32_t rhs = load->getRHSVarID();
    u32_t lhs = load->getLHSVarID();
    as[lhs] = as.loadValue(rhs);
}

void AbstractInterpretation::updateStateOnStore(const StoreStmt *store)
{
    AbstractState& as = getAbsStateFromTrace(store->getICFGNode());
    u32_t rhs = store->getRHSVarID();
    u32_t lhs = store->getLHSVarID();
    as.storeValue(lhs, as[rhs]);
}

void AbstractInterpretation::updateStateOnCopy(const CopyStmt *copy)
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

    auto getTruncValue = [&](const AbstractState& as, const SVFVar* var,
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

    AbstractState& as = getAbsStateFromTrace(copy->getICFGNode());
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



