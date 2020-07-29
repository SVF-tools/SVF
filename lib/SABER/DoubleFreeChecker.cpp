//===- DoubleFreeChecker.cpp -- Detecting double-free errors------------------//
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
 * DoubleFreeChecker.cpp
 *
 *  Created on: Apr 24, 2014
 *      Author: Yulei Sui
 */

#include "SABER/DoubleFreeChecker.h"

using namespace SVF;
using namespace SVFUtil;

void DoubleFreeChecker::reportBug(ProgSlice* slice)
{

    if(slice->isSatisfiableForPairs() == false)
    {
        const SVFGNode* src = slice->getSource();
        const CallBlockNode* cs = getSrcCSID(src);
        SVFUtil::errs() << bugMsg2("\t Double Free :") <<  " memory allocation at : ("
                        << getSourceLoc(cs->getCallSite()) << ")\n";
        SVFUtil::errs() << "\t\t double free path: \n" << slice->evalFinalCond() << "\n";
        slice->annotatePaths();
    }
}

