//===- AbstractInterpretation.h -- Abstract Execution----------//
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
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)
//
#pragma once
#include "AE/Core/AbstractState.h"
#include "AE/Core/ICFGWTO.h"
#include "AE/Svfexe/AEDetector.h"
#include "AE/Svfexe/PreAnalysis.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "AE/Svfexe/AEStat.h"
#include "Util/SVFBugReport.h"
#include "Graphs/SCC.h"
#include "Graphs/CallGraph.h"
#include <deque>

namespace SVF
{
class AbstractInterpretation;
class AbsExtAPI;
class AEStat;
class AEAPI;

template<typename T> class FILOWorkList;

/// AbstractInterpretation is same as Abstract Execution
class AbstractInterpretation
{
    friend class AEStat;
    friend class AEAPI;
    friend class BufOverflowDetector;
    friend class NullptrDerefDetector;

public:
    /*
     * For recursive test case
     * int demo(int a) {
        if (a >= 10000)
            return a;
            demo(a+1);
        }

        int main() {
            int result = demo(0);
        }
     * if set TOP, result = [-oo, +oo] since the return value, and any stored object pointed by q at *q = p in recursive functions will be set to the top value.
     * if set WIDEN_ONLY, result = [10000, +oo] since only widening is applied at the cycle head of recursive functions without narrowing.
     * if set WIDEN_NARROW, result = [10000, 10000] since both widening and narrowing are applied at the cycle head of recursive functions.
     * */
    enum HandleRecur
    {
        TOP,
        WIDEN_ONLY,
        WIDEN_NARROW
    };

    /// Constructor
    AbstractInterpretation();

    virtual void runOnModule(ICFG* icfg);

    /// Destructor
    virtual ~AbstractInterpretation();

    /// Program entry
    void analyse();

    /// Analyze all entry points (functions without callers)
    void analyzeFromAllProgEntries();

    /// Get all entry point functions (functions without callers)
    std::deque<const FunObjVar*> collectProgEntryFuns();


    static AbstractInterpretation& getAEInstance()
    {
        static AbstractInterpretation instance;
        return instance;
    }

    void addDetector(std::unique_ptr<AEDetector> detector)
    {
        detectors.push_back(std::move(detector));
    }

    /// Retrieve SVFVar given its ID; asserts if no such variable exists
    inline const SVFVar* getSVFVar(NodeID varId) const
    {
        return svfir->getSVFVar(varId);
    }

private:
    /// Initialize abstract state for the global ICFG node and process global statements
    virtual void handleGlobalNode();

    /// Propagate the post-state of a node along each outgoing IntraCFGEdge,
    /// writing the (possibly branch-refined) state to edgeAbsStates.
    void propagateToSuccessor(const ICFGNode* node, AbstractState& as);

    /// Merge all incoming edge states from edgeAbsStates into a fresh AbstractState.
    /// Edge states are NOT erased (they persist for loop iteration and phi reads).
    AbstractState mergeInEdges(const ICFGNode* node);

    /// Recursively collect all ICFG nodes in a WTO cycle (excluding the head).
    void collectBodyNodes(const ICFGCycleWTO* cycle, Set<const ICFGNode*>& bodyNodes);

    /// Check if the branch on intraEdge is feasible under abstract state as
    bool isBranchFeasible(const IntraCFGEdge* intraEdge, AbstractState& as);

    /// Handle a call site node: dispatch to ext-call, direct-call, or indirect-call handling
    virtual void handleCallSite(const ICFGNode* node, AbstractState& as);

    /// Handle a WTO cycle (loop or recursive function) using widening/narrowing iteration
    virtual void handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller = nullptr);

