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
// Created by Jiawei Wang on 2024/1/10.
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "Util/WorkList.h"
#include <cmath>

using namespace SVF;
using namespace SVFUtil;
using namespace z3;

// according to varieties of cmp insts,
// maybe var X var, var X const, const X var, const X const
// we accept 'var X const' 'var X var' 'const X const'
// if 'const X var', we need to reverse op0 op1 and its predicate 'var X' const'
// X' is reverse predicate of X
// == -> !=, != -> ==, > -> <=, >= -> <, < -> >=, <= -> >

Map<s32_t, s32_t> _reverse_predicate =
{
    {CmpStmt::Predicate::FCMP_OEQ, CmpStmt::Predicate::FCMP_ONE},  // == -> !=
    {CmpStmt::Predicate::FCMP_UEQ, CmpStmt::Predicate::FCMP_UNE},  // == -> !=
    {CmpStmt::Predicate::FCMP_OGT, CmpStmt::Predicate::FCMP_OLE},  // > -> <=
    {CmpStmt::Predicate::FCMP_OGE, CmpStmt::Predicate::FCMP_OLT},  // >= -> <
    {CmpStmt::Predicate::FCMP_OLT, CmpStmt::Predicate::FCMP_OGE},  // < -> >=
    {CmpStmt::Predicate::FCMP_OLE, CmpStmt::Predicate::FCMP_OGT},  // <= -> >
    {CmpStmt::Predicate::FCMP_ONE, CmpStmt::Predicate::FCMP_OEQ},  // != -> ==
    {CmpStmt::Predicate::FCMP_UNE, CmpStmt::Predicate::FCMP_UEQ},  // != -> ==
    {CmpStmt::Predicate::ICMP_EQ, CmpStmt::Predicate::ICMP_NE},  // == -> !=
    {CmpStmt::Predicate::ICMP_NE, CmpStmt::Predicate::ICMP_EQ},  // != -> ==
    {CmpStmt::Predicate::ICMP_UGT, CmpStmt::Predicate::ICMP_ULE},  // > -> <=
    {CmpStmt::Predicate::ICMP_ULT, CmpStmt::Predicate::ICMP_UGE},  // < -> >=
    {CmpStmt::Predicate::ICMP_UGE, CmpStmt::Predicate::ICMP_ULT},  // >= -> <
    {CmpStmt::Predicate::ICMP_SGT, CmpStmt::Predicate::ICMP_SLE},  // > -> <=
    {CmpStmt::Predicate::ICMP_SLT, CmpStmt::Predicate::ICMP_SGE},  // < -> >=
    {CmpStmt::Predicate::ICMP_SGE, CmpStmt::Predicate::ICMP_SLT},  // >= -> <
};


Map<s32_t, s32_t> _switch_lhsrhs_predicate =
{
    {CmpStmt::Predicate::FCMP_OEQ, CmpStmt::Predicate::FCMP_OEQ},  // == -> ==
    {CmpStmt::Predicate::FCMP_UEQ, CmpStmt::Predicate::FCMP_UEQ},  // == -> ==
    {CmpStmt::Predicate::FCMP_OGT, CmpStmt::Predicate::FCMP_OLT},  // > -> <
    {CmpStmt::Predicate::FCMP_OGE, CmpStmt::Predicate::FCMP_OLE},  // >= -> <=
    {CmpStmt::Predicate::FCMP_OLT, CmpStmt::Predicate::FCMP_OGT},  // < -> >
    {CmpStmt::Predicate::FCMP_OLE, CmpStmt::Predicate::FCMP_OGE},  // <= -> >=
    {CmpStmt::Predicate::FCMP_ONE, CmpStmt::Predicate::FCMP_ONE},  // != -> !=
    {CmpStmt::Predicate::FCMP_UNE, CmpStmt::Predicate::FCMP_UNE},  // != -> !=
    {CmpStmt::Predicate::ICMP_EQ, CmpStmt::Predicate::ICMP_EQ},  // == -> ==
    {CmpStmt::Predicate::ICMP_NE, CmpStmt::Predicate::ICMP_NE},  // != -> !=
    {CmpStmt::Predicate::ICMP_UGT, CmpStmt::Predicate::ICMP_ULT},  // > -> <
    {CmpStmt::Predicate::ICMP_ULT, CmpStmt::Predicate::ICMP_UGT},  // < -> >
    {CmpStmt::Predicate::ICMP_UGE, CmpStmt::Predicate::ICMP_ULE},  // >= -> <=
    {CmpStmt::Predicate::ICMP_SGT, CmpStmt::Predicate::ICMP_SLT},  // > -> <
    {CmpStmt::Predicate::ICMP_SLT, CmpStmt::Predicate::ICMP_SGT},  // < -> >
    {CmpStmt::Predicate::ICMP_SGE, CmpStmt::Predicate::ICMP_SLE},  // >= -> <=
};


void AbstractInterpretation::runOnModule(ICFG *_icfg)
{
    stat->startClk();
    icfg = _icfg;
    svfir = PAG::getPAG();

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
    initExtFunMap();
}
/// Destructor
AbstractInterpretation::~AbstractInterpretation()
{
    delete stat;
    for (auto it: funcToWTO)
        delete it.second;

}

