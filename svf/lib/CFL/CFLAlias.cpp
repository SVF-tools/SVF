//===----- CFLAlias.cpp -- CFL Alias Analysis Client--------------//
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

/*
 * CFLAlias.cpp
 *
 *  Created on: June 27 , 2022
 *      Author: Pei Xu
 */

#include "CFL/CFLAlias.h"
#include "Util/CppUtil.h"
using namespace SVF;
using namespace cppUtil;
using namespace SVFUtil;

/*!
 * On the fly call graph construction
 * callsites is candidate indirect callsites need to be analyzed based on points-to results
 * newEdges is the new indirect call edges discovered
 */
void CFLAlias::onTheFlyCallGraphSolve(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges)
{
    for(CallSiteToFunPtrMap::const_iterator iter = callsites.begin(), eiter = callsites.end(); iter!=eiter; ++iter)
    {
        const CallICFGNode* cs = iter->first;

        if (SVFUtil::getSVFCallSite(cs->getCallSite()).isVirtualCall())
        {
            const SVFValue* vtbl = SVFUtil::getSVFCallSite(cs->getCallSite()).getVtablePtr();
            assert(pag->hasValueNode(vtbl));
            NodeID vtblId = pag->getValueNode(vtbl);
            resolveCPPIndCalls(cs, getCFLPts(vtblId), newEdges);
        }
        else
            resolveIndCalls(iter->first,getCFLPts(iter->second),newEdges);
    }
}

/*!
 * Connect formal and actual parameters for indirect callsites
 */

void CFLAlias::connectCaller2CalleeParams(CallSite cs, const SVFFunction* F)
{
    assert(F);

    DBOUT(DAndersen, outs() << "connect parameters from indirect callsite " << cs.getInstruction()->toString() << " to callee " << *F << "\n");

    CallICFGNode* callBlockNode = svfir->getICFG()->getCallICFGNode(cs.getInstruction());
    RetICFGNode* retBlockNode = svfir->getICFG()->getRetICFGNode(cs.getInstruction());

    if(SVFUtil::isHeapAllocExtFunViaRet(F) && svfir->callsiteHasRet(retBlockNode))
    {
        heapAllocatorViaIndCall(cs);
    }

    if (svfir->funHasRet(F) && svfir->callsiteHasRet(retBlockNode))
    {
        const PAGNode* cs_return = svfir->getCallSiteRet(retBlockNode);
        const PAGNode* fun_return = svfir->getFunRet(F);
        if (cs_return->isPointer() && fun_return->isPointer())
        {
            NodeID dstrec = cs_return->getId();
            NodeID srcret = fun_return->getId();
            addCopyEdge(srcret, dstrec);
        }
        else
        {
            DBOUT(DAndersen, outs() << "not a pointer ignored\n");
        }
    }

    if (svfir->hasCallSiteArgsMap(callBlockNode) && svfir->hasFunArgsList(F))
    {

        // connect actual and formal param
        const SVFIR::SVFVarList& csArgList = svfir->getCallSiteArgsList(callBlockNode);
        const SVFIR::SVFVarList& funArgList = svfir->getFunArgsList(F);
        //Go through the fixed parameters.
        DBOUT(DPAGBuild, outs() << "      args:");
        SVFIR::SVFVarList::const_iterator funArgIt = funArgList.begin(), funArgEit = funArgList.end();
        SVFIR::SVFVarList::const_iterator csArgIt  = csArgList.begin(), csArgEit = csArgList.end();
        for (; funArgIt != funArgEit; ++csArgIt, ++funArgIt)
        {
            //Some programs (e.g. Linux kernel) leave unneeded parameters empty.
            if (csArgIt  == csArgEit)
            {
                DBOUT(DAndersen, outs() << " !! not enough args\n");
                break;
            }
            const PAGNode *cs_arg = *csArgIt ;
            const PAGNode *fun_arg = *funArgIt;

            if (cs_arg->isPointer() && fun_arg->isPointer())
            {
                DBOUT(DAndersen, outs() << "process actual parm  " << cs_arg->toString() << " \n");
                NodeID srcAA = cs_arg->getId();
                NodeID dstFA = fun_arg->getId();
                addCopyEdge(srcAA, dstFA);
            }
        }

        //Any remaining actual args must be varargs.
        if (F->isVarArg())
        {
            NodeID vaF = svfir->getVarargNode(F);
            DBOUT(DPAGBuild, outs() << "\n      varargs:");
            for (; csArgIt != csArgEit; ++csArgIt)
            {
                const PAGNode *cs_arg = *csArgIt;
                if (cs_arg->isPointer())
                {
                    NodeID vnAA = cs_arg->getId();
                    addCopyEdge(vnAA,vaF);
                }
            }
        }
        if(csArgIt != csArgEit)
        {
            writeWrnMsg("too many args to non-vararg func.");
            writeWrnMsg("(" + cs.getInstruction()->getSourceLoc() + ")");
        }
    }
}

