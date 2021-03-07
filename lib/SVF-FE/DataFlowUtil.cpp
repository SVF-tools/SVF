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
    for(auto & it : funToLoopInfoMap)
    {
        if(it.second != nullptr)
        {
            delete it.second;
        }
    }

    for(auto & it : funToDTMap)
    {
        if(it.second != nullptr)
        {
            delete it.second;
        }
    }

    for(auto & it : funToPDTMap)
    {
        if(it.second != nullptr)
        {
            delete it.second;
        }
    }
}

/// Get loop info of a function
LoopInfo* PTACFInfoBuilder::getLoopInfo(const Function* f)
{
	assert(f->isDeclaration()==false && "external function (without body) does not have a loopInfo");
    auto* fun = const_cast<Function*>(f);
    auto it = funToLoopInfoMap.find(fun);
    if(it==funToLoopInfoMap.end())
    {
        auto* dt = new DominatorTree(*fun);
        auto* loopInfo = new LoopInfo(*dt);
        funToLoopInfoMap[fun] = loopInfo;
        return loopInfo;
    }

    return it->second;
}

/// Get post dominator tree of a function
PostDominatorTree* PTACFInfoBuilder::getPostDT(const Function* f)
{
	assert(f->isDeclaration()==false && "external function (without body) does not have a PostDominatorTree");

    auto* fun = const_cast<Function*>(f);
	if(f->isDeclaration())
		return nullptr;
    auto it = funToPDTMap.find(fun);
    if(it==funToPDTMap.end())
    {
        auto * PDT = new PostDominatorTree(*fun);
        funToPDTMap[fun] = PDT;
        return PDT;
    }

    return it->second;
}

/// Get dominator tree of a function
DominatorTree* PTACFInfoBuilder::getDT(const Function* f)
{
    auto* fun = const_cast<Function*>(f);
    auto it = funToDTMap.find(fun);
    if(it==funToDTMap.end())
    {
        auto* dt = new DominatorTree(*fun);
        funToDTMap[fun] = dt;
        return dt;
    }

    return it->second;
}


void IteratedDominanceFrontier::calculate(BasicBlock * bb,
        const DominanceFrontier &DF)
{

    DomSetType worklist;

    auto it = DF.find(bb);
    assert(it != DF.end());

    worklist.insert(it->second.begin(), it->second.end());
    while (!worklist.empty())
    {
        BasicBlock *item = *worklist.begin();
        worklist.erase(worklist.begin());
        if (Frontiers[bb].find(item) == Frontiers[bb].end())
        {
            Frontiers[bb].insert(item);
            auto parent = DF.find(item);
            assert(parent != DF.end());
            worklist.insert(parent->second.begin(), parent->second.end());
        }
    }
}
