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
 */

#ifndef Z3_EXAMPLE_INTERVAL_DOMAIN_H
#define Z3_EXAMPLE_INTERVAL_DOMAIN_H

#include "AbstractExecution/ExeState.h"
#include "AbstractExecution/IntervalValue.h"
#include "Util/Z3Expr.h"

#include <iomanip>

namespace SVF
{

class IntervalExeState : public ExeState
{
    friend class SVFIR2ItvExeState;

public:
    typedef Map<u32_t, IntervalValue> VarToValMap;

    typedef VarToValMap LocToValMap;

    static IntervalExeState globalES;

protected:
    /// key: nodeID value: Domain Value
    VarToValMap _varToItvVal;
    /// key: nodeID value: Domain Value
    LocToValMap _locToItvVal;
    VAddrToVAddrsID _itvMToMR;


public:
    /// default constructor, default pc is true
    IntervalExeState() : ExeState(ExeState::IntervalK) {}

    /// copy constructor
    IntervalExeState(const IntervalExeState &rhs) : ExeState(rhs), _varToItvVal(rhs.getVarToVal()),
                                                    _locToItvVal(rhs.getLocToVal()), _itvMToMR(rhs.getItvMToMR())
    {

    }

    /// check two interval exe state are equal or not. _varToItvVal and _locToItvVal map should be equivalent
    IntervalExeState &operator=(const IntervalExeState &rhs)
    {
        if (rhs != *this)
        {
            _varToItvVal = rhs._varToItvVal;
            _locToItvVal = rhs._locToItvVal;
            _itvMToMR = rhs._itvMToMR;
            ExeState::operator=(rhs);
        }
        return *this;
    }

    /// move constructor
    IntervalExeState(IntervalExeState &&rhs) : ExeState(std::move(rhs)),
                                               _varToItvVal(std::move(rhs._varToItvVal)),
                                               _locToItvVal(std::move(rhs._locToItvVal)),
                                               _itvMToMR(std::move(rhs._itvMToMR))
    {

    }

    /// operator= move constructor
    IntervalExeState &operator=(IntervalExeState &&rhs)
    {
        if (&rhs != this)
        {
            _varToItvVal = std::move(rhs._varToItvVal);
            _locToItvVal = std::move(rhs._locToItvVal);
            _itvMToMR = std::move(rhs._itvMToMR);
            ExeState::operator=(std::move(rhs));
        }
        return *this;
    }

    /// Set all value bottom
    IntervalExeState bottom()
    {
        IntervalExeState inv = *this;
        for (auto &item: inv._varToItvVal)
        {
            item.second.set_to_bottom();
        }
        return inv;
    }

    /// Set all value top
    IntervalExeState top()
    {
        IntervalExeState inv = *this;
        for (auto &item: inv._varToItvVal)
        {
            item.second.set_to_top();
        }
        return inv;
    }

    /// Copy some values and return a new IntervalExeState
    IntervalExeState sliceState(Set<u32_t> &sl)
    {
        IntervalExeState inv;
        for (u32_t id: sl)
        {
            inv._varToItvVal[id] = _varToItvVal[id];
        }
        return inv;
    }

    VAddrsID &getVAddrs(u32_t id) override
    {
        auto it = _varToVAddrs.find(id);
        if (it != _varToVAddrs.end())
            return it->second;
        else
            return globalES._varToVAddrs[id];
    }

    inline bool inVarToIValTable(u32_t id) const
    {
        return _varToItvVal.find(id) != _varToItvVal.end() ||
               globalES._varToItvVal.find(id) != globalES._varToItvVal.end();
    }

    inline bool inVarToAddrsTable(u32_t id) const override
    {
        return _varToVAddrs.find(id) != _varToVAddrs.end() ||
               globalES._varToVAddrs.find(id) != globalES._varToVAddrs.end();
    }

    inline virtual bool inItvMToMRTable(u32_t id) const
    {
        return _itvMToMR.find(id) != _itvMToMR.end() ||
            globalES._itvMToMR.find(id) != globalES._itvMToMR.end();
    }

    inline virtual bool inLocalItvMToMRTable(u32_t id) const
    {
        return _itvMToMR.find(id) != _itvMToMR.end();
    }

    inline bool inLocToIValTable(u32_t vAddrId) const
    {
        return _locToItvVal.find(vAddrId) != _locToItvVal.end() ||
               globalES._locToItvVal.find(vAddrId) != globalES._locToItvVal.end();
    }

    inline bool inLocalLocToIValTable(u32_t vAddrId) const
    {
        return _locToItvVal.find(vAddrId) != _locToItvVal.end();
    }

    inline bool inVAddrMToMRTable(u32_t id) const override
    {
        return _vAddrMToMR.find(id) != _vAddrMToMR.end() ||
               globalES._vAddrMToMR.find(id) != globalES._vAddrMToMR.end();
    }

    inline virtual bool inLocalVAddrMToMRTable(u32_t id) const
    {
        return _vAddrMToMR.find(id) != _vAddrMToMR.end();
    }

    inline bool inLocToAddrsTable(u32_t vAddrId) const override
    {
        return _locToVAddrs.find(vAddrId) != _locToVAddrs.end() ||
               globalES._locToVAddrs.find(vAddrId) != globalES._locToVAddrs.end();
    }

    inline bool inLocalLocToAddrsTable(u32_t vAddrId) const
    {
        return _locToVAddrs.find(vAddrId) != _locToVAddrs.end();
    }

    bool equals(const IntervalExeState &other) const;

