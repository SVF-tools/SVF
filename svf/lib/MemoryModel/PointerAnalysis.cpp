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

#include "Util/CppUtil.h"
#include "Util/Options.h"
#include "SVFIR/SVFModule.h"
#include "Util/SVFUtil.h"

#include "MemoryModel/PointerAnalysisImpl.h"
#include "SVFIR/PAGBuilderFromFile.h"
#include "Util/PTAStat.h"
#include "Graphs/ThreadCallGraph.h"
#include "Graphs/ICFG.h"
#include "Util/CallGraphBuilder.h"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace SVF;
using namespace SVFUtil;
using namespace cppUtil;


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
PointerAnalysis::PointerAnalysis(SVFIR* p, PTATY ty, bool alias_check) :
    svfMod(nullptr),ptaTy(ty),stat(nullptr),ptaCallGraph(nullptr),callGraphSCC(nullptr),icfg(nullptr),chgraph(nullptr),typeSystem(nullptr)
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
    delete ptaCallGraph;
    ptaCallGraph = nullptr;

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

    svfMod = pag->getModule();
    chgraph = pag->getCHG();

    /// initialise pta call graph for every pointer analysis instance
    if(Options::EnableThreadCallGraph())
    {
        ThreadCallGraph* cg = new ThreadCallGraph();
        ThreadCallGraphBuilder bd(cg, pag->getICFG());
        ptaCallGraph = bd.buildThreadCallGraph(pag->getModule());
    }
    else
    {
        PTACallGraph* cg = new PTACallGraph();
        CallGraphBuilder bd(cg,pag->getICFG());
        ptaCallGraph = bd.buildCallGraph(pag->getModule());
    }
    callGraphSCCDetection();

    // dump callgraph
    if (Options::CallGraphDotGraph())
        getPTACallGraph()->dump("callgraph_initial");
}


/*!
 * Return TRUE if this node is a local variable of recursive function.
 */
