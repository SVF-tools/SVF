//===- AbstractExecutionICFG.cpp -- Abstract Execution---------------------------------//
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
#include "SVFIR/SVFIR.h"
#include "AE/Svfexe/AbstractExecutionICFG.h"
#include "Util/Options.h"
#include <cmath>

using namespace SVF;
using namespace SVFUtil;
using namespace z3;


void AbstractExecutionICFG::runOnModule(SVF::SVFIR *svfModule)
{
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
    _icfg->updateCallGraph(_callgraph);

    /// collect checkpoint
    _api->collectCheckPoint();

    /// if function contains callInst that call itself, it is a recursive function.
    markRecursiveFuns();
    for (const SVFFunction* fun: _svfir->getModule()->getFunctionSet())
    {
        auto *wto = new ICFGWTO(_icfg, _icfg->getFunEntryICFGNode(fun));
        wto->init();
        _funcToICFGWTO[fun] = wto;
    }
    analyse();
    _api->checkPointAllSet();
    // 5. Stop clock and report bugs
    _stat->endClk();
    _stat->finializeStat();
    if (Options::PStat())
    {
        _stat->performStat();
    }
    _stat->reportBug();
}

AbstractExecutionICFG::AbstractExecutionICFG()
{
    _stat = new AEStat(this);
}
/// Destructor
AbstractExecutionICFG::~AbstractExecutionICFG()
{

}


/// Program entry
void AbstractExecutionICFG::analyse()
{
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    if (const SVFFunction* fun = _svfir->getModule()->getSVFFunction("main"))
    {
        handleFunc(fun);
    }
}

/// get execution state by merging states of predecessor blocks
/// Scenario 1: preblock -----(intraEdge)----> block, join the preES of inEdges
/// Scenario 2: preblock -----(callEdge)----> block
bool AbstractExecutionICFG::hasInEdgesES(const ICFGNode *block)
{
    if (isGlobalEntry(block))
    {
        _preES[block] = IntervalExeState();
        return true;
    }
    // is common basic block
    else
    {
        IntervalExeState es;
        u32_t inEdgeNum = 0;
        for (auto& edge: block->getInEdges())
        {
            if (_postES.find(edge->getSrcNode()) != _postES.end())
            {
                const IntraCFGEdge *intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
                if (intraCfgEdge && intraCfgEdge->getCondition())
                {
                    IntervalExeState tmpEs = _postES[edge->getSrcNode()];
                    std::cout << "DST BLK NAME:" << intraCfgEdge->getDstNode()->getBB()->getName() << ", " << _stat->getBlockTrace() << std::endl;
                    if (hasBranchES(intraCfgEdge, tmpEs))
                    {
                        es.joinWith(tmpEs);
                        inEdgeNum++;
                    }
                    else
                    {
                        // do nothing
                    }
                }
                else
                {
                    es.joinWith(_postES[edge->getSrcNode()]);
                    inEdgeNum++;
                }
            }
            else
            {

            }
        }
        if (inEdgeNum == 0)
        {
            std::cout << "No In Edges Node: "<< block->toString() << std::endl;
            std::cout << "No In Edges BB: "<< block->getBB()->toString() << std::endl;
            return false;
        }
        else
        {
            _preES[block] = es;
            return true;
        }
    }
    assert(false && "implement this part");
}

bool AbstractExecutionICFG::isFunEntry(const SVF::ICFGNode *block)
{
    if (SVFUtil::isa<FunEntryICFGNode>(block))
    {
        if (_preES.find(block) != _preES.end())
        {
            return true;
        }
    }
    return false;
}

bool AbstractExecutionICFG::isGlobalEntry(const SVF::ICFGNode *block)
{
    for (auto *edge : _icfg->getGlobalICFGNode()->getOutEdges()) {
        if (edge->getDstNode() == block)
        {
            return true;
        }
    }
    return false;
}

/// handle instructions in svf basic blocks
void AbstractExecutionICFG::handleICFGNode(const ICFGNode *curICFGNode)
{
    _stat->getBlockTrace()++;
    // Get execution states from in edges
    if (!hasInEdgesES(curICFGNode))
    {
        // No ES on the in edges - Infeasible block
        return;
    }
    else
    {
        // Has ES on the in edges - Feasible block
        // Get execution state from in edges
        _svfir2ExeState->setEs(_preES[curICFGNode]);
    }
    std::cout << "Now ES Trace ID: " << _stat->getBlockTrace() << std::endl;
    std::cout << curICFGNode->toString() << std::endl;
    //_svfir2ExeState->getEs().printExprValues(std::cout);

    std::deque<const ICFGNode*> worklist;

    for (const SVFStmt *stmt: curICFGNode->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }
    // inlining the callee by calling handleFunc for the callee function
    if (const CallICFGNode* callnode = SVFUtil::dyn_cast<CallICFGNode>(curICFGNode))
    {
        handleCallSite(callnode);
    }
    else
    {

    }
    std::cout << "post ES Trace ID: " << _stat->getBlockTrace() << std::endl;
    //_svfir2ExeState->getEs().printExprValues(std::cout);
    _preES.erase(curICFGNode);
    _postES[curICFGNode] = _svfir2ExeState->getEs();
}

