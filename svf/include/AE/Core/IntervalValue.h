//===- IntervalValue.h ----Interval Value for Abstract Domain-------------//
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
 * IntervalValue.h
 *
 *  Created on: Aug 4, 2022
 *      Author: Jiawei Wang, Xiao Cheng
 *
 */
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)

#ifndef Z3_EXAMPLE_IntervalValue_H
#define Z3_EXAMPLE_IntervalValue_H

#include <sstream>
#include "AE/Core/NumericValue.h"

namespace SVF
{

/// IntervalValue abstract value
///
/// Implemented as a pair of bounds
class IntervalValue
{
private:
    // Lower bound
    BoundedInt _lb;

    // Upper bound
    BoundedInt _ub;

    // Invariant: isBottom() <=> _lb = +inf && _ub = -inf
public:
    friend IntervalValue operator+(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator-(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator*(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator/(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator<<(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator>>(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator&(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator|(const IntervalValue &lhs, const IntervalValue &rhs);
    friend IntervalValue operator^(const IntervalValue &lhs, const IntervalValue &rhs);

    bool isTop() const
    {
        return _lb.is_minus_infinity() && _ub.is_plus_infinity();
    }

    bool isBottom() const
    {
        return _lb.is_plus_infinity() && _ub.is_minus_infinity();
    }

    /// Get minus infinity -inf
    static BoundedInt minus_infinity()
    {
        return BoundedInt::minus_infinity();
    }

    /// Get plus infinity +inf
    static BoundedInt plus_infinity()
    {
        return BoundedInt::plus_infinity();
    }

    static bool is_infinite(const BoundedInt &e)
    {
        return e.is_infinity();
    }

    /// Create the IntervalValue [-inf, +inf]
    static IntervalValue top()
    {
        return IntervalValue(minus_infinity(), plus_infinity());
    }

    /// Create the bottom IntervalValue [+inf, -inf]
    static IntervalValue bottom()
    {
        return IntervalValue(plus_infinity(), minus_infinity());
    }

    /// Create default IntervalValue
    explicit IntervalValue() : _lb(minus_infinity()), _ub(plus_infinity()) {}

    /// Create the IntervalValue [n, n]
    explicit IntervalValue(s64_t n) : _lb(n), _ub(n) {}

    explicit IntervalValue(s32_t n) : IntervalValue((s64_t) n) {}

    explicit IntervalValue(u32_t n) : IntervalValue((s64_t) n) {}

    explicit IntervalValue(double n) : _lb(n), _ub(n) {}

    explicit IntervalValue(BoundedInt n) : IntervalValue(n, n) {}

    /// Create the IntervalValue [lb, ub]
    explicit IntervalValue(BoundedInt lb, BoundedInt ub) : _lb(std::move(lb)), _ub(std::move(ub))
    {
        assert((isBottom() || _lb.leq(_ub)) && "lower bound should be less than or equal to upper bound");
    }

    explicit IntervalValue(s64_t lb, s64_t ub) : IntervalValue(BoundedInt(lb), BoundedInt(ub)) {}

    explicit IntervalValue(double lb, double ub) : IntervalValue(BoundedInt(lb), BoundedInt(ub)) {}

    explicit IntervalValue(float lb, float ub) : IntervalValue(BoundedInt(lb), BoundedInt(ub)) {}

    explicit IntervalValue(s32_t lb, s32_t ub) : IntervalValue((s64_t) lb, (s64_t) ub) {}

    explicit IntervalValue(u32_t lb, u32_t ub) : IntervalValue((s64_t) lb, (s64_t) ub) {}

    explicit IntervalValue(u64_t lb, u64_t ub) : IntervalValue((s64_t) lb, (s64_t) ub) {}

    /// Copy constructor
    IntervalValue(const IntervalValue &) = default;

    /// Move constructor
    IntervalValue(IntervalValue &&) = default;

    /// Copy assignment operator
    IntervalValue &operator=(const IntervalValue &a) = default;

    /// Move assignment operator
    IntervalValue &operator=(IntervalValue &&) = default;

    /// Equality comparison
    IntervalValue operator==(const IntervalValue &other) const
    {
        if (this->isBottom() || other.isBottom())
        {
            return bottom();
        }
        else if (this->isTop() || other.isTop())
        {
            return top();
        }
        else
        {
            if (this->is_numeral() && other.is_numeral())
            {
                return eq(this->_lb, other._lb) ? IntervalValue(1, 1) : IntervalValue(0, 0);
            }
            else
            {
                IntervalValue value = *this;
                value.meet_with(other);
                if (value.isBottom())
                {
                    return IntervalValue(0, 0);
                }
                else
                {
                    // return top();
                    return IntervalValue(0, 1);
                }
            }
        }
    }

    /// Equality comparison
    IntervalValue operator!=(const IntervalValue &other) const
    {
        if (this->isBottom() || other.isBottom())
        {
            return bottom();
        }
        else if (this->isTop() || other.isTop())
        {
            return top();
        }
        else
        {
            if (this->is_numeral() && other.is_numeral())
            {
                return eq(this->_lb, other._lb) ? IntervalValue(0, 0) : IntervalValue(1, 1);
            }
            else
            {
                IntervalValue value = *this;
                value.meet_with(other);
                if (!value.isBottom())
                {
                    return IntervalValue(0, 1);
                }
                else
                {
                    return IntervalValue(1, 1);
                }
            }
        }
    }

    /// Destructor
    ~IntervalValue()  = default;

    /// Return the lower bound
    const BoundedInt &lb() const
    {
        assert(!this->isBottom() && "bottom interval does not have lower bound");
        return this->_lb;
    }

    /// Return the upper bound
    const BoundedInt &ub() const
    {
        assert(!this->isBottom() && "bottom interval does not have upper bound");
        return this->_ub;
    }

    /// Return true if the IntervalValue is [0, 0]
    bool is_zero() const
    {
        return _lb.is_zero() && _ub.is_zero();
    }

    /// Return true if the IntervalValue is infinite IntervalValue
    bool is_infinite() const
    {
        return _lb.is_infinity() || _ub.is_infinity();
    }

    bool is_int() const
    {
        return !is_real();
    }

    bool is_real() const
    {
        bool lb_real = _lb.is_real();
        bool ub_real = _ub.is_real();
        return lb_real || ub_real;
    }

    /// Return
    s64_t getNumeral() const
    {
        assert(is_numeral() && "this IntervalValue is not numeral");
        return _lb.getNumeral();
    }

    s64_t getIntNumeral() const
    {
        assert(is_numeral() && "this IntervalValue is not numeral");
        return _lb.getIntNumeral();
    }

    double getRealNumeral() const
    {
        assert(is_numeral() && "this IntervalValue is not numeral");
        return _lb.getRealNumeral();
    }

    /// Return true if the IntervalValue is a number [num, num]
    bool is_numeral() const
    {
        return eq(_lb, _ub);
    }

    /// Set current IntervalValue as bottom
    void set_to_bottom()
    {
        this->_lb = plus_infinity();
        this->_ub = minus_infinity();
    }

    /// Set current IntervalValue as top
    void set_to_top()
    {
        this->_lb = minus_infinity();
        this->_ub = plus_infinity();
    }

    /// Determines if the current IntervalValue is fully contained within another IntervalValue.
    /// Example: this: [2, 3], other: [1, 4] -> returns true
    /// Note: If the current interval is 'bottom', it is considered contained within any interval.
    ///       If the other interval is 'bottom', it cannot contain any interval.
    bool containedWithin(const IntervalValue &other) const
    {
        if (this->isBottom())
        {
            return true;
        }
        else if (other.isBottom())
        {
            return false;
        }
        else
        {
            return other._lb.leq(this->_lb) && this->_ub.leq(other._ub);
        }

    }

    /// Determines if the current IntervalValue fully contains another IntervalValue.
    /// Example: this: [1, 4], other: [2, 3] -> returns true
    /// Note: If the current interval is 'bottom', it is considered to contain any interval.
    ///       If the other interval is 'bottom', it cannot be contained by any interval.
    bool contain(const IntervalValue &other) const
    {
        if (this->isBottom())
        {
            return true;
        }
        else if (other.isBottom())
        {
            return false;
        }
        else
        {
            return other._lb.geq(this->_lb) && this->_ub.geq(other._ub);
        }
    }

    /// Check the upper bound of this Interval is less than or equal to the lower bound
    /// e.g. [1, 3] < [3, 5] return true, lhs.ub <= rhs.lb
    bool leq(const IntervalValue &other) const
    {
        if (this->isBottom())
        {
            return true;
        }
        else if (other.isBottom())
        {
            return false;
        }
        else
        {
            return this->_ub.leq(other._lb);
        }
    }

    /// Check the lower bound of this Interval is greater than or equal to the upper bound
    /// e.g. [3, 5] > [1, 3] return true, lhs.lb >= rhs.ub
    bool geq(const IntervalValue &other) const
    {
        if (this->isBottom())
        {
            return true;
        }
        else if (other.isBottom())
        {
            return false;
        }
        else
        {
            return this->_lb.geq(other._ub);
        }
    }


    /// Equality comparison
    bool equals(const IntervalValue &other) const
    {
        if (this->isBottom())
        {
            return other.isBottom();
        }
        else if (other.isBottom())
        {
            return false;
        }
        else
        {
            if (this->is_real() && other.is_real())
            {
                return this->_lb.equal(other._lb) && this->_ub.equal(other._ub);
            }
            else if (this->is_int() && other.is_int())
            {
                return this->_lb.equal(other._lb) && this->_ub.equal(other._ub);
            }
            else if (this->is_int())
            {
                double thislb = this->_lb.getIntNumeral();
                double thisub = this->_ub.getIntNumeral();
                double otherlb = other._lb.getRealNumeral();
                double otherub = other._ub.getRealNumeral();
                return thislb == otherlb && thisub == otherub;
            }
            else
            {
                double thislb = this->_lb.getRealNumeral();
                double thisub = this->_ub.getRealNumeral();
                double otherlb = other._lb.getIntNumeral();
                double otherub = other._ub.getIntNumeral();
                return thislb == otherlb && thisub == otherub;
            }
            assert(false && "not implemented");
        }
    }

    /// Current IntervalValue joins with another IntervalValue
    void join_with(const IntervalValue &other)
    {
        if (this->isBottom())
        {
            if (other.isBottom())
            {
                return;
            }
            else
            {
                setValue(other.lb(), other.ub());
            }
        }
        else if (other.isBottom())
        {
            return;
        }
        else
        {
            setValue(min(this->lb(), other.lb()), max(this->ub(), other.ub()));
        }
    }

    /// Current IntervalValue widen with another IntervalValue
    void widen_with(const IntervalValue &other)
    {
        if (this->isBottom())
        {
            this->_lb = other._lb;
            this->_ub = other._ub;
        }
        else if (other.isBottom())
        {
            return;
        }
        else
        {
            setValue(!lb().leq(other.lb()) ? minus_infinity() : this->lb(), !ub().geq(other.ub()) ? plus_infinity() : this->ub());
        }
    }

    /// Current IntervalValue narrow with another IntervalValue
    void narrow_with(const IntervalValue &other)
    {
        if (this->isBottom() || other.isBottom())
        {
            this->set_to_bottom();
        }
        else if (other.isBottom())
        {
            return;
        }
        else
        {
            setValue(is_infinite(this->lb()) ? other._lb : this->_lb, is_infinite(this->ub()) ? other._ub : this->_ub);
        }
    }

    /// Return a intersected IntervalValue
    void meet_with(const IntervalValue &other)
    {
        if (this->isBottom() || other.isBottom())
        {
            this->set_to_bottom();
        }
        else
        {
            if (!(max(this->_lb, other.lb()).leq(min(this->_ub, other.ub()))))
            {
                this->set_to_bottom();
            }
            else
            {
                setValue(max(this->_lb, other.lb()), min(this->_ub, other.ub()));
            }
        }
    }

    /// Return true if the IntervalValue contains n
    bool contains(int n) const
    {
        return this->_lb.leq(n) && this->_ub.geq(n);
    }

    void dump(std::ostream &o) const
    {
        if (this->isBottom())
        {
            o << "⊥";
        }
        else
        {
            o << "[" << this->_lb << ", " << this->_ub << "]";
        }
    }

    const std::string toString() const
    {
        std::string str;
        std::stringstream rawStr(str);
        if (this->isBottom())
        {
            rawStr << "⊥";
        }
        else
        {
            rawStr << "[" << lb().to_string() << ", " << ub().to_string() << "]";
        }
        return rawStr.str();
    }
private:
    /// Set the lower bound
    void setValue(const BoundedInt &lb, const BoundedInt &ub)
    {
        assert((isBottom() || _lb.leq(_ub)) && "lower bound should be less than or equal to upper bound");
        this->_lb = lb;
        this->_ub = ub;
    }

private:
    // internal use for create bottom-tolerant IntervalValue
    static IntervalValue create(const BoundedInt& lb, const BoundedInt& ub)
    {
        if (!lb.leq(ub))
            return IntervalValue::bottom();
        else
            return IntervalValue(lb, ub);
    }
}; // end class IntervalValue

/// Add IntervalValues
inline IntervalValue operator+(const IntervalValue &lhs,
                               const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (lhs.isTop() || rhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        return IntervalValue(lhs.lb() + rhs.lb(), lhs.ub() + rhs.ub());
    }
}

/// Subtract IntervalValues
inline IntervalValue operator-(const IntervalValue &lhs,
                               const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (lhs.isTop() || rhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        return IntervalValue(lhs.lb() - rhs.ub(), lhs.ub() - rhs.lb());
    }
}

/// Multiply IntervalValues
inline IntervalValue operator*(const IntervalValue &lhs,
                               const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else
    {
        BoundedInt ll = lhs.lb() * rhs.lb();
        BoundedInt lu = lhs.lb() * rhs.ub();
        BoundedInt ul = lhs.ub() * rhs.lb();
        BoundedInt uu = lhs.ub() * rhs.ub();
        std::vector<BoundedInt> vec{ll, lu, ul, uu};
        return IntervalValue(BoundedInt::min(vec),
                             BoundedInt::max(vec));
    }
}

/// Divide IntervalValues
inline IntervalValue operator/(const IntervalValue &lhs,
                               const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (rhs.contains(0))
    {
        IntervalValue lb = IntervalValue::create(rhs.lb(), -1);
        IntervalValue ub = IntervalValue::create(1, rhs.ub());
        IntervalValue l_res = lhs / lb;
        IntervalValue r_res = lhs / ub;
        l_res.join_with(r_res);
        return l_res;
    }
    else if (lhs.contains(0))
    {
        IntervalValue lb = IntervalValue::create(lhs.lb(), -1);
        IntervalValue ub = IntervalValue::create(1, lhs.ub());
        IntervalValue l_res = lb / rhs;
        IntervalValue r_res = ub / rhs;
        l_res.join_with(r_res);
        l_res.join_with(IntervalValue(0));
        return l_res;
    }
    else
    {
        // Neither the dividend nor the divisor contains 0
        BoundedInt ll = lhs.lb() / rhs.lb();
        BoundedInt lu = lhs.lb() / rhs.ub();
        BoundedInt ul = lhs.ub() / rhs.lb();
        BoundedInt uu = lhs.ub() / rhs.ub();
        std::vector<BoundedInt> vec{ll, lu, ul, uu};

        IntervalValue res =  IntervalValue(BoundedInt::min(vec),
                                           BoundedInt::max(vec));
        return res;
    }
}

/// Divide IntervalValues
inline IntervalValue operator%(const IntervalValue &lhs,
                               const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (rhs.contains(0))
    {
        return lhs.is_zero() ? IntervalValue(0, 0) : IntervalValue::top();
    }
    else if (lhs.is_numeral() && rhs.is_numeral())
    {
        return IntervalValue(lhs.lb() % rhs.lb(), lhs.lb() % rhs.lb());
    }
    else
    {
        BoundedInt n_ub = max(abs(lhs.lb()), abs(lhs.ub()));
        BoundedInt d_ub = max(abs(rhs.lb()), rhs.ub()) - 1;
        BoundedInt ub = min(n_ub, d_ub);

        if (lhs.lb().getNumeral() < 0)
        {
            if (lhs.ub().getNumeral() > 0)
            {
                return IntervalValue(-ub, ub);
            }
            else
            {
                return IntervalValue(-ub, 0);
            }
        }
        else
        {
            return IntervalValue(0, ub);
        }
    }
}

// Compare two IntervalValues for greater than
inline IntervalValue operator>(const IntervalValue &lhs, const IntervalValue &rhs)
{
    // If either lhs or rhs is bottom, the result is bottom
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    // If either lhs or rhs is top, the result is top
    else if (lhs.isTop() || rhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        // If both lhs and rhs are numerals (lb = ub), directly compare their values
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            // It means lhs.lb() > rhs.lb()? true: false
            return lhs.lb().leq(rhs.lb()) ? IntervalValue(0, 0) : IntervalValue(1, 1);
        }
        else
        {
            // Return [1,1] means lhs is totally greater than rhs
            // When lhs.lb > rhs.ub, e.g., lhs:[3, 4] rhs:[1, 2]
            // lhs.lb(3) > rhs.ub(2)
            // It means lhs.lb() > rhs.ub()
            if (!lhs.lb().leq(rhs.ub()))
            {
                return IntervalValue(1, 1);
            }
            // Return [0,0] means lhs is totally impossible to be greater than rhs
            // i.e., lhs is totally less than or equal to rhs
            // When lhs.ub <= rhs.lb, e.g., lhs:[3, 4] rhs:[4，5]
            // lhs.ub(4) <= rhs.lb(4)
            else if (lhs.ub().leq(rhs.lb()))
            {
                return IntervalValue(0, 0);
            }
            // For other cases, lhs can be greater than or not, depending on the values
            // e.g., lhs: [2,4], rhs: [1,3],
            // lhs can be greater than rhs if lhs is 4 and rhs is 1.
            // lhs can also not be greater than rhs if lhs is 2 and rhs is 3
            else
            {
                return IntervalValue(0, 1);
            }
        }
    }
}

// Compare two IntervalValues for less than
inline IntervalValue operator<(const IntervalValue &lhs, const IntervalValue &rhs)
{
    // If either lhs or rhs is bottom, the result is bottom
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    // If either lhs or rhs is top, the result is top
    else if (lhs.isTop() || rhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        // If both lhs and rhs are numerals (lb = ub), directly compare their values
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            // It means lhs.lb() < rhs.lb()? true: false
            return lhs.lb().geq(rhs.lb()) ? IntervalValue(0, 0) : IntervalValue(1, 1);
        }
        else
        {
            // Return [1,1] means lhs is totally less than rhs
            // When lhs.ub < rhs.lb, e.g., lhs:[1, 2] rhs:[3, 4]
            // lhs.ub(2) < rhs.lb(3)
            // It means lhs.ub() < rhs.lb()
            if (!lhs.ub().geq(rhs.lb()))
            {
                return IntervalValue(1, 1);
            }
            // Return [0,0] means lhs is totally impossible to be less than rhs
            // i.e., lhs is totally greater than or equal to rhs
            // When lhs.lb >= rhs.ub, e.g., lhs:[4,5] rhs:[3，4]
            // lhs.lb(4) >= rhs.ub(4)
            else if (lhs.lb().geq(rhs.ub()))
            {
                return IntervalValue(0, 0);
            }
            // For other cases, lhs can be less than rhs or not, depending on the values
            // e.g., lhs: [2,4], rhs: [1,3],
            // lhs can be less than rhs if lhs is 2, rhs is 3.
            // lhs can also not be less than rhs if lhs is 4 and rhs is 1
            else
            {
                return IntervalValue(0, 1);
            }
        }
    }
}


// Compare two IntervalValues for greater than or equal to
inline IntervalValue operator>=(const IntervalValue &lhs, const IntervalValue &rhs)
{
    // If either lhs or rhs is bottom, the result is bottom
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    // If either lhs or rhs is top, the result is top
    else if (lhs.isTop() || rhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        // If both lhs and rhs are numerals (lb = ub), directly compare their values
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            return lhs.lb().geq(rhs.lb()) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        }
        else
        {
            // Return [1,1] means lhs is totally greater than or equal to rhs
            // When lhs.lb >= rhs.ub, e.g., lhs:[2, 3] rhs:[1, 2]
            // lhs.lb(2) >= rhs.ub(2)
            if (lhs.lb().geq(rhs.ub()))
            {
                return IntervalValue(1, 1);
            }
            // Return [0,0] means lhs is totally impossible to be greater than or equal to rhs
            // i.e., lhs is totally less than rhs
            // When lhs.ub < rhs.lb, e.g., lhs:[1, 2] rhs:[3, 4]
            // lhs.ub(2) < rhs.lb(3)
            // It means lhs.ub() < rhs.lb()
            else if (!lhs.ub().geq(rhs.lb()))
            {
                return IntervalValue(0, 0);
            }
            // For other cases, lhs can be greater than or equal to rhs or not, depending on the values
            // e.g., lhs: [2,4], rhs: [1,3],
            // lhs can be greater than or equal to rhs if lhs is 3, rhs is 2.
            // lhs can also not be greater than or equal to rhs if lhs is 2 and rhs is 3
            else
            {
                return IntervalValue(0, 1);
            }
        }
    }
}

// Compare two IntervalValues for less than or equal to
inline IntervalValue operator<=(const IntervalValue &lhs, const IntervalValue &rhs)
{
    // If either lhs or rhs is bottom, the result is bottom
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    // If either lhs or rhs is top, the result is top
    else if (lhs.isTop() || rhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        // If both lhs and rhs are numerals (lb = ub), directly compare their values
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            return lhs.lb().leq(rhs.lb()) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        }
        else
        {
            // Return [1,1] means lhs is totally less than or equal to rhs
            // When lhs.ub <= rhs.lb, e.g., lhs:[1, 2] rhs:[2, 3]
            // lhs.ub(2) <= rhs.lb(2)
            if (lhs.ub().leq(rhs.lb()))
            {
                return IntervalValue(1, 1);
            }
            // Return [0,0] means lhs is totally impossible to be less than or equal to rhs
            // i.e., lhs is totally greater than rhs
            // When lhs.lb > rhs.ub, e.g., lhs:[3, 4] rhs:[1, 2]
            // lhs.lb(3) > rhs.ub(2)
            // It means lhs.lb() > rhs.ub()
            else if (!lhs.lb().leq(rhs.ub()))
            {
                return IntervalValue(0, 0);
            }
            // For other cases, lhs can be less than or equal to rhs or not, depending on the values
            // e.g., lhs: [2,4], rhs: [1,3],
            // lhs can be less than or equal to rhs if lhs is 3, rhs is 3.
            // lhs can also not be less than or equal to rhs if lhs is 3 and rhs is 2
            else
            {
                return IntervalValue(0, 1);
            }
        }
    }
}

/// Left binary shift of IntervalValues
inline IntervalValue operator<<(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
        return IntervalValue::bottom();
    if (lhs.isTop() && rhs.isTop())
        return IntervalValue::top();
    else
    {
        IntervalValue shift = rhs;
        shift.meet_with(IntervalValue(0, IntervalValue::plus_infinity()));
        if (shift.isBottom())
            return IntervalValue::bottom();
        BoundedInt lb = 0;
        // If the shift is greater than 32, the result is always 0
        if ((s32_t) shift.lb().getNumeral() >= 32 || shift.lb().is_infinity())
        {
            lb = IntervalValue::minus_infinity();
        }
        else
        {
            lb = (1 << (s32_t) shift.lb().getNumeral());
        }
        BoundedInt ub = 0;
        if (shift.ub().is_infinity())
        {
            ub = IntervalValue::plus_infinity();
        }
        else
        {
            ub = (1 << (s32_t) shift.ub().getNumeral());
        }
        IntervalValue coeff(lb, ub);
        return lhs * coeff;
    }
}

/// Left binary shift of IntervalValues
inline IntervalValue operator>>(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
        return IntervalValue::bottom();
    else if (lhs.isTop() && rhs.isTop())
        return IntervalValue::top();
    else
    {
        IntervalValue shift = rhs;
        shift.meet_with(IntervalValue(0, IntervalValue::plus_infinity()));
        if (shift.isBottom())
            return IntervalValue::bottom();
        if (lhs.contains(0))
        {
            IntervalValue l = IntervalValue::create(lhs.lb(), -1);
            IntervalValue u = IntervalValue::create(1, lhs.ub());
            IntervalValue tmp = l >> rhs;
            tmp.join_with(u >> rhs);
            tmp.join_with(IntervalValue(0));
            return tmp;
        }
        else
        {
            BoundedInt ll = lhs.lb() >> shift.lb();
            BoundedInt lu = lhs.lb() >> shift.ub();
            BoundedInt ul = lhs.ub() >> shift.lb();
            BoundedInt uu = lhs.ub() >> shift.ub();
            std::vector<BoundedInt> vec{ll, lu, ul, uu};
            return IntervalValue(BoundedInt::min(vec),
                                 BoundedInt::max(vec));
        }
    }
}

/// Bitwise AND of IntervalValues
inline IntervalValue operator&(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
        return IntervalValue::bottom();
    else if (lhs.is_numeral() && rhs.is_numeral())
    {
        return IntervalValue(lhs.lb() & rhs.lb());
    }
    else if (lhs.lb().getNumeral() >= 0 && rhs.lb().getNumeral() >= 0)
    {
        return IntervalValue((s64_t) 0, min(lhs.ub(), rhs.ub()));
    }
    else if (lhs.lb().getNumeral() >= 0)
    {
        return IntervalValue((s64_t) 0, lhs.ub());
    }
    else if (rhs.lb().getNumeral() >= 0)
    {
        return IntervalValue((s64_t) 0, rhs.ub());
    }
    else
    {
        return IntervalValue::top();
    }
}

/// Bitwise OR of IntervalValues
inline IntervalValue operator|(const IntervalValue &lhs, const IntervalValue &rhs)
{
    auto next_power_of_2 = [](s64_t num)
    {
        int i = 1;
        while ((num >> i) != 0)
        {
            ++i;
        }
        return 1 << i;
    };
    if (lhs.isBottom() || rhs.isBottom())
        return IntervalValue::bottom();
    else if (lhs.is_numeral() && rhs.is_numeral())
        return IntervalValue(lhs.lb() | rhs.lb());
    else if (lhs.lb().getNumeral() >= 0 && !lhs.ub().is_infinity() &&
             rhs.lb().getNumeral() >= 0 && !rhs.ub().is_infinity())
    {
        s64_t m = std::max(lhs.ub().getNumeral(), rhs.ub().getNumeral());
        s64_t ub = next_power_of_2(s64_t(m)) - 1;
        return IntervalValue((s64_t) 0, (s64_t) ub);
    }
    else
    {
        return IntervalValue::top();
    }
}

/// Bitwise XOR of IntervalValues
inline IntervalValue operator^(const IntervalValue &lhs, const IntervalValue &rhs)
{
    auto next_power_of_2 = [](s64_t num)
    {
        int i = 1;
        while ((num >> i) != 0)
        {
            ++i;
        }
        return 1 << i;
    };
    if (lhs.isBottom() || rhs.isBottom())
        return IntervalValue::bottom();
    else if (lhs.is_numeral() && rhs.is_numeral())
        return IntervalValue(lhs.lb() ^ rhs.lb());
    else if (lhs.lb().getNumeral() >= 0 && !lhs.ub().is_infinity() &&
             rhs.lb().getNumeral() >= 0 && !rhs.ub().is_infinity())
    {
        s64_t m = std::max(lhs.ub().getNumeral(), rhs.ub().getNumeral());
        s64_t ub = next_power_of_2(s64_t(m)) - 1;
        return IntervalValue((s64_t) 0, (s64_t) ub);
    }
    else
    {
        return IntervalValue::top();
    }
}

/// Write an IntervalValue on a stream
inline std::ostream &operator<<(std::ostream &o,
                                const IntervalValue &IntervalValue)
{
    IntervalValue.dump(o);
    return o;
}

} // end namespace SVF
#endif //Z3_EXAMPLE_IntervalValue_H
