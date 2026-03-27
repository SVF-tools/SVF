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
#include "SVFIR/SVFIR.h"
#include "Util/Options.h"
#include "Util/WorkList.h"
#include "Graphs/CallGraph.h"
#include "WPA/Andersen.h"
#include <cmath>
#include <deque>

using namespace SVF;
using namespace SVFUtil;


void AbstractInterpretation::runOnModule(ICFG *_icfg)
{
    stat->startClk();
    icfg = _icfg;
    svfir = PAG::getPAG();
    utils = new AbsExtAPI(*this);

    // Run Andersen's pointer analysis and build WTO
    preAnalysis = new PreAnalysis(svfir, icfg);
    callGraph = preAnalysis->getCallGraph();
    icfg->updateCallGraph(callGraph);
    preAnalysis->initWTO();

    /// collect checkpoint
    utils->collectCheckPoint();

    analyse();
    utils->checkPointAllSet();
    stat->endClk();
    stat->finializeStat();
    if (Options::PStat())
        stat->performStat();
    for (auto& detector: detectors)
        detector->reportBug();
}

AbstractInterpretation::AbstractInterpretation()
{
    stat = new AEStat(this);
}

AbstractState& AbstractInterpretation::getAbstractState(const ICFGNode* node)
{
    if (abstractTrace.count(node) == 0)
    {
        assert(false && "No preAbsTrace for this node");
        abort();
    }
    else
    {
        return abstractTrace[node];
    }
}

bool AbstractInterpretation::hasAbstractState(const ICFGNode* node)
{
    return abstractTrace.count(node) != 0;
}

const AbstractValue& AbstractInterpretation::getAbstractValue(const ValVar* var, const ICFGNode* node)
{
    u32_t id = var->getId();
    bool semiSparse = Options::AESparsity() == AESparsity::SemiSparse;

    // Constants: store into current node's state and return
    AbstractState& as = abstractTrace[node];
    if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
    {
        as[id] = IntervalValue(constInt->getSExtValue(), constInt->getSExtValue());
        return as[id];
    }
    if (const ConstFPValVar* constFP = SVFUtil::dyn_cast<ConstFPValVar>(var))
    {
        as[id] = IntervalValue(constFP->getFPValue(), constFP->getFPValue());
        return as[id];
    }
    if (SVFUtil::isa<ConstNullPtrValVar>(var))
    {
        as[id] = AddressValue();
        return as[id];
    }
    if (SVFUtil::isa<ConstDataValVar>(var))
    {
        as[id] = IntervalValue::top();
        return as[id];
    }

    // Dense mode: read from current node's state.
    // Check explicitly — do NOT use as[id] which would insert a default bottom
    // entry via map::operator[]. An absent variable means "uninitialized" → top.
    if (!semiSparse)
    {
        if (as.inVarToValTable(id) || as.inVarToAddrsTable(id))
            return as[id];
        // Fall through to final top fallback
    }

    // Semi-sparse mode: pull from def-site first, then check current state
    const ICFGNode* defNode = var->getICFGNode();
    if (defNode && hasAbstractState(defNode))
    {
        const auto& varMap = getAbstractState(defNode).getVarToVal();
        if (varMap.count(id))
            return getAbstractState(defNode)[id];
    }

    // Fallback for call-result ValVars: their getICFGNode() returns
    // CallICFGNode but the value is written by RetPE at RetICFGNode.
    if (const CallICFGNode* callNode =
            defNode ? SVFUtil::dyn_cast<CallICFGNode>(defNode) : nullptr)
    {
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        if (hasAbstractState(retNode))
        {
            const auto& retMap = getAbstractState(retNode).getVarToVal();
            if (retMap.count(id))
                return getAbstractState(retNode)[id];
        }
    }

    as[id] = IntervalValue::top();
    return as[id];
}

const AbstractValue& AbstractInterpretation::getAbstractValue(const ObjVar* var, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    return as.load(addr);
}

const AbstractValue& AbstractInterpretation::getAbstractValue(const SVFVar* var, const ICFGNode* node)
{
    // Check ObjVar first since ObjVar inherits from ValVar
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        return getAbstractValue(objVar, node);
    if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        return getAbstractValue(valVar, node);
    assert(false && "Unknown SVFVar kind");
    abort();
}

void AbstractInterpretation::updateAbstractValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    as[var->getId()] = val;
}

void AbstractInterpretation::updateAbstractValue(const ObjVar* var, const AbstractValue& val, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
    as.store(addr, val);
}

void AbstractInterpretation::updateAbstractValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node)
{
    // Check ObjVar first since ObjVar inherits from ValVar
    if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        updateAbstractValue(objVar, val, node);
    else if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        updateAbstractValue(valVar, val, node);
    else
        assert(false && "Unknown SVFVar kind");
}

void AbstractInterpretation::getAbstractState(const Set<const ValVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    for (const ValVar* var : vars)
    {
        u32_t id = var->getId();
        result[id] = as[id];
    }
}

void AbstractInterpretation::getAbstractState(const Set<const ObjVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    for (const ObjVar* var : vars)
    {
        u32_t addr = AbstractState::getVirtualMemAddress(var->getId());
        result.store(addr, as.load(addr));
    }
}

void AbstractInterpretation::getAbstractState(const Set<const SVFVar*>& vars, AbstractState& result, const ICFGNode* node)
{
    AbstractState& as = getAbstractState(node);
    for (const SVFVar* var : vars)
    {
        if (const ValVar* valVar = SVFUtil::dyn_cast<ValVar>(var))
        {
            u32_t id = valVar->getId();
            result[id] = as[id];
        }
        else if (const ObjVar* objVar = SVFUtil::dyn_cast<ObjVar>(var))
        {
            u32_t addr = AbstractState::getVirtualMemAddress(objVar->getId());
            result.store(addr, as.load(addr));
        }
    }
}

IntervalValue AbstractInterpretation::getGepElementIndex(const GepStmt* gep, const ICFGNode* node)
{
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantOffset());

    IntervalValue res(0);
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const ValVar* var = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* type = gep->getOffsetVarAndGepTypePairVec()[i].second;

        s64_t idxLb, idxUb;
        if (const ConstIntValVar* constInt = SVFUtil::dyn_cast<ConstIntValVar>(var))
            idxLb = idxUb = constInt->getSExtValue();
        else
        {
            IntervalValue idxItv = getAbstractValue(var, node).getInterval();
            if (idxItv.isBottom())
                idxLb = idxUb = 0;
            else
            {
                idxLb = idxItv.lb().getIntNumeral();
                idxUb = idxItv.ub().getIntNumeral();
            }
        }

        if (SVFUtil::isa<SVFPointerType>(type))
        {
            u32_t elemNum = gep->getAccessPath().getElementNum(gep->getAccessPath().gepSrcPointeeType());
            idxLb = (double)Options::MaxFieldLimit() / elemNum < idxLb ? Options::MaxFieldLimit() : idxLb * elemNum;
            idxUb = (double)Options::MaxFieldLimit() / elemNum < idxUb ? Options::MaxFieldLimit() : idxUb * elemNum;
        }
        else
        {
            if (Options::ModelArrays())
            {
                const std::vector<u32_t>& so = PAG::getPAG()->getTypeInfo(type)->getFlattenedElemIdxVec();
                if (so.empty() || idxUb >= (APOffset)so.size() || idxLb < 0)
                    idxLb = idxUb = 0;
                else
                {
                    idxLb = PAG::getPAG()->getFlattenedElemIdx(type, idxLb);
                    idxUb = PAG::getPAG()->getFlattenedElemIdx(type, idxUb);
                }
            }
            else
                idxLb = idxUb = 0;
        }
        res = res + IntervalValue(idxLb, idxUb);
    }
    res.meet_with(IntervalValue((s64_t)0, (s64_t)Options::MaxFieldLimit()));
    if (res.isBottom())
        res = IntervalValue(0);
    return res;
}