/// handle wto cycle (loop)
void AbstractExecutionICFG::handleICFGCycle(const ICFGWTOCycle *cycle)
{
    // Get execution states from in edges
    if (!hasInEdgesES(cycle->head()))
    {
        // No ES on the in edges - Infeasible block
        return;
    }
    IntervalExeState pre_es = _preES[cycle->head()];
    // set -widen-delay
    s32_t widen_delay = Options::WidenDelay();
    bool incresing = true;
    for (int i = 0; ; i++)
    {
        const ICFGNode* cycle_head = cycle->head();
        // handle cycle head
        handleICFGNode(cycle_head);
        std::cout << "PRE IN CYCLE2:\n";
        //pre_es.printExprValues(std::cout);
        if (i < widen_delay)
        {
            if (i> 0 && pre_es >= _postES[cycle_head])
            {
                break;
            }
            pre_es = _postES[cycle_head];
        }
        else
        {
            if (i >= widen_delay)
            {
                if (incresing)
                {
                    std::cout << "widen" << std::endl;
                    bool is_fixpoint = widenFixpointPass(cycle_head, pre_es);
                    if (is_fixpoint)
                    {
                        std::cout << "Widen Reach Fixed Point" << std::endl;
                        incresing = false;
                    }
                }
                else if (!incresing)
                {
                    std::cout << "narrow" << std::endl;
                    bool is_fixpoint = narrowFixpointPass(cycle_head, pre_es);
                    if (is_fixpoint)
                        break;
                }
            }
        }
        for (auto it = cycle->begin(); it != cycle->end(); ++it)
        {
            const ICFGWTOComp* cur = *it;
            if (const ICFGWTONode* vertex = SVFUtil::dyn_cast<ICFGWTONode>(cur))
            {
                handleICFGNode(vertex->node());
            }
            else if (const ICFGWTOCycle* cycle2 = SVFUtil::dyn_cast<ICFGWTOCycle>(cur))
            {
                handleICFGCycle(cycle2);
            }
            else
            {
                assert(false && "unknown WTO type!");
            }
        }
    }
//    for (const SVFInstruction* inst: cycle->head()->getBB()->getInstructionList()) {
//        const ICFGNode* node = _icfg->getICFGNode(inst);
//        if (node == cycle->head())
//            continue;
//        else
//            handleICFGNode(node);
//    }
}

bool AbstractExecutionICFG::widenFixpointPass(const ICFGNode* cycle_head, IntervalExeState& pre_es)
{
    // increasing iterations
    std::cout << "WIDEN PRE ES:\n";
   // pre_es.printExprValues(std::cout);
    std::cout << "WIDEN POST HEAD ES:\n";
  //  _postES[cycle_head].printExprValues(std::cout);
    IntervalExeState new_pre_es = pre_es.widening(_postES[cycle_head]);
    std::cout << "WIDEN NEW PRE ES:\n";
   // new_pre_es.printExprValues(std::cout);
    IntervalExeState new_pre_vaddr_es = new_pre_es;
    _svfir2ExeState->widenAddrs(new_pre_es, _postES[cycle_head]);

    if (pre_es >= new_pre_es)
    {
        // increasing iterations - fixpoint reached
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return true;
    }
    else
    {
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return false;
    }
}

bool AbstractExecutionICFG::narrowFixpointPass(const SVF::ICFGNode *cycle_head, SVF::IntervalExeState &pre_es)
{
    // decreasing iterations
    std::cout << "NARROW PRE ES:\n";
   /// pre_es.printExprValues(std::cout);
    std::cout << "NARROW POST HEAD ES:\n";
   // _postES[cycle_head].printExprValues(std::cout);
    IntervalExeState new_pre_es = pre_es.narrowing(_postES[cycle_head]);
    IntervalExeState new_pre_vaddr_es = new_pre_es;
    _svfir2ExeState->narrowAddrs(new_pre_es, _postES[cycle_head]);
    if (new_pre_es >= pre_es)
    {
        // decreasing iterations - fixpoint reached
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return true;
    }
    else
    {
        pre_es = new_pre_es;
        _postES[cycle_head] = pre_es;
        return false;
    }
}



/// handle user defined function, ext function is not included.
void AbstractExecutionICFG::handleFunc(const SVFFunction *func)
{
    _stat->getFunctionTrace()++;
    ICFGWTO* wto = _funcToICFGWTO[func];
    // set function entry ES
    for (auto it = wto->begin(); it!= wto->end(); ++it)
    {
        const ICFGWTOComp* cur = *it;
        if (const ICFGWTONode* vertex = SVFUtil::dyn_cast<ICFGWTONode>(cur))
        {
            handleICFGNode(vertex->node());
        }
        else if (const ICFGWTOCycle* cycle = SVFUtil::dyn_cast<ICFGWTOCycle>(cur))
        {
            handleICFGCycle(cycle);
        }
        else
        {
            assert(false && "unknown WTO type!");
        }
    }
}


bool AbstractExecutionICFG::isExtCall(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return SVFUtil::isExtCall(callfun);
}

void AbstractExecutionICFG::extCallPass(const SVF::CallICFGNode *callNode)
{
    _callSiteStack.push_back(callNode);
    _api->handleExtAPI(callNode);
    _callSiteStack.pop_back();
}

bool AbstractExecutionICFG::isRecursiveCall(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return _recursiveFuns.find(callfun) != _recursiveFuns.end();
}

void AbstractExecutionICFG::recursiveCallPass(const SVF::CallICFGNode *callNode)
{
    SkipRecursiveCall(callNode);
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() &&
                !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
            {
                _svfir2ExeState->getEs()[retPE->getLHSVarID()] = IntervalValue::top();
            }
        }
    }
    _postES[retNode] = _svfir2ExeState->getEs();
}

bool AbstractExecutionICFG::isDirectCall(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    return _funcToICFGWTO.find(callfun) != _funcToICFGWTO.end();
}
void AbstractExecutionICFG::directCallFunPass(const SVF::CallICFGNode *callNode)
{
    const SVFFunction *callfun = SVFUtil::getCallee(callNode->getCallSite());
    IntervalExeState preES = _svfir2ExeState->getEs();
    _callSiteStack.push_back(callNode);

    _postES[callNode] = _svfir2ExeState->getEs();

    handleFunc(callfun);
    _callSiteStack.pop_back();
    // handle Ret node
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    // resume ES to callnode
    _postES[retNode] = _postES[callNode];
}

