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
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via
// Cross-Domain Interaction. 46th International Conference on Software
// Engineering. (ICSE24)
//
#pragma once
#include "AE/Core/AbstractState.h"
#include "AE/Core/AbstractValue.h"
#include "AE/Core/ICFGWTO.h"
#include "AE/Svfexe/AEDetector.h"
#include "AE/Svfexe/AEStat.h"
#include "AE/Svfexe/AEWTO.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "Graphs/CallGraph.h"
#include "Graphs/SCC.h"
#include "SVFIR/SVFIR.h"
#include "Util/SVFBugReport.h"
#include "Util/WorkList.h"

namespace SVF
{
class AbstractInterpretation;
class AbsExtAPI;
class AEStat;
class AEAPI;
class AndersenWaveDiff;

template <typename T> class FILOWorkList;

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
     * if set TOP, result = [-oo, +oo] since the return value, and any stored
     object pointed by q at *q = p in recursive functions will be set to the top
     value.
     * if set WIDEN_ONLY, result = [10000, +oo] since only widening is applied
     at the cycle head of recursive functions without narrowing.
     * if set WIDEN_NARROW, result = [10000, 10000] since both widening and
     narrowing are applied at the cycle head of recursive functions.
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

    enum AEFunEntryMode
    {
        MAIN,
        NO_MAIN
    };

    virtual void runOnModule();

    /// Destructor
    virtual ~AbstractInterpretation();

    /// Program entry
    void analyse();

    /// Analyze all entry points (functions without callers)
    void analyzeFromAllProgEntries();

    /// Get all entry point functions (functions without callers)
    FIFOWorkList<const FunObjVar*> collectProgEntryFuns();

    /// Factory: returns the singleton instance.  The concrete class is
    /// chosen once, on first call, from `Options::AESparsity()`:
    /// the native Box semi-sparse/full-sparse implementation, or the dense
    /// Box implementation. Must be called only after option parsing.
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
    virtual AbstractValue getAbsValue(const ValVar* var,
                                      const ICFGNode* node) = 0;
    virtual AbstractValue getAbsValue(const ObjVar* var,
                                      const ICFGNode* node) = 0;
    virtual AbstractValue getAbsValue(const SVFVar* var,
                                      const ICFGNode* node) = 0;

    /// Side-effect-free existence check.
    virtual bool hasAbsValue(const ValVar* var, const ICFGNode* node) const = 0;
    virtual bool hasAbsValue(const ObjVar* var, const ICFGNode* node) const = 0;
    virtual bool hasAbsValue(const SVFVar* var, const ICFGNode* node) const = 0;

    /// Write a variable's abstract value.  Sparse subclasses re-route
    /// ValVar writes to the def-site.
    virtual void updateAbsValue(const ValVar* var, const AbstractValue& value,
                                const ICFGNode* node) = 0;
    virtual void updateAbsValue(const ObjVar* var, const AbstractValue& value,
                                const ICFGNode* node) = 0;
    virtual void updateAbsValue(const SVFVar* var, const AbstractValue& value,
                                const ICFGNode* node) = 0;

    /// Representation-independent memory and lifetime access.  Addresses use
    /// the existing AE virtual-address encoding at this API boundary; native
    /// domains translate them to Location internally.
    virtual AbstractValue getMemoryValue(u32_t address,
                                         const ICFGNode* node) = 0;
    virtual bool hasMemoryValue(u32_t address, const ICFGNode* node) const = 0;
    virtual void updateMemoryValue(u32_t address, const AbstractValue& value,
                                   const ICFGNode* node) = 0;
    virtual void markFreedMemory(u32_t address, const ICFGNode* node) = 0;
    virtual bool isFreedMemory(u32_t address, const ICFGNode* node) const = 0;

    static NodeID objectIdFromAddress(u32_t address)
    {
        return address & FlippedAddressMask;
    }

    // ---- State Access -------------------------------------------------

    /// Return the authoritative complete state used for control-flow joins
    /// and fixpoint computation.
    virtual const AbstractDomain::AbstractState& getAbstractState(
        const ICFGNode* node) const = 0;

    /// Return the analysis-wide SSA-value carrier for `function` when the
    /// selected sparse implementation separates ValVars from ICFG memory
    /// states. Other implementations return nullptr.
    virtual const AbstractDomain::AbstractState* getScalarAbstractState(
        const FunObjVar* function) const;