/**
 * @brief Mark recursive functions in the call graph
 *
 * This function identifies and marks recursive functions in the call graph.
 * It does this by detecting cycles in the call graph's strongly connected components (SCC).
 * Any function found to be part of a cycle is marked as recursive.
 */
void AbstractInterpretation::initWTO()
{
    AndersenWaveDiff* ander = AndersenWaveDiff::createAndersenWaveDiff(svfir);
    // Detect if the call graph has cycles by finding its strongly connected components (SCC)
    Andersen::CallGraphSCC* callGraphScc = ander->getCallGraphSCC();
    callGraphScc->find();
    auto callGraph = ander->getCallGraph();

    // Iterate through the call graph
    for (auto it = callGraph->begin(); it != callGraph->end(); it++)
    {
        // Check if the current function is part of a cycle
        if (callGraphScc->isInCycle(it->second->getId()))
            recursiveFuns.insert(it->second->getFunction()); // Mark the function as recursive
    }

    // Initialize WTO for each function in the module
    for (const SVFFunction* fun : svfir->getModule()->getFunctionSet())
    {
        auto* wto = new ICFGWTO(icfg, icfg->getFunEntryICFGNode(fun));
        wto->init();
        funcToWTO[fun] = wto;
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
    if (const SVFFunction* fun = svfir->getModule()->getSVFFunction("main"))
    {
        ICFGWTO* wto = funcToWTO[fun];
        handleWTOComponents(wto->getWTOComponents());
    }
}

/// handle global node
void AbstractInterpretation::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    abstractTrace[node] = AbstractState();
    abstractTrace[node][SymbolTableInfo::NullPtr] = AddressValue();
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
            const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
            if (intraCfgEdge && intraCfgEdge->getCondition())
            {
                AbstractState tmpEs = abstractTrace[edge->getSrcNode()];
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
                workList.push_back(abstractTrace[edge->getSrcNode()]);
            }
        }
        else
        {

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
            NodeID objId = new_es.getInternalID(addr);
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
            NodeID objId = new_es.getInternalID(addr);
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
            NodeID objId = new_es.getInternalID(addr);
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
            NodeID objId = new_es.getInternalID(addr);
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
            NodeID objId = new_es.getInternalID(addr);
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
                    NodeID objId = new_es.getInternalID(addr);
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
    const SVFValue *cond = intraEdge->getCondition();
    NodeID cmpID = svfir->getValueNode(cond);
    SVFVar *cmpVar = svfir->getGNode(cmpID);
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

    const std::vector<const ICFGNode*>& worklist_vec = icfg->getSubNodes(node);
    for (auto it = worklist_vec.begin(); it != worklist_vec.end(); ++it)
    {
        const ICFGNode* curNode = *it;
        stat->getICFGNodeTrace()++;
        // handle SVF Stmt
        for (const SVFStmt *stmt: curNode->getSVFStmts())
        {
            handleSVFStatement(stmt);
        }
        // inlining the callee by calling handleFunc for the callee function
        if (const CallICFGNode* callnode = SVFUtil::dyn_cast<CallICFGNode>(curNode))
        {
            handleCallSite(callnode);
        }
        for (auto& detector: detectors)
            detector->detect(getAbsStateFromTrace(node), node);
        stat->countStateSize();
    }
}

/**
 * @brief Hanlde two types of WTO components (singleton and cycle)
 */
void AbstractInterpretation::handleWTOComponents(const std::list<const ICFGWTOComp*>& wtoComps)
{
    for (const ICFGWTOComp* wtoNode : wtoComps)
    {
        handleWTOComponent(wtoNode);
    }
}

void AbstractInterpretation::handleWTOComponent(const SVF::ICFGWTOComp* wtoNode)
{
    if (const ICFGSingletonWTO* node = SVFUtil::dyn_cast<ICFGSingletonWTO>(wtoNode))
    {
        if (mergeStatesFromPredecessors(node->getICFGNode()))
            handleSingletonWTO(node);
    }
    // Handle WTO cycles
    else if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(wtoNode))
    {
        if (mergeStatesFromPredecessors(cycle->head()->getICFGNode()))
            handleCycleWTO(cycle);
    }
    // Assert false for unknown WTO types
    else
    {
        assert(false && "unknown WTO type!");
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
        else if (isRecursiveCall(callNode))
        {
            recursiveCallPass(callNode);
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
        {
            assert(false && "implement this part");
        }
    }
    else
    {
        assert (false && "it is not call node");
    }
}

bool AbstractInterpretation::isExtCall(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return SVFUtil::isExtCall(callfun);
}

void AbstractInterpretation::extCallPass(const SVF::CallICFGNode *callNode)
{
    callSiteStack.push_back(callNode);
    handleExtAPI(callNode);
    callSiteStack.pop_back();
}

bool AbstractInterpretation::isRecursiveCall(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return recursiveFuns.find(callfun) != recursiveFuns.end();
}

void AbstractInterpretation::recursiveCallPass(const SVF::CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);
    SkipRecursiveCall(callNode);
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

bool AbstractInterpretation::isDirectCall(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return funcToWTO.find(callfun) != funcToWTO.end();
}
void AbstractInterpretation::directCallFunPass(const SVF::CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    callSiteStack.push_back(callNode);

    abstractTrace[callNode] = as;

    ICFGWTO* wto = funcToWTO[callfun];
    handleWTOComponents(wto->getWTOComponents());

    callSiteStack.pop_back();
    // handle Ret node
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    // resume ES to callnode
    abstractTrace[retNode] = abstractTrace[callNode];
}

