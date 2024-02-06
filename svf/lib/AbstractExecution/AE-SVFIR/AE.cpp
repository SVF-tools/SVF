//===- AE.cpp -- Abstract Execution---------------------------------//
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
#include "WPA/Andersen.h"
#include "Util/CFBasicBlockGBuilder.h"
#include "SVFIR/SVFIR.h"
#include "AbstractExecution/AE-SVFIR/AE.h"
#include "Util/Options.h"
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

Map<s32_t, s32_t> _reverse_predicate = {
        {CmpStmt::Predicate::FCMP_OEQ , CmpStmt::Predicate::FCMP_ONE}, // == -> !=
        {CmpStmt::Predicate::FCMP_UEQ , CmpStmt::Predicate::FCMP_UNE}, // == -> !=
        {CmpStmt::Predicate::FCMP_OGT , CmpStmt::Predicate::FCMP_OLE}, // > -> <=
        {CmpStmt::Predicate::FCMP_OGE , CmpStmt::Predicate::FCMP_OLT}, // >= -> <
        {CmpStmt::Predicate::FCMP_OLT , CmpStmt::Predicate::FCMP_OGE}, // < -> >=
        {CmpStmt::Predicate::FCMP_OLE , CmpStmt::Predicate::FCMP_OGT}, // <= -> >
        {CmpStmt::Predicate::FCMP_ONE , CmpStmt::Predicate::FCMP_OEQ}, // != -> ==
        {CmpStmt::Predicate::FCMP_UNE , CmpStmt::Predicate::FCMP_UEQ}, // != -> ==
        {CmpStmt::Predicate::ICMP_EQ , CmpStmt::Predicate::ICMP_NE}, // == -> !=
        {CmpStmt::Predicate::ICMP_NE , CmpStmt::Predicate::ICMP_EQ}, // != -> ==
        {CmpStmt::Predicate::ICMP_UGT , CmpStmt::Predicate::ICMP_ULE}, // > -> <=
        {CmpStmt::Predicate::ICMP_ULT , CmpStmt::Predicate::ICMP_UGE}, // < -> >=
        {CmpStmt::Predicate::ICMP_UGE , CmpStmt::Predicate::ICMP_ULT}, // >= -> <
        {CmpStmt::Predicate::ICMP_SGT , CmpStmt::Predicate::ICMP_SLE}, // > -> <=
        {CmpStmt::Predicate::ICMP_SLT , CmpStmt::Predicate::ICMP_SGE}, // < -> >=
        {CmpStmt::Predicate::ICMP_SGE , CmpStmt::Predicate::ICMP_SLT}, // >= -> <
};


Map<s32_t, s32_t> _switch_lhsrhs_predicate = {
        {CmpStmt::Predicate::FCMP_OEQ , CmpStmt::Predicate::FCMP_OEQ}, // == -> ==
        {CmpStmt::Predicate::FCMP_UEQ , CmpStmt::Predicate::FCMP_UEQ}, // == -> ==
        {CmpStmt::Predicate::FCMP_OGT , CmpStmt::Predicate::FCMP_OLT}, // > -> <
        {CmpStmt::Predicate::FCMP_OGE , CmpStmt::Predicate::FCMP_OLE}, // >= -> <=
        {CmpStmt::Predicate::FCMP_OLT , CmpStmt::Predicate::FCMP_OGT}, // < -> >
        {CmpStmt::Predicate::FCMP_OLE , CmpStmt::Predicate::FCMP_OGE}, // <= -> >=
        {CmpStmt::Predicate::FCMP_ONE , CmpStmt::Predicate::FCMP_ONE}, // != -> !=
        {CmpStmt::Predicate::FCMP_UNE , CmpStmt::Predicate::FCMP_UNE}, // != -> !=
        {CmpStmt::Predicate::ICMP_EQ , CmpStmt::Predicate::ICMP_EQ}, // == -> ==
        {CmpStmt::Predicate::ICMP_NE , CmpStmt::Predicate::ICMP_NE}, // != -> !=
        {CmpStmt::Predicate::ICMP_UGT , CmpStmt::Predicate::ICMP_ULT}, // > -> <
        {CmpStmt::Predicate::ICMP_ULT , CmpStmt::Predicate::ICMP_UGT}, // < -> >
        {CmpStmt::Predicate::ICMP_UGE , CmpStmt::Predicate::ICMP_ULE}, // >= -> <=
        {CmpStmt::Predicate::ICMP_SGT , CmpStmt::Predicate::ICMP_SLT}, // > -> <
        {CmpStmt::Predicate::ICMP_SLT , CmpStmt::Predicate::ICMP_SGT}, // < -> >
        {CmpStmt::Predicate::ICMP_SGE , CmpStmt::Predicate::ICMP_SLE}, // >= -> <=
};

void AE::runOnModule(SVF::SVFIR *svfModule) {
    // 1. Start clock
    _stat->startClk();

    _svfir = svfModule;
    _ander = AndersenWaveDiff::createAndersenWaveDiff(_svfir);
    _api->setModule(_svfir);
    // init SVF Execution States
    _svfir2ExeState = new SVFIR2ItvExeState(_svfir);

    // init SSE External API Handler
    _callgraph = _ander->getPTACallGraph();
    _icfg = _svfir->getICFG();
    CFBasicBlockGBuilder CFBGBuilder;
    _icfg->updateCallGraph(_callgraph);

    CFBGBuilder.build(_icfg);
    _CFBlockG = CFBGBuilder.getCFBasicBlockGraph();
    /// collect checkpoint
    _api->collectCheckPoint();

    /// if function contains callInst that call itself, it is a recursive function.
    markRecursiveFuns();
    for (const SVFFunction* fun: _svfir->getModule()->getFunctionSet()) {
        if (_CFBlockG->hasGNode(_icfg->getFunEntryICFGNode(fun)->getId()) ) {
            const CFBasicBlockNode *node = _CFBlockG->getGNode(_icfg->getFunEntryICFGNode(fun)->getId());
             auto *wto = new CFBasicBlockGWTO(_CFBlockG, node);
             wto->init();
            _funcToWTO[fun] = wto;
        }
    }
    analyse();
    _api->checkPointAllSet();
    // 5. Stop clock and report bugs
    _stat->endClk();
    _stat->finializeStat();
    _stat->performStat();
    _stat->reportBug();
}

AE::AE() {
    _stat = new AEStat(this);
    _api = new AEAPI(this, _stat);
}
/// Destructor
AE::~AE() {
    delete _stat;
    //delete _api;
}

void AE::markRecursiveFuns() {
    // detect if callgraph has cycle
    CallGraphSCC* _callGraphScc = _ander->getCallGraphSCC();
    _callGraphScc->find();

    for (auto it = _callgraph->begin(); it != _callgraph->end(); it++) {
        if (_callGraphScc->isInCycle(it->second->getId()))
            _recursiveFuns.insert(it->second->getFunction());
    }
}

/// Program entry
void AE::analyse() {
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    if (const SVFFunction* fun = _svfir->getModule()->getSVFFunction("main")) {
        handleFunc(fun);
    }
}

