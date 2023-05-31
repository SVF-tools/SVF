//===- NumericLiteral.h ----Number wrapper for domains------------------//
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
 * NumericLiteral.h
 *
 *  Created on: Aug 4, 2022
 *      Author: Xiao Cheng, Jiawei Wang
 *
 */

#ifndef Z3_EXAMPLE_Number_H
#define Z3_EXAMPLE_Number_H

#include <utility>

#include "SVFIR/SVFType.h"
#include "AbstractExecution/BoundedZ3Expr.h"

namespace SVF
{
class NumericLiteral
{
private:
    BoundedZ3Expr _n;

public:
    /// Default constructor
    NumericLiteral() = delete;

    /// Create a new NumericLiteral from s32_t

    NumericLiteral(const Z3Expr &z3Expr) : _n(z3Expr) {}

    NumericLiteral(const z3::expr &e) : _n(e) {}

    NumericLiteral(s32_t i) : _n(i) {}

    NumericLiteral(int64_t i) : _n(i) {}

    NumericLiteral(BoundedZ3Expr z3Expr) : _n(std::move(z3Expr)) {}

    virtual ~NumericLiteral() = default;

    /// Copy Constructor
    NumericLiteral(const NumericLiteral &) = default;

    /// Move Constructor
    NumericLiteral(NumericLiteral &&) = default;

    /// Operator = , another Copy Constructor
    inline NumericLiteral &operator=(const NumericLiteral &) = default;

    /// Operator = , another Move Constructor
    inline NumericLiteral &operator=(NumericLiteral &&) = default;

    static NumericLiteral plus_infinity()
    {
        return BoundedZ3Expr::plus_infinity();
    }

    static NumericLiteral minus_infinity()
    {
        return BoundedZ3Expr::minus_infinity();
    }

    static z3::context &getContext()
    {
        return BoundedZ3Expr::getContext();
    }

    const std::string to_string() const
    {
        return _n.to_string();
    }

    /// Check if this is minus infinity
    inline bool is_minus_infinity() const
    {
        return _n.is_minus_infinite();
    }

    /// Check if this is plus infinity
    inline bool is_plus_infinity() const
    {
        return _n.is_plus_infinite();
    }

    /// Check if this is infinity (either of plus/minus)
    inline bool is_infinity() const
    {
        return is_minus_infinity() || is_plus_infinity();
    }

    /// Check if this is zero
    inline bool is_zero() const
    {
        return _n.is_zero();
    }

    /// Return Numeral
    inline int64_t getNumeral() const
    {
        return _n.getNumeral();
    }

    /// Check two object is equal
    bool equal(const NumericLiteral &rhs) const
    {
        return eq(*this, rhs);
    }

    /// Less then or equal
    bool leq(const NumericLiteral &rhs) const
    {
        if (is_infinity() ^ rhs.is_infinity())
        {
            if (is_infinity())
            {
                return is_minus_infinity();
            }
            else
            {
                return rhs.is_plus_infinity();
            }
        }
        if(is_infinity() && rhs.is_infinity())
        {
            if(is_minus_infinity()) return true;
            else return rhs.is_plus_infinity();
        }
        else
            return _n.leq(rhs._n).simplify().is_true();
    }

    // Greater than or equal
    bool geq(const NumericLiteral &rhs) const
    {
        if (is_infinity() ^ rhs.is_infinity())
        {
            if (is_infinity())
            {
                return is_plus_infinity();
            }
            else
            {
                return rhs.is_minus_infinity();
            }
        }
        if(is_infinity() && rhs.is_infinity())
        {
            if(is_plus_infinity()) return true;
            else return rhs.is_minus_infinity();
        }
        else
            return _n.geq(rhs._n).simplify().is_true();
    }


    /// Reload operator
    //{%
    friend NumericLiteral operator==(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return eq(lhs, rhs) ? 1 : 0;
    }

    friend NumericLiteral operator!=(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return !eq(lhs, rhs) ? 1 : 0;
    }

    friend NumericLiteral operator>(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (!lhs.leq(rhs)) ? 1 : 0;
    }

    friend NumericLiteral operator<(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (!lhs.geq(rhs)) ? 1 : 0;
    }

    friend NumericLiteral operator<=(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return lhs.leq(rhs) ? 1 : 0;
    }

    friend NumericLiteral operator>=(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return lhs.geq(rhs) ? 1 : 0;
    }