bool AbstractExecutionICFG::isIndirectCall(const SVF::CallICFGNode *callNode)
{
    const auto callsiteMaps = _svfir->getIndirectCallsites();
    return callsiteMaps.find(callNode) != callsiteMaps.end();
}

void AbstractExecutionICFG::indirectCallFunPass(const SVF::CallICFGNode *callNode)
{
    const auto callsiteMaps = _svfir->getIndirectCallsites();
    NodeID call_id = callsiteMaps.at(callNode);
    if (!_svfir2ExeState->getEs().inVarToAddrsTable(call_id))
    {
        return;
    }
    ExeState::Addrs Addrs = _svfir2ExeState->getAddrs(call_id);
    NodeID addr = *Addrs.begin();
    SVFVar *func_var = _svfir->getGNode(_svfir2ExeState->getInternalID(addr));
    const SVFFunction *callfun = SVFUtil::dyn_cast<SVFFunction>(func_var->getValue());
    if (callfun)
    {
        IntervalExeState preES = _svfir2ExeState->getEs();
        _callSiteStack.push_back(callNode);

        _postES[callNode] = _svfir2ExeState->getEs();

        handleFunc(callfun);
        _callSiteStack.pop_back();
        // handle Ret node
        const RetICFGNode *retNode = callNode->getRetICFGNode();
        _postES[retNode] = _postES[callNode];
    }
}


std::string IntervalToIntStr(const IntervalValue& inv)
{
    if (inv.is_infinite())
    {
        return inv.toString();
    }
    else
    {
        int64_t lb_val = inv.lb().getNumeral();
        int64_t ub_val = inv.ub().getNumeral();
        
        // check lb
        s32_t lb_s32 = (lb_val < static_cast<int64_t>(INT_MIN)) ? INT_MIN :
                       (lb_val > static_cast<int64_t>(INT_MAX)) ? INT_MAX :
                                                                static_cast<s32_t>(lb_val);

        // check ub
        s32_t ub_s32 = (ub_val < static_cast<int64_t>(INT_MIN)) ? INT_MIN :
                       (ub_val > static_cast<int64_t>(INT_MAX)) ? INT_MAX :
                                                                static_cast<s32_t>(ub_val);

        return "[" + std::to_string(lb_s32) + ", " + std::to_string(ub_s32) + "]";
    }
}

void BufOverflowCheckerICFG::handleSVFStatement(const SVFStmt *stmt)
{
    AbstractExecutionICFG::handleSVFStatement(stmt);
    // for gep stmt, add the gep stmt to the addrToGep map
    if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        for (NodeID addrID: _svfir2ExeState->getAddrs(gep->getLHSVarID()))
        {
            NodeID objId = _svfir2ExeState->getInternalID(addrID);
            if (auto* extapi = SVFUtil::dyn_cast<BufOverflowCheckerAPI>(_api))
                extapi->_addrToGep[objId] = gep;
        }
    }
}

void BufOverflowCheckerICFG::handleICFGNode(const SVF::ICFGNode *node)
{
    AbstractExecutionICFG::handleICFGNode(node);
    detectBufOverflow(node);
}

//
bool BufOverflowCheckerICFG::detectBufOverflow(const ICFGNode *node)
{

    auto *extapi = SVFUtil::dyn_cast<BufOverflowCheckerAPI>(_api);
    for (auto* stmt: node->getSVFStmts())
    {
        if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            const SVFVar* gepRhs = gep->getRHSVar();
            if (const SVFInstruction* inst = SVFUtil::dyn_cast<SVFInstruction>(gepRhs->getValue()))
            {
                const ICFGNode* icfgNode = _svfir->getICFG()->getICFGNode(inst);
                for (const SVFStmt* stmt2: icfgNode->getSVFStmts())
                {
                    if (const GepStmt *gep2 = SVFUtil::dyn_cast<GepStmt>(stmt2))
                    {
                        return extapi->canSafelyAccessMemory(gep2->getLHSVar()->getValue(), IntervalValue(0, 0), node);
                    }
                }
            }
        }
        else if (const LoadStmt* load =  SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            if (_svfir2ExeState->inVarToAddrsTable(load->getRHSVarID()))
            {
                ExeState::Addrs Addrs = _svfir2ExeState->getAddrs(load->getRHSVarID());
                for (auto vaddr: Addrs)
                {
                    u32_t objId = _svfir2ExeState->getInternalID(vaddr);
                    if (extapi->_addrToGep.find(objId) != extapi->_addrToGep.end())
                    {
                        const GepStmt* gep = extapi->_addrToGep.at(objId);
                        return extapi->canSafelyAccessMemory(gep->getLHSVar()->getValue(), IntervalValue(0, 0), node);
                    }
                }
            }
        }
        else if (const StoreStmt* store =  SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            if (_svfir2ExeState->inVarToAddrsTable(store->getLHSVarID()))
            {
                ExeState::Addrs Addrs = _svfir2ExeState->getAddrs(store->getLHSVarID());
                for (auto vaddr: Addrs)
                {
                    u32_t objId = _svfir2ExeState->getInternalID(vaddr);
                    if (extapi->_addrToGep.find(objId) != extapi->_addrToGep.end())
                    {
                        const GepStmt* gep = extapi->_addrToGep.at(objId);
                        return extapi->canSafelyAccessMemory(gep->getLHSVar()->getValue(), IntervalValue(0, 0), node);
                    }
                }
            }
        }
    }
    return true;
}

