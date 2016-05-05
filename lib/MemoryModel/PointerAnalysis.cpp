//===- PointerAnalysis.cpp -- Base class of pointer analyses------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2016>  <Yulei Sui>
// Copyright (C) <2013-2016>  <Jingling Xue>

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
 * PointerAnalysis.cpp
 *
 *  Created on: May 14, 2013
 *      Author: rocky
 */

#include "MemoryModel/PointerAnalysis.h"
#include "MemoryModel/PAGBuilder.h"
#include "Util/GraphUtil.h"
#include "Util/AnalysisUtil.h"
#include "Util/PTAStat.h"
#include "Util/ThreadCallGraph.h"

using namespace llvm;
using namespace analysisUtil;

static cl::opt<bool> TYPEPrint("print-type", cl::init(false),
                               cl::desc("Print type"));

static cl::opt<bool> FuncPointerPrint("print-fp", cl::init(false),
                                      cl::desc("Print targets of indirect call site"));

static cl::opt<bool> PTSPrint("print-pts", cl::init(false),
                              cl::desc("Print points-to set of top-level pointers"));

static cl::opt<bool> PTSAllPrint("print-all-pts", cl::init(false),
                                 cl::desc("Print all points-to set of both top-level and address-taken variables"));

static cl::opt<bool> PStat("stat", cl::init(true),
                           cl::desc("Statistic for Pointer analysis"));

static cl::opt<unsigned> statBudget("statlimit",  cl::init(20),
                                    cl::desc("Iteration budget for On-the-fly statistics"));

static cl::opt<bool> PAGDotGraph("dump-pag", cl::init(false),
                                 cl::desc("Dump dot graph of PAG"));

static cl::opt<bool> PAGPrint("print-pag", cl::init(false),
                              cl::desc("Print PAG to command line"));

static cl::opt<std::string> Graphtxt("graphtxt", cl::value_desc("filename"),
                                     cl::desc("graph txt file to build PAG"));

static cl::opt<unsigned> IndirectCallLimit("indCallLimit",  cl::init(50000),
        cl::desc("Indirect solved call edge limit"));

static cl::opt<bool> UsePreCompFieldSensitive("preFieldSensitive", cl::init(true),
        cl::desc("Use pre-computed field-sensitivity for later analysis"));

static cl::opt<bool> EnableAliasCheck("alias-check", cl::init(true),
                                      cl::desc("Enable alias check functions"));

static cl::opt<bool> EnableThreadCallGraph("enable-tcg", cl::init(true),
        cl::desc("Enable pointer analysis to use thread call graph"));

static cl::opt<bool> INCDFPTData("incdata", cl::init(true),
                                 cl::desc("Enable incremental DFPTData for flow-sensitive analysis"));

PAG* PointerAnalysis::pag = NULL;
llvm::Module* PointerAnalysis::mod = NULL;

/*!
 * Constructor
 */
PointerAnalysis::PointerAnalysis(PTATY ty) :
    ptaTy(ty),stat(NULL),ptaCallGraph(NULL),callGraphSCC(NULL) {
    OnTheFlyIterBudgetForStat = statBudget;
    print_stat = PStat;
    numOfIteration = 0;
}

/*!
 * Destructor
 */
PointerAnalysis::~PointerAnalysis() {
    destroy();
    // do not delete the PAG for now
    //delete pag;
}


void PointerAnalysis::destroy()
{
    delete ptaCallGraph;
    ptaCallGraph = NULL;

    delete callGraphSCC;
    callGraphSCC = NULL;

    delete stat;
    stat = NULL;
}


/*!
 * Initialization of pointer analysis
 */
void PointerAnalysis::initialize(Module& module) {
    /// whether we have already built PAG
    if(pag == NULL) {

        DBOUT(DGENERAL, outs() << pasMsg("Building Symbol table ...\n"));
        SymbolTableInfo* symTable = SymbolTableInfo::Symbolnfo();
        symTable->buildMemModel(module);

        DBOUT(DGENERAL, outs() << pasMsg("Building PAG ...\n"));
        if (!Graphtxt.getValue().empty()) {
            PAGBuilderFromFile fileBuilder(Graphtxt.getValue());
            pag = fileBuilder.build();

        } else {
            PAGBuilder builder;
            pag = builder.build(module);
        }

        // dump the PAG graph
        if (dumpGraph())
            PAG::getPAG()->dump("pag_initial");

        // print to command line of the PAG graph
        if (PAGPrint)
            pag->print();
    }

    mod = &module;

    /// initialise pta call graph
    if(EnableThreadCallGraph)
        ptaCallGraph = new ThreadCallGraph(mod);
    else
        ptaCallGraph = new PTACallGraph(mod);
    callGraphSCCDetection();
}

