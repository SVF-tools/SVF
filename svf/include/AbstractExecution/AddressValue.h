//===- AddressValue.h ----Address Value Sets-------------------------//
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
 * AddressValue.h
 *
 *  Created on: Jul 9, 2022
 *      Author: Xiao Cheng
 *
 */

#ifndef Z3_EXAMPLE_ADDRESSVALUE_H
#define Z3_EXAMPLE_ADDRESSVALUE_H

#define AddressMask 0x7f000000
#define FlippedAddressMask (AddressMask^0xffffffff)

#include "AbstractExecution/AbstractValue.h"
#include "SVFIR/SVFIR.h"

namespace SVF
{
class AddressValue : public AbstractValue
{
public:
    typedef Set<u32_t> AddrSet;
private:
    AddrSet _addrs;
public:
    /// Default constructor
    AddressValue() : AbstractValue(AddressK) {}

    /// Constructor
    AddressValue(const Set<u32_t> &addrs) : AbstractValue(AddressK), _addrs(addrs) {}

    AddressValue(u32_t addr) : AbstractValue(AddressK), _addrs({addr}) {}

    /// Default destructor
    ~AddressValue() = default;

    /// Copy constructor
    AddressValue(const AddressValue &other) : AbstractValue(AddressK), _addrs(other._addrs) {}

    /// Move constructor
    AddressValue(AddressValue &&other) noexcept: AbstractValue(AddressK), _addrs(std::move(other._addrs)) {}

    /// Copy operator=
    AddressValue &operator=(const AddressValue &other)
    {
        if (!this->equals(other))
        {
            _addrs = other._addrs;
            AbstractValue::operator=(other);
        }
        return *this;
    }

    /// Move operator=
    AddressValue &operator=(AddressValue &&other) noexcept
    {
        if (this != &other)
        {
            _addrs = std::move(other._addrs);
            AbstractValue::operator=(std::move(other));
        }
        return *this;
    }

    bool equals(const AddressValue &rhs) const
    {
        return _addrs == rhs._addrs;
    }

    bool operator==(const AddressValue &rhs) const
    {
        return _addrs == rhs._addrs;
    }

    /// Operator !=
    bool operator!=(const AddressValue &rhs) const
    {
        return _addrs != rhs._addrs;
    }

    AddrSet::const_iterator begin() const
    {
        return _addrs.cbegin();
    }

    AddrSet::const_iterator end() const
    {
        return _addrs.cend();
    }

    bool empty() const
    {
        return _addrs.empty();
    }

    u32_t size() const
    {
        return _addrs.size();
    }

    std::pair<AddressValue::AddrSet::iterator, bool> insert(u32_t id)
    {
        return _addrs.insert(id);
    }

    const AddrSet &getVals() const
    {
        return _addrs;
    }

    void setVals(const AddrSet &vals)
    {
        _addrs = vals;
    }

    /// Current AddressValue joins with another AddressValue
    bool join_with(const AddressValue &other)
    {
        bool changed = false;
        for (const auto &addr: other)
        {
            if (!_addrs.count(addr))
            {
                if (insert(addr).second)
                    changed = true;
            }
        }
        return changed;
    }

    /// Return a intersected AddressValue
    bool meet_with(const AddressValue &other)
    {
        AddrSet s;
        for (const auto &id: other._addrs)
        {
            if (_addrs.find(id) != _addrs.end())
            {
                s.insert(id);
            }
        }
        bool changed = (_addrs != s);
        _addrs = std::move(s);
        return changed;
    }

    /// Return true if the AddressValue contains n
    bool contains(u32_t id) const
    {
        return _addrs.count(id);
    }

    bool hasIntersect(const AddressValue &other)
    {
        AddressValue v = *this;
        v.meet_with(other);
        return !v.empty();
    }

    inline bool isTop() const override
    {
        return *this == getVirtualMemAddress(PAG::getPAG()->getBlackHoleObj()->getId());
    }

    inline bool isBottom() const override
    {
        return empty();
    }

    inline void setTop()
    {
        *this = getVirtualMemAddress(PAG::getPAG()->getBlackHoleObj()->getId());
    }

    inline void setBottom()
    {
        _addrs.clear();
    }

    /// The physical address starts with 0x7f...... + idx
    static inline u32_t getVirtualMemAddress(u32_t idx)
    {
        return AddressMask + idx;
    }

    /// Check bit value of val start with 0x7F000000, filter by 0xFF000000
    static inline bool isVirtualMemAddress(u32_t val)
    {
        return (val & 0xff000000) == AddressMask;
    }

    /// Return the internal index if idx is an address otherwise return the value of idx
    static inline u32_t getInternalID(u32_t idx)
    {
        return (idx & FlippedAddressMask);
    }
};
} // end namespace SVF
#endif //Z3_EXAMPLE_ADDRESSVALUE_H
