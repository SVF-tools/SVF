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
    enum AESparsity
    {
        Dense,
        SemiSparse,
        Sparse
    };

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

    // ===----------------------------------------------------------------------===//
    //  Abstract Value Access API
    //
    //  Unified entry points for reading/writing abstract values.
    //  These encapsulate the dense vs. semi-sparse lookup strategy:
    //    - Dense:       reads from abstractTrace[node] directly.
    //    - Semi-sparse: reads from the current node's state if present,
    //                   otherwise pulls from the variable's def-site.
    //  Callers (updateStateOnXxx, AEDetector, AbsExtAPI) should ONLY use
    //  these APIs — never access abstractTrace[node][id] directly.
    // ===----------------------------------------------------------------------===//

    /// Read a top-level variable's abstract value.
    /// @param var   The ValVar to look up.
    /// @param node  The ICFG node providing context (dense: reads this node's
    ///              state; semi-sparse: checks this state first, then def-site).
    /// @return      Reference to the abstract value.  Returns top if the variable
    ///              is absent from all reachable states (uninitialized).
    const AbstractValue& getAbstractValue(const ValVar* var, const ICFGNode* node);

    /// Read an address-taken variable's content via virtual-address load.
    /// @param var   The ObjVar whose stored content is requested.
    /// @param node  The ICFG node whose state to load from.
    /// @return      Reference to the loaded abstract value (from _addrToAbsVal).
    const AbstractValue& getAbstractValue(const ObjVar* var, const ICFGNode* node);

    /// Read any SVFVar — dispatches to the ValVar or ObjVar overload.
    /// NOTE: checks ObjVar first since ObjVar inherits from ValVar.
    const AbstractValue& getAbstractValue(const SVFVar* var, const ICFGNode* node);

    /// Write a top-level variable's abstract value into abstractTrace[node].
    void updateAbstractValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node);

    /// Write an address-taken variable's content via virtual-address store.
    void updateAbstractValue(const ObjVar* var, const AbstractValue& val, const ICFGNode* node);

    /// Write any SVFVar — dispatches to the ValVar or ObjVar overload.
    void updateAbstractValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node);

    /// Propagate an ObjVar's abstract value from defSite to all its
    /// use-site ICFGNodes (determined by PreAnalysis SVFG info).
    void propagateObjVarAbsVal(const ObjVar* var, const ICFGNode* defSite);

    /// Retrieve the full abstract state for a given ICFG node.
    /// Asserts if no trace exists for the node.
    AbstractState& getAbstractState(const ICFGNode* node);

    /// Check if an abstract state exists in the trace for a given ICFG node.
    bool hasAbstractState(const ICFGNode* node);

    /// Retrieve abstract state filtered to specific top-level variables.
    void getAbstractState(const Set<const ValVar*>& vars, AbstractState& result, const ICFGNode* node);

    /// Retrieve abstract state filtered to specific address-taken variables.
    void getAbstractState(const Set<const ObjVar*>& vars, AbstractState& result, const ICFGNode* node);

    /// Retrieve abstract state filtered to specific SVF variables.
    void getAbstractState(const Set<const SVFVar*>& vars, AbstractState& result, const ICFGNode* node);

    // ===----------------------------------------------------------------------===//
    //  GEP Helpers (lifted from AbstractState to use getAbstractValue)
    //
    //  These replace AbstractState::getElementIndex / getByteOffset / getGepObjAddrs
    //  which internally used (*this)[varId] — that doesn't work in semi-sparse mode
    //  where ValVars may not be in the current node's state.
    // ===----------------------------------------------------------------------===//

    /// Compute the flattened element index for a GepStmt.
    /// Reads variable-offset indices via getAbstractValue; constant offsets are
    /// resolved statically.  Returns an IntervalValue clamped to [0, MaxFieldLimit].
    /// @param gep   The GepStmt to evaluate.
    /// @param node  The ICFG node providing context for index variable lookup.
    /// Used by: updateStateOnGep.
    IntervalValue getGepElementIndex(const GepStmt* gep, const ICFGNode* node);

    /// Compute the byte offset for a GepStmt.
    /// Similar to getGepElementIndex but produces byte-level offsets, handling
    /// array element sizes, pointer element sizes, and struct field offsets.
    /// @param gep   The GepStmt to evaluate.
    /// @param node  The ICFG node providing context for index variable lookup.
    /// Used by: AEDetector (getAccessOffset, updateGepObjOffsetFromBase).
    IntervalValue getGepByteOffset(const GepStmt* gep, const ICFGNode* node);

    /// Compute GEP object addresses for a pointer at a given element offset.
    /// Reads the pointer's AddressValue via getAbstractValue, then resolves each
    /// base object to its GepObjVar at the specified offset.  Initializes the
    /// GepObjVar's virtual address in the state as a side-effect.
    /// @param pointer  The pointer SVFVar whose address set to expand.
    /// @param offset   The element offset interval (from getGepElementIndex).
    /// @param node     The ICFG node providing context.
    /// @return         The set of virtual addresses for the GEP result objects.
    /// Used by: updateStateOnGep, AbsExtAPI (getStrlen, handleMemcpy, handleMemset, strRead).
    AddressValue getGepObjAddrs(const SVFVar* pointer, IntervalValue offset, const ICFGNode* node);

    // ===----------------------------------------------------------------------===//
    //  Branch Condition Helpers
    // ===----------------------------------------------------------------------===//

    /// Pull load-chain pointers for branch condition variables into a state copy.
    ///
    /// Called before isBranchFeasible in mergeStatesFromPredecessors and
    /// updateStateOnPhi.  isCmpBranchFeasible narrows CmpStmt operands (ValVars)
    /// AND their backing ObjVars (e.g., narrowing *%ptr when %x = load %ptr is
    /// refined).  To narrow the ObjVar it needs %ptr's AddressValue in the state.
    ///
    /// This function walks each CmpStmt operand's def-chain looking for a LoadStmt
    /// (possibly through a CopyStmt), and pulls the load's RHS pointer into the
    /// given state via getAbstractValue.
    ///
    /// @param edge   The conditional IntraCFGEdge whose condition to analyze.
    /// @param state  The predecessor state copy to populate (passed to isBranchFeasible).
    /// Used by: mergeStatesFromPredecessors, updateStateOnPhi.
    void pullBranchConditionVars(const IntraCFGEdge* edge, AbstractState& state);

    /// Pull a single load-chain pointer into the given state.
    ///
    /// Given a CmpStmt operand id, traces back through its defining LoadStmt
    /// (or CopyStmt→LoadStmt chain) to find the pointer being loaded from.
    /// If that pointer's AddressValue is not already in the state, fetches it
    /// via getAbstractValue and writes it into the state.
    ///
    /// Example: for `%x = load i32, i32* %ptr`, given opId = %x's NodeID,
    /// this pulls %ptr's AddressValue into the state so that isCmpBranchFeasible
    /// can do `as.load(%ptr_addr).meet_with(narrowed_value)`.
    ///
    /// @param opId   NodeID of the CmpStmt operand to trace.
    /// @param state  The state to populate.
    /// Used by: pullBranchConditionVars.
    void pullLoadChainPointerIntoState(NodeID opId, AbstractState& state);


