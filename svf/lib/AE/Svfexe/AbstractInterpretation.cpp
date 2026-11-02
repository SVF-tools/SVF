//===- AbstractExecution.cpp -- Abstract Execution---------------------------------//
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


//
//  Created on: Jan 10, 2024
//      Author: Xiao Cheng, Jiawei Wang
//

#include "AE/Svfexe/AbstractInterpretation.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "AE/Svfexe/SparseAbstractInterpretation.h"
#include "Graphs/CallGraph.h"
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "Util/WorkList.h"
#include "WPA/Andersen.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <set>

using namespace SVF;
using namespace SVFUtil;

SVF::AddressSet AbstractInterpretation::blackHoleAddressSet() const
{
    const auto* object = SVFUtil::dyn_cast<ObjVar>(
                             svfir->getGNode(IRGraph::BlackHole));
    if (!object)
    {
        assert(false && "SVFIR has no BlackHole object");
        std::abort();
    }
    return SVF::AddressSet::singleton(adapter_.location(*object));
}

void AbstractInterpretation::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    abstractTrace_.insert_or_assign(node, topState());
    for (const SVFStmt* statement : node->getSVFStmts())
        handleSVFStatement(statement);

    if (const auto* variable = SVFUtil::dyn_cast<ValVar>(
                                   svfir->getGNode(PAG::getPAG()->getBlkPtr())))
        updateValue(variable, SVF::Interval::top(),
                    blackHoleAddressSet(), node);

}

void AbstractInterpretation::initializeObjectValue(
    const ObjVar* object, SVF::Interval& interval, SVF::AddressSet& addresses,
    const ICFGNode* node)
{
    interval = SVF::Interval::bottom();
    addresses = SVF::AddressSet::bottom();
    State& denseState = ensureState(node);
    denseState.allocate(adapter_.location(*object));

    const BaseObjVar* base = PAG::getPAG()->getBaseObject(object->getId());

    if (base->isBlackHoleObj())
    {
        addresses = blackHoleAddressSet();
        return;
    }
    if (base->isConstDataOrConstGlobal() || base->isConstantArray() ||
            base->isConstantStruct())
    {
        if (const auto* integer = SVFUtil::dyn_cast<ConstIntObjVar>(object))
            interval =
                SVF::Interval::singleton(SVF::Rational(integer->getSExtValue()));
        else if (const auto* floating =
                     SVFUtil::dyn_cast<ConstFPObjVar>(object))
            interval = SVFIRAdapter::floatingConstant(floating->getFPValue());
        else if (SVFUtil::isa<ConstNullPtrObjVar>(object))
            addresses = SVF::AddressSet::singleton(SVF::Location::null());
        else if (!SVFUtil::isa<GlobalObjVar>(object))
            interval = SVF::Interval::top();
        if (!interval.isBottom() || !addresses.isBottom())
            return;
    }
    addresses = SVF::AddressSet::singleton(adapter_.location(*object));
}

void AbstractInterpretation::runOnModule()
{
    stat->startClk();
    utils = new AbsExtAPI(this);
    utils->collectCheckPoint();
    analyse();
    utils->checkPointAllSet();
    stat->endClk();
    stat->finializeStat();
    if (Options::PStat())
        stat->performStat();
    for (auto& detector : detectors)
        detector->reportBug();
}

AbstractInterpretation::AbstractInterpretation()
    : adapter_(*PAG::getPAG())
{
    stat = new AEStat(this);
    // Run Andersen's pointer analysis and build WTO
    svfir = PAG::getPAG();
    icfg = svfir->getICFG();
    preAnalysis = new AEWTO(svfir, icfg);
    callGraph = preAnalysis->getCallGraph();
    icfg->updateCallGraph(callGraph);
    preAnalysis->initWTO();
}

/// Factory: first call allocates the concrete subclass based on
/// Options::AESparsity(); all subsequent calls return the same instance.
/// Must only be called after the option parser has populated AESparsity.
AbstractInterpretation& AbstractInterpretation::getAEInstance()
{
    static AbstractInterpretation* instance = []() -> AbstractInterpretation*
    {
        switch (Options::AESparsity())
        {
        case AESparsity::SemiSparse:
            return new SemiSparseAbstractInterpretation();
        case AESparsity::Sparse:
            return new FullSparseAbstractInterpretation();
        case AESparsity::Dense:
        default:
            return new AbstractInterpretation();
        }
    }();
    return *instance;
}

/// Destructor
AbstractInterpretation::~AbstractInterpretation()
{
    delete utils;
    delete stat;
    delete preAnalysis;
}

/// Collect entry point functions for analysis.
/// In main mode, entry is main/svf.main. In no-main mode,
/// entries are SCCs with no external caller in the Andersen-resolved CallGraph.
FIFOWorkList<const FunObjVar*> AbstractInterpretation::collectProgEntryFuns()
{
    FIFOWorkList<const FunObjVar*> entryFunctions;
    const bool mainEntry = Options::AEFunEntry() == AEFunEntryMode::MAIN;
    Set<NodeID> visitedEntrySCCs;
    auto* callGraphSCC = preAnalysis->getCallGraphSCC();

    for (auto it = callGraph->begin(); it != callGraph->end(); ++it)
    {
        const CallGraphNode* cgNode = it->second;
        const FunObjVar* fun = cgNode->getFunction();

        // Skip declarations
        if (fun->isDeclaration())
            continue;

        if (mainEntry)
        {
            if (SVFUtil::isProgEntryFunction(fun))
            {
                entryFunctions.push(fun);
                break;
            }
        }
        else
        {
            NodeID repNodeId = callGraphSCC->repNode(cgNode->getId());
            if (visitedEntrySCCs.count(repNodeId))
                continue;

            const NodeBS& cgSCCNodes = callGraphSCC->subNodes(repNodeId);
            bool hasExternalCaller = false;
            for (NodeID nodeId : cgSCCNodes)
            {
                const CallGraphNode* sccNode = callGraph->getGNode(nodeId);
                for (auto inEdge : sccNode->getInEdges())
                {
                    if (!cgSCCNodes.test(inEdge->getSrcID()))
                    {
                        hasExternalCaller = true;
                        break;
                    }
                }
                if (hasExternalCaller)
                    break;
            }

            if (hasExternalCaller)
                continue;

            visitedEntrySCCs.insert(repNodeId);
            const FunObjVar* entryFun = fun;
            for (NodeID nodeId : cgSCCNodes)
            {
                const FunObjVar* sccFun =
                    callGraph->getGNode(nodeId)->getFunction();
                if (SVFUtil::isProgEntryFunction(sccFun))
                {
                    entryFun = sccFun;
                    break;
                }
            }
            entryFunctions.push(entryFun);
        }
    }

    if (mainEntry && entryFunctions.empty())
    {
        SVFUtil::errs() << SVFUtil::errMsg(
                            "AE -ae-fun-entry=main requires a program entry function, but "
                            "main/svf.main was not found.\n");
        assert(false &&
               "No program entry function found for -ae-fun-entry=main");
        abort();
    }

    return entryFunctions;
}

