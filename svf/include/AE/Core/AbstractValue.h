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
    AddressValue addr;

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
            addr = AddressValue();
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
            addr = other.addr;
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
            addr = other.addr;
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
            addr = other.addr;
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
        addr = other;
        return *this;
    }

    AbstractValue operator==(const AbstractValue& other) const
    {
        assert(isInterval() && other.isInterval());
        return interval == other.interval;
    }

    AbstractValue operator==(const IntervalValue& other) const
    {
        assert(isInterval());
        return interval == other;
    }

    AbstractValue operator!=(const AbstractValue& other) const
    {
        assert(isInterval());
        return interval != other.interval;
    }

    AbstractValue operator!=(const IntervalValue& other) const
    {
        assert(isInterval());
        return interval != other;
    }


    AbstractValue(const IntervalValue& ival) : type(IntervalType), interval(ival) {}

    AbstractValue(const AddressValue& addr) : type(AddressType), addr(addr) {}
    // TODO: move constructor
    IntervalValue& getInterval()
    {
        if (isUnknown())
        {
            interval = IntervalValue::top();
        }
        assert(isInterval());
        return interval;
    }

    //
    const IntervalValue getInterval() const
    {
        assert(isInterval());
        return interval;
    }

    AddressValue& getAddrs()
    {
        assert(isAddr());
        return addr;
    }

    const AddressValue getAddrs() const
    {
        assert(isAddr());
        return addr;
    }
    ~AbstractValue() {};


    // interval visit funcs
    bool isTop() const
    {
        assert(isInterval());
        return interval.isTop();
    }

    bool isBottom() const
    {
        assert(isInterval());
        return interval.isBottom();
    }

    const NumericLiteral& lb() const
    {
        assert(isInterval());
        return interval.lb();
    }

    const NumericLiteral& ub() const
    {
        assert(isInterval());
        return interval.ub();
    }

    void setLb(const NumericLiteral& lb)
    {
        assert(isInterval());
        interval.setLb(lb);
    }

    void setUb(const NumericLiteral& ub)
    {
        assert(isInterval());
        interval.setUb(ub);
    }

    void setValue(const NumericLiteral &lb, const NumericLiteral &ub)
    {
        assert(isInterval());
        interval.setValue(lb, ub);
    }

    bool is_zero() const
    {
        assert(isInterval());
        return interval.is_zero();
    }

    bool is_infinite() const
    {
        assert(isInterval());
        return interval.is_infinite();
    }

    bool is_int() const
    {
        assert(isInterval());
        return interval.is_int();
    }

    bool is_real() const
    {
        assert(isInterval());
        return interval.is_real();
    }

    s64_t getIntNumeral() const
    {
        assert(isInterval());
        return interval.getIntNumeral();
    }

    double getRealNumeral() const
    {
        assert(isInterval());
        return interval.getRealNumeral();
    }

    bool is_numeral() const
    {
        assert(isInterval());
        return interval.is_numeral();
    }

    void set_to_bottom()
    {
        assert(isInterval());
        interval.set_to_bottom();
    }

    void set_to_top()
    {
        assert(isInterval());
        interval.set_to_top();
    }

    bool leq(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval.leq(other.interval);
    }

    bool geq(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval.geq(other.interval);
    }

    bool contains(s64_t n) const
    {
        assert(isInterval());
        return interval.contains(n);
    }
    // operator +-*/%>< >= <= << >> & | ^
    AbstractValue operator+(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval + other.interval;
    }
    AbstractValue operator+(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval + other;
    }

    AbstractValue operator-(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval - other.interval;
    }
    AbstractValue operator-(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval - other;
    }

    AbstractValue operator*(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval * other.interval;
    }
    AbstractValue operator*(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval * other;
    }

    AbstractValue operator/(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval / other.interval;
    }
    AbstractValue operator/(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval / other;
    }

    AbstractValue operator%(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval % other.interval;
    }
    AbstractValue operator%(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval % other;
    }

    AbstractValue operator>>(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval >> other.interval;
    }
    AbstractValue operator>>(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval >> other;
    }

    AbstractValue operator<<(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval << other.interval;
    }
    AbstractValue operator<<(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval << other;
    }

    AbstractValue operator&(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval & other.interval;
    }
    AbstractValue operator&(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval & other;
    }

    AbstractValue operator|(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval | other.interval;
    }
    AbstractValue operator|(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval | other;
    }

    AbstractValue operator^(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval ^ other.interval;
    }
    AbstractValue operator^(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval ^ other;
    }

    AbstractValue operator>(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval > other.interval;
    }
    AbstractValue operator>(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval > other;
    }

    AbstractValue operator<(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval < other.interval;
    }
    AbstractValue operator<(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval < other;
    }

    AbstractValue operator>=(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval >= other.interval;
    }
    AbstractValue operator>=(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval >= other;
    }

    AbstractValue operator<=(const AbstractValue &other) const
    {
        assert(isInterval() && other.isInterval());
        return interval <= other.interval;
    }
    AbstractValue operator<=(const IntervalValue &other) const
    {
        assert(isInterval());
        return interval <= other;
    }


    // address visit funcs
    std::pair<AddressValue::AddrSet::iterator, bool> insertAddr(u32_t id)   // insertAddr
    {
        assert(isAddr());
        return addr.insert(id);
    }

    // TODO: equals, join_with, meet_with, widen_with, narrow_with, toString,
    // These should be merged with AddressValue

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
            return addr.equals(rhs.addr);
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
            addr.join_with(other.addr);
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
            addr.meet_with(other.addr);
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
            return addr.toString();
        }
        return "";
    }

};
}