//===- CallGraphBuilder.cpp ----------------------------------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//


/*
 * CallGraphBuilder.cpp
 *
 *  Created on:
 *      Author: Yulei
 */

#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/CallGraphBuilder.h"
#include "Graphs/ICFG.h"

using namespace SVF;
using namespace SVFUtil;

PTACallGraph* CallGraphBuilder::buildCallGraph(SVFModule* svfModule)
{
    /// create nodes
    for (SVFModule::llvm_iterator F = svfModule->llvmFunBegin(), E = svfModule->llvmFunEnd(); F != E; ++F)
    {
        const SVFFunction* fun = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(*F);
        callgraph->addCallGraphNode(fun);
    }

    /// create edges
    for (SVFModule::llvm_iterator F = svfModule->llvmFunBegin(), E = svfModule->llvmFunEnd(); F != E; ++F)
    {
        Function *fun = *F;
        for (inst_iterator I = inst_begin(*fun), J = inst_end(*fun); I != J; ++I)
        {
            const Instruction *inst = &*I;
            if (SVFUtil::isNonInstricCallSite(inst))
            {
                if(const SVFFunction* callee = getCallee(inst))
                {
                    const CallICFGNode* callBlockNode = icfg->getCallICFGNode(inst);
                    const SVFFunction* caller = LLVMModuleSet::getLLVMModuleSet()->getSVFFunction(fun);
                    callgraph->addDirectCallGraphEdge(callBlockNode,caller,callee);
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
    for (SVFModule::llvm_const_iterator fi = svfModule->llvmFunBegin(), efi = svfModule->llvmFunEnd(); fi != efi; ++fi)
    {
        const Function *fun = *fi;
        for (const_inst_iterator II = inst_begin(*fun), E = inst_end(*fun); II != E; ++II)
        {
            const Instruction *inst = &*II;
            if (tdAPI->isTDFork(inst))
            {
                const CallICFGNode* cs = icfg->getCallICFGNode(inst);
                cg->addForksite(cs);
                const Function* forkee = SVFUtil::dyn_cast<Function>(tdAPI->getForkedFun(inst));
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
                const Function* taskFunc = SVFUtil::dyn_cast<Function>(tdAPI->getTaskFuncAtHareParForSite(inst));
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
    // record join sites
    for (SVFModule::llvm_const_iterator fi = svfModule->llvmFunBegin(), efi = svfModule->llvmFunEnd(); fi != efi; ++fi)
    {
        const Function *fun = *fi;
        for (const_inst_iterator II = inst_begin(*fun), E = inst_end(*fun); II != E; ++II)
        {
            const Instruction *inst = &*II;
            if (tdAPI->isTDJoin(inst))
            {
                const CallICFGNode* cs = icfg->getCallICFGNode(inst);
                cg->addJoinsite(cs);
            }
        }
    }

    return cg;
}