void CFLAlias::heapAllocatorViaIndCall(CallSite cs)
{
    assert(SVFUtil::getCallee(cs) == nullptr && "not an indirect callsite?");
    RetICFGNode* retBlockNode = svfir->getICFG()->getRetICFGNode(cs.getInstruction());
    const PAGNode* cs_return = svfir->getCallSiteRet(retBlockNode);
    NodeID srcret;
    CallSite2DummyValPN::const_iterator it = callsite2DummyValPN.find(cs);
    if(it != callsite2DummyValPN.end())
    {
        srcret = it->second;
    }
    else
    {
        NodeID valNode = svfir->addDummyValNode();
        NodeID objNode = svfir->addDummyObjNode(cs.getType());
        callsite2DummyValPN.insert(std::make_pair(cs,valNode));
        graph->addCFLNode(valNode, new CFLNode(valNode));
        graph->addCFLNode(objNode, new CFLNode(objNode));
        srcret = valNode;
    }

    NodeID dstrec = cs_return->getId();
    addCopyEdge(srcret, dstrec);
}

/*!
 * Update call graph for the input indirect callsites
 */
bool CFLAlias::updateCallGraph(const CallSiteToFunPtrMap& callsites)
{
    CallEdgeMap newEdges;
    onTheFlyCallGraphSolve(callsites,newEdges);
    for(CallEdgeMap::iterator it = newEdges.begin(), eit = newEdges.end(); it!=eit; ++it )
    {
        CallSite cs = SVFUtil::getSVFCallSite(it->first->getCallSite());
        for(FunctionSet::iterator cit = it->second.begin(), ecit = it->second.end(); cit!=ecit; ++cit)
        {
            connectCaller2CalleeParams(cs,*cit);
        }
    }

    return (!solver->isWorklistEmpty());
}

void CFLAlias::initialize()
{
    stat = new CFLStat(this);

    // Parameter Checking
    checkParameter();

    // Build CFL Grammar
    buildCFLGrammar();

    // Build CFL Graph
    buildCFLGraph();

    // Normalize CFL Grammar
    normalizeCFLGrammar();

    // Initialize sovler
    initializeSolver();
}

void CFLAlias::initializeSolver()
{
    solver = new CFLSolver(graph, grammar);
}

void CFLAlias::finalize()
{
    numOfChecks = solver->numOfChecks;

    if(Options::PrintCFL() == true)
    {
        if (Options::CFLGraph().empty())
            svfir->dump("IR");
        grammar->dump("Grammar");
        graph->dump("CFLGraph");
    }
    if (Options::CFLGraph().empty())
        PointerAnalysis::finalize();
}

void CFLAlias::solve()
{
    // Start solving
    double start = stat->getClk(true);

    solver->solve();
    if (Options::CFLGraph().empty())
    {
        while (updateCallGraph(svfir->getIndirectCallsites()))
        {
            numOfIteration++;
            solver->solve();
        }
    } // Only cflgraph built from bc could reanlyze by update call graph

    double end = stat->getClk(true);
    timeOfSolving += (end - start) / TIMEINTERVAL;
}

void POCRAlias::initializeSolver()
{
    solver = new POCRSolver(graph, grammar);
}

void POCRHybrid::initializeSolver()
{
    solver = new POCRHybridSolver(graph, grammar);
}