/// handle global node
void AE::handleGlobalNode() {
    IntervalExeState es;
    const ICFGNode* node = _icfg->getGlobalICFGNode();
    _svfir2ExeState->setEs(es);
    // Global Node, we just need to handle addr, load, store, copy and gep
    for (const SVFStmt *stmt: node->getSVFStmts()) {
        if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt)) {
            _svfir2ExeState->translateAddr(addr);
        } else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt)) {
            _svfir2ExeState->translateLoad(load);
        } else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt)) {
            _svfir2ExeState->translateStore(store);
        } else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt)) {
            _svfir2ExeState->translateCopy(copy);
        } else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt)) {
            _svfir2ExeState->translateGep(gep);
        }
        else
            assert(false && "implement this part");
    }
    // for stmts in global node, exe state will move to global state to lower memory usage
    _svfir2ExeState->moveToGlobal();
}

/// get execution state by merging states of predecessor blocks
/// Scenario 1: preblock -----(intraEdge)----> block, join the preES of inEdges
/// Scenario 2: preblock -----(callEdge)----> block
bool AE::hasInEdgesES(const CFBasicBlockNode *block) {
    if (isGlobalEntry(block)) {
        _preES[block] = IntervalExeState();
        return true;
    }
    // is common basic block
    else
    {
        IntervalExeState es;
        u32_t inEdgeNum = 0;
        for (auto& edge: block->getInEdges()) {
            if (_postES.find(edge->getSrcNode()) != _postES.end()) {
                const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge->getICFGEdge());
                if (intraCfgEdge && intraCfgEdge->getCondition()) {
                    IntervalExeState tmpEs = _postES[edge->getSrcNode()];
                    if (hasBranchES(intraCfgEdge, tmpEs)) {
                        es.joinWith(tmpEs);
                        inEdgeNum++;
                    } else {
                        // do nothing
                    }
                }
                else {
                    es.joinWith(_postES[edge->getSrcNode()]);
                    inEdgeNum++;
                }
            } else {

            }
        }
        if (inEdgeNum == 0) {
            return false;
        }
        else {
            _preES[block] = es;
            return true;
        }
    }
    assert(false && "implement this part");
}

bool AE::isFunEntry(const SVF::CFBasicBlockNode *block) {
    if (SVFUtil::isa<FunEntryICFGNode>(*block->getICFGNodes().begin())) {
        if (_preES.find(block) != _preES.end()) {
            return true;
        }
    }
    return false;
}

bool AE::isGlobalEntry(const SVF::CFBasicBlockNode *block) {
    if (!block->hasIncomingEdge())
        return true;
    else
        return false;
}