    /// Return the sparse relational checkpoint associated with a particular
    /// SSA definition, or nullptr when the implementation has no separated
    /// definition checkpoint.
    virtual const AbstractDomain::AbstractState* getScalarAbstractState(
        const ValVar* value) const;

    virtual bool hasAbsState(const ICFGNode* node) const = 0;

    // ---- GEP / Load-Store / Type Helpers ------------------------------

    IntervalValue getGepElementIndex(const GepStmt* gep);
    IntervalValue getGepByteOffset(const GepStmt* gep);
    AddressValue getGepObjAddrs(const ValVar* pointer, IntervalValue offset);

    /// Virtual so full-sparse can layer the GepObj overlay on top.
    virtual AbstractValue loadValue(const ValVar* pointer,
                                    const ICFGNode* node) = 0;
    virtual void storeValue(const ValVar* pointer, const AbstractValue& val,
                            const ICFGNode* node) = 0;

    const SVFType* getPointeeElement(const ObjVar* var, const ICFGNode* node);
    u32_t getAllocaInstByteSize(const AddrStmt* addr);

    const Set<const ICFGNode*>& getAnalyzedNodes() const
    {
        return allAnalyzedNodes;
    }

protected:
    /// Factory-only construction.  External callers must use getAEInstance();
    /// Concrete Box implementations reach this through their constructors.
    AbstractInterpretation();

    // ---- Cycle helpers implemented by Box-backed execution modes ----
    // The dense versions write only to trace[cycle_head].  The semi-sparse
    // subclass adds def-site scatter on top for body ValVars.

    /// Clone the complete cycle-head state. Sparse subclasses may first gather
    /// values held at def-sites; dense implementations clone their domain
    /// state directly.
    virtual std::unique_ptr<AbstractDomain::AbstractState> cloneCycleHeadState(
        const ICFGCycleWTO* cycle) = 0;

    /// Widen prev with cur; write the widened state to trace[cycle_head].
    /// Returns true when next == prev (fixpoint).  Semi-sparse subclass
    /// additionally scatters ValVars to their def-sites.
    virtual bool widenCycleState(const AbstractDomain::AbstractState& prev,
                                 const AbstractDomain::AbstractState& cur,
                                 const ICFGCycleWTO* cycle) = 0;

    /// Narrow prev with cur; write the narrowed state back.  Returns true
    /// when narrowing is disabled or the narrowed state equals prev.
    /// Semi-sparse subclass scatters the narrowed ValVars on non-fixpoint.
    virtual bool narrowCycleState(const AbstractDomain::AbstractState& prev,
                                  const AbstractDomain::AbstractState& cur,
                                  const ICFGCycleWTO* cycle) = 0;

protected:
    /// Representation-independent state lifecycle used by the shared WTO and
    /// call/return drivers. Dense and sparse analyses provide different
    /// storage implementations behind this small surface.
    virtual void resetAbstractState(const ICFGNode* node) = 0;
    virtual void copyAbstractState(const ICFGNode* source,
                                   const ICFGNode* destination) = 0;
    virtual std::unique_ptr<AbstractDomain::AbstractState> cloneAbstractState(
        const ICFGNode* node) const = 0;
    virtual bool isAbstractStateEquivalent(
        const ICFGNode* node,
        const AbstractDomain::AbstractState& snapshot) const = 0;

    /// Normalize a node after its transfers and detectors have consumed any
    /// temporary operands. Native sparse implementations use this boundary to
    /// keep ValVar values out of persistent ICFG states.
    virtual void finalizeAbstractState(const ICFGNode* node);

    /// Pull-based state merge: read abstractTrace[pred] for each predecessor,
    /// apply branch refinement for conditional IntraCFGEdges, and join into
    /// abstractTrace[node]. Returns true if at least one predecessor had state.
    /// Virtual so full-sparse can layer per-MRSVFGNode obj pulls on top of the
    /// base ICFG-edge merge.
    virtual bool mergeStatesFromPredecessors(const ICFGNode* node) = 0;

    /// Representation-independent feasibility query used by shared transfer
    /// code.  Native dense domains apply the constraint directly.
    virtual bool isBranchEdgeFeasibleAt(const IntraCFGEdge* edge,
                                        const ICFGNode* predecessor) = 0;