IntervalValue AbstractInterpretation::getGepByteOffset(const GepStmt* gep, const ICFGNode* node)
{
    if (gep->isConstantOffset())
        return IntervalValue((s64_t)gep->accumulateConstantByteOffset());

    IntervalValue res(0);
    for (int i = gep->getOffsetVarAndGepTypePairVec().size() - 1; i >= 0; i--)
    {
        const ValVar* idxOperandVar = gep->getOffsetVarAndGepTypePairVec()[i].first;
        const SVFType* idxOperandType = gep->getOffsetVarAndGepTypePairVec()[i].second;

        if (SVFUtil::isa<SVFArrayType>(idxOperandType) || SVFUtil::isa<SVFPointerType>(idxOperandType))
        {
            u32_t elemByteSize = 1;
            if (const SVFArrayType* arrOperandType = SVFUtil::dyn_cast<SVFArrayType>(idxOperandType))
                elemByteSize = arrOperandType->getTypeOfElement()->getByteSize();
            else if (SVFUtil::isa<SVFPointerType>(idxOperandType))
                elemByteSize = gep->getAccessPath().gepSrcPointeeType()->getByteSize();
            else
                assert(false && "idxOperandType must be ArrType or PtrType");

            if (const ConstIntValVar* op = SVFUtil::dyn_cast<ConstIntValVar>(idxOperandVar))
            {
                s64_t lb = (double)Options::MaxFieldLimit() / elemByteSize >= op->getSExtValue()
                           ? op->getSExtValue() * elemByteSize
                           : Options::MaxFieldLimit();
                res = res + IntervalValue(lb, lb);
            }
            else
            {
                IntervalValue idxVal = getAbstractValue(idxOperandVar, node).getInterval();
                if (idxVal.isBottom())
                    res = res + IntervalValue(0, 0);
                else
                {
                    s64_t ub = (idxVal.ub().getIntNumeral() < 0) ? 0
                               : (double)Options::MaxFieldLimit() / elemByteSize >= idxVal.ub().getIntNumeral()
                               ? elemByteSize * idxVal.ub().getIntNumeral()
                               : Options::MaxFieldLimit();
                    s64_t lb = (idxVal.lb().getIntNumeral() < 0) ? 0
                               : (double)Options::MaxFieldLimit() / elemByteSize >= idxVal.lb().getIntNumeral()
                               ? elemByteSize * idxVal.lb().getIntNumeral()
                               : Options::MaxFieldLimit();
                    res = res + IntervalValue(lb, ub);
                }
            }
        }
        else if (const SVFStructType* structOperandType = SVFUtil::dyn_cast<SVFStructType>(idxOperandType))
        {
            res = res + IntervalValue(gep->getAccessPath().getStructFieldOffset(idxOperandVar, structOperandType));
        }
        else
        {
            assert(false && "gep type pair only support arr/ptr/struct");
        }
    }
    return res;
}

AddressValue AbstractInterpretation::getGepObjAddrs(const SVFVar* pointer, IntervalValue offset, const ICFGNode* node)
{
    AddressValue gepAddrs;
    AbstractState& as = getAbstractState(node);
    APOffset lb = offset.lb().getIntNumeral() < Options::MaxFieldLimit() ? offset.lb().getIntNumeral()
                  : Options::MaxFieldLimit();
    APOffset ub = offset.ub().getIntNumeral() < Options::MaxFieldLimit() ? offset.ub().getIntNumeral()
                  : Options::MaxFieldLimit();
    for (APOffset i = lb; i <= ub; i++)
    {
        const AbstractValue& addrs = getAbstractValue(pointer, node);
        for (const auto& addr : addrs.getAddrs())
        {
            s64_t baseObj = as.getIDFromAddr(addr);
            assert(SVFUtil::isa<ObjVar>(svfir->getSVFVar(baseObj)) && "Fail to get the base object address!");
            NodeID gepObj = svfir->getGepObjVar(baseObj, i);
            as[gepObj] = AddressValue(AbstractState::getVirtualMemAddress(gepObj));
            gepAddrs.insert(AbstractState::getVirtualMemAddress(gepObj));
        }
    }
    return gepAddrs;
}

void AbstractInterpretation::propagateObjVarAbsVal(const ObjVar* var, const ICFGNode* defSite)
{
    const AbstractValue& val = getAbstractValue(var, defSite);
    for (const ICFGNode* useSite : preAnalysis->getUseSitesOfObjVar(var, defSite))
    {
        updateAbstractValue(var, val, useSite);
    }
}


/// Destructor
AbstractInterpretation::~AbstractInterpretation()
{
    delete stat;
    delete preAnalysis;
}

/// Collect entry point functions for analysis.
/// Entry points are functions without callers (no incoming edges in CallGraph).
/// Uses a deque to allow efficient insertion at front for prioritizing main()
std::deque<const FunObjVar*> AbstractInterpretation::collectProgEntryFuns()
{
    std::deque<const FunObjVar*> entryFunctions;

    for (auto it = callGraph->begin(); it != callGraph->end(); ++it)
    {
        const CallGraphNode* cgNode = it->second;
        const FunObjVar* fun = cgNode->getFunction();

        // Skip declarations
        if (fun->isDeclaration())
            continue;

        // Entry points are functions without callers (no incoming edges)
        if (cgNode->getInEdges().empty())
        {
            // If main exists, put it first for priority using deque's push_front
            if (fun->getName() == "main")
            {
                entryFunctions.push_front(fun);
            }
            else
            {
                entryFunctions.push_back(fun);
            }
        }
    }

    return entryFunctions;
}


/// Program entry - analyze from all entry points (multi-entry analysis is the default)
void AbstractInterpretation::analyse()
{
    // Always use multi-entry analysis from all entry points
    analyzeFromAllProgEntries();
}

/// Analyze all entry points (functions without callers) - for whole-program analysis.
/// Abstract state is shared across entry points so that functions analyzed from
/// earlier entries are not re-analyzed from scratch.
void AbstractInterpretation::analyzeFromAllProgEntries()
{
    // Collect all entry point functions
    std::deque<const FunObjVar*> entryFunctions = collectProgEntryFuns();

    if (entryFunctions.empty())
    {
        assert(false && "No entry functions found for analysis");
        return;
    }
    // handle Global ICFGNode of SVFModule
    handleGlobalNode();
    for (const FunObjVar* entryFun : entryFunctions)
    {
        const ICFGNode* funEntry = icfg->getFunEntryICFGNode(entryFun);
        handleFunction(funEntry);
    }
}

/// handle global node
/// Initializes the abstract state for the global ICFG node and processes all global statements.
/// This includes setting up the null pointer and black hole pointer (blkPtr).
/// BlkPtr is initialized to point to the BlackHole object, representing
/// an unknown memory location that cannot be statically resolved.
void AbstractInterpretation::handleGlobalNode()
{
    const ICFGNode* node = icfg->getGlobalICFGNode();
    abstractTrace[node] = AbstractState();
    abstractTrace[node][IRGraph::NullPtr] = AddressValue();

    // Global Node, we just need to handle addr, load, store, copy and gep
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }

    // BlkPtr represents a pointer whose target is statically unknown (e.g., from
    // int2ptr casts, external function returns, or unmodeled instructions like
    // AtomicCmpXchg). It should be an address pointing to the BlackHole object
    // (ID=2), NOT an interval top.
    //
    // History: this was originally set to IntervalValue::top() as a quick fix when
    // the analysis crashed on programs containing uninitialized BlkPtr. However,
    // BlkPtr is semantically a *pointer* (address domain), not a numeric value
    // (interval domain). Setting it to interval top broke cross-domain consistency:
    // the interval domain and address domain gave contradictory information for the
    // same variable. The correct representation is an AddressValue containing the
    // BlackHole virtual address, which means "points to unknown memory".
    abstractTrace[node][PAG::getPAG()->getBlkPtr()] =
        AddressValue(BlackHoleObjAddr);
}

