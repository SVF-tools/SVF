//===- DPDChecker.cpp -- Use After Free detector ------------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
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


#include "Util/Options.h"
#include "SABER/DpdChecker.h"

using namespace SVF;
using namespace SVFUtil;


/*!
 * Initialize sources
 */
void DpdChecker::initSrcs()
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
                Function * ll_fun = fun->getLLVMFun();
                outs() << "FOUND A DEALLOCATION FUNCTION NAMED " << ll_fun->getName().data() << " WITH ARGUMENTS :\n";

                SVFIR::SVFVarList &arglist = it->second;
                assert(!arglist.empty()	&& "no actual parameter at deallocation site?");
                /// we only choose pointer parameters among all the actual parameters
                for (SVFIR::SVFVarList::const_iterator ait = arglist.begin(),
                        aeit = arglist.end(); ait != aeit; ++ait)
                {
                    const PAGNode *pagNode = *ait;

                    NodeWorkList worklist;
                    SVFGNodeBS visited;

                    worklist.push(svfg->getDefSVFGNode(pagNode));
                    visited.set(svfg->getDefSVFGNode(pagNode)->getId());

                    NodeID freeNodeId = svfg->getDefSVFGNode(pagNode)->getICFGNode()->getId();

                    while (! worklist.empty()) {
                        const SVFGNode* svfgNode = worklist.pop();
                        outs() << "Node Popped : " << svfgNode << "\n";

                        for(auto EdgeIt = svfgNode->InEdgeBegin(), EndEdgeIt = svfgNode->InEdgeEnd() ; EdgeIt != EndEdgeIt ; EdgeIt++){

                            const VFGEdge* edge = *EdgeIt;

                            // outs() << ;
                            const SVFGNode* dstNode = edge->getSrcNode();
                            if (visited.test(dstNode->getId()) == 0) {
                                outs() << "Node Added : " << dstNode << "\n";
                                visited.set(dstNode->getId());

                                if (dstNode->getNodeKind() == SVF::VFGNode::VFGNodeK::Store && dstNode->getICFGNode()->getId() < freeNodeId)
                                {
                                    outs() << "SETTING SOURCE : " << dstNode << "\n";
                                    // addToSinks(dstNode);
                                    addToSources(dstNode);
                                    addSrcToCSID(dstNode, it->first);
                                }
                                
                                

                                worklist.push(dstNode);
                            }
                            else
                                continue;
                        }
                    }

                    // outs() << pagNode->getValueName() << "WHICH IS ";

                    // if (pagNode->isPointer()) {
                    //   outs() << "POINTER";

                    // //   const SVFGNode *src = getSVFG()->getActualParmVFGNode(pagNode, it->first);
                    //   const SVFGNode* src = svfg->getDefSVFGNode(pagNode);
                    //   addToSources(src);
                    //   addSrcToCSID(src, it->first);
                      
                    // }else {
                    //   outs() << "NOT POINTER";
                    // }

                }
            }
        }
    }

    // for(SVFIR::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
    //         eit = pag->getCallSiteRets().end(); it!=eit; ++it)
    // {
    //     const RetICFGNode* cs = it->first;
    //     /// if this callsite return reside in a dead function then we do not care about its leaks
    //     /// for example instruction `int* p = malloc(size)` is in a dead function, then program won't allocate this memory
    //     /// for example a customized malloc `int p = malloc()` returns an integer value, then program treat it as a system malloc
    //     if(SymbolTableInfo::isPtrInUncalledFunction(cs->getCallSite()) || !cs->getCallSite()->getType()->isPointerTy())
    //         continue;
    //
    //     PTACallGraph::FunctionSet callees;
    //     getCallgraph()->getCallees(cs->getCallICFGNode(),callees);
    //     for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++)
    //     {
    //         const SVFFunction* fun = *cit;
    //
    //         if (isSourceLikeFun(fun))
    //         {
    //             Function * ll_fun = fun->getLLVMFun();
    //             outs() << "FOUND AN ALLOCATION FUNCTION NAMED " << ll_fun->getName().data() << " WITH ARGUMENTS :\n";
    //
    //         }
    //     }
    // }

}

