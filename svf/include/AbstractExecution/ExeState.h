//===- ExeState.h ----General Execution States-------------------------//
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
 * ExeState.h
 *
 *  Created on: Aug 4, 2022
 *      Author: Xiao Cheng
 *
 */


#ifndef Z3_EXAMPLE_EXESTATE_H
#define Z3_EXAMPLE_EXESTATE_H

#include "AbstractExecution/AddressValue.h"
#include "AbstractExecution/NumericLiteral.h"
#include "Util/Z3Expr.h"
#include "MemoryModel/PersistentPointsToCache.h"
#include "MemoryModel/PointsTo.h"

namespace SVF
{

class IntervalExeState;

/*!
 * Base execution state
 */
class ExeState
{
    friend class SVFIR2ItvExeState;

public:

    typedef u32_t VAddr;
    typedef PointsTo VAddrs;
    typedef PointsToID VAddrsID;
    typedef Map<NodeID, VAddrsID> VarToVAddrs;
    typedef Map<VAddr, VAddrsID> VAddrToVAddrsID;
    /// Execution state kind
    enum ExeState_TYPE
    {
        IntervalK, ConcreteK
    };
private:
    ExeState_TYPE _kind;

public:
    ExeState(ExeState_TYPE kind) : _kind(kind) {}

    virtual ~ExeState() = default;

    ExeState(const ExeState &rhs) : _varToVAddrs(rhs._varToVAddrs),
                                    _locToVAddrs(rhs._locToVAddrs),
                                    _vAddrMToMR(rhs._vAddrMToMR) {}

    ExeState(ExeState &&rhs) noexcept: _varToVAddrs(std::move(rhs._varToVAddrs)),
                                       _locToVAddrs(std::move(rhs._locToVAddrs)),
                                       _vAddrMToMR(std::move(rhs._vAddrMToMR)) {}

    ExeState &operator=(const ExeState &rhs)
    {
        if(*this != rhs)
        {
            _varToVAddrs = rhs._varToVAddrs;
            _locToVAddrs = rhs._locToVAddrs;
            _vAddrMToMR = rhs._vAddrMToMR;
        }
        return *this;
    }

    ExeState &operator=(ExeState &&rhs) noexcept
    {
        if (this != &rhs)
        {
            _varToVAddrs = std::move(rhs._varToVAddrs);
            _locToVAddrs = std::move(rhs._locToVAddrs);
            _vAddrMToMR = std::move(rhs._vAddrMToMR);
        }
        return *this;
    }

    bool operator==(const ExeState &rhs) const;

    inline bool operator!=(const ExeState &rhs) const
    {
        return !(*this == rhs);
    }

    bool equals(const ExeState *other) const
    {
        return false;
    }

    /// Make all value join with the other
    void joinWith(const ExeState &other);

    /// Make all value meet with the other
    void meetWith(const ExeState &other);

    virtual u32_t hash() const;

    /// Print values of all expressions
    virtual void printExprValues(std::ostream &oss) const {}

    virtual std::string toString() const
    {
        return "";
    }

    inline ExeState_TYPE getExeStateKind() const
    {
        return _kind;
    }

    inline virtual const VarToVAddrs &getVarToVAddrs() const
    {
        return _varToVAddrs;
    }

    inline virtual const VarToVAddrs &getLocToVAddrs() const
    {
        return _locToVAddrs;
    }

    inline virtual const VAddrToVAddrsID &getVAddrMToMR() const
    {
        return _vAddrMToMR;
    }

    inline virtual bool inVarToAddrsTable(u32_t id) const
    {
        return _varToVAddrs.find(id) != _varToVAddrs.end();
    }

    inline virtual bool inVAddrMToMRTable(u32_t id) const
    {
        return _vAddrMToMR.find(id) != _vAddrMToMR.end();
    }

    inline virtual bool inLocToAddrsTable(u32_t vAddrId) const
    {
        return _locToVAddrs.find(vAddrId) != _locToVAddrs.end();
    }

    virtual VAddrsID &getVAddrs(u32_t id)
    {
        return _varToVAddrs[id];
    }

    virtual void storeVAddrs(u32_t vAddrId, const VAddrsID &vaddrs);

    inline virtual VAddrsID &loadVAddrs(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        auto it = _vAddrMToMR.find(addr);
        assert(it != _vAddrMToMR.end() && "null dereference!");
        return _locToVAddrs[it->second];
    }

    inline bool isNullPtr(u32_t addr)
    {
        return getInternalID(addr) == 0;
    }

protected:
    VarToVAddrs _varToVAddrs;
    VarToVAddrs _locToVAddrs;
    VAddrToVAddrsID _vAddrMToMR;

protected:

    static bool eqVarToVAddrs(const VarToVAddrs &lhs, const VarToVAddrs &rhs)
    {
        if (lhs.size() != rhs.size()) return false;
        for (const auto &item: lhs)
        {
            auto it = rhs.find(item.first);
            if (it == rhs.end())
                return false;
            if (item.second != it->second)
            {
                return false;
            }
        }
        return true;
    }

    static bool eqLocToVAddrs(const VarToVAddrs &lhs, const VAddrToVAddrsID &lhsMToMR, const VarToVAddrs &rhs,
                              const VAddrToVAddrsID &rhsMToMR) {
        if (lhsMToMR.size() != rhsMToMR.size()) return false;
        Set<std::pair<VAddrsID, VAddrsID>> visited;
        for (const auto &item: lhsMToMR) {
            auto it = rhsMToMR.find(item.first);
            if (it == rhsMToMR.end())
                return false;
            if(visited.count({it->second, item.second})) continue;
            visited.emplace(it->second, item.second);
            if (lhs.at(item.second) != rhs.at(it->second)) {
                return false;
            }
        }
        return true;
    }

public:
    static z3::context &getContext()
    {
        return Z3Expr::getContext();
    }

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

public:

    static bool emptyVAddrs() {
        return ptCache.emptyPointsToId();
    }

    static bool isEmpty(const VAddrsID& lhs) {
        return lhs == ptCache.emptyPointsToId();
    }

    static VAddrsID unionVAddrs(const VAddrsID& lhs, const VAddrsID& rhs) {
        return ptCache.unionPts(lhs, rhs);
    }

    static VAddrsID intersectVAddrs(const VAddrsID& lhs, const VAddrsID& rhs) {
        return ptCache.intersectPts(lhs, rhs);
    }

    static const VAddrs &getActualVAddrs(VAddrsID id)
    {
        return ptCache.getActualPts(id);
    }

    static VAddrsID emplaceVAddrs(const VAddrs& addrs)
    {
        return ptCache.emplacePts(addrs);
    }

    static VAddrsID emplaceVAddrs(NodeID addr)
    {
        VAddrs vAddrs;
        vAddrs.set(addr);
        return ptCache.emplacePts(vAddrs);
    }
protected:
    static PersistentPointsToCache<VAddrs> ptCache;


}; // end class ExeState



} // end namespace SVF
#endif //Z3_EXAMPLE_EXESTATE_H