bool AbstractInterpretation::isIndirectCall(const SVF::CallICFGNode *callNode)
{
    const auto callsiteMaps = svfir->getIndirectCallsites();
    return callsiteMaps.find(callNode) != callsiteMaps.end();
}

void AbstractInterpretation::indirectCallFunPass(const SVF::CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);
    const auto callsiteMaps = svfir->getIndirectCallsites();
    NodeID call_id = callsiteMaps.at(callNode);
    if (!as.inVarToAddrsTable(call_id))
    {
        return;
    }
    AbstractValue Addrs = as[call_id];
    NodeID addr = *Addrs.getAddrs().begin();
    SVFVar *func_var = svfir->getGNode(AbstractState::getInternalID(addr));
    const SVFFunction *callfun = SVFUtil::dyn_cast<SVFFunction>(func_var->getValue());
    if (callfun)
    {
        callSiteStack.push_back(callNode);
        abstractTrace[callNode] = as;

        ICFGWTO* wto = funcToWTO[callfun];
        handleWTOComponents(wto->getWTOComponents());
        callSiteStack.pop_back();
        // handle Ret node
        const RetICFGNode *retNode = callNode->getRetICFGNode();
        abstractTrace[retNode] = abstractTrace[callNode];
    }
}



/// handle wto cycle (loop)
void AbstractInterpretation::handleCycleWTO(const ICFGCycleWTO*cycle)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();
    // Flag to indicate if we are in the increasing phase
    bool increasing = true;
    // Infinite loop until a fixpoint is reached,
    for (u32_t cur_iter = 0;; cur_iter++)
    {
        // Start widening or narrowing if cur_iter >= widen threshold (widen delay)
        if (cur_iter >= Options::WidenDelay())
        {
            // Widen or narrow after processing cycle head node
            AbstractState prev_head_state = abstractTrace[cycle_head];
            handleWTOComponent(cycle->head());
            AbstractState cur_head_state = abstractTrace[cycle_head];
            if (increasing)
            {
                // Widening phase
                abstractTrace[cycle_head] = prev_head_state.widening(cur_head_state);
                if (abstractTrace[cycle_head] == prev_head_state)
                {
                    increasing = false;
                    continue;
                }
            }
            else
            {
                // Widening's fixpoint reached in the widening phase, switch to narrowing
                abstractTrace[cycle_head] = prev_head_state.narrowing(cur_head_state);
                if (abstractTrace[cycle_head] == prev_head_state)
                {
                    // Narrowing's fixpoint reached in the narrowing phase, exit loop
                    break;
                }
            }
        }
        else
        {
            // Handle the cycle head
            handleSingletonWTO(cycle->head());
        }
        // Handle the cycle body
        handleWTOComponents(cycle->getWTOComponents());
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
}


