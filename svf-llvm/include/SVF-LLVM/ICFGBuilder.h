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

    ICFGBuilder(ICFG* i): icfg(i)
    {

    }
    void build();

private:

    LLVMModuleSet* llvmModuleSet() {
        return LLVMModuleSet::getLLVMModuleSet();
    }

    CSToRetNodeMapTy& csToRetNodeMap() {
        return llvmModuleSet()->CSToRetNodeMap;
    }

    CSToCallNodeMapTy& csToCallNodeMap() {
        return llvmModuleSet()->CSToCallNodeMap;
    }

    InstToBlockNodeMapTy& instToBlockNodeMap() {
        return llvmModuleSet()->InstToBlockNodeMap;
    }

    FunToFunEntryNodeMapTy& funToFunEntryNodeMap() {
        return llvmModuleSet()->FunToFunEntryNodeMap;
    }

    FunToFunExitNodeMapTy& funToFunExitNodeMap() {
        return llvmModuleSet()->FunToFunExitNodeMap;
    }

private:

    /// Create edges between ICFG nodes within a function
    ///@{
    void processFunEntry(const Function*  fun, WorkList& worklist);

    void processNoPrecessorBasicBlocks(const Function*  fun, WorkList& worklist);

    void processFunBody(WorkList& worklist);

    void processFunExit(const Function*  fun);
    //@}

    void checkICFGNodesVisited(const Function* fun);

    void connectGlobalToProgEntry();

    inline void addGlobalICFGNode()
    {
        icfg->globalBlockNode = new GlobalICFGNode(icfg->totalICFGNode++);
        icfg->addICFGNode(icfg->globalBlockNode);
    }

    inline GlobalICFGNode* getGlobalICFGNode() const
    {
        return icfg->getGlobalICFGNode();
    }

    /// Add/Get an inter block ICFGNode
    InterICFGNode* addInterBlockICFGNode(const Instruction* inst);

    /// Add/Get a basic block ICFGNode
    inline ICFGNode* addBlockICFGNode(const Instruction* inst);

    /// Create edges between ICFG nodes across functions
    void addICFGInterEdges(const Instruction*  cs, const Function*  callee);

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

    ICFGNode* getICFGNode(const Instruction* inst);

    bool hasICFGNode(const Instruction* inst);

    /// get a call node
    CallICFGNode* getCallICFGNode(const Instruction*  cs);
    /// get a return node
    RetICFGNode* getRetICFGNode(const Instruction*  cs);
    /// get a intra node
    IntraICFGNode* getIntraICFGNode(const Instruction* inst);

    /// Add and get IntraBlock ICFGNode
    IntraICFGNode* addIntraBlockICFGNode(const Instruction* inst);

    /// Get/Add a call node
    CallICFGNode* getCallBlock(const Instruction* cs);

    /// Get/Add a return node
    inline RetICFGNode* getRetBlock(const Instruction* cs)
    {
        CSToRetNodeMapTy::const_iterator it = csToRetNodeMap().find(cs);
        if (it == csToRetNodeMap().end())
            return nullptr;
        return it->second;
    }

    inline IntraICFGNode* getIntraBlock(const Instruction* inst)
    {
        InstToBlockNodeMapTy::const_iterator it = instToBlockNodeMap().find(inst);
        if (it == instToBlockNodeMap().end())
            return nullptr;
        return it->second;
    }

    /// Get/Add a function entry node
    inline FunEntryICFGNode* getFunEntryBlock(const Function* fun)
    {
        FunToFunEntryNodeMapTy::const_iterator it = funToFunEntryNodeMap().find(fun);
        if (it == funToFunEntryNodeMap().end())
            return nullptr;
        return it->second;
    }
    FunEntryICFGNode* addFunEntryBlock(const Function* fun);

    /// Get/Add a function exit node
    inline FunExitICFGNode* getFunExitBlock(const Function* fun)
    {
        FunToFunExitNodeMapTy::const_iterator it = funToFunExitNodeMap().find(fun);
        if (it == funToFunExitNodeMap().end())
            return nullptr;
        return it->second;
    }
    FunExitICFGNode* addFunExitBlock(const Function* fun);


    /// Add a function entry node
    inline FunEntryICFGNode* getFunEntryICFGNode(const Function*  fun)
    {
        FunEntryICFGNode* b = getFunEntryBlock(fun);
        assert(b && "Function entry not created?");
        return b;
    }
    /// Add a function exit node
    inline FunExitICFGNode* getFunExitICFGNode(const Function*  fun)
    {
        FunExitICFGNode* b = getFunExitBlock(fun);
        assert(b && "Function exit not created?");
        return b;
    }

private:
    BBSet visited;
};

} // End namespace SVF

#endif /* INCLUDE_UTIL_ICFGBUILDER_H_ */
