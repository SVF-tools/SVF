//===- DataFlowUtil.cpp -- Helper class for data-flow analysis----------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