bool AE::hasCmpBranchES(const CmpStmt* cmpStmt, s64_t succ, IntervalExeState& es) {
    IntervalExeState new_es = es;
    // get cmp stmt's op0, op1, and predicate
    NodeID op0 = cmpStmt->getOpVarID(0);
    NodeID op1 = cmpStmt->getOpVarID(1);
    NodeID res_id = cmpStmt->getResID();
    s32_t predicate = cmpStmt->getPredicate();

    // if op0 or op1 is undefined, return;
    // skip address compare
    if (new_es.inVarToAddrsTable(op0) || new_es.inVarToAddrsTable(op1)) {
        es = new_es;
        return true;
    }
    const LoadStmt *load_op0 = nullptr;
    const LoadStmt *load_op1 = nullptr;
    // get '%1 = load i32 s', and load inst may not exist
    SVFVar* loadVar0 = _svfir->getGNode(op0);
    if (!loadVar0->getInEdges().empty()) {
        SVFStmt *loadVar0InStmt = *loadVar0->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt)) {
            load_op0 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar0InStmt)) {
            loadVar0 = _svfir->getGNode(copyStmt->getRHSVarID());
            if (!loadVar0->getInEdges().empty()) {
                SVFStmt *loadVar0InStmt2 = *loadVar0->getInEdges().begin();
                if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt2)) {
                    load_op0 = loadStmt;
                }
            }
        }
    }

    SVFVar* loadVar1 = _svfir->getGNode(op1);
    if (!loadVar1->getInEdges().empty()) {
        SVFStmt *loadVar1InStmt = *loadVar1->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt)) {
            load_op1 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar1InStmt)) {
            loadVar1 = _svfir->getGNode(copyStmt->getRHSVarID());
            if (!loadVar1->getInEdges().empty()) {
                SVFStmt *loadVar1InStmt2 = *loadVar1->getInEdges().begin();
                if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt2)) {
                    load_op1 = loadStmt;
                }
            }
        }
    }
    // for const X const, we may get concrete resVal instantly
    // for var X const, we may get [0,1] if the intersection of var and const is not empty set
    IntervalValue resVal = new_es[res_id];
    resVal.meet_with(IntervalValue((int64_t) succ, succ));
    // If Var X const generates bottom value, it means this branch path is not feasible.
    if (resVal.isBottom()) {
        return false;
    }

    bool b0 = new_es[op0].is_numeral();
    bool b1 = new_es[op1].is_numeral();

    // if const X var, we should reverse op0 and op1.
    if (b0 && !b1) {
        new_es.cpyItvToLocal(op1);
    } else if (!b0 && b1) {
        new_es.cpyItvToLocal(op0);
    }

    // if const X var, we should reverse op0 and op1.
    if (b0 && !b1) {
        std::swap(op0, op1);
        std::swap(load_op0, load_op1);
        predicate = _switch_lhsrhs_predicate[predicate];
    } else {
        // if var X var, we cannot preset the branch condition to infer the intervals of var0,var1
        if (!b0 && !b1) {
            es = new_es;
            return true;
        }
            // if const X const, we can instantly get the resVal
        else if (b0 && b1) {
            es = new_es;
            return true;
        }
    }
    // if cmp is 'var X const == false', we should reverse predicate 'var X' const == true'
    // X' is reverse predicate of X
    if (succ == 0) {
        predicate = _reverse_predicate[predicate];
    } else {}
    // change interval range according to the compare predicate
    ExeState::Addrs addrs;
    if(load_op0 && new_es.inVarToAddrsTable(load_op0->getRHSVarID()))
        addrs = new_es.getAddrs(load_op0->getRHSVarID());

    IntervalValue &lhs = new_es[op0], &rhs = new_es[op1];
    switch (predicate) {
        case CmpStmt::Predicate::ICMP_EQ:
        case CmpStmt::Predicate::FCMP_OEQ:
        case CmpStmt::Predicate::FCMP_UEQ: {
            // Var == Const, so [var.lb, var.ub].meet_with(const)
            lhs.meet_with(rhs);
            // if lhs is register value, we should also change its mem obj
            for (const auto &addr: addrs) {
                NodeID objId = new_es.getInternalID(addr);
                if (new_es.inLocToValTable(objId)) {
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
            for (const auto &addr: addrs) {
                NodeID objId = new_es.getInternalID(addr);
                if (new_es.inLocToValTable(objId)) {
                    new_es.load(addr).meet_with(
                            IntervalValue(rhs.lb() + 1, IntervalValue::plus_infinity()));
                }
            }
            break;
        case CmpStmt::Predicate::ICMP_UGE:
        case CmpStmt::Predicate::ICMP_SGE:
        case CmpStmt::Predicate::FCMP_OGE:
        case CmpStmt::Predicate::FCMP_UGE: {
            // Var >= Const, so [var.lb, var.ub].meet_with([Const, +INF])
            lhs.meet_with(IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
            // if lhs is register value, we should also change its mem obj
            for (const auto &addr: addrs) {
                NodeID objId = new_es.getInternalID(addr);
                if (new_es.inLocToValTable(objId)) {
                    new_es.load(addr).meet_with(
                            IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
                }
            }
            break;
        }
        case CmpStmt::Predicate::ICMP_ULT:
        case CmpStmt::Predicate::ICMP_SLT:
        case CmpStmt::Predicate::FCMP_OLT:
        case CmpStmt::Predicate::FCMP_ULT: {
            // Var < Const, so [var.lb, var.ub].meet_with([-INF, const.ub-1])
            lhs.meet_with(IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
            // if lhs is register value, we should also change its mem obj
            for (const auto &addr: addrs) {
                NodeID objId = new_es.getInternalID(addr);
                if (new_es.inLocToValTable(objId)) {
                    new_es.load(addr).meet_with(
                            IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
                }
            }
            break;
        }
        case CmpStmt::Predicate::ICMP_ULE:
        case CmpStmt::Predicate::ICMP_SLE:
        case CmpStmt::Predicate::FCMP_OLE:
        case CmpStmt::Predicate::FCMP_ULE: {
            // Var <= Const, so [var.lb, var.ub].meet_with([-INF, const.ub])
            lhs.meet_with(IntervalValue(IntervalValue::minus_infinity(), rhs.ub()));
            // if lhs is register value, we should also change its mem obj
            for (const auto &addr: addrs) {
                NodeID objId = new_es.getInternalID(addr);
                if (new_es.inLocToValTable(objId)) {
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
    es = new_es;
    return true;
}

bool AE::hasSwitchBranchES(const SVFVar* var, s64_t succ, IntervalExeState& es) {
    IntervalExeState new_es = es;
    new_es.cpyItvToLocal(var->getId());
    IntervalValue& switch_cond = new_es[var->getId()];
    s64_t value = succ;
    FIFOWorkList<const SVFStmt*> workList;
    for (SVFStmt *cmpVarInStmt: var->getInEdges()) {
        workList.push(cmpVarInStmt);
    }
    switch_cond.meet_with(IntervalValue(value, value));
    if (switch_cond.isBottom()) {
        return false;
    }
    while(!workList.empty()) {
        const SVFStmt* stmt = workList.pop();
        if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt)) {
            IntervalValue& copy_cond = new_es[var->getId()];
            copy_cond.meet_with(IntervalValue(value, value));
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt)) {
            if (new_es.inVarToAddrsTable(load->getRHSVarID())) {
                ExeState::Addrs &addrs = new_es.getAddrs(load->getRHSVarID()); //3108
                for (const auto &addr: addrs) {
                    NodeID objId = new_es.getInternalID(addr);
                    if (new_es.inLocToValTable(objId)) {
                        new_es.load(addr).meet_with(switch_cond);
                    }
                }
            }
        }
    }
    es = new_es;
    return true;
}

bool AE::hasBranchES(const IntraCFGEdge* intraEdge, IntervalExeState& es) {
    const SVFValue *cond = intraEdge->getCondition();
    NodeID cmpID = _svfir->getValueNode(cond);
    SVFVar *cmpVar = _svfir->getGNode(cmpID);
    if (cmpVar->getInEdges().empty()) {
        return hasSwitchBranchES(cmpVar, intraEdge->getSuccessorCondValue(), es);
    } else {
        assert(!cmpVar->getInEdges().empty() &&
               "no in edges?");
        SVFStmt *cmpVarInStmt = *cmpVar->getInEdges().begin();
        if (const CmpStmt *cmpStmt = SVFUtil::dyn_cast<CmpStmt>(cmpVarInStmt)) {
            return hasCmpBranchES(cmpStmt, intraEdge->getSuccessorCondValue(), es);
        } else {
            return hasSwitchBranchES(cmpVar, intraEdge->getSuccessorCondValue(), es);
        }
    }
    return true;
}
/// handle instructions in svf basic blocks
void AE::handleBlock(const CFBasicBlockNode *block) {
    _stat->getBlockTrace()++;
    // Get execution states from in edges
    if (!hasInEdgesES(block)) {
        // No ES on the in edges - Infeasible block
        return;
    } else {
        // Has ES on the in edges - Feasible block
        // Get execution state from in edges
        _svfir2ExeState->setEs(_preES[block]);
    }

    std::deque<const ICFGNode*> worklist;
    for (auto it = block->begin(); it != block->end(); ++it) {
        worklist.push_back(*it);
    }
    while(!worklist.empty()) {
        const ICFGNode* curICFGNode = worklist.front();
        worklist.pop_front();
        handleICFGNode(curICFGNode);
    }
    _preES.erase(block);
    _postES[block] = _svfir2ExeState->getEs();
}

void AE::handleCallSite(const ICFGNode* node) {
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node)) {
        if (isExtCall(callNode)) {
            extCallPass(callNode);
        }
        else if (isRecursiveCall(callNode)) {
            recursiveCallPass(callNode);
        }
        else if (isDirectCall(callNode)) {
            directCallFunPass(callNode);
        }
        else if (isIndirectCall(callNode)) {
            indirectCallFunPass(callNode);
        }
        else {
            assert(false && "implement this part");
        }
    }
    else {
        assert (false && "it is not call node");
    }
}

bool AE::isExtCall(const SVF::CallICFGNode *callNode) {
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return SVFUtil::isExtCall(callfun);
}

void AE::extCallPass(const SVF::CallICFGNode *callNode) {
    _callSiteStack.push_back(callNode);
    _api->handleExtAPI(callNode);
    _callSiteStack.pop_back();
}

bool AE::isRecursiveCall(const SVF::CallICFGNode *callNode) {
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return _recursiveFuns.find(callfun) != _recursiveFuns.end();
}

void AE::recursiveCallPass(const SVF::CallICFGNode *callNode) {
    SkipRecursiveCall(callNode);
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0) {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin())) {
            if (!retPE->getLHSVar()->isPointer() &&
                !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr()) {
                _svfir2ExeState->getEs()[retPE->getLHSVarID()] = IntervalValue::top();
            }
        }
    }
}

bool AE::isDirectCall(const SVF::CallICFGNode *callNode) {
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return _funcToWTO.find(callfun) != _funcToWTO.end();
}
void AE::directCallFunPass(const SVF::CallICFGNode *callNode) {
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    IntervalExeState preES = _svfir2ExeState->getEs();
    _callSiteStack.push_back(callNode);

    auto* curBlockNode = _CFBlockG->getCFBasicBlockNode(callNode->getId());
    _postES[curBlockNode] = _svfir2ExeState->getEs();

    handleFunc(callfun);
    _callSiteStack.pop_back();
    // handle Ret node
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    // resume ES to callnode
    _postES[_CFBlockG->getCFBasicBlockNode(retNode->getId())] = _postES[_CFBlockG->getCFBasicBlockNode(callNode->getId())];
}

bool AE::isIndirectCall(const SVF::CallICFGNode *callNode) {
    const auto callsiteMaps = _svfir->getIndirectCallsites();
    return callsiteMaps.find(callNode) != callsiteMaps.end();
}

void AE::indirectCallFunPass(const SVF::CallICFGNode *callNode) {
    const auto callsiteMaps = _svfir->getIndirectCallsites();
    NodeID call_id = callsiteMaps.at(callNode);
    if (!_svfir2ExeState->getEs().inVarToAddrsTable(call_id)) {
        return;
    }
    ExeState::Addrs Addrs = _svfir2ExeState->getAddrs(call_id);
    NodeID addr = *Addrs.begin();
    SVFVar *func_var = _svfir->getGNode(_svfir2ExeState->getInternalID(addr));
    const SVFFunction *callfun = SVFUtil::dyn_cast<SVFFunction>(func_var->getValue());
    if (callfun) {
        IntervalExeState preES = _svfir2ExeState->getEs();
        _callSiteStack.push_back(callNode);
        auto *curBlockNode = _CFBlockG->getCFBasicBlockNode(callNode->getId());

        _postES[curBlockNode] = _svfir2ExeState->getEs();

        handleFunc(callfun);
        _callSiteStack.pop_back();
        // handle Ret node
        const RetICFGNode *retNode = callNode->getRetICFGNode();
        _postES[_CFBlockG->getCFBasicBlockNode(retNode->getId())] = _postES[_CFBlockG->getCFBasicBlockNode(callNode->getId())];
    }
}



void AE::handleICFGNode(const ICFGNode *curICFGNode) {
    _stat->getICFGNodeTrace()++;
    // handle SVF Stmt
    for (const SVFStmt *stmt: curICFGNode->getSVFStmts()) {
        handleSVFStatement(stmt);
    }
    // inlining the callee by calling handleFunc for the callee function
    if (const CallICFGNode* callnode = SVFUtil::dyn_cast<CallICFGNode>(curICFGNode)) {
        handleCallSite(callnode);
    } else {

    }
    _stat->countStateSize();
}

/// handle wto cycle (loop)
void AE::handleCycle(const CFBasicBlockGWTOCycle *cycle) {
    // Get execution states from in edges
    if (!hasInEdgesES(cycle->head())) {
        // No ES on the in edges - Infeasible block
        return;
    }
    IntervalExeState pre_es = _preES[cycle->head()];
    // set -widen-delay
    s32_t widen_delay = Options::WidenDelay();
    bool incresing = true;
    for (int i = 0; ; i++) {
        const CFBasicBlockNode* cycle_head = cycle->head();
        // handle cycle head
        handleBlock(cycle_head);
        if (i < widen_delay) {
            if (i> 0 && pre_es >= _postES[cycle_head]) {
                break;
            }
            pre_es = _postES[cycle_head];
        }
        else {
            if (i >= widen_delay) {
                if (incresing) {
                    bool is_fixpoint = widenFixpointPass(cycle_head, pre_es);
                    if (is_fixpoint)
                        incresing = false;
                }
                if (!incresing) {
                    bool is_fixpoint = narrowFixpointPass(cycle_head, pre_es);
                    if (is_fixpoint)
                        break;
                }
            }
        }
        for (auto it = cycle->begin(); it != cycle->end(); ++it) {
            const CFBasicBlockGWTOComp* cur = *it;
            if (const CFBasicBlockGWTONode* vertex = SVFUtil::dyn_cast<CFBasicBlockGWTONode>(cur)) {
                handleBlock(vertex->node());
            }
            else if (const CFBasicBlockGWTOCycle* cycle = SVFUtil::dyn_cast<CFBasicBlockGWTOCycle>(cur)) {
                handleCycle(cycle);
            } else {
                assert(false && "unknown WTO type!");
            }
        }
    }
}

bool AE::widenFixpointPass(const CFBasicBlockNode* cycle_head, IntervalExeState& pre_es) {
    // increasing iterations
    IntervalExeState new_pre_es = pre_es.widening(_postES[cycle_head]);
    IntervalExeState new_pre_vaddr_es = new_pre_es;
    _svfir2ExeState->widenAddrs(new_pre_es, _postES[cycle_head]);

    if (pre_es >= new_pre_es) {
        // increasing iterations - fixpoint reached
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return true;
    } else {
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return false;
    }
}

bool AE::narrowFixpointPass(const SVF::CFBasicBlockNode *cycle_head, SVF::IntervalExeState &pre_es) {
    // decreasing iterations
    IntervalExeState new_pre_es = pre_es.narrowing(_postES[cycle_head]);
    IntervalExeState new_pre_vaddr_es = new_pre_es;
    _svfir2ExeState->narrowAddrs(new_pre_es, _postES[cycle_head]);
    if (new_pre_es >= pre_es) {
        // decreasing iterations - fixpoint reached
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return true;
    }
    else {
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return false;
    }
}



/// handle user defined function, ext function is not included.
void AE::handleFunc(const SVFFunction *func) {
    _stat->getFunctionTrace()++;
    CFBasicBlockGWTO* wto = _funcToWTO[func];
    // set function entry ES
    for (auto it = wto->begin(); it!= wto->end(); ++it) {
        const CFBasicBlockGWTOComp* cur = *it;
        if (const CFBasicBlockGWTONode* vertex = SVFUtil::dyn_cast<CFBasicBlockGWTONode>(cur)) {
            handleBlock(vertex->node());
        }
        else if (const CFBasicBlockGWTOCycle* cycle = SVFUtil::dyn_cast<CFBasicBlockGWTOCycle>(cur)) {
            handleCycle(cycle);
        } else {
            assert(false && "unknown WTO type!");
        }
    }
}


void AE::handleSVFStatement(const SVFStmt *stmt) {
    if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt)) {
        _svfir2ExeState->translateAddr(addr);
    } else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt)) {
        _svfir2ExeState->translateBinary(binary);
    } else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt)) {
        _svfir2ExeState->translateCmp(cmp);
    } else if (const UnaryOPStmt *unary = SVFUtil::dyn_cast<UnaryOPStmt>(stmt)) {
    } else if (const BranchStmt *br = SVFUtil::dyn_cast<BranchStmt>(stmt)) {
        // branch stmt is handled in hasBranchES
    } else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt)) {
        _svfir2ExeState->translateLoad(load);
    } else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt)) {
        _svfir2ExeState->translateStore(store);
    } else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt)) {
        _svfir2ExeState->translateCopy(copy);
    } else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt)) {
        _svfir2ExeState->translateGep(gep);
    } else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt)) {
        _svfir2ExeState->translateSelect(select);
    } else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt)) {
        _svfir2ExeState->translatePhi(phi);
    } else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt)) {
        // To handle Call Edge
        _svfir2ExeState->translateCall(callPE);
    } else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt)) {
        _svfir2ExeState->translateRet(retPE);
    } else
        assert(false && "implement this part");
}