void AbstractInterpretation::SkipRecursiveCall(const CallICFGNode *callNode)
{
    AbstractState& as = getAbsStateFromTrace(callNode);
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
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
    for (const SVFBasicBlock * bb: callfun->getReachableBBs())
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
    Set<const SVFFunction *> funs;
    for (const auto &it: *_ae->svfir->getICFG())
    {
        if (it.second->getFun())
        {
            funs.insert(it.second->getFun());
        }
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(it.second))
        {
            if (!isExtCall(callNode->getCallSite()))
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

void AbstractInterpretation::initExtFunMap()
{
#define SSE_FUNC_PROCESS(LLVM_NAME ,FUNC_NAME) \
        auto sse_##FUNC_NAME = [this](const CallSite &cs) { \
        /* run real ext function */            \
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(        \
            svfir->getICFG()->getICFGNode(cs.getInstruction())); \
        AbstractState& as = getAbsStateFromTrace(callNode); \
        u32_t rhs_id = svfir->getValueNode(cs.getArgument(0)); \
        if (!as.inVarToValTable(rhs_id)) return; \
        u32_t rhs = as[rhs_id].getInterval().lb().getIntNumeral(); \
        s32_t res = FUNC_NAME(rhs);            \
        u32_t lhsId = svfir->getValueNode(cs.getInstruction());               \
        as[lhsId] = IntervalValue(res);           \
        return; \
    };                                                                         \
    func_map[#FUNC_NAME] = sse_##FUNC_NAME;

    SSE_FUNC_PROCESS(isalnum, isalnum);
    SSE_FUNC_PROCESS(isalpha, isalpha);
    SSE_FUNC_PROCESS(isblank, isblank);
    SSE_FUNC_PROCESS(iscntrl, iscntrl);
    SSE_FUNC_PROCESS(isdigit, isdigit);
    SSE_FUNC_PROCESS(isgraph, isgraph);
    SSE_FUNC_PROCESS(isprint, isprint);
    SSE_FUNC_PROCESS(ispunct, ispunct);
    SSE_FUNC_PROCESS(isspace, isspace);
    SSE_FUNC_PROCESS(isupper, isupper);
    SSE_FUNC_PROCESS(isxdigit, isxdigit);
    SSE_FUNC_PROCESS(llvm.sin.f64, sin);
    SSE_FUNC_PROCESS(llvm.cos.f64, cos);
    SSE_FUNC_PROCESS(llvm.tan.f64, tan);
    SSE_FUNC_PROCESS(llvm.log.f64, log);
    SSE_FUNC_PROCESS(sinh, sinh);
    SSE_FUNC_PROCESS(cosh, cosh);
    SSE_FUNC_PROCESS(tanh, tanh);

    auto sse_svf_assert = [this](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        checkpoints.erase(callNode);
        u32_t arg0 = svfir->getValueNode(cs.getArgument(0));
        AbstractState&as = getAbsStateFromTrace(callNode);
        as[arg0].getInterval().meet_with(IntervalValue(1, 1));
        if (as[arg0].getInterval().equals(IntervalValue(1, 1)))
        {
            SVFUtil::errs() << SVFUtil::sucMsg("The assertion is successfully verified!!\n");
        }
        else
        {
            SVFUtil::errs() <<"svf_assert Fail. " << cs.getInstruction()->toString() << "\n";
            assert(false);
        }
        return;
    };
    func_map["svf_assert"] = sse_svf_assert;

    auto svf_print = [&](const CallSite &cs)
    {
        if (cs.arg_size() < 2) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t num_id = svfir->getValueNode(cs.getArgument(0));
        std::string text = strRead(as, cs.getArgument(1));
        assert(as.inVarToValTable(num_id) && "print() should pass integer");
        IntervalValue itv = as[num_id].getInterval();
        std::cout << "Text: " << text <<", Value: " << cs.getArgument(0)->toString() << ", PrintVal: " << itv.toString() << std::endl;
        return;
    };
    func_map["svf_print"] = svf_print;


    auto sse_scanf = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState& as = getAbsStateFromTrace(callNode);
        //scanf("%d", &data);
        if (cs.arg_size() < 2) return;

        u32_t dst_id = svfir->getValueNode(cs.getArgument(1));
        if (!as.inVarToAddrsTable(dst_id))
        {
            return;
        }
        else
        {
            AbstractValue Addrs = as[dst_id];
            for (auto vaddr: Addrs.getAddrs())
            {
                u32_t objId = AbstractState::getInternalID(vaddr);
                AbstractValue range = getRangeLimitFromType(svfir->getGNode(objId)->getType());
                as.store(vaddr, range);
            }
        }
    };
    auto sse_fscanf = [&](const CallSite &cs)
    {
        //fscanf(stdin, "%d", &data);
        if (cs.arg_size() < 3) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState& as = getAbsStateFromTrace(callNode);
        u32_t dst_id = svfir->getValueNode(cs.getArgument(2));
        if (!as.inVarToAddrsTable(dst_id))
        {
        }
        else
        {
            AbstractValue Addrs = as[dst_id];
            for (auto vaddr: Addrs.getAddrs())
            {
                u32_t objId = AbstractState::getInternalID(vaddr);
                AbstractValue range = getRangeLimitFromType(svfir->getGNode(objId)->getType());
                as.store(vaddr, range);
            }
        }
    };

    func_map["__isoc99_fscanf"] = sse_fscanf;
    func_map["__isoc99_scanf"] = sse_scanf;
    func_map["__isoc99_vscanf"] = sse_scanf;
    func_map["fscanf"] = sse_fscanf;
    func_map["scanf"] = sse_scanf;
    func_map["sscanf"] = sse_scanf;
    func_map["__isoc99_sscanf"] = sse_scanf;
    func_map["vscanf"] = sse_scanf;

    auto sse_fread = [&](const CallSite &cs)
    {
        if (cs.arg_size() < 3) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t block_count_id = svfir->getValueNode(cs.getArgument(2));
        u32_t block_size_id = svfir->getValueNode(cs.getArgument(1));
        IntervalValue block_count = as[block_count_id].getInterval();
        IntervalValue block_size = as[block_size_id].getInterval();
        IntervalValue block_byte = block_count * block_size;
    };
    func_map["fread"] = sse_fread;

    auto sse_sprintf = [&](const CallSite &cs)
    {
        // printf is difficult to predict since it has no byte size arguments
    };

    auto sse_snprintf = [&](const CallSite &cs)
    {
        if (cs.arg_size() < 2) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t size_id = svfir->getValueNode(cs.getArgument(1));
        u32_t dst_id = svfir->getValueNode(cs.getArgument(0));
        // get elem size of arg2
        u32_t elemSize = 1;
        if (cs.getArgument(2)->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(cs.getArgument(2)->getType())->getTypeOfElement()->getByteSize();
        }
        else if (cs.getArgument(2)->getType()->isPointerTy())
        {
            elemSize = as.getPointeeElement(svfir->getValueNode(cs.getArgument(2)))->getByteSize();
        }
        else
        {
            return;
            // assert(false && "we cannot support this type");
        }
        IntervalValue size = as[size_id].getInterval() * IntervalValue(elemSize) - IntervalValue(1);
        if (!as.inVarToAddrsTable(dst_id))
        {
        }
    };
    func_map["__snprintf_chk"] = sse_snprintf;
    func_map["__vsprintf_chk"] = sse_sprintf;
    func_map["__sprintf_chk"] = sse_sprintf;
    func_map["snprintf"] = sse_snprintf;
    func_map["sprintf"] = sse_sprintf;
    func_map["vsprintf"] = sse_sprintf;
    func_map["vsnprintf"] = sse_snprintf;
    func_map["__vsnprintf_chk"] = sse_snprintf;
    func_map["swprintf"] = sse_snprintf;
    func_map["_snwprintf"] = sse_snprintf;


    auto sse_itoa = [&](const CallSite &cs)
    {
        // itoa(num, ch, 10);
        // num: int, ch: char*, 10 is decimal
        if (cs.arg_size() < 3) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t num_id = svfir->getValueNode(cs.getArgument(0));

        u32_t num = (u32_t) as[num_id].getInterval().getNumeral();
        std::string snum = std::to_string(num);
    };
    func_map["itoa"] = sse_itoa;


    auto sse_strlen = [&](const CallSite &cs)
    {
        // check the arg size
        if (cs.arg_size() < 1) return;
        const SVFValue* strValue = cs.getArgument(0);
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState& as = getAbsStateFromTrace(callNode);
        NodeID value_id = svfir->getValueNode(strValue);
        u32_t lhsId = svfir->getValueNode(cs.getInstruction());
        u32_t dst_size = 0;
        for (const auto& addr : as[value_id].getAddrs())
        {
            NodeID objId = AbstractState::getInternalID(addr);
            if (svfir->getBaseObj(objId)->isConstantByteSize())
            {
                dst_size = svfir->getBaseObj(objId)->getByteSizeOfObj();
            }
            else
            {
                const ICFGNode* addrNode = svfir->getICFG()->getICFGNode(SVFUtil::cast<SVFInstruction>(svfir->getBaseObj(objId)->getValue()));
                for (const SVFStmt* stmt2: addrNode->getSVFStmts())
                {
                    if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                    {
                        dst_size = as.getAllocaInstByteSize(addrStmt);
                    }
                }
            }
        }
        u32_t len = 0;
        NodeID dstid = svfir->getValueNode(strValue);
        if (as.inVarToAddrsTable(dstid))
        {
            for (u32_t index = 0; index < dst_size; index++)
            {
                AbstractValue expr0 =
                    as.getGepObjAddrs(dstid, IntervalValue(index));
                AbstractValue val;
                for (const auto &addr: expr0.getAddrs())
                {
                    val.join_with(as.load(addr));
                }
                if (val.getInterval().is_numeral() && (char) val.getInterval().getIntNumeral() == '\0')
                {
                    break;
                }
                ++len;
            }
        }
        if (len == 0)
        {
            as[lhsId] = IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit());
        }
        else
        {
            as[lhsId] = IntervalValue(len);
        }
    };
    func_map["strlen"] = sse_strlen;
    func_map["wcslen"] = sse_strlen;

    auto sse_recv = [&](const CallSite &cs)
    {
        // recv(sockfd, buf, len, flags);
        if (cs.arg_size() < 4) return;
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t len_id = svfir->getValueNode(cs.getArgument(2));
        IntervalValue len = as[len_id].getInterval() - IntervalValue(1);
        u32_t lhsId = svfir->getValueNode(cs.getInstruction());
        as[lhsId] = len;
    };
    func_map["recv"] = sse_recv;
    func_map["__recv"] = sse_recv;
    auto safe_bufaccess = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        checkpoints.erase(callNode);
        //void SAFE_BUFACCESS(void* data, int size);
        if (cs.arg_size() < 2) return;
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t size_id = svfir->getValueNode(cs.getArgument(1));
        IntervalValue val = as[size_id].getInterval();
        if (val.isBottom())
        {
            val = IntervalValue(0);
            assert(false && "SAFE_BUFACCESS size is bottom");
        }
        for (auto& detector: detectors)
        {
            // if detector is a buffer overflow detector, call it
            if (SVFUtil::isa<BufOverflowDetector>(detector))
            {
                BufOverflowDetector* bufDetector = SVFUtil::cast<BufOverflowDetector>(detector.get());
                bool isSafe = bufDetector->canSafelyAccessMemory(as, cs.getArgument(0), val);
                if (isSafe)
                {
                    std::cout << "safe buffer access success: " << callNode->toString() << std::endl;
                    return;
                }
                else
                {
                    std::string err_msg = "this SAFE_BUFACCESS should be a safe access but detected buffer overflow. Pos: ";
                    err_msg += cs.getInstruction()->getSourceLoc();
                    std::cerr << err_msg << std::endl;
                    assert(false);
                }
            }
        }
    };
    func_map["SAFE_BUFACCESS"] = safe_bufaccess;

    auto unsafe_bufaccess = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(
                                           svfir->getICFG()->getICFGNode(cs.getInstruction()));
        checkpoints.erase(callNode);
        //void UNSAFE_BUFACCESS(void* data, int size);
        if (cs.arg_size() < 2) return;
        AbstractState&as = getAbsStateFromTrace(callNode);
        u32_t size_id = svfir->getValueNode(cs.getArgument(1));
        IntervalValue val = as[size_id].getInterval();
        if (val.isBottom())
        {
            assert(false && "UNSAFE_BUFACCESS size is bottom");
        }
        for (auto& detector: detectors)
        {
            // if detector is a buffer overflow detector, call it
            if (SVFUtil::isa<BufOverflowDetector>(detector))
            {
                BufOverflowDetector* bufDetector = SVFUtil::cast<BufOverflowDetector>(detector.get());
                bool isSafe = bufDetector->canSafelyAccessMemory(as, cs.getArgument(0), val);
                if (!isSafe)
                {
                    std::cout << "detect buffer overflow success: " << callNode->toString() << std::endl;
                    return;
                }
                else
                {
                    std::string err_msg = "this UNSAFE_BUFACCESS should be a buffer overflow but not detected. Pos: ";
                    err_msg += cs.getInstruction()->getSourceLoc();
                    std::cerr << err_msg << std::endl;
                    assert(false);
                }
            }
        }
    };
    func_map["UNSAFE_BUFACCESS"] = unsafe_bufaccess;

    // init checkpoint_names
    checkpoint_names.insert("svf_assert");
    checkpoint_names.insert("UNSAFE_ACCESS");
    checkpoint_names.insert("SAFE_ACCESS");
};

