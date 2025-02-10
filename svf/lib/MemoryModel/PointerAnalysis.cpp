//===- PointerAnalysis.cpp -- Base class of pointer analyses------------------//
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
 * PointerAnalysis.cpp
 *
 *  Created on: May 14, 2013
 *      Author: Yulei Sui
 */

#include "Util/Options.h"
#include "Util/SVFUtil.h"

#include "MemoryModel/PointerAnalysisImpl.h"
#include "SVFIR/PAGBuilderFromFile.h"
#include "Util/PTAStat.h"
#include "Graphs/ThreadCallGraph.h"
#include "Graphs/ICFG.h"
#include "Graphs/CallGraph.h"
#include "Util/CallGraphBuilder.h"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace SVF;
using namespace SVFUtil;


SVFIR* PointerAnalysis::pag = nullptr;

const std::string PointerAnalysis::aliasTestMayAlias            = "MAYALIAS";
const std::string PointerAnalysis::aliasTestMayAliasMangled     = "_Z8MAYALIASPvS_";
const std::string PointerAnalysis::aliasTestNoAlias             = "NOALIAS";
const std::string PointerAnalysis::aliasTestNoAliasMangled      = "_Z7NOALIASPvS_";
const std::string PointerAnalysis::aliasTestPartialAlias        = "PARTIALALIAS";
const std::string PointerAnalysis::aliasTestPartialAliasMangled = "_Z12PARTIALALIASPvS_";
const std::string PointerAnalysis::aliasTestMustAlias           = "MUSTALIAS";
const std::string PointerAnalysis::aliasTestMustAliasMangled    = "_Z9MUSTALIASPvS_";
const std::string PointerAnalysis::aliasTestFailMayAlias        = "EXPECTEDFAIL_MAYALIAS";
const std::string PointerAnalysis::aliasTestFailMayAliasMangled = "_Z21EXPECTEDFAIL_MAYALIASPvS_";
const std::string PointerAnalysis::aliasTestFailNoAlias         = "EXPECTEDFAIL_NOALIAS";
const std::string PointerAnalysis::aliasTestFailNoAliasMangled  = "_Z20EXPECTEDFAIL_NOALIASPvS_";

/*!
 * Constructor
 */
PointerAnalysis::PointerAnalysis(SVFIR *p, PTATY ty, bool alias_check) : ptaTy(ty), stat(nullptr), callgraph(nullptr),
    callGraphSCC(nullptr), icfg(nullptr),
    chgraph(nullptr)
{
    pag = p;
    OnTheFlyIterBudgetForStat = Options::StatBudget();
    print_stat = Options::PStat();
    ptaImplTy = BaseImpl;
    alias_validation = (alias_check && Options::EnableAliasCheck());
}

/*!
 * Destructor
 */
PointerAnalysis::~PointerAnalysis()
{
    destroy();
    // do not delete the SVFIR for now
    //delete pag;
}


void PointerAnalysis::destroy()
{
    delete callgraph;
    callgraph = nullptr;

    delete callGraphSCC;
    callGraphSCC = nullptr;

    delete stat;
    stat = nullptr;
}

/*!
 * Initialization of pointer analysis
 */
void PointerAnalysis::initialize()
{
    assert(pag && "SVFIR has not been built!");

    chgraph = pag->getCHG();

    /// initialise pta call graph for every pointer analysis instance
    if(Options::EnableThreadCallGraph())
    {
        CallGraphBuilder bd;
        callgraph = bd.buildThreadCallGraph();
    }
    else
    {
        CallGraphBuilder bd;
        callgraph = bd.buildPTACallGraph();
    }
    callGraphSCCDetection();

    // dump callgraph
    if (Options::CallGraphDotGraph())
        getCallGraph()->dump("callgraph_initial");
}


/*!
 * Return TRUE if this node is a local variable of recursive function.
 */
bool PointerAnalysis::isLocalVarInRecursiveFun(NodeID id) const
{
    const BaseObjVar* baseObjVar = pag->getBaseObject(id);
    assert(baseObjVar && "base object not found!!");
    if(SVFUtil::isa<StackObjVar>(baseObjVar))
    {
        if(const FunObjVar* svffun = pag->getGNode(id)->getFunction())
        {
            return callGraphSCC->isInCycle(getCallGraph()->getCallGraphNode(svffun)->getId());
        }
    }
    return false;
}

