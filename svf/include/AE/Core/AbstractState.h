//===- AbstractExeState.h ----Interval Domain-------------------------//
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
 * IntervalExeState.h
 *
 *  Created on: Jul 9, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 *                         [-oo,+oo]
 *          /           /            \           \
 *       [-oo,1] ... [-oo,10] ... [-1,+oo] ... [0,+oo]
 *          \           \           /          /
 *           \            [-1,10]            /
 *            \        /         \         /
 *       ...   [-1,1]      ...     [0,10]      ...
 *           \    |    \         /       \    /
 *       ...   [-1,0]    [0,1]    ...     [1,9]  ...
 *           \    |   \    |   \        /
 *       ...  [-1,-1]  [0,0]     [1,1]  ...
 *         \    \        \        /      /
 *                          ⊥
 */
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)

#ifndef Z3_EXAMPLE_INTERVAL_DOMAIN_H
#define Z3_EXAMPLE_INTERVAL_DOMAIN_H

#include "AE/Core/AbstractValue.h"
#include "AE/Core/IntervalValue.h"
#include "SVFIR/SVFVariables.h"

namespace SVF
{
class AbstractState
{
    friend class SVFIR2AbsState;
    friend class RelationSolver;
public:
    typedef Map<u32_t, AbstractValue> VarToAbsValMap;
    typedef VarToAbsValMap AddrToAbsValMap;
    /// default constructor
    AbstractState()
    {
    }

    AbstractState(VarToAbsValMap&_varToValMap, AddrToAbsValMap&_locToValMap) : _varToAbsVal(_varToValMap), _addrToAbsVal(_locToValMap) {}

    /// copy constructor
    AbstractState(const AbstractState&rhs) : _varToAbsVal(rhs.getVarToVal()), _addrToAbsVal(rhs.getLocToVal()), _freedAddrs(rhs._freedAddrs)
    {

    }

    virtual ~AbstractState() = default;

    // initObjVar
    void initObjVar(const ObjVar* objVar);


    /// The physical address starts with 0x7f...... + idx
    static inline u32_t getVirtualMemAddress(u32_t idx)
    {
        return AddressValue::getVirtualMemAddress(idx);
    }

    /// Check bit value of val start with 0x7F000000, filter by 0xFF000000
    static inline bool isVirtualMemAddress(u32_t val)
    {
        return AddressValue::isVirtualMemAddress(val);
    }

    /// Return the internal index if addr is an address otherwise return the value of idx
    inline u32_t getIDFromAddr(u32_t addr) const
    {
        return _freedAddrs.count(addr) ?  AddressValue::getInternalID(BlackHoleObjAddr) : AddressValue::getInternalID(addr);
    }

    /// move constructor
    AbstractState(AbstractState&&rhs) : _varToAbsVal(std::move(rhs._varToAbsVal)),
        _addrToAbsVal(std::move(rhs._addrToAbsVal)),
        _freedAddrs(std::move(rhs._freedAddrs))
    {

    }

    /// Set all value bottom
    AbstractState bottom() const
    {
        AbstractState inv = *this;
        for (auto &item: inv._varToAbsVal)
        {
            if (item.second.isInterval())
                item.second.getInterval().set_to_bottom();
        }
        return inv;
    }

    /// Set all value top
    AbstractState top() const
    {
        AbstractState inv = *this;
        for (auto &item: inv._varToAbsVal)
        {
            if (item.second.isInterval())
                item.second.getInterval().set_to_top();
        }
        return inv;
    }

    /// Copy some values and return a new IntervalExeState
    AbstractState sliceState(Set<u32_t> &sl)
    {
        AbstractState inv;
        for (u32_t id: sl)
            inv._varToAbsVal[id] = _varToAbsVal[id];
        return inv;
    }

    static inline bool isNullMem(u32_t addr)
    {
        return addr == NullMemAddr;
    }

    static inline bool isBlackHoleObjAddr(u32_t addr)
    {
        return addr == BlackHoleObjAddr;
    }


protected:
    VarToAbsValMap _varToAbsVal; ///< Map a variable (symbol) to its abstract value
    AddrToAbsValMap _addrToAbsVal; ///< Map a memory address to its stored abstract value
    Set<NodeID> _freedAddrs;

public:


    /// get abstract value of variable
    inline virtual AbstractValue &operator[](u32_t varId)
    {
        assert(!isVirtualMemAddress(varId) && "varId is a virtual memory address, use load() instead");
        return _varToAbsVal[varId];
    }

    /// get abstract value of variable
    inline virtual const AbstractValue &operator[](u32_t varId) const
    {
        assert(!isVirtualMemAddress(varId) && "varId is a virtual memory address, use load() instead");
        return _varToAbsVal.at(varId);
    }

    inline virtual AbstractValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getIDFromAddr(addr);
        return _addrToAbsVal[objId];
    }

