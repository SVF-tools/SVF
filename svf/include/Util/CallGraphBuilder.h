//===- CallGraphBuilder.h ----------------------------------------------------------------//
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
// Created by Weigang He on 21/09/24.
//

#ifndef SVF_CALLGRAPHBUILDER_H
#define SVF_CALLGRAPHBUILDER_H

#include "Graphs/CallGraph.h"

namespace SVF
{

class ICFG;

class CallGraphBuilder
{

protected:
    CallGraph* callgraph;
    ICFG* icfg;

public:
    CallGraphBuilder(CallGraph* cg, ICFG* i) : callgraph(cg), icfg(i) {}

    /// Build normal callgraph
    CallGraph* buildCallGraph(SVFModule* svfModule);
};
}
#endif // SVF_CALLGRAPHBUILDER_H
