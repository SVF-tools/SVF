//===- BoundedFloat.h ----Address Value Sets-------------------------//
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
 * BoundedFloat.h
 *
 *  Created on: Mar 20, 2023
 *      Author: Xiao Cheng
 *
 */

#ifndef SVF_BoundedFloat_H
#define SVF_BoundedFloat_H

#include "SVFIR/SVFType.h"
#include <cmath>
#include <cfloat> // For DBL_MAX

namespace SVF
{

/*!
 * Atom Z3 expr for unlimited precision integers
 */
class BoundedFloat
{
private:

    float _fVal;

    BoundedFloat() = default;

public:
    BoundedFloat(float fVal) : _fVal(fVal) {}

    BoundedFloat(const BoundedFloat& rhs) : _fVal(rhs._fVal) {}
    inline BoundedFloat& operator=(const BoundedFloat& rhs)
    {
        _fVal = rhs._fVal;
        return *this;
    }

    BoundedFloat(BoundedFloat &&rhs) : _fVal(std::move(rhs._fVal)) {}


    inline BoundedFloat &operator=(BoundedFloat &&rhs)
    {
        _fVal = std::move(rhs._fVal);
        return *this;
    }

    const float getFVal() const {
        return _fVal;
    }

    bool is_plus_infinity() const
    {
        return _fVal == std::numeric_limits<float>::infinity();
    }

    bool is_minus_infinity() const
    {
        return _fVal == -std::numeric_limits<float>::infinity();
    }

    bool is_infinity() const
    {
        return is_plus_infinity() || is_minus_infinity();
    }

    void set_plus_infinity()
    {
        *this = plus_infinity();
    }

    void set_minus_infinity()
    {
        *this = minus_infinity();
    }

    static BoundedFloat plus_infinity()
    {
        return std::numeric_limits<float>::infinity();
    }

    static BoundedFloat minus_infinity()
    {
        return -std::numeric_limits<float>::infinity();
    }


    bool is_zero() const
    {
        return _fVal == 0.0f;
    }

    static bool isZero(const BoundedFloat &expr)
    {
        return expr._fVal == 0.0f;
    }

    bool equal(const BoundedFloat &rhs) const
    {
        return _fVal == rhs._fVal;
    }

    bool leq(const BoundedFloat &rhs) const
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
            return _fVal <= rhs._fVal;
    }

