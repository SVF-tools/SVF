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
#include "BasicTypes.h"
#include "LLVMModule.h"

namespace SVF
{

class ICFGBuilder
{

public:

    typedef std::vector<const Instruction*> InstVec;
    typedef Set<const Instruction*> BBSet;
    typedef Map<const Instruction*, CallICFGNode *> CSToCallNodeMapTy;
    typedef Map<const Instruction*, RetICFGNode *> CSToRetNodeMapTy;
    typedef Map<const Instruction*, IntraICFGNode *> InstToBlockNodeMapTy;
    typedef Map<const Function*, FunEntryICFGNode *> FunToFunEntryNodeMapTy;
    typedef Map<const Function*, FunExitICFGNode *> FunToFunExitNodeMapTy;


private:
    ICFG* icfg;

public:
    typedef FIFOWorkList<const Instruction*> WorkList;

    ICFGBuilder() = default;

    ICFG* build();

private:

    inline LLVMModuleSet* llvmModuleSet()
    {
        return LLVMModuleSet::getLLVMModuleSet();
    }

private:

    /// Create edges between ICFG nodes within a function
    ///@{
    void processFunEntry(const Function*  fun, WorkList& worklist);

    void processUnreachableFromEntry(const Function*  fun, WorkList& worklist);

    void processFunBody(WorkList& worklist);

    void processFunExit(const Function*  fun);
    //@}

    void checkICFGNodesVisited(const Function* fun);

    void connectGlobalToProgEntry();


    /// Create edges between ICFG nodes across functions
    void addICFGInterEdges(const Instruction*  cs, const Function*  callee);

    inline ICFGNode* getICFGNode(const Instruction* inst)
    {
        return llvmModuleSet()->getICFGNode(inst);
    }

    inline bool hasICFGNode(const Instruction* inst)
    {
        return llvmModuleSet()->hasICFGNode(inst);
    }

    /// get a call node
    inline CallICFGNode* getCallICFGNode(const Instruction*  cs)
    {
        return llvmModuleSet()->getCallICFGNode(cs);
    }
    /// get a return node
    inline RetICFGNode* getRetICFGNode(const Instruction*  cs)
    {
        return llvmModuleSet()->getRetICFGNode(cs);
    }
    /// get a intra node
    inline IntraICFGNode* getIntraICFGNode(const Instruction* inst)
    {
        return llvmModuleSet()->getIntraICFGNode(inst);
    }

    /// get a function entry node
    inline FunEntryICFGNode* getFunEntryICFGNode(const Function*  fun)
    {
        return llvmModuleSet()->getFunEntryICFGNode(fun);
    }
    /// get a function exit node
    inline FunExitICFGNode* getFunExitICFGNode(const Function*  fun)
    {
        return llvmModuleSet()->getFunExitICFGNode(fun);
    }

    inline GlobalICFGNode* getGlobalICFGNode() const
    {
        return icfg->getGlobalICFGNode();
    }

    /// Add/Get an inter block ICFGNode
    InterICFGNode* addInterBlockICFGNode(const Instruction* inst);

    /// Add/Get a basic block ICFGNode
    inline ICFGNode* addBlockICFGNode(const Instruction* inst);

    /// Add and get IntraBlock ICFGNode
    IntraICFGNode* addIntraBlockICFGNode(const Instruction* inst);

    FunEntryICFGNode* addFunEntryBlock(const Function* fun);

    FunExitICFGNode* addFunExitBlock(const Function* fun);

    inline void addGlobalICFGNode()
    {
        icfg->globalBlockNode = new GlobalICFGNode(icfg->totalICFGNode++);
        icfg->addICFGNode(icfg->globalBlockNode);
    }

private:
    BBSet visited;
};

} // End namespace SVF

#endif /* INCLUDE_UTIL_ICFGBUILDER_H_ */