/// Pull-based state merge: for each predecessor that has an abstract state,
/// copy its state, apply branch refinement for conditional IntraCFGEdges,
/// and join all feasible states into abstractTrace[node].
/// In semi-sparse mode, only ObjVar state (AddrToVal) is merged; ValVars are
/// pulled on demand via getAbstractValue from their def-sites.
/// Returns true if at least one predecessor contributed state.
bool AbstractInterpretation::mergeStatesFromPredecessors(const ICFGNode* node)
{
    std::vector<AbstractState> intraWorkList;
    std::vector<AbstractState> interWorkList;
    for (auto& edge : node->getInEdges())
    {
        const ICFGNode* pred = edge->getSrcNode();
        if (abstractTrace.find(pred) == abstractTrace.end())
            continue;

        if (const IntraCFGEdge* intraCfgEdge = SVFUtil::dyn_cast<IntraCFGEdge>(edge))
        {
            AbstractState tmpState = abstractTrace[pred];
            if (intraCfgEdge->getCondition())
            {
                if (isBranchFeasible(intraCfgEdge, tmpState, pred))
                    intraWorkList.push_back(tmpState);
            }
            else
            {
                intraWorkList.push_back(tmpState);
            }
        }
        else if (SVFUtil::isa<CallCFGEdge>(edge))
        {
            AbstractState tmpState = abstractTrace[pred];
            interWorkList.push_back(tmpState);
        }
        else if (SVFUtil::isa<RetCFGEdge>(edge))
        {
            switch (Options::HandleRecur())
            {
            case TOP:
            {
                AbstractState tmpState = abstractTrace[pred];
                interWorkList.push_back(tmpState);
                break;
            }
            case WIDEN_ONLY:
            case WIDEN_NARROW:
            {
                const RetICFGNode* returnSite = SVFUtil::dyn_cast<RetICFGNode>(node);
                const CallICFGNode* callSite = returnSite->getCallICFGNode();
                if (hasAbstractState(callSite))
                {
                    AbstractState tmpState = abstractTrace[pred];
                    interWorkList.push_back(tmpState);
                }
                break;
            }
            }
        }
    }

    if (intraWorkList.empty() && interWorkList.empty())
        return false;

    // joinWith internally skips ValVar merge in semi-sparse mode.
    bool first = true;
    for (auto& state : intraWorkList)
    {
        if (first) { abstractTrace[node] = state; first = false; }
        else abstractTrace[node].joinWith(state);
    }
    for (auto& state : interWorkList)
    {
        if (first) { abstractTrace[node] = state; first = false; }
        else abstractTrace[node].joinWith(state);
    }

    return true;
}


bool AbstractInterpretation::isCmpBranchFeasible(const CmpStmt* cmpStmt, s64_t succ,
        AbstractState& as, const ICFGNode* predNode)
{
    AbstractState new_es = as;
    // get cmp stmt's op0, op1, and predicate
    NodeID op0 = cmpStmt->getOpVarID(0);
    NodeID op1 = cmpStmt->getOpVarID(1);
    NodeID res_id = cmpStmt->getResID();
    // Ensure ValVar operands are in new_es via getAbstractValue
    // (handles semi-sparse def-site lookup and dense uninitialized vars).
    if (!new_es.inVarToValTable(op0) && !new_es.inVarToAddrsTable(op0))
        new_es[op0] = getAbstractValue(cmpStmt->getOpVar(0), predNode);
    if (!new_es.inVarToValTable(op1) && !new_es.inVarToAddrsTable(op1))
        new_es[op1] = getAbstractValue(cmpStmt->getOpVar(1), predNode);
    if (!new_es.inVarToValTable(res_id) && !new_es.inVarToAddrsTable(res_id))
        new_es[res_id] = getAbstractValue(cmpStmt->getRes(), predNode);
    s32_t predicate = cmpStmt->getPredicate();
    // if op0 or op1 is nullptr, no need to change value, just copy the state
    if (op0 == IRGraph::NullPtr || op1 == IRGraph::NullPtr)
    {
        as = new_es;
        return true;
    }
    // if op0 or op1 is undefined, return;
    // skip address compare
    if (new_es.inVarToAddrsTable(op0) || new_es.inVarToAddrsTable(op1))
    {
        as = new_es;
        return true;
    }
    const LoadStmt *load_op0 = nullptr;
    const LoadStmt *load_op1 = nullptr;
    // get '%1 = load i32 s', and load inst may not exist
    const SVFVar* loadVar0 = getSVFVar(op0);
    if (!loadVar0->getInEdges().empty())
    {
        SVFStmt *loadVar0InStmt = *loadVar0->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt))
        {
            load_op0 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar0InStmt))
        {
            loadVar0 = getSVFVar(copyStmt->getRHSVarID());
            if (!loadVar0->getInEdges().empty())
            {
                SVFStmt *loadVar0InStmt2 = *loadVar0->getInEdges().begin();
                if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar0InStmt2))
                {
                    load_op0 = loadStmt;
                }
            }
        }
    }

    const SVFVar* loadVar1 = getSVFVar(op1);
    if (!loadVar1->getInEdges().empty())
    {
        SVFStmt *loadVar1InStmt = *loadVar1->getInEdges().begin();
        if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt))
        {
            load_op1 = loadStmt;
        }
        else if (const CopyStmt *copyStmt = SVFUtil::dyn_cast<CopyStmt>(loadVar1InStmt))
        {
            loadVar1 = getSVFVar(copyStmt->getRHSVarID());
            if (!loadVar1->getInEdges().empty())
            {
                SVFStmt *loadVar1InStmt2 = *loadVar1->getInEdges().begin();
                if (const LoadStmt *loadStmt = SVFUtil::dyn_cast<LoadStmt>(loadVar1InStmt2))
                {
                    load_op1 = loadStmt;
                }
            }
        }
    }
    // for const X const, we may get concrete resVal instantly
    // for var X const, we may get [0,1] if the intersection of var and const is not empty set
    IntervalValue resVal = new_es[res_id].getInterval();
    resVal.meet_with(IntervalValue((s64_t) succ, succ));
    // If Var X const generates bottom value, it means this branch path is not feasible.
    if (resVal.isBottom())
    {
        return false;
    }

    bool b0 = new_es[op0].getInterval().is_numeral();
    bool b1 = new_es[op1].getInterval().is_numeral();

    // if const X var, we should reverse op0 and op1.
    if (b0 && !b1)
    {
        std::swap(op0, op1);
        std::swap(load_op0, load_op1);
        predicate = _switch_lhsrhs_predicate[predicate];
    }
    else
    {
        // if var X var, we cannot preset the branch condition to infer the intervals of var0,var1
        if (!b0 && !b1)
        {
            as = new_es;
            return true;
        }
        // if const X const, we can instantly get the resVal
        else if (b0 && b1)
        {
            as = new_es;
            return true;
        }
    }
    // if cmp is 'var X const == false', we should reverse predicate 'var X' const == true'
    // X' is reverse predicate of X
    if (succ == 0)
    {
        predicate = _reverse_predicate[predicate];
    }
    else {}
    // change interval range according to the compare predicate
    AddressValue addrs;
    if(load_op0 && new_es.inVarToAddrsTable(load_op0->getRHSVarID()))
        addrs = new_es[load_op0->getRHSVarID()].getAddrs();

    IntervalValue &lhs = new_es[op0].getInterval(), &rhs = new_es[op1].getInterval();
    switch (predicate)
    {
    case CmpStmt::Predicate::ICMP_EQ:
    case CmpStmt::Predicate::FCMP_OEQ:
    case CmpStmt::Predicate::FCMP_UEQ:
    {
        // Var == Const, so [var.lb, var.ub].meet_with(const)
        lhs.meet_with(rhs);
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(rhs);
            }
        }
        break;
    }
    case CmpStmt::Predicate::ICMP_NE:
    case CmpStmt::Predicate::FCMP_ONE:
    case CmpStmt::Predicate::FCMP_UNE:
        // Compliment set
        break;
    case CmpStmt::Predicate::ICMP_UGT:
    case CmpStmt::Predicate::ICMP_SGT:
    case CmpStmt::Predicate::FCMP_OGT:
    case CmpStmt::Predicate::FCMP_UGT:
        // Var > Const, so [var.lb, var.ub].meet_with([Const+1, +INF])
        lhs.meet_with(IntervalValue(rhs.lb() + 1, IntervalValue::plus_infinity()));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(rhs.lb() + 1, IntervalValue::plus_infinity()));
            }
        }
        break;
    case CmpStmt::Predicate::ICMP_UGE:
    case CmpStmt::Predicate::ICMP_SGE:
    case CmpStmt::Predicate::FCMP_OGE:
    case CmpStmt::Predicate::FCMP_UGE:
    {
        // Var >= Const, so [var.lb, var.ub].meet_with([Const, +INF])
        lhs.meet_with(IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(rhs.lb(), IntervalValue::plus_infinity()));
            }
        }
        break;
    }
    case CmpStmt::Predicate::ICMP_ULT:
    case CmpStmt::Predicate::ICMP_SLT:
    case CmpStmt::Predicate::FCMP_OLT:
    case CmpStmt::Predicate::FCMP_ULT:
    {
        // Var < Const, so [var.lb, var.ub].meet_with([-INF, const.ub-1])
        lhs.meet_with(IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(IntervalValue::minus_infinity(), rhs.ub() - 1));
            }
        }
        break;
    }
    case CmpStmt::Predicate::ICMP_ULE:
    case CmpStmt::Predicate::ICMP_SLE:
    case CmpStmt::Predicate::FCMP_OLE:
    case CmpStmt::Predicate::FCMP_ULE:
    {
        // Var <= Const, so [var.lb, var.ub].meet_with([-INF, const.ub])
        lhs.meet_with(IntervalValue(IntervalValue::minus_infinity(), rhs.ub()));
        // if lhs is register value, we should also change its mem obj
        for (const auto &addr: addrs)
        {
            NodeID objId = new_es.getIDFromAddr(addr);
            if (new_es.inAddrToValTable(objId))
            {
                new_es.load(addr).meet_with(
                    IntervalValue(IntervalValue::minus_infinity(), rhs.ub()));
            }
        }
        break;
    }
    case CmpStmt::Predicate::FCMP_FALSE:
        break;
    case CmpStmt::Predicate::FCMP_TRUE:
        break;
    default:
        assert(false && "implement this part");
        abort();
    }
    as = new_es;
    return true;
}

