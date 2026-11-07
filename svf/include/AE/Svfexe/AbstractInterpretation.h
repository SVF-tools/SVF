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
#include "AE/Core/AbstractDomain.h"
#include "AE/Core/AddressDomain.h"
#include "AE/Core/BoxAddressDomain.h"
#include "AE/Core/ICFGWTO.h"
#include "AE/Core/NumericalDomain.h"
#include "AE/Svfexe/AEDetector.h"
#include "AE/Svfexe/AEStat.h"
#include "AE/Svfexe/AEWTO.h"
#include "AE/Svfexe/AbsExtAPI.h"
#include "AE/Svfexe/SVFIRAdapter.h"
#include "Graphs/CallGraph.h"
#include "Graphs/SCC.h"
#include "SVFIR/SVFIR.h"
#include "Util/SVFBugReport.h"
#include "Util/WorkList.h"
#include <memory>
#include <set>
#include <vector>

namespace SVF
{
class AbstractInterpretation;
class AbsExtAPI;
class AEStat;
/// AbstractInterpretation is same as Abstract Execution.
///
/// Owns the per-node abstract trace and exposes the read/write API
/// directly (no separate state-manager indirection).  Sparse modes are
/// implemented as subclasses that override the virtual hooks below
/// (cycle helpers, ValVar accessors, joinStates, def/use queries).
class AbstractInterpretation
{
    friend class AEStat;
    friend class AbsExtAPI;
    friend class BufOverflowDetector;
    friend class NullptrDerefDetector;

public:
    using State = SVF::BoxAddressDomain;

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
        Sparse,
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

    // ---- Domain projection access ------------------------------------

    /// Read a top-level variable's abstract value.  Dense base does a
    /// direct trace lookup; sparse subclasses override with their own
    /// definition-site lookup.
    /// All three overloads are virtual so full-sparse can route ObjVar
    /// reads through the SVFG.
    virtual SVF::Interval getInterval(const ValVar* var,
                                      const ICFGNode* node);
    /// Return the numerical payload conditional on the value being
    /// initialized. Ordinary reads use getInterval(), which applies AE's
    /// uninitialized-read policy.
    virtual SVF::Interval getDefinedInterval(
        const ValVar* var, const ICFGNode* node);
    virtual SVF::Interval getInterval(const ObjVar* var,
                                      const ICFGNode* node);
    virtual SVF::Interval getInterval(const SVFVar* var,
                                      const ICFGNode* node);
    virtual SVF::AddressSet getAddressSet(const ValVar* var,
                                          const ICFGNode* node);
    virtual SVF::AddressSet getAddressSet(const ObjVar* var,
                                          const ICFGNode* node);
    virtual SVF::AddressSet getAddressSet(const SVFVar* var,
                                          const ICFGNode* node);

    /// Whether the value is present at this node.
    virtual bool hasAbsValue(const ValVar* var, const ICFGNode* node) const;
    virtual bool hasAbsValue(const ObjVar* var, const ICFGNode* node) const;
    virtual bool hasAbsValue(const SVFVar* var, const ICFGNode* node) const;

    /// Whether an uninitialized numerical alternative is present.
    virtual bool numericalValueMayBeUninitialized(
        const ValVar* var, const ICFGNode* node) const;

    /// Write both scalar facets without constructing an intermediate value
    /// object. Sparse subclasses re-route ValVar writes to the def-site.
    virtual void updateValue(const ValVar* var,
                             const SVF::Interval& interval,
                             const SVF::AddressSet& addresses,
                             const ICFGNode* node);
    virtual void updateValue(const ObjVar* var,
                             const SVF::Interval& interval,
                             const SVF::AddressSet& addresses,
                             const ICFGNode* node);
    virtual void updateValue(const SVFVar* var,
                             const SVF::Interval& interval,
                             const SVF::AddressSet& addresses,
                             const ICFGNode* node);
    virtual void addUninitializedNumericalAlternative(
        const ValVar* var, const ICFGNode* node);

    void updateInterval(const SVFVar* var,
                        const SVF::Interval& interval,
                        const ICFGNode* node);
    void updateAddressSet(const SVFVar* var,
                          const SVF::AddressSet& addresses,
                          const ICFGNode* node);