void AE::SkipRecursiveCall(const CallICFGNode *callNode) {
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0) {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin())) {
            IntervalExeState es;
            if (!retPE->getLHSVar()->isPointer() && !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
                _svfir2ExeState->getEs()[retPE->getLHSVarID()] = IntervalValue::top();
        }
    }
    if (!retNode->getOutEdges().empty()) {
        if (retNode->getOutEdges().size() == 1) {

        } else {
            return;
        }
    }
    SkipRecursiveFunc(callfun);
}

void AE::SkipRecursiveFunc(const SVFFunction *func) {
    // handle Recursive Funcs, go throw every relevant funcs/blocks.
    // for every Call Argv, Ret , Global Vars, we make it as Top value
    FIFOWorkList<const SVFBasicBlock *> blkWorkList;
    FIFOWorkList<const ICFGNode *> instWorklist;
    for (const SVFBasicBlock * bb: func->getReachableBBs()) {
        for (const SVFInstruction* inst: bb->getInstructionList()) {
            const ICFGNode* node = _icfg->getICFGNode(inst);
            for (const SVFStmt *stmt: node->getSVFStmts()) {
                if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt)) {
                    const SVFVar *rhsVar = store->getRHSVar();
                    u32_t lhs = store->getLHSVarID();
                    IntervalExeState &curES = _svfir2ExeState->getEs();
                    if (curES.inVarToAddrsTable(lhs)) {
                        if (!rhsVar->isPointer() && !rhsVar->isConstDataOrAggDataButNotNullPtr()) {
                            const SVFIR2ItvExeState::Addrs &addrs =curES.getAddrs(lhs);
                            assert(!addrs.empty());
                            for (const auto &addr: addrs) {
                                curES.store(addr, IntervalValue::top());
                            }
                        }
                    }
                }
            }
        }
    }
}