std::string AbstractInterpretation::strRead(AbstractState& as, const SVFValue* rhs)
{
    // sse read string nodeID->string
    std::string str0;

    for (u32_t index = 0; index < Options::MaxFieldLimit(); index++)
    {
        // dead loop for string and break if there's a \0. If no \0, it will throw err.
        if (!as.inVarToAddrsTable(svfir->getValueNode(rhs))) continue;
        AbstractValue expr0 =
            as.getGepObjAddrs(svfir->getValueNode(rhs), IntervalValue(index));

        AbstractValue val;
        for (const auto &addr: expr0.getAddrs())
        {
            val.join_with(as.load(addr));
        }
        if (!val.getInterval().is_numeral())
        {
            break;
        }
        if ((char) val.getInterval().getIntNumeral() == '\0')
        {
            break;
        }
        str0.push_back((char) val.getInterval().getIntNumeral());
    }
    return str0;
}

void AbstractInterpretation::handleExtAPI(const CallICFGNode *call)
{
    AbstractState& as = getAbsStateFromTrace(call);
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    assert(fun && "SVFFunction* is nullptr");
    CallSite cs = SVFUtil::getSVFCallSite(call);
    ExtAPIType extType = UNCLASSIFIED;
    // get type of mem api
    for (const std::string &annotation: fun->getAnnotations())
    {
        if (annotation.find("MEMCPY") != std::string::npos)
            extType =  MEMCPY;
        if (annotation.find("MEMSET") != std::string::npos)
            extType =  MEMSET;
        if (annotation.find("STRCPY") != std::string::npos)
            extType = STRCPY;
        if (annotation.find("STRCAT") != std::string::npos)
            extType =  STRCAT;
    }
    if (extType == UNCLASSIFIED)
    {
        if (func_map.find(fun->getName()) != func_map.end())
        {
            func_map[fun->getName()](cs);
        }
        else
        {
            u32_t lhsId = svfir->getValueNode(SVFUtil::getSVFCallSite(call).getInstruction());
            if (as.inVarToAddrsTable(lhsId))
            {

            }
            else
            {
                as[lhsId] = IntervalValue();
            }
            return;
        }
    }
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    else if (extType == MEMCPY)
    {
        IntervalValue len = as[svfir->getValueNode(cs.getArgument(2))].getInterval();
        handleMemcpy(as, cs.getArgument(0), cs.getArgument(1), len, 0);
    }
    else if (extType == MEMSET)
    {
        // memset dst is arg0, elem is arg1, size is arg2
        IntervalValue len = as[svfir->getValueNode(cs.getArgument(2))].getInterval();
        IntervalValue elem = as[svfir->getValueNode(cs.getArgument(1))].getInterval();
        handleMemset(as,cs.getArgument(0), elem, len);
    }
    else if (extType == STRCPY)
    {
        handleStrcpy(call);
    }
    else if (extType == STRCAT)
    {
        handleStrcat(call);
    }
    else
    {

    }
    return;
}

