//===- IntervalExeState.cpp----Interval Domain-------------------------//
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
 * IntervalExeState.cpp
 *
 *  Created on: Jul 9, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#include "AbstractExecution/IntervalExeState.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

IntervalExeState IntervalExeState::globalES;
bool IntervalExeState::equals(const IntervalExeState &other) const
{
    return *this == other;
}

u32_t IntervalExeState::hash() const
{
    size_t h = getVarToVal().size() * 2;
    Hash<u32_t> hf;
    for (const auto &t: getVarToVal())
    {
        h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    size_t h2 = getLocToVal().size() * 2;
    for (const auto &t: getLocToVal())
    {
        h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }
    Hash<std::pair<std::pair<u32_t, u32_t>, u32_t>> pairH;
    return pairH(std::make_pair(std::make_pair(h, h2), (u32_t) ExeState::hash()));
}

IntervalExeState IntervalExeState::widening(const IntervalExeState& other)
{
    IntervalExeState es = *this;
    for (auto it = es._varToItvVal.begin(); it != es._varToItvVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToItvVal.find(key) != other._varToItvVal.end())
            it->second.widen_with(other._varToItvVal.at(key));
    }
    for (auto it = es._locToItvVal.begin(); it != es._locToItvVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToItvVal.find(key) != other._locToItvVal.end())
            it->second.widen_with(other._locToItvVal.at(key));
    }
    return es;
}

IntervalExeState IntervalExeState::narrowing(const IntervalExeState& other)
{
    IntervalExeState es = *this;
    for (auto it = es._varToItvVal.begin(); it != es._varToItvVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToItvVal.find(key) != other._varToItvVal.end())
            it->second.narrow_with(other._varToItvVal.at(key));
    }
    for (auto it = es._locToItvVal.begin(); it != es._locToItvVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToItvVal.find(key) != other._locToItvVal.end())
            it->second.narrow_with(other._locToItvVal.at(key));
    }
    return es;

}

/// domain widen with other, important! other widen this.
void IntervalExeState::widenWith(const IntervalExeState& other)
{
    for (auto it = _varToItvVal.begin(); it != _varToItvVal.end(); ++it)
    {
        auto key = it->first;
        if (other.getVarToVal().find(key) != other.getVarToVal().end())
            it->second.widen_with(other._varToItvVal.at(key));
    }
    for (auto it = _locToItvVal.begin(); it != _locToItvVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToItvVal.find(key) != other._locToItvVal.end())
            it->second.widen_with(other._locToItvVal.at(key));
    }
}

/// domain join with other, important! other widen this.
void IntervalExeState::joinWith(const IntervalExeState& other)
{
    ExeState::joinWith(other);
    for (auto it = other._varToItvVal.begin(); it != other._varToItvVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToItvVal.find(key);
        if (oit != _varToItvVal.end())
        {
            oit->second.join_with(it->second);
        }
        else
        {
            _varToItvVal.emplace(key, it->second);
        }
    }
    for (auto it = other._locToItvVal.begin(); it != other._locToItvVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToItvVal.find(key);
        if (oit != _locToItvVal.end())
        {
            oit->second.join_with(it->second);
        }
        else
        {
            _locToItvVal.emplace(key, it->second);
        }
    }
}

/// domain narrow with other, important! other widen this.
void IntervalExeState::narrowWith(const IntervalExeState& other)
{
    for (auto it = _varToItvVal.begin(); it != _varToItvVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = other.getVarToVal().find(key);
        if (oit != other.getVarToVal().end())
            it->second.narrow_with(oit->second);
    }
    for (auto it = _locToItvVal.begin(); it != _locToItvVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = other._locToItvVal.find(key);
        if (oit != other._locToItvVal.end())
            it->second.narrow_with(oit->second);
    }
}

/// domain meet with other, important! other widen this.
void IntervalExeState::meetWith(const IntervalExeState& other)
{
    ExeState::meetWith(other);
    for (auto it = other._varToItvVal.begin(); it != other._varToItvVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToItvVal.find(key);
        if (oit != _varToItvVal.end())
        {
            oit->second.meet_with(it->second);
        }
    }
    for (auto it = other._locToItvVal.begin(); it != other._locToItvVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToItvVal.find(key);
        if (oit != _locToItvVal.end())
        {
            oit->second.meet_with(it->second);
        }
    }
}

/// Print values of all expressions
void IntervalExeState::printExprValues(std::ostream &oss) const
{
    oss << "-----------Var and Value-----------\n";
    printTable(_varToItvVal, oss);
    printTable(_locToItvVal, oss);
    printTable(_varToVAddrs, oss);
    printTable(_locToVAddrs, oss);
    oss << "-----------------------------------------\n";
}

void IntervalExeState::printTable(const VarToValMap &table, std::ostream &oss) const
{
    oss.flags(std::ios::left);
    std::set<NodeID> ordered;
    for (const auto &item: table)
    {
        ordered.insert(item.first);
    }
    for (const auto &item: ordered)
    {
        oss << "Var" << std::to_string(item);
        IntervalValue sim = table.at(item);
        if (sim.is_numeral() && isVirtualMemAddress(Interval2NumValue(sim)))
        {
            oss << "\t Value: " << std::hex << "0x" << Interval2NumValue(sim) << "\n";
        }
        else
        {
            oss << "\t Value: " << std::dec << sim << "\n";
        }
    }
}

void IntervalExeState::printTable(const VarToVAddrs &table, std::ostream &oss) const
{
    oss.flags(std::ios::left);
    std::set<NodeID> ordered;
    for (const auto &item: table)
    {
        ordered.insert(item.first);
    }
    for (const auto &item: ordered)
    {
        oss << "Var" << std::to_string(item);
        VAddrs sim = table.at(item);
        oss << "\t Value: " << std::hex << "[ ";
        for (auto it = sim.begin(); it != sim.end(); ++it)
        {
            oss << std::hex << "0x" << *it << "(" << std::to_string(*it&(0x00ffffff)) << ") ,";
        }
        oss << "]\n";
    }
}
