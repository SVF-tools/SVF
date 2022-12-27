//===- ICFGBuilder.h ----------------------------------------------------------------//
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
 * ICFGBuilder.h
 *
 *  Created on:
 *      Author: yulei
 */

#ifndef INCLUDE_UTIL_ICFGBUILDER_H_
#define INCLUDE_UTIL_ICFGBUILDER_H_

#include "Graphs/ICFG.h"
#include "Util/WorkList.h"

namespace SVF
{

class ICFGBuilder
{

public:

    typedef std::vector<const Instruction*> InstVec;
    typedef Set<const Instruction*> BBSet;

private:
    ICFG* icfg;

public:
    typedef FIFOWorkList<const Instruction*> WorkList;

    ICFGBuilder(ICFG* i): icfg(i)
    {

    }
    void build(SVFModule* svfModule);

private:
    /// Create edges between ICFG nodes within a function
    ///@{
    void processFunEntry(const Function*  fun, WorkList& worklist);

    void processFunBody(WorkList& worklist);

    void processFunExit(const Function*  fun);
    //@}

    void connectGlobalToProgEntry(SVFModule* svfModule);

    /// Add/Get an inter block ICFGNode
    InterICFGNode* getOrAddInterBlockICFGNode(const SVFInstruction* inst);

    /// Add/Get a basic block ICFGNode
    inline ICFGNode* getOrAddBlockICFGNode(const SVFInstruction* inst)
    {
        if(SVFUtil::isNonInstricCallSite(inst))
            return getOrAddInterBlockICFGNode(inst);
        else
            return getOrAddIntraBlockICFGNode(inst);
    }

    /// Create edges between ICFG nodes across functions
    void addICFGInterEdges(const SVFInstruction*  cs, const SVFFunction*  callee);

    /// Add a call node
    inline CallICFGNode* getCallICFGNode(const SVFInstruction*  cs)
    {
        return icfg->getCallICFGNode(cs);
    }
    /// Add a return node
    inline RetICFGNode* getRetICFGNode(const SVFInstruction*  cs)
    {
        return icfg->getRetICFGNode(cs);
    }

    /// Add and get IntraBlock ICFGNode
    IntraICFGNode* getOrAddIntraBlockICFGNode(const SVFInstruction* inst)
    {
        return icfg->getIntraICFGNode(inst);
    }
};

} // End namespace SVF

#endif /* INCLUDE_UTIL_ICFGBUILDER_H_ */