// count the size of memory map
void AEStat::countStateSize() {
    if (count == 0) {
        generalNumMap["Global_ES_Var_AVG_Num"] = IntervalExeState::globalES.getVarToVal().size();
        generalNumMap["Global_ES_Loc_AVG_Num"] = IntervalExeState::globalES.getLocToVal().size();
        generalNumMap["Global_ES_Var_Addr_AVG_Num"] = IntervalExeState::globalES.getVarToAddrs().size();
        generalNumMap["Global_ES_Loc_Addr_AVG_Num"] = IntervalExeState::globalES.getLocToAddrs().size();
        generalNumMap["ES_Var_AVG_Num"] = 0;
        generalNumMap["ES_Loc_AVG_Num"] = 0;
        generalNumMap["ES_Var_Addr_AVG_Num"] = 0;
        generalNumMap["ES_Loc_Addr_AVG_Num"] = 0;
    }
    ++count;
    generalNumMap["ES_Var_AVG_Num"] += _ae->_svfir2ExeState->getEs().getVarToVal().size();
    generalNumMap["ES_Loc_AVG_Num"] += _ae->_svfir2ExeState->getEs().getLocToVal().size();
    generalNumMap["ES_Var_Addr_AVG_Num"] += _ae->_svfir2ExeState->getEs().getVarToAddrs().size();
    generalNumMap["ES_Loc_Addr_AVG_Num"] += _ae->_svfir2ExeState->getEs().getLocToAddrs().size();
}

void AEStat::finializeStat() {
    memUsage = getMemUsage();
    if (count > 0) {
        generalNumMap["ES_Var_AVG_Num"] /= count;
        generalNumMap["ES_Loc_AVG_Num"] /= count;
        generalNumMap["ES_Var_Addr_AVG_Num"] /= count;
        generalNumMap["ES_Loc_Addr_AVG_Num"] /= count;
    }
    generalNumMap["SVF_STMT_NUM"] = count;
    generalNumMap["ICFG_Node_Num"] = _ae->_svfir->getICFG()->nodeNum;
    u32_t callSiteNum = 0;
    u32_t extCallSiteNum = 0;
    Set<const SVFFunction *> funs;
    for (const auto &it: *_ae->_svfir->getICFG()) {
        if (it.second->getFun()) {
            funs.insert(it.second->getFun());
        }
        if (const CallICFGNode *callNode = dyn_cast<CallICFGNode>(it.second)) {
            if (!isExtCall(callNode->getCallSite())) {
                callSiteNum++;
            } else {
                extCallSiteNum++;
            }
        }
    }
    generalNumMap["Func_Num"] = funs.size();
    generalNumMap["EXT_CallSite_Num"] = extCallSiteNum;
    generalNumMap["NonEXT_CallSite_Num"] = callSiteNum;
    generalNumMap["VarToAddrSize"] = _ae->_svfir2ExeState->getEs().getVarToAddrs().size();
    generalNumMap["LocToAddrSize"] = _ae->_svfir2ExeState->getEs().getLocToAddrs().size();
    generalNumMap["Bug_Num"] = _ae->_nodeToBugInfo.size();
    timeStatMap["Total_Time(sec)"] = (double)(endTime - startTime) / TIMEINTERVAL;

}

void AEStat::performStat() {
    std::string fullName(_ae->_moduleName);
    std::string name;
    std::string moduleName;
    if (fullName.find('/') == std::string::npos) {
        std::string name = fullName;
        moduleName = name.substr(0, fullName.find('.'));
    } else {
        std::string name = fullName.substr(fullName.find('/'), fullName.size());
        moduleName = name.substr(0, fullName.find('.'));
    }

    SVFUtil::outs() << "\n************************\n";
    SVFUtil::outs() << "################ (program : " << moduleName << ")###############\n";
    SVFUtil::outs().flags(std::ios::left);
    unsigned field_width = 30;
    for (NUMStatMap::iterator it = generalNumMap.begin(), eit = generalNumMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        std::cout << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "-------------------------------------------------------\n";
    for (TIMEStatMap::iterator it = timeStatMap.begin(), eit = timeStatMap.end(); it != eit; ++it) {
        // format out put with width 20 space
        SVFUtil::outs() << std::setw(field_width) << it->first << it->second << "\n";
    }
    SVFUtil::outs() << "Memory usage: " << memUsage << "\n";

    SVFUtil::outs() << "#######################################################" << std::endl;
    SVFUtil::outs().flush();
}

void AEStat::reportBug() {

    std::ofstream f;
    if (Options::OutputName().size() == 0) {
        f.open("/dev/null");
    } else {
        f.open(Options::OutputName());
    }

    std::cerr << "######################Full Overflow (" + std::to_string(_ae->_nodeToBugInfo.size()) + " found)######################\n";
    f << "######################Full Overflow (" + std::to_string(_ae->_nodeToBugInfo.size()) + " found)######################\n";
    std::cerr << "---------------------------------------------\n";
    f << "---------------------------------------------\n";
    for (auto& it: _ae->_nodeToBugInfo) {
        std::cerr << it.second << "---------------------------------------------\n";
        f << it.second << "---------------------------------------------\n";
    }
}

void AEAPI::initExtFunMap() {
#define SSE_FUNC_PROCESS(LLVM_NAME ,FUNC_NAME) \
        auto sse_##FUNC_NAME = [this](const CallSite &cs) { \
        /* run real ext function */ \
        IntervalExeState &es = _ae->_svfir2ExeState->getEs(); \
        u32_t rhs_id = _svfir->getValueNode(cs.getArgument(0)); \
        if (!es.inVarToValTable(rhs_id)) return; \
        u32_t rhs = _ae->_svfir2ExeState->getEs()[rhs_id].lb().getNumeral(); \
        s32_t res = FUNC_NAME(rhs);            \
        u32_t lhsId = _svfir->getValueNode(cs.getInstruction()); \
        _ae->_svfir2ExeState->getEs()[lhsId] = IntervalValue(res); \
    };                              \
    _func_map[#FUNC_NAME] = sse_##FUNC_NAME;  \

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

    auto sse_svf_assert = [this](const CallSite &cs) {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        _checkpoints.erase(callNode);
        u32_t arg0 = _svfir->getValueNode(cs.getArgument(0));
        IntervalExeState &es = _ae->_svfir2ExeState->getEs();
        es[arg0].meet_with(IntervalValue(1, 1));
        if (es[arg0].equals(IntervalValue(1, 1))) {
            SVFUtil::outs() << SVFUtil::sucMsg("The assertion is successfully verified!!\n");
        } else {
            SVFUtil::errs() <<"svf_assert Fail. " << cs.getInstruction()->toString() << "\n";
            assert(false);
        }
    };
    _func_map["svf_assert"] = sse_svf_assert;

    auto svf_print = [&](const CallSite &cs) {
        if (cs.arg_size() < 2) return;
        IntervalExeState &es = _ae->_svfir2ExeState->getEs();
        u32_t num_id = _svfir->getValueNode(cs.getArgument(0));
        std::string text = strRead(cs.getArgument(1));
        assert(es.inVarToValTable(num_id) && "print() should pass integer");
        IntervalValue itv = es[num_id];
        std::cout << "Text: " << text <<", Value: " << cs.getArgument(0)->toString() << ", PrintVal: " << itv.toString() << std::endl;
    };
    _func_map["svf_print"] = svf_print;

    // init _checkpoint_names
    _checkpoint_names.insert("svf_assert");
};

std::string AEAPI::strRead(const SVFValue* rhs) {
    // sse read string nodeID->string
    IntervalExeState &es = _ae->_svfir2ExeState->getEs();
    std::string str0;

    for (u32_t index = 0; index < Options::MaxFieldLimit(); index++) {
        // dead loop for string and break if there's a \0. If no \0, it will throw err.
        if (!es.inVarToAddrsTable(_svfir->getValueNode(rhs))) continue;
        Addrs expr0 = _ae->_svfir2ExeState->getGepObjAddress(_svfir->getValueNode(rhs), index);
        IntervalValue val = IntervalValue::bottom();
        for (const auto &addr: expr0) {
            val.join_with(es.load(addr));
        }
        if (!val.is_numeral()) {
            break;
        }
        if ((char) val.getNumeral() == '\0') {
            break;
        }
        str0.push_back((char) val.getNumeral());
    }
    return str0;
}

void AEAPI::handleExtAPI(const CallICFGNode *call) {
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    assert(fun && "SVFFunction* is nullptr");
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    ExtAPIType extType = UNCLASSIFIED;
    // get type of mem api
    for (const std::string &annotation: fun->getAnnotations()) {
        if (annotation.find("MEMCPY") != std::string::npos)
            extType =  MEMCPY;
        if (annotation.find("MEMSET") != std::string::npos)
            extType =  MEMSET;
        if (annotation.find("STRCPY") != std::string::npos)
            extType = STRCPY;
        if (annotation.find("STRCAT") != std::string::npos)
            extType =  STRCAT;
    }
    if (extType == UNCLASSIFIED) {
        if (_func_map.find(fun->getName()) != _func_map.end()) {
            _func_map[fun->getName()](cs);
        } else {
            u32_t lhsId = _svfir->getValueNode(SVFUtil::getSVFCallSite(call->getCallSite()).getInstruction());
            if (_ae->_svfir2ExeState->getEs().inVarToAddrsTable(lhsId)) {

            } else {
                _ae->_svfir2ExeState->getEs()[lhsId] = IntervalValue();
            }
            return;
        }
    }
        // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    else if (extType == MEMCPY) {
        IntervalValue len = _ae->_svfir2ExeState->getEs()[_svfir->getValueNode(cs.getArgument(2))];
        handleMemcpy(cs.getArgument(0), cs.getArgument(1), len, 0);
    }
    else if (extType == MEMSET) {
        // memset dst is arg0, elem is arg1, size is arg2
        IntervalValue len = _ae->_svfir2ExeState->getEs()[_svfir->getValueNode(cs.getArgument(2))];
        IntervalValue elem = _ae->_svfir2ExeState->getEs()[_svfir->getValueNode(cs.getArgument(1))];
        handleMemset(cs.getArgument(0), elem, len);
    }
    else if (extType == STRCPY) {
        handleStrcpy(call);
    }
    else if (extType == STRCAT) {
        handleStrcat(call);
    }
    else {

    }
}

