//===- IntervalExeState.h ----Interval Domain-------------------------//
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

#include "AbstractExecution/ExeState.h"
#include "AbstractExecution/IntervalValue.h"
#include "Util/Z3Expr.h"

#include <iomanip>

namespace SVF
{
class IntervalESBase: public ExeState
{
    friend class SVFIR2ItvExeState;
    friend class RelationSolver;
public:
    typedef Map<u32_t, IntervalValue> VarToValMap;

    typedef VarToValMap LocToValMap;

public:
    /// default constructor
    IntervalESBase() : ExeState(ExeState::IntervalK) {}

    IntervalESBase(VarToValMap &_varToValMap, LocToValMap &_locToValMap) : ExeState(ExeState::IntervalK),
        _varToItvVal(_varToValMap),
        _locToItvVal(_locToValMap) {}

    /// copy constructor
    IntervalESBase(const IntervalESBase &rhs) : ExeState(rhs), _varToItvVal(rhs.getVarToVal()),
        _locToItvVal(rhs.getLocToVal())
    {

    }

    virtual ~IntervalESBase() = default;


    IntervalESBase &operator=(const IntervalESBase &rhs)
    {
        if (rhs != *this)
        {
            _varToItvVal = rhs._varToItvVal;
            _locToItvVal = rhs._locToItvVal;
            ExeState::operator=(rhs);
        }
        return *this;
    }

    /// move constructor
    IntervalESBase(IntervalESBase &&rhs) : ExeState(std::move(rhs)),
        _varToItvVal(std::move(rhs._varToItvVal)),
        _locToItvVal(std::move(rhs._locToItvVal))
    {

    }

    /// operator= move constructor
    IntervalESBase &operator=(IntervalESBase &&rhs)
    {
        if (&rhs != this)
        {
            _varToItvVal = std::move(rhs._varToItvVal);
            _locToItvVal = std::move(rhs._locToItvVal);
            ExeState::operator=(std::move(rhs));
        }
        return *this;
    }

    /// Set all value bottom
    IntervalESBase bottom() const
    {
        IntervalESBase inv = *this;
        for (auto &item: inv._varToItvVal)
        {
            item.second.set_to_bottom();
        }
        return inv;
    }

    /// Set all value top
    IntervalESBase top() const
    {
        IntervalESBase inv = *this;
        for (auto &item: inv._varToItvVal)
        {
            item.second.set_to_top();
        }
        return inv;
    }

    /// Copy some values and return a new IntervalExeState
    IntervalESBase sliceState(Set<u32_t> &sl)
    {
        IntervalESBase inv;
        for (u32_t id: sl)
        {
            inv._varToItvVal[id] = _varToItvVal[id];
        }
        return inv;
    }

protected:

    VarToValMap _varToItvVal; ///< Map a variable (symbol) to its interval value
    LocToValMap _locToItvVal; ///< Map a memory address to its stored interval value

public:

    /// get memory addresses of variable
    Addrs &getAddrs(u32_t id) override
    {
        return _varToAddrs[id];
    }

    /// get interval value of variable
    inline virtual IntervalValue &operator[](u32_t varId)
    {
        return _varToItvVal[varId];
    }

    /// whether the variable is in varToAddrs table
    inline bool inVarToAddrsTable(u32_t id) const override
    {
        return _varToAddrs.find(id) != _varToAddrs.end();
    }

    /// whether the variable is in varToVal table
    inline virtual bool inVarToValTable(u32_t id) const
    {
        return _varToItvVal.find(id) != _varToItvVal.end();
    }

    /// whether the memory address stores memory addresses
    inline bool inLocToAddrsTable(u32_t id) const override
    {
        return _locToAddrs.find(id) != _locToAddrs.end();
    }

    /// whether the memory address stores interval value
    inline virtual bool inLocToValTable(u32_t id) const
    {
        return _locToItvVal.find(id) != _locToItvVal.end();
    }

    /// get var2val map
    const VarToValMap &getVarToVal() const
    {
        return _varToItvVal;
    }

    /// get loc2val map
    const LocToValMap &getLocToVal() const
    {
        return _locToItvVal;
    }

public:

    /// domain widen with other, and return the widened domain
    IntervalESBase widening(const IntervalESBase &other);

    /// domain narrow with other, and return the narrowed domain
    IntervalESBase narrowing(const IntervalESBase &other);

    /// domain widen with other, important! other widen this.
    void widenWith(const IntervalESBase &other);

    /// domain join with other, important! other widen this.
    void joinWith(const IntervalESBase &other);

    /// domain narrow with other, important! other widen this.
    void narrowWith(const IntervalESBase &other);

    /// domain meet with other, important! other widen this.
    void meetWith(const IntervalESBase &other);


    /// Return int value from an expression if it is a numeral, otherwise return an approximate value
    inline s32_t Interval2NumValue(const IntervalValue &e) const
    {
        //TODO: return concrete value;
        return (s32_t) e.lb().getNumeral();
    }

