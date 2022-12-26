//===- PCG.cpp -- Procedure creation graph-------------//
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
 * PCG.cpp
 *
 *  Created on: Jun 24, 2015
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "MTA/PCG.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

//=====================================================//
/*!
 * Whether two functions may happen in parallel
 */

//static Option<bool> TDPrint(
//    "print-td",
//    true,
//    "Print Thread Analysis Results"
//);

bool PCG::analyze()
{

    //callgraph = new PTACallGraph(mod);

    DBOUT(DMTA, outs() << pasMsg("Starting MHP analysis\n"));

    initFromThreadAPI(mod);

    inferFromCallGraph();

    //interferenceAnalysis();

    //if (Options::TDPrint()) {
    //printResults();
    //tdAPI->performAPIStat(mod);
    //}
    return false;
}

bool PCG::mayHappenInParallelBetweenFunctions(const SVFFunction* fun1, const SVFFunction* fun2) const
{
    // if neither of functions are spawnees, then they won't happen in parallel
    if (isSpawneeFun(fun1) == false && isSpawneeFun(fun2) == false)
        return false;
    // if there exit one of the function are not spawner, spawnee or follower, then they won't happen in parallel
    if (isSpawnerFun(fun1) == false && isSpawneeFun(fun1) == false && isFollowerFun(fun1) == false)
        return false;
    if (isSpawnerFun(fun2) == false && isSpawneeFun(fun2) == false && isFollowerFun(fun2) == false)
        return false;

    return true;
}

bool PCG::mayHappenInParallel(const SVFInstruction* i1, const SVFInstruction* i2) const
{
    const SVFFunction* fun1 = i1->getFunction();
    const SVFFunction* fun2 = i2->getFunction();
    return mayHappenInParallelBetweenFunctions(fun1, fun2);
}


/*!
 * Initialize thread spawners and spawnees from threadAPI functions
 * a procedure is a spawner if it creates a thread and the created thread is still existent on its return
 * a procedure is a spawnee if it is created by fork call
 */
void PCG::initFromThreadAPI(SVFModule* module)
{
    for (const SVFFunction* fun : module->getFunctionSet())
    {
        for (const SVFBasicBlock* svfbb : fun->getBasicBlockList())
        {
            for (const SVFInstruction* inst : svfbb->getInstructionList())
            {
                if (tdAPI->isTDFork(inst))
                {
                    const SVFValue* forkVal = tdAPI->getForkedFun(inst);
                    if (const SVFFunction* svForkfun = SVFUtil::dyn_cast<SVFFunction>(forkVal))
                    {
                        addSpawnsite(inst);
                        spawners.insert(fun);
                        spawnees.insert(svForkfun);
                    }
                    /// TODO: handle indirect call here for the fork Fun
                    else
                    {
                        writeWrnMsg("pthread create");
                        outs() << inst->toString() << "\n";
                        writeWrnMsg("invoke spawnee indirectly");
                    }
                }
            }
        }
    }
}

/*!
 * 	Infer spawners and spawnees from call graph. The inference are recursively done
 * 	spawners: procedures may create a thread and return with the created thread still running
 * 	spawnees: procedures may be executed as a spawned thread
 * 	followers: procedures may be invoked by a thread after the thread returns from a spawner
 * 	(procedure may be called after pthread_creat is called).
 */
void PCG::inferFromCallGraph()
{

    collectSpawners();

    collectSpawnees();

    collectFollowers();
}

/*!
 * spawner: given a spawner, all its callers on callgraph are spawners
 */
void PCG::collectSpawners()
{

    /// find all the spawners recursively on call graph
    FunWorkList worklist;
    for (FunSet::iterator it = spawners.begin(), eit = spawners.end(); it != eit; ++it)
    {
        worklist.push(*it);
    }
    while (!worklist.empty())
    {
        const SVFFunction* svffun = worklist.pop();
        PTACallGraphNode* funNode = callgraph->getCallGraphNode(svffun);
        for (PTACallGraphNode::const_iterator it = funNode->InEdgeBegin(), eit = funNode->InEdgeEnd(); it != eit;
                ++it)
        {
            PTACallGraphEdge* callEdge = (*it);
            const SVFFunction* caller = callEdge->getSrcNode()->getFunction();
            if (isSpawnerFun(caller) == false)
            {
                worklist.push(caller);
                addSpawnerFun(caller);
            }
            /// add all the callsites from callers to callee (spawner) as a spawn site.
            for (PTACallGraphEdge::CallInstSet::const_iterator dit = callEdge->directCallsBegin(), deit =
                        callEdge->directCallsEnd(); dit != deit; ++dit)
            {
                addSpawnsite((*dit)->getCallSite());
            }
            for (PTACallGraphEdge::CallInstSet::const_iterator dit = callEdge->indirectCallsBegin(), deit =
                        callEdge->indirectCallsEnd(); dit != deit; ++dit)
            {
                addSpawnsite((*dit)->getCallSite());
            }
        }
    }
}

/*!
 * spawnee: given a spawnee, all its callees on callgraph are spawnees
 */