void AEAPI::collectCheckPoint() {
    // traverse every ICFGNode
    for (auto it = _ae->_svfir->getICFG()->begin(); it != _ae->_svfir->getICFG()->end(); ++it) {
        const ICFGNode* node = it->second;
        if (const CallICFGNode *call = SVFUtil::dyn_cast<CallICFGNode>(node)) {
            if (const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite())) {
                if (_checkpoint_names.find(fun->getName()) != _checkpoint_names.end()) {
                    _checkpoints.insert(call);
                }
            }
        }
    }
}

void AEAPI::checkPointAllSet() {
    if (_checkpoints.size() == 0) {
        return;
    }
    else {
        SVFUtil::errs() << SVFUtil::sucMsg("There exists checkpoints not checked!!\n");
        for (const CallICFGNode* call: _checkpoints) {
            SVFUtil::errs() << SVFUtil::sucMsg(call->toString() + "\n");
        }
    }

}


void AEAPI::handleStrcpy(const CallICFGNode *call) {
    // strcpy, __strcpy_chk, stpcpy , wcscpy, __wcscpy_chk
    // get the dst and src
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    const SVFValue* arg0Val = cs.getArgument(0);
    const SVFValue* arg1Val = cs.getArgument(1);
    IntervalValue strLen = getStrlen(arg1Val);
    // no need to -1, since it has \0 as the last byte
    handleMemcpy(arg0Val, arg1Val, strLen,strLen.lb().getNumeral());
}

u32_t AEAPI::getAllocaInstByteSize(const AddrStmt *addr)  {
    if (const ObjVar* objvar = SVFUtil::dyn_cast<ObjVar>(addr->getRHSVar())) {
        objvar->getType();
        if (objvar->getMemObj()->isConstantByteSize()) {
            u32_t sz = objvar->getMemObj()->getByteSizeOfObj();
            return sz;
        }

        else {
            const std::vector<SVFValue*>& sizes = addr->getArrSize();
            // Default element size is set to 1.
            u32_t elementSize = 1;
            u64_t res = elementSize;
            for (const SVFValue* value: sizes) {
                if (!_ae->_svfir2ExeState->inVarToValTable(_svfir->getValueNode(value))) {
                    _ae->_svfir2ExeState->getEs()[_svfir->getValueNode(value)] = IntervalValue(Options::MaxFieldLimit());
                }
                IntervalValue itv = _ae->_svfir2ExeState->getEs()[_svfir->getValueNode(value)];
                res = res * itv.ub().getNumeral() > Options::MaxFieldLimit()? Options::MaxFieldLimit(): res * itv.ub().getNumeral();
            }
            return (u32_t)res;
        }
    }
    assert (false && "Addr rhs value is not ObjVar");
}