void AbstractInterpretation::collectCheckPoint()
{
    // traverse every ICFGNode
    for (auto it = svfir->getICFG()->begin(); it != svfir->getICFG()->end(); ++it)
    {
        const ICFGNode* node = it->second;
        if (const CallICFGNode *call = SVFUtil::dyn_cast<CallICFGNode>(node))
        {
            if (const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite()))
            {
                if (checkpoint_names.find(fun->getName()) !=
                        checkpoint_names.end())
                {
                    checkpoints.insert(call);
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


void AbstractInterpretation::handleStrcpy(const CallICFGNode *call)
{
    // strcpy, __strcpy_chk, stpcpy , wcscpy, __wcscpy_chk
    // get the dst and src
    AbstractState& as = getAbsStateFromTrace(call);
    CallSite cs = SVFUtil::getSVFCallSite(call);
    const SVFValue* arg0Val = cs.getArgument(0);
    const SVFValue* arg1Val = cs.getArgument(1);
    IntervalValue strLen = getStrlen(as, arg1Val);
    // no need to -1, since it has \0 as the last byte
    handleMemcpy(as, arg0Val, arg1Val, strLen, strLen.lb().getIntNumeral());
}

IntervalValue AbstractInterpretation::getStrlen(AbstractState& as, const SVF::SVFValue *strValue)
{
    NodeID value_id = svfir->getValueNode(strValue);
    u32_t dst_size = 0;
    for (const auto& addr : as[value_id].getAddrs())
    {
        NodeID objId = AbstractState::getInternalID(addr);
        if (svfir->getBaseObj(objId)->isConstantByteSize())
        {
            dst_size = svfir->getBaseObj(objId)->getByteSizeOfObj();
        }
        else
        {
            const ICFGNode* addrNode = svfir->getICFG()->getICFGNode(SVFUtil::cast<SVFInstruction>(svfir->getBaseObj(objId)->getValue()));
            for (const SVFStmt* stmt2: addrNode->getSVFStmts())
            {
                if (const AddrStmt* addrStmt = SVFUtil::dyn_cast<AddrStmt>(stmt2))
                {
                    dst_size = as.getAllocaInstByteSize(addrStmt);
                }
            }
        }
    }
    u32_t len = 0;
    NodeID dstid = svfir->getValueNode(strValue);
    u32_t elemSize = 1;
    if (as.inVarToAddrsTable(dstid))
    {
        for (u32_t index = 0; index < dst_size; index++)
        {
            AbstractValue expr0 =
                as.getGepObjAddrs(dstid, IntervalValue(index));
            AbstractValue val;
            for (const auto &addr: expr0.getAddrs())
            {
                val.join_with(as.load(addr));
            }
            if (val.getInterval().is_numeral() && (char) val.getInterval().getIntNumeral() == '\0')
            {
                break;
            }
            ++len;
        }
        if (strValue->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(strValue->getType())->getTypeOfElement()->getByteSize();
        }
        else if (strValue->getType()->isPointerTy())
        {
            if (const SVFType* elemType = as.getPointeeElement(svfir->getValueNode(strValue)))
            {
                if (elemType->isArrayTy())
                    elemSize = SVFUtil::dyn_cast<SVFArrayType>(elemType)->getTypeOfElement()->getByteSize();
                else
                    elemSize = elemType->getByteSize();
            }
            else
            {
                elemSize = 1;
            }
        }
        else
        {
            assert(false && "we cannot support this type");
        }
    }
    if (len == 0)
    {
        return IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit());
    }
    else
    {
        return IntervalValue(len * elemSize);
    }
}


void AbstractInterpretation::handleStrcat(const SVF::CallICFGNode *call)
{
    // __strcat_chk, strcat, __wcscat_chk, wcscat, __strncat_chk, strncat, __wcsncat_chk, wcsncat
    // to check it is  strcat group or strncat group
    AbstractState& as = getAbsStateFromTrace(call);
    const SVFFunction *fun = SVFUtil::getCallee(call);
    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};
    if (std::find(strcatGroup.begin(), strcatGroup.end(), fun->getName()) != strcatGroup.end())
    {
        CallSite cs = SVFUtil::getSVFCallSite(call);
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg1Val = cs.getArgument(1);
        IntervalValue strLen0 = getStrlen(as, arg0Val);
        IntervalValue strLen1 = getStrlen(as, arg1Val);
        IntervalValue totalLen = strLen0 + strLen1;
        handleMemcpy(as, arg0Val, arg1Val, strLen1, strLen0.lb().getIntNumeral());
        // do memcpy
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), fun->getName()) != strncatGroup.end())
    {
        CallSite cs = SVFUtil::getSVFCallSite(call);
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg1Val = cs.getArgument(1);
        const SVFValue* arg2Val = cs.getArgument(2);
        IntervalValue arg2Num = as[svfir->getValueNode(arg2Val)].getInterval();
        IntervalValue strLen0 = getStrlen(as, arg0Val);
        IntervalValue totalLen = strLen0 + arg2Num;
        handleMemcpy(as, arg0Val, arg1Val, arg2Num, strLen0.lb().getIntNumeral());
        // do memcpy
    }
    else
    {
        assert(false && "unknown strcat function, please add it to strcatGroup or strncatGroup");
    }
}