// void DpdChecker::initSrcs()
// {

//     SVFIR* pag = getPAG();
//     ICFG* icfg = pag->getICFG();
//     for(SVFIR::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
//             eit = pag->getCallSiteRets().end(); it!=eit; ++it)
//     {
//         const RetICFGNode* cs = it->first;
//         /// if this callsite return reside in a dead function then we do not care about its leaks
//         /// for example instruction `int* p = malloc(size)` is in a dead function, then program won't allocate this memory
//         /// for example a customized malloc `int p = malloc()` returns an integer value, then program treat it as a system malloc
//         if(SymbolTableInfo::isPtrInUncalledFunction(cs->getCallSite()) || !cs->getCallSite()->getType()->isPointerTy())
//             continue;

//         PTACallGraph::FunctionSet callees;
//         getCallgraph()->getCallees(cs->getCallICFGNode(),callees);
//         for(PTACallGraph::FunctionSet::const_iterator cit = callees.begin(), ecit = callees.end(); cit!=ecit; cit++)
//         {
//             const SVFFunction* fun = *cit;
//             if (isSourceLikeFun(fun))
//             {
//                 CSWorkList worklist;
//                 SVFGNodeBS visited;
//                 worklist.push(it->first->getCallICFGNode());
//                 while (!worklist.empty())
//                 {
//                     const CallICFGNode* cs = worklist.pop();
//                     const RetICFGNode* retBlockNode = icfg->getRetICFGNode(cs->getCallSite());
//                     const PAGNode* pagNode = pag->getCallSiteRet(retBlockNode);
//                     const SVFGNode* node = getSVFG()->getDefSVFGNode(pagNode);
//                     if (visited.test(node->getId()) == 0)
//                         visited.set(node->getId());
//                     else
//                         continue;

//                     CallSiteSet csSet;
//                     // if this node is in an allocation wrapper, find all its call nodes
//                     if (isInAWrapper(node, csSet))
//                     {
//                         for (CallSiteSet::iterator it = csSet.begin(), eit =
//                                     csSet.end(); it != eit; ++it)
//                         {
//                             worklist.push(*it);
//                         }
//                     }
//                     // otherwise, this is the source we are interested
//                     else
//                     {
//                         // exclude sources in dead functions
//                         if (SymbolTableInfo::isPtrInUncalledFunction(cs->getCallSite()) == false)
//                         {
//                             addToSources(node);
//                             addSrcToCSID(node, cs);
//                         }
//                     }
//                 }
//             }
//         }
//     }

// }

/*!
 * Initialize sinks
 */
void DpdChecker::initSnks()
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
                  // const SVFGNode* svfgNode = svfg->getDefSVFGNode(pagNode);

                  NodeWorkList worklist;
                  SVFGNodeBS visited;

                  worklist.push(svfg->getDefSVFGNode(pagNode));
                  visited.set(svfg->getDefSVFGNode(pagNode)->getId());

                  NodeID freeNodeId = svfg->getDefSVFGNode(pagNode)->getId();

                  while (! worklist.empty()) {
                      const SVFGNode* svfgNode = worklist.pop();
                      outs() << "Node Popped : " << svfgNode << "\n";

                      for(auto EdgeIt = svfgNode->InEdgeBegin(), EndEdgeIt = svfgNode->InEdgeEnd() ; EdgeIt != EndEdgeIt ; EdgeIt++){

                        const VFGEdge* edge = *EdgeIt;

                        // outs() << ;
                        const SVFGNode* dstNode = edge->getSrcNode();
                        if (visited.test(dstNode->getId()) == 0) {
                            outs() << "Node Added : " << dstNode << "\n";
                            visited.set(dstNode->getId());

                            if (dstNode->getNodeKind() == SVF::VFGNode::VFGNodeK::Load && dstNode->getId() > freeNodeId)
                            {
                                outs() << "SETTING SINK : " << dstNode << "\n";
                                addToSinks(dstNode);
                            }
                            
                            

                            worklist.push(dstNode);
                          }
                        else
                            continue;
                      }

                      for(auto EdgeIt = svfgNode->OutEdgeBegin(), EndEdgeIt = svfgNode->OutEdgeEnd() ; EdgeIt != EndEdgeIt ; EdgeIt++){

                        const VFGEdge* edge = *EdgeIt;
                        const SVFGNode* dstNode = edge->getDstNode();
                        if (visited.test(dstNode->getId()) == 0) {
                            outs() << "Node Added : " << dstNode << "\n";
                            visited.set(dstNode->getId());

                            if (dstNode->getNodeKind() == SVF::VFGNode::VFGNodeK::Load && dstNode->getId() > freeNodeId)
                            {
                                outs() << "SETTING SINK : " << dstNode << "\n";
                                addToSinks(dstNode);
                            }

                            worklist.push(dstNode);
                          }
                        else
                            continue;
                      }
                  }
              }
          }
      }
  }
}


