//===- IntervalValue.h ----Interval Value for Interval Domain-------------//
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


#ifndef Z3_EXAMPLE_IntervalValue_H
#define Z3_EXAMPLE_IntervalValue_H

#include "AbstractExecution/NumericLiteral.h"
#include "AbstractExecution/AbstractValue.h"

namespace SVF
{

/// IntervalValue abstract value
///
/// Implemented as a pair of bounds
class IntervalValue final : public AbstractValue
{
private:
    // Lower bound
    NumericLiteral _lb;

    // Upper bound
    NumericLiteral _ub;

    // Invariant: isBottom() <=> _lb = 1 && _ub = 0
public:

    bool isTop() const override
    {
        return this->_lb.is_minus_infinity() && this->_ub.is_plus_infinity();
    }

    bool isBottom() const override
    {
        return !_ub.geq(_lb);
    }

    /// Get minus infinity -oo
    static NumericLiteral minus_infinity()
    {
        return NumericLiteral::minus_infinity();
    }

    /// Get plus infinity +oo
    static NumericLiteral plus_infinity()
    {
        return NumericLiteral::plus_infinity();
    }

    static bool is_infinite(const NumericLiteral &e)
    {
        return e.is_infinity();
    }

    /// Create the IntervalValue [-oo, +oo]
    static IntervalValue top()
    {
        return IntervalValue(minus_infinity(), plus_infinity());
    }

    /// Create the bottom IntervalValue
    static IntervalValue bottom()
    {
        return IntervalValue(1, 0);
    }

    /// Create default IntervalValue
    explicit IntervalValue() : AbstractValue(AbstractValue::IntervalK), _lb(minus_infinity()), _ub(plus_infinity()) {}

    /// Create the IntervalValue [n, n]
    explicit IntervalValue(int64_t n) : AbstractValue(AbstractValue::IntervalK), _lb(n), _ub(n) {}

    explicit IntervalValue(s32_t n) : IntervalValue((int64_t) n) {}

    explicit IntervalValue(u32_t n) : IntervalValue((int64_t) n) {}

    explicit IntervalValue(double n) : IntervalValue((int64_t) n) {}

    explicit IntervalValue(NumericLiteral n) : IntervalValue(n, n) {}

    /// Create the IntervalValue [lb, ub]
    explicit IntervalValue(NumericLiteral lb, NumericLiteral ub) : AbstractValue(AbstractValue::IntervalK),
        _lb(std::move(lb)), _ub(std::move(ub)) {}

    explicit IntervalValue(int64_t lb, int64_t ub) : IntervalValue(NumericLiteral(lb), NumericLiteral(ub)) {}

    explicit IntervalValue(double lb, double ub) : IntervalValue(NumericLiteral((int64_t) lb), NumericLiteral((int64_t) ub)) {}

    explicit IntervalValue(s32_t lb, s32_t ub) : IntervalValue((int64_t) lb, (int64_t) ub) {}

    explicit IntervalValue(u32_t lb, u32_t ub) : IntervalValue((int64_t) lb, (int64_t) ub) {}

    explicit IntervalValue(u64_t lb, u64_t ub) : IntervalValue((int64_t) lb, (int64_t) ub) {}

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
    ~IntervalValue() override = default;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    //@{
    static inline bool classof(const IntervalValue *)
    {
        return true;
    }

    static inline bool classof(const AbstractValue *v)
    {
        return v->getAbstractValueKind() == AbstractValue::IntervalK;
    }
    //@}

    /// Return the lower bound
    const NumericLiteral &lb() const
    {
        assert(!this->isBottom());
        return this->_lb;
    }

    /// Return the upper bound
    const NumericLiteral &ub() const
    {
        assert(!this->isBottom());
        return this->_ub;
    }

    /// Set the lower bound
    void setLb(const NumericLiteral &lb)
    {
        this->_lb = lb;
    }

    /// Set the upper bound
    void setUb(const NumericLiteral &ub)
    {
        this->_ub = ub;
    }

