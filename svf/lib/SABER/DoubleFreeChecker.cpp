//===- DoubleFreeChecker.cpp -- Detecting double-free errors------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
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
#include "Util/SVFUtil.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

void DoubleFreeChecker::reportBug(ProgSlice* slice)
{

    if(slice->isSatisfiableForPairs() == false)
    {
        GenericBug::EventStack eventStack;
        slice->evalFinalCond2Event(eventStack);
        eventStack.push_back(
            SVFBugEvent(SVFBugEvent::SourceInst, getSrcCSID(slice->getSource())->getCallSite()));
        report.addSaberBug(GenericBug::DOUBLEFREE, eventStack);
    }
    if(Options::ValidateTests())
        testsValidation(slice);
}



void DoubleFreeChecker::testsValidation(ProgSlice *slice)
{
    const SVFGNode* source = slice->getSource();
    const CallICFGNode* cs = getSrcCSID(source);
    const SVFFunction* fun = getCallee(cs->getCallSite());
    if(fun==nullptr)
        return;
    validateSuccessTests(slice,fun);
    validateExpectedFailureTests(slice,fun);
}

void DoubleFreeChecker::validateSuccessTests(ProgSlice *slice, const SVFFunction *fun)
{
    const SVFGNode* source = slice->getSource();
    const CallICFGNode* cs = getSrcCSID(source);

    bool success = false;

    if(fun->getName() == "SAFEMALLOC")
    {
        if(slice->isSatisfiableForPairs() == true)
            success = true;
    }
    else if(fun->getName() == "DOUBLEFREEMALLOC")
    {
        if(slice->isSatisfiableForPairs() == false)
            success = true;
    }
    else if(fun->getName() == "DOUBLEFREEMALLOCFN" || fun->getName() == "SAFEMALLOCFP")
    {
        return;
    }
    else
    {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }

    std::string funName = source->getFun()->getName();

    if (success)
    {
        outs() << sucMsg("\t SUCCESS :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << getSrcCSID(source)->getCallSite()->toString() << "> at ("
               << cs->getCallSite()->getSourceLoc() << ")\n";
        outs() << "\t\t double free path: \n" << slice->evalFinalCond() << "\n";
    }
    else
    {
        SVFUtil::errs() << errMsg("\t FAILURE :") << funName << " check <src id:" << source->getId()
                        << ", cs id:" <<getSrcCSID(source)->getCallSite()->toString() << "> at ("
                        << cs->getCallSite()->getSourceLoc() << ")\n";
        SVFUtil::errs() << "\t\t double free path: \n" << slice->evalFinalCond() << "\n";
        assert(false && "test case failed!");
    }
}

void DoubleFreeChecker::validateExpectedFailureTests(ProgSlice *slice, const SVFFunction *fun)
{
    const SVFGNode* source = slice->getSource();
    const CallICFGNode* cs = getSrcCSID(source);

    bool expectedFailure = false;
    /// output safe but should be double free
    if(fun->getName() == "DOUBLEFREEMALLOCFN")
    {
        if(slice->isSatisfiableForPairs() == true)
            expectedFailure = true;
    } /// output double free but should be safe
    else if(fun->getName() == "SAFEMALLOCFP")
    {
        if(slice->isSatisfiableForPairs() == false)
            expectedFailure = true;
    }
    else if(fun->getName() == "SAFEMALLOC" || fun->getName() == "DOUBLEFREEMALLOC")
    {
        return;
    }
    else
    {
        writeWrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }

    std::string funName = source->getFun()->getName();

    if (expectedFailure)
    {
        outs() << sucMsg("\t EXPECTED-FAILURE :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << getSrcCSID(source)->getCallSite()->toString() << "> at ("
               << cs->getCallSite()->getSourceLoc() << ")\n";
        outs() << "\t\t double free path: \n" << slice->evalFinalCond() << "\n";
    }
    else
    {
        SVFUtil::errs() << errMsg("\t UNEXPECTED FAILURE :") << funName
                        << " check <src id:" << source->getId()
                        << ", cs id:" << getSrcCSID(source)->getCallSite()->toString() << "> at ("
                        << cs->getCallSite()->getSourceLoc() << ")\n";
        SVFUtil::errs() << "\t\t double free path: \n" << slice->evalFinalCond() << "\n";
        assert(false && "test case failed!");
    }
}
