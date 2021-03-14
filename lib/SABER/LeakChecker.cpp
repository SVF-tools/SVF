//===- LeakChecker.cpp -- Memory leak detector ------------------------------//
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
 * LeakChecker.cpp
 *
 *  Created on: Apr 2, 2014
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "SVF-FE/LLVMUtil.h"
#include "SABER/LeakChecker.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * Initialize sources
 */
void LeakChecker::initSrcs()
{

    PAG* pag = getPAG();
    ICFG* icfg = pag->getICFG();
    for(PAG::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
            eit = pag->getCallSiteRets().end(); it!=eit; ++it)
    {
        const RetBlockNode* cs = it->first;
        /// if this callsite return reside in a dead function then we do not care about its leaks
        /// for example instruction p = malloc is in a dead function, then program won't allocate this memory
        if(isPtrInDeadFunction(cs->getCallSite()))
            continue;

        PTACallGraph::FunctionSet callees;
        getCallgraph()->getCallees(cs->getCallBlockNode(),callees);
        for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++)
        {
            const SVFFunction* fun = *cit;
            if (isSourceLikeFun(fun))
            {
                CSWorkList worklist;
                SVFGNodeBS visited;
                worklist.push(it->first->getCallBlockNode());
                while (!worklist.empty())
                {
                    const CallBlockNode* cs = worklist.pop();
                    const RetBlockNode* retBlockNode = icfg->getRetBlockNode(cs->getCallSite());
                    const PAGNode* pagNode = pag->getCallSiteRet(retBlockNode);
                    const SVFGNode* node = getSVFG()->getDefSVFGNode(pagNode);
                    if (visited.test(node->getId()) == 0)
                        visited.set(node->getId());
                    else
                        continue;

                    CallSiteSet csSet;
                    // if this node is in an allocation wrapper, find all its call nodes
                    if (isInAWrapper(node, csSet))
                    {
                        for (CallSiteSet::iterator it = csSet.begin(), eit =
                                    csSet.end(); it != eit; ++it)
                        {
                            worklist.push(*it);
                        }
                    }
                    // otherwise, this is the source we are interested
                    else
                    {
                        // exclude sources in dead functions
                        if (isPtrInDeadFunction(cs->getCallSite()) == false)
                        {
                            addToSources(node);
                            addSrcToCSID(node, cs);
                        }
                    }
                }
            }
        }
    }

}

/*!
 * Initialize sinks
 */
void LeakChecker::initSnks()
{

    PAG* pag = getPAG();

    for(PAG::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it)
    {

        PTACallGraph::FunctionSet callees;
        getCallgraph()->getCallees(it->first,callees);
        for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++)
        {
            const SVFFunction* fun = *cit;
			if (isSinkLikeFun(fun)) {
				PAG::PAGNodeList &arglist = it->second;
				assert(!arglist.empty()	&& "no actual parameter at deallocation site?");
				/// we only choose pointer parameters among all the actual parameters
				for (PAG::PAGNodeList::const_iterator ait = arglist.begin(),
						aeit = arglist.end(); ait != aeit; ++ait) {
					const PAGNode *pagNode = *ait;
					if (pagNode->isPointer()) {
						const SVFGNode *snk = getSVFG()->getActualParmVFGNode(pagNode, it->first);
						addToSinks(snk);
					}
				}
			}
        }
    }
}


void LeakChecker::reportNeverFree(const SVFGNode* src)
{
    const CallBlockNode* cs = getSrcCSID(src);
    SVFUtil::errs() << bugMsg1("\t NeverFree :") <<  " memory allocation at : ("
                    << getSourceLoc(cs->getCallSite()) << ")\n";
}

void LeakChecker::reportPartialLeak(const SVFGNode* src)
{
    const CallBlockNode* cs = getSrcCSID(src);
    SVFUtil::errs() << bugMsg2("\t PartialLeak :") <<  " memory allocation at : ("
                    << getSourceLoc(cs->getCallSite()) << ")\n";
}

