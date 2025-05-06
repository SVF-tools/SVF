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
#include "Util/Z3Expr.h"

#include <iomanip>

namespace SVF
{
class AbstractState
{
    friend class SVFIR2AbsState;
    friend class RelationSolver;
public:
    typedef Map<u32_t, AbstractValue> VarToAbsValMap;
    typedef VarToAbsValMap AddrToAbsValMap;
    Set<NodeID> _freedAddrs;


public:
    /// default constructor
    AbstractState()
    {
    }

    AbstractState(VarToAbsValMap&_varToValMap, AddrToAbsValMap&_locToValMap) : _varToAbsVal(_varToValMap), _addrToAbsVal(_locToValMap) {}

    /// copy constructor
    AbstractState(const AbstractState&rhs) : _freedAddrs(rhs._freedAddrs), _varToAbsVal(rhs.getVarToVal()), _addrToAbsVal(rhs.getLocToVal())
    {

    }

    virtual ~AbstractState() = default;

    // getGepObjAddrs
    AddressValue getGepObjAddrs(u32_t pointer, IntervalValue offset);

    // initObjVar
    void initObjVar(ObjVar* objVar);
    // getElementIndex
    IntervalValue getElementIndex(const GepStmt* gep);
    // getByteOffset
    IntervalValue getByteOffset(const GepStmt* gep);
    // printAbstractState
    // loadValue
    AbstractValue loadValue(NodeID varId);
    // storeValue
    void storeValue(NodeID varId, AbstractValue val);

    u32_t getAllocaInstByteSize(const AddrStmt *addr);


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
    inline u32_t getIDFromAddr(u32_t addr)
    {
        return _freedAddrs.count(addr) ?  AddressValue::getInternalID(InvalidMemAddr) : AddressValue::getInternalID(addr);
    }

    AbstractState&operator=(const AbstractState&rhs)
    {
        if (rhs != *this)
        {
            _varToAbsVal = rhs._varToAbsVal;
            _addrToAbsVal = rhs._addrToAbsVal;
            _freedAddrs = rhs._freedAddrs;
        }
        return *this;
    }

    /// move constructor
    AbstractState(AbstractState&&rhs) : _varToAbsVal(std::move(rhs._varToAbsVal)),
        _addrToAbsVal(std::move(rhs._addrToAbsVal))
    {

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
        {
            inv._varToAbsVal[id] = _varToAbsVal[id];
        }
        return inv;
    }

    static inline bool isNullMem(u32_t addr)
    {
        return AddressValue::getInternalID(addr) == NullMemAddr;
    }

    static inline bool isInvalidMem(u32_t addr)
    {
        return AddressValue::getInternalID(addr) == InvalidMemAddr;
    }


protected:
    VarToAbsValMap _varToAbsVal; ///< Map a variable (symbol) to its abstract value
    AddrToAbsValMap
    _addrToAbsVal; ///< Map a memory address to its stored abstract value

public:


    /// get abstract value of variable
    inline virtual AbstractValue &operator[](u32_t varId)
    {
        return _varToAbsVal[varId];
    }

    /// get abstract value of variable
    inline virtual const AbstractValue &operator[](u32_t varId) const
    {
        return _varToAbsVal.at(varId);
    }

    /// whether the variable is in varToAddrs table
    inline bool inVarToAddrsTable(u32_t id) const
    {
        if (_varToAbsVal.find(id)!= _varToAbsVal.end())
        {
            if (_varToAbsVal.at(id).isAddr())
            {
                return true;
            }
        }
        return false;
    }

    /// whether the variable is in varToVal table
    inline virtual bool inVarToValTable(u32_t id) const
    {
        if (_varToAbsVal.find(id) != _varToAbsVal.end())
        {
            if (_varToAbsVal.at(id).isInterval())
            {
                return true;
            }
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
    const VarToAbsValMap&getVarToVal() const
    {
        return _varToAbsVal;
    }

    /// get loc2val map
    const AddrToAbsValMap&getLocToVal() const
    {
        return _addrToAbsVal;
    }

public:

    /// domain widen with other, and return the widened domain
    AbstractState widening(const AbstractState&other);

    /// domain narrow with other, and return the narrowed domain
    AbstractState narrowing(const AbstractState&other);

    /// domain join with other, important! other widen this.
    void joinWith(const AbstractState&other);

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


    /**
    * if this NodeID in SVFIR is a pointer, get the pointee type
    * e.g  arr = (int*) malloc(10*sizeof(int))
    *      getPointeeType(arr) -> return int
    * we can set arr[0]='c', arr[1]='c', arr[2]='\0'
    * @param call callnode of memset like api
     */
    const SVFType* getPointeeElement(NodeID id);


    u32_t hash() const;

public:
    inline void store(u32_t addr, const AbstractValue &val)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getIDFromAddr(addr);
        if (isNullMem(addr)) return;
        _addrToAbsVal[objId] = val;
    }

    inline virtual AbstractValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getIDFromAddr(addr);
        return _addrToAbsVal[objId];

    }

    void printAbstractState() const;

    std::string toString() const
    {
        return "";
    }

    bool equals(const AbstractState&other) const;


    static bool eqVarToValMap(const VarToAbsValMap&lhs, const VarToAbsValMap&rhs)
    {
        if (lhs.size() != rhs.size()) return false;
        for (const auto &item: lhs)
        {
            auto it = rhs.find(item.first);
            if (it == rhs.end())
                return false;
            if (!item.second.equals(it->second))
                return false;
            else
            {
            }
        }
        return true;
    }

    static bool lessThanVarToValMap(const VarToAbsValMap&lhs, const VarToAbsValMap&rhs)
    {
        if (lhs.empty()) return !rhs.empty();
        for (const auto &item: lhs)
        {
            auto it = rhs.find(item.first);
            if (it == rhs.end()) return false;
            // judge from expr id
            if (item.second.getInterval().contain(it->second.getInterval())) return false;
        }
        return true;
    }

    // lhs >= rhs
    static bool geqVarToValMap(const VarToAbsValMap&lhs, const VarToAbsValMap&rhs)
    {
        if (rhs.empty()) return true;
        for (const auto &item: rhs)
        {
            auto it = lhs.find(item.first);
            if (it == lhs.end()) return false;
            // judge from expr id
            if (!it->second.getInterval().contain(
                        item.second.getInterval()))
                return false;

        }
        return true;
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

};

}


#endif //Z3_EXAMPLE_INTERVAL_DOMAIN_H
