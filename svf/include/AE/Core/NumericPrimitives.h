//===- NumericPrimitives.h -- Exact abstract-domain numbers ---*- C++ -*-===//

#ifndef SVF_AE_NUMERIC_PRIMITIVES_H
#define SVF_AE_NUMERIC_PRIMITIVES_H

#include <gmpxx.h>
#include <mpfr.h>

#include <cstdint>
#include <string>

namespace SVF::AbstractDomain
{

class Integer
{
public:
    Integer();
    explicit Integer(std::int64_t value);
    explicit Integer(const std::string& value);

    const mpz_class& value() const { return value_; }
    std::string toString() const;

    friend bool operator==(const Integer& lhs, const Integer& rhs)
    {
        return lhs.value_ == rhs.value_;
    }

private:
    mpz_class value_;
};

class Rational
{
public:
    Rational();
    explicit Rational(std::int64_t value);
    explicit Rational(const Integer& value);
    explicit Rational(const std::string& value);
    Rational(const Integer& numerator, const Integer& denominator);

    static Rational fromRaw(const mpq_class& value);

    const mpq_class& value() const { return value_; }
    bool isZero() const { return value_ == 0; }
    int sign() const { return mpq_sgn(value_.get_mpq_t()); }
    std::string toString() const;

    Rational floor() const;
    Rational ceil() const;
    Rational dividedByPowerOfTwo(unsigned exponent) const;
    Rational& assignSum(const Rational& lhs, const Rational& rhs);
    Rational& divideByPowerOfTwoInPlace(unsigned exponent);

    Rational& operator+=(const Rational& rhs);
    Rational& operator-=(const Rational& rhs);
    Rational& operator*=(const Rational& rhs);
    Rational& operator/=(const Rational& rhs);

    friend Rational operator+(Rational lhs, const Rational& rhs)
    {
        return lhs += rhs;
    }
    friend Rational operator-(Rational lhs, const Rational& rhs)
    {
        return lhs -= rhs;
    }
    friend Rational operator*(Rational lhs, const Rational& rhs)
    {
        return lhs *= rhs;
    }
    friend Rational operator/(Rational lhs, const Rational& rhs)
    {
        return lhs /= rhs;
    }
    friend Rational operator-(const Rational& value)
    {
        return Rational::fromRaw(-value.value_);
    }

    friend bool operator==(const Rational& lhs, const Rational& rhs)
    {
        return lhs.value_ == rhs.value_;
    }
    friend bool operator!=(const Rational& lhs, const Rational& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(const Rational& lhs, const Rational& rhs)
    {
        return lhs.value_ < rhs.value_;
    }
    friend bool operator<=(const Rational& lhs, const Rational& rhs)
    {
        return lhs.value_ <= rhs.value_;
    }
    friend bool operator>(const Rational& lhs, const Rational& rhs)
    {
        return rhs < lhs;
    }
    friend bool operator>=(const Rational& lhs, const Rational& rhs)
    {
        return rhs <= lhs;
    }

private:
    explicit Rational(mpq_class value, int);
    mpq_class value_;
};

/// An ordered extended-rational endpoint.  For a finite upper bound, strict
/// means "< value" and non-strict means "<= value".  At an equal numeric
/// value a strict bound is tighter than a non-strict one.
class Bound
{
public:
    enum class Kind
    {
        MinusInfinity,
        Finite,
        PlusInfinity
    };

    Bound();
    static Bound minusInfinity();
    static Bound finite(Rational value, bool strict = false);
    static Bound plusInfinity();

    Kind kind() const { return kind_; }
    bool isFinite() const { return kind_ == Kind::Finite; }
    bool isMinusInfinity() const { return kind_ == Kind::MinusInfinity; }
    bool isPlusInfinity() const { return kind_ == Kind::PlusInfinity; }
    const Rational& value() const;
    bool isStrict() const { return strict_; }

