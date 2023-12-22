//===- ExeState.cpp ----General Execution States-------------------------//
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
 * ExeState.cpp
 *
 *  Created on: Aug 4, 2022
 *      Author: Xiao Cheng
 *
 */

#include "AbstractExecution/ExeState.h"
#include "AbstractExecution/IntervalExeState.h"

using namespace SVF;

bool ExeState::operator==(const ExeState &rhs) const
{
    return eqVarToAddrs(_varToAddrs, rhs._varToAddrs) && eqVarToAddrs(_locToAddrs, rhs._locToAddrs);
}

bool ExeState::joinWith(const ExeState &other)
{
    bool changed = false;
    for (auto it = other._varToAddrs.begin(); it != other._varToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAddrs.find(key);
        if (oit != _varToAddrs.end())
        {
            if(oit->second.join_with(it->second))
                changed = true;
        }
        else
        {
            changed = true;
            _varToAddrs.emplace(key, it->second);
        }
    }
    for (auto it = other._locToAddrs.begin(); it != other._locToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToAddrs.find(key);
        if (oit != _locToAddrs.end())
        {
            if(oit->second.join_with(it->second))
                changed = true;
        }
        else
        {
            changed = true;
            _locToAddrs.emplace(key, it->second);
        }
    }
    return changed;
}

bool ExeState::meetWith(const ExeState &other)
{
    bool changed = false;
    for (auto it = other._varToAddrs.begin(); it != other._varToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToAddrs.find(key);
        if (oit != _varToAddrs.end())
        {
            if(oit->second.meet_with(it->second))
                changed = true;
        }
    }
    for (auto it = other._locToAddrs.begin(); it != other._locToAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToAddrs.find(key);
        if (oit != _locToAddrs.end())
        {
            if(oit->second.meet_with(it->second))
                changed = true;
        }
    }
    return changed;
}

u32_t ExeState::hash() const
{
    size_t h = getVarToAddrs().size() * 2;
    Hash<u32_t> hf;
    for (const auto &t: getVarToAddrs())
    {
        h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    size_t h2 = getLocToAddrs().size() * 2;
    for (const auto &t: getLocToAddrs())
    {
        h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }
    Hash<std::pair<u32_t, u32_t>> pairH;
    return pairH(std::make_pair(h, h2));
}