void BufOverflowCheckerICFG::addBugToRecoder(const BufOverflowException& e, const ICFGNode* node)
{
    const SVFInstruction* inst = nullptr;
    if (const CallICFGNode* call = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        inst = call->getCallSite();
    }
    else
    {
        inst = node->getSVFStmts().back()->getInst();
    }
    GenericBug::EventStack eventStack;
    SVFBugEvent sourceInstEvent(SVFBugEvent::EventType::SourceInst, inst);
    for (const auto &callsite: _callSiteStack)
    {
        SVFBugEvent callSiteEvent(SVFBugEvent::EventType::CallSite, callsite->getCallSite());
        eventStack.push_back(callSiteEvent);
    }
    eventStack.push_back(sourceInstEvent);
    if (eventStack.size() == 0) return;
    std::string loc = eventStack.back().getEventLoc();
    if (_bugLoc.find(loc) != _bugLoc.end())
    {
        return;
    }
    else
    {
        _bugLoc.insert(loc);
    }
    _recoder.addAbsExecBug(GenericBug::FULLBUFOVERFLOW, eventStack, e.getAllocLb(), e.getAllocUb(), e.getAccessLb(),
                           e.getAccessUb());
    _nodeToBugInfo[node] = e.what();
}

void BufOverflowCheckerICFGAPI::initExtAPIBufOverflowCheckRules()
{
    //void llvm_memcpy_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i64"] = {{0, 2}, {1,2}};
    //void llvm_memcpy_p0_p0_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0_p0_i64"] = {{0, 2}, {1,2}};
    //void llvm_memcpy_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy_p0i8_p0i8_i32"] = {{0, 2}, {1,2}};
    //void llvm_memcpy(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memcpy"] = {{0, 2}, {1,2}};
    //void llvm_memmove(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0i8_p0i8_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i64"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0_p0_i64(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0_p0_i64"] = {{0, 2}, {1,2}};
    //void llvm_memmove_p0i8_p0i8_i32(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memmove_p0i8_p0i8_i32"] = {{0, 2}, {1,2}};
    //void __memcpy_chk(char* dst, char* src, int sz, int flag){}
    _extAPIBufOverflowCheckRules["__memcpy_chk"] = {{0, 2}, {1,2}};
    //void *memmove(void *str1, const void *str2, unsigned long n)
    _extAPIBufOverflowCheckRules["memmove"] = {{0, 2}, {1,2}};
    //void bcopy(const void *s1, void *s2, unsigned long n){}
    _extAPIBufOverflowCheckRules["bcopy"] = {{0, 2}, {1,2}};
    //void *memccpy( void * restrict dest, const void * restrict src, int c, unsigned long count)
    _extAPIBufOverflowCheckRules["memccpy"] = {{0, 3}, {1,3}};
    //void __memmove_chk(char* dst, char* src, int sz){}
    _extAPIBufOverflowCheckRules["__memmove_chk"] = {{0, 2}, {1,2}};
    //void llvm_memset(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset"] = {{0, 2}};
    //void llvm_memset_p0i8_i32(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0i8_i32"] = {{0, 2}};
    //void llvm_memset_p0i8_i64(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0i8_i64"] = {{0, 2}};
    //void llvm_memset_p0_i64(char* dst, char elem, int sz, int flag){}
    _extAPIBufOverflowCheckRules["llvm_memset_p0_i64"] = {{0, 2}};
    //char *__memset_chk(char * dest, int c, unsigned long destlen, int flag)
    _extAPIBufOverflowCheckRules["__memset_chk"] = {{0, 2}};
    //char *wmemset(wchar_t * dst, wchar_t elem, int sz, int flag) {
    _extAPIBufOverflowCheckRules["wmemset"] = {{0, 2}};
    //char *strncpy(char *dest, const char *src, unsigned long n)
    _extAPIBufOverflowCheckRules["strncpy"] = {{0, 2}, {1,2}};
    //unsigned long iconv(void* cd, char **restrict inbuf, unsigned long *restrict inbytesleft, char **restrict outbuf, unsigned long *restrict outbytesleft)
    _extAPIBufOverflowCheckRules["iconv"] = {{1, 2}, {3, 4}};
}


bool BufOverflowCheckerICFGAPI::detectStrcpy(const CallICFGNode *call)
{
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    const SVFValue* arg0Val = cs.getArgument(0);
    const SVFValue* arg1Val = cs.getArgument(1);
    IntervalValue strLen = getStrlen(arg1Val);
    // no need to -1, since it has \0 as the last byte
    return canSafelyAccessMemory(arg0Val, strLen, call);
}

