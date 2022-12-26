//===- CallGraphBuilder.cpp ----------------------------------------------------------------//
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
 * CallGraphBuilder.cpp
 *
 *  Created on:
 *      Author: Yulei
 */

#include "Util/SVFUtil.h"
#include "Util/CallGraphBuilder.h"
#include "Graphs/ICFG.h"

using namespace SVF;
using namespace SVFUtil;

PTACallGraph* CallGraphBuilder::buildCallGraph(SVFModule* svfModule)
{
    /// create nodes
    for (SVFModule::const_iterator F = svfModule->begin(), E = svfModule->end(); F != E; ++F)
    {
        callgraph->addCallGraphNode(*F);
    }

    /// create edges
    for (SVFModule::const_iterator F = svfModule->begin(), E = svfModule->end(); F != E; ++F)
    {
        for (const SVFBasicBlock* svfbb : (*F)->getBasicBlockList())
        {
            for (const SVFInstruction* inst : svfbb->getInstructionList())
            {
                if (SVFUtil::isNonInstricCallSite(inst))
                {
                    if(const SVFFunction* callee = getCallee(inst))
                    {
                        const CallICFGNode* callBlockNode = icfg->getCallICFGNode(inst);
                        callgraph->addDirectCallGraphEdge(callBlockNode,*F,callee);
                    }
                }
            }
        }
    }

    return callgraph;
}

PTACallGraph* ThreadCallGraphBuilder::buildThreadCallGraph(SVFModule* svfModule)
{

    buildCallGraph(svfModule);

    ThreadCallGraph* cg = dyn_cast<ThreadCallGraph>(callgraph);
    assert(cg && "not a thread callgraph?");

    ThreadAPI* tdAPI = ThreadAPI::getThreadAPI();
    for (SVFModule::const_iterator F = svfModule->begin(), E = svfModule->end(); F != E; ++F)
    {
        for (const SVFBasicBlock* svfbb : (*F)->getBasicBlockList())
        {
            for (const SVFInstruction* inst : svfbb->getInstructionList())
            {
                if (tdAPI->isTDFork(inst))
                {
                    const CallICFGNode* cs = icfg->getCallICFGNode(inst);
                    cg->addForksite(cs);
                    const SVFFunction* forkee = SVFUtil::dyn_cast<SVFFunction>(tdAPI->getForkedFun(inst));
                    if (forkee)
                    {
                        cg->addDirectForkEdge(cs);
                    }
                    // indirect call to the start routine function
                    else
                    {
                        cg->addThreadForkEdgeSetMap(cs,nullptr);
                    }
                }
                else if (tdAPI->isHareParFor(inst))
                {
                    const CallICFGNode* cs = icfg->getCallICFGNode(inst);
                    cg->addParForSite(cs);
                    const SVFFunction* taskFunc = SVFUtil::dyn_cast<SVFFunction>(tdAPI->getTaskFuncAtHareParForSite(inst));
                    if (taskFunc)
                    {
                        cg->addDirectParForEdge(cs);
                    }
                    // indirect call to the start routine function
                    else
                    {
                        cg->addHareParForEdgeSetMap(cs,nullptr);
                    }
                }
            }
        }
    }
    // record join sites
    for (SVFModule::const_iterator F = svfModule->begin(), E = svfModule->end(); F != E; ++F)
    {
        for (const SVFBasicBlock* svfbb : (*F)->getBasicBlockList())
        {
            for (const SVFInstruction* inst : svfbb->getInstructionList())
            {
                if (tdAPI->isTDJoin(inst))
                {
                    const CallICFGNode* cs = icfg->getCallICFGNode(inst);
                    cg->addJoinsite(cs);
                }
            }
        }
    }

    return cg;
}