void LeakChecker::reportBug(ProgSlice* slice)
{

    if(isAllPathReachable() == false && isSomePathReachable() == false)
    {
        reportNeverFree(slice->getSource());
    }
    else if (isAllPathReachable() == false && isSomePathReachable() == true)
    {
        reportPartialLeak(slice->getSource());
        SVFUtil::errs() << "\t\t conditional free path: \n" << slice->evalFinalCond() << "\n";
        slice->annotatePaths();
    }

    if(Options::ValidateTests)
        testsValidation(slice);
}


/*!
 * Validate test cases for regression test purpose
 */
void LeakChecker::testsValidation(const ProgSlice* slice)
{
    const SVFGNode* source = slice->getSource();
    const CallBlockNode* cs = getSrcCSID(source);
    const SVFFunction* fun = getCallee(cs->getCallSite());
    if(fun==nullptr)
        return;

    validateSuccessTests(source,fun);
    validateExpectedFailureTests(source,fun);
}


void LeakChecker::validateSuccessTests(const SVFGNode* source, const SVFFunction* fun)
{

    const CallBlockNode* cs = getSrcCSID(source);

    bool success = false;

    if(fun->getName() == "SAFEMALLOC")
    {
        if(isAllPathReachable() == true && isSomePathReachable() == true)
            success = true;
    }
    else if(fun->getName() == "NFRMALLOC")
    {
        if(isAllPathReachable() == false && isSomePathReachable() == false)
            success = true;
    }
    else if(fun->getName() == "PLKMALLOC")
    {
        if(isAllPathReachable() == false && isSomePathReachable() == true)
            success = true;
    }
    else if(fun->getName() == "CLKMALLOC")
    {
        if(isAllPathReachable() == false && isSomePathReachable() == false)
            success = true;
    }
    else if(fun->getName() == "NFRLEAKFP" || fun->getName() == "PLKLEAKFP"
            || fun->getName() == "LEAKFN")
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
        outs() << sucMsg("\t SUCCESS :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << *getSrcCSID(source)->getCallSite() << "> at ("
               << getSourceLoc(cs->getCallSite()) << ")\n";
    else
    {
        SVFUtil::errs() << errMsg("\t FAILURE :") << funName << " check <src id:" << source->getId()
                        << ", cs id:" << *getSrcCSID(source)->getCallSite() << "> at ("
                        << getSourceLoc(cs->getCallSite()) << ")\n";
        assert(false && "test case failed!");
    }
}

void LeakChecker::validateExpectedFailureTests(const SVFGNode* source, const SVFFunction* fun)
{

    const CallBlockNode* cs = getSrcCSID(source);

    bool expectedFailure = false;

    if(fun->getName() == "NFRLEAKFP")
    {
        if(isAllPathReachable() == false && isSomePathReachable() == false)
            expectedFailure = true;
    }
    else if(fun->getName() == "PLKLEAKFP")
    {
        if(isAllPathReachable() == false && isSomePathReachable() == true)
            expectedFailure = true;
    }
    else if(fun->getName() == "LEAKFN")
    {
        if(isAllPathReachable() == true && isSomePathReachable() == true)
            expectedFailure = true;
    }
    else if(fun->getName() == "SAFEMALLOC" || fun->getName() == "NFRMALLOC"
            || fun->getName() == "PLKMALLOC" || fun->getName() == "CLKLEAKFN")
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
        outs() << sucMsg("\t EXPECTED-FAILURE :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << *getSrcCSID(source)->getCallSite() << "> at ("
               << getSourceLoc(cs->getCallSite()) << ")\n";
    else
    {
        SVFUtil::errs() << errMsg("\t UNEXPECTED FAILURE :") << funName
                        << " check <src id:" << source->getId()
                        << ", cs id:" << *getSrcCSID(source)->getCallSite() << "> at ("
                        << getSourceLoc(cs->getCallSite()) << ")\n";
        assert(false && "test case failed!");
    }
}