void DpdChecker::reportAlwaysUAF(ProgSlice* slice)
{
    const CallICFGNode* cs = getSrcCSID(slice->getSource());
    for (auto SinksIt = slice->sinksBegin(), SinksEnd = slice->sinksEnd() ; SinksIt != SinksEnd ; SinksIt++) {
        const SVFGNode* sinkNode = *SinksIt;
        SVFIR* pag = getPAG();
        PAGNode* pagSinkNode = pag->getGNode(sinkNode->getId());
        SVFUtil::errs() << " memory used at : ("
                    << getSourceLoc(pagSinkNode->getValue()) << ")\n";
    }
    SVFUtil::errs() << bugMsg1("\t Always UAF :") <<  " memory freed at : ("
                    << getSourceLoc(cs->getCallSite()) << ")\n";
}

void DpdChecker::reportConditionalUAF(ProgSlice* slice)
{

    const CallICFGNode* cs = getSrcCSID(slice->getSource());
    for (auto SinksIt = slice->sinksBegin(), SinksEnd = slice->sinksEnd() ; SinksIt != SinksEnd ; SinksIt++) {
        const SVFGNode* sinkNode = *SinksIt;
        SVFIR* pag = getPAG();
        PAGNode* pagSinkNode = pag->getGNode(sinkNode->getId());
        SVFUtil::errs() << " memory used at : ("
                    << getSourceLoc(pagSinkNode->getValue()) << ")\n";
    }
    SVFUtil::errs() << bugMsg2("\t Conditional UAF :") <<  " memory freed at : ("
                    << getSourceLoc(cs->getCallSite()) << ")\n";
}

void DpdChecker::reportBug(ProgSlice* slice)
{

    if (isAllPathReachable() == false && isSomePathReachable() == true)
    {
        reportConditionalUAF(slice);
        SVFUtil::errs() << "\t\t conditional free path: \n" << slice->evalFinalCond() << "\n";
        // slice->annotatePaths();
    } else if (isAllPathReachable() == true) {
        reportAlwaysUAF(slice);
    }

    if(Options::ValidateTests)
        testsValidation(slice);
}


/*!
 * Validate test cases for regression test purpose
 */
void DpdChecker::testsValidation(const ProgSlice* slice)
{
    const SVFGNode* source = slice->getSource();
    const CallICFGNode* cs = getSrcCSID(source);
    const SVFFunction* fun = getCallee(cs->getCallSite());
    if(fun==nullptr)
        return;

    validateSuccessTests(source,fun);
    validateExpectedFailureTests(source,fun);
}


void DpdChecker::validateSuccessTests(const SVFGNode* source, const SVFFunction* fun)
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

void DpdChecker::validateExpectedFailureTests(const SVFGNode* source, const SVFFunction* fun)
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
