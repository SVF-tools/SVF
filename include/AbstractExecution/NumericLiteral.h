//===- NumericLiteral.h ----Number wrapper for domains------------------//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013-2022>  <Yulei Sui>
//

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//
/*
 * NumericLiteral.h
 *
 *  Created on: Aug 4, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#ifndef Z3_EXAMPLE_Number_H
#define Z3_EXAMPLE_Number_H

#include "SVFIR/SVFType.h"

namespace SVF {
class NumericLiteral {
private:
    double _n;

public:
    /// Default constructor
    NumericLiteral() = delete;

    /// Create a new NumericLiteral from double
    NumericLiteral(double n) : _n(n) {}

    /// Create a new NumericLiteral from s32_t
    NumericLiteral(s32_t n) : _n(n) {}

    /// Create a new NumericLiteral from s64_t
    NumericLiteral(s64_t n) : _n(n) {}

    /// Create a new NumericLiteral from u32_t
    NumericLiteral(u32_t n) : _n(n) {}

    /// Create a new NumericLiteral from u64_t
    NumericLiteral(u64_t n) : _n(n) {}

    virtual ~NumericLiteral() = default;

    /// Copy Constructor
    NumericLiteral(const NumericLiteral &) noexcept = default;

    /// Move Constructor
    NumericLiteral(NumericLiteral &&) noexcept = default;

    /// Operator = , another Copy Constructor
    inline NumericLiteral &operator=(const NumericLiteral &) noexcept = default;

    /// Operator = , another Move Constructor
    inline NumericLiteral &operator=(NumericLiteral &&) noexcept = default;

    /// Get minus infinity -oo
    static NumericLiteral minus_infinity() { return NumericLiteral(INT_MIN); }

    /// Get plus infinity +oo
    static NumericLiteral plus_infinity() { return NumericLiteral(INT_MAX); }

    /// Check if this is minus infinity
    inline bool is_minus_infinity() const { return _n == INT_MIN; }

    /// Check if this is plus infinity
    inline bool is_plus_infinity() const { return _n == INT_MAX; }

    /// Check if this is infinity (either of plus/minus)
    inline bool is_infinity() const { return is_minus_infinity() || is_plus_infinity(); }

    /// Check if this is zero
    inline bool is_zero() const { return _n == 0; }

    /// Return Numeral
    inline double getNumeral() const {
        return _n;
    }

    /// Check two object is equal
    bool equal(const NumericLiteral &rhs) const {
        return eq(*this, rhs);
    }

    /// Less then or equal
    bool leq(const NumericLiteral &rhs) const {
        if (is_infinity() ^ rhs.is_infinity()) {
            if (is_infinity()) {
                return is_minus_infinity();
            } else {
                return rhs.is_plus_infinity();
            }
        }
        return _n <= rhs._n;
    }

    // Greater than or equal
    bool geq(const NumericLiteral &rhs) const {
        if (is_infinity() ^ rhs.is_infinity()) {
            if (is_infinity()) {
                return is_plus_infinity();
            } else {
                return rhs.is_minus_infinity();
            }
        }
        return _n >= rhs._n;
    }


    /// Reload operator
    //{%
    friend NumericLiteral operator==(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return eq(lhs, rhs);
    }

    friend NumericLiteral operator!=(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return !eq(lhs, rhs);
    }

    friend NumericLiteral operator>(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return !lhs.leq(rhs);
    }

    friend NumericLiteral operator<(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return !lhs.geq(rhs);
    }

    friend NumericLiteral operator<=(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return lhs.leq(rhs);
    }

    friend NumericLiteral operator>=(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return lhs.geq(rhs);
    }

    friend NumericLiteral operator+(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        if (!lhs.is_infinity() && !rhs.is_infinity())
            return lhs.getNumeral() + rhs.getNumeral();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return rhs;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return lhs;
        else if (eq(lhs, rhs))
            return lhs;
        else
            assert(false && "undefined operation +oo + -oo");
    }

    friend NumericLiteral operator-(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        if (!lhs.is_infinity() && !rhs.is_infinity())
            return lhs.getNumeral() - rhs.getNumeral();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return -rhs;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return lhs;
        else if (!eq(lhs, rhs))
            return lhs;
        else
            assert(false && "undefined operation +oo - +oo");
    }

    friend NumericLiteral operator*(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        if (lhs.is_zero() || rhs.is_zero()) return 0;
        else if (lhs.is_infinity() && rhs.is_infinity())
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
        else if (lhs.is_infinity())
            return rhs.getNumeral() > 0 ? lhs : -lhs;
        else if (rhs.is_infinity())
            return lhs.getNumeral() > 0 ? rhs : -rhs;
        else
            return lhs.getNumeral() * rhs.getNumeral();
    }

    friend NumericLiteral operator/(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        if (rhs.is_zero()) assert(false && "divide by zero");
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return lhs.getNumeral() / rhs.getNumeral();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return rhs.getNumeral() > 0 ? lhs : -lhs;
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    friend NumericLiteral operator%(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        if (rhs.is_zero()) assert(false && "divide by zero");
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return (s32_t) lhs.getNumeral() % (s32_t) rhs.getNumeral();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0;
            // TODO: not sure
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return rhs.getNumeral() > 0 ? lhs : -lhs;
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    // TODO: logic operation for infinity?
    friend NumericLiteral operator^(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return (s32_t) lhs.getNumeral() ^ (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator&(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return (s32_t) lhs.getNumeral() & (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator|(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return (s32_t) lhs.getNumeral() | (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator>>(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs._n >= 0 ? 0 : -1;
        else
            return (s32_t) lhs.getNumeral() >> (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator<<(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs._n >= 0 ? plus_infinity() : minus_infinity();
        else
            return (s32_t) lhs.getNumeral() << (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator&&(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return lhs.getNumeral() && rhs.getNumeral();
    }

    friend NumericLiteral operator||(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return lhs.getNumeral() || rhs.getNumeral();
    }

    friend NumericLiteral operator!(const NumericLiteral &lhs) {
        return !lhs.getNumeral();
    }

    friend NumericLiteral operator-(const NumericLiteral &lhs) {
        return -lhs.getNumeral();
    }

    /// Return ite? lhs : rhs
    friend NumericLiteral ite(const NumericLiteral &cond, const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return cond.getNumeral() ? lhs.getNumeral() : rhs.getNumeral();
    }

    friend std::ostream &operator<<(std::ostream &out, const NumericLiteral &expr) {
        if (expr.is_plus_infinity())
            out << "+INF";
        else if (expr.is_minus_infinity())
            out << "-INF";
        else
            out << std::to_string(expr.getNumeral());
        return out;
    }

    friend bool eq(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return lhs._n == rhs._n;
    }

    friend NumericLiteral min(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return std::min(lhs.getNumeral(), rhs.getNumeral());
    }

    friend NumericLiteral max(const NumericLiteral &lhs, const NumericLiteral &rhs) {
        return std::max(lhs.getNumeral(), rhs.getNumeral());
    }

    // TODO: how to use initializer_list as argument?
    friend NumericLiteral min(std::initializer_list<NumericLiteral> _l) {
        NumericLiteral ret(INT_MAX);
        for (const auto &it: _l) {
            if (it.is_minus_infinity())
                return minus_infinity();
            else if (!it.geq(ret)) {
                ret = it;
            }
        }
        return ret;
    }

    friend NumericLiteral max(std::initializer_list<NumericLiteral> _l) {
        NumericLiteral ret(INT_MIN);
        for (const auto &it: _l) {
            if (it.is_plus_infinity())
                return plus_infinity();
            else if (!it.leq(ret)) {
                ret = it;
            }
        }
        return ret;
    }

    //%}


}; // end class IntervalDouble
} // end namespace SVF
#endif //Z3_EXAMPLE_Number_H