void AbstractInterpretation::handleMemcpy(AbstractState& as, const SVF::SVFValue *dst, const SVF::SVFValue *src, IntervalValue len,  u32_t start_idx)
{
    u32_t dstId = svfir->getValueNode(dst); // pts(dstId) = {objid}  objbar objtypeinfo->getType().
    u32_t srcId = svfir->getValueNode(src);
    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy())
    {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())->getTypeOfElement()->getByteSize();
    }
    // memcpy(i32*, i32*, 40)
    else if (dst->getType()->isPointerTy())
    {
        if (const SVFType* elemType = as.getPointeeElement(svfir->getValueNode(dst)))
        {
            if (elemType->isArrayTy())
                elemSize = SVFUtil::dyn_cast<SVFArrayType>(elemType)->getTypeOfElement()->getByteSize();
            else
                elemSize = elemType->getByteSize();
        }
        else
        {
            elemSize = 1;
        }
    }
    else
    {
        assert(false && "we cannot support this type");
    }
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(), (u32_t) len.lb().getIntNumeral());
    u32_t range_val = size / elemSize;
    if (as.inVarToAddrsTable(srcId) && as.inVarToAddrsTable(dstId))
    {
        for (u32_t index = 0; index < range_val; index++)
        {
            // dead loop for string and break if there's a \0. If no \0, it will throw err.
            AbstractValue expr_src =
                as.getGepObjAddrs(srcId, IntervalValue(index));
            AbstractValue expr_dst =
                as.getGepObjAddrs(dstId, IntervalValue(index + start_idx));
            for (const auto &dst: expr_dst.getAddrs())
            {
                for (const auto &src: expr_src.getAddrs())
                {
                    u32_t objId = AbstractState::getInternalID(src);
                    if (as.inAddrToValTable(objId))
                    {
                        as.store(dst, as.load(src));
                    }
                    else if (as.inAddrToAddrsTable(objId))
                    {
                        as.store(dst, as.load(src));
                    }
                }
            }
        }
    }
}

