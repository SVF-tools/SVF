//===- FileChecker.cpp -- Checking incorrect file-open close errors-----------//
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