    /// Set the lower bound
    void setValue(const NumericLiteral &lb, const NumericLiteral &ub)
    {
        this->_lb = lb;
        this->_ub = ub;
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

    /// Return
    int64_t getNumeral() const
    {
        assert(is_numeral() && "this IntervalValue is not numeral");
        return _lb.getNumeral();
    }

    /// Return true if the IntervalValue is a number [num, num]
    bool is_numeral() const
    {
        return eq(_lb, _ub);
    }

    /// Set current IntervalValue as bottom
    void set_to_bottom()
    {
        this->_lb = 1;
        this->_ub = 0;
    }

    /// Set current IntervalValue as top
    void set_to_top()
    {
        this->_lb = minus_infinity();
        this->_ub = plus_infinity();
    }

    /// Check current IntervalValue is smaller than or equal to the other
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
            return other._lb.leq(this->_lb) && this->_ub.leq(other._ub);
        }

    }

    /// Check current IntervalValue is greater than or equal to the other
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
            return other._lb.geq(this->_lb) && this->_ub.geq(other._ub);
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
            return this->_lb.equal(other._lb) && this->_ub.equal(other._ub);
            // TODO: IntervalValueZ3Expr equals
            // TODO: shall we consider expr and solve.
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
                this->_lb = other.lb();
                this->_ub = other.ub();
            }
        }
        else if (other.isBottom())
        {
            return;
        }
        else
        {
            this->_lb = min(this->lb(), other.lb());
            this->_ub = max(this->ub(), other.ub());
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
            this->_lb = !lb().leq(other.lb()) ? minus_infinity() : this->lb();
            this->_ub = !ub().geq(other.ub()) ? plus_infinity() : this->ub();
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
            this->_lb = is_infinite(this->lb()) ? other._lb : this->_lb;
            this->_ub = is_infinite(this->ub()) ? other._ub : this->_ub;
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
            this->_lb = max(this->_lb, other.lb());
            this->_ub = min(this->_ub, other.ub());
            if (this->isBottom())
                this->set_to_bottom();
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

/// Substract IntervalValues
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
        NumericLiteral ll = lhs.lb() * rhs.lb();
        NumericLiteral lu = lhs.lb() * rhs.ub();
        NumericLiteral ul = lhs.ub() * rhs.lb();
        NumericLiteral uu = lhs.ub() * rhs.ub();
        std::vector<NumericLiteral> vec{ll, lu, ul, uu};
        return IntervalValue(NumericLiteral::min(vec),
                             NumericLiteral::max(vec));
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
        return lhs.is_zero() ? IntervalValue(0, 0) : IntervalValue::top();
    }
    else
    {
        // Neither the dividend nor the divisor contains 0
        NumericLiteral ll = lhs.lb() / rhs.lb();
        NumericLiteral lu = lhs.lb() / rhs.ub();
        NumericLiteral ul = lhs.ub() / rhs.lb();
        NumericLiteral uu = lhs.ub() / rhs.ub();
        std::vector<NumericLiteral> vec{ll, lu, ul, uu};

        return IntervalValue(NumericLiteral::min(vec),
                             NumericLiteral::max(vec));
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
        NumericLiteral n_ub = max(abs(lhs.lb()), abs(lhs.ub()));
        NumericLiteral d_ub = max(abs(rhs.lb()), rhs.ub()) - 1;
        NumericLiteral ub = min(n_ub, d_ub);

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

/// Greater than IntervalValues
inline IntervalValue operator>(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (lhs.isTop() || lhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            return lhs.lb().leq(rhs.lb()) ? IntervalValue(0, 0) : IntervalValue(1, 1);
        }
        else
        {
            // lhs[3,4] rhs[1,2]
            if (!lhs.lb().leq(rhs.ub()))
            {
                return IntervalValue(1, 1);
                // lhs[1,2] rhs[3,4]
            }
            else if (!lhs.ub().geq(rhs.lb()))
            {
                return IntervalValue(0, 0);
            }
            else
            {
                // lhs[1,3] rhs[2,4]
                return IntervalValue(0, 1);
            }
        }
    }
}

/// Greater than IntervalValues
inline IntervalValue operator<(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (lhs.isTop() || lhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            return lhs.lb().geq(rhs.lb()) ? IntervalValue(0, 0) : IntervalValue(1, 1);
        }
        else
        {
            // lhs [1,2] rhs [3,4]
            if (!lhs.ub().geq(rhs.lb()))
            {
                return IntervalValue(1, 1);
                // lhs [3,4] rhs [1,2]
            }
            else if (!lhs.lb().leq(rhs.ub()))
            {
                return IntervalValue(0, 0);
                // lhs [1,3] rhs [2,4]
            }
            else
            {
                return IntervalValue(0, 1);
            }
        }
    }
}


