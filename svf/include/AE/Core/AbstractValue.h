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

#include "AE/Core/IntervalValue.h"
#include "AE/Core/AddressValue.h"

namespace SVF
{

class AbstractValue
{

public:

    enum DataType
    {
        IntervalType,
        AddressType,
        UnknownType,
    };
    DataType type;
    IntervalValue interval;
    AddressValue addrs;

    AbstractValue() : type(IntervalType)
    {
        interval = IntervalValue::top();
    }
    AbstractValue(DataType type) : type(type)
    {
        switch (type)
        {
        case IntervalType:
            interval = IntervalValue::top();
            break;
        case AddressType:
            addrs = AddressValue();
            break;
        case UnknownType:
            break;
        }
    }

    AbstractValue(const AbstractValue& other): type(other.type)
    {
        switch (type)
        {
        case IntervalType:
            interval = other.interval;
            break;
        case AddressType:
            addrs = other.addrs;
            break;
        case UnknownType:
            break;
        }
    }

    inline bool isInterval() const
    {
        return type == IntervalType;
    }
    inline bool isAddr() const
    {
        return type == AddressType;
    }
    inline bool isUnknown() const
    {
        return type == UnknownType;
    }

    inline DataType getType() const
    {
        return type;
    }

    AbstractValue(AbstractValue &&other) : type(other.type)
    {
        switch (type)
        {
        case IntervalType:
            interval = other.interval;
            break;
        case AddressType:
            addrs = other.addrs;
            break;
        case UnknownType:
            break;
        }
    }

    // operator overload, supporting both interval and address
    AbstractValue& operator=(const AbstractValue& other)
    {
        type = other.type;
        switch (type)
        {
        case IntervalType:
            interval = other.interval;
            break;
        case AddressType:
            addrs = other.addrs;
            break;
        case UnknownType:
            break;
        }
        return *this;
    }

    AbstractValue& operator=(const IntervalValue& other)
    {
        type = IntervalType;
        interval = other;
        return *this;
    }

    AbstractValue& operator=(const AddressValue& other)
    {
        type = AddressType;
        addrs = other;
        return *this;
    }

    AbstractValue(const IntervalValue& ival) : type(IntervalType), interval(ival) {}

    AbstractValue(const AddressValue& addr) : type(AddressType), addrs(addr) {}

    IntervalValue& getInterval()
    {
        if (isUnknown())
        {
            interval = IntervalValue::top();
        }
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
        if (type != rhs.type)
        {
            return false;
        }
        if (isInterval())
        {
            return interval.equals(rhs.interval);
        }
        if (isAddr())
        {
            return addrs.equals(rhs.addrs);
        }
        return false;
    }

    void join_with(const AbstractValue &other)
    {
        if (isUnknown())
        {
            *this = other;
            return;
        }
        else if (type != other.type)
        {
            return;
        }
        if (isInterval() && other.isInterval())
        {
            interval.join_with(other.interval);
        }
        if (isAddr() && other.isAddr())
        {
            addrs.join_with(other.addrs);
        }
        return;
    }

    void meet_with(const AbstractValue &other)
    {
        if (type != other.type)
        {
            return;
        }
        if (isInterval() && other.isInterval())
        {
            interval.meet_with(other.interval);
        }
        if (isAddr() && other.isAddr())
        {
            addrs.meet_with(other.addrs);
        }
        return;
    }

    void widen_with(const AbstractValue &other)
    {
        // widen_with only in interval
        if (isInterval() && other.isInterval())
        {
            interval.widen_with(other.interval);
        }
    }

    void narrow_with(const AbstractValue &other)
    {
        // narrow_with only in interval
        if (isInterval() && other.isInterval())
        {
            interval.narrow_with(other.interval);
        }
    }

    std::string toString() const
    {
        if (isInterval())
        {
            return interval.toString();
        }
        else if (isAddr())
        {
            return addrs.toString();
        }
        return "";
    }

};
}