bool AbstractInterpretation::isSwitchBranchFeasible(const SVFVar* var, s64_t succ,
        AbstractState& as, const ICFGNode* predNode)
{
    AbstractState new_es = as;
    if (!new_es.inVarToValTable(var->getId()) && !new_es.inVarToAddrsTable(var->getId()))
        new_es[var->getId()] = getAbstractValue(var, predNode);
    IntervalValue& switch_cond = new_es[var->getId()].getInterval();
    s64_t value = succ;
    FIFOWorkList<const SVFStmt*> workList;
    for (SVFStmt *cmpVarInStmt: var->getInEdges())
    {
        workList.push(cmpVarInStmt);
    }
    switch_cond.meet_with(IntervalValue(value, value));
    if (switch_cond.isBottom())
    {
        return false;
    }
    while(!workList.empty())
    {
        const SVFStmt* stmt = workList.pop();
        if (SVFUtil::isa<CopyStmt>(stmt))
        {
            IntervalValue& copy_cond = new_es[var->getId()].getInterval();
            copy_cond.meet_with(IntervalValue(value, value));
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            if (new_es.inVarToAddrsTable(load->getRHSVarID()))
            {
                AddressValue &addrs = new_es[load->getRHSVarID()].getAddrs();
                for (const auto &addr: addrs)
                {
                    NodeID objId = new_es.getIDFromAddr(addr);
                    if (new_es.inAddrToValTable(objId))
                    {
                        new_es.load(addr).meet_with(switch_cond);
                    }
                }
            }
        }
    }
    as = new_es;
    return true;
}

bool AbstractInterpretation::isBranchFeasible(const IntraCFGEdge* intraEdge,
        AbstractState& as, const ICFGNode* predNode)
{
    const SVFVar *cmpVar = intraEdge->getCondition();
    if (cmpVar->getInEdges().empty())
    {
        return isSwitchBranchFeasible(cmpVar,
                                      intraEdge->getSuccessorCondValue(), as, predNode);
    }
    else
    {
        assert(!cmpVar->getInEdges().empty() &&
               "no in edges?");
        SVFStmt *cmpVarInStmt = *cmpVar->getInEdges().begin();
        if (const CmpStmt *cmpStmt = SVFUtil::dyn_cast<CmpStmt>(cmpVarInStmt))
        {
            return isCmpBranchFeasible(cmpStmt,
                                       intraEdge->getSuccessorCondValue(), as, predNode);
        }
        else
        {
            return isSwitchBranchFeasible(
                       cmpVar, intraEdge->getSuccessorCondValue(), as, predNode);
        }
    }
    return true;
}
/**
 * Handle an ICFG node: execute statements on the current abstract state.
 * The node's pre-state must already be in abstractTrace (set by
 * mergeStatesFromPredecessors, or by handleGlobalNode for the global node).
 * Returns true if the abstract state has changed, false if fixpoint reached or unreachable.
 */
bool AbstractInterpretation::handleICFGNode(const ICFGNode* node)
{
    // Check reachability: pre-state must have been propagated by predecessors
    bool isFunEntry = SVFUtil::isa<FunEntryICFGNode>(node);
    if (!hasAbstractState(node))
    {
        if (isFunEntry)
        {
            // Entry point with no callers: inherit from global node
            const ICFGNode* globalNode = icfg->getGlobalICFGNode();
            if (hasAbstractState(globalNode))
                abstractTrace[node] = abstractTrace[globalNode];
            else
                abstractTrace[node] = AbstractState();
        }
        else
        {
            return false;  // unreachable node
        }
    }

    // Store the previous state for fixpoint detection
    AbstractState prevState = abstractTrace[node];

    stat->getBlockTrace()++;
    stat->getICFGNodeTrace()++;

    // Handle SVF statements
    for (const SVFStmt *stmt: node->getSVFStmts())
    {
        handleSVFStatement(stmt);
    }

    // Handle call sites
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        handleCallSite(callNode);
    }

    // Run detectors
    for (auto& detector: detectors)
        detector->detect(*this, node);
    stat->countStateSize();

    // Track this node as analyzed (for coverage statistics across all entry points)
    allAnalyzedNodes.insert(node);

    if (abstractTrace[node] == prevState)
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
    if (it == preAnalysis->getFuncToWTO().end())
        return;

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
        assert (false && "it is not call node");
}

