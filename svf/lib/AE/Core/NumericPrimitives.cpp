//===- NumericPrimitives.cpp -- Exact abstract-domain numbers -----------===//

#include "AE/Core/NumericPrimitives.h"

#include <stdexcept>
#include <utility>

namespace SVF::AbstractDomain
{

Integer::Integer() : value_(0) {}

Integer::Integer(std::int64_t value)
{
    if (mpz_set_str(value_.get_mpz_t(), std::to_string(value).c_str(), 10) != 0)
        throw std::invalid_argument("invalid 64-bit integer");
}

Integer::Integer(const std::string& value) : value_(value) {}

std::string Integer::toString() const
{
    return value_.get_str();
}

Rational::Rational() : value_(0) {}

Rational::Rational(std::int64_t value)
{
    mpz_class integer;
    if (mpz_set_str(integer.get_mpz_t(), std::to_string(value).c_str(), 10) !=
        0)
        throw std::invalid_argument("invalid 64-bit rational integer");
    mpq_set_z(value_.get_mpq_t(), integer.get_mpz_t());
}

Rational::Rational(const Integer& value) : value_(value.value()) {}

Rational::Rational(const std::string& value) : value_(value)
{
    value_.canonicalize();
}

Rational::Rational(const Integer& numerator, const Integer& denominator)
{
    if (denominator.value() == 0)
        throw std::invalid_argument("a rational denominator cannot be zero");
    mpq_set_num(value_.get_mpq_t(), numerator.value().get_mpz_t());
    mpq_set_den(value_.get_mpq_t(), denominator.value().get_mpz_t());
    value_.canonicalize();
}

Rational::Rational(mpq_class value, int) : value_(std::move(value))
{
    value_.canonicalize();
}

Rational Rational::fromRaw(const mpq_class& value)
{
    return Rational(value, 0);
}

std::string Rational::toString() const
{
    return value_.get_str();
}

Rational Rational::floor() const
{
    mpz_class result;
    mpz_fdiv_q(result.get_mpz_t(), mpq_numref(value_.get_mpq_t()),
               mpq_denref(value_.get_mpq_t()));
    return Rational::fromRaw(mpq_class(result));
}

Rational Rational::ceil() const
{
    mpz_class result;
    mpz_cdiv_q(result.get_mpz_t(), mpq_numref(value_.get_mpq_t()),
               mpq_denref(value_.get_mpq_t()));
    return Rational::fromRaw(mpq_class(result));
}

Rational Rational::dividedByPowerOfTwo(unsigned exponent) const
{
    Rational result;
    mpq_div_2exp(result.value_.get_mpq_t(), value_.get_mpq_t(), exponent);
    return result;
}

Rational& Rational::assignSum(const Rational& lhs, const Rational& rhs)
{
    mpq_add(value_.get_mpq_t(), lhs.value_.get_mpq_t(),
            rhs.value_.get_mpq_t());
    return *this;
}

Rational& Rational::divideByPowerOfTwoInPlace(unsigned exponent)
{
    mpq_div_2exp(value_.get_mpq_t(), value_.get_mpq_t(), exponent);
    return *this;
}

Rational& Rational::operator+=(const Rational& rhs)
{
    value_ += rhs.value_;
    return *this;
}

Rational& Rational::operator-=(const Rational& rhs)
{
    value_ -= rhs.value_;
    return *this;
}

Rational& Rational::operator*=(const Rational& rhs)
{
    value_ *= rhs.value_;
    return *this;
}

Rational& Rational::operator/=(const Rational& rhs)
{
    if (rhs.isZero())
        throw std::domain_error("division by zero rational");
    value_ /= rhs.value_;
    return *this;
}

Bound::Bound() = default;

Bound::Bound(Kind kind, Rational value, bool strict)
    : kind_(kind), value_(std::move(value)),
      strict_(kind == Kind::Finite && strict)
{
}

Bound Bound::minusInfinity()
{
    return Bound(Kind::MinusInfinity, Rational(), false);
}

Bound Bound::finite(Rational value, bool strict)
{
    return Bound(Kind::Finite, std::move(value), strict);
}

Bound Bound::plusInfinity()
{
    return Bound(Kind::PlusInfinity, Rational(), false);
}

const Rational& Bound::value() const
{
    if (!isFinite())
        throw std::logic_error("an infinite bound has no finite value");
    return value_;
}

int Bound::compare(const Bound& lhs, const Bound& rhs)
{
    if (lhs.kind_ != rhs.kind_)
        return static_cast<int>(lhs.kind_) < static_cast<int>(rhs.kind_) ? -1
                                                                         : 1;
    if (!lhs.isFinite())
        return 0;
    if (lhs.value_ < rhs.value_)
        return -1;
    if (rhs.value_ < lhs.value_)
        return 1;
    if (lhs.strict_ == rhs.strict_)
        return 0;
    return lhs.strict_ ? -1 : 1;
}

Bound Bound::min(const Bound& lhs, const Bound& rhs)
{
    return lhs <= rhs ? lhs : rhs;
}

Bound Bound::max(const Bound& lhs, const Bound& rhs)
{
    return lhs <= rhs ? rhs : lhs;
}

Bound Bound::add(const Bound& lhs, const Bound& rhs)
{
    if ((lhs.isMinusInfinity() && rhs.isPlusInfinity()) ||
            (lhs.isPlusInfinity() && rhs.isMinusInfinity()))
        throw std::domain_error("indeterminate sum of opposite infinities");
    if (lhs.isMinusInfinity() || rhs.isMinusInfinity())
        return minusInfinity();
    if (lhs.isPlusInfinity() || rhs.isPlusInfinity())
        return plusInfinity();
    return finite(lhs.value_ + rhs.value_, lhs.strict_ || rhs.strict_);
}

Bound& Bound::assignSum(const Bound& lhs, const Bound& rhs)
{
    if ((lhs.isMinusInfinity() && rhs.isPlusInfinity()) ||
            (lhs.isPlusInfinity() && rhs.isMinusInfinity()))
        throw std::domain_error("indeterminate sum of opposite infinities");
    if (lhs.isMinusInfinity() || rhs.isMinusInfinity())
    {
        kind_ = Kind::MinusInfinity;
        strict_ = false;
        return *this;
    }
    if (lhs.isPlusInfinity() || rhs.isPlusInfinity())
    {
        kind_ = Kind::PlusInfinity;
        strict_ = false;
        return *this;
    }
    kind_ = Kind::Finite;
    value_.assignSum(lhs.value_, rhs.value_);
    strict_ = lhs.strict_ || rhs.strict_;
    return *this;
}

Bound& Bound::divideByTwoInPlace()
{
    if (isFinite())
        value_.divideByPowerOfTwoInPlace(1);
    return *this;
}

Bound Bound::divideByTwo(const Bound& bound)
{
    if (!bound.isFinite())
        return bound;
    return finite(bound.value_.dividedByPowerOfTwo(1), bound.strict_);
}

Bound Bound::divideByPositive(const Bound& bound, const Rational& divisor)
{
    if (divisor.sign() <= 0)
        throw std::invalid_argument("bound divisor must be positive");
    if (!bound.isFinite())
        return bound;
    return finite(bound.value_ / divisor, bound.strict_);
}

std::string Bound::toString() const
{
    if (isMinusInfinity())
        return "-inf";
    if (isPlusInfinity())
        return "+inf";
    return std::string(strict_ ? "<" : "<=") + value_.toString();
}

Interval::Interval() : lower_(Bound::minusInfinity()),
    upper_(Bound::plusInfinity())
{
}

Interval::Interval(Bound lower, Bound upper)
    : lower_(std::move(lower)), upper_(std::move(upper))
{
}

Interval Interval::top()
{
    return Interval();
}

Interval Interval::singleton(const Rational& value)
{
    return Interval(Bound::finite(value), Bound::finite(value));
}

bool Interval::isTop() const
{
    return lower_.isMinusInfinity() && upper_.isPlusInfinity();
}

bool Interval::isBottom() const
{
    if (lower_.isPlusInfinity() || upper_.isMinusInfinity())
        return true;
    if (!lower_.isFinite() || !upper_.isFinite())
        return false;
    if (upper_.value() < lower_.value())
        return true;
    return upper_.value() == lower_.value() &&
           (lower_.isStrict() || upper_.isStrict());
}

std::string Interval::toString() const
{
    const char left = lower_.isStrict() ? '(' : '[';
    const char right = upper_.isStrict() ? ')' : ']';
    const std::string lower = lower_.isMinusInfinity()
                                  ? "-inf"
                                  : lower_.isPlusInfinity()
                                        ? "+inf"
                                        : lower_.value().toString();
    const std::string upper = upper_.isPlusInfinity()
                                  ? "+inf"
                                  : upper_.isMinusInfinity()
                                        ? "-inf"
                                        : upper_.value().toString();
    return std::string(1, left) + lower + ", " + upper +
           std::string(1, right);
}

MpfrValue::MpfrValue(mpfr_prec_t precision)
{
    if (precision < MPFR_PREC_MIN || precision > MPFR_PREC_MAX)
        throw std::invalid_argument("invalid MPFR precision");
    mpfr_init2(value_, precision);
    mpfr_set_zero(value_, 1);
}

MpfrValue::MpfrValue(const MpfrValue& rhs)
{
    mpfr_init2(value_, rhs.precision());
    mpfr_set(value_, rhs.value_, MPFR_RNDN);
}

MpfrValue::MpfrValue(MpfrValue&& rhs) noexcept
{
    mpfr_init2(value_, rhs.precision());
    mpfr_swap(value_, rhs.value_);
}

MpfrValue& MpfrValue::operator=(const MpfrValue& rhs)
{
    if (this == &rhs)
        return *this;
    mpfr_set_prec(value_, rhs.precision());
    mpfr_set(value_, rhs.value_, MPFR_RNDN);
    return *this;
}

MpfrValue& MpfrValue::operator=(MpfrValue&& rhs) noexcept
{
    if (this != &rhs)
        mpfr_swap(value_, rhs.value_);
    return *this;
}

MpfrValue::~MpfrValue()
{
    mpfr_clear(value_);
}

void MpfrValue::set(const Rational& value, mpfr_rnd_t rounding)
{
    mpfr_set_q(value_, value.value().get_mpq_t(), rounding);
}

Rational MpfrValue::toRational() const
{
    if (!mpfr_number_p(value_))
        throw std::domain_error("a non-finite MPFR value is not rational");
    mpq_class result;
    mpfr_get_q(result.get_mpq_t(), value_);
    result.canonicalize();
    return Rational::fromRaw(result);
}

namespace
{
mpfr_rnd_t toMpfrRounding(RoundingMode mode)
{
    switch (mode)
    {
    case RoundingMode::NearestTiesToEven:
        return MPFR_RNDN;
    case RoundingMode::TowardZero:
        return MPFR_RNDZ;
    case RoundingMode::TowardPositive:
        return MPFR_RNDU;
    case RoundingMode::TowardNegative:
        return MPFR_RNDD;
    }
    return MPFR_RNDN;
}
} // namespace

Rational FloatSemantics::evaluate(BinaryOperation operation,
                                  const Rational& lhs, const Rational& rhs,
                                  unsigned significandBits,
                                  RoundingMode rounding)
{
    if (significandBits < static_cast<unsigned>(MPFR_PREC_MIN))
        throw std::invalid_argument("invalid floating significand precision");

    const mpfr_rnd_t mode = toMpfrRounding(rounding);
    MpfrValue left(significandBits);
    MpfrValue right(significandBits);
    MpfrValue result(significandBits);
    left.set(lhs, mode);
    right.set(rhs, mode);

    switch (operation)
    {
    case BinaryOperation::Add:
        mpfr_add(result.raw(), left.raw(), right.raw(), mode);
        break;
    case BinaryOperation::Subtract:
        mpfr_sub(result.raw(), left.raw(), right.raw(), mode);
        break;
    case BinaryOperation::Multiply:
        mpfr_mul(result.raw(), left.raw(), right.raw(), mode);
        break;
    case BinaryOperation::Divide:
        mpfr_div(result.raw(), left.raw(), right.raw(), mode);
        break;
    }
    return result.toRational();
}

Rational FloatSemantics::add(const Rational& lhs, const Rational& rhs,
                             unsigned significandBits, RoundingMode rounding)
{
    return evaluate(BinaryOperation::Add, lhs, rhs, significandBits, rounding);
}

Rational FloatSemantics::subtract(const Rational& lhs, const Rational& rhs,
                                  unsigned significandBits,
                                  RoundingMode rounding)
{
    return evaluate(BinaryOperation::Subtract, lhs, rhs, significandBits,
                    rounding);
}

Rational FloatSemantics::multiply(const Rational& lhs, const Rational& rhs,
                                  unsigned significandBits,
                                  RoundingMode rounding)
{
    return evaluate(BinaryOperation::Multiply, lhs, rhs, significandBits,
                    rounding);
}

Rational FloatSemantics::divide(const Rational& lhs, const Rational& rhs,
                                unsigned significandBits,
                                RoundingMode rounding)
{
    return evaluate(BinaryOperation::Divide, lhs, rhs, significandBits,
                    rounding);
}

} // namespace SVF::AbstractDomain