IntervalValue AEAPI::traceMemoryAllocationSize(const SVFValue *value) {
    /// Usually called by a GepStmt overflow check, or external API (like memcpy) overflow check
    /// Defitions of Terms:
    /// source node: malloc or gepStmt(array), sink node: gepStmt or external API (like memcpy)
    /// it tracks the value flow from sink to source, and accumulates offset
    /// then compare the accumulated offset and malloc size (or gepStmt array size)
    SVF::FILOWorkList<const SVFValue *> worklist;
    Set<const SVFValue *> visited;
    visited.insert(value);
    Map<const ICFGNode *, IntervalValue> gep_offsets;
    worklist.push(value);
    IntervalValue total_bytes(0);
    while (!worklist.empty()) {
        value = worklist.pop();
        if (const SVFInstruction* ins = SVFUtil::dyn_cast<SVFInstruction>(value)) {
            const ICFGNode* node = _svfir->getICFG()->getICFGNode(ins);
            /// CallNode means Source Node
            if (const CallICFGNode* callnode = SVFUtil::dyn_cast<CallICFGNode>(node)) {
                //to handle Ret PE
                AccessMemoryViaRetNode(callnode, worklist, visited);
            }
            for (const SVFStmt *stmt: node->getSVFStmts()) {
                if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt)) {
                    // Copy Stmt, forward to lhs
                    AccessMemoryViaCopyStmt(copy, worklist, visited);
                }
                else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt)) {
                    // Load Stmt, forward to the Var from last Store Stmt
                    AccessMemoryViaLoadStmt(load, worklist, visited);
                }
                else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt)) {
                    // there are 3 type of gepStmt
                    // 1. ptr get offset
                    // 2. struct get field
                    // 3. array get element
                    // for array gep, there are two kind of overflow checking
                    //  Arr [Struct.C * 10] arr, Struct.C {i32 a, i32 b}
                    //     arr[11].a = **, it is "lhs = gep *arr, 0 (ptr), 11 (arrIdx), 0 (ptr), 0(struct field)"
                    //  1) in this case arrIdx 11 is overflow.
                    //  Other case,
                    //   Struct.C {i32 a, [i32*10] b, i32 c}, C.b[11] = 1
                    //   it is "lhs - gep *C, 0(ptr), 1(struct field), 0(ptr), 11(arrIdx)"
                    //  2) in this case arrIdx 11 is larger than its getOffsetVar.Type Array([i32*10])

                    // therefore, if last getOffsetVar.Type is not the Array, just check the overall offset and its
                    // gep source type size (together with totalOffset along the value flow).
                    // Alloc Size: TBD, but totalOffset + current Gep offset

                    // otherwise, if last getOffsetVar.Type is the Array, check the last idx and array. (just offset,
                    //  not with totalOffset during check)
                    // Alloc Size: getOffsetVar.TypeByteSize()

                    // make sure it has OffsetVarAndGepType Pair
                    if (gep->getOffsetVarAndGepTypePairVec().size() > 0) {
                        // check if last OffsetVarAndGepType Pair is Array
                        const SVFType* gepType = gep->getOffsetVarAndGepTypePairVec().back().second;
                        // if its array
                        if (gepType->isArrayTy()) {
                            u32_t rhs_type_bytes = gepType->getByteSize();
                            // if gepStmt's base var is Array, compares offset with the arraysize
                            return IntervalValue(rhs_type_bytes);
                        }
                        else {
                            IntervalValue byteOffset;
                            if (gep->isConstantOffset()) {
                                byteOffset = IntervalValue(gep->accumulateConstantByteOffset());
                            }
                            else {
                                IntervalValue byteOffset = _ae->_svfir2ExeState->getByteOffset(gep);
                            }
                            // for variable offset, join with accumulate gep offset
                            gep_offsets[gep->getICFGNode()] = byteOffset;
                            total_bytes = total_bytes + byteOffset;
                        }
                    }
                    if (!visited.count(gep->getRHSVar()->getValue())) {
                        visited.insert(gep->getRHSVar()->getValue());
                        worklist.push(gep->getRHSVar()->getValue());
                    }
                }
                else if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt)) {
                    // addrStmt is source node.
                    u32_t arr_type_size = getAllocaInstByteSize(addr);
                    return IntervalValue(arr_type_size) - total_bytes;
                }
            }
        }
        else if (const SVF::SVFGlobalValue* gvalue = SVFUtil::dyn_cast<SVF::SVFGlobalValue>(value)) {
            u32_t arr_type_size = 0;
            const SVFType* svftype = gvalue->getType();
            if (const SVFPointerType* svfPtrType = SVFUtil::dyn_cast<SVFPointerType>(svftype)) {
                if(const SVFArrayType* ptrArrType = SVFUtil::dyn_cast<SVFArrayType>(getPointeeElement(_svfir->getValueNode(value))))
                    arr_type_size = ptrArrType->getByteSize();
                else
                    arr_type_size = svftype->getByteSize();
            } else
                arr_type_size = svftype->getByteSize();
            return IntervalValue(arr_type_size) - total_bytes;
        }
        else if (const SVF::SVFArgument* arg = SVFUtil::dyn_cast<SVF::SVFArgument>(value)) {
            // to handle call PE
            AccessMemoryViaCallArgs(arg, worklist, visited);
        }
        else {
            // maybe SVFConstant
            return IntervalValue(0);
        }
    }
    return IntervalValue(0);
}


IntervalValue AEAPI::getStrlen(const SVF::SVFValue *strValue) {
    IntervalExeState &es = _ae->_svfir2ExeState->getEs();
    IntervalValue dst_size = traceMemoryAllocationSize(strValue);
    u32_t len = 0;
    NodeID dstid = _svfir->getValueNode(strValue);
    u32_t elemSize = 1;
    if (_ae->_svfir2ExeState->inVarToAddrsTable(dstid)) {
        for (u32_t index = 0; index < dst_size.lb().getNumeral(); index++) {
            Addrs expr0 = _ae->_svfir2ExeState->getGepObjAddress(dstid, index);
            IntervalValue val = IntervalValue::bottom();
            for (const auto &addr: expr0) {
                val.join_with(es.load(addr));
            }
            if (val.is_numeral() && (char) val.getNumeral() == '\0') {
                break;
            }
            ++len;
        }
        if (strValue->getType()->isArrayTy()) {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(strValue->getType())->getTypeOfElement()->getByteSize();
        } else if (strValue->getType()->isPointerTy()) {
            if (const SVFType* elemType = getPointeeElement(_svfir->getValueNode(strValue))) {
                elemSize = elemType->getByteSize();
            }
            else {
                elemSize = 1;
            }
        } else {
            assert(false && "we cannot support this type");
        }
    }
    if (len == 0) {
        return IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit());
    } else {
        return IntervalValue(len * elemSize);
    }
}


void AEAPI::handleStrcat(const SVF::CallICFGNode *call) {
    // __strcat_chk, strcat, __wcscat_chk, wcscat, __strncat_chk, strncat, __wcsncat_chk, wcsncat
    // to check it is  strcat group or strncat group
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};
    if (std::find(strcatGroup.begin(), strcatGroup.end(), fun->getName()) != strcatGroup.end()) {
        CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg1Val = cs.getArgument(1);
        IntervalValue strLen0 = getStrlen(arg0Val);
        IntervalValue strLen1 = getStrlen(arg1Val);
        IntervalValue totalLen = strLen0 + strLen1;
        handleMemcpy(arg0Val, arg1Val, strLen1, strLen0.lb().getNumeral());
        // do memcpy
    } else if (std::find(strncatGroup.begin(), strncatGroup.end(), fun->getName()) != strncatGroup.end()) {
        CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg1Val = cs.getArgument(1);
        const SVFValue* arg2Val = cs.getArgument(2);
        IntervalValue arg2Num = _ae->_svfir2ExeState->getEs()[_svfir->getValueNode(arg2Val)];
        IntervalValue strLen0 = getStrlen(arg0Val);
        IntervalValue totalLen = strLen0 + arg2Num;
        handleMemcpy(arg0Val, arg1Val, arg2Num, strLen0.lb().getNumeral());
        // do memcpy
    } else {
        assert(false && "unknown strcat function, please add it to strcatGroup or strncatGroup");
    }
}

