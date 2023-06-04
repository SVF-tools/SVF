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
    return eqVarToVAddrs(_varToVAddrs, rhs._varToVAddrs) && eqVarToVAddrs(_locToVAddrs, rhs._locToVAddrs);
}

bool ExeState::joinWith(const ExeState &other)
{
    bool changed = false;
    for (auto it = other._varToVAddrs.begin(); it != other._varToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToVAddrs.find(key);
        if (oit != _varToVAddrs.end())
        {
            if(oit->second.join_with(it->second))
                changed = true;
        }
        else
        {
            changed = true;
            _varToVAddrs.emplace(key, it->second);
        }
    }
    for (auto it = other._locToVAddrs.begin(); it != other._locToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToVAddrs.find(key);
        if (oit != _locToVAddrs.end())
        {
            if(oit->second.join_with(it->second))
                changed = true;
        }
        else
        {
            changed = true;
            _locToVAddrs.emplace(key, it->second);
        }
    }
    return changed;
}

bool ExeState::meetWith(const ExeState &other)
{
    bool changed = false;
    for (auto it = other._varToVAddrs.begin(); it != other._varToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToVAddrs.find(key);
        if (oit != _varToVAddrs.end())
        {
            if(oit->second.meet_with(it->second))
                changed = true;
        }
    }
    for (auto it = other._locToVAddrs.begin(); it != other._locToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToVAddrs.find(key);
        if (oit != _locToVAddrs.end())
        {
            if(oit->second.meet_with(it->second))
                changed = true;
        }
    }
    return changed;
}

u32_t ExeState::hash() const
{
    size_t h = getVarToVAddrs().size() * 2;
    Hash<u32_t> hf;
    for (const auto &t: getVarToVAddrs())
    {
        h ^= hf(t.first) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    size_t h2 = getLocToVAddrs().size() * 2;
    for (const auto &t: getLocToVAddrs())
    {
        h2 ^= hf(t.first) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
    }
    Hash<std::pair<u32_t, u32_t>> pairH;
    return pairH(std::make_pair(h, h2));
}
