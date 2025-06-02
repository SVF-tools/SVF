//===- NumericValue.h ----Numeric Value-------------------------//
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
 * Numeri Value.h
 *
 *  Created on: May 11, 2024
 *      Author: Xiao Cheng, Jiawei Ren
 *
 */
// The implementation is based on
// Xiao Cheng, Jiawei Wang and Yulei Sui. Precise Sparse Abstract Execution via Cross-Domain Interaction.
// 46th International Conference on Software Engineering. (ICSE24)

#ifndef SVF_NUMERICVALUE_H
#define SVF_NUMERICVALUE_H

#include "SVFIR/SVFType.h"
#include <cfloat> // For DBL_MAX
#include <cmath>
#include <utility>

#define epsilon std::numeric_limits<double>::epsilon();
namespace SVF
{

/**
 * @brief A class representing a bounded 64-bit integer.
 *
 * BoundedInt is a class that represents a 64-bit integer that can also
 * represent positive and negative infinity. It includes a 64-bit integer
 * value and a boolean flag indicating whether the value is infinite.
 * If the value is infinite, the integer value is used to represent the sign
 * of infinity (1 for positive infinity and 0 for negative infinity).
 */
class BoundedInt
{
protected:
    s64_t _iVal; // The 64-bit integer value.
    bool _isInf; // True if the value is infinite. If true, _iVal == 1
    // represents positive infinity and _iVal == 0 represents
    // negative infinity.

    // Default constructor is protected to prevent creating an object without
    // initializing _iVal and _isInf.
    BoundedInt() = default;

public:
    // Constructs a BoundedInt with the given 64-bit integer value. The value is
    // not infinite.
    BoundedInt(s64_t fVal) : _iVal(fVal), _isInf(false) {}

    // Constructs a BoundedInt with the given 64-bit integer value and infinity
    // flag.
    BoundedInt(s64_t fVal, bool isInf) : _iVal(fVal), _isInf(isInf) {}

    // Copy constructor.
    BoundedInt(const BoundedInt& rhs) : _iVal(rhs._iVal), _isInf(rhs._isInf) {}

    // Copy assignment operator.
    BoundedInt& operator=(const BoundedInt& rhs)
    {
        _iVal = rhs._iVal;
        _isInf = rhs._isInf;
        return *this;
    }

    // Move constructor.
    BoundedInt(BoundedInt&& rhs) : _iVal(rhs._iVal), _isInf(rhs._isInf) {}

    // Move assignment operator.
    BoundedInt& operator=(BoundedInt&& rhs)
    {
        _iVal = rhs._iVal;
        _isInf = rhs._isInf;
        return *this;
    }

    // Virtual destructor.
    virtual ~BoundedInt() {}

    // Checks if the BoundedInt represents positive infinity.
    bool is_plus_infinity() const
    {
        return _isInf && _iVal == 1;
    }

    // Checks if the BoundedInt represents negative infinity.
    bool is_minus_infinity() const
    {
        return _isInf && _iVal == -1;
    }

    // Checks if the BoundedInt represents either positive or negative infinity.
    bool is_infinity() const
    {
        return is_plus_infinity() || is_minus_infinity();
    }

    // Sets the BoundedInt to represent positive infinity.
    void set_plus_infinity()
    {
        *this = plus_infinity();
    }

    // Sets the BoundedInt to represent negative infinity.
    void set_minus_infinity()
    {
        *this = minus_infinity();
    }

    // Returns a BoundedInt representing positive infinity.
    static BoundedInt plus_infinity()
    {
        return {1, true};
    }

    // Returns a BoundedInt representing negative infinity.
    static BoundedInt minus_infinity()
    {
        return {-1, true};
    }

    // Checks if the BoundedInt represents zero.
    bool is_zero() const
    {
        return _iVal == 0;
    }

    // Checks if the given BoundedInt represents zero.
    static bool isZero(const BoundedInt& expr)
    {
        return expr._iVal == 0;
    }

