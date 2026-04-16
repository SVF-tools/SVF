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
#include "AE/Svfexe/AbstractStateManager.h"
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

    /// Get the state manager instance.
    AbstractStateManager* getStateMgr()
    {
        return svfStateMgr;
    }

    // ---------------------------------------------------------------
    //  Convenience wrappers around AbstractStateManager
    // ---------------------------------------------------------------
    /// Read-only access to a node's AbstractState. Mutations must go through
    /// updateAbsState (to replace) or updateAbsValue (to update one variable).
    inline const AbstractState& getAbsState(const ICFGNode* node) const
    {
        return svfStateMgr->getAbstractState(node);
    }

    inline bool hasAbsState(const ICFGNode* node)
    {
        return svfStateMgr->hasAbstractState(node);
    }

    inline void updateAbsState(const ICFGNode* node, const AbstractState& state)
    {
        svfStateMgr->updateAbstractState(node, state);
    }

    inline bool hasAbsValue(const SVFVar* var, const ICFGNode* node)
    {
        return svfStateMgr->hasAbstractValue(var, node);
    }

    inline const AbstractValue& getAbsValue(const SVFVar* var, const ICFGNode* node)
    {
        return svfStateMgr->getAbstractValue(var, node);
    }

    inline void updateAbsValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node)
    {
        svfStateMgr->updateAbstractValue(var, val, node);
    }

    /// Propagate an ObjVar's abstract value from defSite to all its use-sites.
    void propagateObjVarAbsVal(const ObjVar* var, const ICFGNode* defSite);

private:
    /// Initialize abstract state for the global ICFG node and process global statements
    virtual void handleGlobalNode();

    /// Pull-based state merge: read abstractTrace[pred] for each predecessor,
    /// apply branch refinement for conditional IntraCFGEdges, and join into
    /// abstractTrace[node]. Returns true if at least one predecessor had state.
    bool mergeStatesFromPredecessors(const ICFGNode* node);

    /// Returns true if the branch is reachable; narrows as in-place.
    bool isBranchFeasible(const IntraCFGEdge* edge, AbstractState& as);

    /// Handle a call site node: dispatch to ext-call, direct-call, or indirect-call handling
    virtual void handleCallSite(const ICFGNode* node);

    /// Handle a WTO cycle (loop or recursive function) using widening/narrowing iteration
    virtual void handleLoopOrRecursion(const ICFGCycleWTO* cycle, const CallICFGNode* caller = nullptr);

    // ---- Semi-sparse cycle helpers ----
    // ValVars whose def-site is inside the cycle but NOT cycle_head do not
    // flow through cycle_head's merge in semi-sparse mode, so the around-merge
    // widening cannot observe them.  getFullCycleHeadState pulls these ValVars
    // into a single AbstractState snapshot so widen/narrow can treat ValVars
    // and ObjVars uniformly; after widen/narrow we scatter the ValVars back
    // to their def-sites.

    /// Build a full cycle-head AbstractState: the ObjVars currently at
    /// cycle_head combined with every cycle ValVar pulled from its
    /// def-site.  Skips ValVars without a stored value to avoid the
    /// top-fallback contamination.  In dense mode this is equivalent to
    /// trace[cycle_head] since ValVars already live there.
    AbstractState getFullCycleHeadState(const ICFGCycleWTO* cycle);

    /// Widen prev with cur; write the widened state to trace[cycle_head]
    /// and scatter its ValVars back to their def-sites.  Returns true
    /// when the widened result equals prev (fixpoint).
    bool widenCycleState(const AbstractState& prev, const AbstractState& cur,
                         const ICFGCycleWTO* cycle);
    /// Narrow prev with cur; write the narrowed state back and scatter.
    bool narrowCycleState(const AbstractState& prev, const AbstractState& cur,
                          const ICFGCycleWTO* cycle);

    /// Handle a function body via worklist-driven WTO traversal starting from funEntry
    void handleFunction(const ICFGNode* funEntry, const CallICFGNode* caller = nullptr);

    /// Handle an ICFG node: execute statements; return true if state changed
    bool handleICFGNode(const ICFGNode* node);

    /// Dispatch an SVF statement (Addr/Binary/Cmp/Load/Store/Copy/Gep/Select/Phi/Call/Ret) to its handler
    virtual void handleSVFStatement(const SVFStmt* stmt);

    /// Returns true if the cmp-conditional branch is feasible; narrows as in-place.
    bool isCmpBranchFeasible(const IntraCFGEdge* edge, AbstractState& as);

    /// Returns true if the switch branch is feasible; narrows as in-place.
    bool isSwitchBranchFeasible(const IntraCFGEdge* edge, AbstractState& as);

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
    virtual void skipRecursionWithTop(const CallICFGNode *callNode);
    virtual bool isRecursiveCallSite(const CallICFGNode* callNode, const FunObjVar *);
    virtual void handleFunCall(const CallICFGNode* callNode);

    bool skipRecursiveCall(const CallICFGNode* callNode);
    const FunObjVar* getCallee(const CallICFGNode* callNode);
    bool shouldApplyNarrowing(const FunObjVar* fun);

    // there data should be shared with subclasses
    Map<std::string, std::function<void(const CallICFGNode*)>> func_map;

    AbstractStateManager* svfStateMgr{nullptr}; // state management (owns abstractTrace)
    Set<const ICFGNode*> allAnalyzedNodes; // All nodes ever analyzed (across all entry points)
    std::string moduleName;

    std::vector<std::unique_ptr<AEDetector>> detectors;
    AbsExtAPI* utils;


};
}