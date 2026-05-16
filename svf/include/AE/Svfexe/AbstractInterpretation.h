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
#include "AE/Svfexe/AEWTO.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "AE/Svfexe/AEStat.h"
#include "SVFIR/SVFIR.h"
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
class AndersenWaveDiff;

template<typename T> class FILOWorkList;

/// AbstractInterpretation is same as Abstract Execution.
///
/// Owns the per-node abstract trace and exposes the read/write API
/// directly (no separate state-manager indirection).  Sparse modes are
/// implemented as subclasses that override the virtual hooks below
/// (cycle helpers, ValVar accessors, joinStates, def/use queries).
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

    virtual void runOnModule();

    /// Destructor
    virtual ~AbstractInterpretation();

    /// Program entry
    void analyse();

    /// Analyze all entry points (functions without callers)
    void analyzeFromAllProgEntries();

    /// Get all entry point functions (functions without callers)
    std::deque<const FunObjVar*> collectProgEntryFuns();

    /// Factory: returns the singleton instance.  The concrete class is
    /// chosen once, on first call, from `Options::AESparsity()`:
    /// `SemiSparseAbstractInterpretation` for SemiSparse,
    /// `FullSparseAbstractInterpretation` for Sparse, otherwise the
    /// dense base.  Must be called only after the option parser has run.
    static AbstractInterpretation& getAEInstance();

    void addDetector(std::unique_ptr<AEDetector> detector)
    {
        detectors.push_back(std::move(detector));
    }

    /// Retrieve SVFVar given its ID; asserts if no such variable exists
    inline const SVFVar* getSVFVar(NodeID varId) const
    {
        return svfir->getSVFVar(varId);
    }

    // ---- Abstract Value Access ----------------------------------------

    /// Read a top-level variable's abstract value.  Dense base does a
    /// direct trace lookup; sparse subclasses override with their own
    /// resolution chain (def-site walk, call-result fallback, etc.).
    /// All three overloads are virtual so full-sparse can route ObjVar
    /// reads through the SVFG.
    virtual const AbstractValue& getAbsValue(const ValVar* var, const ICFGNode* node);
    virtual const AbstractValue& getAbsValue(const ObjVar* var, const ICFGNode* node);
    virtual const AbstractValue& getAbsValue(const SVFVar* var, const ICFGNode* node);

    /// Side-effect-free existence check.
    virtual bool hasAbsValue(const ValVar* var, const ICFGNode* node) const;
    virtual bool hasAbsValue(const ObjVar* var, const ICFGNode* node) const;
    virtual bool hasAbsValue(const SVFVar* var, const ICFGNode* node) const;

    /// Write a variable's abstract value.  Sparse subclasses re-route
    /// ValVar writes to the def-site.
    virtual void updateAbsValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node);
    virtual void updateAbsValue(const ObjVar* var, const AbstractValue& val, const ICFGNode* node);
    virtual void updateAbsValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node);


    // ---- State Access -------------------------------------------------

    AbstractState& getAbsState(const ICFGNode* node);

    /// Replace the state at `node`.  Sparse subclasses replace only the
    /// ObjVar map (ValVars live at def-sites).
    virtual void updateAbsState(const ICFGNode* node, const AbstractState& state);

    /// Join `src` into `dst` with sparsity-aware semantics.  Dense merges
    /// everything; semi-sparse skips ValVars.
    virtual void joinStates(AbstractState& dst, const AbstractState& src);

    bool hasAbsState(const ICFGNode* node);

    void getAbsState(const Set<const ValVar*>& vars, AbstractState& result, const ICFGNode* node);
    void getAbsState(const Set<const ObjVar*>& vars, AbstractState& result, const ICFGNode* node);
    void getAbsState(const Set<const SVFVar*>& vars, AbstractState& result, const ICFGNode* node);

    // ---- GEP / Load-Store / Type Helpers ------------------------------

    IntervalValue getGepElementIndex(const GepStmt* gep);
    IntervalValue getGepByteOffset(const GepStmt* gep);
    AddressValue getGepObjAddrs(const ValVar* pointer, IntervalValue offset);

    /// Virtual so full-sparse can layer the GepObj overlay on top.
    virtual AbstractValue loadValue(const ValVar* pointer, const ICFGNode* node);
    virtual void storeValue(const ValVar* pointer, const AbstractValue& val, const ICFGNode* node);

    const SVFType* getPointeeElement(const ObjVar* var, const ICFGNode* node);
    u32_t getAllocaInstByteSize(const AddrStmt* addr);

    // ---- Direct Trace Access ------------------------------------------

    Map<const ICFGNode*, AbstractState>& getTrace()
    {
        return abstractTrace;
    }
    AbstractState& operator[](const ICFGNode* node)
    {
        return abstractTrace[node];
    }