    // Checks if the BoundedInt is equal to another BoundedInt.
    bool equal(const BoundedInt& rhs) const
    {
        return _iVal == rhs._iVal && _isInf == rhs._isInf;
    }

    // Checks if the BoundedInt is less than or equal to another BoundedInt.
    bool leq(const BoundedInt& rhs) const
    {
        // If only one of the two BoundedInts is infinite.
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
        // If both BoundedInts are infinite.
        if (is_infinity() && rhs.is_infinity())
        {
            if (is_minus_infinity())
                return true;
            else
                return rhs.is_plus_infinity();
        }
        // If neither BoundedInt is infinite.
        else
            return _iVal <= rhs._iVal;
    }

    // Checks if the BoundedInt is greater than or equal to another BoundedInt.
    bool geq(const BoundedInt& rhs) const
    {
        // If only one of the two BoundedInts is infinite.
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
        // If both BoundedInts are infinite.
        if (is_infinity() && rhs.is_infinity())
        {
            if (is_plus_infinity())
                return true;
            else
                return rhs.is_minus_infinity();
        }
        // If neither BoundedInt is infinite.
        else
            return _iVal >= rhs._iVal;
    }

    /// Reload operator
    //{%
    // Overloads the equality operator to compare two BoundedInt objects.
    friend BoundedInt operator==(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs.equal(rhs);
    }

    // Overloads the inequality operator to compare two BoundedInt objects.
    friend BoundedInt operator!=(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return !lhs.equal(rhs);
    }

    // Overloads the greater than operator to compare two BoundedInt objects.
    friend BoundedInt operator>(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return !lhs.leq(rhs);
    }

    // Overloads the less than operator to compare two BoundedInt objects.
    friend BoundedInt operator<(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return !lhs.geq(rhs);
    }

    // Overloads the less than or equal to operator to compare two BoundedInt
    // objects.
    friend BoundedInt operator<=(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs.leq(rhs);
    }

    // Overloads the greater than or equal to operator to compare two BoundedInt
    // objects.
    friend BoundedInt operator>=(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs.geq(rhs);
    }

    /**
     * Safely adds two BoundedInt objects.
     *
     * This function adds two BoundedInt objects in a way that respects the
     * bounds of the underlying s64_t type. It checks for conditions that would
     * result in overflow or underflow and returns a representation of positive
     * or negative infinity in those cases. If addition of the two numbers would
     * result in a value that is within the representable range of s64_t, it
     * performs the addition and returns the result. If the addition is not
     * defined (e.g., positive infinity plus negative infinity), it asserts
     * false to indicate an error.
     *
     * @param lhs The first BoundedInt to add. This can be any valid BoundedInt,
     * including positive and negative infinity.
     * @param rhs The second BoundedInt to add. This can be any valid
     * BoundedInt, including positive and negative infinity.
     * @return A BoundedInt that represents the result of the addition. If the
     * addition would result in overflow, the function returns a BoundedInt
     * representing positive infinity. If the addition would result in
     * underflow, the function returns a BoundedInt representing negative
     * infinity. If the addition is not defined (e.g., positive infinity plus
     * negative infinity), the function asserts false and does not return a
     * value.
     */
    static BoundedInt safeAdd(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        // If one number is positive infinity and the other is negative
        // infinity, this is an invalid operation, so we assert false.
        if ((lhs.is_plus_infinity() && rhs.is_minus_infinity()) ||
                (lhs.is_minus_infinity() && rhs.is_plus_infinity()))
        {
            assert(false && "invalid add");
        }

        // If either number is positive infinity, the result is positive
        // infinity.
        if (lhs.is_plus_infinity() || rhs.is_plus_infinity())
        {
            return plus_infinity();
        }

        // If either number is negative infinity, the result is negative
        // infinity.
        if (lhs.is_minus_infinity() || rhs.is_minus_infinity())
        {
            return minus_infinity();
        }

        // If both numbers are positive and their sum would exceed the maximum
        // representable number, the result is positive infinity.
        if (lhs._iVal > 0 && rhs._iVal > 0 &&
                (std::numeric_limits<s64_t>::max() - lhs._iVal) < rhs._iVal)
        {
            return plus_infinity();
        }

        // If both numbers are negative and their sum would be less than the
        // most negative representable number, the result is negative infinity.
        if (lhs._iVal < 0 && rhs._iVal < 0 &&
                (-std::numeric_limits<s64_t>::max() - lhs._iVal) > rhs._iVal)
        {
            return minus_infinity();
        }

        // If none of the above conditions are met, the numbers can be safely
        // added.
        return lhs._iVal + rhs._iVal;
    }

