/* SVF - Static Value-Flow Analysis Framework 
Copyright (C) 2015 Yulei Sui
Copyright (C) 2015 Jingling Xue

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

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
