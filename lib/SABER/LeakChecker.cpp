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

#include "SABER/LeakChecker.h"
#include "Util/AnalysisUtil.h"
#include <llvm/Support/CommandLine.h>

using namespace llvm;
using namespace analysisUtil;

char LeakChecker::ID = 0;

static RegisterPass<LeakChecker> LEAKCHECKER("leak-checker",
        "Memory Leak Checker");
static cl::opt<bool> ValidateTests("valid-tests", cl::init(false),
                                   cl::desc("Validate memory leak tests"));

/*!
 * Initialize sources
 */
void LeakChecker::initSrcs() {

    PAG* pag = getPAG();

    for(PAG::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
            eit = pag->getCallSiteRets().end(); it!=eit; ++it) {
        CallSite cs = it->first;
        /// if this callsite return reside in a dead function then we do not care about its leaks
        /// for example instruction p = malloc is in a dead function, then program won't allocate this memory
        if(isPtrInDeadFunction(cs.getInstruction()))
            continue;

        const Function* fun = getCallee(cs);
        if(isSourceLikeFun(fun)) {
            CSWorkList worklist;
            SVFGNodeBS visited;
            worklist.push(it->first);
            while (!worklist.empty()) {
                CallSite cs = worklist.pop();
                const PAGNode* pagNode = pag->getCallSiteRet(cs);
                const SVFGNode* node = getSVFG()->getDefSVFGNode(pagNode);
                if(visited.test(node->getId())==0)
                    visited.set(node->getId());
                else
                    continue;

                CallSiteSet csSet;
                // if this node is in an allocation wrapper, find all its call nodes
                if(isInAWrapper(node,csSet)) {
                    for(CallSiteSet::iterator it = csSet.begin(), eit = csSet.end(); it!=eit ; ++it) {
                        worklist.push(*it);
                    }
                }
                // otherwise, this is the source we are interested
                else {
                    // exclude sources in dead functions
                    if(isPtrInDeadFunction(cs.getInstruction()) == false) {
                        addToSources(node);
                        addSrcToCSID(node,cs);
                    }
                }
            }
        }
    }

}

/*!
 * Initialize sinks
 */
void LeakChecker::initSnks() {

    PAG* pag = getPAG();

    for(PAG::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
            eit = pag->getCallSiteArgsMap().end(); it!=eit; ++it) {
        const Function* fun = getCallee(it->first);
        if(isSinkLikeFun(fun)) {
            PAG::PAGNodeList& arglist =	it->second;
            assert(!arglist.empty() && "no actual parameter at deallocation site?");
            /// we only pick the first parameter of all the actual parameters
            const SVFGNode* snk = getSVFG()->getActualParmSVFGNode(arglist.front(),it->first);
            addToSinks(snk);
        }
    }
}

/*!
 * determine whether a SVFGNode n is in a allocation wrapper function,
 * if so, return all SVFGNodes which receive the value of node n
 */
bool LeakChecker::isInAWrapper(const SVFGNode* src, CallSiteSet& csIdSet) {

    bool reachFunExit = false;

    WorkList worklist;
    worklist.push(src);
    SVFGNodeBS visited;
    while (!worklist.empty()) {
        const SVFGNode* node  = worklist.pop();

        if(visited.test(node->getId())==0)
            visited.set(node->getId());
        else
            continue;

        for (SVFGNode::const_iterator it = node->OutEdgeBegin(), eit =
                    node->OutEdgeEnd(); it != eit; ++it) {
            const SVFGEdge* edge = (*it);
            assert(edge->isDirectVFGEdge() && "the edge should always be direct VF");
            // if this is a call edge
            if(edge->isCallDirectVFGEdge()) {
                return false;
            }
            // if this is a return edge
            else if(edge->isRetDirectVFGEdge()) {
                reachFunExit = true;
                csIdSet.insert(getSVFG()->getCallSite(cast<RetDirSVFGEdge>(edge)->getCallSiteId()));
            }
            // if this is an intra edge
            else {
                const SVFGNode* succ = edge->getDstNode();
                if (isa<CopySVFGNode>(succ) || isa<GepSVFGNode>(succ)
                        || isa<PHISVFGNode>(succ) || isa<FormalRetSVFGNode>(succ)
                        || isa<ActualRetSVFGNode>(succ)) {
                    worklist.push(succ);
                }
                else {
                    return false;
                }
            }
        }
    }
    if(reachFunExit)
        return true;
    else
        return false;
}


