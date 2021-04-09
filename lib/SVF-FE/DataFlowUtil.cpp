//===- DataFlowUtil.cpp -- Helper class for data-flow analysis----------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2017>  <Yulei Sui>
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
 * DataFlowUtil.cpp
 *
 *  Created on: Jun 4, 2013
 *      Author: Yulei Sui
 */

#include "SVF-FE/DataFlowUtil.h"
#include "SVF-FE/LLVMModule.h"

using namespace SVF;
using namespace llvm;

char IteratedDominanceFrontier::ID = 0;
//static RegisterPass<IteratedDominanceFrontier> IDF("IDF",
//		"IteratedDominanceFrontier Pass");

PTACFInfoBuilder::PTACFInfoBuilder()
{
//    Module* mod = LLVMModuleSet::getLLVMModuleSet()->getMainLLVMModule();
//    llvm::legacy::PassManager PM;
//    PM.add(new llvm::PTACFInfoBuilderPass());
//    PM.run(*mod);
}

PTACFInfoBuilder::~PTACFInfoBuilder(){
    for(FunToLoopInfoMap::iterator it = funToLoopInfoMap.begin(), eit = funToLoopInfoMap.end(); it!=eit; ++it)
    {
        if(it->second != nullptr)
        {
            delete it->second;
        }
    }
    for(FunToDTMap::iterator it = funToDTMap.begin(), eit = funToDTMap.end(); it!=eit; ++it)
    {
        if(it->second != nullptr)
        {
            delete it->second;
        }
    }
    for(FunToPostDTMap::iterator it = funToPDTMap.begin(), eit = funToPDTMap.end(); it!=eit; ++it)
    {
        if(it->second != nullptr)
        {
            delete it->second;
        }
    }
}

/// Get loop info of a function
LoopInfo* PTACFInfoBuilder::getLoopInfo(const Function* f)
{
	assert(f->isDeclaration()==false && "external function (without body) does not have a loopInfo");
    Function* fun = const_cast<Function*>(f);
    FunToLoopInfoMap::iterator it = funToLoopInfoMap.find(fun);
    if(it==funToLoopInfoMap.end())
    {
        DominatorTree* dt = new DominatorTree(*fun);
        LoopInfo* loopInfo = new LoopInfo(*dt);
        funToLoopInfoMap[fun] = loopInfo;
        return loopInfo;
    }
    else
        return it->second;
}

/// Get post dominator tree of a function
PostDominatorTree* PTACFInfoBuilder::getPostDT(const Function* f)
{
	assert(f->isDeclaration()==false && "external function (without body) does not have a PostDominatorTree");

    Function* fun = const_cast<Function*>(f);
	if(f->isDeclaration())
		return nullptr;
    FunToPostDTMap::iterator it = funToPDTMap.find(fun);
    if(it==funToPDTMap.end())
    {
        PostDominatorTree * PDT = new PostDominatorTree(*fun);
        funToPDTMap[fun] = PDT;
        return PDT;
    }
    else
        return it->second;
}

/// Get dominator tree of a function
DominatorTree* PTACFInfoBuilder::getDT(const Function* f)
{
    Function* fun = const_cast<Function*>(f);
    FunToDTMap::iterator it = funToDTMap.find(fun);
    if(it==funToDTMap.end())
    {
        DominatorTree* dt = new DominatorTree(*fun);
        funToDTMap[fun] = dt;
        return dt;
    }
    else
        return it->second;
}


void IteratedDominanceFrontier::calculate(BasicBlock * bb,
        const DominanceFrontier &DF)
{

    DomSetType worklist;

    DominanceFrontierBase<BasicBlock, false>::const_iterator it = DF.find(bb);
    assert(it != DF.end());

    worklist.insert(it->second.begin(), it->second.end());
    while (!worklist.empty())
    {
        BasicBlock *item = *worklist.begin();
        worklist.erase(worklist.begin());
        if (Frontiers[bb].find(item) == Frontiers[bb].end())
        {
            Frontiers[bb].insert(item);
            const_iterator parent = DF.find(item);
            assert(parent != DF.end());
            worklist.insert(parent->second.begin(), parent->second.end());
        }
    }
}