    // Overloads the addition operator to safely add two BoundedInt objects.
    // Utilizes the safeAdd method to handle potential overflow and underflow.
    friend BoundedInt operator+(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return safeAdd(lhs, rhs);
    }

    // Overloads the unary minus operator to negate a BoundedInt object.
    // The operation simply negates the internal integer value.
    friend BoundedInt operator-(const BoundedInt& lhs)
    {
        return {-lhs._iVal, lhs._isInf};
    }

    // Overloads the subtraction operator to safely subtract one BoundedInt
    // object from another. This is implemented as the addition of the lhs and
    // the negation of the rhs, using the safeAdd method for safety.
    friend BoundedInt operator-(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return safeAdd(lhs, -rhs);
    }

    /**
     * @brief Performs safe multiplication of two BoundedInt objects.
     *
     * This function ensures that the multiplication of two BoundedInt objects
     * doesn't result in overflow or underflow. It returns the multiplication
     * result if it can be represented within the range of a 64-bit integer. If
     * the result would be larger than the maximum representable positive
     * number, it returns positive infinity. If the result would be less than
     * the minimum representable negative number, it returns negative infinity.
     * If either of the inputs is zero, the result is zero. If either of the
     * inputs is infinity, the result is determined by the signs of the inputs.
     *
     * @param lhs The first BoundedInt to multiply.
     * @param rhs The second BoundedInt to multiply.
     * @return The result of the multiplication, or positive/negative infinity
     * if the result would be outside the range of a 64-bit integer.
     */
    static BoundedInt safeMul(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        // If either number is zero, the result is zero.
        if (lhs._iVal == 0 || rhs._iVal == 0)
            return 0;

        // If either number is infinity, the result depends on the signs of the
        // numbers.
        if (lhs.is_infinity() || rhs.is_infinity())
        {
            // If the signs of the numbers are the same, the result is positive
            // infinity. If the signs of the numbers are different, the result
            // is negative infinity.
            if (lhs._iVal * rhs._iVal > 0)
            {
                return plus_infinity();
            }
            else
            {
                return minus_infinity();
            }
        }

        // If both numbers are positive and their product would exceed the
        // maximum representable number, the result is positive infinity.
        if (lhs._iVal > 0 && rhs._iVal > 0 &&
                (std::numeric_limits<s64_t>::max() / lhs._iVal) < rhs._iVal)
        {
            return plus_infinity();
        }

        // If both numbers are negative and their product would exceed the
        // maximum representable number, the result is positive infinity.
        if (lhs._iVal < 0 && rhs._iVal < 0 &&
                (std::numeric_limits<s64_t>::max() / lhs._iVal) > rhs._iVal)
        {
            return plus_infinity();
        }

        // If one number is positive and the other is negative and their product
        // would be less than the most negative representable number, the result
        // is negative infinity.
        if ((lhs._iVal > 0 && rhs._iVal < 0 &&
                (-std::numeric_limits<s64_t>::max() / lhs._iVal) > rhs._iVal) ||
                (lhs._iVal < 0 && rhs._iVal > 0 &&
                 (-std::numeric_limits<s64_t>::max() / rhs._iVal) > lhs._iVal))
        {
            return minus_infinity();
        }

        // If none of the above conditions are met, the numbers can be safely
        // multiplied.
        return lhs._iVal * rhs._iVal;
    }