/*!
 * Return TRUE if this node is a local variable of recursive function.
 */
bool PointerAnalysis::isLocalVarInRecursiveFun(NodeID id) const
{
    const MemObj* obj = this->pag->getObject(id);
    assert(obj && "object not found!!");
    if(obj->isStack()) {
        if(const AllocaInst* local = dyn_cast<AllocaInst>(obj->getRefVal())) {
            const Function* fun = local->getParent()->getParent();
            return callGraphSCC->isInCycle(getPTACallGraph()->getCallGraphNode(fun)->getId());
        }
    }
    return false;
}

/*!
 * Reset field sensitivity
 */
void PointerAnalysis::resetObjFieldSensitive()
{
    for (PAG::iterator nIter = pag->begin(); nIter != pag->end(); ++nIter) {
        if(ObjPN* node = dyn_cast<ObjPN>(nIter->second))
            const_cast<MemObj*>(node->getMemObj())->setFieldSensitive();
    }
}

/*!
 * Flag in order to dump graph
 */
bool PointerAnalysis::dumpGraph() {
    return PAGDotGraph;
}

/*
 * Dump statistics
 */

void PointerAnalysis::dumpStat() {

    if(PStat && stat)
        stat->performStat();
}


/*!
 * Finalize the analysis after solving
 * Given the alias results, verify whether it is correct or not using alias check functions
 */
void PointerAnalysis::finalize() {

    /// Print statistics
    dumpStat();

    PAG* pag = PAG::getPAG();
    // dump the PAG graph
    if (dumpGraph())
        pag->dump("pag_final");

    /// Dump results
    if (PTSPrint) {
        dumpTopLevelPtsTo();
        //dumpAllPts();
        //dumpCPts();
    }

    if (TYPEPrint)
        dumpAllTypes();

    if(PTSAllPrint)
        dumpAllPts();

    if (FuncPointerPrint)
        printIndCSTargets();

    getPTACallGraph()->vefityCallGraph();

    getPTACallGraph()->dump("callgraph_final");

    if(!pag->isBuiltFromFile() && EnableAliasCheck) {
        validateSuccessTests("MAYALIAS");
        validateSuccessTests("NOALIAS");
        validateSuccessTests("MUSTALIAS");
        validateSuccessTests("PARTIALALIAS");
        validateExpectedFailureTests("EXPECTEDFAIL_MAYALIAS");
        validateExpectedFailureTests("EXPECTEDFAIL_NOALIAS");
    }

    if (!UsePreCompFieldSensitive)
        resetObjFieldSensitive();
}

void PointerAnalysis::dumpAllTypes()
{
    for (NodeBS::iterator nIter = this->getAllValidPtrs().begin();
            nIter != this->getAllValidPtrs().end(); ++nIter) {
        const PAGNode* node = getPAG()->getPAGNode(*nIter);
        if (isa<DummyObjPN>(node) || isa<DummyValPN>(node))
            continue;

        outs() << "##<" << node->getValue()->getName() << "> ";
        outs() << "Souce Loc: " << getSourceLoc(node->getValue());
        outs() << "\nNodeID " << node->getId() << "\n";

        llvm::Type* type = node->getValue()->getType();
        SymbolTableInfo::Symbolnfo()->printFlattenFields(type);
        if (PointerType* ptType = dyn_cast<PointerType>(type))
            SymbolTableInfo::Symbolnfo()->printFlattenFields(ptType->getElementType());
    }
}


/*!
 * Constructor
 */
BVDataPTAImpl::BVDataPTAImpl(PointerAnalysis::PTATY type) : PointerAnalysis(type) {
    if(type == Andersen_WPA || type == AndersenWave_WPA || type == AndersenLCD_WPA) {
        ptD = new PTDataTy();
    }
    else if (type == AndersenWaveDiff_WPA) {
        ptD = new DiffPTDataTy();
    }
    else if (type == FSSPARSE_WPA) {
        if(INCDFPTData)
            ptD = new IncDFPTDataTy();
        else
            ptD = new DFPTDataTy();
    }
    else if (type == FlowS_DDA) {
        ptD = new PTDataTy();
    }
    else
        assert(false && "no points-to data available");
}

