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

#ifndef Z3_EXAMPLE_INTERVAL_DOMAIN_H
#define Z3_EXAMPLE_INTERVAL_DOMAIN_H

#include "AE/Core/IntervalValue.h"
#include "AE/Core/AbstractValue.h"
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

public:
    /// default constructor
    AbstractState()
    {
    }

    AbstractState(VarToAbsValMap&_varToValMap, AddrToAbsValMap&_locToValMap) : _varToAbsVal(_varToValMap), _addrToAbsVal(_locToValMap) {}

    /// copy constructor
    AbstractState(const AbstractState&rhs) : _varToAbsVal(rhs.getVarToVal()), _addrToAbsVal(rhs.getLocToVal())
    {

    }

    virtual ~AbstractState() = default;


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

    /// Return the internal index if idx is an address otherwise return the value of idx
    static inline u32_t getInternalID(u32_t idx)
    {
        return AddressValue::getInternalID(idx);
    }

    static inline bool isNullPtr(u32_t addr)
    {
        return getInternalID(addr) == 0;
    }

    AbstractState&operator=(const AbstractState&rhs)
    {
        if (rhs != *this)
        {
            _varToAbsVal = rhs._varToAbsVal;
            _addrToAbsVal = rhs._addrToAbsVal;
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

    /// get memory addresses of variable
    AbstractValue &getAddrs(u32_t id)
    {
        if (_varToAbsVal.find(id)!= _varToAbsVal.end())
        {
            return _varToAbsVal[id];
        }
        else
        {
            _varToAbsVal[id] = AddressValue();
            return _varToAbsVal[id];
        }
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

    /// domain widen with other, important! other widen this.
    void widenWith(const AbstractState&other);

    /// domain join with other, important! other widen this.
    void joinWith(const AbstractState&other);

    /// domain narrow with other, important! other widen this.
    void narrowWith(const AbstractState&other);

    /// domain meet with other, important! other widen this.
    void meetWith(const AbstractState&other);


    /// Return int value from an expression if it is a numeral, otherwise return an approximate value
    inline s32_t Interval2NumValue(const IntervalValue &e) const
    {
        //TODO: return concrete value;
        return (s32_t) e.lb().getNumeral();
    }

    /// Return true if map has bottom value
    inline bool has_bottom()
    {
        for (auto it = _varToAbsVal.begin(); it != _varToAbsVal.end(); ++it)
        {
            if (it->second.isInterval())
            {
                if (it->second.getInterval().isBottom())
                {
                    return true;
                }
            }
        }
        for (auto it = _addrToAbsVal.begin(); it != _addrToAbsVal.end(); ++it)
        {
            if (it->second.isInterval())
            {
                if (it->second.getInterval().isBottom())
                {
                    return true;
                }
            }
        }
        return false;
    }

    u32_t hash() const;

public:
    inline void store(u32_t addr, const AbstractValue &val)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        if (isNullPtr(addr)) return;
        u32_t objId = getInternalID(addr);
        _addrToAbsVal[objId] = val;
    }

    inline virtual AbstractValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _addrToAbsVal.find(objId);
        if(it != _addrToAbsVal.end())
            return it->second;
        else
        {
            _addrToAbsVal[objId] = IntervalValue::top();
            return _addrToAbsVal[objId];
        }
    }


    /// Print values of all expressions
    void printExprValues(std::ostream &oss) const;

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
            if (item.second.getType() == it->second.getType())
            {
                if (item.second.isInterval())
                {
                    if (!item.second.getInterval().equals(it->second.getInterval()))
                    {
                        return false;
                    }
                }
                else if (item.second.isAddr())
                {
                    if (!item.second.getAddrs().equals(it->second.getAddrs()))
                    {
                        return false;
                    }
                }
            }
            else
            {
                return false;
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
            if (it->second.isInterval() && item.second.isInterval())
            {
                if (!it->second.getInterval().contain(
                            item.second.getInterval()))
                    return false;
            }

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
    }


protected:
    void printTable(const VarToAbsValMap&table, std::ostream &oss) const;

};

}


#endif //Z3_EXAMPLE_INTERVAL_DOMAIN_H
