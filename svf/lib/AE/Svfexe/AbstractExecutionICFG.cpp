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
        _funcToWTO[fun] = wto;
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
    delete _stat;
    delete _api;
    delete _svfir2ExeState;
    for (auto it: _funcToWTO)
        delete it.second;

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
    if (curICFGNode->getId() == 8) {
       std::cout << "Node 8, Var26: " << _svfir2ExeState->getEs()[26].toString() << std::endl;
    }
    std::cout << "Now ES Trace ID: " << _stat->getBlockTrace() << std::endl;
    std::cout << curICFGNode->toString() << std::endl;
    _svfir2ExeState->getEs().printExprValues(std::cout);

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
    _svfir2ExeState->getEs().printExprValues(std::cout);
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
        pre_es.printExprValues(std::cout);
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
                        incresing = false;
                }
                if (!incresing)
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
    for (const SVFInstruction* inst: cycle->head()->getBB()->getInstructionList()) {
        const ICFGNode* node = _icfg->getICFGNode(inst);
        if (node == cycle->head())
            continue;
        else
            handleICFGNode(node);
    }
}

bool AbstractExecutionICFG::widenFixpointPass(const ICFGNode* cycle_head, IntervalExeState& pre_es)
{
    // increasing iterations
    std::cout << "WIDEN PRE ES:\n";
    pre_es.printExprValues(std::cout);
    std::cout << "WIDEN POST HEAD ES:\n";
    _postES[cycle_head].printExprValues(std::cout);
    IntervalExeState new_pre_es = pre_es.widening(_postES[cycle_head]);
    std::cout << "WIDEN NEW PRE ES:\n";
    new_pre_es.printExprValues(std::cout);
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
    pre_es.printExprValues(std::cout);
    std::cout << "NARROW POST HEAD ES:\n";
    _postES[cycle_head].printExprValues(std::cout);
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
    ICFGWTO* wto = _funcToWTO[func];
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