//===- AbstractValue.h ----AbstractValue-------------------------//
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

// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)

#include "AE/Core/IntervalValue.h"
#include "AE/Core/AddressValue.h"
#include "Util/SVFUtil.h"

namespace SVF
{

class AbstractValue
{

public:
    IntervalValue interval;
    AddressValue addrs;

    AbstractValue()
    {
        interval = IntervalValue::bottom();
        addrs = AddressValue();
    }

    AbstractValue(const AbstractValue& other)
    {
        interval = other.interval;
        addrs = other.addrs;
    }

    inline bool isInterval() const
    {
        return !interval.isBottom();
    }
    inline bool isAddr() const
    {
        return !addrs.isBottom();
    }

    AbstractValue(AbstractValue &&other)
    {
        interval = SVFUtil::move(other.interval);
        addrs = SVFUtil::move(other.addrs);
    }

    // operator overload, supporting both interval and address
    AbstractValue& operator=(const AbstractValue& other)
    {
        interval = other.interval;
        addrs = other.addrs;
        return *this;
    }

    AbstractValue& operator=(const AbstractValue&& other)
    {
        interval = SVFUtil::move(other.interval);
        addrs = SVFUtil::move(other.addrs);
        return *this;
    }

    AbstractValue& operator=(const IntervalValue& other)
    {
        interval = other;
        addrs = AddressValue();
        return *this;
    }

    AbstractValue& operator=(const AddressValue& other)
    {
        addrs = other;
        interval = IntervalValue::bottom();
        return *this;
    }

    AbstractValue(const IntervalValue& ival) : interval(ival), addrs(AddressValue()) {}

    AbstractValue(const AddressValue& addr) : interval(IntervalValue::bottom()), addrs(addr) {}

    IntervalValue& getInterval()
    {
        return interval;
    }

    const IntervalValue getInterval() const
    {
        return interval;
    }

    AddressValue& getAddrs()
    {
        return addrs;
    }

    const AddressValue getAddrs() const
    {
        return addrs;
    }

    ~AbstractValue() {};

    bool equals(const AbstractValue &rhs) const
    {
        return interval.equals(rhs.interval) && addrs.equals(rhs.addrs);
    }

    void join_with(const AbstractValue &other)
    {
        interval.join_with(other.interval);
        addrs.join_with(other.addrs);
    }

    void meet_with(const AbstractValue &other)
    {
        interval.meet_with(other.interval);
        addrs.meet_with(other.addrs);
    }

    void widen_with(const AbstractValue &other)
    {
        interval.widen_with(other.interval);
        // TODO: widen Addrs
    }

    void narrow_with(const AbstractValue &other)
    {
        interval.narrow_with(other.interval);
        // TODO: narrow Addrs
    }

    std::string toString() const
    {
        return "<" + interval.toString() + ", " + addrs.toString() + ">";
    }
};
}