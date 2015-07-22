//===- DoubleFreeChecker.cpp -- Detecting double-free errors------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * DoubleFreeChecker.cpp
 *
 *  Created on: Apr 24, 2014
 *      Author: Yulei Sui
 */


#include "SABER/DoubleFreeChecker.h"
#include "Util/AnalysisUtil.h"
#include <llvm/Support/CommandLine.h>

using namespace llvm;
using namespace analysisUtil;

char DoubleFreeChecker::ID = 0;

static RegisterPass<DoubleFreeChecker> DFREECHECKER("dfree-checker",
        "File Open/Close Checker");

void DoubleFreeChecker::reportBug(ProgSlice* slice) {

    if(isSatisfiableForPairs(slice) == false) {
        const SVFGNode* src = slice->getSource();
        CallSite cs = getSrcCSID(src);
        errs() << bugMsg2("\t Double Free :") <<  " memory allocation at : ("
               << getSourceLoc(cs.getInstruction()) << ")\n";
        errs() << "\t\t double free path: \n" << slice->evalFinalCond() << "\n";
        slice->annotatePaths();
    }
}

