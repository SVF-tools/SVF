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

    typedef VarToAbsValMap LocToAbsValMap;

public:
    /// default constructor
    AbstractState() {}

    AbstractState(VarToAbsValMap&_varToValMap, LocToAbsValMap&_locToValMap) : _varToAbsVal(_varToValMap),
        _locToAbsVal(_locToValMap) {}

    /// copy constructor
    AbstractState(const AbstractState&rhs) : _varToAbsVal(rhs.getVarToVal()),
        _locToAbsVal(rhs.getLocToVal())
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
            _locToAbsVal = rhs._locToAbsVal;
        }
        return *this;
    }

    /// move constructor
    AbstractState(AbstractState&&rhs) : _varToAbsVal(std::move(rhs._varToAbsVal)),
        _locToAbsVal(std::move(rhs._locToAbsVal))
    {

    }

    /// operator= move constructor
    AbstractState&operator=(AbstractState&&rhs)
    {
        if (&rhs != this)
        {
            _varToAbsVal = std::move(rhs._varToAbsVal);
            _locToAbsVal = std::move(rhs._locToAbsVal);
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
    LocToAbsValMap _locToAbsVal; ///< Map a memory address to its stored abstract value

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
    inline bool inLocToAddrsTable(u32_t id) const
    {
        if (_locToAbsVal.find(id)!= _locToAbsVal.end())
        {
            if (_locToAbsVal.at(id).isAddr())
            {
                return true;
            }
        }
        return false;
    }

    /// whether the memory address stores abstract value
    inline virtual bool inLocToValTable(u32_t id) const
    {
        if (_locToAbsVal.find(id) != _locToAbsVal.end())
        {
            if (_locToAbsVal.at(id).isInterval())
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
    const LocToAbsValMap&getLocToVal() const
    {
        return _locToAbsVal;
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
        for (auto it = _locToAbsVal.begin(); it != _locToAbsVal.end(); ++it)
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
        _locToAbsVal[objId] = val;
    }

    inline virtual AbstractValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToAbsVal.find(objId);
        if(it != _locToAbsVal.end())
            return it->second;
        else
        {
            _locToAbsVal[objId] = IntervalValue::top();
            return _locToAbsVal[objId];
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
            if (item.second.getInterval().geq(it->second.getInterval())) return false;
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
                if (!it->second.getInterval().geq(item.second.getInterval()))
                    return false;
            }

        }
        return true;
    }

    bool operator==(const AbstractState&rhs) const
    {
        return  eqVarToValMap(_varToAbsVal, rhs.getVarToVal()) &&
                eqVarToValMap(_locToAbsVal, rhs.getLocToVal());
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
        return geqVarToValMap(_varToAbsVal, rhs.getVarToVal()) && geqVarToValMap(_locToAbsVal, rhs.getLocToVal());
    }

    void clear()
    {
        _locToAbsVal.clear();
        _varToAbsVal.clear();
    }


protected:
    void printTable(const VarToAbsValMap&table, std::ostream &oss) const;

};

class SparseAbstractState : public AbstractState
{
    friend class SVFIR2AbsState;
    friend class RelationSolver;

public:
    static SparseAbstractState globalES;

public:
    /// default constructor
    SparseAbstractState() : AbstractState() {}

    SparseAbstractState(VarToAbsValMap&_varToValMap, LocToAbsValMap&_locToValMap) : AbstractState(_varToValMap, _locToValMap) {}

    /// copy constructor
    SparseAbstractState(const SparseAbstractState&rhs) : AbstractState(rhs)
    {

    }

    virtual ~SparseAbstractState() = default;

    SparseAbstractState&operator=(const SparseAbstractState&rhs)
    {
        AbstractState::operator=(rhs);
        return *this;
    }

    virtual void printExprValues(std::ostream &oss) const;

    /// move constructor
    SparseAbstractState(SparseAbstractState&&rhs) : AbstractState(std::move(rhs))
    {

    }

    /// operator= move constructor
    SparseAbstractState&operator=(SparseAbstractState&&rhs)
    {
        AbstractState::operator=(std::move(rhs));
        return *this;
    }

public:

    /// get memory addresses of variable
    AbstractValue &getAddrs(u32_t id)
    {
        if (_varToAbsVal.find(id)!= _varToAbsVal.end())
        {
            return _varToAbsVal[id];
        }
        else if (globalES._varToAbsVal.find(id)!= globalES._varToAbsVal.end())
        {
            return globalES._varToAbsVal[id];
        }
        else
        {
            globalES._varToAbsVal[id] = AddressValue();
            return globalES._varToAbsVal[id];
        }
    }

    /// get abstract value of variable
    inline AbstractValue &operator[](u32_t varId)
    {
        auto localIt = _varToAbsVal.find(varId);
        if(localIt != _varToAbsVal.end())
            return localIt->second;
        else
        {
            return globalES._varToAbsVal[varId];
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
            else
            {
                return false;
            }
        }
        else if (globalES._varToAbsVal.find(id)!= globalES._varToAbsVal.end())
        {
            if (globalES._varToAbsVal[id].isAddr())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    /// whether the variable is in varToVal table
    inline bool inVarToValTable(u32_t id) const
    {
        if (_varToAbsVal.find(id)!= _varToAbsVal.end())
        {
            if (_varToAbsVal.at(id).isInterval())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else if (globalES._varToAbsVal.find(id)!= globalES._varToAbsVal.end())
        {
            if (globalES._varToAbsVal[id].isInterval())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    /// whether the memory address stores memory addresses
    inline bool inLocToAddrsTable(u32_t id) const
    {
        if (_locToAbsVal.find(id)!= _locToAbsVal.end())
        {
            if (_locToAbsVal.at(id).isAddr())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else if (globalES._locToAbsVal.find(id)!= globalES._locToAbsVal.end())
        {
            if (globalES._locToAbsVal[id].isAddr())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    /// whether the memory address stores abstract value
    inline bool inLocToValTable(u32_t id) const
    {
        if (_locToAbsVal.find(id)!= _locToAbsVal.end())
        {
            if (_locToAbsVal.at(id).isInterval())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else if (globalES._locToAbsVal.find(id)!= globalES._locToAbsVal.end())
        {
            if (globalES._locToAbsVal[id].isInterval())
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    inline bool inLocalLocToValTable(u32_t id) const
    {
        if (_locToAbsVal.find(id)!= _locToAbsVal.end())
        {
            return _locToAbsVal.at(id).isInterval();
        }
        else
            return false;
    }

    inline bool inLocalLocToAddrsTable(u32_t id) const
    {
        if (_locToAbsVal.find(id)!= _locToAbsVal.end())
        {
            return _locToAbsVal.at(id).isAddr();
        }
        else
            return false;
    }

public:

    inline void cpyItvToLocal(u32_t varId)
    {
        auto localIt = _varToAbsVal.find(varId);
        // local already have varId
        if (localIt != _varToAbsVal.end()) return;
        auto globIt = globalES._varToAbsVal.find(varId);
        if (globIt != globalES._varToAbsVal.end())
        {
            _varToAbsVal[varId] = globIt->second;
        }
    }

    /// domain widen with other, and return the widened domain
    SparseAbstractState widening(const SparseAbstractState&other);

    /// domain narrow with other, and return the narrowed domain
    SparseAbstractState narrowing(const SparseAbstractState&other);

    /// domain widen with other, important! other widen this.
    void widenWith(const SparseAbstractState&other);

    /// domain join with other, important! other widen this.
    void joinWith(const SparseAbstractState&other);

    /// domain narrow with other, important! other widen this.
    void narrowWith(const SparseAbstractState&other);

    /// domain meet with other, important! other widen this.
    void meetWith(const SparseAbstractState&other);

    u32_t hash() const;

public:

    inline AbstractValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToAbsVal.find(objId);
        if(it != _locToAbsVal.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToAbsVal.find(objId);
            if(globIt != globalES._locToAbsVal.end())
                return globIt->second;
            else
            {
                return globalES._locToAbsVal[objId];
            }

        }
    }

    bool equals(const SparseAbstractState&other) const;

    bool operator==(const SparseAbstractState&rhs) const
    {
        return eqVarToValMap(_varToAbsVal, rhs._varToAbsVal) &&
               eqVarToValMap(_locToAbsVal, rhs._locToAbsVal);
    }

    bool operator!=(const SparseAbstractState&rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const SparseAbstractState&rhs) const
    {
        return !(*this >= rhs);
    }


    bool operator>=(const SparseAbstractState&rhs) const
    {
        return geqVarToValMap(_varToAbsVal, rhs.getVarToVal()) && geqVarToValMap(_locToAbsVal, rhs._locToAbsVal);
    }
};
}

template<>
struct std::hash<SVF::SparseAbstractState>
{
    size_t operator()(const SVF::SparseAbstractState&exeState) const
    {
        return exeState.hash();
    }
};

#endif //Z3_EXAMPLE_INTERVAL_DOMAIN_H