    ///TODO: Create new interval value
    IntervalValue createIntervalValue(double lb, double ub, NodeID id)
    {
        _varToItvVal[id] = IntervalValue(lb, ub);
        return _varToItvVal[id];
    }

    /// Return true if map has bottom value
    inline bool has_bottom()
    {
        for (auto it = _varToItvVal.begin(); it != _varToItvVal.end(); ++it)
        {
            if (it->second.isBottom())
            {
                return true;
            }
        }
        for (auto it = _locToItvVal.begin(); it != _locToItvVal.end(); ++it)
        {
            if (it->second.isBottom())
            {
                return true;
            }
        }
        return false;
    }

    u32_t hash() const override;

public:
    inline void store(u32_t addr, const IntervalValue &val)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        if (isNullPtr(addr)) return;
        u32_t objId = getInternalID(addr);
        _locToItvVal[objId] = val;
    }

    inline virtual IntervalValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToItvVal.find(objId);
        if(it != _locToItvVal.end())
            return it->second;
        else
        {
            return _locToItvVal[objId];
        }
    }

    inline virtual Addrs &loadAddrs(u32_t addr) override
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToAddrs.find(objId);
        if(it != _locToAddrs.end())
            return it->second;
        else
        {
            return _locToAddrs[objId];
        }
    }

    inline virtual IntervalValue& getLocVal(u32_t id)
    {
        auto it = _locToItvVal.find(id);
        if(it != _locToItvVal.end())
            return it->second;
        else
        {
            return _locToItvVal[id];
        }
    }

    inline virtual Addrs& getLocAddrs(u32_t id)
    {
        auto it = _locToAddrs.find(id);
        if(it != _locToAddrs.end())
            return it->second;
        else
        {
            return _locToAddrs[id];
        }
    }

    /// Print values of all expressions
    void printExprValues(std::ostream &oss) const override;

    std::string toString() const override
    {
        return "";
    }

    bool equals(const IntervalESBase &other) const;

    static bool eqVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs)
    {
        if (lhs.size() != rhs.size()) return false;
        for (const auto &item: lhs)
        {
            auto it = rhs.find(item.first);
            if (it == rhs.end())
                return false;
            if (!item.second.equals(it->second))
            {
                return false;
            }
        }
        return true;
    }


    static bool lessThanVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs)
    {
        if (lhs.empty()) return !rhs.empty();
        for (const auto &item: lhs)
        {
            auto it = rhs.find(item.first);
            if (it == rhs.end()) return false;
            // judge from expr id
            if (item.second.geq(it->second)) return false;
        }
        return true;
    }

    // lhs >= rhs
    static bool geqVarToValMap(const VarToValMap &lhs, const VarToValMap &rhs)
    {
        if (rhs.empty()) return true;
        for (const auto &item: rhs)
        {
            auto it = lhs.find(item.first);
            if (it == lhs.end()) return false;
            // judge from expr id
            if (!it->second.geq(item.second)) return false;
        }
        return true;
    }

    using ExeState::operator==;
    using ExeState::operator!=;
    bool operator==(const IntervalESBase &rhs) const
    {
        return ExeState::operator==(rhs) && eqVarToValMap(_varToItvVal, rhs.getVarToVal()) &&
               eqVarToValMap(_locToItvVal, rhs.getLocToVal());
    }

    bool operator!=(const IntervalESBase &rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const IntervalESBase &rhs) const
    {
        return !(*this >= rhs);
    }


    bool operator>=(const IntervalESBase &rhs) const
    {
        return geqVarToValMap(_varToItvVal, rhs.getVarToVal()) && geqVarToValMap(_locToItvVal, rhs.getLocToVal());
    }

    void clear()
    {
        _locToItvVal.clear();
        _varToItvVal.clear();
        _locToAddrs.clear();
        _varToAddrs.clear();
    }


protected:
    void printTable(const VarToValMap &table, std::ostream &oss) const;

    void printTable(const VarToAddrs &table, std::ostream &oss) const;
};

class IntervalExeState : public IntervalESBase
{
    friend class SVFIR2ItvExeState;
    friend class RelationSolver;

public:
    static IntervalExeState globalES;

public:
    /// default constructor
    IntervalExeState() : IntervalESBase() {}

    IntervalExeState(VarToValMap &_varToValMap, LocToValMap &_locToValMap) : IntervalESBase(_varToValMap, _locToValMap) {}

    /// copy constructor
    IntervalExeState(const IntervalExeState &rhs) : IntervalESBase(rhs)
    {

    }

    virtual ~IntervalExeState() = default;


    IntervalExeState &operator=(const IntervalExeState &rhs)
    {
        IntervalESBase::operator=(rhs);
        return *this;
    }

    /// move constructor
    IntervalExeState(IntervalExeState &&rhs) : IntervalESBase(std::move(rhs))
    {

    }

    /// operator= move constructor
    IntervalExeState &operator=(IntervalExeState &&rhs)
    {
        IntervalESBase::operator=(std::move(rhs));
        return *this;
    }

public:

    /// get memory addresses of variable
    Addrs &getAddrs(u32_t id) override
    {
        auto it = _varToAddrs.find(id);
        if (it != _varToAddrs.end())
            return it->second;
        else
            return globalES._varToAddrs[id];
    }

    /// get interval value of variable
    inline IntervalValue &operator[](u32_t varId) override
    {
        auto localIt = _varToItvVal.find(varId);
        if(localIt != _varToItvVal.end())
            return localIt->second;
        else
        {
            return globalES._varToItvVal[varId];
        }
    }

    /// whether the variable is in varToAddrs table
    inline bool inVarToAddrsTable(u32_t id) const override
    {
        return _varToAddrs.find(id) != _varToAddrs.end() ||
               globalES._varToAddrs.find(id) != globalES._varToAddrs.end();
    }

    /// whether the variable is in varToVal table
    inline bool inVarToValTable(u32_t id) const override
    {
        return _varToItvVal.find(id) != _varToItvVal.end() ||
               globalES._varToItvVal.find(id) != globalES._varToItvVal.end();
    }

    /// whether the memory address stores memory addresses
    inline bool inLocToAddrsTable(u32_t id) const override
    {
        return _locToAddrs.find(id) != _locToAddrs.end() ||
               globalES._locToAddrs.find(id) != globalES._locToAddrs.end();
    }

    /// whether the memory address stores interval value
    inline bool inLocToValTable(u32_t id) const override
    {
        return _locToItvVal.find(id) != _locToItvVal.end() ||
               globalES._locToItvVal.find(id) != globalES._locToItvVal.end();
    }

    inline bool inLocalLocToValTable(u32_t id) const
    {
        return _locToItvVal.find(id) != _locToItvVal.end();
    }

    inline bool inLocalLocToAddrsTable(u32_t id) const
    {
        return _locToAddrs.find(id) != _locToAddrs.end();
    }

public:

    inline void cpyItvToLocal(u32_t varId)
    {
        auto localIt = _varToItvVal.find(varId);
        // local already have varId
        if (localIt != _varToItvVal.end()) return;
        auto globIt = globalES._varToItvVal.find(varId);
        if (globIt != globalES._varToItvVal.end())
        {
            _varToItvVal[varId] = globIt->second;
        }
    }

    /// domain widen with other, and return the widened domain
    IntervalExeState widening(const IntervalExeState &other);

    /// domain narrow with other, and return the narrowed domain
    IntervalExeState narrowing(const IntervalExeState &other);

    /// domain widen with other, important! other widen this.
    void widenWith(const IntervalExeState &other);

    /// domain join with other, important! other widen this.
    void joinWith(const IntervalExeState &other);

    /// domain narrow with other, important! other widen this.
    void narrowWith(const IntervalExeState &other);

    /// domain meet with other, important! other widen this.
    void meetWith(const IntervalExeState &other);

    u32_t hash() const override;

public:

    inline IntervalValue &load(u32_t addr) override
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToItvVal.find(objId);
        if(it != _locToItvVal.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToItvVal.find(objId);
            if(globIt != globalES._locToItvVal.end())
                return globIt->second;
            else
                return _locToItvVal[objId];
        }
    }

    inline Addrs &loadAddrs(u32_t addr) override
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        auto it = _locToAddrs.find(objId);
        if(it != _locToAddrs.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToAddrs.find(objId);
            if(globIt != globalES._locToAddrs.end())
                return globIt->second;
            else
                return _locToAddrs[objId];
        }
    }

    inline IntervalValue& getLocVal(u32_t id) override
    {
        auto it = _locToItvVal.find(id);
        if(it != _locToItvVal.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToItvVal.find(id);
            if(globIt != globalES._locToItvVal.end())
                return globIt->second;
            else
                return _locToItvVal[id];
        }
    }

    inline Addrs& getLocAddrs(u32_t id) override
    {
        auto it = _locToAddrs.find(id);
        if(it != _locToAddrs.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToAddrs.find(id);
            if(globIt != globalES._locToAddrs.end())
                return globIt->second;
            else
                return _locToAddrs[id];
        }
    }

    bool equals(const IntervalExeState &other) const;

    using ExeState::operator==;
    using ExeState::operator!=;
    bool operator==(const IntervalExeState &rhs) const
    {
        return ExeState::operator==(rhs) && eqVarToValMap(_varToItvVal, rhs._varToItvVal) &&
               eqVarToValMap(_locToItvVal, rhs._locToItvVal);
    }

    bool operator!=(const IntervalExeState &rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(const IntervalExeState &rhs) const
    {
        return !(*this >= rhs);
    }


    bool operator>=(const IntervalExeState &rhs) const
    {
        return geqVarToValMap(_varToItvVal, rhs.getVarToVal()) && geqVarToValMap(_locToItvVal, rhs._locToItvVal);
    }
};
}

template<>
struct std::hash<SVF::IntervalExeState>
{
    size_t operator()(const SVF::IntervalExeState &exeState) const
    {
        return exeState.hash();
    }
};

#endif //Z3_EXAMPLE_INTERVAL_DOMAIN_H