void LeakChecker::reportNeverFree(const SVFGNode* src) {
    CallSite cs = getSrcCSID(src);
    errs() << bugMsg1("\t NeverFree :") <<  " memory allocation at : ("
           << getSourceLoc(cs.getInstruction()) << ")\n";
}

void LeakChecker::reportPartialLeak(const SVFGNode* src) {
    CallSite cs = getSrcCSID(src);
    errs() << bugMsg2("\t PartialLeak :") <<  " memory allocation at : ("
           << getSourceLoc(cs.getInstruction()) << ")\n";
}

void LeakChecker::reportBug(ProgSlice* slice) {

    if(isAllPathReachable() == false && isSomePathReachable() == false) {
        reportNeverFree(slice->getSource());
    }
    else if (isAllPathReachable() == false && isSomePathReachable() == true) {
        reportPartialLeak(slice->getSource());
        errs() << "\t\t conditional free path: \n" << slice->evalFinalCond() << "\n";
        slice->annotatePaths();
    }

    if(ValidateTests)
        testsValidation(slice);
}


/*!
 * Validate test cases for regression test purpose
 */
void LeakChecker::testsValidation(const ProgSlice* slice) {
    const SVFGNode* source = slice->getSource();
    CallSite cs = getSrcCSID(source);
    const Function* fun = getCallee(cs);
    if(fun==NULL)
        return;

    validateSuccessTests(source,fun);
    validateExpectedFailureTests(source,fun);
}


void LeakChecker::validateSuccessTests(const SVFGNode* source, const Function* fun) {

    CallSite cs = getSrcCSID(source);

    bool success = false;

    if(fun->getName() == "SAFEMALLOC") {
        if(isAllPathReachable() == true && isSomePathReachable() == true)
            success = true;
    }
    else if(fun->getName() == "NFRMALLOC") {
        if(isAllPathReachable() == false && isSomePathReachable() == false)
            success = true;
    }
    else if(fun->getName() == "PLKMALLOC") {
        if(isAllPathReachable() == false && isSomePathReachable() == true)
            success = true;
    }
    else if(fun->getName() == "CLKMALLOC") {
        if(isAllPathReachable() == false && isSomePathReachable() == false)
            success = true;
    }
    else if(fun->getName() == "NFRLEAKFP" || fun->getName() == "PLKLEAKFP"
            || fun->getName() == "LEAKFN") {
        return;
    }
    else {
        wrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }

    std::string funName = source->getBB()->getParent()->getName();

    if (success)
        outs() << sucMsg("\t SUCCESS :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << *getSrcCSID(source).getInstruction() << "> at ("
               << getSourceLoc(cs.getInstruction()) << ")\n";
    else
        errs() << errMsg("\t FAILURE :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << *getSrcCSID(source).getInstruction() << "> at ("
               << getSourceLoc(cs.getInstruction()) << ")\n";
}

void LeakChecker::validateExpectedFailureTests(const SVFGNode* source, const Function* fun) {

    CallSite cs = getSrcCSID(source);

    bool expectedFailure = false;

    if(fun->getName() == "NFRLEAKFP") {
        if(isAllPathReachable() == false && isSomePathReachable() == false)
            expectedFailure = true;
    }
    else if(fun->getName() == "PLKLEAKFP") {
        if(isAllPathReachable() == false && isSomePathReachable() == true)
            expectedFailure = true;
    }
    else if(fun->getName() == "LEAKFN") {
        if(isAllPathReachable() == true && isSomePathReachable() == true)
            expectedFailure = true;
    }
    else if(fun->getName() == "SAFEMALLOC" || fun->getName() == "NFRMALLOC"
            || fun->getName() == "PLKMALLOC" || fun->getName() == "CLKLEAKFN") {
        return;
    }
    else {
        wrnMsg("\t can not validate, check function not found, please put it at the right place!!");
        return;
    }

    std::string funName = source->getBB()->getParent()->getName();

    if (expectedFailure)
        outs() << sucMsg("\t EXPECTED FAIL :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << *getSrcCSID(source).getInstruction() << "> at ("
               << getSourceLoc(cs.getInstruction()) << ")\n";
    else
        errs() << errMsg("\t UNEXPECTED FAIL :") << funName << " check <src id:" << source->getId()
               << ", cs id:" << *getSrcCSID(source).getInstruction() << "> at ("
               << getSourceLoc(cs.getInstruction()) << ")\n";
}