/*!
 * Reset field sensitivity
 */
void PointerAnalysis::resetObjFieldSensitive()
{
    for (SVFIR::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter)
    {
        if(ObjVar* node = SVFUtil::dyn_cast<ObjVar>(nIter->second))
            const_cast<BaseObjVar*>(pag->getBaseObject(node->getId()))->setFieldSensitive();
    }
}

/*
 * Dump statistics
 */

void PointerAnalysis::dumpStat()
{

    if(print_stat && stat)
    {
        stat->performStat();
    }
}

/*!
 * Finalize the analysis after solving
 * Given the alias results, verify whether it is correct or not using alias check functions
 */
void PointerAnalysis::finalize()
{

    /// Print statistics
    dumpStat();

    /// Dump results
    if (Options::PTSPrint())
    {
        dumpTopLevelPtsTo();
        //dumpAllPts();
        //dumpCPts();
    }

    if (Options::TypePrint())
        dumpAllTypes();

    if(Options::PTSAllPrint())
        dumpAllPts();

    if (Options::FuncPointerPrint())
        printIndCSTargets();

    getCallGraph()->verifyCallGraph();

    if (Options::CallGraphDotGraph())
        getCallGraph()->dump("callgraph_final");

    if(!pag->isBuiltFromFile() && alias_validation)
        validateTests();

    if (!Options::UsePreCompFieldSensitive())
        resetObjFieldSensitive();
}

/*!
 * Validate test cases
 */
void PointerAnalysis::validateTests()
{
    validateSuccessTests(aliasTestMayAlias);
    validateSuccessTests(aliasTestNoAlias);
    validateSuccessTests(aliasTestMustAlias);
    validateSuccessTests(aliasTestPartialAlias);
    validateExpectedFailureTests(aliasTestFailMayAlias);
    validateExpectedFailureTests(aliasTestFailNoAlias);

    validateSuccessTests(aliasTestMayAliasMangled);
    validateSuccessTests(aliasTestNoAliasMangled);
    validateSuccessTests(aliasTestMustAliasMangled);
    validateSuccessTests(aliasTestPartialAliasMangled);
    validateExpectedFailureTests(aliasTestFailMayAliasMangled);
    validateExpectedFailureTests(aliasTestFailNoAliasMangled);
}


void PointerAnalysis::dumpAllTypes()
{
    for (OrderedNodeSet::iterator nIter = this->getAllValidPtrs().begin();
            nIter != this->getAllValidPtrs().end(); ++nIter)
    {
        const PAGNode* node = getPAG()->getGNode(*nIter);
        if (SVFUtil::isa<DummyObjVar, DummyValVar>(node))
            continue;

        outs() << "##<" << node->getName() << "> ";
        outs() << "Source Loc: " << node->getSourceLoc();
        outs() << "\nNodeID " << node->getId() << "\n";

        const SVFType* type = node->getType();
        pag->printFlattenFields(type);
    }
}

/*!
 * Dump points-to of top-level pointers (ValVar)
 */