    friend BoundedInt operator%(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        if (rhs.is_zero())
            assert(false && "divide by zero");
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return lhs._iVal % rhs._iVal;
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0;
        // TODO: not sure
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return ite(rhs._iVal > 0, lhs, -lhs);
        else
            // TODO: +oo/-oo L'Hôpital's rule?
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
        abort();
    }
    // Overloads the multiplication operator to safely multiply two BoundedInt
    // objects. Utilizes the safeMul method to handle potential overflow.
    friend BoundedInt operator*(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return safeMul(lhs, rhs);
    }

    // Overloads the division operator to safely divide a BoundedInt object by
    // another. Utilizes the safeDiv method to handle potential division by zero
    // and overflow.
    friend BoundedInt operator/(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        if (rhs.is_zero())
        {
            assert(false && "divide by zero");
            abort();
        }
        else if (!lhs.is_infinity() && !rhs.is_infinity())
            return lhs._iVal / rhs._iVal;
        else if (!lhs.is_infinity() && rhs.is_infinity())
            return 0;
        else if (lhs.is_infinity() && !rhs.is_infinity())
            return ite(rhs._iVal >= 0, lhs, -lhs);
        else
            return eq(lhs, rhs) ? plus_infinity() : minus_infinity();
    }

    // Overload bitwise operators for BoundedInt objects. These operators
    // directly apply the corresponding bitwise operators to the internal
    // integer values of the BoundedInt objects.

    // Overloads the bitwise XOR operator for BoundedInt objects.
    friend BoundedInt operator^(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs._iVal ^ rhs._iVal;
    }

    // Overloads the bitwise AND operator for BoundedInt objects.
    friend BoundedInt operator&(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs._iVal & rhs._iVal;
    }

    // Overloads the bitwise OR operator for BoundedInt objects.
    friend BoundedInt operator|(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs._iVal | rhs._iVal;
    }

    // Overload logical operators for BoundedInt objects. These operators
    // directly apply the corresponding logical operators to the internal
    // integer values of the BoundedInt objects.

    // Overloads the logical AND operator for BoundedInt objects.
    friend BoundedInt operator&&(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs._iVal && rhs._iVal;
    }

    // Overloads the logical OR operator for BoundedInt objects.
    friend BoundedInt operator||(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs._iVal || rhs._iVal;
    }

    // Overloads the logical NOT operator for BoundedInt objects.
    friend BoundedInt operator!(const BoundedInt& lhs)
    {
        return !lhs._iVal;
    }

    // Overloads the right shift operator for BoundedInt objects.
    // This operation is safe as long as the right-hand side is non-negative.
    // If the left-hand side is zero or infinity, the result is the same as the
    // left-hand side. If the right-hand side is infinity, the result depends on
    // the sign of the left-hand side.
    friend BoundedInt operator>>(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs.geq(0) ? 0 : -1;
        else
            return lhs._iVal >> rhs._iVal;
    }

    // Overloads the left shift operator for BoundedInt objects.
    // This operation is safe as long as the right-hand side is non-negative.
    // If the left-hand side is zero or infinity, the result is the same as the
    // left-hand side. If the right-hand side is infinity, the result depends on
    // the sign of the left-hand side.
    friend BoundedInt operator<<(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs.geq(0) ? plus_infinity() : minus_infinity();
        else
            return lhs._iVal << rhs._iVal;
    }

    // Overloads the ternary if-then-else operator for BoundedInt objects.
    // The condition is evaluated as a boolean, and the result is either the
    // second or third argument depending on the condition.
    friend BoundedInt ite(const BoundedInt& cond, const BoundedInt& lhs,
                          const BoundedInt& rhs)
    {
        return cond._iVal != 0 ? lhs : rhs;
    }

