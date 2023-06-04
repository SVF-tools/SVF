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

    typedef AddressValue VAddrs;
    typedef Map<u32_t, VAddrs> VarToVAddrs;
    /// Execution state kind
    enum ExeState_TYPE
    {
        IntervalK, SingleValueK
    };
private:
    ExeState_TYPE _kind;

public:
    ExeState(ExeState_TYPE kind) : _kind(kind) {}

    virtual ~ExeState() = default;

    ExeState(const ExeState &rhs) : _varToVAddrs(rhs._varToVAddrs),
        _locToVAddrs(rhs._locToVAddrs) {}

    ExeState(ExeState &&rhs) noexcept: _varToVAddrs(std::move(rhs._varToVAddrs)),
        _locToVAddrs(std::move(rhs._locToVAddrs)) {}

    ExeState &operator=(const ExeState &rhs)
    {
        if(*this != rhs)
        {
            _varToVAddrs = rhs._varToVAddrs;
            _locToVAddrs = rhs._locToVAddrs;
        }
        return *this;
    }

    ExeState &operator=(ExeState &&rhs) noexcept
    {
        if (this != &rhs)
        {
            _varToVAddrs = std::move(rhs._varToVAddrs);
            _locToVAddrs = std::move(rhs._locToVAddrs);
        }
        return *this;
    }

    virtual bool operator==(const ExeState &rhs) const;

    inline virtual bool operator!=(const ExeState &rhs) const
    {
        return !(*this == rhs);
    }

    bool equals(const ExeState *other) const
    {
        return false;
    }

    /// Make all value join with the other
    bool joinWith(const ExeState &other);

    /// Make all value meet with the other
    bool meetWith(const ExeState &other);

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

    inline virtual bool inVarToAddrsTable(u32_t id) const
    {
        return _varToVAddrs.find(id) != _varToVAddrs.end();
    }

    inline virtual bool inLocToAddrsTable(u32_t id) const
    {
        return _locToVAddrs.find(id) != _locToVAddrs.end();
    }

    virtual VAddrs &getVAddrs(u32_t id)
    {
        return _varToVAddrs[id];
    }

    inline virtual void storeVAddrs(u32_t addr, const VAddrs &vaddrs)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        if(isNullPtr(addr)) return;
        u32_t objId = getInternalID(addr);
        _locToVAddrs[objId] = vaddrs;
    }

    inline virtual VAddrs &loadVAddrs(u32_t addr)
    {
        assert(isVirtualMemAddress(addr) && "not virtual address?");
        u32_t objId = getInternalID(addr);
        return _locToVAddrs[objId];
    }

    inline bool isNullPtr(u32_t addr)
    {
        return getInternalID(addr) == 0;
    }

protected:
    VarToVAddrs _varToVAddrs{{0, getVirtualMemAddress(0)}};
    VarToVAddrs _locToVAddrs;

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

public:

    virtual std::string varToAddrs(u32_t varId) const
    {
        std::stringstream exprName;
        auto it = _varToVAddrs.find(varId);
        if (it == _varToVAddrs.end())
        {
            exprName << "Var not in varToAddrs!\n";
        }
        else
        {
            const VAddrs &vaddrs = it->second;
            if (vaddrs.size() == 1)
            {
                exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
            }
            else
            {
                exprName << "addr: {";
                for (const auto &addr: vaddrs)
                {
                    exprName << std::dec << getInternalID(addr) << ", ";
                }
                exprName << "}\n";
            }
        }
        return SVFUtil::move(exprName.str());
    }

    virtual std::string locToAddrs(u32_t objId) const
    {
        std::stringstream exprName;
        auto it = _locToVAddrs.find(objId);
        if (it == _locToVAddrs.end())
        {
            exprName << "Var not in varToAddrs!\n";
        }
        else
        {
            const VAddrs &vaddrs = it->second;
            if (vaddrs.size() == 1)
            {
                exprName << "addr: {" << std::dec << getInternalID(*vaddrs.begin()) << "}\n";
            }
            else
            {
                exprName << "addr: {";
                for (const auto &addr: vaddrs)
                {
                    exprName << std::dec << getInternalID(addr) << ", ";
                }
                exprName << "}\n";
            }
        }
        return SVFUtil::move(exprName.str());
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


}; // end class ExeState



} // end namespace SVF

template<>
struct std::hash<SVF::ExeState>
{
    size_t operator()(const SVF::ExeState &es) const
    {
        return es.hash();
    }
};
#endif //Z3_EXAMPLE_EXESTATE_H