    friend NumericLiteral operator+(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        if (!lhs.is_infinity() && !rhs.is_infinity())
            return (lhs._n + rhs._n).simplify();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return rhs;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return lhs;
        else if (eq(lhs, rhs))
            return lhs;
        else
        {
            assert(false && "undefined operation +oo + -oo");
            abort();
        }
    }

    friend NumericLiteral operator-(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        if (!lhs.is_infinity() && !rhs.is_infinity())
            return (lhs._n - rhs._n).simplify();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return -rhs;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return lhs;
        else if (!eq(lhs, rhs))
            return lhs;
        else
        {
            assert(false && "undefined operation +oo - +oo");
            abort();
        }
    }

    friend NumericLiteral operator*(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        if (lhs.is_zero() || rhs.is_zero())
            return 0;
        else if (lhs.is_infinity() && rhs.is_infinity())
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
        else if (lhs.is_infinity())
            return !rhs.leq(0) ? lhs : -lhs;
        else if (rhs.is_infinity())
            return !lhs.leq(0) ? rhs : -rhs;
        else
            return (lhs._n * rhs._n).simplify();
    }

    friend NumericLiteral operator/(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        if (rhs.is_zero())
        {

            assert(false && "divide by zero");
            abort();
        }
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return (lhs._n / rhs._n).simplify();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return !rhs.leq(0) ? lhs : -lhs;
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    friend NumericLiteral operator%(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        if (rhs.is_zero())
        {
            assert(false && "divide by zero");
            abort();
        }
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return (lhs._n % rhs._n).simplify();
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0;
        // TODO: not sure
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return !rhs.leq(0) ? lhs : -lhs;
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    // TODO: logic operation for infinity?
    friend NumericLiteral operator^(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (lhs._n ^ rhs._n).simplify();
    }

    friend NumericLiteral operator&(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (lhs._n & rhs._n).simplify();
    }

    friend NumericLiteral operator|(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (lhs._n | rhs._n).simplify();
    }

    friend NumericLiteral operator>>(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs.geq(0) ? 0 : -1;
        else
            return (s32_t) lhs.getNumeral() >> (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator<<(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs.geq(0) ? plus_infinity() : minus_infinity();
        else
            return (s32_t) lhs.getNumeral() << (s32_t) rhs.getNumeral();
    }

    friend NumericLiteral operator&&(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (lhs._n && rhs._n).simplify();
    }

    friend NumericLiteral operator||(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return (lhs._n || rhs._n).simplify();
    }

    friend NumericLiteral operator!(const NumericLiteral &lhs)
    {
        return (!lhs._n).simplify();
    }

    friend NumericLiteral operator-(const NumericLiteral &lhs)
    {
        if (lhs.is_plus_infinity())
        {
            return minus_infinity();
        }
        else if (lhs.is_minus_infinity())
        {
            return plus_infinity();
        }
        else
            return (-lhs._n).simplify();
    }

    /// Return ite? lhs : rhs
    friend NumericLiteral ite(const NumericLiteral &cond, const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return ite(cond._n, lhs._n, rhs._n).simplify();
    }

    friend std::ostream &operator<<(std::ostream &out, const NumericLiteral &expr)
    {
        if (expr.is_plus_infinity())
            out << "+INF";
        else if (expr.is_minus_infinity())
            out << "-INF";
        else
            out << expr._n;
        return out;
    }

    friend bool eq(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return eq(lhs._n, rhs._n);
    }

    friend NumericLiteral min(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return lhs.leq(rhs) ? lhs : rhs;
    }

    friend NumericLiteral max(const NumericLiteral &lhs, const NumericLiteral &rhs)
    {
        return lhs.leq(rhs) ? rhs : lhs;
    }

    friend NumericLiteral abs(const NumericLiteral &lhs)
    {
        return lhs.leq(0) ? -lhs : lhs;
    }

    // TODO: how to use initializer_list as argument?
    static NumericLiteral min(std::vector<NumericLiteral>& _l)
    {
        NumericLiteral ret(plus_infinity());
        for (const auto &it: _l)
        {
            if (it.is_minus_infinity())
                return minus_infinity();
            else if (!it.geq(ret))
            {
                ret = it;
            }
        }
        return ret;
    }

    static NumericLiteral max(std::vector<NumericLiteral>& _l)
    {
        NumericLiteral ret(minus_infinity());
        for (const auto &it: _l)
        {
            if (it.is_plus_infinity())
                return plus_infinity();
            else if (!it.leq(ret))
            {
                ret = it;
            }
        }
        return ret;
    }

    //%}


}; // end class NumericLiteral
} // end namespace SVF
#endif //Z3_EXAMPLE_Number_H
