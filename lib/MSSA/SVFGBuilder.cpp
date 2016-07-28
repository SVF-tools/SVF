//===- SVFGBuilder.cpp -- SVFG builder----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * SVFGBuilder.cpp
 *
 *  Created on: Apr 15, 2014
 *      Author: Yulei Sui
 */
#include "MSSA/MemSSA.h"
#include "MSSA/SVFG.h"
#include "MSSA/SVFGBuilder.h"
#include "WPA/Andersen.h"

using namespace llvm;
using namespace analysisUtil;

static cl::opt<bool> IndCallWithPreAnalysis("svfgIndCallPreAna", cl::init(false),
        cl::desc("Update Indirect Calls for SVFG using pre-analysis"));

/*!
 * Create SVFG
 */
void SVFGBuilder::createSVFG(MemSSA* mssa, SVFG* graph) {
    svfg = graph;
    svfg->buildSVFG(mssa);
    if(mssa->getPTA()->printStat())
        svfg->performStat();
    svfg->dump("FS_SVFG");
}

/*!
 * Release memory
 */
void SVFGBuilder::releaseMemory(SVFG* vfg) {
    vfg->clearMSSA();
}

/*!
 * We start the pass here
 */
bool SVFGBuilder::build(SVFG* graph,BVDataPTAImpl* pta) {

    MemSSA mssa(pta);

    DBOUT(DGENERAL, outs() << pasMsg("Build Memory SSA \n"));

    DominatorTree dt;
    MemSSADF df;

    for (llvm::Module::iterator iter = pta->getModule()->begin(), eiter = pta->getModule()->end();
            iter != eiter; ++iter) {

        llvm::Function& fun = *iter;
        if (analysisUtil::isExtCall(&fun))
            continue;

        dt.recalculate(fun);
        df.runOnDT(dt);

        mssa.buildMemSSA(fun, &df, &dt);
    }

    mssa.performStat();
    mssa.dumpMSSA();

    DBOUT(DGENERAL, outs() << pasMsg("Build Sparse Value-Flow Graph \n"));

    createSVFG(&mssa, graph);

    if(IndCallWithPreAnalysis)
        updateCallGraph(mssa.getPTA());

    releaseMemory(graph);

    return false;
}



/// Update call graph using pre-analysis results
void SVFGBuilder::updateCallGraph(PointerAnalysis* pta)
{
    CallEdgeMap::const_iterator iter = pta->getIndCallMap().begin();
    CallEdgeMap::const_iterator eiter = pta->getIndCallMap().end();
    for (; iter != eiter; iter++) {
        llvm::CallSite newcs = iter->first;
        const FunctionSet & functions = iter->second;
        for (FunctionSet::const_iterator func_iter = functions.begin(); func_iter != functions.end(); func_iter++) {
            const llvm::Function * func = *func_iter;
            svfg->connectCallerAndCallee(newcs, func, vfEdgesAtIndCallSite);
        }
    }
}