void AbstractInterpretation::handleMemset(AbstractState& as, const SVF::SVFValue *dst, IntervalValue elem, IntervalValue len)
{
    u32_t dstId = svfir->getValueNode(dst);
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(), (u32_t) len.lb().getIntNumeral());
    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy())
    {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())->getTypeOfElement()->getByteSize();
    }
    else if (dst->getType()->isPointerTy())
    {
        if (const SVFType* elemType = as.getPointeeElement(svfir->getValueNode(dst)))
        {
            elemSize = elemType->getByteSize();
        }
        else
        {
            elemSize = 1;
        }
    }
    else
    {
        assert(false && "we cannot support this type");
    }

    u32_t range_val = size / elemSize;
    for (u32_t index = 0; index < range_val; index++)
    {
        // dead loop for string and break if there's a \0. If no \0, it will throw err.
        if (as.inVarToAddrsTable(dstId))
        {
            AbstractValue lhs_gep = as.getGepObjAddrs(dstId, IntervalValue(index));
            for (const auto &addr: lhs_gep.getAddrs())
            {
                u32_t objId = AbstractState::getInternalID(addr);
                if (as.inAddrToValTable(objId))
                {
                    AbstractValue tmp = as.load(addr);
                    tmp.join_with(elem);
                    as.store(addr, tmp);
                }
                else
                {
                    as.store(addr, elem);
                }
            }
        }
        else
            break;
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
            AbstractState& opAs = getAbsStateFromTrace(opICFGNode);
            rhs.join_with(opAs[curId]);
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
        as[addr->getRHSVarID()].getInterval().meet_with(getRangeLimitFromType(addr->getRHSVar()->getType()));
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
    if (!as.inVarToValTable(op0)) as[op0] = IntervalValue::top();
    if (!as.inVarToValTable(op1)) as[op1] = IntervalValue::top();
    u32_t res = cmp->getResID();
    if (as.inVarToValTable(op0) && as.inVarToValTable(op1))
    {
        IntervalValue resVal;
        if (as[op0].isInterval() && as[op1].isInterval())
        {
            IntervalValue &lhs = as[op0].getInterval(), &rhs = as[op1].getInterval();
            //AbstractValue
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
            {
                assert(false && "undefined compare: ");
            }
            }
            as[res] = resVal;
        }
        else if (as[op0].isAddr() && as[op1].isAddr())
        {
            AddressValue &lhs = as[op0].getAddrs(), &rhs = as[op1].getAddrs();
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
            {
                assert(false && "undefined compare: ");
            }
            }
            as[res] = resVal;
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
                {
                    assert(false && "cannot support int type other than u8/16/32/64");
                }
            }
            else
            {
                return IntervalValue::top(); // TODO: may have better solution
            }
        }
        return IntervalValue::top(); // TODO: may have better solution
    };

    auto getTruncValue = [](const AbstractState& as, const SVF::SVFVar* var,
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
                return IntervalValue::top();
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
                return IntervalValue::top();
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
                return IntervalValue::top();
            }
            return IntervalValue(s32_lb, s32_ub);
        }
        else
        {
            assert(false && "cannot support dst int type other than u8/16/32");
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
    {
        assert(false && "undefined copy kind");
        abort();
    }
}

/**
 * This function, getRangeLimitFromType, calculates the lower and upper bounds of
 * a numeric range for a given SVFType. It is used to determine the possible value
 * range of integer types. If the type is an SVFIntegerType, it calculates the bounds
 * based on the size and signedness of the type. The calculated bounds are returned
 * as an IntervalValue representing the lower (lb) and upper (ub) limits of the range.
 *
 * @param type   The SVFType for which to calculate the value range.
 *
 * @return       An IntervalValue representing the lower and upper bounds of the range.
 */
IntervalValue AbstractInterpretation::getRangeLimitFromType(const SVFType* type)
{
    if (const SVFIntegerType* intType = SVFUtil::dyn_cast<SVFIntegerType>(type))
    {
        u32_t bits = type->getByteSize() * 8;
        s64_t ub = 0;
        s64_t lb = 0;
        if (bits >= 32)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<s32_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<s32_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u32_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u32_t>::min());
            }
        }
        else if (bits == 16)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<s16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<s16_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u16_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u16_t>::min());
            }
        }
        else if (bits == 8)
        {
            if (intType->isSigned())
            {
                ub = static_cast<s64_t>(std::numeric_limits<int8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<int8_t>::min());
            }
            else
            {
                ub = static_cast<s64_t>(std::numeric_limits<u_int8_t>::max());
                lb = static_cast<s64_t>(std::numeric_limits<u_int8_t>::min());
            }
        }
        return IntervalValue(lb, ub);
    }
    else if (SVFUtil::isa<SVFOtherType>(type))
    {
        // handle other type like float double, set s32_t as the range
        s64_t ub = static_cast<s64_t>(std::numeric_limits<s32_t>::max());
        s64_t lb = static_cast<s64_t>(std::numeric_limits<s32_t>::min());
        return IntervalValue(lb, ub);
    }
    else
    {
        return IntervalValue::top();
        // other types, return top interval
    }
}



