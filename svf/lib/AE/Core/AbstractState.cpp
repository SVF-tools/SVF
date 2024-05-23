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
 * AbstractExeState.cpp
 *
 *  Created on: Jul 9, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#include "AE/Core/AbstractState.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

bool AbstractState::equals(const AbstractState&other) const
{
    return *this == other;
}

u32_t AbstractState::hash() const
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
    Hash<std::pair<u32_t, u32_t>> pairH;
    return pairH({h, h2});
}

AbstractState AbstractState::widening(const AbstractState& other)
{
    // widen interval
    AbstractState es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._addrToAbsVal.begin(); it != es._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._addrToAbsVal.find(key) != other._addrToAbsVal.end())
            if (it->second.isInterval() && other._addrToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._addrToAbsVal.at(key).getInterval());
    }
    return es;
}

AbstractState AbstractState::narrowing(const AbstractState& other)
{
    AbstractState es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().narrow_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._addrToAbsVal.begin(); it != es._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._addrToAbsVal.find(key) != other._addrToAbsVal.end())
            if (it->second.isInterval() && other._addrToAbsVal.at(key).isInterval())
                it->second.getInterval().narrow_with(other._addrToAbsVal.at(key).getInterval());
    }
    return es;

}

/// domain widen with other, important! other widen this.
void AbstractState::widenWith(const AbstractState& other)
{
    for (auto it = _varToAbsVal.begin(); it != _varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other.getVarToVal().find(key) != other.getVarToVal().end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = _addrToAbsVal.begin(); it != _addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._addrToAbsVal.find(key) != other._addrToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._addrToAbsVal.at(key).getInterval());
    }
}

/// domain join with other, important! other widen this.
void AbstractState::joinWith(const AbstractState& other)
{
    for (auto it = other._varToAbsVal.begin(); it != other._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAbsVal.find(key);
        if (oit != _varToAbsVal.end())
        {
            oit->second.join_with(it->second);
        }
        else
        {
            _varToAbsVal.emplace(key, it->second);
        }
    }
    for (auto it = other._addrToAbsVal.begin(); it != other._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _addrToAbsVal.find(key);
        if (oit != _addrToAbsVal.end())
        {
            oit->second.join_with(it->second);
        }
        else
        {
            _addrToAbsVal.emplace(key, it->second);
        }
    }
}

/// domain narrow with other, important! other widen this.
void AbstractState::narrowWith(const AbstractState& other)
{
    for (auto it = _varToAbsVal.begin(); it != _varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = other.getVarToVal().find(key);
        if (oit != other.getVarToVal().end())
            if (it->second.isInterval() && oit->second.isInterval())
                it->second.getInterval().narrow_with(oit->second.getInterval());
    }
    for (auto it = _addrToAbsVal.begin(); it != _addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = other._addrToAbsVal.find(key);
        if (oit != other._addrToAbsVal.end())
            if (it->second.isInterval() && oit->second.isInterval())
                it->second.getInterval().narrow_with(oit->second.getInterval());
    }
}

/// domain meet with other, important! other widen this.
void AbstractState::meetWith(const AbstractState& other)
{
    for (auto it = other._varToAbsVal.begin(); it != other._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAbsVal.find(key);
        if (oit != _varToAbsVal.end())
        {
            oit->second.meet_with(it->second);
        }
    }
    for (auto it = other._addrToAbsVal.begin(); it != other._addrToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _addrToAbsVal.find(key);
        if (oit != _addrToAbsVal.end())
        {
            oit->second.meet_with(it->second);
        }
    }
}

/// Print values of all expressions
void AbstractState::printExprValues(std::ostream &oss) const
{
    oss << "-----------Var and Value-----------\n";
    printTable(_varToAbsVal, oss);
    printTable(_addrToAbsVal, oss);
    oss << "-----------------------------------------\n";
}

void AbstractState::printTable(const VarToAbsValMap&table, std::ostream &oss) const
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
        oss << "\t Value: " << table.at(item).toString() << "\n";
    }
}