void PCG::collectSpawnees()
{

    /// find all the spawnees recursively on call graph
    FunWorkList worklist;
    for (FunSet::iterator it = spawnees.begin(), eit = spawnees.end(); it != eit; ++it)
    {
        worklist.push(*it);
    }
    while (!worklist.empty())
    {
        const SVFFunction* svffun = worklist.pop();
        PTACallGraphNode* funNode = callgraph->getCallGraphNode(svffun);
        for (PTACallGraphNode::const_iterator it = funNode->OutEdgeBegin(), eit = funNode->OutEdgeEnd(); it != eit;
                ++it)
        {
            const SVFFunction* caller = (*it)->getDstNode()->getFunction();
            if (isSpawneeFun(caller) == false)
            {
                worklist.push(caller);
                addSpawneeFun(caller);
            }
        }
    }
}

/*!
 * Identify initial followers
 * a procedure whose callsite lies in a control flow path that starts just after a spawner's callsite
 */
void PCG::identifyFollowers()
{

    for (CallInstSet::const_iterator sit = spawnSitesBegin(), esit = spawnSitesEnd(); sit != esit; ++sit)
    {
        const SVFInstruction* inst = *sit;
        BBWorkList bb_worklist;
        Set<const SVFBasicBlock*> visitedBBs;
        bb_worklist.push(inst->getParent());
        while (!bb_worklist.empty())
        {
            const SVFBasicBlock* bb = bb_worklist.pop();
            for (SVFBasicBlock::const_iterator it = bb->begin(), eit = bb->end(); it != eit; ++it)
            {
                const SVFInstruction* inst = *it;
                // mark the callee of this callsite as follower
                // if this is an call/invoke instruction but not a spawn site
                if ((SVFUtil::isCallSite(inst)) && !isSpawnsite(inst))
                {
                    CallICFGNode* cbn = getCallICFGNode(inst);
                    if (callgraph->hasCallGraphEdge(cbn))
                    {
                        for (PTACallGraph::CallGraphEdgeSet::const_iterator cgIt = callgraph->getCallEdgeBegin(cbn),
                                ecgIt = callgraph->getCallEdgeEnd(cbn); cgIt != ecgIt; ++cgIt)
                        {
                            const PTACallGraphEdge* edge = *cgIt;
                            addFollowerFun(edge->getDstNode()->getFunction());
                        }
                    }
                }
            }
            for (const SVFBasicBlock* svf_scc_bb : bb->getSuccessors())
            {
                if (visitedBBs.count(svf_scc_bb) == 0)
                {
                    visitedBBs.insert(svf_scc_bb);
                    bb_worklist.push(svf_scc_bb);
                }
            }
        }
    }

}

/*!
 * collect follower procedures which may be called after pthread_create is invoked directly or indirectly
 * a procedure which is called from a follower is also a follower.
 */
void PCG::collectFollowers()
{

    /// identify initial followers
    identifyFollowers();

    /// find all the followers recursively on call graph
    FunWorkList worklist;
    for (FunSet::iterator it = followers.begin(), eit = followers.end(); it != eit; ++it)
    {
        worklist.push(*it);
    }
    while (!worklist.empty())
    {
        const SVFFunction* svffun = worklist.pop();
        PTACallGraphNode* funNode = callgraph->getCallGraphNode(svffun);
        for (PTACallGraphNode::const_iterator it = funNode->OutEdgeBegin(), eit = funNode->OutEdgeEnd(); it != eit;
                ++it)
        {
            const SVFFunction* caller = (*it)->getDstNode()->getFunction();
            if (isFollowerFun(caller) == false)
            {
                worklist.push(caller);
                addFollowerFun(caller);
            }
        }
    }
}

/*!
 * Thread interference analysis,
 * Suppose we have a undirected graph G = {F,E,I}
 * F denotes procedure,
 * E represents interference edge (x,y) \in E, x \in F, y \in F
 * means execution of x in one thread may overlap execution of y in another thread
 * I(x,y) is a set of memory locations for this interference edge
 */
void PCG::interferenceAnalysis()
{

//	DBOUT(DMTA, outs() << pasMsg("Starting Race Detection\n"));

    PCG::FunVec worklist;
    for (SVFModule::const_iterator F = mod->begin(), E = mod->end(); F != E; ++F)
    {
        const SVFFunction* fun = *F;
        if (isExtCall(fun))
            continue;
        worklist.push_back(fun);
    }

    while (!worklist.empty())
    {
        const SVFFunction* fun1 = worklist.back();
        worklist.pop_back();

        bool ismhpfun = false;
        for (PCG::FunVec::iterator it = worklist.begin(), eit = worklist.end(); it != eit; ++it)
        {
            const SVFFunction* fun2 = *it;
            if (mayHappenInParallelBetweenFunctions(fun1, fun2))
            {
                ismhpfun = true;
                mhpfuns.insert(fun2);
            }
        }
        if (ismhpfun)
        {
            mhpfuns.insert(fun1);
        }
    }
}

/*!
 * Print analysis results
 */
void PCG::printResults()
{

    printTDFuns();
}

/*!
 * Print Thread sensitive properties for each function
 */
void PCG::printTDFuns()
{

    for (SVFModule::const_iterator fi = mod->begin(), efi = mod->end(); fi != efi; ++fi)
    {
        const SVFFunction* fun = (*fi);
        if (fun->isDeclaration())
            continue;

        std::string isSpawner = isSpawnerFun(fun) ? " SPAWNER " : "";
        std::string isSpawnee = isSpawneeFun(fun) ? " CHILDREN " : "";
        std::string isFollower = isFollowerFun(fun) ? " FOLLOWER " : "";
        outs() << fun->getName() << " [" << isSpawner << isSpawnee << isFollower << "]\n";
    }
}
