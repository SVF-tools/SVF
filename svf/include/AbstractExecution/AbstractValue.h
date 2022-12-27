//===- AbstractValue.h --Abstract Value for domains---------------------------//
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
 * AbstractValue.h
 *
 *  Created on: Aug 4, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#ifndef Z3_EXAMPLE_ABSTRACTVALUE_H
#define Z3_EXAMPLE_ABSTRACTVALUE_H

#include <type_traits>
#include <string>
#include "SVFIR/SVFValue.h"

namespace SVF
{

class IntervalValue;

/*!
 * Base class of abstract value
 */
class AbstractValue
{
public:
    /// Abstract value kind
    enum AbstractValueK
    {
        IntervalK, ConcreteK, AddressK
    };
private:
    AbstractValueK _kind;

public:
    AbstractValue(AbstractValueK kind) : _kind(kind) {}

    virtual ~AbstractValue() = default;

    AbstractValue(const AbstractValue &) noexcept = default;

    AbstractValue(AbstractValue &&) noexcept = default;

    AbstractValue &operator=(const AbstractValue &) noexcept = default;

    AbstractValue &operator=(AbstractValue &&) noexcept = default;

    inline AbstractValueK getAbstractValueKind() const
    {
        return _kind;
    }

    virtual bool isTop() const = 0;

    virtual bool isBottom() const = 0;
}; // end class AbstractValue
} // end namespace SVF
#endif //Z3_EXAMPLE_ABSTRACTVALUE_H
