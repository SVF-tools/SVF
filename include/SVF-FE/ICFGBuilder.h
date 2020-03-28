//===- ICFGBuilder.h ----------------------------------------------------------------//
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
 * ICFGBuilder.h
 *
 *  Created on:
 *      Author: yulei
 */

#ifndef INCLUDE_UTIL_ICFGBUILDER_H_
#define INCLUDE_UTIL_ICFGBUILDER_H_

#include "Graphs/ICFG.h"
#include "Util/WorkList.h"

class ICFGBuilder {

public:

    typedef std::vector<const Instruction*> InstVec;
    typedef std::set<const Instruction*> BBSet;

private:
	ICFG* icfg;

public:
    typedef FIFOWorkList<const Instruction*> WorkList;

	ICFGBuilder(ICFG* i): icfg(i) {

	}
    void build(SVFModule* svfModule);

private:
    /// Create edges between ICFG nodes within a function
    ///@{
    void processFunEntry(const Function* fun, WorkList& worklist);

    void processFunBody(WorkList& worklist);

    void processFunExit(const Function* fun);
    //@}


    /// Add/Get an inter block ICFGNode
    InterBlockNode* getOrAddInterBlockICFGNode(const Instruction* inst);

	/// Add/Get a basic block ICFGNode
	inline ICFGNode* getOrAddBlockICFGNode(const Instruction* inst) {
		if(SVFUtil::isNonInstricCallSite(inst))
			return getOrAddInterBlockICFGNode(inst);
		else
			return getOrAddIntraBlockICFGNode(inst);
	}

    /// Create edges between ICFG nodes across functions
    void addICFGInterEdges(CallSite cs, const Function* callee);

    /// Add a function entry node
    inline FunEntryBlockNode* getOrAddFunEntryICFGNode(const Function* fun) {
		FunEntryBlockNode* b = icfg->getFunEntryICFGNode(fun);
		if (b == NULL)
			return icfg->addFunEntryICFGNode(fun);
		else
			return b;
	}
	/// Add a function exit node
	inline FunExitBlockNode* getOrAddFunExitICFGNode(const Function* fun) {
		FunExitBlockNode* b = icfg->getFunExitICFGNode(fun);
		if (b == NULL)
			return icfg->addFunExitICFGNode(fun);
		else
			return b;
	}
    /// Add a call node
    inline CallBlockNode* getOrAddCallICFGNode(CallSite cs) {
    	CallBlockNode* b = icfg->getCallICFGNode(cs);
		if (b == NULL) {
			return icfg->addCallICFGNode(cs);
		}
		return b;
    }
    /// Add a return node
    inline RetBlockNode* getOrAddRetICFGNode(CallSite cs) {
    	RetBlockNode* b = icfg->getRetICFGNode(cs);
		if (b == NULL)
			return icfg->addRetICFGNode(cs);
		else
			return b;
    }

    /// Add and get IntraBlock ICFGNode
    IntraBlockNode* getOrAddIntraBlockICFGNode(const Instruction* inst) {
		IntraBlockNode* b = icfg->getIntraBlockICFGNode(inst);
		if (b == NULL)
			return icfg->addIntraBlockICFGNode(inst);
		else
			return b;
    }
};


#endif /* INCLUDE_UTIL_ICFGBUILDER_H_ */
