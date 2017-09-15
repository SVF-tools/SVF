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

#include "Util/DataFlowUtil.h"
using namespace llvm;

char IteratedDominanceFrontier::ID = 0;
//static RegisterPass<IteratedDominanceFrontier> IDF("IDF",
//		"IteratedDominanceFrontier Pass");

void IteratedDominanceFrontier::calculate(llvm::BasicBlock * bb,
        const llvm::DominanceFrontier &DF) {

    DomSetType worklist;

    DominanceFrontierBase<llvm::BasicBlock>::const_iterator it = DF.find(bb);
    assert(it != DF.end());

    worklist.insert(it->second.begin(), it->second.end());
    while (!worklist.empty()) {
        BasicBlock *item = *worklist.begin();
        worklist.erase(worklist.begin());
        if (Frontiers[bb].find(item) == Frontiers[bb].end()) {
            Frontiers[bb].insert(item);
            const_iterator parent = DF.find(item);
            assert(parent != DF.end());
            worklist.insert(parent->second.begin(), parent->second.end());
        }
    }
}