void PointerAnalysis::dumpPts(NodeID ptr, const PointsTo& pts)
{

    const PAGNode* node = pag->getGNode(ptr);
    /// print the points-to set of node which has the maximum pts size.
    if (SVFUtil::isa<DummyObjVar> (node))
    {
        outs() << "##<Dummy Obj > id:" << node->getId();
    }
    else if (!SVFUtil::isa<DummyValVar>(node) && !SVFIR::pagReadFromTXT())
    {
        outs() << "##<" << node->getName() << "> ";
        outs() << "Source Loc: " << node->getSourceLoc();
    }
    outs() << "\nPtr " << node->getId() << " ";

    if (pts.empty())
    {
        outs() << "\t\tPointsTo: {empty}\n\n";
    }
    else
    {
        outs() << "\t\tPointsTo: { ";
        for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it != eit;
                ++it)
            outs() << *it << " ";
        outs() << "}\n\n";
    }

    outs() << "";

    for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it != eit; ++it)
    {
        const PAGNode* node = pag->getGNode(*it);
        if(SVFUtil::isa<ObjVar>(node) == false)
            continue;
        NodeID ptd = node->getId();
        outs() << "!!Target NodeID " << ptd << "\t [";
        const PAGNode* pagNode = pag->getGNode(ptd);
        if (SVFUtil::isa<DummyValVar>(node))
            outs() << "DummyVal\n";
        else if (SVFUtil::isa<DummyObjVar>(node))
            outs() << "Dummy Obj id: " << node->getId() << "]\n";
        else
        {
            if (!SVFIR::pagReadFromTXT())
            {
                outs() << "<" << pagNode->getName() << "> ";
                outs() << "Source Loc: "
                       << pagNode->getSourceLoc() << "] \n";
            }
        }
    }
}

/*!
 * Print indirect call targets at an indirect callsite
 */
void PointerAnalysis::printIndCSTargets(const CallICFGNode* cs, const FunctionSet& targets)
{
    outs() << "\nNodeID: " << getFunPtr(cs);
    outs() << "\nCallSite: ";
    outs() << cs->toString();
    outs() << "\tLocation: " << cs->getSourceLoc();
    outs() << "\t with Targets: ";

    if (!targets.empty())
    {
        FunctionSet::const_iterator fit = targets.begin();
        FunctionSet::const_iterator feit = targets.end();
        for (; fit != feit; ++fit)
        {
            const FunObjVar* callee = *fit;
            outs() << "\n\t" << callee->getName();
        }
    }
    else
    {
        outs() << "\n\tNo Targets!";
    }

    outs() << "\n";
}

/*!
 * Print all indirect callsites
 */
void PointerAnalysis::printIndCSTargets()
{
    outs() << "==================Function Pointer Targets==================\n";
    const CallEdgeMap& callEdges = getIndCallMap();
    CallEdgeMap::const_iterator it = callEdges.begin();
    CallEdgeMap::const_iterator eit = callEdges.end();
    for (; it != eit; ++it)
    {
        const CallICFGNode* cs = it->first;
        const FunctionSet& targets = it->second;
        printIndCSTargets(cs, targets);
    }

    const CallSiteToFunPtrMap& indCS = getIndirectCallsites();
    CallSiteToFunPtrMap::const_iterator csIt = indCS.begin();
    CallSiteToFunPtrMap::const_iterator csEit = indCS.end();
    for (; csIt != csEit; ++csIt)
    {
        const CallICFGNode* cs = csIt->first;
        if (hasIndCSCallees(cs) == false)
        {
            outs() << "\nNodeID: " << csIt->second;
            outs() << "\nCallSite: ";
            outs() << cs->toString();
            outs() << "\tLocation: " << cs->getSourceLoc();
            outs() << "\n\t!!!has no targets!!!\n";
        }
    }
}



/*!
 * Resolve indirect calls
 */
void PointerAnalysis::resolveIndCalls(const CallICFGNode* cs, const PointsTo& target, CallEdgeMap& newEdges)
{

    assert(pag->isIndirectCallSites(cs) && "not an indirect callsite?");
    /// discover indirect pointer target
    for (PointsTo::iterator ii = target.begin(), ie = target.end();
            ii != ie; ii++)
    {

        if(getNumOfResolvedIndCallEdge() >= Options::IndirectCallLimit())
        {
            wrnMsg("Resolved Indirect Call Edges are Out-Of-Budget, please increase the limit");
            return;
        }

        if(ObjVar* objPN = SVFUtil::dyn_cast<ObjVar>(pag->getGNode(*ii)))
        {
            const BaseObjVar* obj = pag->getBaseObject(objPN->getId());

            if(obj->isFunction())
            {
                const FunObjVar* calleefun = SVFUtil::cast<FunObjVar>(obj)->getFunction();
                const FunObjVar* callee = calleefun->getDefFunForMultipleModule();

                if(SVFUtil::matchArgs(cs, callee) == false)
                    continue;

                if(0 == getIndCallMap()[cs].count(callee))
                {
                    newEdges[cs].insert(callee);
                    getIndCallMap()[cs].insert(callee);

                    callgraph->addIndirectCallGraphEdge(cs, cs->getCaller(), callee);
                    // FIXME: do we need to update llvm call graph here?
                    // The indirect call is maintained by ourself, We may update llvm's when we need to
                    //PTACallGraphNode* callgraphNode = callgraph->getOrInsertFunction(cs.getCaller());
                    //callgraphNode->addCalledFunction(cs,callgraph->getOrInsertFunction(callee));
                }
            }
        }
    }
}