bool PointerAnalysis::isLocalVarInRecursiveFun(NodeID id) const
{
    const MemObj* obj = pag->getObject(id);
    assert(obj && "object not found!!");
    if(obj->isStack())
    {
        if(const SVFFunction* svffun = pag->getGNode(id)->getFunction())
        {
            return callGraphSCC->isInCycle(getPTACallGraph()->getCallGraphNode(svffun)->getId());
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
            const_cast<MemObj*>(node->getMemObj())->setFieldSensitive();
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

    getPTACallGraph()->verifyCallGraph();

    if (Options::CallGraphDotGraph())
        getPTACallGraph()->dump("callgraph_final");

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

        outs() << "##<" << node->getValue()->getName() << "> ";
        outs() << "Source Loc: " << node->getValue()->getSourceLoc();
        outs() << "\nNodeID " << node->getId() << "\n";

        const SVFType* type = node->getValue()->getType();
        pag->getSymbolInfo()->printFlattenFields(type);
        if (const SVFPointerType* ptType = SVFUtil::dyn_cast<SVFPointerType>(type))
            pag->getSymbolInfo()->printFlattenFields(ptType->getPtrElementType());
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
    else if (!SVFUtil::isa<DummyValVar>(node) && !SVFModule::pagReadFromTXT())
    {
        if (node->hasValue())
        {
            outs() << "##<" << node->getValue()->getName() << "> ";
            outs() << "Source Loc: " << node->getValue()->getSourceLoc();
        }
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
            if (!SVFModule::pagReadFromTXT())
            {
                if (node->hasValue())
                {
                    outs() << "<" << pagNode->getValue()->getName() << "> ";
                    outs() << "Source Loc: "
                           << pagNode->getValue()->getSourceLoc() << "] \n";
                }
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
    outs() << cs->getCallSite()->toString();
    outs() << "\tLocation: " << cs->getCallSite()->getSourceLoc();
    outs() << "\t with Targets: ";

    if (!targets.empty())
    {
        FunctionSet::const_iterator fit = targets.begin();
        FunctionSet::const_iterator feit = targets.end();
        for (; fit != feit; ++fit)
        {
            const SVFFunction* callee = *fit;
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
            outs() << cs->getCallSite()->toString();
            outs() << "\tLocation: " << cs->getCallSite()->getSourceLoc();
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
            const MemObj* obj = pag->getObject(objPN);

            if(obj->isFunction())
            {
                const SVFFunction* calleefun = SVFUtil::cast<SVFFunction>(obj->getValue());
                const SVFFunction* callee = calleefun->getDefFunForMultipleModule();

                if(SVFUtil::matchArgs(cs->getCallSite(), callee) == false)
                    continue;

                if(0 == getIndCallMap()[cs].count(callee))
                {
                    newEdges[cs].insert(callee);
                    getIndCallMap()[cs].insert(callee);

                    ptaCallGraph->addIndirectCallGraphEdge(cs, cs->getCaller(), callee);
                    // FIXME: do we need to update llvm call graph here?
                    // The indirect call is maintained by ourself, We may update llvm's when we need to
                    //CallGraphNode* callgraphNode = callgraph->getOrInsertFunction(cs.getCaller());
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
    if (chgraph->csHasVFnsBasedonCHA(SVFUtil::getSVFCallSite(cs->getCallSite())))
        vfns = chgraph->getCSVFsBasedonCHA(SVFUtil::getSVFCallSite(cs->getCallSite()));
}

/*
 * Get virtual functions "vfns" from PoninsTo set "target" for callsite "cs"
 */
void PointerAnalysis::getVFnsFromPts(const CallICFGNode* cs, const PointsTo &target, VFunSet &vfns)
{

    if (chgraph->csHasVtblsBasedonCHA(SVFUtil::getSVFCallSite(cs->getCallSite())))
    {
        Set<const SVFGlobalValue*> vtbls;
        const VTableSet &chaVtbls = chgraph->getCSVtblsBasedonCHA(SVFUtil::getSVFCallSite(cs->getCallSite()));
        for (PointsTo::iterator it = target.begin(), eit = target.end(); it != eit; ++it)
        {
            const PAGNode *ptdnode = pag->getGNode(*it);
            if (ptdnode->hasValue())
            {
                if (const SVFGlobalValue *vtbl = SVFUtil::dyn_cast<SVFGlobalValue>(ptdnode->getValue()))
                {
                    if (chaVtbls.find(vtbl) != chaVtbls.end())
                        vtbls.insert(vtbl);
                }
            }
        }
        chgraph->getVFnsFromVtbls(SVFUtil::getSVFCallSite(cs->getCallSite()), vtbls, vfns);
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
        const SVFFunction* callee = *fit;
        callee = callee->getDefFunForMultipleModule();
        if (getIndCallMap()[cs].count(callee) > 0)
            continue;
        if(SVFUtil::getSVFCallSite(cs->getCallSite()).arg_size() == callee->arg_size() ||
                (SVFUtil::getSVFCallSite(cs->getCallSite()).isVarArg() && callee->isVarArg()))
        {
            newEdges[cs].insert(callee);
            getIndCallMap()[cs].insert(callee);
            const CallICFGNode* callBlockNode = pag->getICFG()->getCallICFGNode(cs->getCallSite());
            ptaCallGraph->addIndirectCallGraphEdge(callBlockNode, cs->getCaller(),callee);
        }
    }
}

/// Resolve cpp indirect call edges
void PointerAnalysis::resolveCPPIndCalls(const CallICFGNode* cs, const PointsTo& target, CallEdgeMap& newEdges)
{
    assert(SVFUtil::getSVFCallSite(cs->getCallSite()).isVirtualCall() && "not cpp virtual call");

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
    if (const SVFFunction* checkFun = svfMod->getSVFFunction(fun))
    {
        if(!checkFun->isUncalledFunction())
            outs() << "[" << this->PTAName() << "] Checking " << fun << "\n";

        for(const CallICFGNode* callNode : pag->getCallSiteSet())
        {
            const SVFInstruction* svfInst = callNode->getCallSite();
            if (SVFUtil::getCallee(svfInst) == checkFun)
            {

                CallSite cs(svfInst);
                assert(cs.getNumArgOperands() == 2
                       && "arguments should be two pointers!!");
                const SVFValue* V1 = cs.getArgOperand(0);
                const SVFValue* V2 = cs.getArgOperand(1);
                AliasResult aliasRes = alias(V1, V2);

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

                NodeID id1 = pag->getValueNode(V1);
                NodeID id2 = pag->getValueNode(V2);

                if (checkSuccessful)
                    outs() << sucMsg("\t SUCCESS :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << svfInst->getSourceLoc() << ")\n";
                else
                {
                    SVFUtil::errs() << errMsg("\t FAILURE :") << fun
                                    << " check <id:" << id1 << ", id:" << id2
                                    << "> at (" << svfInst->getSourceLoc() << ")\n";
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

    if (const SVFFunction* checkFun = svfMod->getSVFFunction(fun))
    {
        if(!checkFun->isUncalledFunction())
            outs() << "[" << this->PTAName() << "] Checking " << fun << "\n";

        for(const CallICFGNode* callNode : pag->getCallSiteSet())
        {
            const SVFInstruction* svfInst = callNode->getCallSite();
            if (SVFUtil::getCallee(svfInst) == checkFun)
            {
                CallSite call = getSVFCallSite(svfInst);
                assert(call.arg_size() == 2
                       && "arguments should be two pointers!!");
                const SVFValue* V1 = call.getArgOperand(0);
                const SVFValue* V2 = call.getArgOperand(1);
                AliasResult aliasRes = alias(V1, V2);

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

                NodeID id1 = pag->getValueNode(V1);
                NodeID id2 = pag->getValueNode(V2);

                if (expectedFailure)
                    outs() << sucMsg("\t EXPECTED-FAILURE :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << call.getInstruction()->getSourceLoc() << ")\n";
                else
                {
                    SVFUtil::errs() << errMsg("\t UNEXPECTED FAILURE :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                                    << call.getInstruction()->getSourceLoc() << ")\n";
                    assert(false && "test case failed!");
                }
            }
        }
    }
}