/// Program entry - entry policy is selected by -ae-fun-entry.
void AbstractInterpretation::analyse()
{
    analyzeFromAllProgEntries();
}

/// Analyze the entry functions selected by collectProgEntryFuns().
/// Abstract state is shared across entry points so that functions analyzed from
/// earlier entries are not re-analyzed from scratch.
void AbstractInterpretation::analyzeFromAllProgEntries()
{
    // Collect all entry point functions
    FIFOWorkList<const FunObjVar*> entryFunctions = collectProgEntryFuns();

    if (entryFunctions.empty())
    {
        assert(false && "No entry functions found for analysis");
        return;
    }
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    const ICFGNode* globalNode = icfg->getGlobalICFGNode();
    while (!entryFunctions.empty())
    {
        const FunObjVar* entryFun = entryFunctions.pop();
        const FunEntryICFGNode* funEntry = icfg->getFunEntryICFGNode(entryFun);
        // Selected roots have no caller transfer to define their arguments.
        // Inputs are unknown values, unlike uninitialized object contents.
        // Match upstream's type-directed policy for external pointer inputs.
        for (const SVFVar* argument : funEntry->getFormalParms())
        {
            if (argument->getType()->isPointerTy())
                updateAddressSet(argument, blackHoleAddressSet(), globalNode);
            else
                updateInterval(argument, unknownInterval(argument), globalNode);
        }
        copyAbstractState(globalNode, funEntry);
        handleFunction(funEntry, nullptr);
    }
}

/// Given a cmp operand, walk its SSA def edge to find the LoadStmt that
/// produced it. This lets us trace back to the ObjVar in memory so that
/// branch narrowing can refine the stored value.
///
/// Example: for `%cmp = icmp sgt %a, 5` where `%a = load i32, ptr %p`,
/// calling findBackingLoad(%a) returns the LoadStmt, and we can then
/// narrow the ObjVar behind %p.
///
/// Follows one level of CopyStmt (e.g., zext/sext) if the load is not
/// directly on the cmp operand. Returns nullptr if no load is found.
static const LoadStmt* findBackingLoad(const SVFVar* var)
{
    if (var->getInEdges().empty())
        return nullptr;
    SVFStmt* inStmt = *var->getInEdges().begin();
    if (const LoadStmt* ls = SVFUtil::dyn_cast<LoadStmt>(inStmt))
        return ls;
    if (const CopyStmt* cs = SVFUtil::dyn_cast<CopyStmt>(inStmt))
    {
        const SVFVar* src = cs->getRHSVar();
        if (!src->getInEdges().empty())
            return SVFUtil::dyn_cast<LoadStmt>(*src->getInEdges().begin());
    }
    return nullptr;
}