    virtual ~IntervalExeState() = default;


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

    /// get loc2val map
    const VAddrToVAddrsID &getItvMToMR() const
    {
        return _itvMToMR;
    }

    ///  [], call getValueExpr()
    inline IntervalValue &operator[](u32_t varId)
    {
        auto localIt = _varToItvVal.find(varId);
        if(localIt != _varToItvVal.end())
            return localIt->second;
        else
        {
            return globalES._varToItvVal[varId];
        }
    }

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

    /// domain meet with other varToValMap
    void meetWith(VarToValMap *other);


    /// Return int value from an expression if it is a numeral, otherwise return an approximate value
    inline s32_t Interval2NumValue(const IntervalValue &e) const
    {
        //TODO: return concrete value;
        return (s32_t) e.lb().getNumeral();
    }

    ///TODO: Create new inteval value
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
    inline void store(u32_t addr, const IntervalValue &val);

    inline IntervalValue &load(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        auto it = _itvMToMR.find(addr);
        if(it != _itvMToMR.end())
            return _locToItvVal[it->second];
        else
        {
            auto globIt = globalES._itvMToMR.find(addr);
            if (globIt != globalES._itvMToMR.end()) {
                return globalES._locToItvVal[globIt->second];
            } else {
                auto itvIt = _itvMToMR.find(addr);
                assert(itvIt != _itvMToMR.end() && "null dereference!");
                return _locToItvVal[itvIt->second];
            }
        }
    }

    inline VAddrsID &loadVAddrs(u32_t addr) override
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        auto it = _vAddrMToMR.find(addr);
        if(it != _vAddrMToMR.end())
            return _locToVAddrs[it->second];
        else
        {
            auto globIt = globalES._vAddrMToMR.find(addr);
            if (globIt != globalES._vAddrMToMR.end()) {
                return globalES._locToVAddrs[globIt->second];
            } else {
                auto addrIt = _vAddrMToMR.find(addr);
                assert(addrIt != _vAddrMToMR.end() && "null dereference!");
                return _locToVAddrs[addrIt->second];
            }
        }
    }

    inline IntervalValue& getLocToItv(u32_t vAddrId)
    {
        auto it = _locToItvVal.find(vAddrId);
        if(it != _locToItvVal.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToItvVal.find(vAddrId);
            if(globIt != globalES._locToItvVal.end())
                return globIt->second;
            else
                return _locToItvVal[vAddrId];
        }
    }

    inline VAddrsID& getLocVAddrs(u32_t vAddrId)
    {
        auto it = _locToVAddrs.find(vAddrId);
        if(it != _locToVAddrs.end())
            return it->second;
        else
        {
            auto globIt = globalES._locToVAddrs.find(vAddrId);
            if(globIt != globalES._locToVAddrs.end())
                return globIt->second;
            else
                return _locToVAddrs[vAddrId];
        }
    }

    /// Print values of all expressions
    void printExprValues(std::ostream &oss) const override;

    std::string toString() const override
    {
        return "";
    }

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

    static bool eqLocToValMap(const VarToValMap &lhs, const VAddrToVAddrsID &lhsMToMR, const VarToValMap &rhs,
                              const VAddrToVAddrsID &rhsMToMR)
    {
        if (lhsMToMR.size() != rhsMToMR.size()) return false;
        Set<std::pair<VAddrsID, VAddrsID>> visited;
        for (const auto &item: lhsMToMR)
        {
            auto it = rhsMToMR.find(item.first);
            if (it == rhsMToMR.end())
                return false;
            if(visited.count({it->second, item.second})) continue;
            visited.emplace(it->second, item.second);
            if (!lhs.at(item.second).equals(rhs.at(it->second)))
            {
                return false;
            }
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

    static bool geqLocToValMap(const VarToValMap &lhs, const VAddrToVAddrsID &lhsItvMToMR, const VarToValMap &rhs,
                               const VAddrToVAddrsID &rhsItvMToMR) {
        if (rhsItvMToMR.empty()) return true;
        Set<std::pair<VAddrsID, VAddrsID>> visited;
        for (const auto &item: rhsItvMToMR) {
            auto it = lhsItvMToMR.find(item.first);
            if (it == lhsItvMToMR.end()) return false;
            // judge from expr id
            if(visited.count({it->second, item.second})) continue;
            visited.emplace(it->second, item.second);
            if (!lhs.at(it->second).geq(rhs.at(item.second))) return false;
        }
        return true;
    }

    bool operator==(const IntervalExeState &rhs) const
    {
        return ExeState::operator==(rhs) && eqVarToValMap(_varToItvVal, rhs.getVarToVal()) &&
               eqLocToValMap(_locToItvVal, rhs.getLocToVal());
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
        return geqVarToValMap(_varToItvVal, rhs.getVarToVal()) &&
               geqLocToValMap(_locToItvVal, _itvMToMR, rhs.getLocToVal(), rhs.getItvMToMR());
    }

    void clear()
    {
        _locToItvVal.clear();
        _itvMToMR.clear();
        _varToItvVal.clear();
        _locToVAddrs.clear();
        _vAddrMToMR.clear();
        _varToVAddrs.clear();
    }


private:
    void printTable(const VarToValMap &table, std::ostream &oss) const;

    void printLocTable(const VarToValMap &table, std::ostream &oss) const;

    void printTable(const VarToVAddrs &table, std::ostream &oss) const;

    void printLocTable(const VarToVAddrs &table, std::ostream &oss) const;
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