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
using namespace LLVMUtil;


/*!
 * Initialize sources
 */
void LeakChecker::initSrcs()
{

    SVFIR* pag = getPAG();
    ICFG* icfg = pag->getICFG();
    for(SVFIR::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
            eit = pag->getCallSiteRets().end(); it!=eit; ++it)
    {
        const RetICFGNode* cs = it->first;
        /// if this callsite return reside in a dead function then we do not care about its leaks
        /// for example instruction `int* p = malloc(size)` is in a dead function, then program won't allocate this memory
        /// for example a customized malloc `int p = malloc()` returns an integer value, then program treat it as a system malloc
        if(isPtrInDeadFunction(cs->getCallSite()) || !cs->getCallSite()->getType()->isPointerTy())
            continue;

        PTACallGraph::FunctionSet callees;
        getCallgraph()->getCallees(cs->getCallICFGNode(),callees);
        for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++)
        {
            const SVFFunction* fun = *cit;
            if (isSourceLikeFun(fun))
            {
                CSWorkList worklist;
                SVFGNodeBS visited;
                worklist.push(it->first->getCallICFGNode());
                while (!worklist.empty())
                {
                    const CallICFGNode* cs = worklist.pop();
                    const RetICFGNode* retBlockNode = icfg->getRetICFGNode(cs->getCallSite());
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

    SVFIR* pag = getPAG();

    for(SVFIR::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it)
    {

        PTACallGraph::FunctionSet callees;
        getCallgraph()->getCallees(it->first,callees);
        for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++)
        {
            const SVFFunction* fun = *cit;
            if (isSinkLikeFun(fun))
            {
                SVFIR::SVFVarList &arglist = it->second;
                assert(!arglist.empty()	&& "no actual parameter at deallocation site?");
                /// we only choose pointer parameters among all the actual parameters
                for (SVFIR::SVFVarList::const_iterator ait = arglist.begin(),
                        aeit = arglist.end(); ait != aeit; ++ait)
                {
                    const PAGNode *pagNode = *ait;
                    if (pagNode->isPointer())
                    {
                        const SVFGNode *snk = getSVFG()->getActualParmVFGNode(pagNode, it->first);
                        addToSinks(snk);

                        // For any multi-level pointer e.g., XFree(void** pagNode) that passed into a ExtAPI::EFT_FREE_MULTILEVEL function (e.g., XFree),
                        // we will add the DstNode of a load edge, i.e., dummy = *pagNode
                        SVFStmt::SVFStmtSetTy& loads = const_cast<PAGNode*>(pagNode)->getOutgoingEdges(SVFStmt::Load);
                        for(const SVFStmt* ld : loads)
                        {
                            if(SVFUtil::isa<DummyValVar>(ld->getDstNode()))
                                addToSinks(getSVFG()->getStmtVFGNode(ld));
                        }
                    }
                }
            }
        }
    }
}


void LeakChecker::reportNeverFree(const SVFGNode* src)
{
    const CallICFGNode* cs = getSrcCSID(src);
    SVFUtil::errs() << bugMsg1("\t NeverFree :") <<  " memory allocation at : ("
                    << getSourceLoc(cs->getCallSite()) << ")\n";
}

void LeakChecker::reportPartialLeak(const SVFGNode* src)
{
    const CallICFGNode* cs = getSrcCSID(src);
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
    const CallICFGNode* cs = getSrcCSID(source);
    const SVFFunction* fun = getCallee(cs->getCallSite());
    if(fun==nullptr)
        return;

    validateSuccessTests(source,fun);
    validateExpectedFailureTests(source,fun);
}


void LeakChecker::validateSuccessTests(const SVFGNode* source, const SVFFunction* fun)
{

    const CallICFGNode* cs = getSrcCSID(source);

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
    {
        outs() << sucMsg("\t SUCCESS :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << SVFUtil::value2String(getSrcCSID(source)->getCallSite()) << "> at ("
               << getSourceLoc(cs->getCallSite()) << ")\n";
    }
    else
    {
        SVFUtil::errs() << errMsg("\t FAILURE :") << funName << " check <src id:" << source->getId()
                        << ", cs id:" << SVFUtil::value2String(getSrcCSID(source)->getCallSite()) << "> at ("
                        << getSourceLoc(cs->getCallSite()) << ")\n";
        assert(false && "test case failed!");
    }
}

void LeakChecker::validateExpectedFailureTests(const SVFGNode* source, const SVFFunction* fun)
{

    const CallICFGNode* cs = getSrcCSID(source);

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
    {
        outs() << sucMsg("\t EXPECTED-FAILURE :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << SVFUtil::value2String(getSrcCSID(source)->getCallSite()) << "> at ("
               << getSourceLoc(cs->getCallSite()) << ")\n";
    }
    else
    {
        SVFUtil::errs() << errMsg("\t UNEXPECTED FAILURE :") << funName
                        << " check <src id:" << source->getId()
                        << ", cs id:" << SVFUtil::value2String(getSrcCSID(source)->getCallSite()) << "> at ("
                        << getSourceLoc(cs->getCallSite()) << ")\n";
        assert(false && "test case failed!");
    }
}