    /// Representation-independent memory and lifetime access uses native
    /// abstract locations; no encoded integer-address protocol is exposed.
    virtual SVF::Interval getMemoryInterval(
        SVF::Location location, const ICFGNode* node);
    virtual SVF::AddressSet getMemoryAddressSet(
        SVF::Location location, const ICFGNode* node);
    virtual bool hasMemoryValue(SVF::Location location,
                                const ICFGNode* node) const;
    virtual void updateMemoryValue(SVF::Location location,
                                   const SVF::Interval& interval,
                                   const SVF::AddressSet& addresses,
                                   const ICFGNode* node);
    virtual void markFreedMemory(SVF::Location location,
                                 const ICFGNode* node);
    virtual bool isFreedMemory(SVF::Location location,
                               const ICFGNode* node) const;

    // ---- State Access -------------------------------------------------

    /// Return the authoritative complete state used for control-flow joins
    /// and fixpoint computation.
    virtual const SVF::AbstractDomain& getAbstractState(
        const ICFGNode* node) const;

    virtual bool hasAbsState(const ICFGNode* node) const;

    virtual SVF::Location locationOf(const ObjVar* object) const;
    virtual const ObjVar* objectAt(SVF::Location location) const;

    // ---- GEP / Load-Store / Type Helpers ------------------------------

    SVF::Interval getGepElementIndex(const GepStmt* gep);
    SVF::Interval getGepByteOffset(const GepStmt* gep);
    SVF::AddressSet getGepObjAddrs(
        const ValVar* pointer, const SVF::Interval& offset,
        const ICFGNode* node);

    /// Virtual so full-sparse can layer the GepObj overlay on top.
    virtual void loadValue(const ValVar* pointer,
                           SVF::Interval& interval,
                           SVF::AddressSet& addresses,
                           bool& numericalMayBeUninitialized,
                           const ICFGNode* node);
    virtual void storeValue(const ValVar* pointer,
                            const SVF::Interval& interval,
                            const SVF::AddressSet& addresses,
                            const ICFGNode* node);

    struct AllocationSizeBounds
    {
        u32_t lowerBound;
        u32_t upperBound;
    };

    AllocationSizeBounds getAllocaInstByteSize(const AddrStmt* addr);
    /// Value of an unmodelled input (external result, root argument,
    /// unresolved call): its machine type range for integers, else Top.
    SVF::Interval unknownInterval(const SVFVar* var) const;

    const Set<const ICFGNode*>& getAnalyzedNodes() const
    {
        return allAnalyzedNodes;
    }

protected:
    /// Factory-only construction. External callers use getAEInstance().
    AbstractInterpretation();

    /// SVF's BlackHole is an explicit summary object in the current transfer
    /// policy, not AddressSet object-top. Keeping its Location concrete
    /// matches the pointer-analysis value-flow contract while AddressSet still
    /// supports object-top for clients that require arbitrary modeled objects.
    SVF::AddressSet blackHoleAddressSet() const;
    // ---- Cycle helpers implemented by Box-backed execution modes ----
    // The dense versions write only to trace[cycle_head].  The semi-sparse
    // subclass adds def-site scatter on top for body ValVars.

    /// Clone the complete cycle-head state. Sparse subclasses may first gather
    /// values held at def-sites; dense implementations clone their domain
    /// state directly.
    virtual std::unique_ptr<SVF::AbstractDomain> cloneCycleHeadState(
        const ICFGCycleWTO* cycle);
    /// Widen prev with cur; write the widened state to trace[cycle_head].
    /// Returns true when next == prev (fixpoint).  Semi-sparse subclass
    /// additionally scatters ValVars to their def-sites.
    virtual bool widenCycleState(const SVF::AbstractDomain& prev,
                                 const SVF::AbstractDomain& cur,
                                 const ICFGCycleWTO* cycle);

    /// Narrow prev with cur; write the narrowed state back.  Returns true
    /// when narrowing is disabled or the narrowed state equals prev.
    /// Semi-sparse subclass scatters the narrowed ValVars on non-fixpoint.
    virtual bool narrowCycleState(const SVF::AbstractDomain& prev,
                                  const SVF::AbstractDomain& cur,
                                  const ICFGCycleWTO* cycle);

protected:
    /// Representation-independent state lifecycle used by the shared WTO and
    /// call/return drivers. Dense and sparse analyses provide different
    /// storage implementations behind this small surface.
    virtual void resetAbstractState(const ICFGNode* node);
    virtual void copyAbstractState(const ICFGNode* source,
                                   const ICFGNode* destination);
    virtual std::unique_ptr<SVF::AbstractDomain> cloneAbstractState(
        const ICFGNode* node) const;