/// Compute the interval constraint on one cmp operand given the predicate,
/// branch direction (succ), which side it is on, and the other operand's
/// interval. Returns top if no useful narrowing is possible.
///
/// Called from collectBranchRefinement for each non-constant operand that has a
/// backing load. Given a branch condition like:
///
///   %cmp = icmp sgt %a, 5       ;  a > 5
///   br i1 %cmp, label %T, %F
///
/// On the true branch (succ=1), operand %a (isLHS=true) is constrained to
/// [6, +inf). On the false branch (succ=0), %a is constrained to (-inf, 5].
/// The result is used to narrow the ObjVar behind %a's load.
static SVF::Interval computeCmpConstraint(s32_t predicate, s64_t succ,
        bool isLHS, const SVF::Interval& self,
        const SVF::Interval& other)
{
    // Normalize: always reason from the LHS perspective.
    // If we are the RHS operand, swap the predicate direction.
    if (!isLHS)
    {
        // a > b from b's perspective: b < a
        static const Map<s32_t, s32_t> swapPred =
        {
            {CmpStmt::ICMP_EQ, CmpStmt::ICMP_EQ},
            {CmpStmt::ICMP_NE, CmpStmt::ICMP_NE},
            {CmpStmt::ICMP_SGT, CmpStmt::ICMP_SLT},
            {CmpStmt::ICMP_SGE, CmpStmt::ICMP_SLE},
            {CmpStmt::ICMP_SLT, CmpStmt::ICMP_SGT},
            {CmpStmt::ICMP_SLE, CmpStmt::ICMP_SGE},
            {CmpStmt::ICMP_UGT, CmpStmt::ICMP_ULT},
            {CmpStmt::ICMP_UGE, CmpStmt::ICMP_ULE},
            {CmpStmt::ICMP_ULT, CmpStmt::ICMP_UGT},
            {CmpStmt::ICMP_ULE, CmpStmt::ICMP_UGE},
            {CmpStmt::FCMP_OEQ, CmpStmt::FCMP_OEQ},
            {CmpStmt::FCMP_UEQ, CmpStmt::FCMP_UEQ},
            {CmpStmt::FCMP_OGT, CmpStmt::FCMP_OLT},
            {CmpStmt::FCMP_OGE, CmpStmt::FCMP_OLE},
            {CmpStmt::FCMP_OLT, CmpStmt::FCMP_OGT},
            {CmpStmt::FCMP_OLE, CmpStmt::FCMP_OGE},
            {CmpStmt::FCMP_UGT, CmpStmt::FCMP_ULT},
            {CmpStmt::FCMP_UGE, CmpStmt::FCMP_ULE},
            {CmpStmt::FCMP_ULT, CmpStmt::FCMP_UGT},
            {CmpStmt::FCMP_ULE, CmpStmt::FCMP_UGE},
            {CmpStmt::FCMP_ONE, CmpStmt::FCMP_ONE},
            {CmpStmt::FCMP_UNE, CmpStmt::FCMP_UNE},
        };
        auto it = swapPred.find(predicate);
        if (it == swapPred.end())
            return SVF::Interval::top();
        predicate = it->second;
    }

    // If false branch, negate the predicate.
    if (succ == 0)
    {
        static const Map<s32_t, s32_t> negPred =
        {
            {CmpStmt::ICMP_EQ, CmpStmt::ICMP_NE},
            {CmpStmt::ICMP_NE, CmpStmt::ICMP_EQ},
            {CmpStmt::ICMP_SGT, CmpStmt::ICMP_SLE},
            {CmpStmt::ICMP_SGE, CmpStmt::ICMP_SLT},
            {CmpStmt::ICMP_SLT, CmpStmt::ICMP_SGE},
            {CmpStmt::ICMP_SLE, CmpStmt::ICMP_SGT},
            {CmpStmt::ICMP_UGT, CmpStmt::ICMP_ULE},
            {CmpStmt::ICMP_UGE, CmpStmt::ICMP_ULT},
            {CmpStmt::ICMP_ULT, CmpStmt::ICMP_UGE},
            {CmpStmt::ICMP_ULE, CmpStmt::ICMP_UGT},
            {CmpStmt::FCMP_OEQ, CmpStmt::FCMP_ONE},
            {CmpStmt::FCMP_UEQ, CmpStmt::FCMP_UNE},
            {CmpStmt::FCMP_OGT, CmpStmt::FCMP_OLE},
            {CmpStmt::FCMP_OGE, CmpStmt::FCMP_OLT},
            {CmpStmt::FCMP_OLT, CmpStmt::FCMP_OGE},
            {CmpStmt::FCMP_OLE, CmpStmt::FCMP_OGT},
            {CmpStmt::FCMP_UGT, CmpStmt::FCMP_ULE},
            {CmpStmt::FCMP_UGE, CmpStmt::FCMP_ULT},
            {CmpStmt::FCMP_ULT, CmpStmt::FCMP_UGE},
            {CmpStmt::FCMP_ULE, CmpStmt::FCMP_UGT},
            {CmpStmt::FCMP_ONE, CmpStmt::FCMP_OEQ},
            {CmpStmt::FCMP_UNE, CmpStmt::FCMP_UEQ},
        };
        auto it = negPred.find(predicate);
        if (it == negPred.end())
            return SVF::Interval::top();
        predicate = it->second;
    }

    // Now compute the constraint on LHS given: LHS <predicate> other
    SVF::Interval result = self;
    switch (predicate)
    {
    case CmpStmt::ICMP_EQ:
    case CmpStmt::FCMP_OEQ:
    case CmpStmt::FCMP_UEQ:
        result.meetWith(other);
        break;
    case CmpStmt::ICMP_NE:
    case CmpStmt::FCMP_ONE:
    case CmpStmt::FCMP_UNE:
    case CmpStmt::FCMP_FALSE:
    case CmpStmt::FCMP_TRUE:
        // Disequality does not narrow the memory interval.
        return SVF::Interval::top();
    case CmpStmt::ICMP_UGT:
    case CmpStmt::ICMP_SGT:
    case CmpStmt::FCMP_OGT:
    case CmpStmt::FCMP_UGT:
        if (!other.lower().isFinite())
            return result;
        result.meetWith(
            SVF::Interval(SVF::Bound::finite(other.lower().value(), true),
                          SVF::Bound::plusInfinity()));
        break;
    case CmpStmt::ICMP_UGE:
    case CmpStmt::ICMP_SGE:
    case CmpStmt::FCMP_OGE:
    case CmpStmt::FCMP_UGE:
        if (!other.lower().isFinite())
            return result;
        result.meetWith(
            SVF::Interval(SVF::Bound::finite(other.lower().value(),
                                             other.lower().isStrict()),
                          SVF::Bound::plusInfinity()));
        break;
    case CmpStmt::ICMP_ULT:
    case CmpStmt::ICMP_SLT:
    case CmpStmt::FCMP_OLT:
    case CmpStmt::FCMP_ULT:
        if (!other.upper().isFinite())
            return result;
        result.meetWith(
            SVF::Interval(SVF::Bound::minusInfinity(),
                          SVF::Bound::finite(other.upper().value(), true)));
        break;
    case CmpStmt::ICMP_ULE:
    case CmpStmt::ICMP_SLE:
    case CmpStmt::FCMP_OLE:
    case CmpStmt::FCMP_ULE:
        if (!other.upper().isFinite())
            return result;
        result.meetWith(
            SVF::Interval(SVF::Bound::minusInfinity(),
                          SVF::Bound::finite(other.upper().value(),
                                             other.upper().isStrict())));
        break;
    default:
        return SVF::Interval::top();
    }
    return result;
}

bool AbstractInterpretation::isSwitchDefaultEdge(const IntraCFGEdge* edge) const
{
    const auto* branch = SVFUtil::cast<BranchStmt>(
                             edge->getSrcNode()->getSVFStmts().back());
    return branch->getSuccessor(0) == edge->getDstNode();
}