    /// Collect branch-induced interval refinement after a feasible edge has
    /// been selected for normal CFG-state merging.
    void collectBranchRefinement(const IntraCFGEdge* edge,
                                 AbstractDomain::AbstractState& state);

    /// Hook called by collectBranchRefinement for each object narrowed by the
    /// branch. Dense and semi-sparse implementations meet the constraint into
    /// the transient edge state; full-sparse records it for MemorySSA flow.
    virtual void recordBranchRefinement(NodeID objId,
                                        const IntervalValue& narrowed,
                                        AbstractDomain::AbstractState& state,
                                        const ICFGNode* loadIcfg,
                                        const ICFGNode* succ);

protected:
    /// Initialize abstract state for the global ICFG node and process global
    /// statements
    virtual void handleGlobalNode() = 0;

    /// Materialise the value produced by an AddrStmt without prescribing a
    /// concrete state representation.
    virtual AbstractValue initializeObjectAddress(const ObjVar* object,
                                                  const ICFGNode* node) = 0;

    /// Handle a call site node: dispatch to ext-call, direct-call, or
    /// indirect-call handling
    virtual void handleCallSite(const ICFGNode* node);

    /// Handle a WTO cycle (loop or recursive function) using widening/narrowing
    /// iteration
    virtual void handleLoopOrRecursion(const ICFGCycleWTO* cycle,
                                       const CallICFGNode* caller);

    /// Handle a function body via worklist-driven WTO traversal starting from
    /// funEntry
    void handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller);

    /// Handle an ICFG node: execute statements; return true if state changed
    bool handleICFGNode(const ICFGNode* node);

    /// Dispatch an SVF statement
    /// (Addr/Binary/Cmp/Load/Store/Copy/Gep/Select/Phi/Call/Ret) to its handler
    virtual void handleSVFStatement(const SVFStmt* stmt);

    void updateStateOnAddr(const AddrStmt* addr);

    void updateStateOnBinary(const BinaryOPStmt* binary);

    void updateStateOnCmp(const CmpStmt* cmp);

    void updateStateOnLoad(const LoadStmt* load);

    void updateStateOnStore(const StoreStmt* store);

    void updateStateOnCopy(const CopyStmt* copy);

    void updateStateOnCall(const CallPE* callPE);

    void updateStateOnRet(const RetPE* retPE);

    void updateStateOnGep(const GepStmt* gep);

    void updateStateOnSelect(const SelectStmt* select);

    void updateStateOnPhi(const PhiStmt* phi);

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
    virtual void skipRecursionWithTop(const CallICFGNode* callNode);
    virtual bool isRecursiveCallSite(const CallICFGNode* callNode,
                                     const FunObjVar*);
    virtual void handleFunCall(const CallICFGNode* callNode);

    bool skipRecursiveCall(const CallICFGNode* callNode);
    const FunObjVar* getCallee(const CallICFGNode* callNode);

    // there data should be shared with subclasses
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map;

    Set<const ICFGNode*>
        allAnalyzedNodes; // All nodes ever analyzed (across all entry points)
    std::string moduleName;

    std::vector<std::unique_ptr<AEDetector>> detectors;
    AbsExtAPI* utils;

protected:
    /// Data and helpers reachable from native sparse implementations.
    SVFIR* svfir{nullptr};
    AEWTO* preAnalysis{nullptr};

    bool shouldApplyNarrowing(const FunObjVar* fun);

    // ---- Domain-specific precision hooks -----------------------------
    // Sparse modes use the no-op base implementations. Dense mode updates
    // its selected numerical domain through these hooks after legacy
    // transfer code computes the compatibility AbstractValue.
    virtual void initializeDomainState(const ICFGNode* node);
    virtual void assignDomainInterval(const ICFGNode* node,
                                      const SVFVar* target,
                                      const IntervalValue& interval);
    virtual void updateDomainOnBinary(const BinaryOPStmt* binary,
                                      const IntervalValue& result);
    virtual void updateDomainOnCopy(const CopyStmt* copy);
    virtual void updateDomainCopyValue(const ICFGNode* node,
                                       const SVFVar* target,
                                       const SVFVar* source,
                                       bool exactMathematicalCopy);
};
} // namespace SVF
