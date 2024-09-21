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


//
// Created by WeigangHe on 21/09/24.
//

#include "Graphs/ICFG.h"
#include "Util/CallGraphBuilder.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;


CallGraph* CallGraphBuilder::buildCallGraph(SVFModule* svfModule)
{
    /// create nodes
    for (SVFModule::const_callgraphnode_iterator F = svfModule->callgraphnode_begin(), E = svfModule->callgraphnode_end(); F != E; ++F)
    {
        callgraph->addCallGraphNode(*F);
    }

    /// create edges
    for (SVFModule::const_callgraphnode_iterator F = svfModule->callgraphnode_begin(), E = svfModule->callgraphnode_end(); F != E; ++F)
    {
        for (const SVFBasicBlock* svfbb : (*F)->getFunction()->getBasicBlockList())
        {
            for (const ICFGNode* inst : svfbb->getICFGNodeList())
            {
                if (SVFUtil::isNonInstricCallSite(inst))
                {
                    const CallICFGNode* callBlockNode = cast<CallICFGNode>(inst);
                    if(const SVFFunction* callee = callBlockNode->getCalledFunction())
                    {
                        callgraph->addDirectCallGraphEdge(callBlockNode,(*F)->getFunction(),callee);
                    }
                }
            }
        }
    }

    return callgraph;
}
