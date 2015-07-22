//===- FileChecker.cpp -- Checking incorrect file-open close errors-----------//
//
//                     SVF: Static Value-Flow Analysis
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/*
 * FileChecker.cpp
 *
 *  Created on: Apr 24, 2014
 *      Author: Yulei Sui
 */

#include "SABER/FileChecker.h"
#include "Util/AnalysisUtil.h"
#include <llvm/Support/CommandLine.h>

using namespace llvm;
using namespace analysisUtil;

char FileChecker::ID = 0;

static RegisterPass<FileChecker> FILECHECKER("file-checker",
        "File Open/Close Checker");



void FileChecker::reportNeverClose(const SVFGNode* src) {
    CallSite cs = getSrcCSID(src);
    errs() << bugMsg1("\t FileNeverClose :") <<  " file open location at : ("
           << getSourceLoc(cs.getInstruction()) << ")\n";
}

void FileChecker::reportPartialClose(const SVFGNode* src) {
    CallSite cs = getSrcCSID(src);
    errs() << bugMsg2("\t PartialFileClose :") <<  " file open location at : ("
           << getSourceLoc(cs.getInstruction()) << ")\n";
}

void FileChecker::reportBug(ProgSlice* slice) {

    if(isAllPathReachable() == false && isSomePathReachable() == false) {
        reportNeverClose(slice->getSource());
    }
    else if (isAllPathReachable() == false && isSomePathReachable() == true) {
        reportPartialClose(slice->getSource());
        errs() << "\t\t conditional file close path: \n" << slice->evalFinalCond() << "\n";
        slice->annotatePaths();
    }

}