bool AbstractInterpretation::isSwitchBranchEdgeFeasible(
    const IntraCFGEdge* edge, const SVF::Interval& input) const
{
    if (input.isBottom())
        return false;
    if (edge->getCondition()->getType()->getByteSize() > sizeof(s64_t))
        return true;
    const auto* branch = SVFUtil::cast<BranchStmt>(
                             edge->getSrcNode()->getSVFStmts().back());
    u64_t matchedCases = 0;
    for (u32_t i = 1; i < branch->getNumSuccessors(); ++i)
    {
        const SVF::Rational value(branch->getSuccessorCondValue(i));
        if (input.contains(value))
        {
            if (branch->getSuccessor(i) == edge->getDstNode())
                return true;
            ++matchedCases;
        }
    }
    if (!isSwitchDefaultEdge(edge))
        return false;
    if (!input.lower().isFinite() || !input.upper().isFinite())
        return true;
    const SVF::Rational span = input.upper().value() - input.lower().value();
    return span >= SVF::Rational(static_cast<s64_t>(matchedCases));
}

void AbstractInterpretation::collectSwitchRefinement(
    const IntraCFGEdge* edge, SVF::AbstractDomain& state)
{
    if (isSwitchDefaultEdge(edge))
        return;
    const auto* branch = SVFUtil::cast<BranchStmt>(
                             edge->getSrcNode()->getSVFStmts().back());
    u32_t caseCount = 0;
    s64_t caseValue = 0;
    for (u32_t i = 1; i < branch->getNumSuccessors(); ++i)
        if (branch->getSuccessor(i) == edge->getDstNode())
        {
            ++caseCount;
            caseValue = branch->getSuccessorCondValue(i);
        }
    if (caseCount != 1)
        return;
    const LoadStmt* load = findBackingLoad(edge->getCondition());
    if (!load)
        return;
    const SVF::AddressSet targets = getAddressSet(load->getRHSVar(), load->getICFGNode());
    if (!targets.isSingleton() || targets.hasUnknownObject())
        return;
    const ObjVar* object = objectAt(*targets.begin());
    if (object && object->getId() != IRGraph::NullPtr && object->getId() != IRGraph::BlackHole)
        recordBranchRefinement(object->getId(),
                               SVF::Interval::singleton(SVF::Rational(caseValue)), state,
                               load->getICFGNode(), edge->getDstNode());
}

