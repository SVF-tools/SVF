/*
 * PCG.cpp
 *
 *  Created on: Jun 24, 2015
 *      Author: Yulei Sui, Peng Di
 */

#include "Util/Options.h"
#include "MTA/PCG.h"
#include "Util/SVFUtil.h"
#include "SVF-FE/BasicTypes.h"

using namespace SVF;
using namespace SVFUtil;

//=====================================================//
/*!
 * Whether two functions may happen in parallel
 */

//static llvm::cl::opt<bool> TDPrint("print-td", llvm::cl::init(true), llvm::cl::desc("Print Thread Analysis Results"));
bool PCG::analyze()
{

    //callgraph = new PTACallGraph(mod);

    DBOUT(DMTA, outs() << pasMsg("Starting MHP analysis\n"));

    initFromThreadAPI(mod);

    inferFromCallGraph();

    //interferenceAnalysis();

    //if (Options::TDPrint) {
    //printResults();
    //tdAPI->performAPIStat(mod);
    //}
    return false;
}

bool PCG::mayHappenInParallelBetweenFunctions(const Function* fun1, const Function* fun2) const
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

bool PCG::mayHappenInParallel(const Instruction* i1, const Instruction* i2) const
{
    const Function* fun1 = i1->getParent()->getParent();
    const Function* fun2 = i2->getParent()->getParent();
    return mayHappenInParallelBetweenFunctions(fun1, fun2);
}


/*!
 * Initialize thread spawners and spawnees from threadAPI functions
 * a procedure is a spawner if it creates a thread and the created thread is still existent on its return
 * a procedure is a spawnee if it is created by fork call
 */
void PCG::initFromThreadAPI(SVFModule* module)
{
    for (SVFModule::const_iterator fi = module->begin(), efi = module->end(); fi != efi; ++fi)
    {
        const Function* fun = (*fi)->getLLVMFun();
        for (inst_iterator II = inst_begin((*fi)->getLLVMFun()), E = inst_end((*fi)->getLLVMFun()); II != E; ++II)
        {
            const Instruction *inst = &*II;
            if (tdAPI->isTDFork(inst))
            {
                const Value* forkVal = tdAPI->getForkedFun(inst);
                if (const Function* forkFun = getLLVMFunction(forkVal))
                {
                    addSpawnsite(inst);
                    spawners.insert(fun);
                    spawnees.insert(forkFun);
                }
                /// TODO: handle indirect call here for the fork Fun
                else
                {
                    writeWrnMsg("pthread create");
                    outs() << SVFUtil::value2String(inst) << "\n";
                    writeWrnMsg("invoke spawnee indirectly");
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
        const Function* fun = worklist.pop();
        const SVFFunction* svffun = getSVFFun(fun);
        PTACallGraphNode* funNode = callgraph->getCallGraphNode(svffun);
        for (PTACallGraphNode::const_iterator it = funNode->InEdgeBegin(), eit = funNode->InEdgeEnd(); it != eit;
                ++it)
        {
            PTACallGraphEdge* callEdge = (*it);
            const Function* caller = callEdge->getSrcNode()->getFunction()->getLLVMFun();
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
        const Function* fun = worklist.pop();
        const SVFFunction* svffun = getSVFFun(fun);
        PTACallGraphNode* funNode = callgraph->getCallGraphNode(svffun);
        for (PTACallGraphNode::const_iterator it = funNode->OutEdgeBegin(), eit = funNode->OutEdgeEnd(); it != eit;
                ++it)
        {
            const Function* caller = (*it)->getDstNode()->getFunction()->getLLVMFun();
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
        const Instruction* inst = *sit;
        BBWorkList bb_worklist;
        Set<const BasicBlock*> visitedBBs;
        bb_worklist.push(inst->getParent());
        while (!bb_worklist.empty())
        {
            const BasicBlock* bb = bb_worklist.pop();
            for (BasicBlock::const_iterator it = bb->begin(), eit = bb->end(); it != eit; ++it)
            {
                const Instruction* inst = &*it;
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
                            addFollowerFun(edge->getDstNode()->getFunction()->getLLVMFun());
                        }
                    }
                }
            }
            for (succ_const_iterator succ_it = succ_begin(bb); succ_it != succ_end(bb); succ_it++)
            {
                const BasicBlock* succ_bb = *succ_it;
                if (visitedBBs.count(succ_bb) == 0)
                {
                    visitedBBs.insert(succ_bb);
                    bb_worklist.push(succ_bb);
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
        const Function* fun = worklist.pop();
        const SVFFunction* svffun = getSVFFun(fun);
        PTACallGraphNode* funNode = callgraph->getCallGraphNode(svffun);
        for (PTACallGraphNode::const_iterator it = funNode->OutEdgeBegin(), eit = funNode->OutEdgeEnd(); it != eit;
                ++it)
        {
            const Function* caller = (*it)->getDstNode()->getFunction()->getLLVMFun();
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
        worklist.push_back(fun->getLLVMFun());
    }

    while (!worklist.empty())
    {
        const Function* fun1 = worklist.back();
        worklist.pop_back();

        bool ismhpfun = false;
        for (PCG::FunVec::iterator it = worklist.begin(), eit = worklist.end(); it != eit; ++it)
        {
            const Function* fun2 = *it;
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
        const Function* fun = (*fi)->getLLVMFun();
        if (fun->isDeclaration())
            continue;

        std::string isSpawner = isSpawnerFun(fun) ? " SPAWNER " : "";
        std::string isSpawnee = isSpawneeFun(fun) ? " CHILDREN " : "";
        std::string isFollower = isFollowerFun(fun) ? " FOLLOWER " : "";
        outs() << fun->getName().str() << " [" << isSpawner << isSpawnee << isFollower << "]\n";
    }
}
