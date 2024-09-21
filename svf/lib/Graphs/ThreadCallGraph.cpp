//===- ThreadCallGraph.cpp -- Call graph considering thread fork/join---------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * ThreadCallGraph.cpp
 *
 *  Created on: Jul 12, 2014
 *      Author: Yulei Sui, Peng Di, Ding Ye
 */

#include "SVFIR/SVFModule.h"
#include "Graphs/ThreadCallGraph.h"
#include "Util/ThreadAPI.h"
#include "SVFIR/SVFIR.h"
#include "MemoryModel/PointerAnalysisImpl.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Constructor
 */
ThreadCallGraph::ThreadCallGraph() :
    CallGraph(ThdCallGraph), tdAPI(ThreadAPI::getThreadAPI())
{
    DBOUT(DGENERAL, outs() << SVFUtil::pasMsg("Building ThreadCallGraph\n"));
}


/*
 * Update call graph using pointer analysis results
 * (1) resolve function pointers for non-fork calls
 * (2) resolve function pointers for fork sites
 * (3) resolve function pointers for parallel_for sites
 */
void ThreadCallGraph::updateCallGraph(PointerAnalysis* pta)
{

    PointerAnalysis::CallEdgeMap::const_iterator iter = pta->getIndCallMap().begin();
    PointerAnalysis::CallEdgeMap::const_iterator eiter = pta->getIndCallMap().end();
    for (; iter != eiter; iter++)
    {
        const CallICFGNode* cs = iter->first;
        const CallGraph::FunctionSet &functions = iter->second;
        for (CallGraph::FunctionSet::const_iterator func_iter =
                    functions.begin(); func_iter != functions.end(); func_iter++)
        {
            const SVFFunction* callee = *func_iter;
            this->addIndirectCallGraphEdge(cs, cs->getCaller(), callee);
        }
    }

    // Fork sites
    for (CallSiteSet::const_iterator it = forksitesBegin(), eit = forksitesEnd(); it != eit; ++it)
    {
        const SVFValue* forkedval = tdAPI->getForkedFun(*it);
        if(SVFUtil::dyn_cast<SVFFunction>(forkedval)==nullptr)
        {
            SVFIR* pag = pta->getPAG();
            const NodeBS targets = pta->getPts(pag->getValueNode(forkedval)).toNodeBS();
            for (NodeBS::iterator ii = targets.begin(), ie = targets.end(); ii != ie; ii++)
            {
                if(ObjVar* objPN = SVFUtil::dyn_cast<ObjVar>(pag->getGNode(*ii)))
                {
                    const MemObj* obj = pag->getObject(objPN);
                    if(obj->isFunction())
                    {
                        const SVFFunction* svfCallee = SVFUtil::cast<SVFFunction>(obj->getValue());
                        this->addIndirectForkEdge(*it, svfCallee);
                    }
                }
            }
        }
    }
}


/*!
 * Update join edge using pointer analysis results
 */
void ThreadCallGraph::updateJoinEdge(PointerAnalysis* pta)
{

    for (CallSiteSet::const_iterator it = joinsitesBegin(), eit = joinsitesEnd(); it != eit; ++it)
    {
        const SVFValue* jointhread = tdAPI->getJoinedThread(*it);
        // find its corresponding fork sites first
        CallSiteSet forkset;
        for (CallSiteSet::const_iterator it = forksitesBegin(), eit = forksitesEnd(); it != eit; ++it)
        {
            const SVFValue* forkthread = tdAPI->getForkedThread(*it);
            if (pta->alias(jointhread, forkthread))
            {
                forkset.insert(*it);
            }
        }
        assert(!forkset.empty() && "Can't find a forksite for this join!!");
        addDirectJoinEdge(*it,forkset);
    }
}

/*!
 * Add direct fork edges
 */
bool ThreadCallGraph::addDirectForkEdge(const CallICFGNode* cs)
{

    CallGraphNode* caller = getCallGraphNode(cs->getCaller());
    const SVFFunction* forkee = SVFUtil::dyn_cast<SVFFunction>(tdAPI->getForkedFun(cs));
    assert(forkee && "callee does not exist");
    CallGraphNode* callee = getCallGraphNode(forkee->getDefFunForMultipleModule());
    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if (!hasGraphEdge(caller, callee, CallGraphEdge::TDForkEdge, csId))
    {
        assert(cs->getCaller() == caller->getFunction() && "callee instruction not inside caller??");

        ThreadForkEdge* edge = new ThreadForkEdge(caller, callee, csId);
        edge->addDirectCallSite(cs);

        addEdge(edge);
        addThreadForkEdgeSetMap(cs, edge);
        return true;
    }
    else
        return false;
}

/*!
 * Add indirect fork edge to update call graph
 */
bool ThreadCallGraph::addIndirectForkEdge(const CallICFGNode* cs, const SVFFunction* calleefun)
{
    CallGraphNode* caller = getCallGraphNode(cs->getCaller());
    CallGraphNode* callee = getCallGraphNode(calleefun);

    CallSiteID csId = addCallSite(cs, callee->getFunction());

    if (!hasGraphEdge(caller, callee, CallGraphEdge::TDForkEdge, csId))
    {
        assert(cs->getCaller() == caller->getFunction() && "callee instruction not inside caller??");

        ThreadForkEdge* edge = new ThreadForkEdge(caller, callee, csId);
        edge->addInDirectCallSite(cs);

        addEdge(edge);
        addThreadForkEdgeSetMap(cs, edge);
        return true;
    }
    else
        return false;
}

/*!
 * Add direct fork edges
 * As join edge is a special return which is back to join site(s) rather than its fork site
 * A ThreadJoinEdge is created from the functions where join sites reside in to the start routine function
 * But we don't invoke addEdge() method to add the edge to src and dst, otherwise it makes a scc cycle
 */
void ThreadCallGraph::addDirectJoinEdge(const CallICFGNode* cs,const CallSiteSet& forkset)
{

    CallGraphNode* joinFunNode = getCallGraphNode(cs->getCaller());

    for (CallSiteSet::const_iterator it = forkset.begin(), eit = forkset.end(); it != eit; ++it)
    {

        const SVFFunction* threadRoutineFun = SVFUtil::dyn_cast<SVFFunction>(tdAPI->getForkedFun(*it));
        assert(threadRoutineFun && "thread routine function does not exist");
        CallGraphNode* threadRoutineFunNode = getCallGraphNode(threadRoutineFun);
        CallSiteID csId = addCallSite(cs, threadRoutineFun);

        if (!hasThreadJoinEdge(cs,joinFunNode,threadRoutineFunNode, csId))
        {
            assert(cs->getCaller() == joinFunNode->getFunction() && "callee instruction not inside caller??");
            ThreadJoinEdge* edge = new ThreadJoinEdge(joinFunNode,threadRoutineFunNode,csId);
            edge->addDirectCallSite(cs);

            addThreadJoinEdgeSetMap(cs, edge);
        }
    }
}