    inline virtual const AbstractValue &load(u32_t addr) const
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getIDFromAddr(addr);
        return _addrToAbsVal.at(objId);
    }

    inline void store(u32_t addr, const AbstractValue &val)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getIDFromAddr(addr);
        if (isNullMem(addr)) return;
        _addrToAbsVal[objId] = val;
    }

    /// whether the variable is in varToAddrs table
    inline bool inVarToAddrsTable(u32_t id) const
    {
        if (_varToAbsVal.find(id)!= _varToAbsVal.end())
        {
            if (_varToAbsVal.at(id).isAddr())
                return true;
        }
        return false;
    }

    /// whether the variable is in varToVal table
    inline virtual bool inVarToValTable(u32_t id) const
    {
        if (_varToAbsVal.find(id) != _varToAbsVal.end())
        {
            if (_varToAbsVal.at(id).isInterval())
                return true;
        }
        return false;
    }

    /// whether the memory address stores memory addresses
    inline bool inAddrToAddrsTable(u32_t id) const
    {
        if (_addrToAbsVal.find(id)!= _addrToAbsVal.end())
        {
            if (_addrToAbsVal.at(id).isAddr())
            {
                return true;
            }
        }
        return false;
    }

    /// whether the memory address stores abstract value
    inline virtual bool inAddrToValTable(u32_t id) const
    {
        if (_addrToAbsVal.find(id) != _addrToAbsVal.end())
        {
            if (_addrToAbsVal.at(id).isInterval())
            {
                return true;
            }
        }
        return false;
    }

    /// get var2val map
    inline const VarToAbsValMap&getVarToVal() const
    {
        return _varToAbsVal;
    }

    /// get loc2val map
    inline const AddrToAbsValMap&getLocToVal() const
    {
        return _addrToAbsVal;
    }

    /// domain widen with other, and return the widened domain
    AbstractState widening(const AbstractState&other);

    /// domain narrow with other, and return the narrowed domain
    AbstractState narrowing(const AbstractState&other);

    /// domain join with other, important! other widen this.
    void joinWith(const AbstractState&other);


    /// Replace address-taken (ObjVar) state with other's, preserving ValVar state.
    void updateAddrStateOnly(const AbstractState& other)
    {
        _addrToAbsVal = other._addrToAbsVal;
        _freedAddrs = other._freedAddrs;
    }

    /// domain meet with other, important! other widen this.
    void meetWith(const AbstractState&other);

    void addToFreedAddrs(NodeID addr)
    {
        _freedAddrs.insert(addr);
    }

    bool isFreedMem(u32_t addr) const
    {
        return _freedAddrs.find(addr) != _freedAddrs.end();
    }


    void printAbstractState() const;

    std::string toString() const;

    u32_t hash() const;

    // lhs == rhs for varToValMap
    bool eqVarToValMap(const VarToAbsValMap&lhs, const VarToAbsValMap&rhs) const;
    // lhs >= rhs for varToValMap
    bool geqVarToValMap(const VarToAbsValMap&lhs, const VarToAbsValMap&rhs) const;
    // lhs == rhs for AbstractState
    bool equals(const AbstractState&other) const;

    /// Assignment operator
    AbstractState&operator=(const AbstractState&rhs)
    {
        if (&rhs != this)
        {
            _varToAbsVal = rhs._varToAbsVal;
            _addrToAbsVal = rhs._addrToAbsVal;
            _freedAddrs = rhs._freedAddrs;
        }
        return *this;
    }

    /// operator= move constructor
    AbstractState&operator=(AbstractState&&rhs)
    {
        if (&rhs != this)
        {
            _varToAbsVal = std::move(rhs._varToAbsVal);
            _addrToAbsVal = std::move(rhs._addrToAbsVal);
            _freedAddrs = std::move(rhs._freedAddrs);
        }
        return *this;
    }

    bool operator==(const AbstractState&rhs) const
    {
        return  eqVarToValMap(_varToAbsVal, rhs.getVarToVal()) &&
                eqVarToValMap(_addrToAbsVal, rhs.getLocToVal());
    }

    bool operator!=(const AbstractState&rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const AbstractState&rhs) const
    {
        return !(*this >= rhs);
    }

    bool operator>=(const AbstractState&rhs) const
    {
        return geqVarToValMap(_varToAbsVal, rhs.getVarToVal()) && geqVarToValMap(_addrToAbsVal, rhs.getLocToVal());
    }

    void clear()
    {
        _addrToAbsVal.clear();
        _varToAbsVal.clear();
        _freedAddrs.clear();
    }

    /// Drop all top-level variables (ValVars), keeping ObjVar storage and
    /// freed addresses intact. Used when building a cycle snapshot so the
    /// ValVar set is controlled by the caller rather than whatever was
    /// cached at the seed node.
    void clearValVars()
    {
        _varToAbsVal.clear();
    }


};

}


#endif //Z3_EXAMPLE_INTERVAL_DOMAIN_H