/*
 * Get virtual functions "vfns" based on CHA
 */
void PointerAnalysis::getVFnsFromCHA(const CallICFGNode* cs, VFunSet &vfns)
{
    if (chgraph->csHasVFnsBasedonCHA(cs))
        vfns = chgraph->getCSVFsBasedonCHA(cs);
}

/*
 * Get virtual functions "vfns" from PoninsTo set "target" for callsite "cs"
 */
void PointerAnalysis::getVFnsFromPts(const CallICFGNode* cs, const PointsTo &target, VFunSet &vfns)
{

    if (chgraph->csHasVtblsBasedonCHA(cs))
    {
        Set<const GlobalObjVar*> vtbls;
        const VTableSet &chaVtbls = chgraph->getCSVtblsBasedonCHA(cs);
        for (PointsTo::iterator it = target.begin(), eit = target.end(); it != eit; ++it)
        {
            const PAGNode *ptdnode = pag->getGNode(*it);
            const GlobalObjVar* pVar = nullptr;
            if (isa<ObjVar>(ptdnode) && isa<GlobalObjVar>(pag->getBaseObject(ptdnode->getId())))
            {
                pVar = cast<GlobalObjVar>(pag->getBaseObject(ptdnode->getId()));

            }
            else if (isa<ValVar>(ptdnode) &&
                     isa<GlobalValVar>(
                         pag->getBaseValVar(ptdnode->getId())))
            {
                pVar = cast<GlobalObjVar>(
                           SVFUtil::getObjVarOfValVar(cast<GlobalValVar>(
                                   pag->getBaseValVar(ptdnode->getId()))));
            }

            if (pVar && chaVtbls.find(pVar) != chaVtbls.end())
                vtbls.insert(pVar);
        }
        chgraph->getVFnsFromVtbls(cs, vtbls, vfns);
    }
}

/*
 * Connect callsite "cs" to virtual functions in "vfns"
 */
void PointerAnalysis::connectVCallToVFns(const CallICFGNode* cs, const VFunSet &vfns, CallEdgeMap& newEdges)
{
    //// connect all valid functions
    for (VFunSet::const_iterator fit = vfns.begin(),
            feit = vfns.end(); fit != feit; ++fit)
    {
        const FunObjVar* callee = *fit;
        callee = callee->getDefFunForMultipleModule();
        if (getIndCallMap()[cs].count(callee) > 0)
            continue;
        if(cs->arg_size() == callee->arg_size() ||
                (cs->isVarArg() && callee->isVarArg()))
        {
            newEdges[cs].insert(callee);
            getIndCallMap()[cs].insert(callee);
            const CallICFGNode* callBlockNode = cs;
            callgraph->addIndirectCallGraphEdge(callBlockNode, cs->getCaller(),callee);
        }
    }
}

/// Resolve cpp indirect call edges
void PointerAnalysis::resolveCPPIndCalls(const CallICFGNode* cs, const PointsTo& target, CallEdgeMap& newEdges)
{
    assert(cs->isVirtualCall() && "not cpp virtual call");

    VFunSet vfns;
    if (Options::ConnectVCallOnCHA())
        getVFnsFromCHA(cs, vfns);
    else
        getVFnsFromPts(cs, target, vfns);
    connectVCallToVFns(cs, vfns, newEdges);
}

/*!
 * Find the alias check functions annotated in the C files
 * check whether the alias analysis results consistent with the alias check function itself
 */