void AEAPI::handleMemcpy(const SVF::SVFValue *dst, const SVF::SVFValue *src, SVF::IntervalValue len,  u32_t start_idx) {
    IntervalExeState &es = _ae->_svfir2ExeState->getEs();
    u32_t dstId = _svfir->getValueNode(dst); // pts(dstId) = {objid}  objbar objtypeinfo->getType().
    u32_t srcId = _svfir->getValueNode(src);
    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy()) {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())->getTypeOfElement()->getByteSize();
    }
    // memcpy(i32*, i32*, 40)
    else if (dst->getType()->isPointerTy()) {
        if (const SVFType* elemType = getPointeeElement(_svfir->getValueNode(dst))) {
            if (elemType->isArrayTy())
                elemSize = SVFUtil::dyn_cast<SVFArrayType>(elemType)->getTypeOfElement()->getByteSize();
            else
                elemSize = elemType->getByteSize();
        }
        else {
            elemSize = 1;
        }
    }
    else {
        assert(false && "we cannot support this type");
    }
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(), (u32_t) len.lb().getNumeral());
    u32_t range_val = size / elemSize;
    if (_ae->_svfir2ExeState->inVarToAddrsTable(srcId) && _ae->_svfir2ExeState->inVarToAddrsTable(dstId)) {
        for (u32_t index = 0; index < range_val; index++) {
            // dead loop for string and break if there's a \0. If no \0, it will throw err.
            Addrs expr_src = _ae->_svfir2ExeState->getGepObjAddress(srcId, index);
            Addrs expr_dst = _ae->_svfir2ExeState->getGepObjAddress(dstId, index + start_idx);
            for (const auto &dst: expr_dst) {
                for (const auto &src: expr_src) {
                    u32_t objId = ExeState::getInternalID(src);
                    if (es.inLocToValTable(objId)) {
                        es.store(dst, es.load(src));
                    } else if (es.inLocToAddrsTable(objId)) {
                        es.storeAddrs(dst, es.loadAddrs(src));
                    }
                }
            }
        }
    }
}

const SVFType* AEAPI::getPointeeElement(NodeID id) {
    assert(_ae->_svfir2ExeState->inVarToAddrsTable(id) && "id is not in varToAddrsTable");
    if (_ae->_svfir2ExeState->inVarToAddrsTable(id)) {
        const Addrs& addrs = _ae->_svfir2ExeState->getAddrs(id);
        for (auto addr: addrs) {
            NodeID addr_id = _ae->_svfir2ExeState->getInternalID(addr);
            if (addr_id == 0) // nullptr has no memobj, skip
                continue;
            return SVFUtil::dyn_cast<ObjVar>(_svfir->getGNode(addr_id))->getMemObj()->getType();
        }
    }
    return nullptr;
}

void AEAPI::handleMemset(const SVF::SVFValue *dst, SVF::IntervalValue elem, SVF::IntervalValue len) {
    IntervalExeState &es = _ae->_svfir2ExeState->getEs();
    u32_t dstId = _svfir->getValueNode(dst);
    u32_t size = std::min((u32_t)Options::MaxFieldLimit(), (u32_t) len.lb().getNumeral());
    u32_t elemSize = 1;
    if (dst->getType()->isArrayTy()) {
        elemSize = SVFUtil::dyn_cast<SVFArrayType>(dst->getType())->getTypeOfElement()->getByteSize();
    }
    else if (dst->getType()->isPointerTy()) {
        if (const SVFType* elemType = getPointeeElement(_svfir->getValueNode(dst))) {
            elemSize = elemType->getByteSize();
        }
        else {
            elemSize = 1;
        }
    }
    else {
        assert(false && "we cannot support this type");
    }

    u32_t range_val = size / elemSize;
    for (u32_t index = 0; index < range_val; index++) {
        // dead loop for string and break if there's a \0. If no \0, it will throw err.
        if (_ae->_svfir2ExeState->inVarToAddrsTable(dstId)) {
            Addrs lhs_gep = _ae->_svfir2ExeState->getGepObjAddress(dstId, index);
            for (const auto &addr: lhs_gep) {
                u32_t objId = ExeState::getInternalID(addr);
                if (es.inLocToValTable(objId)) {
                    IntervalValue tmp = es.load(addr);
                    tmp.join_with(IntervalValue(elem));
                    es.store(addr, tmp);
                } else {
                    es.store(addr, IntervalValue(elem));
                }
            }
        } else
            break;
    }
}



void AEAPI::AccessMemoryViaRetNode(const CallICFGNode *callnode, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited) {
    if (callnode->getRetICFGNode()->getSVFStmts().size() > 0) {
        const RetPE *ret = SVFUtil::dyn_cast<RetPE>(*callnode->getRetICFGNode()->getSVFStmts().begin());
        SVF::ValVar *ret_gnode = SVFUtil::dyn_cast<ValVar>(_svfir->getGNode(ret->getRHSVar()->getId()));
        if (ret_gnode->hasIncomingEdges(SVFStmt::PEDGEK::Phi)) {
            const SVFStmt::SVFStmtSetTy &stmt_set = ret_gnode->getIncomingEdges(SVFStmt::PEDGEK::Phi);
            for (auto it = stmt_set.begin(); it != stmt_set.end(); ++it) {
                const SVFStmt *stmt = *it;
                if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt)) {
                    if (!visited.count(phi->getOpVar(0)->getValue())) {
                        worklist.push(phi->getOpVar(0)->getValue());
                        visited.insert(phi->getOpVar(0)->getValue());
                    }
                }
            }
        }
    }
}

void AEAPI::AccessMemoryViaCopyStmt(const CopyStmt *copy, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited) {
    if (!visited.count(copy->getRHSVar()->getValue())) {
        visited.insert(copy->getRHSVar()->getValue());
        worklist.push(copy->getRHSVar()->getValue());
    }
}

void AEAPI::AccessMemoryViaLoadStmt(const LoadStmt *load, SVF::FILOWorkList<const SVFValue *>& worklist, Set<const SVFValue *>& visited) {
    if (_ae->_svfir2ExeState->inVarToAddrsTable(load->getLHSVarID())) {
        const ExeState::Addrs &Addrs = _ae->_svfir2ExeState->getAddrs(load->getLHSVarID());
        for (auto vaddr: Addrs) {
            NodeID id = _ae->_svfir2ExeState->getInternalID(vaddr);
            if (id == 0) // nullptr has no memobj, skip
                continue;
            const auto *val = _svfir->getGNode(id);
            if (!visited.count(val->getValue())) {
                visited.insert(val->getValue());
                worklist.push(val->getValue());
            }
        }
    }
}

void AEAPI::AccessMemoryViaCallArgs(const SVF::SVFArgument *arg,
                                                 SVF::FILOWorkList<const SVFValue *> &worklist,
                                                 Set<const SVF::SVFValue *> &visited) {
    std::vector<const CallICFGNode *> callstack = _ae->_callSiteStack;
    SVF::ValVar *arg_gnode = SVFUtil::cast<ValVar>(_svfir->getGNode(_svfir->getValueNode(arg)));
    if (arg_gnode->hasIncomingEdges(SVFStmt::PEDGEK::Call)) {
        while (!callstack.empty()) {
            const CallICFGNode *cur_call = callstack.back();
            callstack.pop_back();
            for (const SVFStmt *stmt: cur_call->getSVFStmts()) {
                if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt)) {
                    if (callPE->getLHSVarID() == _svfir->getValueNode(arg)) {
                        if (!SVFUtil::isa<DummyObjVar>(callPE->getRHSVar()) &&
                            !SVFUtil::isa<DummyValVar>(callPE->getRHSVar())) {
                            if (!visited.count(callPE->getRHSVar()->getValue())) {
                                visited.insert(callPE->getRHSVar()->getValue());
                                worklist.push(callPE->getRHSVar()->getValue());
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