    bool geq(const BoundedFloat &rhs) const
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
            return _fVal >= rhs._fVal;
    }

    /// Reload operator
    //{%
    friend BoundedFloat operator==(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return lhs.equal(rhs);
    }

    friend BoundedFloat operator!=(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return !lhs.equal(rhs);
    }

    friend BoundedFloat operator>(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return !lhs.leq(rhs);
    }

    friend BoundedFloat operator<(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return !lhs.geq(rhs);
    }

    friend BoundedFloat operator<=(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return lhs.leq(rhs);
    }

    friend BoundedFloat operator>=(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return lhs.geq(rhs);
    }

    /**
     * Adds two floating-point numbers safely, checking for overflow and
     * underflow conditions.
     *
     * @param lhs Left-hand side operand of the addition.
     * @param rhs Right-hand side operand of the addition.
     * @return The sum of lhs and rhs. If overflow or underflow occurs, returns
     * positive or negative infinity.
     */
    static float safeAdd(float lhs, float rhs)
    {
        if ((lhs == std::numeric_limits<float>::infinity() &&
             rhs == -std::numeric_limits<float>::infinity()) ||
            (lhs == -std::numeric_limits<float>::infinity() &&
             rhs == std::numeric_limits<float>::infinity())) {
            assert(false && "invalid add");
        }
        float res =
            lhs + rhs; // Perform the addition and store the result in 'res'

        // Check if the result is positive infinity due to overflow
        if (res == std::numeric_limits<float>::infinity())
        {
            return res; // Positive overflow has occurred, return positive
                        // infinity
        }

        // Check if the result is negative infinity, which can indicate a large
        // negative overflow
        if (res == -std::numeric_limits<float>::infinity())
        {
            return res; // Negative "overflow", effectively an underflow to
                        // negative infinity
        }

        // Check for positive overflow: verify if both operands are positive and
        // their sum exceeds the maximum float value
        if (lhs > 0 && rhs > 0 &&
            (std::numeric_limits<float>::max() - lhs) < rhs)
        {
            res =
                std::numeric_limits<float>::infinity(); // Set result to
                                                        // positive infinity to
                                                        // indicate overflow
            return res;
        }

        // Check for an underflow scenario: both numbers are negative and their
        // sum is more negative than what float can represent
        if (lhs < 0 && rhs < 0 &&
            (-std::numeric_limits<float>::max() - lhs) > rhs)
        {
            res = -std::numeric_limits<
                float>::infinity(); // Set result to negative infinity to
                                    // clarify extreme negative sum
            return res;
        }

        // If none of the above conditions are met, return the result of the
        // addition
        return res;
    }

    friend BoundedFloat operator+(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return safeAdd(lhs._fVal, rhs._fVal);
    }

    friend BoundedFloat operator-(const BoundedFloat &lhs)
    {
        return -lhs._fVal;
    }

    friend BoundedFloat operator-(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return safeAdd(lhs._fVal, -rhs._fVal);
    }

    /**
     * Safely multiplies two floating-point numbers, checking for overflow and
     * underflow.
     *
     * @param lhs Left-hand side operand of the multiplication.
     * @param rhs Right-hand side operand of the multiplication.
     * @return The product of lhs and rhs. If overflow or underflow occurs,
     * returns positive or negative infinity accordingly.
     */
    static float safeMul(float lhs, float rhs)
    {
        if (lhs == 0.0f || rhs == 0.0f)
            return 0.0f;
        float res = lhs * rhs;
        // Check if the result is positive infinity due to overflow
        if (res == std::numeric_limits<float>::infinity())
        {
            return res; // Positive overflow has occurred, return positive
                        // infinity
        }

        // Check if the result is negative infinity, which can indicate a large
        // negative overflow
        if (res == -std::numeric_limits<float>::infinity())
        {
            return res; // Negative "overflow", effectively an underflow to
                        // negative infinity
        }
        // Check for overflow scenarios
        if (lhs > 0 && rhs > 0 && lhs > std::numeric_limits<float>::max() / rhs)
        {
            return std::numeric_limits<float>::infinity();
        }
        if (lhs < 0 && rhs < 0 && lhs < std::numeric_limits<float>::max() / rhs)
        {
            return std::numeric_limits<float>::infinity();
        }

        // Check for "underflow" scenarios (negative overflow)
        if (lhs > 0 && rhs < 0 &&
            rhs < std::numeric_limits<float>::lowest() / lhs)
        {
            return -std::numeric_limits<float>::infinity();
        }
        if (lhs < 0 && rhs > 0 &&
            lhs < std::numeric_limits<float>::lowest() / rhs)
        {
            return -std::numeric_limits<float>::infinity();
        }

        return res; // If no overflow or underflow, return the product
    }

    friend BoundedFloat operator*(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return safeMul(lhs._fVal, rhs._fVal);
    }

    /**
     * Safely divides one floating-point number by another, checking for
     * division by zero and overflow.
     *
     * @param lhs Left-hand side operand (numerator).
     * @param rhs Right-hand side operand (denominator).
     * @return The quotient of lhs and rhs. Returns positive or negative
     * infinity for division by zero, or when overflow occurs.
     */
    static float safeDiv(float lhs, float rhs)
    {
        // Check for division by zero
        if (rhs == 0.0f)
        {
            return (lhs >= 0.0f) ? std::numeric_limits<float>::infinity()
                              : -std::numeric_limits<float>::infinity();
        }
        float res = lhs / rhs;
        // Check if the result is positive infinity due to overflow
        if (res == std::numeric_limits<float>::infinity())
        {
            return res; // Positive overflow has occurred, return positive
                        // infinity
        }

        // Check if the result is negative infinity, which can indicate a large
        // negative overflow
        if (res == -std::numeric_limits<float>::infinity())
        {
            return res; // Negative "overflow", effectively an underflow to
                        // negative infinity
        }

        // Check for overflow when dividing small numbers
        if (rhs > 0 && rhs < std::numeric_limits<float>::min() &&
            lhs > std::numeric_limits<float>::max() * rhs)
        {
            return std::numeric_limits<float>::infinity();
        }
        if (rhs < 0 && rhs > -std::numeric_limits<float>::min() &&
            lhs > std::numeric_limits<float>::max() * rhs)
        {
            return -std::numeric_limits<float>::infinity();
        }

        return res; // If no special cases, return the quotient
    }

    friend BoundedFloat operator/(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return safeDiv(lhs._fVal, rhs._fVal);
    }

    friend BoundedFloat operator%(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        if (rhs.is_zero()) assert(false && "divide by zero");
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return std::fmod(lhs._fVal, rhs._fVal);
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0.0f;
        // TODO: not sure
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return ite(rhs._fVal > 0.0f, lhs, -lhs);
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
        abort();
    }

    inline bool is_int() const {
        return _fVal == std::round(_fVal);
    }
    inline bool is_real() const {
        return !is_int();
    }

    friend BoundedFloat operator^(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        int lInt = std::round(rhs._fVal), rInt = std::round(rhs._fVal);
        return lInt ^ rInt;
    }

    friend BoundedFloat operator&(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        int lInt = std::round(rhs._fVal), rInt = std::round(rhs._fVal);
        return lInt & rInt;
    }

    friend BoundedFloat operator|(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        int lInt = std::round(rhs._fVal), rInt = std::round(rhs._fVal);
        return lInt | rInt;
    }


    friend BoundedFloat operator&&(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return lhs._fVal && rhs._fVal;
    }

    friend BoundedFloat operator||(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return lhs._fVal || rhs._fVal;
    }

    friend BoundedFloat operator!(const BoundedFloat &lhs)
    {
        return !lhs._fVal;
    }

    friend BoundedFloat operator>>(const BoundedFloat &lhs, const BoundedFloat &rhs)
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
    
    friend BoundedFloat operator<<(const BoundedFloat &lhs, const BoundedFloat &rhs)
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
    
    friend BoundedFloat ite(const BoundedFloat& cond, const BoundedFloat& lhs,
                            const BoundedFloat& rhs)
    {
        return cond._fVal != 0.0f ? lhs._fVal : rhs._fVal;
    }

    friend std::ostream &operator<<(std::ostream &out, const BoundedFloat &expr)
    {
        out << expr._fVal;
        return out;
    }

    friend bool eq(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return lhs._fVal == rhs._fVal;
    }

    friend BoundedFloat min(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return std::min(lhs._fVal, rhs._fVal);
    }

    friend BoundedFloat max(const BoundedFloat &lhs, const BoundedFloat &rhs)
    {
        return std::max(lhs._fVal, rhs._fVal);
    }
    
    static BoundedFloat min(std::vector<BoundedFloat>& _l)
    {
        BoundedFloat ret(plus_infinity());
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
    
    static BoundedFloat max(std::vector<BoundedFloat>& _l)
    {
        BoundedFloat ret(minus_infinity());
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

    friend BoundedFloat abs(const BoundedFloat &lhs)
    {
        return lhs.leq(0) ? -lhs : lhs;
    }

    inline bool is_true() const
    {
        return _fVal != 0.0f;
    }

    /// Return Numeral
    inline s64_t getNumeral() const
    {
        if (is_minus_infinity())
        {
            return INT64_MIN;
        }
        else if (is_plus_infinity())
        {
            return INT64_MAX;
        }
        else
        {
            return std::round(_fVal);
        }
    }

    inline s64_t getIntNumeral() const {
        return getNumeral();
    }

    inline double getRealNumeral() const {
        return _fVal;
    }

    inline const std::string to_string() const {
        return std::to_string(_fVal);
    }

    //%}
}; // end class BoundedFloat
} // end namespace SVF

#endif //SVF_BoundedFloat_H