void PointerAnalysis::validateSuccessTests(std::string fun)
{
    // check for must alias cases, whether our alias analysis produce the correct results
    if (const FunObjVar* checkFun = pag->getFunObjVar(fun))
    {
        if(!checkFun->isUncalledFunction())
            outs() << "[" << this->PTAName() << "] Checking " << fun << "\n";

        for(const CallICFGNode* callNode : pag->getCallSiteSet())
        {
            if (callNode->getCalledFunction() == checkFun)
            {
                assert(callNode->getNumArgOperands() == 2
                       && "arguments should be two pointers!!");
                const SVFVar* V1 = callNode->getArgument(0);
                const SVFVar* V2 = callNode->getArgument(1);
                AliasResult aliasRes = alias(V1->getId(), V2->getId());

                bool checkSuccessful = false;
                if (fun == aliasTestMayAlias || fun == aliasTestMayAliasMangled)
                {
                    if (aliasRes == AliasResult::MayAlias || aliasRes == AliasResult::MustAlias)
                        checkSuccessful = true;
                }
                else if (fun == aliasTestNoAlias || fun == aliasTestNoAliasMangled)
                {
                    if (aliasRes == AliasResult::NoAlias)
                        checkSuccessful = true;
                }
                else if (fun == aliasTestMustAlias || fun == aliasTestMustAliasMangled)
                {
                    // change to must alias when our analysis support it
                    if (aliasRes == AliasResult::MayAlias || aliasRes == AliasResult::MustAlias)
                        checkSuccessful = true;
                }
                else if (fun == aliasTestPartialAlias || fun == aliasTestPartialAliasMangled)
                {
                    // change to partial alias when our analysis support it
                    if (aliasRes == AliasResult::MayAlias)
                        checkSuccessful = true;
                }
                else
                    assert(false && "not supported alias check!!");

                NodeID id1 = V1->getId();
                NodeID id2 = V2->getId();

                if (checkSuccessful)
                    outs() << sucMsg("\t SUCCESS :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << callNode->getSourceLoc() << ")\n";
                else
                {
                    SVFUtil::errs() << errMsg("\t FAILURE :") << fun
                                    << " check <id:" << id1 << ", id:" << id2
                                    << "> at (" << callNode->getSourceLoc() << ")\n";
                    assert(false && "test case failed!");
                }
            }
        }
    }
}

/*!
 * Pointer analysis validator
 */
void PointerAnalysis::validateExpectedFailureTests(std::string fun)
{

    if (const FunObjVar* checkFun = pag->getFunObjVar(fun))
    {
        if(!checkFun->isUncalledFunction())
            outs() << "[" << this->PTAName() << "] Checking " << fun << "\n";

        for(const CallICFGNode* callNode : pag->getCallSiteSet())
        {
            if (callNode->getCalledFunction() == checkFun)
            {
                assert(callNode->arg_size() == 2
                       && "arguments should be two pointers!!");
                const SVFVar* V1 = callNode->getArgument(0);
                const SVFVar* V2 = callNode->getArgument(1);
                AliasResult aliasRes = alias(V1->getId(), V2->getId());

                bool expectedFailure = false;
                if (fun == aliasTestFailMayAlias || fun == aliasTestFailMayAliasMangled)
                {
                    // change to must alias when our analysis support it
                    if (aliasRes == AliasResult::NoAlias)
                        expectedFailure = true;
                }
                else if (fun == aliasTestFailNoAlias || fun == aliasTestFailNoAliasMangled)
                {
                    // change to partial alias when our analysis support it
                    if (aliasRes == AliasResult::MayAlias || aliasRes == AliasResult::PartialAlias || aliasRes == AliasResult::MustAlias)
                        expectedFailure = true;
                }
                else
                    assert(false && "not supported alias check!!");

                NodeID id1 = V1->getId();
                NodeID id2 = V2->getId();

                if (expectedFailure)
                    outs() << sucMsg("\t EXPECTED-FAILURE :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << callNode->getSourceLoc() << ")\n";
                else
                {
                    SVFUtil::errs() << errMsg("\t UNEXPECTED FAILURE :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                                    << callNode->getSourceLoc() << ")\n";
                    assert(false && "test case failed!");
                }
            }
        }
    }
}