void AbstractInterpretation::collectBranchRefinement(
    const IntraCFGEdge* edge, SVF::AbstractDomain& state)
{
    const SVFVar* cond = edge->getCondition();
    const ICFGNode* pred = edge->getSrcNode();
    const ICFGNode* succNode = edge->getDstNode();
    s64_t succ = edge->getSuccessorCondValue();

    // Some frontends leave synthetic branch conditions without a defining
    // SVF statement. Assertions are disabled in benchmark builds, so do not
    // dereference the empty edge set; there is simply no sound refinement to
    // collect in this case.
    if (cond->getInEdges().empty())
        return;
    const SVFStmt* condDef = *cond->getInEdges().begin();

    const CmpStmt* cmpStmt = SVFUtil::dyn_cast<CmpStmt>(condDef);
    if (cmpStmt)
    {
        s32_t predicate = cmpStmt->getPredicate();
        if (cmpStmt->getOpVarID(0) == IRGraph::NullPtr ||
                cmpStmt->getOpVarID(1) == IRGraph::NullPtr)
        {
            // p == NULL / p != NULL: no interval obj to refine.
        }
        else
        {
            SVF::Interval opVal[2] = {getInterval(cmpStmt->getOpVar(0), pred),
                                      getInterval(cmpStmt->getOpVar(1), pred)
                                     };
            SVF::AddressSet opAddr[2] =
            {
                getAddressSet(cmpStmt->getOpVar(0), pred),
                getAddressSet(cmpStmt->getOpVar(1), pred)
            };

            if ((opVal[0].isBottom() || opVal[1].isBottom()) &&
                    (!opAddr[0].isBottom() || !opAddr[1].isBottom()))
            {
                // Pointer-valued cmp: branch feasibility only.
            }
            else
            {
                for (int i = 0; i < 2; i++)
                {
                    const int other = 1 - i;
                    const LoadStmt* load =
                        findBackingLoad(cmpStmt->getOpVar(i));

                    if (opVal[i].isSingleton())
                    {
                        // Example: in x < 5, operand 5 is not refined.
                    }
                    else if (!opVal[other].isSingleton())
                    {
                        // Match Original: refine a loaded value only against
                        // a fixed numerical bound, not another interval.
                    }
                    else if (!load)
                    {
                        // Example: cmp uses a computed temporary, not load p.
                    }
                    else
                    {
                        SVF::Interval narrowed = computeCmpConstraint(
                                                     predicate, succ, i == 0, opVal[i], opVal[other]);

                        if (narrowed.isTop())
                        {
                            // != and unsupported predicates reach here.
                        }
                        else
                        {
                            const ICFGNode* loadIcfg = load->getICFGNode();
                            const SVF::AddressSet ptrVal =
                                getAddressSet(load->getRHSVar(), loadIcfg);
                            if (ptrVal.isBottom() || !ptrVal.isFinite())
                            {
                                // Cannot map load p back to concrete ObjVars.
                            }
                            else
                            {
                                for (const SVF::Location location : ptrVal)
                                {
                                    if (const ObjVar* object = objectAt(location))

                                        recordBranchRefinement(
                                            object->getId(), narrowed, state,
                                            loadIcfg, succNode);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
        collectSwitchRefinement(edge, state);
}

void AbstractInterpretation::recordBranchRefinement(
    NodeID objectId, const SVF::Interval& narrowed,
    SVF::AbstractDomain& abstractState, const ICFGNode* loadIcfg, const ICFGNode*)
{
    const auto* object = SVFUtil::dyn_cast<ObjVar>(svfir->getGNode(objectId));
    if (object && hasAbsValue(object, loadIcfg))
    {
        SVF::Interval value = getInterval(object, loadIcfg);
        if (!value.isBottom())
        {
            value.meetWith(narrowed);
            assignInterval(static_cast<State&>(abstractState),
                           adapter_.contentVariable(*object), value);
        }
    }
}

/**
 * Handle an ICFG node: execute statements on the current abstract state.
 * The node's pre-state must already be installed by
 * mergeStatesFromPredecessors, or by handleGlobalNode for the global node.
 * Returns true if the abstract state has changed, false if fixpoint reached or
 * unreachable.
 */
bool AbstractInterpretation::handleICFGNode(const ICFGNode* node)
{
    // Check reachability: pre-state must have been propagated by predecessors
    bool isFunEntry = SVFUtil::isa<FunEntryICFGNode>(node);
    if (!hasAbsState(node))
    {
        if (isFunEntry)
        {
            // Entry point with no callers: inherit from global node
            const ICFGNode* globalNode = icfg->getGlobalICFGNode();
            if (hasAbsState(globalNode))
            {
                copyAbstractState(globalNode, node);
            }
            else
            {
                resetAbstractState(node);
            }
        }
        else
        {
            return false; // unreachable node
        }
    }

    beginAbstractState(node);
    // Store the previous state for fixpoint detection
    std::unique_ptr<SVF::AbstractDomain> previousState =
        cloneAbstractState(node);

    stat->getBlockTrace()++;
    stat->getICFGNodeTrace()++;

    for (const SVFStmt* stmt : node->getSVFStmts())
        handleSVFStatement(stmt);

    // Handle call sites
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        handleCallSite(callNode);
    }

    // Run detectors
    for (auto& detector : detectors)
        detector->detect(node);

    // Track this node as analyzed (for coverage statistics across all entry
    // points)
    allAnalyzedNodes.insert(node);

    if (isAbstractStateEquivalent(node, *previousState))
        return false;

    return true;
}

/**
 * Handle a function using worklist algorithm guided by WTO order.
 * All top-level WTO components are pushed into the worklist upfront,
 * so the traversal order is exactly the WTO order — each node is
 * visited once, and cycles are handled as whole components.
 */
void AbstractInterpretation::handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller)
{
    auto it = preAnalysis->getFuncToWTO().find(funEntry->getFun());
    assert(it != preAnalysis->getFuncToWTO().end() && "Missing WTO for function");

    // Push all top-level WTO components into the worklist in WTO order
    FIFOWorkList<const ICFGWTOComp*> worklist(it->second->getWTOComponents());

    while (!worklist.empty())
    {
        const ICFGWTOComp* comp = worklist.pop();

        if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
        {
            const ICFGNode* node = singleton->getICFGNode();
            if (mergeStatesFromPredecessors(node))
                handleICFGNode(node);
        }
        else if (const ICFGCycleWTO* cycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
        {
            if (mergeStatesFromPredecessors(cycle->head()->getICFGNode()))
                handleLoopOrRecursion(cycle, caller);
        }
    }
}

void AbstractInterpretation::handleCallSite(const ICFGNode* node)
{
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        if (isExtCall(callNode))
        {
            handleExtCall(callNode);
        }
        else
        {
            // Handle both direct and indirect calls uniformly
            handleFunCall(callNode);
        }
    }
    else
        assert(false && "it is not call node");
}

bool AbstractInterpretation::isExtCall(const CallICFGNode* callNode)
{
    return SVFUtil::isExtCall(callNode->getCalledFunction());
}

void AbstractInterpretation::handleExtCall(const CallICFGNode* callNode)
{
    utils->handleExtAPI(callNode);
    // Keep the external model's result, including an initialized Top. The
    // initialization guard carries that definition without an artificial range.
    for (auto& detector : detectors)
    {
        detector->handleStubFunctions(callNode);
    }
}

/// Get callee function: directly for direct calls, via pointer analysis for
/// indirect calls
const FunObjVar* AbstractInterpretation::getCallee(const CallICFGNode* callNode)
{
    // Direct call: get callee directly from call node
    if (const FunObjVar* callee = callNode->getCalledFunction())
        return callee;

    // Indirect call: resolve callee through pointer analysis
    const auto callsiteMaps = svfir->getIndirectCallsites();
    auto it = callsiteMaps.find(callNode);
    if (it == callsiteMaps.end())
        return nullptr;

    NodeID call_id = it->second;
    if (!hasAbsState(callNode))
        return nullptr;

    const SVF::AddressSet addresses =
        getAddressSet(svfir->getSVFVar(call_id), callNode);
    if (!addresses.isFinite() || addresses.empty())
        return nullptr;

    const ObjVar* object = objectAt(*addresses.begin());
    return object ? SVFUtil::dyn_cast<FunObjVar>(object) : nullptr;
}

/// Handle direct or indirect call: get callee(s), process function body, set return state.
///
/// For direct calls, the callee is known statically.
/// For indirect calls, the previous implementation resolved callees from the abstract
/// state's address domain, which only picked the first address and missed other targets.
/// Since the abstract state's address domain is not an over-approximation for function
/// pointers (it may be uninitialized or incomplete), we now use Andersen's pointer
/// analysis results from the pre-computed call graph, which soundly resolves all
/// possible indirect call targets.
void AbstractInterpretation::handleFunCall(const CallICFGNode *callNode)
{
    if (skipRecursiveCall(callNode))
        return;

    // Direct call: callee is known
    if (const FunObjVar* callee = callNode->getCalledFunction())
    {
        const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
        handleFunction(calleeEntry, callNode);
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        copyAbstractState(callNode, retNode);
        return;
    }

    // Indirect call: use Andersen's call graph to get all resolved callees.
    const RetICFGNode* retNode = callNode->getRetICFGNode();
    if (callGraph->hasIndCSCallees(callNode))
    {
        const auto& callees = callGraph->getIndCSCallees(callNode);
        for (const FunObjVar* callee : callees)
        {
            if (callee->isDeclaration())
                continue;
            const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
            handleFunction(calleeEntry, callNode);
        }
    }
    // Resume return node from caller's state (context-insensitive)
    copyAbstractState(callNode, retNode);
}

bool AbstractInterpretation::mergeStatesFromPredecessors(
    const ICFGNode* node)
{
    State merged = bottomState();
    bool hasFeasiblePredecessor = false;
    NodeBS defined;

    for (const ICFGEdge* edge : node->getInEdges())
    {
        const ICFGNode* predecessor = edge->getSrcNode();
        if (abstractTrace_.count(predecessor) == 0)
            continue;

        bool shouldMerge = false;
        const IntraCFGEdge* conditional = SVFUtil::dyn_cast<IntraCFGEdge>(edge);
        if (conditional)
            shouldMerge = true;
        else if (SVFUtil::isa<CallCFGEdge>(edge))
        {
            shouldMerge = true;
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            const auto* returnSite = SVFUtil::dyn_cast<RetICFGNode>(node);
            // A shared callee exit can be reachable through another caller.
            // Preserve call/return matching even when recursive calls use a
            // TOP summary; the summary resumes through copyAbstractState().
            shouldMerge =
                returnSite && (Options::HandleRecur() == TOP ||
                               abstractTrace_.count(returnSite->getCallICFGNode()) != 0);
        }
        if (!shouldMerge)
            continue;

        State source = state(predecessor);
        if (conditional && conditional->getCondition())
        {
            assumeBranch(conditional, source);
            collectBranchRefinement(conditional, source);
        }
        if (source.isBottom())
            continue;

        merged.joinWith(source);
        defined |= definedValues_[predecessor];
        hasFeasiblePredecessor = true;
    }

    if (!hasFeasiblePredecessor)
        return false;
    abstractTrace_.insert_or_assign(node, std::move(merged));
    definedValues_[node] = std::move(defined);
    return true;
}

// Loop / recursion handling (handleLoopOrRecursion + cycle helpers +
// recursion utilities) lives in AELoopRecursion.cpp.

void AbstractInterpretation::handleSVFStatement(const SVFStmt* stmt)
{
    if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
    {
        updateStateOnAddr(addr);
    }
    else if (const BinaryOPStmt* binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
    {
        updateStateOnBinary(binary);
    }
    else if (const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
    {
        updateStateOnCmp(cmp);
    }
    else if (SVFUtil::isa<UnaryOPStmt>(stmt))
    {
    }
    else if (SVFUtil::isa<BranchStmt>(stmt))
    {
        // branch stmt is handled in hasBranchES
    }
    else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
    {
        updateStateOnLoad(load);
    }
    else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
    {
        updateStateOnStore(store);
    }
    else if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
    {
        updateStateOnCopy(copy);
    }
    else if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        updateStateOnGep(gep);
    }
    else if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
    {
        updateStateOnSelect(select);
    }
    else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
    {
        updateStateOnPhi(phi);
    }
    else if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
    {
        // To handle Call Edge
        updateStateOnCall(callPE);
    }
    else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
    {
        updateStateOnRet(retPE);
    }
    else
        assert(false && "implement this part");
    // NullPtr should not be changed by any statement. If the entry is missing
    // (not yet auto-inserted) we treat that as "unchanged" — only check the
    // entry if it actually exists.
}

void AbstractInterpretation::updateStateOnGep(const GepStmt* gep)
{
    const ICFGNode* node = gep->getICFGNode();
    const SVF::Interval offset = getGepElementIndex(gep);
    updateAddressSet(
        gep->getLHSVar(),
        getGepObjAddrs(SVFUtil::cast<ValVar>(gep->getRHSVar()), offset, node),
        node);
}

void AbstractInterpretation::updateStateOnSelect(const SelectStmt* select)
{
    const ICFGNode* node = select->getICFGNode();
    const SVF::Interval condition = getInterval(select->getCondition(), node);
    SVF::Interval interval;
    SVF::AddressSet addresses;
    if (condition.isSingleton())
    {
        const SVFVar* selected = condition.isZero() ? select->getFalseValue()
                                 : select->getTrueValue();
        interval = getInterval(selected, node);
        addresses = getAddressSet(selected, node);
    }
    else
    {
        interval = getInterval(select->getTrueValue(), node);
        interval.joinWith(getInterval(select->getFalseValue(), node));
        addresses = getAddressSet(select->getTrueValue(), node);
        addresses.joinWith(getAddressSet(select->getFalseValue(), node));
    }
    updateValue(select->getRes(), interval, addresses, node);

    return;
}

void AbstractInterpretation::updateStateOnPhi(const PhiStmt* phi)
{
    const ICFGNode* icfgNode = phi->getICFGNode();
    SVF::Interval interval = SVF::Interval::bottom();
    SVF::AddressSet addresses = SVF::AddressSet::bottom();
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = phi->getOpICFGNode(i);
        if (hasAbsState(opICFGNode))
        {
            bool feasible = true;
            const ICFGEdge* edge =
                icfg->getICFGEdge(opICFGNode, icfgNode, ICFGEdge::IntraCF);
            if (edge)
            {
                const IntraCFGEdge* intraEdge =
                    SVFUtil::cast<IntraCFGEdge>(edge);
                if (intraEdge->getCondition())
                {
                    feasible = isBranchEdgeFeasibleAt(intraEdge, opICFGNode);
                }
            }
            if (feasible)
            {
                const SVF::Interval operandInterval =
                    getInterval(phi->getOpVar(i), opICFGNode);
                interval.joinWith(operandInterval);

                addresses.joinWith(getAddressSet(phi->getOpVar(i),
                                                 opICFGNode));

            }
        }
    }

    updateValue(phi->getRes(), interval, addresses, icfgNode);

}

/// Handle CallPE: phi-like merging of actual parameters from all call sites
/// into the formal parameter at FunEntryICFGNode (e.g., formal =
/// join(actual1@cs1, actual2@cs2, ...))
void AbstractInterpretation::updateStateOnCall(const CallPE* callPE)
{
    const ICFGNode* node = callPE->getICFGNode();
    const SVFVar* res = callPE->getRes();
    SVF::Interval interval = SVF::Interval::bottom();
    SVF::AddressSet addresses = SVF::AddressSet::bottom();
    for (u32_t i = 0; i < callPE->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = callPE->getOpCallICFGNode(i);
        if (hasAbsState(opICFGNode))
        {
            interval.joinWith(getInterval(callPE->getOpVar(i), opICFGNode));
            addresses.joinWith(getAddressSet(callPE->getOpVar(i),
                                             opICFGNode));

        }
    }
    updateValue(res, interval, addresses, node);

}

void AbstractInterpretation::updateStateOnRet(const RetPE* retPE)
{
    const ICFGNode* node = retPE->getICFGNode();
    updateValue(retPE->getLHSVar(),
                getInterval(retPE->getRHSVar(), node),
                getAddressSet(retPE->getRHSVar(), node), node);
    return;
}

void AbstractInterpretation::updateStateOnAddr(const AddrStmt* addr)
{
    const ICFGNode* node = addr->getICFGNode();
    const auto* object = SVFUtil::cast<ObjVar>(addr->getRHSVar());
    SVF::Interval interval = SVF::Interval::bottom();
    SVF::AddressSet addresses = SVF::AddressSet::bottom();
    initializeObjectValue(object, interval, addresses, node);
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        interval.meetWith(
            utils->getRangeLimitFromType(addr->getRHSVar()->getType()));
    updateValue(addr->getLHSVar(), interval, addresses, node);
}

void AbstractInterpretation::updateStateOnBinary(const BinaryOPStmt* binary)
{
    const ICFGNode* node = binary->getICFGNode();
    // Treat any unexpected bottom operand as unconstrained for soundness.
    SVF::Interval lhs = getInterval(binary->getOpVar(0), node);
    SVF::Interval rhs = getInterval(binary->getOpVar(1), node);
    if (lhs.isBottom())
        lhs = SVF::Interval::top();
    if (rhs.isBottom())
        rhs = SVF::Interval::top();
    SVF::Interval result;
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    case BinaryOPStmt::FAdd:
        result = SVF::add(lhs, rhs);
        break;
    case BinaryOPStmt::Sub:
    case BinaryOPStmt::FSub:
        result = SVF::subtract(lhs, rhs);
        break;
    case BinaryOPStmt::Mul:
    case BinaryOPStmt::FMul:
        result = SVF::multiply(lhs, rhs);
        break;
    case BinaryOPStmt::SDiv:
    case BinaryOPStmt::FDiv:
    case BinaryOPStmt::UDiv:
        result = SVF::divide(lhs, rhs);
        break;
    case BinaryOPStmt::SRem:
    case BinaryOPStmt::FRem:
    case BinaryOPStmt::URem:
        result = SVF::remainder(lhs, rhs);
        break;
    case BinaryOPStmt::Xor:
        result = SVF::bitwiseXor(lhs, rhs);
        break;
    case BinaryOPStmt::And:
        result = SVF::bitwiseAnd(lhs, rhs);
        break;
    case BinaryOPStmt::Or:
        result = SVF::bitwiseOr(lhs, rhs);
        break;
    case BinaryOPStmt::AShr:
        result = SVF::shiftRight(lhs, rhs);
        break;
    case BinaryOPStmt::Shl:
        result = SVF::shiftLeft(lhs, rhs);
        break;
    case BinaryOPStmt::LShr:
        result = SVF::shiftRight(lhs, rhs);
        break;
    default:
        assert(false && "undefined binary: ");
    }
    updateInterval(binary->getRes(), result, node);
}

void AbstractInterpretation::updateStateOnCmp(const CmpStmt* cmp)
{
    const ICFGNode* node = cmp->getICFGNode();
    SVF::Interval lhsInterval = getInterval(cmp->getOpVar(0), node);
    SVF::Interval rhsInterval = getInterval(cmp->getOpVar(1), node);
    const SVF::AddressSet lhsAddresses = getAddressSet(cmp->getOpVar(0), node);
    const SVF::AddressSet rhsAddresses = getAddressSet(cmp->getOpVar(1), node);
    const bool addressComparison = cmp->getOpVar(0)->isPointer();
    SVF::Interval result =
        SVF::Interval::closed(SVF::Rational(0), SVF::Rational(1));
    const auto boolean = [](bool value)
    {
        return SVF::Interval::singleton(SVF::Rational(value ? 1 : 0));
    };

    if (!addressComparison)
    {
        if (lhsInterval.isBottom()) lhsInterval = SVF::Interval::top();
        if (rhsInterval.isBottom()) rhsInterval = SVF::Interval::top();
    }
    const auto predicate = cmp->getPredicate();
    if (predicate == CmpStmt::FCMP_FALSE)
        result = boolean(false);
    else if (predicate == CmpStmt::FCMP_TRUE)
        result = boolean(true);
    else if (predicate == CmpStmt::FCMP_ORD || predicate == CmpStmt::FCMP_UNO)
    {
        // NaN is not tracked, so either outcome is possible.
    }
    else if (addressComparison)
    {
        const auto containsBlackHole = [&](const SVF::AddressSet& addresses)
        {
            if (!addresses.isFinite())
                return true;
            return std::any_of(addresses.begin(), addresses.end(),
                               [&](SVF::Location location)
            {
                const ObjVar* object = objectAt(location);
                const BaseObjVar* base = object
                                         ? svfir->getBaseObject(object->getId()) : nullptr;
                return base && base->isBlackHoleObj();
            });
        };
        const bool unknownAddress = containsBlackHole(lhsAddresses) ||
                                    containsBlackHole(rhsAddresses);
        const bool exact = lhsAddresses.isSingleton() &&
                           rhsAddresses.isSingleton() && !unknownAddress;
        const bool disjoint = lhsAddresses.isFinite() &&
                              rhsAddresses.isFinite() &&
                              !lhsAddresses.isBottom() &&
                              !rhsAddresses.isBottom() &&
                              !unknownAddress &&
                              !lhsAddresses.hasIntersection(rhsAddresses);
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
            if (exact)
                result =
                    boolean(*lhsAddresses.begin() == *rhsAddresses.begin());
            else if (disjoint)
                result = boolean(false);
            break;
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
            if (exact)
                result =
                    boolean(*lhsAddresses.begin() != *rhsAddresses.begin());
            else if (disjoint)
                result = boolean(true);
            break;
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
            // Object identifiers do not encode pointer order.
            break;
        default:
            assert(false && "undefined pointer compare");
        }
    }
    else
    {
        // An LLVM comparison remains Boolean even if a sparse operand
        // summary is unavailable.
        switch (predicate)
        {
        case CmpStmt::ICMP_EQ:
        case CmpStmt::FCMP_OEQ:
        case CmpStmt::FCMP_UEQ:
            result = SVF::equalTo(lhsInterval, rhsInterval);
            break;
        case CmpStmt::ICMP_NE:
        case CmpStmt::FCMP_ONE:
        case CmpStmt::FCMP_UNE:
            result = SVF::notEqualTo(lhsInterval, rhsInterval);
            break;
        case CmpStmt::ICMP_UGT:
        case CmpStmt::ICMP_SGT:
        case CmpStmt::FCMP_OGT:
        case CmpStmt::FCMP_UGT:
            result = SVF::greaterThan(lhsInterval, rhsInterval);
            break;
        case CmpStmt::ICMP_UGE:
        case CmpStmt::ICMP_SGE:
        case CmpStmt::FCMP_OGE:
        case CmpStmt::FCMP_UGE:
            result = SVF::greaterEqual(lhsInterval, rhsInterval);
            break;
        case CmpStmt::ICMP_ULT:
        case CmpStmt::ICMP_SLT:
        case CmpStmt::FCMP_OLT:
        case CmpStmt::FCMP_ULT:
            result = SVF::lessThan(lhsInterval, rhsInterval);
            break;
        case CmpStmt::ICMP_ULE:
        case CmpStmt::ICMP_SLE:
        case CmpStmt::FCMP_OLE:
        case CmpStmt::FCMP_ULE:
            result = SVF::lessEqual(lhsInterval, rhsInterval);
            break;
        default:
            assert(false && "undefined numerical compare");
        }

    }

    if (!addressComparison && predicate != CmpStmt::FCMP_FALSE &&
            predicate != CmpStmt::FCMP_TRUE && predicate != CmpStmt::FCMP_ORD &&
            predicate != CmpStmt::FCMP_UNO)
    {
        if (lhsInterval.isBottom() || rhsInterval.isBottom())
            result = SVF::Interval::bottom();
        else if (lhsInterval.isTop() || rhsInterval.isTop())
            result = SVF::Interval::top();
    }
    updateInterval(cmp->getRes(), result, node);
}

void AbstractInterpretation::updateStateOnLoad(const LoadStmt* load)
{
    const ICFGNode* node = load->getICFGNode();
    SVF::Interval interval;
    SVF::AddressSet addresses;
    bool numericalMayBeUninitialized = false;
    loadValue(SVFUtil::cast<ValVar>(load->getRHSVar()), interval, addresses,
              numericalMayBeUninitialized, node);

    updateValue(load->getLHSVar(), interval, addresses, node);
    if (numericalMayBeUninitialized)
        addUninitializedNumericalAlternative(load->getLHSVar(), node);

}

void AbstractInterpretation::updateStateOnStore(const StoreStmt* store)
{
    const ICFGNode* node = store->getICFGNode();

    storeValue(SVFUtil::cast<ValVar>(store->getLHSVar()),
               getInterval(store->getRHSVar(), node),
               getAddressSet(store->getRHSVar(), node), node);

}

void AbstractInterpretation::updateStateOnCopy(const CopyStmt* copy)
{
    const ICFGNode* node = copy->getICFGNode();
    const SVFVar* lhsVar = copy->getLHSVar();
    const SVFVar* rhsVar = copy->getRHSVar();

    const SVF::Interval rhsInterval = getInterval(rhsVar, node);
    const SVF::AddressSet rhsAddresses = getAddressSet(rhsVar, node);

    if (copy->getCopyKind() == CopyStmt::COPYVAL)
    {
        updateValue(lhsVar, rhsInterval, rhsAddresses, node);
    }
    else if (copy->getCopyKind() == CopyStmt::ZEXT)
    {
        const auto* sourceType = SVFUtil::dyn_cast<SVFIntegerType>(rhsVar->getType());
        const u32_t bits = rhsVar->getType()->getByteSize() * 8;
        updateInterval(lhsVar, sourceType && rhsInterval.isSingleton()
                       ? (bits == 64 ? rhsInterval : SVF::zeroExtendIntegerInterval(rhsInterval, bits, true))
                       : SVF::Interval::top(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SEXT)
    {
        updateInterval(lhsVar, rhsInterval, node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOSI)
    {
        updateInterval(lhsVar, rhsInterval, node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOUI)
    {
        updateInterval(lhsVar, rhsInterval, node);
    }
    else if (copy->getCopyKind() == CopyStmt::SITOFP)
    {
        updateInterval(lhsVar, rhsInterval, node);
    }
    else if (copy->getCopyKind() == CopyStmt::UITOFP)
    {
        updateInterval(lhsVar, rhsInterval, node);
    }
    else if (copy->getCopyKind() == CopyStmt::TRUNC)
    {
        const auto* destinationType =
            SVFUtil::dyn_cast<SVFIntegerType>(lhsVar->getType());
        updateInterval(
            lhsVar,
            destinationType
            ? SVF::wrapIntegerInterval(rhsInterval,
                                       lhsVar->getType()->getByteSize() * 8,
                                       destinationType->isSigned())
            : SVF::Interval::top(),
            node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTRUNC)
    {
        updateInterval(lhsVar, rhsInterval, node);
    }
    else if (copy->getCopyKind() == CopyStmt::INTTOPTR)
    {
        // Preserve the existing policy: no value is assigned by this cast.
    }
    else if (copy->getCopyKind() == CopyStmt::PTRTOINT)
    {
        updateInterval(lhsVar, SVF::Interval::top(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::BITCAST)
    {
        if (!rhsAddresses.isBottom())
            updateValue(lhsVar, rhsInterval, rhsAddresses, node);
    }
    else
        assert(false && "undefined copy kind");
}