void BufOverflowCheckerICFGAPI::initExtFunMap()
{

    auto sse_scanf = [&](const CallSite &cs)
    {
        //scanf("%d", &data);
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 2) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t dst_id = _svfir->getValueNode(cs.getArgument(1));
        if (!ae->_svfir2ExeState->inVarToAddrsTable(dst_id))
        {
            BufOverflowException bug("scanf may cause buffer overflow.\n", 0, 0, 0, 0, cs.getArgument(1));
            ae->addBugToRecoder(bug, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
            return;
        }
        else
        {
            Addrs Addrs = ae->_svfir2ExeState->getAddrs(dst_id);
            for (auto vaddr: Addrs)
            {
                u32_t objId = ae->_svfir2ExeState->getInternalID(vaddr);
                IntervalValue range = ae->_svfir2ExeState->getRangeLimitFromType(_svfir->getGNode(objId)->getType());
                es.store(vaddr, range);
            }
        }
    };
    auto sse_fscanf = [&](const CallSite &cs)
    {
        //fscanf(stdin, "%d", &data);
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 3) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t dst_id = _svfir->getValueNode(cs.getArgument(2));
        if (!ae->_svfir2ExeState->inVarToAddrsTable(dst_id))
        {
            BufOverflowException bug("scanf may cause buffer overflow.\n", 0, 0, 0, 0, cs.getArgument(2));
            ae->addBugToRecoder(bug, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
            return;
        }
        else
        {
            Addrs Addrs = ae->_svfir2ExeState->getAddrs(dst_id);
            for (auto vaddr: Addrs)
            {
                u32_t objId = ae->_svfir2ExeState->getInternalID(vaddr);
                IntervalValue range = ae->_svfir2ExeState->getRangeLimitFromType(_svfir->getGNode(objId)->getType());
                es.store(vaddr, range);
            }
        }
    };

    _func_map["__isoc99_fscanf"] = sse_fscanf;
    _func_map["__isoc99_scanf"] = sse_scanf;
    _func_map["__isoc99_vscanf"] = sse_scanf;
    _func_map["fscanf"] = sse_fscanf;
    _func_map["scanf"] = sse_scanf;
    _func_map["sscanf"] = sse_scanf;
    _func_map["__isoc99_sscanf"] = sse_scanf;
    _func_map["vscanf"] = sse_scanf;

    auto sse_fread = [&](const CallSite &cs)
    {
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 3) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t block_count_id = _svfir->getValueNode(cs.getArgument(2));
        u32_t block_size_id = _svfir->getValueNode(cs.getArgument(1));
        IntervalValue block_count = es[block_count_id];
        IntervalValue block_size = es[block_size_id];
        IntervalValue block_byte = block_count * block_size;
        canSafelyAccessMemory(cs.getArgument(0), block_byte, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
    };
    _func_map["fread"] = sse_fread;

    auto sse_sprintf = [&](const CallSite &cs)
    {
        // printf is difficult to predict since it has no byte size arguments
    };

    auto sse_snprintf = [&](const CallSite &cs)
    {
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 2) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t size_id = _svfir->getValueNode(cs.getArgument(1));
        u32_t dst_id = _svfir->getValueNode(cs.getArgument(0));
        // get elem size of arg2
        u32_t elemSize = 1;
        if (cs.getArgument(2)->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(cs.getArgument(2)->getType())->getTypeOfElement()->getByteSize();
        }
        else if (cs.getArgument(2)->getType()->isPointerTy())
        {
            elemSize = getPointeeElement(_svfir->getValueNode(cs.getArgument(2)))->getByteSize();
        }
        else
        {
            return;
            // assert(false && "we cannot support this type");
        }
        IntervalValue size = es[size_id] * IntervalValue(elemSize) - IntervalValue(1);
        if (!es.inVarToAddrsTable(dst_id))
        {
            if (Options::BufferOverflowCheck())
            {
                BufOverflowException bug(
                    "snprintf dst_id or dst is not defined nor initializesd.\n",
                    0, 0, 0, 0, cs.getArgument(0));
                ae->addBugToRecoder(bug, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
                return;
            }
        }
        canSafelyAccessMemory(cs.getArgument(0), size, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
    };
    _func_map["__snprintf_chk"] = sse_snprintf;
    _func_map["__vsprintf_chk"] = sse_sprintf;
    _func_map["__sprintf_chk"] = sse_sprintf;
    _func_map["snprintf"] = sse_snprintf;
    _func_map["sprintf"] = sse_sprintf;
    _func_map["vsprintf"] = sse_sprintf;
    _func_map["vsnprintf"] = sse_snprintf;
    _func_map["__vsnprintf_chk"] = sse_snprintf;
    _func_map["swprintf"] = sse_snprintf;
    _func_map["_snwprintf"] = sse_snprintf;


    auto sse_itoa = [&](const CallSite &cs)
    {
        // itoa(num, ch, 10);
        // num: int, ch: char*, 10 is decimal
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 3) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t num_id = _svfir->getValueNode(cs.getArgument(0));

        u32_t num = (u32_t) es[num_id].getNumeral();
        std::string snum = std::to_string(num);
        canSafelyAccessMemory(cs.getArgument(1), IntervalValue((s32_t)snum.size()), _svfir->getICFG()->getICFGNode(cs.getInstruction()));
    };
    _func_map["itoa"] = sse_itoa;


    auto sse_strlen = [&](const CallSite &cs)
    {
        // check the arg size
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 1) return;
        const SVFValue* strValue = cs.getArgument(0);
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        IntervalValue dst_size = getStrlen(strValue);
        u32_t elemSize = 1;
        if (strValue->getType()->isArrayTy())
        {
            elemSize = SVFUtil::dyn_cast<SVFArrayType>(strValue->getType())->getTypeOfElement()->getByteSize();
        }
        else if (strValue->getType()->isPointerTy())
        {
            elemSize = getPointeeElement(_svfir->getValueNode(strValue))->getByteSize();
        }
        u32_t lhsId = _svfir->getValueNode(cs.getInstruction());
        es[lhsId] = dst_size / IntervalValue(elemSize);
    };
    _func_map["strlen"] = sse_strlen;
    _func_map["wcslen"] = sse_strlen;

    auto sse_recv = [&](const CallSite &cs)
    {
        // recv(sockfd, buf, len, flags);
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 4) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t len_id = _svfir->getValueNode(cs.getArgument(2));
        IntervalValue len = es[len_id] - IntervalValue(1);
        u32_t lhsId = _svfir->getValueNode(cs.getInstruction());
        es[lhsId] = len;
        canSafelyAccessMemory(cs.getArgument(1), len, _svfir->getICFG()->getICFGNode(cs.getInstruction()));;
    };
    _func_map["recv"] = sse_recv;
    _func_map["__recv"] = sse_recv;
    auto safe_bufaccess = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        _checkpoints.erase(callNode);
        //void SAFE_BUFACCESS(void* data, int size);
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 2) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t size_id = _svfir->getValueNode(cs.getArgument(1));
        IntervalValue val = es[size_id];
        if (val.isBottom())
        {
            val = IntervalValue(0);
            assert(false && "SAFE_BUFACCESS size is bottom");
        }
        bool isSafe = canSafelyAccessMemory(cs.getArgument(0), val, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
        if (isSafe)
        {
            std::cout << "safe buffer access success\n";
            return;
        }
        else
        {
            std::string err_msg = "this SAFE_BUFACCESS should be a safe access but detected buffer overflow. Pos: ";
            err_msg += cs.getInstruction()->getSourceLoc();
            std::cerr << err_msg << std::endl;
            assert(false);
        }
    };
    _func_map["SAFE_BUFACCESS"] = safe_bufaccess;

    auto unsafe_bufaccess = [&](const CallSite &cs)
    {
        const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(_svfir->getICFG()->getICFGNode(cs.getInstruction()));
        _checkpoints.erase(callNode);
        //void UNSAFE_BUFACCESS(void* data, int size);
        BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
        if (cs.arg_size() < 2) return;
        IntervalExeState &es = ae->_svfir2ExeState->getEs();
        u32_t size_id = _svfir->getValueNode(cs.getArgument(1));
        IntervalValue val = es[size_id];
        if (val.isBottom())
        {
            assert(false && "UNSAFE_BUFACCESS size is bottom");
        }
        bool isSafe = canSafelyAccessMemory(cs.getArgument(0), val, _svfir->getICFG()->getICFGNode(cs.getInstruction()));
        if (!isSafe)
        {
            std::cout << "detect buffer overflow success\n";
            return;
        }
        else
        {
            // if it is safe, it means it is wrongly labeled, assert false.
            std::string err_msg = "this UNSAFE_BUFACCESS should be a buffer overflow but not detected. Pos: ";
            err_msg += cs.getInstruction()->getSourceLoc();
            std::cerr << err_msg << std::endl;
            assert(false);
        }
    };
    _func_map["UNSAFE_BUFACCESS"] = unsafe_bufaccess;

    // init _checkpoint_names
    _checkpoint_names.insert("SAFE_BUFACCESS");
    _checkpoint_names.insert("UNSAFE_BUFACCESS");
}