bool AbstractInterpretation::isExtCall(const CallICFGNode *callNode)
{
    return SVFUtil::isExtCall(callNode->getCalledFunction());
}

void AbstractInterpretation::handleExtCall(const CallICFGNode *callNode)
{
    utils->handleExtAPI(callNode);
    for (auto& detector : detectors)
    {
        detector->handleStubFunctions(callNode);
    }
}

/// Check if a function is recursive (part of a call graph SCC)
bool AbstractInterpretation::isRecursiveFun(const FunObjVar* fun)
{
    return preAnalysis->getPointerAnalysis()->isInRecursion(fun);
}

/// Handle recursive call in TOP mode: set all stores and return value to TOP
void AbstractInterpretation::handleRecursiveCall(const CallICFGNode *callNode)
{
    AbstractState& as = getAbstractState(callNode);
    setTopToObjInRecursion(callNode);
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() &&
                    !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
            {
                updateAbstractValue(retPE->getLHSVar(), IntervalValue::top(), callNode);
            }
        }
    }
    abstractTrace[retNode] = as;
}

/// Check if caller and callee are in the same CallGraph SCC (i.e. a recursive callsite)
bool AbstractInterpretation::isRecursiveCallSite(const CallICFGNode* callNode,
        const FunObjVar* callee)
{
    const FunObjVar* caller = callNode->getCaller();
    return preAnalysis->getPointerAnalysis()->inSameCallGraphSCC(caller, callee);
}

/// Get callee function: directly for direct calls, via pointer analysis for indirect calls
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
    if (!hasAbstractState(callNode))
        return nullptr;

    AbstractState& as = getAbstractState(callNode);
    if (!as.inVarToAddrsTable(call_id))
        return nullptr;

    AbstractValue Addrs = getAbstractValue(svfir->getSVFVar(call_id), callNode);
    if (Addrs.getAddrs().empty())
        return nullptr;

    NodeID addr = *Addrs.getAddrs().begin();
    const SVFVar* func_var = getSVFVar(as.getIDFromAddr(addr));
    return SVFUtil::dyn_cast<FunObjVar>(func_var);
}

/// Skip recursive callsites (within SCC); entry calls from outside SCC are not skipped
bool AbstractInterpretation::skipRecursiveCall(const CallICFGNode* callNode)
{
    const FunObjVar* callee = getCallee(callNode);
    if (!callee)
        return false;

    // Non-recursive function: never skip, always inline
    if (!isRecursiveFun(callee))
        return false;

    // For recursive functions, skip only recursive callsites (within same SCC).
    // Entry calls (from outside SCC) are not skipped - they are inlined so that
    // handleLoopOrRecursion() can analyze the function body.
    // This applies uniformly to all modes (TOP/WIDEN_ONLY/WIDEN_NARROW).
    return isRecursiveCallSite(callNode, callee);
}

/// Check if narrowing should be applied: always for regular loops, mode-dependent for recursion
bool AbstractInterpretation::shouldApplyNarrowing(const FunObjVar* fun)
{
    // Non-recursive functions (regular loops): always apply narrowing
    if (!isRecursiveFun(fun))
        return true;

    // Recursive functions: WIDEN_NARROW applies narrowing, WIDEN_ONLY does not
    // TOP mode exits early in handleLoopOrRecursion, so should not reach here
    switch (Options::HandleRecur())
    {
    case TOP:
        assert(false && "TOP mode should not reach narrowing phase for recursive functions");
        return false;
    case WIDEN_ONLY:
        return false;  // Skip narrowing for recursive functions
    case WIDEN_NARROW:
        return true;   // Apply narrowing for recursive functions
    default:
        assert(false && "Unknown recursion handling mode");
        return false;
    }
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
    AbstractState& as = getAbstractState(callNode);
    abstractTrace[callNode] = as;
    if (skipRecursiveCall(callNode))
        return;

    // Direct call: callee is known
    if (const FunObjVar* callee = callNode->getCalledFunction())
    {
        const ICFGNode* calleeEntry = icfg->getFunEntryICFGNode(callee);
        handleFunction(calleeEntry, callNode);
        const RetICFGNode* retNode = callNode->getRetICFGNode();
        abstractTrace[retNode] = abstractTrace[callNode];
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
    abstractTrace[retNode] = abstractTrace[callNode];
}

/// Handle WTO cycle (loop or recursive function) using widening/narrowing iteration.
///
/// Widening is applied at the cycle head to ensure termination of the analysis.
/// The cycle head's abstract state is iteratively updated until a fixpoint is reached.
///
/// == What is being widened ==
/// The abstract state at the cycle head node, which includes:
/// - Variable values (intervals) that may change across loop iterations
/// - For example, a loop counter `i` starting at 0 and incrementing each iteration
///
/// == Regular loops (non-recursive functions) ==
/// All modes (TOP/WIDEN_ONLY/WIDEN_NARROW) behave the same for regular loops:
/// 1. Widening phase: Iterate until the cycle head state stabilizes
///    Example: for(i=0; i<100; i++) -> i widens to [0, +inf]
/// 2. Narrowing phase: Refine the over-approximation from widening
///    Example: [0, +inf] narrows to [0, 100] using loop condition
///
/// == Recursive function cycles ==
/// Behavior depends on Options::HandleRecur():
///
/// - TOP mode:
///     Does not iterate. Calls handleRecursiveCall() to set all stores and
///     return value to TOP immediately. This is the most conservative but fastest.
///     Example:
///       int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
///       factorial(5) -> returns [-inf, +inf]
///
/// - WIDEN_ONLY mode:
///     Widening phase only, no narrowing for recursive functions.
///     The recursive function body is analyzed with widening until fixpoint.
///     Example:
///       int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
///       factorial(5) -> returns [10000, +inf] (widened upper bound)
///
/// - WIDEN_NARROW mode:
///     Both widening and narrowing phases for recursive functions.
///     After widening reaches fixpoint, narrowing refines the result.
///     Example:
///       int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
///       factorial(5) -> returns [10000, 10000] (precise after narrowing)
void AbstractInterpretation::handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller)
{
    const ICFGNode* cycle_head = cycle->head()->getICFGNode();

    // TOP mode for recursive function cycles: set all stores and return value to TOP
    if (Options::HandleRecur() == TOP && isRecursiveFun(cycle_head->getFun()))
    {
        if (caller)
            handleRecursiveCall(caller);
        return;
    }

    // Iterate until fixpoint with widening/narrowing on the cycle head.
    bool increasing = true;
    u32_t widen_delay = Options::WidenDelay();
    for (u32_t cur_iter = 0;; cur_iter++)
    {
        if (cur_iter >= widen_delay)
        {
            // Save state before processing head
            AbstractState prev_head_state = abstractTrace[cycle_head];

            // Process cycle head: merge from predecessors, then execute statements
            // (uses same gated pattern as handleWTOComponent in origin/master)
            if (mergeStatesFromPredecessors(cycle_head))
                handleICFGNode(cycle_head);
            AbstractState cur_head_state = abstractTrace[cycle_head];

            if (increasing)
            {
                abstractTrace[cycle_head] = prev_head_state.widening(cur_head_state);
                if (abstractTrace[cycle_head] == prev_head_state)
                {
                    // Widening fixpoint reached; switch to narrowing phase.
                    increasing = false;
                    continue;
                }
            }
            else
            {
                if (!shouldApplyNarrowing(cycle_head->getFun()))
                    break;
                abstractTrace[cycle_head] = prev_head_state.narrowing(cur_head_state);
                if (abstractTrace[cycle_head] == prev_head_state)
                    break;
            }
        }
        else
        {
            // Before widen_delay: process cycle head with gated pattern
            if (mergeStatesFromPredecessors(cycle_head))
                handleICFGNode(cycle_head);
        }

        // Process cycle body components (each with gated merge+handle)
        for (const ICFGWTOComp* comp : cycle->getWTOComponents())
        {
            if (const ICFGSingletonWTO* singleton = SVFUtil::dyn_cast<ICFGSingletonWTO>(comp))
            {
                const ICFGNode* node = singleton->getICFGNode();
                if (mergeStatesFromPredecessors(node))
                    handleICFGNode(node);
            }
            else if (const ICFGCycleWTO* subCycle = SVFUtil::dyn_cast<ICFGCycleWTO>(comp))
            {
                if (mergeStatesFromPredecessors(subCycle->head()->getICFGNode()))
                    handleLoopOrRecursion(subCycle, caller);
            }
        }
    }
}