    /// Ordering used by upper bounds: tighter/smaller first.
    static int compare(const Bound& lhs, const Bound& rhs);
    static Bound min(const Bound& lhs, const Bound& rhs);
    static Bound max(const Bound& lhs, const Bound& rhs);
    static Bound add(const Bound& lhs, const Bound& rhs);
    Bound& assignSum(const Bound& lhs, const Bound& rhs);
    Bound& divideByTwoInPlace();
    static Bound divideByTwo(const Bound& bound);
    static Bound divideByPositive(const Bound& bound,
                                  const Rational& divisor);

    std::string toString() const;

    friend bool operator==(const Bound& lhs, const Bound& rhs)
    {
        return compare(lhs, rhs) == 0;
    }
    friend bool operator!=(const Bound& lhs, const Bound& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator<(const Bound& lhs, const Bound& rhs)
    {
        return compare(lhs, rhs) < 0;
    }
    friend bool operator<=(const Bound& lhs, const Bound& rhs)
    {
        return compare(lhs, rhs) <= 0;
    }

private:
    Bound(Kind kind, Rational value, bool strict);

    Kind kind_ = Kind::PlusInfinity;
    Rational value_;
    bool strict_ = false;
};

class Interval
{
public:
    Interval();
    Interval(Bound lower, Bound upper);

    static Interval top();
    static Interval singleton(const Rational& value);

    const Bound& lower() const { return lower_; }
    const Bound& upper() const { return upper_; }
    bool isTop() const;
    bool isBottom() const;
    std::string toString() const;

    friend bool operator==(const Interval& lhs, const Interval& rhs)
    {
        return lhs.lower_ == rhs.lower_ && lhs.upper_ == rhs.upper_;
    }
    friend bool operator!=(const Interval& lhs, const Interval& rhs)
    {
        return !(lhs == rhs);
    }

private:
    Bound lower_;
    Bound upper_;
};

enum class RoundingMode
{
    NearestTiesToEven,
    TowardZero,
    TowardPositive,
    TowardNegative
};

struct FloatFormat
{
    unsigned exponentBits = 0;
    unsigned significandBits = 0;

    static FloatFormat binary32() { return {8, 24}; }
    static FloatFormat binary64() { return {11, 53}; }
};

class MpfrValue
{
public:
    explicit MpfrValue(mpfr_prec_t precision);
    MpfrValue(const MpfrValue& rhs);
    MpfrValue(MpfrValue&& rhs) noexcept;
    MpfrValue& operator=(const MpfrValue& rhs);
    MpfrValue& operator=(MpfrValue&& rhs) noexcept;
    ~MpfrValue();

    mpfr_ptr raw() { return value_; }
    mpfr_srcptr raw() const { return value_; }
    mpfr_prec_t precision() const { return mpfr_get_prec(value_); }

    void set(const Rational& value, mpfr_rnd_t rounding);
    Rational toRational() const;

private:
    mpfr_t value_;
};

/// Ground MPFR operations used at the floating-semantics boundary.  The
/// returned rational is the exact dyadic value of the rounded MPFR result.
class FloatSemantics
{
public:
    static Rational add(const Rational& lhs, const Rational& rhs,
                        unsigned significandBits, RoundingMode rounding);
    static Rational subtract(const Rational& lhs, const Rational& rhs,
                             unsigned significandBits,
                             RoundingMode rounding);
    static Rational multiply(const Rational& lhs, const Rational& rhs,
                             unsigned significandBits,
                             RoundingMode rounding);
    static Rational divide(const Rational& lhs, const Rational& rhs,
                           unsigned significandBits, RoundingMode rounding);

private:
    enum class BinaryOperation
    {
        Add,
        Subtract,
        Multiply,
        Divide
    };

    static Rational evaluate(BinaryOperation operation, const Rational& lhs,
                             const Rational& rhs, unsigned significandBits,
                             RoundingMode rounding);
};

} // namespace SVF::AbstractDomain

#endif // SVF_AE_NUMERIC_PRIMITIVES_H