bool BufOverflowCheckerICFGAPI::detectStrcat(const CallICFGNode *call)
{
    BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    // check the arg size
    // if it is strcat group, we need to check the length of string,
    // e.g. strcat(str1, str2); which checks AllocSize(str1) >= Strlen(str1) + Strlen(str2);
    // if it is strncat group, we do not need to check the length of string,
    // e.g. strncat(str1, str2, n); which checks AllocSize(str1) >= Strlen(str1) + n;

    const std::vector<std::string> strcatGroup = {"__strcat_chk", "strcat", "__wcscat_chk", "wcscat"};
    const std::vector<std::string> strncatGroup = {"__strncat_chk", "strncat", "__wcsncat_chk", "wcsncat"};
    if (std::find(strcatGroup.begin(), strcatGroup.end(), fun->getName()) != strcatGroup.end())
    {
        CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg1Val = cs.getArgument(1);
        IntervalValue strLen0 = getStrlen(arg0Val);
        IntervalValue strLen1 = getStrlen(arg1Val);
        IntervalValue totalLen = strLen0 + strLen1;
        return canSafelyAccessMemory(arg0Val, totalLen, call);
    }
    else if (std::find(strncatGroup.begin(), strncatGroup.end(), fun->getName()) != strncatGroup.end())
    {
        CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
        const SVFValue* arg0Val = cs.getArgument(0);
        const SVFValue* arg2Val = cs.getArgument(2);
        IntervalValue arg2Num = ae->_svfir2ExeState->getEs()[_svfir->getValueNode(arg2Val)];
        IntervalValue strLen0 = getStrlen(arg0Val);
        IntervalValue totalLen = strLen0 + arg2Num;
        return canSafelyAccessMemory(arg0Val, totalLen, call);
    }
    else
    {
        assert(false && "unknown strcat function, please add it to strcatGroup or strncatGroup");
    }
}

void BufOverflowCheckerICFGAPI::handleExtAPI(const CallICFGNode *call)
{
    AEAPI::handleExtAPI(call);
    BufOverflowCheckerICFG* ae = SVFUtil::dyn_cast<BufOverflowCheckerICFG>(_ae);
    const SVFFunction *fun = SVFUtil::getCallee(call->getCallSite());
    assert(fun && "SVFFunction* is nullptr");
    CallSite cs = SVFUtil::getSVFCallSite(call->getCallSite());
    // check the type of mem api,
    // MEMCPY: like memcpy, memcpy_chk, llvm.memcpy etc.
    // MEMSET: like memset, memset_chk, llvm.memset etc.
    // STRCPY: like strcpy, strcpy_chk, wcscpy etc.
    // STRCAT: like strcat, strcat_chk, wcscat etc.
    // for other ext api like printf, scanf, etc., they have their own handlers
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
    // 1. memcpy functions like memcpy_chk, strncpy, annotate("MEMCPY"), annotate("BUF_CHECK:Arg0, Arg2"), annotate("BUF_CHECK:Arg1, Arg2")
    if (extType == MEMCPY)
    {
        if (_extAPIBufOverflowCheckRules.count(fun->getName()) == 0)
        {
            // if it is not in the rules, we do not check it
            SVFUtil::errs() << "Warning: " << fun->getName() << " is not in the rules, please implement it\n";
            return;
        }
        // call parseMemcpyBufferCheckArgs to parse the BUF_CHECK annotation
        std::vector<std::pair<u32_t, u32_t>> args = _extAPIBufOverflowCheckRules.at(fun->getName());
        // loop the args and check the offset
        for (auto arg: args)
        {
            IntervalValue offset = ae->_svfir2ExeState->getEs()[_svfir->getValueNode(cs.getArgument(arg.second))] - IntervalValue(1);
            canSafelyAccessMemory(cs.getArgument(arg.first), offset, call);
        }
    }
    // 2. memset functions like memset, memset_chk, annotate("MEMSET"), annotate("BUF_CHECK:Arg0, Arg2")
    else if (extType == MEMSET)
    {
        if (_extAPIBufOverflowCheckRules.count(fun->getName()) == 0)
        {
            // if it is not in the rules, we do not check it
            SVFUtil::errs() << "Warning: " << fun->getName() << " is not in the rules, please implement it\n";
            return;
        }
        std::vector<std::pair<u32_t, u32_t>> args = _extAPIBufOverflowCheckRules.at(fun->getName());
        // loop the args and check the offset
        for (auto arg: args)
        {
            IntervalValue offset = ae->_svfir2ExeState->getEs()[_svfir->getValueNode(cs.getArgument(arg.second))] - IntervalValue(1);
            canSafelyAccessMemory(cs.getArgument(arg.first), offset, call);
        }
    }
    else if (extType == STRCPY)
    {
        detectStrcpy(call);
    }
    else if (extType == STRCAT)
    {
        detectStrcat(call);
    }
    else
    {

    }
    return;
}