protected:
    /// Factory-only construction.  External callers must use getAEInstance();
    /// `SparseAbstractInterpretation` reaches this via its own ctor.
    AbstractInterpretation();

    // ---- Cycle helpers overridden by SparseAbstractInterpretation ----
    // The dense versions write only to trace[cycle_head].  The semi-sparse
    // subclass adds def-site scatter on top for body ValVars.

    /// Build a full cycle-head AbstractState.  Dense default: trace[cycle_head]
    /// as-is.  Semi-sparse subclass: also pull cycle ValVars from def-sites.
    virtual AbstractState getFullCycleHeadState(const ICFGCycleWTO* cycle);

    /// Widen prev with cur; write the widened state to trace[cycle_head].
    /// Returns true when next == prev (fixpoint).  Semi-sparse subclass
    /// additionally scatters ValVars to their def-sites.
    virtual bool widenCycleState(const AbstractState& prev, const AbstractState& cur,
                                 const ICFGCycleWTO* cycle);

    /// Narrow prev with cur; write the narrowed state back.  Returns true
    /// when narrowing is disabled or the narrowed state equals prev.
    /// Semi-sparse subclass scatters the narrowed ValVars on non-fixpoint.
    virtual bool narrowCycleState(const AbstractState& prev, const AbstractState& cur,
                                  const ICFGCycleWTO* cycle);

protected:
    /// Pull-based state merge: read abstractTrace[pred] for each predecessor,
    /// apply branch refinement for conditional IntraCFGEdges, and join into
    /// abstractTrace[node]. Returns true if at least one predecessor had state.
    /// Virtual so full-sparse can layer per-MRSVFGNode obj pulls on top of the
    /// base ICFG-edge merge.
    virtual bool mergeStatesFromPredecessors(const ICFGNode* node);

    /// Returns true if the branch is reachable.  When `recordNarrowing` is
    /// true, also narrows `as` in-place via recordBranchNarrowing for each obj
    /// the cmp/switch refines.
    bool isBranchFeasible(const IntraCFGEdge* edge, AbstractState& as,
                          bool recordNarrowing = true);

    /// Hook called by isCmp/SwitchBranchFeasible for each obj that the
    /// branch narrows.  Default (dense/semi): MEET `narrowed` onto
    /// obj's value (read at `loadIcfg` where sparse keeps it) and
    /// write the result into the local `as` (per-edge predState copy)
    /// so joinStates carries it to `succ`.  FullSparse overrides to
    /// capture into refinementTrace[succ] instead.
    virtual void recordBranchNarrowing(
        NodeID objId,
        const IntervalValue& narrowed,
        AbstractState& as,
        const ICFGNode* loadIcfg,
        const ICFGNode* succ);

private:
    /// Initialize abstract state for the global ICFG node and process global statements
    virtual void handleGlobalNode();

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

    /// Returns true if the cmp-conditional branch is feasible; optionally narrows as in-place.
    bool isCmpBranchFeasible(const IntraCFGEdge* edge, AbstractState& as,
                             bool recordNarrowing);

    /// Returns true if the switch branch is feasible; optionally narrows as in-place.
    bool isSwitchBranchFeasible(const IntraCFGEdge* edge, AbstractState& as,
                                bool recordNarrowing);

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

    /// Execution State, used to store the Interval Value of every SVF variable
    AEAPI* api{nullptr};

    ICFG* icfg;
    CallGraph* callGraph;
    AEStat* stat;

    AbsExtAPI* getUtils()
    {
        return utils;
    }

    // helper functions in handleCallSite
    virtual bool isExtCall(const CallICFGNode* callNode);
    virtual void handleExtCall(const CallICFGNode* callNode);
    virtual bool isRecursiveFun(const FunObjVar* fun);
    virtual void skipRecursionWithTop(const CallICFGNode *callNode);
    virtual bool isRecursiveCallSite(const CallICFGNode* callNode, const FunObjVar *);
    virtual void handleFunCall(const CallICFGNode* callNode);

    bool skipRecursiveCall(const CallICFGNode* callNode);
    const FunObjVar* getCallee(const CallICFGNode* callNode);

    // there data should be shared with subclasses
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map;

    Set<const ICFGNode*> allAnalyzedNodes; // All nodes ever analyzed (across all entry points)
    std::string moduleName;

    std::vector<std::unique_ptr<AEDetector>> detectors;
    AbsExtAPI* utils;

protected:
    /// Data and helpers reachable from SparseAbstractInterpretation.
    SVFIR* svfir{nullptr};
    AEWTO* preAnalysis{nullptr};
    Map<const ICFGNode*, AbstractState> abstractTrace; ///< per-node trace; owned here

    bool shouldApplyNarrowing(const FunObjVar* fun);
};
}
