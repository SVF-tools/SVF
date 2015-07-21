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