private:
    /// Initialize abstract state for the global ICFG node and process global statements
    virtual void handleGlobalNode();

    /// Pull-based state merge: read abstractTrace[pred] for each predecessor,
    /// apply branch refinement for conditional IntraCFGEdges, and join into
    /// abstractTrace[node]. Returns true if at least one predecessor had state.
    bool mergeStatesFromPredecessors(const ICFGNode* node);

    /// Check if the branch on intraEdge is feasible under abstract state as
    bool isBranchFeasible(const IntraCFGEdge* intraEdge, AbstractState& as, const ICFGNode* predNode);

    /// Handle a call site node: dispatch to ext-call, direct-call, or indirect-call handling
    virtual void handleCallSite(const ICFGNode* node);

    /// Handle a WTO cycle (loop or recursive function) using widening/narrowing iteration
    virtual void handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller = nullptr);

    /// Handle a function body via worklist-driven WTO traversal starting from funEntry
    void handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller = nullptr);

    /// Handle an ICFG node: execute statements; return true if state changed
    bool handleICFGNode(const ICFGNode* node);

    /// Dispatch an SVF statement (Addr/Binary/Cmp/Load/Store/Copy/Gep/Select/Phi/Call/Ret) to its handler
    virtual void handleSVFStatement(const SVFStmt* stmt);

    /// Set all store targets and return value to TOP for a recursive call node
    virtual void setTopToObjInRecursion(const CallICFGNode* callnode);

    /// Check if cmpStmt with successor value succ is feasible; refine intervals in as accordingly
    bool isCmpBranchFeasible(const CmpStmt* cmpStmt, s64_t succ,
                             AbstractState& as, const ICFGNode* predNode);

    /// Check if switch branch with case value succ is feasible; refine intervals in as accordingly
    bool isSwitchBranchFeasible(const SVFVar* var, s64_t succ, AbstractState& as, const ICFGNode* predNode);

    void updateStateOnAddr(const AddrStmt *addr);

    void updateStateOnBinary(const BinaryOPStmt *binary);

    void updateStateOnCmp(const CmpStmt *cmp);

    void updateStateOnLoad(const LoadStmt *load);

    void updateStateOnStore(const StoreStmt *store);

    void updateStateOnCopy(const CopyStmt *copy);

    void updateStateOnCall(const CallPE *callPE);

    void updateStateOnRet(const RetPE *retPE);

    void updateStateOnGep(const GepStmt *gep);

    void updateStateOnSelect(const SelectStmt *select);

    void updateStateOnPhi(const PhiStmt *phi);


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
    virtual void handleExtCall(const CallICFGNode* callNode);
    virtual bool isRecursiveFun(const FunObjVar* fun);
    virtual void handleRecursiveCall(const CallICFGNode *callNode);
    virtual bool isRecursiveCallSite(const CallICFGNode* callNode, const FunObjVar *);
    virtual void handleFunCall(const CallICFGNode* callNode);

    bool skipRecursiveCall(const CallICFGNode* callNode);
    const FunObjVar* getCallee(const CallICFGNode* callNode);
    bool shouldApplyNarrowing(const FunObjVar* fun);

    // there data should be shared with subclasses
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map;

    Map<const ICFGNode*, AbstractState> abstractTrace; // abstract states for nodes
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