/*!
 * Expand all fields of an aggregate in all points-to sets
 */
void BVDataPTAImpl::expandFIObjs(const PointsTo& pts, PointsTo& expandedPts) {
    expandedPts = pts;;
    for(PointsTo::iterator pit = pts.begin(), epit = pts.end(); pit!=epit; ++pit) {
        if(pag->getBaseObjNode(*pit)==*pit) {
            expandedPts |= pag->getAllFieldsObjNode(*pit);
        }
    }
}

/*!
 * Dump points-to of each pag node
 */
void BVDataPTAImpl::dumpTopLevelPtsTo() {
    for (NodeBS::iterator nIter = this->getAllValidPtrs().begin();
            nIter != this->getAllValidPtrs().end(); ++nIter) {
        const PAGNode* node = getPAG()->getPAGNode(*nIter);
        if (getPAG()->isValidTopLevelPtr(node)) {
            PointsTo& pts = this->getPts(node->getId());
            outs() << "\nNodeID " << node->getId() << " ";

            if (pts.empty()) {
                outs() << "\t\tPointsTo: {empty}\n\n";
            } else {
                outs() << "\t\tPointsTo: { ";
                for (PointsTo::iterator it = pts.begin(), eit = pts.end();
                        it != eit; ++it)
                    outs() << *it << " ";
                outs() << "}\n\n";
            }
        }
    }
}

/*!
 * Dump points-to of top-level pointers (ValPN)
 */
void PointerAnalysis::dumpPts(NodeID ptr, const PointsTo& pts) {

    const PAGNode* node = pag->getPAGNode(ptr);
    /// print the points-to set of node which has the maximum pts size.
    if (isa<DummyObjPN> (node)) {
        outs() << "##<Dummy Obj > id:" << node->getId();
    } else if (!isa<DummyValPN>(node)) {
        outs() << "##<" << node->getValue()->getName() << "> ";
        outs() << "Souce Loc: " << getSourceLoc(node->getValue());
    }
    outs() << "\nPtr " << node->getId() << " ";

    if (pts.empty()) {
        outs() << "\t\tPointsTo: {empty}\n\n";
    } else {
        outs() << "\t\tPointsTo: { ";
        for (PointsTo::iterator it = pts.begin(), eit = pts.end(); it != eit;
                ++it)
            outs() << *it << " ";
        outs() << "}\n\n";
    }

    outs() << "";

    for (NodeBS::iterator it = pts.begin(), eit = pts.end(); it != eit; ++it) {
        const PAGNode* node = pag->getPAGNode(*it);
        if(isa<ObjPN>(node) == false)
            continue;
        NodeID ptd = node->getId();
        outs() << "!!Target NodeID " << ptd << "\t [";
        const PAGNode* pagNode = pag->getPAGNode(ptd);
        if (isa<DummyValPN>(node))
            outs() << "DummyVal\n";
        else if (isa<DummyObjPN>(node))
            outs() << "Dummy Obj id: " << node->getId() << "]\n";
        else {
            outs() << "<" << pagNode->getValue()->getName() << "> ";
            outs() << "Souce Loc: " << getSourceLoc(pagNode->getValue()) << "] \n";
        }
    }
}


/*!
 * Dump all points-to including top-level (ValPN) and address-taken (ObjPN) variables
 */
void BVDataPTAImpl::dumpAllPts() {
    for(PAG::iterator it = pag->begin(), eit = pag->end(); it!=eit; it++) {
        outs() << "----------------------------------------------\n";
        dumpPts(it->first, this->getPts(it->first));
        outs() << "----------------------------------------------\n";
    }
}

/*!
 * Print indirect call targets at an indirect callsite
 */
void PointerAnalysis::printIndCSTargets(const llvm::CallSite cs, const FunctionSet& targets)
{
    llvm::outs() << "\nNodeID: " << getFunPtr(cs);
    llvm::outs() << "\nCallSite: ";
    cs.getInstruction()->print(llvm::outs());
    llvm::outs() << "\tLocation: " << analysisUtil::getSourceLoc(cs.getInstruction());
    llvm::outs() << "\t with Targets: ";

    if (!targets.empty()) {
        FunctionSet::const_iterator fit = targets.begin();
        FunctionSet::const_iterator feit = targets.end();
        for (; fit != feit; ++fit) {
            const llvm::Function* callee = *fit;
            llvm::outs() << "\n\t" << callee->getName();
        }
    }
    else {
        llvm::outs() << "\n\tNo Targets!";
    }

    llvm::outs() << "\n";
}

