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

#include "Util/ICFG.h"
#include "Util/WorkList.h"

class ICFGBuilder {

public:

    typedef std::map<const Instruction*, IntraBlockNode *> InstToBlockNodeMapTy;
    typedef PAG::PAGEdgeSet PAGEdgeSet;
    typedef std::vector<const Instruction*> InstVec;
    typedef std::set<const Instruction*> BBSet;

private:
	ICFG* icfg;
	PAG* pag;

    InstToBlockNodeMapTy InstToBlockNodeMap; ///< map a basic block to its ICFGNode

public:
    typedef FIFOWorkList<const Instruction*> WorkList;

	ICFGBuilder(ICFG* i): icfg(i), pag(PAG::getPAG()){

	}
    void build();

private:
    /// Create edges between ICFG nodes within a function
    ///@{
    void processFunEntry(const Function* fun, WorkList& worklist);

    void processFunBody(WorkList& worklist);

    void processFunExit(const Function* fun);
    //@}


    /// Add and get IntraBlock ICFGNode
    IntraBlockNode* getIntraBlockICFGNode(const Instruction* inst) {
    	InstToBlockNodeMapTy::const_iterator it = InstToBlockNodeMap.find(inst);
    	if (it == InstToBlockNodeMap.end()) {
    		IntraBlockNode* sNode = new IntraBlockNode(icfg->totalICFGNode++,inst);
    		icfg->addICFGNode(sNode);
    		InstToBlockNodeMap[inst] = sNode;
    		return sNode;
    	}
    	return it->second;
    }

    /// Add/Get an inter block ICFGNode
    InterBlockNode* getInterBlockICFGNode(const Instruction* inst);

	/// Add/Get a basic block ICFGNode
	inline ICFGNode* getBlockICFGNode(const Instruction* inst) {
		if(SVFUtil::isNonInstricCallSite(inst))
			return getInterBlockICFGNode(inst);
		else
			return getIntraBlockICFGNode(inst);
	}

    /// Create edges between ICFG nodes across functions
    void addICFGInterEdges(CallSite cs, const Function* callee);


	/// Add VFG nodes to ICFG
	//@{
	/// Add PAGEdges to ICFG
	void addPAGEdgeToICFG();
	/// Add global stores to the function entry ICFGNode of main
	void connectGlobalToProgEntry();
	/// Add VFGStmtNode into IntraBlockNode
	void handleIntraBlock(IntraBlockNode* intraICFGNode);
	/// Add ArgumentVFGNode into InterBlockNode
	void handleInterBlock(InterBlockNode* interICFGNode);
	//@}
	/// Within handleInterBlock: handle 4 kinds of ArgumentNodes
	void handleFormalParm(FunEntryBlockNode* funEntryBlockNode);
	void handleFormalRet(FunExitBlockNode* funExitBlockNode);
	void handleActualParm(CallBlockNode* callBlockNode);
	void handleActualRet(RetBlockNode* retBlockNode);
	//@}

    /// Add a function entry node
    inline FunEntryBlockNode* getFunEntryICFGNode(const Function* fun) {
		FunEntryBlockNode* b = icfg->getFunEntryICFGNode(fun);
		if (b == NULL)
			return icfg->addFunEntryICFGNode(fun);
		else
			return b;
	}
	/// Add a function exit node
	inline FunExitBlockNode* getFunExitICFGNode(const Function* fun) {
		FunExitBlockNode* b = icfg->getFunExitICFGNode(fun);
		if (b == NULL)
			return icfg->addFunExitICFGNode(fun);
		else
			return b;
	}
    /// Add a call node
    inline CallBlockNode* getCallICFGNode(CallSite cs) {
    	CallBlockNode* b = icfg->getCallICFGNode(cs);
		if (b == NULL) {
			return icfg->addCallICFGNode(cs);
		}
		return b;
    }
    /// Add a return node
    inline RetBlockNode* getRetICFGNode(CallSite cs) {
    	RetBlockNode* b = icfg->getRetICFGNode(cs);
		if (b == NULL)
			return icfg->addRetICFGNode(cs);
		else
			return b;
    }

};


#endif /* INCLUDE_UTIL_ICFGBUILDER_H_ */