    // Overloads the stream insertion operator for BoundedInt objects.
    // This allows BoundedInt objects to be printed directly using std::cout or
    // other output streams.
    friend std::ostream& operator<<(std::ostream& out, const BoundedInt& expr)
    {
        out << expr._iVal;
        return out;
    }

    // Defines a function to compare two BoundedInt objects for equality.
    // This function directly compares the internal integer values of the
    // BoundedInt objects.
    friend bool eq(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        return lhs._iVal == rhs._iVal && lhs._isInf == rhs._isInf;
    }

    // Defines a function to find the minimum of two BoundedInt objects.
    // This function directly compares the internal integer values of the BoundedInt objects,
    // and also checks if either of them represents infinity.
    friend BoundedInt min(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        if (lhs.is_minus_infinity() || rhs.is_minus_infinity())
            return minus_infinity();
        else if(lhs.is_plus_infinity())
            return rhs;
        else if(rhs.is_plus_infinity())
            return lhs;
        else
            return BoundedInt(std::min(lhs._iVal, rhs._iVal));
    }


    // Defines a function to find the maximum of two BoundedInt objects.
    // This function directly compares the internal integer values of the BoundedInt objects,
    // and also checks if either of them represents infinity.
    friend BoundedInt max(const BoundedInt& lhs, const BoundedInt& rhs)
    {
        if (lhs.is_plus_infinity() || rhs.is_plus_infinity())
            return plus_infinity();
        else if(lhs.is_minus_infinity())
            return rhs;
        else if(rhs.is_minus_infinity())
            return lhs;
        else
            return BoundedInt(std::max(lhs._iVal, rhs._iVal));
    }