bool BufOverflowCheckerICFGAPI::canSafelyAccessMemory(const SVFValue *value, const IntervalValue &len, const ICFGNode *curNode)
{
    BufOverflowCheckerICFG* ae = static_cast<BufOverflowCheckerICFG*>(this->_ae);
    const SVFValue *firstValue = value;
    /// Usually called by a GepStmt overflow check, or external API (like memcpy) overflow check
    /// Defitions of Terms:
    /// source node: malloc or gepStmt(array), sink node: gepStmt or external API (like memcpy)
    /// e.g. 1) a = malloc(10), a[11] = 10, a[11] is the sink node, a is the source node (malloc)
    ///  2) A = struct {int a[10];}, A.a[11] = 10, A.a[11] is the sink, A.a is the source node (gepStmt(array))

    /// it tracks the value flow from sink to source, and accumulates offset
    /// then compare the accumulated offset and malloc size (or gepStmt array size)
    SVF::FILOWorkList<const SVFValue *> worklist;
    Set<const SVFValue *> visited;
    visited.insert(value);
    Map<const ICFGNode *, IntervalValue> gep_offsets;
    IntervalValue total_bytes = len;
    worklist.push(value);
    std::vector<const CallICFGNode *> callstack = ae->_callSiteStack;
    while (!worklist.empty())
    {
        value = worklist.pop();
        if (const SVFInstruction *ins = SVFUtil::dyn_cast<SVFInstruction>(value))
        {
            const ICFGNode *node = _svfir->getICFG()->getICFGNode(ins);
            if (const CallICFGNode *callnode = SVFUtil::dyn_cast<CallICFGNode>(node))
            {
                AccessMemoryViaRetNode(callnode, worklist, visited);
            }
            for (const SVFStmt *stmt: node->getSVFStmts())
            {
                if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
                {
                    AccessMemoryViaCopyStmt(copy, worklist, visited);
                }
                else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt))
                {
                    AccessMemoryViaLoadStmt(load, worklist, visited);
                }
                else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
                {
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
                    //   so if curgepOffset + totalOffset >= gepSrc (overflow)
                    //      else totalOffset += curgepOffset

                    // otherwise, if last getOffsetVar.Type is the Array, check the last idx and array. (just offset,
                    //  not with totalOffset during check)
                    //  so if getOffsetVarVal > getOffsetVar.TypeSize (overflow)
                    //     else safe and return.
                    IntervalValue byteOffset;
                    if (gep->isConstantOffset())
                    {
                        byteOffset = IntervalValue(gep->accumulateConstantByteOffset());
                    }
                    else
                    {
                        byteOffset = ae->_svfir2ExeState->getByteOffset(gep);
                    }
                    // for variable offset, join with accumulate gep offset
                    gep_offsets[gep->getICFGNode()] = byteOffset;
                    if (byteOffset.ub().getNumeral() >= Options::MaxFieldLimit() && Options::GepUnknownIdx())
                    {
                        return true;
                    }

                    if (gep->getOffsetVarAndGepTypePairVec().size() > 0)
                    {
                        const SVFVar *gepVal = gep->getOffsetVarAndGepTypePairVec().back().first;
                        const SVFType *gepType = gep->getOffsetVarAndGepTypePairVec().back().second;

                        if (gepType->isArrayTy())
                        {
                            const SVFArrayType *gepArrType = SVFUtil::dyn_cast<SVFArrayType>(gepType);
                            IntervalValue gepArrTotalByte(0);
                            const SVFValue *idxValue = gepVal->getValue();
                            u32_t arrElemSize = gepArrType->getTypeOfElement()->getByteSize();
                            if (const SVFConstantInt *op = SVFUtil::dyn_cast<SVFConstantInt>(idxValue))
                            {
                                u32_t lb = (double) Options::MaxFieldLimit() / arrElemSize >= op->getSExtValue() ?
                                                                                                                op->getSExtValue() * arrElemSize : Options::MaxFieldLimit();
                                gepArrTotalByte = gepArrTotalByte + IntervalValue(lb, lb);
                            }
                            else
                            {
                                u32_t idx = _svfir->getValueNode(idxValue);
                                IntervalValue idxVal = ae->_svfir2ExeState->getEs()[idx];
                                if (idxVal.isBottom())
                                {
                                    gepArrTotalByte = gepArrTotalByte + IntervalValue(0, 0);
                                }
                                else
                                {
                                    u32_t ub = (idxVal.ub().getNumeral() < 0) ? 0 :
                                               (double) Options::MaxFieldLimit() / arrElemSize >=
                                                       idxVal.ub().getNumeral() ?
                                                   arrElemSize * idxVal.ub().getNumeral() : Options::MaxFieldLimit();
                                    u32_t lb = (idxVal.lb().getNumeral() < 0) ? 0 :
                                               ((double) Options::MaxFieldLimit() / arrElemSize >=
                                                idxVal.lb().getNumeral()) ?
                                                   arrElemSize * idxVal.lb().getNumeral() : Options::MaxFieldLimit();
                                    gepArrTotalByte = gepArrTotalByte + IntervalValue(lb, ub);
                                }
                            }
                            total_bytes = total_bytes + gepArrTotalByte;
                            if (total_bytes.ub().getNumeral() >= gepArrType->getByteSize())
                            {
                                std::string msg =
                                    "Buffer overflow!! Accessing buffer range: " +
                                    IntervalToIntStr(total_bytes) +
                                    "\nAllocated Gep buffer size: " +
                                    std::to_string(gepArrType->getByteSize()) + "\n";
                                msg += "Position: " + firstValue->toString() + "\n";
                                msg += " The following is the value flow. [[\n";
                                for (auto it = gep_offsets.begin(); it != gep_offsets.end(); ++it)
                                {
                                    msg += it->first->toString() + ", Offset: " + IntervalToIntStr(it->second) +
                                           "\n";
                                }
                                msg += "]].\nAlloc Site: " + gep->toString() + "\n";

                                BufOverflowException bug(SVFUtil::errMsg(msg), gepArrType->getByteSize(),
                                                         gepArrType->getByteSize(),
                                                         total_bytes.lb().getNumeral(), total_bytes.ub().getNumeral(),
                                                         firstValue);
                                ae->addBugToRecoder(bug, curNode);
                                return false;
                            }
                            else
                            {
                                // for gep last index's type is arr, stop here.
                                return true;
                            }
                        }
                        else
                        {
                            total_bytes = total_bytes + byteOffset;
                        }

                    }
                    if (!visited.count(gep->getRHSVar()->getValue()))
                    {
                        visited.insert(gep->getRHSVar()->getValue());
                        worklist.push(gep->getRHSVar()->getValue());
                    }
                }
                else if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
                {
                    // addrStmt is source node.
                    u32_t arr_type_size = getAllocaInstByteSize(addr);
                    if (total_bytes.ub().getNumeral() >= arr_type_size ||
                        total_bytes.lb().getNumeral() < 0)
                    {
                        std::string msg =
                            "Buffer overflow!! Accessing buffer range: " + IntervalToIntStr(total_bytes) +
                            "\nAllocated buffer size: " + std::to_string(arr_type_size) + "\n";
                        msg += "Position: " + firstValue->toString() + "\n";
                        msg += " The following is the value flow. [[\n";
                        for (auto it = gep_offsets.begin(); it != gep_offsets.end(); ++it)
                        {
                            msg += it->first->toString() + ", Offset: " + IntervalToIntStr(it->second) + "\n";
                        }
                        msg += "]].\n Alloc Site: " + addr->toString() + "\n";
                        BufOverflowException bug(SVFUtil::wrnMsg(msg), arr_type_size, arr_type_size,
                                                 total_bytes.lb().getNumeral(), total_bytes.ub().getNumeral(),
                                                 firstValue);
                        ae->addBugToRecoder(bug, curNode);
                        return false;
                    }
                    else
                    {

                        return true;
                    }
                }
            }
        }
        else if (const SVF::SVFGlobalValue *gvalue = SVFUtil::dyn_cast<SVF::SVFGlobalValue>(value))
        {
            u32_t arr_type_size = 0;
            const SVFType *svftype = gvalue->getType();
            if (SVFUtil::isa<SVFPointerType>(svftype))
            {
                if (const SVFArrayType *ptrArrType = SVFUtil::dyn_cast<SVFArrayType>(
                        getPointeeElement(_svfir->getValueNode(gvalue))))
                    arr_type_size = ptrArrType->getByteSize();
                else
                    arr_type_size = svftype->getByteSize();
            }
            else
                arr_type_size = svftype->getByteSize();

            if (total_bytes.ub().getNumeral() >= arr_type_size || total_bytes.lb().getNumeral() < 0)
            {
                std::string msg = "Buffer overflow!! Accessing buffer range: " + IntervalToIntStr(total_bytes) +
                                  "\nAllocated buffer size: " + std::to_string(arr_type_size) + "\n";
                msg += "Position: " + firstValue->toString() + "\n";
                msg += " The following is the value flow.\n[[";
                for (auto it = gep_offsets.begin(); it != gep_offsets.end(); ++it)
                {
                    msg += it->first->toString() + ", Offset: " + IntervalToIntStr(it->second) + "\n";
                }
                msg += "]]. \nAlloc Site: " + gvalue->toString() + "\n";

                BufOverflowException bug(SVFUtil::wrnMsg(msg), arr_type_size, arr_type_size,
                                         total_bytes.lb().getNumeral(), total_bytes.ub().getNumeral(), firstValue);
                ae->addBugToRecoder(bug, curNode);
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (const SVF::SVFArgument *arg = SVFUtil::dyn_cast<SVF::SVFArgument>(value))
        {
            AccessMemoryViaCallArgs(arg, worklist, visited);
        }
        else
        {
            // maybe SVFConstant
            // it may be cannot find the source, maybe we start from non-main function,
            // therefore it loses the value flow track
            return true;
        }
    }
    // it may be cannot find the source, maybe we start from non-main function,
    // therefore it loses the value flow track
    return true;
}


