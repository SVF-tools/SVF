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

#include "AE/Core/AbstractExeState.h"
#include "Util/SVFUtil.h"

using namespace SVF;
using namespace SVFUtil;

bool AbstractESBase::equals(const AbstractESBase&other) const
{
    return *this == other;
}

u32_t AbstractESBase::hash() const
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

AbstractESBase AbstractESBase::widening(const AbstractESBase& other)
{
    // widen interval
    AbstractESBase es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end()) {
            if (it->second.isInterval())
                it->second.getInterval().widen_with(other._varToAbsVal.at(key).getInterval());
        }
    }
    for (auto it = es._locToAbsVal.begin(); it != es._locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToAbsVal.find(key) != other._locToAbsVal.end())
            if (it->second.isInterval())
                it->second.getInterval().widen_with(other._locToAbsVal.at(key).getInterval());
    }
    return es;
}

AbstractESBase AbstractESBase::narrowing(const AbstractESBase& other)
{
    AbstractESBase es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            it->second.getInterval().narrow_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._locToAbsVal.begin(); it != es._locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToAbsVal.find(key) != other._locToAbsVal.end())
            it->second.getInterval().narrow_with(other._locToAbsVal.at(key).getInterval());
    }
    return es;

}

/// domain widen with other, important! other widen this.
void AbstractESBase::widenWith(const AbstractESBase& other)
{
    for (auto it = _varToAbsVal.begin(); it != _varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other.getVarToVal().find(key) != other.getVarToVal().end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = _locToAbsVal.begin(); it != _locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToAbsVal.find(key) != other._locToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._locToAbsVal.at(key).getInterval());
    }
}

/// domain join with other, important! other widen this.
void AbstractESBase::joinWith(const AbstractESBase& other)
{
    for (auto it = other._varToAbsVal.begin(); it != other._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAbsVal.find(key);
        if (oit != _varToAbsVal.end())
        {
            if (oit->second.isInterval() && it->second.isInterval()) {
                oit->second.getInterval().join_with(it->second.getInterval());
            }
            else if (oit->second.isAddr() && it->second.isAddr())
            {
                oit->second.getAddrs().join_with(it->second.getAddrs());
            }
            else {
                // do nothing
            }
        }
        else
        {
            _varToAbsVal.emplace(key, it->second);
        }
    }
    for (auto it = other._locToAbsVal.begin(); it != other._locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToAbsVal.find(key);
        if (oit != _locToAbsVal.end())
        {
            if (oit->second.isInterval() && it->second.isInterval()) {
                oit->second.getInterval().join_with(it->second.getInterval());
            }
            else if (oit->second.isAddr() && it->second.isAddr())
            {
                oit->second.getAddrs().join_with(it->second.getAddrs());
            }
            else {
                // do nothing
            }
        }
        else
        {
            _locToAbsVal.emplace(key, it->second);
        }
    }
}

/// domain narrow with other, important! other widen this.
void AbstractESBase::narrowWith(const AbstractESBase& other)
{
    for (auto it = _varToAbsVal.begin(); it != _varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = other.getVarToVal().find(key);
        if (oit != other.getVarToVal().end())
            if (it->second.isInterval() && oit->second.isInterval())
                it->second.getInterval().narrow_with(oit->second.getInterval());
    }
    for (auto it = _locToAbsVal.begin(); it != _locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = other._locToAbsVal.find(key);
        if (oit != other._locToAbsVal.end())
            if (it->second.isInterval() && oit->second.isInterval())
                it->second.getInterval().narrow_with(oit->second.getInterval());
    }
}

/// domain meet with other, important! other widen this.
void AbstractESBase::meetWith(const AbstractESBase& other)
{
    for (auto it = other._varToAbsVal.begin(); it != other._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAbsVal.find(key);
        if (oit != _varToAbsVal.end())
        {
            if (oit->second.isInterval() && it->second.isInterval()) {
                oit->second.getInterval().meet_with(it->second.getInterval());
            }
            else if (oit->second.isAddr() && it->second.isAddr())
            {
                oit->second.getAddrs().meet_with(it->second.getAddrs());
            }
            else {
                // do nothing
            }
        }
    }
    for (auto it = other._locToAbsVal.begin(); it != other._locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToAbsVal.find(key);
        if (oit != _locToAbsVal.end())
        {
            if (oit->second.isInterval() && it->second.isInterval()) {
                oit->second.getInterval().meet_with(it->second.getInterval());
            }
            else if (oit->second.isAddr() && it->second.isAddr())
            {
                oit->second.getAddrs().meet_with(it->second.getAddrs());
            }
            else {
                // do nothing
            }
        }
    }
}

/// Print values of all expressions
void AbstractESBase::printExprValues(std::ostream &oss) const
{
    oss << "-----------Var and Value-----------\n";
    printTable(_varToAbsVal, oss);
    printTable(_locToAbsVal, oss);
    oss << "-----------------------------------------\n";
}

void AbstractESBase::printTable(const VarToAbsValMap&table, std::ostream &oss) const
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
        IntervalValue sim = table.at(item).getInterval();
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

AbstractExeState AbstractExeState::globalES;
bool AbstractExeState::equals(const AbstractExeState&other) const
{
    return *this == other;
}

u32_t AbstractExeState::hash() const
{
    return AbstractESBase::hash();
}

AbstractExeState AbstractExeState::widening(const AbstractExeState& other)
{
    AbstractExeState es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._locToAbsVal.begin(); it != es._locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToAbsVal.find(key) != other._locToAbsVal.end())
            if (it->second.isInterval() && other._locToAbsVal.at(key).isInterval())
                it->second.getInterval().widen_with(other._locToAbsVal.at(key).getInterval());
    }
    return es;
}

AbstractExeState AbstractExeState::narrowing(const AbstractExeState& other)
{
    AbstractExeState es = *this;
    for (auto it = es._varToAbsVal.begin(); it != es._varToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._varToAbsVal.find(key) != other._varToAbsVal.end())
            if (it->second.isInterval() && other._varToAbsVal.at(key).isInterval())
                it->second.getInterval().narrow_with(other._varToAbsVal.at(key).getInterval());
    }
    for (auto it = es._locToAbsVal.begin(); it != es._locToAbsVal.end(); ++it)
    {
        auto key = it->first;
        if (other._locToAbsVal.find(key) != other._locToAbsVal.end())
            if (it->second.isInterval() && other._locToAbsVal.at(key).isInterval())
                it->second.getInterval().narrow_with(other._locToAbsVal.at(key).getInterval());
    }
    return es;

}

/// domain widen with other, important! other widen this.
void AbstractExeState::widenWith(const AbstractExeState& other)
{
    AbstractESBase::widenWith(other);
}

/// domain join with other, important! other widen this.
void AbstractExeState::joinWith(const AbstractExeState& other)
{
    AbstractESBase::joinWith(other);
}

/// domain narrow with other, important! other widen this.
void AbstractExeState::narrowWith(const AbstractExeState& other)
{
    AbstractESBase::narrowWith(other);
}

/// domain meet with other, important! other widen this.
void AbstractExeState::meetWith(const AbstractExeState& other)
{
    AbstractESBase::meetWith(other);
}


/// Print values of all expressions
void AbstractExeState::printExprValues(std::ostream &oss) const
{
    oss << "-----------Var and Value-----------\n";
    printTable(_varToAbsVal, oss);
    printTable(_locToAbsVal, oss);
    oss << "------------Global---------------------\n";
    printTable(globalES._varToAbsVal, oss);
    printTable(globalES._locToAbsVal, oss);

}