    // Defines a function to find the minimum of a vector of BoundedInt objects.
    // This function iterates over the vector and returns the smallest
    // BoundedInt object.
    static BoundedInt min(std::vector<BoundedInt>& _l)
    {
        BoundedInt ret(plus_infinity());
        for (const auto& it : _l)
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

    // Defines a function to find the maximum of a vector of BoundedInt objects.
    // This function iterates over the vector and returns the largest BoundedInt
    // object.
    static BoundedInt max(std::vector<BoundedInt>& _l)
    {
        BoundedInt ret(minus_infinity());
        for (const auto& it : _l)
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

    // Defines a function to find the absolute value of a BoundedInt object.
    // This function directly applies the unary minus operator if the BoundedInt
    // object is negative.
    friend BoundedInt abs(const BoundedInt& lhs)
    {
        return lhs.leq(0) ? -lhs : lhs;
    }

    // Defines a method to check if a BoundedInt object is true.
    // A BoundedInt object is considered true if its internal integer value is
    // non-zero.
    inline bool is_true() const
    {
        return _iVal != 0;
    }

    /**
     * @brief Retrieves the numeral value of the BoundedInt object.
     *
     * This method returns the numeral representation of the BoundedInt object.
     * If the object represents negative infinity, it returns the minimum
     * representable 64-bit integer. If the object represents positive infinity,
     * it returns the maximum representable 64-bit integer. Otherwise, it
     * returns the actual 64-bit integer value of the object.
     *
     * @return The numeral value of the BoundedInt object.
     */
    inline s64_t getNumeral() const
    {
        // If the object represents negative infinity, return the minimum
        // representable 64-bit integer.
        if (is_minus_infinity())
        {
            return std::numeric_limits<s64_t>::min();
        }
        // If the object represents positive infinity, return the maximum
        // representable 64-bit integer.
        else if (is_plus_infinity())
        {
            return std::numeric_limits<s64_t>::max();
        }
        // Otherwise, return the actual 64-bit integer value of the object.
        else
        {
            return _iVal;
        }
    }

    inline virtual const std::string to_string() const
    {
        if (is_minus_infinity())
        {
            return "-oo";
        }
        if (is_plus_infinity())
        {
            return "+oo";
        }
        else
            return std::to_string(_iVal);
    }

    //%}

    bool is_real() const
    {
        return false;
    }

    inline s64_t getIntNumeral() const
    {
        return getNumeral();
    }

    inline double getRealNumeral() const
    {
        assert(false && "cannot get real number for integer!");
        abort();
    }

    const double getFVal() const
    {
        assert(false && "cannot get real number for integer!");
        abort();
    }
};
/*!
 * Bounded double numeric value
 */
class BoundedDouble
{
protected:
    double _fVal;

    BoundedDouble() = default;

public:
    BoundedDouble(double fVal) : _fVal(fVal) {}

    BoundedDouble(const BoundedDouble& rhs) : _fVal(rhs._fVal) {}

    BoundedDouble& operator=(const BoundedDouble& rhs)
    {
        _fVal = rhs._fVal;
        return *this;
    }

    BoundedDouble(BoundedDouble&& rhs) : _fVal(std::move(rhs._fVal)) {}

    BoundedDouble& operator=(BoundedDouble&& rhs)
    {
        _fVal = std::move(rhs._fVal);
        return *this;
    }

    virtual ~BoundedDouble() {}

    static bool doubleEqual(double a, double b)
    {
        if (std::isinf(a) && std::isinf(b))
            return a == b;
        return std::fabs(a - b) < epsilon;
    }

    const double getFVal() const
    {
        return _fVal;
    }

    bool is_plus_infinity() const
    {
        return _fVal == std::numeric_limits<double>::infinity();
    }

    bool is_minus_infinity() const
    {
        return _fVal == -std::numeric_limits<double>::infinity();
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

    static BoundedDouble plus_infinity()
    {
        return std::numeric_limits<double>::infinity();
    }

    static BoundedDouble minus_infinity()
    {
        return -std::numeric_limits<double>::infinity();
    }

    bool is_zero() const
    {
        return doubleEqual(_fVal, 0.0f);
    }

    static bool isZero(const BoundedDouble& expr)
    {
        return doubleEqual(expr.getFVal(), 0.0f);
    }

    bool equal(const BoundedDouble& rhs) const
    {
        return doubleEqual(_fVal, rhs._fVal);
    }

    bool leq(const BoundedDouble& rhs) const
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
        if (is_infinity() && rhs.is_infinity())
        {
            if (is_minus_infinity())
                return true;
            else
                return rhs.is_plus_infinity();
        }
        else
            return _fVal <= rhs._fVal;
    }

    bool geq(const BoundedDouble& rhs) const
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
        if (is_infinity() && rhs.is_infinity())
        {
            if (is_plus_infinity())
                return true;
            else
                return rhs.is_minus_infinity();
        }
        else
            return _fVal >= rhs._fVal;
    }

    /// Reload operator
    //{%
    friend BoundedDouble operator==(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        return lhs.equal(rhs);
    }

    friend BoundedDouble operator!=(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        return !lhs.equal(rhs);
    }

    friend BoundedDouble operator>(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        return !lhs.leq(rhs);
    }

    friend BoundedDouble operator<(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        return !lhs.geq(rhs);
    }

    friend BoundedDouble operator<=(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        return lhs.leq(rhs);
    }

    friend BoundedDouble operator>=(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
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
    static double safeAdd(double lhs, double rhs)
    {
        if ((lhs == std::numeric_limits<double>::infinity() &&
                rhs == -std::numeric_limits<double>::infinity()) ||
                (lhs == -std::numeric_limits<double>::infinity() &&
                 rhs == std::numeric_limits<double>::infinity()))
        {
            assert(false && "invalid add");
        }
        double res =
            lhs + rhs; // Perform the addition and store the result in 'res'

        // Check if the result is positive infinity due to overflow
        if (res == std::numeric_limits<double>::infinity())
        {
            return res; // Positive overflow has occurred, return positive
            // infinity
        }

        // Check if the result is negative infinity, which can indicate a large
        // negative overflow
        if (res == -std::numeric_limits<double>::infinity())
        {
            return res; // Negative "overflow", effectively an underflow to
            // negative infinity
        }

        // Check for positive overflow: verify if both operands are positive and
        // their sum exceeds the maximum double value
        if (lhs > 0 && rhs > 0 &&
                (std::numeric_limits<double>::max() - lhs) < rhs)
        {
            res = std::numeric_limits<double>::infinity(); // Set result to
            // positive infinity to
            // indicate overflow
            return res;
        }

        // Check for an underflow scenario: both numbers are negative and their
        // sum is more negative than what double can represent
        if (lhs < 0 && rhs < 0 &&
                (-std::numeric_limits<double>::max() - lhs) > rhs)
        {
            res = -std::numeric_limits<
                  double>::infinity(); // Set result to negative infinity to
            // clarify extreme negative sum
            return res;
        }

        // If none of the above conditions are met, return the result of the
        // addition
        return res;
    }

    friend BoundedDouble operator+(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        return safeAdd(lhs._fVal, rhs._fVal);
    }

    friend BoundedDouble operator-(const BoundedDouble& lhs)
    {
        return -lhs._fVal;
    }

    friend BoundedDouble operator-(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
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
    static double safeMul(double lhs, double rhs)
    {
        if (doubleEqual(lhs, 0.0f) || doubleEqual(rhs, 0.0f))
            return 0.0f;
        double res = lhs * rhs;
        // Check if the result is positive infinity due to overflow
        if (res == std::numeric_limits<double>::infinity())
        {
            return res; // Positive overflow has occurred, return positive
            // infinity
        }

        // Check if the result is negative infinity, which can indicate a large
        // negative overflow
        if (res == -std::numeric_limits<double>::infinity())
        {
            return res; // Negative "overflow", effectively an underflow to
            // negative infinity
        }
        // Check for overflow scenarios
        if (lhs > 0 && rhs > 0 &&
                lhs > std::numeric_limits<double>::max() / rhs)
        {
            return std::numeric_limits<double>::infinity();
        }
        if (lhs < 0 && rhs < 0 &&
                lhs < std::numeric_limits<double>::max() / rhs)
        {
            return std::numeric_limits<double>::infinity();
        }

        // Check for "underflow" scenarios (negative overflow)
        if (lhs > 0 && rhs < 0 &&
                rhs < std::numeric_limits<double>::lowest() / lhs)
        {
            return -std::numeric_limits<double>::infinity();
        }
        if (lhs < 0 && rhs > 0 &&
                lhs < std::numeric_limits<double>::lowest() / rhs)
        {
            return -std::numeric_limits<double>::infinity();
        }

        return res; // If no overflow or underflow, return the product
    }

    friend BoundedDouble operator*(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
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
    static double safeDiv(double lhs, double rhs)
    {
        // Check for division by zero
        if (doubleEqual(rhs, 0.0f))
        {
            return (lhs >= 0.0f) ? std::numeric_limits<double>::infinity()
                   : -std::numeric_limits<double>::infinity();
        }
        double res = lhs / rhs;
        // Check if the result is positive infinity due to overflow
        if (res == std::numeric_limits<double>::infinity())
        {
            return res; // Positive overflow has occurred, return positive
            // infinity
        }

        // Check if the result is negative infinity, which can indicate a large
        // negative overflow
        if (res == -std::numeric_limits<double>::infinity())
        {
            return res; // Negative "overflow", effectively an underflow to
            // negative infinity
        }

        // Check for overflow when dividing small numbers
        if (rhs > 0 && rhs < std::numeric_limits<double>::min() &&
                lhs > std::numeric_limits<double>::max() * rhs)
        {
            return std::numeric_limits<double>::infinity();
        }
        if (rhs < 0 && rhs > -std::numeric_limits<double>::min() &&
                lhs > std::numeric_limits<double>::max() * rhs)
        {
            return -std::numeric_limits<double>::infinity();
        }

        return res; // If no special cases, return the quotient
    }

    friend BoundedDouble operator/(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        return safeDiv(lhs._fVal, rhs._fVal);
    }

    friend BoundedDouble operator%(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        if (rhs.is_zero())
            assert(false && "divide by zero");
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

    inline bool is_int() const
    {
        return _fVal == std::round(_fVal);
    }
    inline bool is_real() const
    {
        return !is_int();
    }

    friend BoundedDouble operator^(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        int lInt = std::round(lhs._fVal), rInt = std::round(rhs._fVal);
        return lInt ^ rInt;
    }

    friend BoundedDouble operator&(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        int lInt = std::round(lhs._fVal), rInt = std::round(rhs._fVal);
        return lInt & rInt;
    }

    friend BoundedDouble operator|(const BoundedDouble& lhs,
                                   const BoundedDouble& rhs)
    {
        int lInt = std::round(lhs._fVal), rInt = std::round(rhs._fVal);
        return lInt | rInt;
    }

    friend BoundedDouble operator&&(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        return lhs._fVal && rhs._fVal;
    }

    friend BoundedDouble operator||(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        return lhs._fVal || rhs._fVal;
    }

    friend BoundedDouble operator!(const BoundedDouble& lhs)
    {
        return !lhs._fVal;
    }

    friend BoundedDouble operator>>(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs.geq(0) ? 0 : -1;
        else
            return (s32_t)lhs.getNumeral() >> (s32_t)rhs.getNumeral();
    }

    friend BoundedDouble operator<<(const BoundedDouble& lhs,
                                    const BoundedDouble& rhs)
    {
        assert(rhs.geq(0) && "rhs should be greater or equal than 0");
        if (lhs.is_zero())
            return lhs;
        else if (lhs.is_infinity())
            return lhs;
        else if (rhs.is_infinity())
            return lhs.geq(0) ? plus_infinity() : minus_infinity();
        else
            return (s32_t)lhs.getNumeral() << (s32_t)rhs.getNumeral();
    }

    friend BoundedDouble ite(const BoundedDouble& cond,
                             const BoundedDouble& lhs, const BoundedDouble& rhs)
    {
        return cond._fVal != 0.0f ? lhs._fVal : rhs._fVal;
    }

    friend std::ostream& operator<<(std::ostream& out,
                                    const BoundedDouble& expr)
    {
        out << expr._fVal;
        return out;
    }

    friend bool eq(const BoundedDouble& lhs, const BoundedDouble& rhs)
    {
        return doubleEqual(lhs._fVal, rhs._fVal);
    }

    friend BoundedDouble min(const BoundedDouble& lhs, const BoundedDouble& rhs)
    {
        return std::min(lhs._fVal, rhs._fVal);
    }

    friend BoundedDouble max(const BoundedDouble& lhs, const BoundedDouble& rhs)
    {
        return std::max(lhs._fVal, rhs._fVal);
    }

    static BoundedDouble min(std::vector<BoundedDouble>& _l)
    {
        BoundedDouble ret(plus_infinity());
        for (const auto& it : _l)
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

    static BoundedDouble max(std::vector<BoundedDouble>& _l)
    {
        BoundedDouble ret(minus_infinity());
        for (const auto& it : _l)
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

    friend BoundedDouble abs(const BoundedDouble& lhs)
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

    inline s64_t getIntNumeral() const
    {
        return getNumeral();
    }

    inline double getRealNumeral() const
    {
        return _fVal;
    }

    inline virtual const std::string to_string() const
    {
        return std::to_string(_fVal);
    }

    //%}
}; // end class BoundedDouble

} // end namespace SVF

#endif // SVF_NUMERICVALUE_H
