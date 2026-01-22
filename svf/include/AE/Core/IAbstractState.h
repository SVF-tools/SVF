//===- AbstractState.h -- Abstract State Interface -------------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
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
/*
 * AbstractState.h
 *
 * Interface for abstract state to support runtime polymorphism.
 * This enables switching between different abstract state implementations
 * (e.g., dense vs sparse) at runtime via factory pattern.
 *
 */

#ifndef SVF_ABSTRACTSTATE_H
#define SVF_ABSTRACTSTATE_H

#include "AE/Core/AbstractValue.h"
#include "AE/Core/IntervalValue.h"
#include "SVFIR/SVFVariables.h"
#include <memory>

namespace SVF
{

// Forward declarations
class GepStmt;
class AddrStmt;

/// Abstract interface for abstract state implementations
/// This interface enables runtime polymorphism for different state representations
/// (e.g., DenseAbstractState, SparseAbstractState)
class AbstractState
{
public:
    virtual ~AbstractState() = default;

    //============= Core Domain Operations =============//

    /// Domain join with other state (modifies this state)
    virtual void joinWith(const AbstractState& other) = 0;

    /// Domain meet with other state (modifies this state)
    virtual void meetWith(const AbstractState& other) = 0;

    /// Check equality with another state
    virtual bool equals(const AbstractState& other) const = 0;

    /// Widening operation - returns new widened state
    virtual std::unique_ptr<AbstractState> widening(const AbstractState& other) const = 0;

    /// Narrowing operation - returns new narrowed state
    virtual std::unique_ptr<AbstractState> narrowing(const AbstractState& other) const = 0;

    /// Clone this state
    virtual std::unique_ptr<AbstractState> clone() const = 0;

    //============= Variable Access Operations =============//

    /// Get abstract value of variable (mutable)
    virtual AbstractValue& operator[](u32_t varId) = 0;

    /// Get abstract value of variable (const)
    virtual const AbstractValue& operator[](u32_t varId) const = 0;

    /// Check if variable is in var-to-val table (interval value)
    virtual bool inVarToValTable(u32_t id) const = 0;

    /// Check if variable is in var-to-addrs table (address value)
    virtual bool inVarToAddrsTable(u32_t id) const = 0;

    /// Check if memory address stores interval value
    virtual bool inAddrToValTable(u32_t id) const = 0;

    /// Check if memory address stores address value
    virtual bool inAddrToAddrsTable(u32_t id) const = 0;

    //============= Memory Operations =============//

    /// Load value from addresses pointed by varId
    virtual AbstractValue loadValue(NodeID varId) = 0;

    /// Store value to addresses pointed by varId
    virtual void storeValue(NodeID varId, AbstractValue val) = 0;

    /// Store value to a specific memory address
    virtual void store(u32_t addr, const AbstractValue& val) = 0;

    /// Load value from a specific memory address
    virtual AbstractValue& load(u32_t addr) = 0;

    //============= GEP Operations =============//

    /// Get element index for GEP statement
    virtual IntervalValue getElementIndex(const GepStmt* gep) = 0;

    /// Get byte offset for GEP statement
    virtual IntervalValue getByteOffset(const GepStmt* gep) = 0;

    /// Get GEP object addresses given pointer and offset
    virtual AddressValue getGepObjAddrs(u32_t pointer, IntervalValue offset) = 0;

    //============= Utility Operations =============//

    /// Get internal ID from virtual memory address
    virtual u32_t getIDFromAddr(u32_t addr) = 0;

    /// Initialize object variable
    virtual void initObjVar(ObjVar* objVar) = 0;

    /// Get byte size of alloca instruction
    virtual u32_t getAllocaInstByteSize(const AddrStmt* addr) = 0;

    /// Get pointee element type
    virtual const SVFType* getPointeeElement(NodeID id) = 0;

    /// Add address to freed addresses set
    virtual void addToFreedAddrs(NodeID addr) = 0;

    /// Check if address is freed
    virtual bool isFreedMem(u32_t addr) const = 0;

    /// Print abstract state for debugging
    virtual void printAbstractState() const = 0;

    /// Get state type name for identification
    virtual const char* getStateName() const = 0;

    //============= Static Address Utilities =============//
    // Note: These remain static in AbstractState for backward compatibility
    // and can be called via AbstractState::isNullMem, etc.
};

} // namespace SVF

#endif // SVF_ABSTRACTSTATE_H