void AbstractInterpretation::handleSVFStatement(const SVFStmt *stmt)
{
    if (const AddrStmt *addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
    {
        updateStateOnAddr(addr);
    }
    else if (const BinaryOPStmt *binary = SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
    {
        updateStateOnBinary(binary);
    }
    else if (const CmpStmt *cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
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
    else if (const LoadStmt *load = SVFUtil::dyn_cast<LoadStmt>(stmt))
    {
        updateStateOnLoad(load);
    }
    else if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
    {
        updateStateOnStore(store);
    }
    else if (const CopyStmt *copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
    {
        updateStateOnCopy(copy);
    }
    else if (const GepStmt *gep = SVFUtil::dyn_cast<GepStmt>(stmt))
    {
        updateStateOnGep(gep);
    }
    else if (const SelectStmt *select = SVFUtil::dyn_cast<SelectStmt>(stmt))
    {
        updateStateOnSelect(select);
    }
    else if (const PhiStmt *phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
    {
        updateStateOnPhi(phi);
    }
    else if (const CallPE *callPE = SVFUtil::dyn_cast<CallPE>(stmt))
    {
        // To handle Call Edge
        updateStateOnCall(callPE);
    }
    else if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(stmt))
    {
        updateStateOnRet(retPE);
    }
    else
        assert(false && "implement this part");
    // NullPtr is index 0, it should not be changed
    assert(!getAbstractState(stmt->getICFGNode())[IRGraph::NullPtr].isInterval() &&
           !getAbstractState(stmt->getICFGNode())[IRGraph::NullPtr].isAddr());
}

/// Set all store values in a recursive function to TOP (used in TOP mode)
void AbstractInterpretation::setTopToObjInRecursion(const CallICFGNode *callNode)
{
    const RetICFGNode *retNode = callNode->getRetICFGNode();
    if (retNode->getSVFStmts().size() > 0)
    {
        if (const RetPE *retPE = SVFUtil::dyn_cast<RetPE>(*retNode->getSVFStmts().begin()))
        {
            if (!retPE->getLHSVar()->isPointer() && !retPE->getLHSVar()->isConstDataOrAggDataButNotNullPtr())
                updateAbstractValue(retPE->getLHSVar(), IntervalValue::top(), callNode);
        }
    }
    if (!retNode->getOutEdges().empty())
    {
        if (retNode->getOutEdges().size() == 1)
        {

        }
        else
        {
            return;
        }
    }
    for (const SVFBasicBlock * bb: callNode->getCalledFunction()->getReachableBBs())
    {
        for (const ICFGNode* node: bb->getICFGNodeList())
        {
            for (const SVFStmt *stmt: node->getSVFStmts())
            {
                if (const StoreStmt *store = SVFUtil::dyn_cast<StoreStmt>(stmt))
                {
                    const SVFVar *rhsVar = store->getRHSVar();
                    if (!rhsVar->isPointer() && !rhsVar->isConstDataOrAggDataButNotNullPtr())
                    {
                        const AbstractValue& addrs = getAbstractValue(store->getLHSVar(), callNode);
                        if (addrs.isAddr())
                        {
                            AbstractState& as = getAbstractState(callNode);
                            for (const auto &addr: addrs.getAddrs())
                                as.store(addr, IntervalValue::top());
                        }
                    }
                }
            }
        }
    }
}

void AbstractInterpretation::updateStateOnGep(const GepStmt *gep)
{
    const ICFGNode* node = gep->getICFGNode();
    IntervalValue offsetPair = getGepElementIndex(gep, node);
    AddressValue gepAddrs = getGepObjAddrs(gep->getRHSVar(), offsetPair, node);
    updateAbstractValue(gep->getLHSVar(), gepAddrs, node);
}

void AbstractInterpretation::updateStateOnSelect(const SelectStmt *select)
{
    const ICFGNode* node = select->getICFGNode();
    const AbstractValue& condVal = getAbstractValue(select->getCondition(), node);
    const AbstractValue& tVal = getAbstractValue(select->getTrueValue(), node);
    const AbstractValue& fVal = getAbstractValue(select->getFalseValue(), node);
    AbstractValue resVal;
    if (condVal.getInterval().is_numeral())
    {
        resVal = condVal.getInterval().is_zero() ? fVal : tVal;
    }
    else
    {
        resVal = tVal;
        resVal.join_with(fVal);
    }
    updateAbstractValue(select->getRes(), resVal, node);
}

void AbstractInterpretation::updateStateOnPhi(const PhiStmt *phi)
{
    const ICFGNode* icfgNode = phi->getICFGNode();
    AbstractValue rhs;
    for (u32_t i = 0; i < phi->getOpVarNum(); i++)
    {
        const ICFGNode* opICFGNode = phi->getOpICFGNode(i);
        if (hasAbstractState(opICFGNode))
        {
            AbstractState tmpEs = abstractTrace[opICFGNode];
            const ICFGEdge* edge = icfg->getICFGEdge(opICFGNode, icfgNode, ICFGEdge::IntraCF);

            // Read phi operand via getAbstractValue from the predecessor node.
            // Dense: reads from abstractTrace[opICFGNode] (same as tmpEs).
            // Semi-sparse: falls through to def-site if not in predecessor state.
            AbstractValue opVal = getAbstractValue(phi->getOpVar(i), opICFGNode);

            if (edge)
            {
                const IntraCFGEdge* intraEdge = SVFUtil::cast<IntraCFGEdge>(edge);
                if (intraEdge->getCondition())
                {
                    if (isBranchFeasible(intraEdge, tmpEs, opICFGNode))
                        rhs.join_with(opVal);
                }
                else
                    rhs.join_with(opVal);
            }
            else
            {
                rhs.join_with(opVal);
            }
        }
    }
    updateAbstractValue(phi->getRes(), rhs, icfgNode);
}


void AbstractInterpretation::updateStateOnCall(const CallPE *callPE)
{
    const ICFGNode* node = callPE->getICFGNode();
    const AbstractValue& rhsVal = getAbstractValue(callPE->getRHSVar(), node);
    updateAbstractValue(callPE->getLHSVar(), rhsVal, node);
}

void AbstractInterpretation::updateStateOnRet(const RetPE *retPE)
{
    const ICFGNode* node = retPE->getICFGNode();
    const AbstractValue& rhsVal = getAbstractValue(retPE->getRHSVar(), node);
    updateAbstractValue(retPE->getLHSVar(), rhsVal, node);
}


void AbstractInterpretation::updateStateOnAddr(const AddrStmt *addr)
{
    const ICFGNode* node = addr->getICFGNode();
    AbstractState& as = getAbstractState(node);
    as.initObjVar(SVFUtil::cast<ObjVar>(addr->getRHSVar()));
    // AddrStmt: lhs(ValVar) = &rhs(ObjVar).
    // as[rhsId] stores the ObjVar's virtual address in _varToVal,
    // NOT the object contents. So we must use as[] directly for ObjVar.
    u32_t rhsId = addr->getRHSVarID();
    if (addr->getRHSVar()->getType()->getKind() == SVFType::SVFIntegerTy)
        as[rhsId].getInterval().meet_with(utils->getRangeLimitFromType(addr->getRHSVar()->getType()));
    // LHS is a ValVar (pointer), write through the API
    updateAbstractValue(addr->getLHSVar(), as[rhsId], node);
}


void AbstractInterpretation::updateStateOnBinary(const BinaryOPStmt *binary)
{
    const ICFGNode* node = binary->getICFGNode();
    // Treat bottom (uninitialized) operands as top for soundness
    const AbstractValue& op0Val = getAbstractValue(binary->getOpVar(0), node);
    const AbstractValue& op1Val = getAbstractValue(binary->getOpVar(1), node);
    IntervalValue lhs = op0Val.getInterval().isBottom() ? IntervalValue::top() : op0Val.getInterval();
    IntervalValue rhs = op1Val.getInterval().isBottom() ? IntervalValue::top() : op1Val.getInterval();
    IntervalValue resVal;
    switch (binary->getOpcode())
    {
    case BinaryOPStmt::Add:
    case BinaryOPStmt::FAdd:
        resVal = (lhs + rhs);
        break;
    case BinaryOPStmt::Sub:
    case BinaryOPStmt::FSub:
        resVal = (lhs - rhs);
        break;
    case BinaryOPStmt::Mul:
    case BinaryOPStmt::FMul:
        resVal = (lhs * rhs);
        break;
    case BinaryOPStmt::SDiv:
    case BinaryOPStmt::FDiv:
    case BinaryOPStmt::UDiv:
        resVal = (lhs / rhs);
        break;
    case BinaryOPStmt::SRem:
    case BinaryOPStmt::FRem:
    case BinaryOPStmt::URem:
        resVal = (lhs % rhs);
        break;
    case BinaryOPStmt::Xor:
        resVal = (lhs ^ rhs);
        break;
    case BinaryOPStmt::And:
        resVal = (lhs & rhs);
        break;
    case BinaryOPStmt::Or:
        resVal = (lhs | rhs);
        break;
    case BinaryOPStmt::AShr:
        resVal = (lhs >> rhs);
        break;
    case BinaryOPStmt::Shl:
        resVal = (lhs << rhs);
        break;
    case BinaryOPStmt::LShr:
        resVal = (lhs >> rhs);
        break;
    default:
        assert(false && "undefined binary: ");
    }
    updateAbstractValue(binary->getRes(), resVal, node);
}

void AbstractInterpretation::updateStateOnCmp(const CmpStmt *cmp)
{
    const ICFGNode* node = cmp->getICFGNode();
    u32_t op0 = cmp->getOpVarID(0);
    u32_t op1 = cmp->getOpVarID(1);
    const AbstractValue& op0Val = getAbstractValue(cmp->getOpVar(0), node);
    const AbstractValue& op1Val = getAbstractValue(cmp->getOpVar(1), node);

    // if it is address
    if (op0Val.isAddr() && op1Val.isAddr())
    {
        IntervalValue resVal;
        AddressValue addrOp0 = op0Val.getAddrs();
        AddressValue addrOp1 = op1Val.getAddrs();
        if (addrOp0.equals(addrOp1))
        {
            resVal = IntervalValue(1, 1);
        }
        else if (addrOp0.hasIntersect(addrOp1))
        {
            resVal = IntervalValue(0, 1);
        }
        else
        {
            resVal = IntervalValue(0, 0);
        }
        updateAbstractValue(cmp->getRes(), resVal, node);
    }
    // if op0 or op1 is nullptr, compare abstractValue instead of touching addr or interval
    else if (op0 == IRGraph::NullPtr || op1 == IRGraph::NullPtr)
    {
        IntervalValue resVal = (op0Val.equals(op1Val)) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        updateAbstractValue(cmp->getRes(), resVal, node);
    }
    else
    {
        {
            IntervalValue resVal;
            if (op0Val.isInterval() && op1Val.isInterval())
            {
                // Treat bottom (uninitialized) operands as top for soundness
                IntervalValue lhs = op0Val.getInterval().isBottom() ? IntervalValue::top() : op0Val.getInterval(),
                              rhs = op1Val.getInterval().isBottom() ? IntervalValue::top() : op1Val.getInterval();
                // AbstractValue
                auto predicate = cmp->getPredicate();
                switch (predicate)
                {
                case CmpStmt::ICMP_EQ:
                case CmpStmt::FCMP_OEQ:
                case CmpStmt::FCMP_UEQ:
                    resVal = (lhs == rhs);
                    // resVal = (lhs.getInterval() == rhs.getInterval());
                    break;
                case CmpStmt::ICMP_NE:
                case CmpStmt::FCMP_ONE:
                case CmpStmt::FCMP_UNE:
                    resVal = (lhs != rhs);
                    break;
                case CmpStmt::ICMP_UGT:
                case CmpStmt::ICMP_SGT:
                case CmpStmt::FCMP_OGT:
                case CmpStmt::FCMP_UGT:
                    resVal = (lhs > rhs);
                    break;
                case CmpStmt::ICMP_UGE:
                case CmpStmt::ICMP_SGE:
                case CmpStmt::FCMP_OGE:
                case CmpStmt::FCMP_UGE:
                    resVal = (lhs >= rhs);
                    break;
                case CmpStmt::ICMP_ULT:
                case CmpStmt::ICMP_SLT:
                case CmpStmt::FCMP_OLT:
                case CmpStmt::FCMP_ULT:
                    resVal = (lhs < rhs);
                    break;
                case CmpStmt::ICMP_ULE:
                case CmpStmt::ICMP_SLE:
                case CmpStmt::FCMP_OLE:
                case CmpStmt::FCMP_ULE:
                    resVal = (lhs <= rhs);
                    break;
                case CmpStmt::FCMP_FALSE:
                    resVal = IntervalValue(0, 0);
                    break;
                case CmpStmt::FCMP_TRUE:
                    resVal = IntervalValue(1, 1);
                    break;
                case CmpStmt::FCMP_ORD:
                case CmpStmt::FCMP_UNO:
                    // FCMP_ORD: true if both operands are not NaN
                    // FCMP_UNO: true if either operand is NaN
                    // Conservatively return [0, 1] since we don't track NaN
                    resVal = IntervalValue(0, 1);
                    break;
                default:
                    assert(false && "undefined compare: ");
                }
                updateAbstractValue(cmp->getRes(), resVal, node);
            }
            else if (op0Val.isAddr() && op1Val.isAddr())
            {
                AddressValue lhs = op0Val.getAddrs(),
                             rhs = op1Val.getAddrs();
                auto predicate = cmp->getPredicate();
                switch (predicate)
                {
                case CmpStmt::ICMP_EQ:
                case CmpStmt::FCMP_OEQ:
                case CmpStmt::FCMP_UEQ:
                {
                    if (lhs.hasIntersect(rhs))
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    else if (lhs.empty() && rhs.empty())
                    {
                        resVal = IntervalValue(1, 1);
                    }
                    else
                    {
                        resVal = IntervalValue(0, 0);
                    }
                    break;
                }
                case CmpStmt::ICMP_NE:
                case CmpStmt::FCMP_ONE:
                case CmpStmt::FCMP_UNE:
                {
                    if (lhs.hasIntersect(rhs))
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    else if (lhs.empty() && rhs.empty())
                    {
                        resVal = IntervalValue(0, 0);
                    }
                    else
                    {
                        resVal = IntervalValue(1, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_UGT:
                case CmpStmt::ICMP_SGT:
                case CmpStmt::FCMP_OGT:
                case CmpStmt::FCMP_UGT:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() > *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_UGE:
                case CmpStmt::ICMP_SGE:
                case CmpStmt::FCMP_OGE:
                case CmpStmt::FCMP_UGE:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() >= *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_ULT:
                case CmpStmt::ICMP_SLT:
                case CmpStmt::FCMP_OLT:
                case CmpStmt::FCMP_ULT:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() < *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::ICMP_ULE:
                case CmpStmt::ICMP_SLE:
                case CmpStmt::FCMP_OLE:
                case CmpStmt::FCMP_ULE:
                {
                    if (lhs.size() == 1 && rhs.size() == 1)
                    {
                        resVal = IntervalValue(*lhs.begin() <= *rhs.begin());
                    }
                    else
                    {
                        resVal = IntervalValue(0, 1);
                    }
                    break;
                }
                case CmpStmt::FCMP_FALSE:
                    resVal = IntervalValue(0, 0);
                    break;
                case CmpStmt::FCMP_TRUE:
                    resVal = IntervalValue(1, 1);
                    break;
                case CmpStmt::FCMP_ORD:
                case CmpStmt::FCMP_UNO:
                    // FCMP_ORD: true if both operands are not NaN
                    // FCMP_UNO: true if either operand is NaN
                    // Conservatively return [0, 1] since we don't track NaN
                    resVal = IntervalValue(0, 1);
                    break;
                default:
                    assert(false && "undefined compare: ");
                }
                updateAbstractValue(cmp->getRes(), resVal, node);
            }
        }
    }
}

void AbstractInterpretation::updateStateOnLoad(const LoadStmt *load)
{
    const ICFGNode* node = load->getICFGNode();
    // Step 1: get pointer's address set (ValVar, may be at def-site in semi-sparse)
    const AbstractValue& ptrVal = getAbstractValue(load->getRHSVar(), node);
    // Step 2: load from each addr (ObjVar, always dense)
    AbstractState& as = getAbstractState(node);
    AbstractValue res;
    for (auto addr : ptrVal.getAddrs())
        res.join_with(as.load(addr));
    // Step 3: write result to lhs (ValVar)
    updateAbstractValue(load->getLHSVar(), res, node);
}

void AbstractInterpretation::updateStateOnStore(const StoreStmt *store)
{
    const ICFGNode* node = store->getICFGNode();
    // Step 1: get pointer's address set (ValVar)
    const AbstractValue& ptrVal = getAbstractValue(store->getLHSVar(), node);
    // Step 2: get rhs value (ValVar)
    const AbstractValue& rhsVal = getAbstractValue(store->getRHSVar(), node);
    // Step 3: store to each addr (ObjVar, always dense)
    AbstractState& as = getAbstractState(node);
    for (auto addr : ptrVal.getAddrs())
        as.store(addr, rhsVal);
}

void AbstractInterpretation::updateStateOnCopy(const CopyStmt *copy)
{
    const ICFGNode* node = copy->getICFGNode();
    const SVFVar* lhsVar = copy->getLHSVar();
    const SVFVar* rhsVar = copy->getRHSVar();

    auto getZExtValue = [&](const SVFVar* var)
    {
        const SVFType* type = var->getType();
        if (SVFUtil::isa<SVFIntegerType>(type))
        {
            u32_t bits = type->getByteSize() * 8;
            const AbstractValue& val = getAbstractValue(var, node);
            if (val.getInterval().is_numeral())
            {
                if (bits == 8)
                {
                    int8_t signed_i8_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<uint8_t>(signed_i8_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 16)
                {
                    s16_t signed_i16_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u16_t>(signed_i16_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 32)
                {
                    s32_t signed_i32_value = val.getInterval().getIntNumeral();
                    u32_t unsigned_value = static_cast<u32_t>(signed_i32_value);
                    return IntervalValue(unsigned_value, unsigned_value);
                }
                else if (bits == 64)
                {
                    s64_t signed_i64_value = val.getInterval().getIntNumeral();
                    return IntervalValue((s64_t)signed_i64_value, (s64_t)signed_i64_value);
                }
                else
                    assert(false && "cannot support int type other than u8/16/32/64");
            }
            else
            {
                return IntervalValue::top();
            }
        }
        return IntervalValue::top();
    };

    auto getTruncValue = [&](const SVFVar* var, const SVFType* dstType)
    {
        const IntervalValue& itv = getAbstractValue(var, node).getInterval();
        if(itv.isBottom()) return itv;
        s64_t int_lb = itv.lb().getIntNumeral();
        s64_t int_ub = itv.ub().getIntNumeral();
        u32_t dst_bits = dstType->getByteSize() * 8;
        if (dst_bits == 8)
        {
            int8_t s8_lb = static_cast<int8_t>(int_lb);
            int8_t s8_ub = static_cast<int8_t>(int_ub);
            if (s8_lb > s8_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s8_lb, s8_ub);
        }
        else if (dst_bits == 16)
        {
            s16_t s16_lb = static_cast<s16_t>(int_lb);
            s16_t s16_ub = static_cast<s16_t>(int_ub);
            if (s16_lb > s16_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s16_lb, s16_ub);
        }
        else if (dst_bits == 32)
        {
            s32_t s32_lb = static_cast<s32_t>(int_lb);
            s32_t s32_ub = static_cast<s32_t>(int_ub);
            if (s32_lb > s32_ub)
                return utils->getRangeLimitFromType(dstType);
            return IntervalValue(s32_lb, s32_ub);
        }
        else
        {
            assert(false && "cannot support dst int type other than u8/16/32");
            abort();
        }
    };

    const AbstractValue& rhsVal = getAbstractValue(rhsVar, node);

    if (copy->getCopyKind() == CopyStmt::COPYVAL)
    {
        updateAbstractValue(lhsVar, rhsVal, node);
    }
    else if (copy->getCopyKind() == CopyStmt::ZEXT)
    {
        updateAbstractValue(lhsVar, getZExtValue(rhsVar), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SEXT)
    {
        updateAbstractValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOSI)
    {
        updateAbstractValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTOUI)
    {
        updateAbstractValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::SITOFP)
    {
        updateAbstractValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::UITOFP)
    {
        updateAbstractValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::TRUNC)
    {
        updateAbstractValue(lhsVar, getTruncValue(rhsVar, lhsVar->getType()), node);
    }
    else if (copy->getCopyKind() == CopyStmt::FPTRUNC)
    {
        updateAbstractValue(lhsVar, rhsVal.getInterval(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::INTTOPTR)
    {
        //insert nullptr
    }
    else if (copy->getCopyKind() == CopyStmt::PTRTOINT)
    {
        updateAbstractValue(lhsVar, IntervalValue::top(), node);
    }
    else if (copy->getCopyKind() == CopyStmt::BITCAST)
    {
        if (rhsVal.isAddr())
            updateAbstractValue(lhsVar, rhsVal, node);
    }
    else
        assert(false && "undefined copy kind");
}



