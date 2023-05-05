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

PersistentPointsToCache<PointsTo> ExeState::ptCache;

bool ExeState::operator==(const ExeState &rhs) const
{
    return eqVarToVAddrs(_varToVAddrs, rhs._varToVAddrs) &&
           eqLocToVAddrs(_locToVAddrs, _vAddrMToMR, rhs._locToVAddrs, rhs._vAddrMToMR);
}

void ExeState::storeVAddrs(u32_t vAddrId, const ExeState::VAddrsID &vaddrs) {
    if(isEmpty(vAddrId)) return;
    auto it = _locToVAddrs.find(vAddrId);
    if (it != _locToVAddrs.end()) {
        it->second = vaddrs;
    } else {
        // find intersection
        Map<VAddrsID, VAddrs> intersectMap;
        for (const auto &vAddr: getActualVAddrs(vAddrId)) {
            auto originalIt = _vAddrMToMR.find(vAddr);
            if (originalIt != _vAddrMToMR.end()) {
                intersectMap[originalIt->second].set(vAddr);
            }
        }
        // update intersection
        Map<VAddrsID, VAddrs> MR2M;
        for (const auto &vAddrsID: _locToVAddrs) {
            VAddrs oVAddrs = getActualVAddrs(vAddrsID.first);
            auto interIt = intersectMap.find(vAddrsID.first);
            if (interIt != intersectMap.end()) {
                oVAddrs -= interIt->second; // remove intersection
                MR2M[vAddrsID.first] = oVAddrs;
            }
        }
        // update \delta and m2MR
        for (const auto &mit: MR2M) {
            VAddrsID newId = emplaceVAddrs(mit.second);
            _locToVAddrs[newId] = _locToVAddrs[mit.first];
            _locToVAddrs.erase(mit.first);
            for (const auto &m: mit.second) {
                _vAddrMToMR[m] = newId;
            }
        }
        for (const auto &vAddr: getActualVAddrs(vAddrId)) {
            _vAddrMToMR[vAddr] = vAddrId;
        }
        _locToVAddrs[vAddrId] = vaddrs;
    }
}

void ExeState::joinWith(const ExeState &other)
{
    for (auto it = other._varToVAddrs.begin(); it != other._varToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToVAddrs.find(key);
        if (oit != _varToVAddrs.end())
        {
            oit->second = unionVAddrs(oit->second, it->second);
        }
        else
        {
            _varToVAddrs.emplace(key, it->second);
        }
    }
    VarToVAddrs locToVAddrs;
    VAddrToVAddrsID vAddrMToMR;
    Set<VAddrsID> IMRs;
    Map<std::pair<VAddrsID, VAddrsID>, VAddrs> Pair2IM;
    
    for (auto it = other._locToVAddrs.begin(); it != other._locToVAddrs.end(); ++it)
    {
        const VAddrs &M = getActualVAddrs(it->first);
        auto lhsIt = _locToVAddrs.find(it->first);
        if (lhsIt != _locToVAddrs.end()) {
            IMRs.insert(it->first);
            locToVAddrs[it->first] = unionVAddrs(it->second, lhsIt->second);
            for (const auto &m: M) {
                vAddrMToMR[m] = it->first;
            }
            continue;
        }
        for (const auto &m: M) {
            auto lhsVIt = _vAddrMToMR.find(m);
            if (lhsVIt != _vAddrMToMR.end()) {
                Pair2IM[{lhsVIt->second, it->first}].set(m);
            }
        }
    }
    // update intersection
    for (const auto &item: Pair2IM) {
        VAddrsID newId = emplaceVAddrs(item.second);
        locToVAddrs[newId] = unionVAddrs(_locToVAddrs.at(item.first.first), other._locToVAddrs.at(item.first.second));
        for (const auto &m: item.second) {
            vAddrMToMR[m] = newId;
        }
    }
    Map<VAddrsID, VAddrs> MR2M1, MR2M2;
    for (const auto &lhs: _locToVAddrs) {
        if (!IMRs.count(lhs.first)) {
            MR2M1[lhs.first] = getActualVAddrs(lhs.first);
        }
    }
    for (const auto &lhs: other._locToVAddrs) {
        if (!IMRs.count(lhs.first)) {
            MR2M2[lhs.first] = getActualVAddrs(lhs.first);
        }
    }
    // erase intersection
    for (const auto &item: Pair2IM) {
        MR2M1[item.first.first] -= item.second;
        MR2M2[item.first.second] -= item.second;
    }
    // update complement
    for (const auto &item: MR2M1) {
        if (!item.second.empty()) {
            VAddrsID newId = emplaceVAddrs(item.second);
            locToVAddrs[newId] = _locToVAddrs[item.first];
            for (const auto &m: item.second) {
                vAddrMToMR[m] = newId;
            }
        }
    }
    for (const auto &item: MR2M2) {
        if (!item.second.empty()) {
            VAddrsID newId = emplaceVAddrs(item.second);
            locToVAddrs[newId] = other._locToVAddrs.at(item.first);
            for (const auto &m: item.second) {
                vAddrMToMR[m] = newId;
            }
        }
    }
    _locToVAddrs = std::move(locToVAddrs);
    _vAddrMToMR = std::move(vAddrMToMR);
}


void ExeState::meetWith(const ExeState &other)
{
    for (auto it = other._varToVAddrs.begin(); it != other._varToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _varToVAddrs.find(key);
        if (oit != _varToVAddrs.end())
        {
            oit->second = intersectVAddrs(oit->second, it->second);
        }
    }
    for (auto it = other._locToVAddrs.begin(); it != other._locToVAddrs.end(); ++it)
    {
        auto key = it->first;
        auto oit = _locToVAddrs.find(key);
        if (oit != _locToVAddrs.end())
        {
            oit->second = intersectVAddrs(oit->second, it->second);
        }
    }
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