    virtual void beginAbstractState(const ICFGNode*) {}

    /// Pull-based state merge: read abstractTrace[pred] for each predecessor,
    /// apply branch refinement for conditional IntraCFGEdges, and join into
    /// abstractTrace[node]. Returns true if at least one predecessor had state.
    /// Virtual so full-sparse can layer per-MRSVFGNode obj pulls on top of the
    /// base ICFG-edge merge.
    virtual bool mergeStatesFromPredecessors(const ICFGNode* node);

    /// Representation-independent feasibility query used by shared transfer
    /// code.  Native dense domains apply the constraint directly.
    virtual bool isBranchEdgeFeasibleAt(const IntraCFGEdge* edge,
                                        const ICFGNode* predecessor);

    bool isSwitchDefaultEdge(const IntraCFGEdge* edge) const;
    bool isSwitchBranchEdgeFeasible(const IntraCFGEdge* edge,
                                    const SVF::Interval& input) const;
    bool collectSwitchRefinement(const IntraCFGEdge* edge,
                                 SVF::AbstractDomain& state);

    /// Collect branch-induced interval refinement after a feasible edge has
    /// been selected for normal CFG-state merging.
    bool collectBranchRefinement(const IntraCFGEdge* edge,
                                 SVF::AbstractDomain& state);

    /// Hook called by collectBranchRefinement for each object narrowed by the
    /// branch. Dense and semi-sparse implementations meet the constraint into
    /// the transient edge state; full-sparse records it for MemorySSA flow.
    virtual bool recordBranchRefinement(
        NodeID objId, const SVF::Interval& narrowed,
        SVF::AbstractDomain& state, const ICFGNode* loadIcfg,
        const ICFGNode* succ);

protected:
    /// Initialize abstract state for the global ICFG node and process global
    /// statements
    virtual void handleGlobalNode();

    /// Materialise the value produced by an AddrStmt without prescribing a
    /// concrete state representation.
    virtual void initializeObjectValue(const ObjVar* object,
                                       SVF::Interval& interval,
                                       SVF::AddressSet& addresses,
                                       const ICFGNode* node);

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
    void handleICFGNode(const ICFGNode* node);

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

    Set<const ICFGNode*>
    allAnalyzedNodes; // All nodes ever analyzed (across all entry points)

    std::vector<std::unique_ptr<AEDetector>> detectors;
    AbsExtAPI* utils;

protected:

    virtual State& ensureState(const ICFGNode* node);
    virtual const State& state(const ICFGNode* node) const;
    State topState() const;
    State bottomState() const;
    std::unique_ptr<SVF::NumericalDomain> makeNumericalDomain(
        bool bottom) const;

    void assignValue(State& state, SVF::Variable variable,
                     const SVF::Interval& interval,
                     const SVF::AddressSet& addresses);
    void assignMemoryValue(State& state,
                           SVF::Variable content,
                           const SVF::Interval& interval,
                           const SVF::AddressSet& addresses);
    /// Strong writes require a definite, non-summary, live memory cell.
    bool canStrongStore(const SVF::AddressSet& pointees,
                        const ObjVar& object, const State& state);
    SVF::Variable memoryVariable(const ObjVar& object,
                                 const State& state) const;
    void assignInterval(State& state, SVF::Variable variable,
                        const SVF::Interval& interval);
    void constrainInterval(State& state, SVF::Variable variable,
                           const SVF::Interval& interval) const;
    void forgetValue(State& state,
                     SVF::Variable variable) const;
    void assumeBranch(const IntraCFGEdge* edge, State& state);

    SVFIR* svfir{nullptr};
    AEWTO* preAnalysis{nullptr};
    SVFIRAdapter adapter_;
    bool equivalentValues(const State& left, const State& right) const;

    Map<const ICFGNode*, State> abstractTrace_;
    Map<const ICFGNode*, NodeBS> definedValues_;

    bool shouldApplyNarrowing(const FunObjVar* fun);
};
} // namespace SVF
