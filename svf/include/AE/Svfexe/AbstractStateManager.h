//===- AbstractStateManager.h -- State management for abstract execution --//
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
//  Created on: Mar 2026
//      Author: Jiawei Wang
//
#pragma once
#include "AE/Core/AbstractState.h"
#include "SVFIR/SVFIR.h"

namespace SVF
{

class GepStmt;
class AddrStmt;
class SVFG;
class AndersenWaveDiff;

/// Manages abstract states across ICFG nodes and provides a unified API
/// for reading/writing abstract values. Encapsulates the dense vs.
/// semi-sparse lookup strategy so that all consumers (updateStateOnXxx,
/// AEDetector, AbsExtAPI) are sparsity-agnostic.
///
/// Two sparsity-dependent behaviors live here:
///   1. getAbstractValue(ValVar*): dense reads from current node's state;
///      semi-sparse pulls from def-site.
///   2. joinWith (inside AbstractState): dense merges all variables;
///      semi-sparse skips ValVar merge.
class AbstractStateManager
{
public:
    AbstractStateManager(SVFIR* svfir, AndersenWaveDiff* pta);
    ~AbstractStateManager();

    // ===----------------------------------------------------------------------===//
    //  Abstract Value Access API
    // ===----------------------------------------------------------------------===//

    /// Read a top-level variable's abstract value.
    /// Dense: reads from abstractTrace[node]. Semi-sparse: checks current state
    /// first, then pulls from def-site. Returns top if absent everywhere.
    const AbstractValue& getAbstractValue(const ValVar* var, const ICFGNode* node);

    /// Read an address-taken variable's content via virtual-address load.
    const AbstractValue& getAbstractValue(const ObjVar* var, const ICFGNode* node);

    /// Dispatch to ValVar or ObjVar overload (checks ObjVar first due to inheritance).
    const AbstractValue& getAbstractValue(const SVFVar* var, const ICFGNode* node);

    /// Check whether a ValVar has a real stored value reachable by
    /// getAbstractValue.  Unlike getAbstractValue, this is side-effect free
    /// and does NOT treat the final top-fallback as "present" — so callers
    /// that plan to write the fetched value back (e.g. cycle widen/narrow)
    /// can distinguish a genuine stored value from the top sentinel.
    bool hasAbstractValue(const ValVar* var, const ICFGNode* node) const;

    /// Check whether an ObjVar has a stored value at node.
    bool hasAbstractValue(const ObjVar* var, const ICFGNode* node) const;

    /// Dispatch to ValVar or ObjVar overload.
    bool hasAbstractValue(const SVFVar* var, const ICFGNode* node) const;

    /// Write a top-level variable's abstract value into abstractTrace[node].
    void updateAbstractValue(const ValVar* var, const AbstractValue& val, const ICFGNode* node);

    /// Write an address-taken variable's content via virtual-address store.
    void updateAbstractValue(const ObjVar* var, const AbstractValue& val, const ICFGNode* node);

    /// Dispatch to ValVar or ObjVar overload.
    void updateAbstractValue(const SVFVar* var, const AbstractValue& val, const ICFGNode* node);

    // ===----------------------------------------------------------------------===//
    //  State Access
    // ===----------------------------------------------------------------------===//

    /// Retrieve the abstract state for a given ICFG node. Asserts if absent.
    AbstractState& getAbstractState(const ICFGNode* node);
    void updateAbstractState(const ICFGNode* node, const AbstractState& state);

    /// Check if an abstract state exists for a given ICFG node.
    bool hasAbstractState(const ICFGNode* node);

    /// Retrieve abstract state filtered to specific variable sets.
    void getAbstractState(const Set<const ValVar*>& vars, AbstractState& result, const ICFGNode* node);
    void getAbstractState(const Set<const ObjVar*>& vars, AbstractState& result, const ICFGNode* node);
    void getAbstractState(const Set<const SVFVar*>& vars, AbstractState& result, const ICFGNode* node);

    // ===----------------------------------------------------------------------===//
    //  GEP Helpers
    //
    //  Lifted from AbstractState to use getAbstractValue for index variable lookup.
    // ===----------------------------------------------------------------------===//

    /// Compute the flattened element index for a GepStmt.
    IntervalValue getGepElementIndex(const GepStmt* gep);

    /// Compute the byte offset for a GepStmt.
    IntervalValue getGepByteOffset(const GepStmt* gep);

    /// Compute GEP object addresses for a pointer at a given element offset.
    AddressValue getGepObjAddrs(const ValVar* pointer, IntervalValue offset);

    // ===----------------------------------------------------------------------===//
    //  Load / Store through pointer (combines ValVar lookup + ObjVar access)
    // ===----------------------------------------------------------------------===//

    /// Load value through a pointer: resolve pointer's address set via
    /// getAbstractValue (sparsity-aware), then load from each ObjVar address.
    /// @param pointer  The pointer SVFVar (ValVar).
    /// @param node     The ICFG node providing context.
    /// @return         The joined abstract value from all pointed-to objects.
    AbstractValue loadValue(const ValVar* pointer, const ICFGNode* node);

    /// Store value through a pointer: resolve pointer's address set via
    /// getAbstractValue (sparsity-aware), then store to each ObjVar address.
    /// @param pointer  The pointer SVFVar (ValVar).
    /// @param val      The value to store.
    /// @param node     The ICFG node providing context.
    void storeValue(const ValVar* pointer, const AbstractValue& val, const ICFGNode* node);

    // ===----------------------------------------------------------------------===//
    //  Type / Size Helpers
    // ===----------------------------------------------------------------------===//

    /// Get the pointee type for a pointer variable.
    const SVFType* getPointeeElement(const ObjVar* var, const ICFGNode* node);

    /// Get the byte size of a stack allocation.
    u32_t getAllocaInstByteSize(const AddrStmt* addr);

    // ===----------------------------------------------------------------------===//
    //  Direct Trace Access (for merge, fixpoint, etc.)
    // ===----------------------------------------------------------------------===//

    Map<const ICFGNode*, AbstractState>& getTrace()
    {
        return abstractTrace;
    }
    AbstractState& operator[](const ICFGNode* node)
    {
        return abstractTrace[node];
    }

    // ===----------------------------------------------------------------------===//
    //  Def/Use site queries (sparsity-aware)
    // ===----------------------------------------------------------------------===//

    /// Given an ObjVar and its use-site ICFGNode, find all downstream use-site ICFGNodes.
    Set<const ICFGNode*> getUseSitesOfObjVar(const ObjVar* obj, const ICFGNode* node) const;

    /// Given a ValVar, find all use-site ICFGNodes.
    Set<const ICFGNode*> getUseSitesOfValVar(const ValVar* var) const;

    /// Given a ValVar, find its definition-site ICFGNode.
    const ICFGNode* getDefSiteOfValVar(const ValVar* var) const;

    /// Given an ObjVar and its use-site ICFGNode, find the definition-site ICFGNode.
    const ICFGNode* getDefSiteOfObjVar(const ObjVar* obj, const ICFGNode* node) const;

private:
    SVFIR* svfir;
    SVFG* svfg;
    Map<const ICFGNode*, AbstractState> abstractTrace;
};

} // namespace SVF