/*!
 * Print all indirect callsites
 */
void PointerAnalysis::printIndCSTargets()
{
    llvm::outs() << "==================Function Pointer Targets==================\n";
    const CallEdgeMap& callEdges = getIndCallMap();
    CallEdgeMap::const_iterator it = callEdges.begin();
    CallEdgeMap::const_iterator eit = callEdges.end();
    for (; it != eit; ++it) {
        const llvm::CallSite cs = it->first;
        const FunctionSet& targets = it->second;
        printIndCSTargets(cs, targets);
    }

    const CallSiteToFunPtrMap& indCS = getIndirectCallsites();
    CallSiteToFunPtrMap::const_iterator csIt = indCS.begin();
    CallSiteToFunPtrMap::const_iterator csEit = indCS.end();
    for (; csIt != csEit; ++csIt) {
        const llvm::CallSite& cs = csIt->first;
        if (hasIndCSCallees(cs) == false) {
            llvm::outs() << "\nNodeID: " << csIt->second;
            llvm::outs() << "\nCallSite: ";
            cs.getInstruction()->print(llvm::outs());
            llvm::outs() << "\tLocation: " << analysisUtil::getSourceLoc(cs.getInstruction());
            llvm::outs() << "\n\t!!!has no targets!!!\n";
        }
    }
}

/*!
 * On the fly call graph construction
 * callsites is candidate indirect callsites need to be analyzed based on points-to results
 * newEdges is the new indirect call edges discovered
 */
void BVDataPTAImpl::onTheFlyCallGraphSolve(const CallSiteToFunPtrMap& callsites, CallEdgeMap& newEdges,CallGraph* callgraph) {
    for(CallSiteToFunPtrMap::const_iterator iter = callsites.begin(), eiter = callsites.end(); iter!=eiter; ++iter) {
        resolveIndCalls(iter->first,getPts(iter->second),newEdges,callgraph);
    }
}

/*!
 * Resolve indirect calls
 */
void PointerAnalysis::resolveIndCalls(CallSite cs, const PointsTo& target, CallEdgeMap& newEdges,llvm::CallGraph* callgraph) {

    assert(pag->isIndirectCallSites(cs) && "not an indirect callsite?");
    /// discover indirect pointer target
    for (PointsTo::iterator ii = target.begin(), ie = target.end();
            ii != ie; ii++) {

        if(getNumOfResolvedIndCallEdge() > IndirectCallLimit) {
            errMsg("Resolved Indirect Call Edges are Out-Of-Budget, please increase the limit");
            return;
        }

        if(ObjPN* objPN = dyn_cast<ObjPN>(pag->getPAGNode(*ii))) {
            const MemObj* obj = pag->getObject(objPN);

            if(obj->isFunction()) {
                const Function* callee = cast<Function>(obj->getRefVal());

                /// if the arg size does not match then we do not need to connect this parameter
                /// even if the callee is a variadic function (the first parameter of variadic function is its paramter number)
                if(cs.arg_size() != callee->arg_size())
                    continue;

                if(0 == getIndCallMap()[cs].count(callee)) {
                    newEdges[cs].insert(callee);
                    getIndCallMap()[cs].insert(callee);

                    ptaCallGraph->addIndirectCallGraphEdge(cs.getInstruction(), callee);
                    // FIXME: do we need to update llvm call graph here?
                    // The indirect call is maintained by ourself, We may update llvm's when we need to
                    //CallGraphNode* callgraphNode = callgraph->getOrInsertFunction(cs.getCaller());
                    //callgraphNode->addCalledFunction(cs,callgraph->getOrInsertFunction(callee));
                }
            }
        }
    }
}

/*!
 * Find the alias check functions annotated in the C files
 * check whether the alias analysis results consistent with the alias check function itself
 */
