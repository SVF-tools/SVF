//===- SVFGBuilder.cpp -- SVFG builder----------------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
    releaseMemory(graph);

    return false;
}