/// Greater than IntervalValues
inline IntervalValue operator>=(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (lhs.isTop() || lhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            return lhs.lb().geq(rhs.lb()) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        }
        else
        {
            // lhs [2,3] rhs [1,2]
            if (lhs.lb().geq(rhs.ub()))
            {
                return IntervalValue(1, 1);
                // lhs [1,2] rhs[3,4]
            }
            else if (!lhs.ub().geq(rhs.lb()))
            {
                return IntervalValue(0, 0);
                // lhs [1,3] rhs [2,4]
            }
            else
            {
                if (lhs.equals(rhs)) return IntervalValue(1, 1);
                else return IntervalValue(0, 1);
            }
        }
    }
}

/// Greater than IntervalValues
inline IntervalValue operator<=(const IntervalValue &lhs, const IntervalValue &rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
    {
        return IntervalValue::bottom();
    }
    else if (lhs.isTop() || lhs.isTop())
    {
        return IntervalValue::top();
    }
    else
    {
        if (lhs.is_numeral() && rhs.is_numeral())
        {
            return lhs.lb().leq(rhs.lb()) ? IntervalValue(1, 1) : IntervalValue(0, 0);
        }
        else
        {
            // lhs [1,2] rhs [2,3]
            if (lhs.ub().leq(rhs.lb()))
            {
                return IntervalValue(1, 1);
                // lhs [3,4] rhs[1,2]
            }
            else if (!lhs.lb().leq(rhs.ub()))
            {
                return IntervalValue(0, 0);
                // lhs [1,3] rhs [2,4]
            }
            else
            {
                if (lhs.equals(rhs)) return IntervalValue(1, 1);
                else return IntervalValue(0, 1);
            }
        }
    }
}

/// Left binary shift of IntervalValues
inline IntervalValue operator<<(const IntervalValue &lhs, const IntervalValue &rhs)
{
    //TODO: implement <<
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
        IntervalValue coeff(1 << (s32_t) shift.lb().getNumeral(),
                            shift.ub().is_infinity() ? IntervalValue::plus_infinity() : 1
                            << (s32_t) shift.ub().getNumeral());
        return lhs * coeff;
    }
}

/// Left binary shift of IntervalValues
inline IntervalValue operator>>(const IntervalValue &lhs, const IntervalValue &rhs)
{
    //TODO: implement >>
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
            IntervalValue l(lhs.lb(), -1);
            IntervalValue u(1, lhs.ub());
            IntervalValue tmp = l >> rhs;
            tmp.join_with(u >> rhs);
            tmp.join_with(IntervalValue(0));
            return tmp;
        }
        else
        {
            NumericLiteral ll = lhs.lb() >> shift.lb();
            NumericLiteral lu = lhs.lb() >> shift.ub();
            NumericLiteral ul = lhs.ub() >> shift.lb();
            NumericLiteral uu = lhs.ub() >> shift.ub();
            std::vector<NumericLiteral> vec{ll, lu, ul, uu};
            return IntervalValue(NumericLiteral::min(vec),
                                 NumericLiteral::max(vec));
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
        return IntervalValue((int64_t) 0, min(lhs.ub(), rhs.ub()));
    }
    else if (lhs.lb().getNumeral() >= 0)
    {
        return IntervalValue((int64_t) 0, lhs.ub());
    }
    else if (rhs.lb().getNumeral() >= 0)
    {
        return IntervalValue((int64_t) 0, rhs.ub());
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
        int64_t m = std::max(lhs.ub().getNumeral(), rhs.ub().getNumeral());
        int64_t ub = next_power_of_2(s64_t(m+1));
        return IntervalValue((int64_t) 0, (int64_t) ub);
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
        int64_t m = std::max(lhs.ub().getNumeral(), rhs.ub().getNumeral());
        int64_t ub = next_power_of_2(s64_t(m+1));
        return IntervalValue((int64_t) 0, (int64_t) ub);
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