void PointerAnalysis::validateSuccessTests(const char* fun) {

    // check for must alias cases, whether our alias analysis produce the correct results
    if (Function* checkFun = mod->getFunction(fun)) {
        if(!checkFun->use_empty())
            outs() << "[" << this->PTAName() << "] Checking " << fun << "\n";

        for (Value::user_iterator i = checkFun->user_begin(), e =
                    checkFun->user_end(); i != e; ++i)
            if (CallInst *call = dyn_cast<CallInst>(*i)) {
                assert(call->getNumArgOperands() == 2
                       && "arguments should be two pointers!!");
                Value* V1 = call->getArgOperand(0);
                Value* V2 = call->getArgOperand(1);
                AliasResult aliasRes = alias(V1, V2);

                bool checkSuccessful = false;
                if (strcmp(fun, "MAYALIAS") == 0) {
                    if (aliasRes == MayAlias || aliasRes == MustAlias)
                        checkSuccessful = true;
                } else if (strcmp(fun, "NOALIAS") == 0) {
                    if (aliasRes == NoAlias)
                        checkSuccessful = true;
                } else if (strcmp(fun, "MUSTALIAS") == 0) {
                    // change to must alias when our analysis support it
                    if (aliasRes == MayAlias || aliasRes == MustAlias)
                        checkSuccessful = true;
                } else if (strcmp(fun, "PARTIALALIAS") == 0) {
                    // change to partial alias when our analysis support it
                    if (aliasRes == MayAlias)
                        checkSuccessful = true;
                } else
                    assert(false && "not supported alias check!!");

                NodeID id1 = pag->getValueNode(V1);
                NodeID id2 = pag->getValueNode(V2);

                if (checkSuccessful)
                    outs() << sucMsg("\t SUCCESS :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << getSourceLoc(call) << ")\n";
                else
                    errs() << errMsg("\t FAIL :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << getSourceLoc(call) << ")\n";
            } else
                assert(false && "alias check functions not only used at callsite??");

    }
}

/*!
 * Pointer analysis validator
 */
void PointerAnalysis::validateExpectedFailureTests(const char* fun) {

    if (Function* checkFun = mod->getFunction(fun)) {
        if(!checkFun->use_empty())
            outs() << "[" << this->PTAName() << "] Checking " << fun << "\n";

        for (Value::user_iterator i = checkFun->user_begin(), e =
                    checkFun->user_end(); i != e; ++i)
            if (CallInst *call = dyn_cast<CallInst>(*i)) {
                assert(call->getNumArgOperands() == 2
                       && "arguments should be two pointers!!");
                Value* V1 = call->getArgOperand(0);
                Value* V2 = call->getArgOperand(1);
                AliasResult aliasRes = alias(V1, V2);

                bool expectedFailure = false;
                if (strcmp(fun, "EXPECTEDFAIL_MAYALIAS") == 0) {
                    // change to must alias when our analysis support it
                    if (aliasRes == NoAlias)
                        expectedFailure = true;
                } else if (strcmp(fun, "EXPECTEDFAIL_NOALIAS") == 0) {
                    // change to partial alias when our analysis support it
                    if (aliasRes == MayAlias || aliasRes == PartialAlias || aliasRes == MustAlias)
                        expectedFailure = true;
                } else
                    assert(false && "not supported alias check!!");

                NodeID id1 = pag->getValueNode(V1);
                NodeID id2 = pag->getValueNode(V2);

                if (expectedFailure)
                    outs() << sucMsg("\t EXPECTED FAIL :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << getSourceLoc(call) << ")\n";
                else
                    errs() << errMsg("\t UNEXPECTED FAIL :") << fun << " check <id:" << id1 << ", id:" << id2 << "> at ("
                           << getSourceLoc(call) << ")\n";
            }
            else
                assert(false && "alias check functions not only used at callsite??");
    }
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
llvm::AliasResult BVDataPTAImpl::alias(const llvm::MemoryLocation &LocA,
                                       const llvm::MemoryLocation &LocB) {
    return alias(LocA.Ptr, LocB.Ptr);
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
llvm::AliasResult BVDataPTAImpl::alias(const Value* V1,
                                       const Value* V2) {
    return alias(pag->getValueNode(V1),pag->getValueNode(V2));
}

/*!
 * Return alias results based on our points-to/alias analysis
 */
llvm::AliasResult BVDataPTAImpl::alias(NodeID node1, NodeID node2) {

    PointsTo& p1 = getPts(node1);
    PointsTo& p2 = getPts(node2);
    PointsTo pts1;
    expandFIObjs(p1,pts1);
    PointsTo pts2;
    expandFIObjs(p2,pts2);

    if (containBlackHoleNode(pts1) || containBlackHoleNode(pts2) || pts1.intersects(pts2))
        return MayAlias;
    else
        return NoAlias;
}