    /// Handle a function body via worklist-driven WTO traversal starting from funEntry.
    /// Returns the exit state of the function.
    AbstractState handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller = nullptr);

    /// Handle an ICFG node: execute statements on the given state
    void handleICFGNode(const ICFGNode* node, AbstractState& as);

    /// Dispatch an SVF statement (Addr/Binary/Cmp/Load/Store/Copy/Gep/Select/Phi/Call/Ret) to its handler
    virtual void handleSVFStatement(const SVFStmt* stmt, AbstractState& as);

    /// Set all store targets and return value to TOP for a recursive call node
    virtual void setTopToObjInRecursion(const CallICFGNode* callnode, AbstractState& as);

    /// Check if cmpStmt with successor value succ is feasible; refine intervals in as accordingly
    bool isCmpBranchFeasible(const CmpStmt* cmpStmt, s64_t succ,
                             AbstractState& as);

    /// Check if switch branch with case value succ is feasible; refine intervals in as accordingly
    bool isSwitchBranchFeasible(const SVFVar* var, s64_t succ, AbstractState& as);

    void updateStateOnAddr(const AddrStmt *addr, AbstractState& as);

    void updateStateOnBinary(const BinaryOPStmt *binary, AbstractState& as);

    void updateStateOnCmp(const CmpStmt *cmp, AbstractState& as);

    void updateStateOnLoad(const LoadStmt *load, AbstractState& as);

    void updateStateOnStore(const StoreStmt *store, AbstractState& as);

    void updateStateOnCopy(const CopyStmt *copy, AbstractState& as);

    void updateStateOnCall(const CallPE *callPE, AbstractState& as);

    void updateStateOnRet(const RetPE *retPE, AbstractState& as);

    void updateStateOnGep(const GepStmt *gep, AbstractState& as);

    void updateStateOnSelect(const SelectStmt *select, AbstractState& as);

    void updateStateOnPhi(const PhiStmt *phi, AbstractState& as);


    /// protected data members, also used in subclasses
    SVFIR* svfir;
    /// Execution State, used to store the Interval Value of every SVF variable
    AEAPI* api{nullptr};

    ICFG* icfg;
    CallGraph* callGraph;
    AEStat* stat;

    PreAnalysis* preAnalysis{nullptr};

    AbsExtAPI* getUtils()
    {
        return utils;
    }

    // helper functions in handleCallSite
    virtual bool isExtCall(const CallICFGNode* callNode);
    virtual void handleExtCall(const CallICFGNode* callNode, AbstractState& as);
    virtual bool isRecursiveFun(const FunObjVar* fun);
    virtual void handleRecursiveCall(const CallICFGNode *callNode, AbstractState& as);
    virtual bool isRecursiveCallSite(const CallICFGNode* callNode, const FunObjVar *);
    virtual void handleFunCall(const CallICFGNode* callNode, AbstractState& as);

    bool skipRecursiveCall(const CallICFGNode* callNode, AbstractState& as);
    const FunObjVar* getCallee(const CallICFGNode* callNode, AbstractState& as);
    bool shouldApplyNarrowing(const FunObjVar* fun);

    // there data should be shared with subclasses
    Map<std::string, std::function<void(const CallICFGNode*, AbstractState&)>> func_map;

    AbstractState globalState; // global node state (persists for function entry fallback)
    Map<const ICFGEdge*, AbstractState> edgeAbsStates; // edge states (primary storage, NOT erased)
    Set<const ICFGNode*> allAnalyzedNodes; // All nodes ever analyzed (across all entry points)
    std::string moduleName;

    std::vector<std::unique_ptr<AEDetector>> detectors;
    AbsExtAPI* utils;

    // according to varieties of cmp insts,
    // maybe var X var, var X const, const X var, const X const
    // we accept 'var X const' 'var X var' 'const X const'
    // if 'const X var', we need to reverse op0 op1 and its predicate 'var X' const'
    // X' is reverse predicate of X
    // == -> !=, != -> ==, > -> <=, >= -> <, < -> >=, <= -> >

    Map<s32_t, s32_t> _reverse_predicate =
    {
        {CmpStmt::Predicate::FCMP_OEQ, CmpStmt::Predicate::FCMP_ONE},  // == -> !=
        {CmpStmt::Predicate::FCMP_UEQ, CmpStmt::Predicate::FCMP_UNE},  // == -> !=
        {CmpStmt::Predicate::FCMP_OGT, CmpStmt::Predicate::FCMP_OLE},  // > -> <=
        {CmpStmt::Predicate::FCMP_OGE, CmpStmt::Predicate::FCMP_OLT},  // >= -> <
        {CmpStmt::Predicate::FCMP_OLT, CmpStmt::Predicate::FCMP_OGE},  // < -> >=
        {CmpStmt::Predicate::FCMP_OLE, CmpStmt::Predicate::FCMP_OGT},  // <= -> >
        {CmpStmt::Predicate::FCMP_ONE, CmpStmt::Predicate::FCMP_OEQ},  // != -> ==
        {CmpStmt::Predicate::FCMP_UNE, CmpStmt::Predicate::FCMP_UEQ},  // != -> ==
        {CmpStmt::Predicate::ICMP_EQ, CmpStmt::Predicate::ICMP_NE},  // == -> !=
        {CmpStmt::Predicate::ICMP_NE, CmpStmt::Predicate::ICMP_EQ},  // != -> ==
        {CmpStmt::Predicate::ICMP_UGT, CmpStmt::Predicate::ICMP_ULE},  // > -> <=
        {CmpStmt::Predicate::ICMP_ULT, CmpStmt::Predicate::ICMP_UGE},  // < -> >=
        {CmpStmt::Predicate::ICMP_UGE, CmpStmt::Predicate::ICMP_ULT},  // >= -> <
        {CmpStmt::Predicate::ICMP_SGT, CmpStmt::Predicate::ICMP_SLE},  // > -> <=
        {CmpStmt::Predicate::ICMP_SLT, CmpStmt::Predicate::ICMP_SGE},  // < -> >=
        {CmpStmt::Predicate::ICMP_SGE, CmpStmt::Predicate::ICMP_SLT},  // >= -> <
    };


    Map<s32_t, s32_t> _switch_lhsrhs_predicate =
    {
        {CmpStmt::Predicate::FCMP_OEQ, CmpStmt::Predicate::FCMP_OEQ},  // == -> ==
        {CmpStmt::Predicate::FCMP_UEQ, CmpStmt::Predicate::FCMP_UEQ},  // == -> ==
        {CmpStmt::Predicate::FCMP_OGT, CmpStmt::Predicate::FCMP_OLT},  // > -> <
        {CmpStmt::Predicate::FCMP_OGE, CmpStmt::Predicate::FCMP_OLE},  // >= -> <=
        {CmpStmt::Predicate::FCMP_OLT, CmpStmt::Predicate::FCMP_OGT},  // < -> >
        {CmpStmt::Predicate::FCMP_OLE, CmpStmt::Predicate::FCMP_OGE},  // <= -> >=
        {CmpStmt::Predicate::FCMP_ONE, CmpStmt::Predicate::FCMP_ONE},  // != -> !=
        {CmpStmt::Predicate::FCMP_UNE, CmpStmt::Predicate::FCMP_UNE},  // != -> !=
        {CmpStmt::Predicate::ICMP_EQ, CmpStmt::Predicate::ICMP_EQ},  // == -> ==
        {CmpStmt::Predicate::ICMP_NE, CmpStmt::Predicate::ICMP_NE},  // != -> !=
        {CmpStmt::Predicate::ICMP_UGT, CmpStmt::Predicate::ICMP_ULT},  // > -> <
        {CmpStmt::Predicate::ICMP_ULT, CmpStmt::Predicate::ICMP_UGT},  // < -> >
        {CmpStmt::Predicate::ICMP_UGE, CmpStmt::Predicate::ICMP_ULE},  // >= -> <=
        {CmpStmt::Predicate::ICMP_SGT, CmpStmt::Predicate::ICMP_SLT},  // > -> <
        {CmpStmt::Predicate::ICMP_SLT, CmpStmt::Predicate::ICMP_SGT},  // < -> >
        {CmpStmt::Predicate::ICMP_SGE, CmpStmt::Predicate::ICMP_SLE},  // >= -> <=
    };

};
}
