//===- NumericalDomain.cpp -- Shared numerical-state implementation ----===//

#include "AE/Core/NumericalDomain.h"

#include "AE/Core/BoxDomain.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SVF::AbstractDomain
{

namespace
{

constexpr std::array<std::uint8_t, 8> RawMagic{'S', 'V', 'F', 'A',
                                               'D', 'R', 'A', 'W'};
constexpr std::uint16_t RawVersion = 1;
constexpr std::uint32_t MaxCollectionEntries = 1U << 20;
constexpr std::uint64_t FnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t FnvPrime = 1099511628211ULL;

enum class DomainTag : std::uint8_t
{
    Box = 1
};

std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size)
{
    std::uint64_t result = FnvOffset;
    for (std::size_t index = 0; index < size; ++index)
    {
        result ^= data[index];
        result *= FnvPrime;
    }
    return result;
}

class Writer
{
public:
    void writeByte(std::uint8_t value)
    {
        bytes_.push_back(value);
    }

    void writeU16(std::uint16_t value)
    {
        for (unsigned shift = 0; shift < 16; shift += 8)
            writeByte(static_cast<std::uint8_t>(value >> shift));
    }

    void writeU32(std::uint32_t value)
    {
        for (unsigned shift = 0; shift < 32; shift += 8)
            writeByte(static_cast<std::uint8_t>(value >> shift));
    }

    void writeU64(std::uint64_t value)
    {
        for (unsigned shift = 0; shift < 64; shift += 8)
            writeByte(static_cast<std::uint8_t>(value >> shift));
    }

    void writeString(const std::string& value)
    {
        if (value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("raw state string is too large");
        writeU32(static_cast<std::uint32_t>(value.size()));
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    void writeMagic()
    {
        bytes_.insert(bytes_.end(), RawMagic.begin(), RawMagic.end());
    }

    NumericalState::RawBuffer finish()
    {
        const std::uint64_t checksum = fnv1a(bytes_.data(), bytes_.size());
        writeU64(checksum);
        return std::move(bytes_);
    }

private:
    NumericalState::RawBuffer bytes_;
};

std::uint64_t readTrailingU64(const NumericalState::RawBuffer& buffer)
{
    if (buffer.size() < sizeof(std::uint64_t))
        throw std::invalid_argument("raw state buffer is truncated");
    std::uint64_t value = 0;
    const std::size_t offset = buffer.size() - sizeof(std::uint64_t);
    for (unsigned index = 0; index < sizeof(std::uint64_t); ++index)
        value |= static_cast<std::uint64_t>(buffer[offset + index])
                 << (8 * index);
    return value;
}

class Reader
{
public:
    explicit Reader(const NumericalState::RawBuffer& bytes)
        : bytes_(bytes), limit_(checkedLimit(bytes))
    {
        const std::uint64_t expected = readTrailingU64(bytes_);
        const std::uint64_t actual = fnv1a(bytes_.data(), limit_);
        if (actual != expected)
            throw std::invalid_argument("raw state checksum mismatch");
    }

    void readMagic()
    {
        for (std::uint8_t expected : RawMagic)
        {
            if (readByte() != expected)
                throw std::invalid_argument("raw state has invalid magic");
        }
    }

    std::uint8_t readByte()
    {
        require(1);
        return bytes_[position_++];
    }

    std::uint16_t readU16()
    {
        std::uint16_t value = 0;
        for (unsigned index = 0; index < sizeof(value); ++index)
            value |= static_cast<std::uint16_t>(readByte()) << (8 * index);
        return value;
    }

    std::uint32_t readU32()
    {
        std::uint32_t value = 0;
        for (unsigned index = 0; index < sizeof(value); ++index)
            value |= static_cast<std::uint32_t>(readByte()) << (8 * index);
        return value;
    }

    std::string readString()
    {
        const std::uint32_t size = readU32();
        require(size);
        const auto begin =
            bytes_.begin() + static_cast<std::ptrdiff_t>(position_);
        position_ += size;
        return std::string(begin, begin + size);
    }

    bool empty() const
    {
        return position_ == limit_;
    }

private:
    static std::size_t checkedLimit(const NumericalState::RawBuffer& bytes)
    {
        if (bytes.size() <
            RawMagic.size() + sizeof(std::uint16_t) + 2 + sizeof(std::uint64_t))
            throw std::invalid_argument("raw state buffer is truncated");
        return bytes.size() - sizeof(std::uint64_t);
    }

    void require(std::size_t size) const
    {
        if (size > limit_ - position_)
            throw std::invalid_argument("raw state buffer is truncated");
    }

    const NumericalState::RawBuffer& bytes_;
    std::size_t limit_;
    std::size_t position_ = 0;
};

std::uint8_t encodeKind(NumericKind kind)
{
    switch (kind)
    {
    case NumericKind::Integer:
        return 0;
    case NumericKind::Real:
        return 1;
    case NumericKind::IEEEFloat:
        return 2;
    }
    throw std::logic_error("unknown numerical kind");
}

NumericKind decodeKind(std::uint8_t value)
{
    switch (value)
    {
    case 0:
        return NumericKind::Integer;
    case 1:
        return NumericKind::Real;
    case 2:
        return NumericKind::IEEEFloat;
    default:
        throw std::invalid_argument("raw state has invalid numerical kind");
    }
}

std::uint8_t encodeConstraintKind(ConstraintKind kind)
{
    switch (kind)
    {
    case ConstraintKind::Equal:
        return 0;
    case ConstraintKind::NotEqual:
        return 1;
    case ConstraintKind::LessThan:
        return 2;
    case ConstraintKind::LessEqual:
        return 3;
    case ConstraintKind::GreaterThan:
        return 4;
    case ConstraintKind::GreaterEqual:
        return 5;
    }
    throw std::logic_error("unknown linear constraint kind");
}

ConstraintKind decodeConstraintKind(std::uint8_t value)
{
    switch (value)
    {
    case 0:
        return ConstraintKind::Equal;
    case 1:
        return ConstraintKind::NotEqual;
    case 2:
        return ConstraintKind::LessThan;
    case 3:
        return ConstraintKind::LessEqual;
    case 4:
        return ConstraintKind::GreaterThan;
    case 5:
        return ConstraintKind::GreaterEqual;
    default:
        throw std::invalid_argument("raw state has invalid constraint kind");
    }
}

DomainTag domainTag(const NumericalState& state)
{
    if (state.isState<BoxState>())
        return DomainTag::Box;
    throw std::invalid_argument(
        "raw serialization does not support this domain");
}

std::uint8_t configurationFlags(const NumericalState& state, DomainTag tag)
{
    switch (tag)
    {
    case DomainTag::Box: {
        const auto& box = static_cast<const BoxState&>(state);
        return box.config().integerTightening ? 1U : 0U;
    }
    }
    throw std::logic_error("unknown raw state domain tag");
}

void writeEnvironment(Writer& writer, const VariableEnvironment& environment)
{
    if (environment.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("raw state environment is too large");
    writer.writeU32(static_cast<std::uint32_t>(environment.size()));
    for (const VariableDeclaration& declaration : environment.variables())
    {
        writer.writeU32(declaration.variable.id());
        writer.writeByte(encodeKind(declaration.type.kind));
        writer.writeU32(declaration.type.floatFormat.exponentBits);
        writer.writeU32(declaration.type.floatFormat.significandBits);
        writer.writeString(declaration.name);
    }
}

VariableEnvironment readEnvironment(Reader& reader)
{
    const std::uint32_t count = reader.readU32();
    if (count > MaxCollectionEntries)
        throw std::invalid_argument("raw state environment is too large");
    std::vector<VariableDeclaration> declarations;
    declarations.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const Variable variable(reader.readU32());
        NumericType type;
        type.kind = decodeKind(reader.readByte());
        type.floatFormat.exponentBits = reader.readU32();
        type.floatFormat.significandBits = reader.readU32();
        declarations.push_back({variable, type, reader.readString()});
    }
    return VariableEnvironment(std::move(declarations));
}

void writeConstraints(Writer& writer, const LinearConstraintSet& constraints)
{
    if (constraints.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("raw state has too many constraints");
    writer.writeU32(static_cast<std::uint32_t>(constraints.size()));
    for (const LinearConstraint& constraint : constraints)
    {
        writer.writeByte(encodeConstraintKind(constraint.kind()));
        writer.writeString(constraint.expression().constant().toString());
        const auto& terms = constraint.expression().terms();
        if (terms.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("raw state constraint has too many terms");
        writer.writeU32(static_cast<std::uint32_t>(terms.size()));
        for (const auto& [variable, coefficient] : terms)
        {
            writer.writeU32(variable.id());
            writer.writeString(coefficient.toString());
        }
    }
}

LinearConstraintSet canonicalConstraints(const NumericalState& state, DomainTag)
{
    return state.isBottom() ? LinearConstraintSet{} : state.toConstraints();
}

Rational readRational(Reader& reader)
{
    const std::string encoded = reader.readString();
    if (encoded.empty())
        throw std::invalid_argument("raw state contains an empty rational");
    try
    {
        return Rational(encoded);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument("raw state contains an invalid rational");
    }
}

LinearConstraintSet readConstraints(Reader& reader,
                                    const VariableEnvironment& environment)
{
    const std::uint32_t count = reader.readU32();
    if (count > MaxCollectionEntries)
        throw std::invalid_argument("raw state has too many constraints");
    LinearConstraintSet constraints;
    constraints.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const ConstraintKind kind = decodeConstraintKind(reader.readByte());
        LinearExpression expression(readRational(reader));
        const std::uint32_t termCount = reader.readU32();
        if (termCount > environment.size())
            throw std::invalid_argument(
                "raw state constraint has too many terms");
        std::set<Variable> seen;
        for (std::uint32_t term = 0; term < termCount; ++term)
        {
            const Variable variable(reader.readU32());
            if (!environment.contains(variable))
                throw std::invalid_argument(
                    "raw state constraint uses an unknown variable");
            if (!seen.insert(variable).second)
                throw std::invalid_argument(
                    "raw state constraint repeats a variable");
            expression.setCoefficient(variable, readRational(reader));
        }
        constraints.emplace_back(std::move(expression), kind);
    }
    return constraints;
}

DomainTag decodeDomainTag(std::uint8_t value)
{
    switch (value)
    {
    case static_cast<std::uint8_t>(DomainTag::Box):
        return DomainTag::Box;
    default:
        throw std::invalid_argument("raw state has an unknown domain tag");
    }
}

std::unique_ptr<NumericalState> restore(DomainTag tag, std::uint8_t flags,
                                        const VariableEnvironment& environment,
                                        bool bottom,
                                        const LinearConstraintSet& constraints)
{
    switch (tag)
    {
    case DomainTag::Box: {
        if ((flags & ~1U) != 0)
            throw std::invalid_argument("raw Box state has invalid flags");
        BoxConfig config;
        config.integerTightening = (flags & 1U) != 0;
        BoxState state =
            bottom
                ? BoxState::bottom(environment, config)
                : BoxState::fromConstraints(environment, constraints, config);
        return std::make_unique<BoxState>(std::move(state));
    }
    }
    throw std::logic_error("unknown raw state domain tag");
}

Interval bottomInterval()
{
    return Interval(Bound::plusInfinity(), Bound::minusInfinity());
}

bool singletonZero(const Interval& value)
{
    return value.lower().isFinite() && value.upper().isFinite() &&
           value.lower().value().isZero() && value.upper().value().isZero() &&
           !value.lower().isStrict() && !value.upper().isStrict();
}

std::optional<Rational> singletonValue(const Interval& value)
{
    if (!value.lower().isFinite() || !value.upper().isFinite() ||
        value.lower().isStrict() || value.upper().isStrict() ||
        value.lower().value() != value.upper().value())
        return std::nullopt;
    return value.lower().value();
}

Interval negateInterval(const Interval& value)
{
    if (value.isBottom())
        return bottomInterval();
    const Bound lower =
        value.upper().isFinite()
            ? Bound::finite(-value.upper().value(), value.upper().isStrict())
        : value.upper().isPlusInfinity() ? Bound::minusInfinity()
                                         : Bound::plusInfinity();
    const Bound upper =
        value.lower().isFinite()
            ? Bound::finite(-value.lower().value(), value.lower().isStrict())
        : value.lower().isMinusInfinity() ? Bound::plusInfinity()
                                          : Bound::minusInfinity();
    return Interval(lower, upper);
}

Interval addIntervals(const Interval& lhs, const Interval& rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
        return bottomInterval();
    Bound lower = Bound::minusInfinity();
    Bound upper = Bound::plusInfinity();
    if (lhs.lower().isFinite() && rhs.lower().isFinite())
        lower = Bound::finite(lhs.lower().value() + rhs.lower().value(),
                              lhs.lower().isStrict() || rhs.lower().isStrict());
    if (lhs.upper().isFinite() && rhs.upper().isFinite())
        upper = Bound::finite(lhs.upper().value() + rhs.upper().value(),
                              lhs.upper().isStrict() || rhs.upper().isStrict());
    return Interval(lower, upper);
}

struct ExtendedRational
{
    /// -1 is minus infinity, 0 is finite, and 1 is plus infinity.
    int infinity = 0;
    Rational value;
};

ExtendedRational extended(const Bound& bound)
{
    if (bound.isMinusInfinity())
        return {-1, Rational()};
    if (bound.isPlusInfinity())
        return {1, Rational()};
    return {0, bound.value()};
}

int compareExtended(const ExtendedRational& lhs, const ExtendedRational& rhs)
{
    if (lhs.infinity != rhs.infinity)
        return lhs.infinity < rhs.infinity ? -1 : 1;
    if (lhs.infinity != 0)
        return 0;
    if (lhs.value < rhs.value)
        return -1;
    if (rhs.value < lhs.value)
        return 1;
    return 0;
}

ExtendedRational multiplyExtended(const ExtendedRational& lhs,
                                  const ExtendedRational& rhs)
{
    if (lhs.infinity == 0 && rhs.infinity == 0)
        return {0, lhs.value * rhs.value};
    if ((lhs.infinity == 0 && lhs.value.isZero()) ||
        (rhs.infinity == 0 && rhs.value.isZero()))
        return {0, Rational()};
    const int lhsSign = lhs.infinity != 0 ? lhs.infinity : lhs.value.sign();
    const int rhsSign = rhs.infinity != 0 ? rhs.infinity : rhs.value.sign();
    return {lhsSign * rhsSign, Rational()};
}

Bound extendedBound(const ExtendedRational& value)
{
    if (value.infinity < 0)
        return Bound::minusInfinity();
    if (value.infinity > 0)
        return Bound::plusInfinity();
    return Bound::finite(value.value);
}

Interval multiplyIntervals(const Interval& lhs, const Interval& rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
        return bottomInterval();
    if (singletonZero(lhs) || singletonZero(rhs))
        return Interval::singleton(Rational());
    const ExtendedRational lhsLower = extended(lhs.lower());
    const ExtendedRational lhsUpper = extended(lhs.upper());
    const ExtendedRational rhsLower = extended(rhs.lower());
    const ExtendedRational rhsUpper = extended(rhs.upper());
    const std::array<ExtendedRational, 4> products{
        multiplyExtended(lhsLower, rhsLower),
        multiplyExtended(lhsLower, rhsUpper),
        multiplyExtended(lhsUpper, rhsLower),
        multiplyExtended(lhsUpper, rhsUpper)};
    ExtendedRational lower = products.front();
    ExtendedRational upper = products.front();
    for (const ExtendedRational& product : products)
    {
        if (compareExtended(product, lower) < 0)
            lower = product;
        if (compareExtended(upper, product) < 0)
            upper = product;
    }
    return Interval(extendedBound(lower), extendedBound(upper));
}

bool containsZero(const Interval& value)
{
    if (value.isBottom())
        return false;
    const bool aboveLower =
        value.lower().isMinusInfinity() ||
        (value.lower().isFinite() &&
         (value.lower().value() < Rational() ||
          (value.lower().value().isZero() && !value.lower().isStrict())));
    const bool belowUpper =
        value.upper().isPlusInfinity() ||
        (value.upper().isFinite() &&
         (Rational() < value.upper().value() ||
          (value.upper().value().isZero() && !value.upper().isStrict())));
    return aboveLower && belowUpper;
}

Rational truncateTowardZero(const Rational& value)
{
    return value.sign() < 0 ? value.ceil() : value.floor();
}

Interval divideIntervals(const Interval& lhs, const Interval& rhs,
                         bool integerDivision)
{
    if (lhs.isBottom() || rhs.isBottom())
        return bottomInterval();
    if (containsZero(rhs))
        return Interval::top();

    Bound reciprocalLower;
    Bound reciprocalUpper;
    const bool positive =
        rhs.lower().isFinite() && rhs.lower().value().sign() >= 0;
    if (positive)
    {
        reciprocalLower =
            rhs.upper().isPlusInfinity()
                ? Bound::finite(Rational())
                : Bound::finite(Rational(1) / rhs.upper().value());
        reciprocalUpper =
            rhs.lower().value().isZero()
                ? Bound::plusInfinity()
                : Bound::finite(Rational(1) / rhs.lower().value());
    }
    else
    {
        reciprocalLower =
            rhs.upper().isFinite() && rhs.upper().value().isZero()
                ? Bound::minusInfinity()
                : Bound::finite(Rational(1) / rhs.upper().value());
        reciprocalUpper =
            rhs.lower().isMinusInfinity()
                ? Bound::finite(Rational())
                : Bound::finite(Rational(1) / rhs.lower().value());
    }
    Interval result =
        multiplyIntervals(lhs, Interval(reciprocalLower, reciprocalUpper));
    if (!integerDivision || result.isBottom())
        return result;
    const Bound lower =
        result.lower().isFinite()
            ? Bound::finite(truncateTowardZero(result.lower().value()))
            : result.lower();
    const Bound upper =
        result.upper().isFinite()
            ? Bound::finite(truncateTowardZero(result.upper().value()))
            : result.upper();
    return Interval(lower, upper);
}

Rational powerOfTwo(long exponent)
{
    mpq_class value(1);
    if (exponent >= 0)
        mpz_mul_2exp(value.get_num_mpz_t(), value.get_num_mpz_t(), exponent);
    else
        mpz_mul_2exp(value.get_den_mpz_t(), value.get_den_mpz_t(), -exponent);
    value.canonicalize();
    return Rational::fromRaw(value);
}

struct IEEEFormatBounds
{
    Rational maximum;
    Rational minimumNormal;
    Rational minimumSubnormal;
};

IEEEFormatBounds ieeeBounds(const FloatFormat& format)
{
    if (format.exponentBits < 2 || format.exponentBits >= 63 ||
        format.significandBits < 2)
        throw std::invalid_argument("invalid IEEE floating format");
    const std::uint64_t bias =
        (std::uint64_t(1) << (format.exponentBits - 1)) - 1;
    const Rational maximum =
        (Rational(2) -
         powerOfTwo(1 - static_cast<long>(format.significandBits))) *
        powerOfTwo(static_cast<long>(bias));
    const Rational minimumNormal = powerOfTwo(1 - static_cast<long>(bias));
    const long minimumExponent = 1 - static_cast<long>(bias) -
                                 static_cast<long>(format.significandBits - 1);
    return {maximum, minimumNormal, powerOfTwo(minimumExponent)};
}

Rational roundIntegral(const Rational& value, RoundingMode rounding)
{
    const Rational lower = value.floor();
    const Rational upper = value.ceil();
    switch (rounding)
    {
    case RoundingMode::TowardZero:
        return value.sign() < 0 ? upper : lower;
    case RoundingMode::TowardPositive:
        return upper;
    case RoundingMode::TowardNegative:
        return lower;
    case RoundingMode::NearestTiesToEven: {
        const Rational lowerDistance = value - lower;
        const Rational upperDistance = upper - value;
        if (lowerDistance < upperDistance)
            return lower;
        if (upperDistance < lowerDistance)
            return upper;
        return mpz_even_p(lower.value().get_num_mpz_t()) != 0 ? lower : upper;
    }
    }
    return value;
}

std::optional<Rational> roundedIEEE(const Rational& value,
                                    const FloatFormat& format,
                                    RoundingMode rounding)
{
    const IEEEFormatBounds bounds = ieeeBounds(format);
    if (bounds.maximum < value || value < -bounds.maximum)
        return std::nullopt;
    if (-bounds.minimumNormal < value && value < bounds.minimumNormal)
        return roundIntegral(value / bounds.minimumSubnormal, rounding) *
               bounds.minimumSubnormal;
    return FloatSemantics::add(value, Rational(), format.significandBits,
                               rounding);
}

Interval roundIEEEInterval(const Interval& value, const FloatFormat& format,
                           RoundingMode rounding)
{
    if (value.isBottom())
        return bottomInterval();
    if (!value.lower().isFinite() || !value.upper().isFinite())
        return Interval::top();
    const std::optional<Rational> lower =
        roundedIEEE(value.lower().value(), format, rounding);
    const std::optional<Rational> upper =
        roundedIEEE(value.upper().value(), format, rounding);
    if (!lower || !upper)
        return Interval::top();
    return Interval(Bound::finite(*lower), Bound::finite(*upper));
}

Interval remainderIntervals(const Interval& lhs, const Interval& rhs)
{
    if (lhs.isBottom() || rhs.isBottom())
        return bottomInterval();
    if (containsZero(rhs))
        return Interval::top();
    if (singletonZero(lhs))
        return Interval::singleton(Rational());
    const std::optional<Rational> lhsValue = singletonValue(lhs);
    const std::optional<Rational> rhsValue = singletonValue(rhs);
    if (lhsValue && rhsValue)
    {
        const Rational quotient = truncateTowardZero(*lhsValue / *rhsValue);
        return Interval::singleton(*lhsValue - quotient * *rhsValue);
    }
    std::optional<Rational> magnitude;
    if (rhs.lower().isFinite() && rhs.upper().isFinite())
    {
        magnitude = rhs.lower().value().sign() < 0 ? -rhs.lower().value()
                                                   : rhs.lower().value();
        const Rational upperMagnitude = rhs.upper().value().sign() < 0
                                            ? -rhs.upper().value()
                                            : rhs.upper().value();
        if (*magnitude < upperMagnitude)
            magnitude = upperMagnitude;
    }
    if (lhs.lower().isFinite() && lhs.upper().isFinite())
    {
        Rational lhsMagnitude = lhs.lower().value().sign() < 0
                                    ? -lhs.lower().value()
                                    : lhs.lower().value();
        const Rational upperMagnitude = lhs.upper().value().sign() < 0
                                            ? -lhs.upper().value()
                                            : lhs.upper().value();
        if (lhsMagnitude < upperMagnitude)
            lhsMagnitude = upperMagnitude;
        if (!magnitude || lhsMagnitude < *magnitude)
            magnitude = lhsMagnitude;
    }
    if (!magnitude)
        return Interval::top();
    if (magnitude->isZero())
        return Interval::top();
    Rational lower = -*magnitude;
    Rational upper = *magnitude;
    if (lhs.lower().isFinite() && lhs.lower().value().sign() >= 0)
        lower = Rational();
    if (lhs.upper().isFinite() && lhs.upper().value().sign() <= 0)
        upper = Rational();
    return Interval(Bound::finite(lower, true), Bound::finite(upper, true));
}

Interval squareRootInterval(const Interval& operand, const NumericType& type,
                            RoundingMode rounding)
{
    if (operand.isBottom())
        return bottomInterval();
    if (!operand.lower().isFinite() || operand.lower().value().sign() < 0)
        return Interval::top();
    const unsigned precision = type.kind == NumericKind::IEEEFloat
                                   ? type.floatFormat.significandBits
                                   : 256U;
    MpfrValue input(precision);
    MpfrValue output(precision);
    input.set(operand.lower().value(), MPFR_RNDD);
    mpfr_sqrt(output.raw(), input.raw(), MPFR_RNDD);
    const Rational lower = output.toRational();
    if (!operand.upper().isFinite())
        return Interval(Bound::finite(lower), Bound::plusInfinity());
    input.set(operand.upper().value(), MPFR_RNDU);
    mpfr_sqrt(output.raw(), input.raw(), MPFR_RNDU);
    Interval result(Bound::finite(lower), Bound::finite(output.toRational()));
    return type.kind == NumericKind::IEEEFloat
               ? roundIEEEInterval(result, type.floatFormat, rounding)
               : result;
}

Interval castInterval(const Interval& operand, const NumericType& type,
                      RoundingMode rounding)
{
    if (operand.isBottom())
        return bottomInterval();
    if (type.kind == NumericKind::Real)
        return operand;
    if (type.kind == NumericKind::IEEEFloat)
        return roundIEEEInterval(operand, type.floatFormat, rounding);
    if (!operand.lower().isFinite() || !operand.upper().isFinite())
        return Interval::top();
    Rational lower = truncateTowardZero(operand.lower().value());
    Rational upper = truncateTowardZero(operand.upper().value());
    if (upper < lower)
        std::swap(lower, upper);
    return Interval(Bound::finite(lower), Bound::finite(upper));
}

Interval evaluateTree(const NumericalState& state,
                      const TreeExpression& expression)
{
    switch (expression.kind())
    {
    case TreeExpression::Kind::Constant:
        return castInterval(Interval::singleton(expression.constant()),
                            expression.type(), expression.roundingMode());
    case TreeExpression::Kind::Variable:
        if (!state.environment().contains(expression.variable()))
            throw std::invalid_argument(
                "tree expression uses an unknown variable");
        if (state.environment().typeOf(expression.variable()) !=
            expression.type())
            throw std::invalid_argument(
                "tree variable type does not match environment");
        return state.bound(expression.variable());
    case TreeExpression::Kind::Unary: {
        const Interval operand = evaluateTree(state, expression.lhs());
        switch (expression.unaryOperator())
        {
        case UnaryOperator::Negate:
            return expression.type().kind == NumericKind::IEEEFloat
                       ? roundIEEEInterval(negateInterval(operand),
                                           expression.type().floatFormat,
                                           expression.roundingMode())
                       : negateInterval(operand);
        case UnaryOperator::Cast:
            return castInterval(operand, expression.type(),
                                expression.roundingMode());
        case UnaryOperator::SquareRoot:
            return squareRootInterval(operand, expression.type(),
                                      expression.roundingMode());
        }
    }
    case TreeExpression::Kind::Binary: {
        const Interval lhs = evaluateTree(state, expression.lhs());
        const Interval rhs = evaluateTree(state, expression.rhs());
        Interval result;
        switch (expression.binaryOperator())
        {
        case BinaryOperator::Add:
            result = addIntervals(lhs, rhs);
            break;
        case BinaryOperator::Subtract:
            result = addIntervals(lhs, negateInterval(rhs));
            break;
        case BinaryOperator::Multiply:
            result = multiplyIntervals(lhs, rhs);
            break;
        case BinaryOperator::Divide:
            result = divideIntervals(
                lhs, rhs, expression.type().kind == NumericKind::Integer);
            break;
        case BinaryOperator::Remainder:
            result = remainderIntervals(lhs, rhs);
            break;
        }
        return expression.type().kind == NumericKind::IEEEFloat
                   ? roundIEEEInterval(result, expression.type().floatFormat,
                                       expression.roundingMode())
                   : result;
    }
    }
    return Interval::top();
}

bool definitelyTrue(const Interval& value, ConstraintKind kind)
{
    if (value.isBottom())
        return true;
    switch (kind)
    {
    case ConstraintKind::Equal:
        return singletonZero(value);
    case ConstraintKind::NotEqual:
        return !containsZero(value);
    case ConstraintKind::LessEqual:
        return value.upper().isFinite() && value.upper().value() <= Rational();
    case ConstraintKind::LessThan:
        return value.upper().isFinite() &&
               (value.upper().value() < Rational() ||
                (value.upper().value().isZero() && value.upper().isStrict()));
    case ConstraintKind::GreaterEqual:
        return value.lower().isFinite() && Rational() <= value.lower().value();
    case ConstraintKind::GreaterThan:
        return value.lower().isFinite() &&
               (Rational() < value.lower().value() ||
                (value.lower().value().isZero() && value.lower().isStrict()));
    }
    return false;
}

bool definitelyFalse(const Interval& value, ConstraintKind kind)
{
    if (value.isBottom())
        return false;
    switch (kind)
    {
    case ConstraintKind::Equal:
        return !containsZero(value);
    case ConstraintKind::NotEqual:
        return singletonZero(value);
    case ConstraintKind::LessEqual:
        return value.lower().isFinite() &&
               (Rational() < value.lower().value() ||
                (value.lower().value().isZero() && value.lower().isStrict()));
    case ConstraintKind::LessThan:
        return value.lower().isFinite() && Rational() <= value.lower().value();
    case ConstraintKind::GreaterEqual:
        return value.upper().isFinite() &&
               (value.upper().value() < Rational() ||
                (value.upper().value().isZero() && value.upper().isStrict()));
    case ConstraintKind::GreaterThan:
        return value.upper().isFinite() && value.upper().value() <= Rational();
    }
    return false;
}

struct BilinearDecomposition
{
    LinearExpression affine;
    LinearExpression lhs;
    LinearExpression rhs;
    Rational factor;
    bool hasProduct = false;
};

bool affineConstant(const std::optional<LinearExpression>& expression,
                    Rational& value)
{
    if (!expression || !expression->terms().empty())
        return false;
    value = expression->constant();
    return true;
}

bool decomposeSingleProduct(const TreeExpression& expression,
                            const Rational& scale,
                            BilinearDecomposition& result)
{
    if (scale.isZero())
        return true;
    if (const std::optional<LinearExpression> linear = expression.asLinear())
    {
        result.affine += *linear * scale;
        return true;
    }
    if (expression.kind() == TreeExpression::Kind::Unary &&
        expression.unaryOperator() == UnaryOperator::Negate)
        return decomposeSingleProduct(expression.lhs(), -scale, result);
    if (expression.kind() != TreeExpression::Kind::Binary)
        return false;

    if (expression.binaryOperator() == BinaryOperator::Add ||
        expression.binaryOperator() == BinaryOperator::Subtract)
    {
        if (!decomposeSingleProduct(expression.lhs(), scale, result))
            return false;
        const Rational rhsScale =
            expression.binaryOperator() == BinaryOperator::Add ? scale : -scale;
        return decomposeSingleProduct(expression.rhs(), rhsScale, result);
    }

    const std::optional<LinearExpression> lhs = expression.lhs().asLinear();
    const std::optional<LinearExpression> rhs = expression.rhs().asLinear();
    if (expression.binaryOperator() == BinaryOperator::Multiply)
    {
        if (lhs && rhs)
        {
            if (result.hasProduct)
                return false;
            result.lhs = *lhs;
            result.rhs = *rhs;
            result.factor = scale;
            result.hasProduct = true;
            return true;
        }
        Rational constant;
        if (affineConstant(lhs, constant))
            return decomposeSingleProduct(expression.rhs(), scale * constant,
                                          result);
        if (affineConstant(rhs, constant))
            return decomposeSingleProduct(expression.lhs(), scale * constant,
                                          result);
        return false;
    }
    if (expression.binaryOperator() == BinaryOperator::Divide)
    {
        Rational divisor;
        return affineConstant(rhs, divisor) && !divisor.isZero() &&
               decomposeSingleProduct(expression.lhs(), scale / divisor,
                                      result);
    }
    return false;
}

} // namespace

void NumericalState::assignParallel(const LinearAssignmentList& assignments)
{
    if (assignments.empty())
    {
        recordOperation(OperationKind::Assignment, ApproximationKind::Exact,
                        true);
        return;
    }

    const VariableEnvironment originalEnvironment = environment();
    std::set<Variable> targets;
    for (const LinearAssignment& assignment : assignments)
    {
        if (!originalEnvironment.contains(assignment.target))
            throw std::invalid_argument(
                "parallel assignment target is not in environment");
        if (!targets.insert(assignment.target).second)
            throw std::invalid_argument(
                "parallel assignment contains a duplicate target");
        for (const auto& [variable, coefficient] :
             assignment.expression.terms())
        {
            (void)coefficient;
            if (!originalEnvironment.contains(variable))
                throw std::invalid_argument(
                    "parallel assignment expression uses an unknown variable");
        }
    }
    if (isBottom())
    {
        recordOperation(OperationKind::Assignment, ApproximationKind::Exact,
                        true);
        return;
    }

    ApproximationKind approximation = ApproximationKind::Exact;
    bool best = true;
    std::string reason;
    const auto includeLastOperation = [&]() {
        const OperationMetadata& metadata = lastOperation();
        if (metadata.approximation == ApproximationKind::UnsupportedFallback ||
            (metadata.approximation ==
                 ApproximationKind::SoundOverApproximation &&
             approximation == ApproximationKind::Exact))
            approximation = metadata.approximation;
        best = best && metadata.best;
        if (metadata.approximation != ApproximationKind::Exact &&
            !metadata.reason.empty())
            reason = metadata.reason;
    };

    std::uint64_t nextId = 0;
    for (const VariableDeclaration& declaration :
         originalEnvironment.variables())
        nextId = std::max(
            nextId, static_cast<std::uint64_t>(declaration.variable.id()) + 1);
    if (nextId + assignments.size() >
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) +
            1)
        throw std::overflow_error(
            "not enough temporary variable IDs for parallel assignment");

    std::map<Variable, Variable> oldValues;
    std::vector<VariableDeclaration> temporaries;
    temporaries.reserve(assignments.size());
    for (const LinearAssignment& assignment : assignments)
    {
        const Variable temporary(static_cast<std::uint32_t>(nextId++));
        oldValues.emplace(assignment.target, temporary);
        temporaries.push_back(
            {temporary, originalEnvironment.typeOf(assignment.target),
             "$parallel_old_" + originalEnvironment.nameOf(assignment.target)});
    }

    changeEnvironment(originalEnvironment.add(std::move(temporaries)));
    for (const auto& [target, temporary] : oldValues)
    {
        assign(temporary, LinearExpression(target));
        includeLastOperation();
    }

    for (const LinearAssignment& assignment : assignments)
    {
        LinearExpression rewritten(assignment.expression.constant());
        for (const auto& [variable, coefficient] :
             assignment.expression.terms())
        {
            const auto old = oldValues.find(variable);
            const Variable source =
                old == oldValues.end() ? variable : old->second;
            rewritten.setCoefficient(source, rewritten.coefficient(source) +
                                                 coefficient);
        }
        assign(assignment.target, rewritten);
        includeLastOperation();
    }
    changeEnvironment(originalEnvironment);
    recordOperation(OperationKind::Assignment, approximation, best,
                    std::move(reason));
}

void NumericalState::assignParallel(const TreeAssignmentList& assignments)
{
    std::set<Variable> targets;
    LinearAssignmentList affine;
    std::vector<std::pair<Variable, Interval>> intervalized;
    affine.reserve(assignments.size());
    intervalized.reserve(assignments.size());
    for (const TreeAssignment& assignment : assignments)
    {
        if (!environment().contains(assignment.target))
            throw std::invalid_argument(
                "parallel tree assignment target is not in environment");
        if (!targets.insert(assignment.target).second)
            throw std::invalid_argument(
                "parallel tree assignment contains a duplicate target");
        if (const std::optional<LinearExpression> linear =
                assignment.expression.asLinear())
            affine.push_back({assignment.target, *linear});
        else
            intervalized.emplace_back(
                assignment.target,
                evaluateTreeExpression(assignment.expression));
    }

    // Every nonlinear RHS interval was evaluated above from the common
    // incoming state. Affine right-hand sides now run simultaneously, then the
    // precomputed nonlinear intervals are committed without rereading targets.
    assignParallel(affine);
    for (const auto& [target, value] : intervalized)
        assignInterval(target, value);
    if (!intervalized.empty())
        recordOperation(OperationKind::Assignment,
                        ApproximationKind::SoundOverApproximation, false,
                        "parallel nonlinear or finite IEEE assignments were "
                        "interval-linearized");
}

void NumericalState::assumeAll(const LinearConstraintSet& constraints)
{
    if (constraints.empty())
    {
        recordOperation(OperationKind::Assumption, ApproximationKind::Exact,
                        true);
        return;
    }
    if (constraints.size() < 2)
    {
        for (const LinearConstraint& constraint : constraints)
            assume(constraint);
        return;
    }

    // One pass per constraint and dimension bounds any propagation chain that
    // terminates at all; the equivalence test stops earlier in practice, and
    // immediately for a domain that is exact on linear constraints.
    const std::size_t limit =
        constraints.size() * (environment().size() + 1) + 1;
    for (std::size_t pass = 0; pass < limit; ++pass)
    {
        const std::unique_ptr<AbstractState> before = clone();
        for (const LinearConstraint& constraint : constraints)
            assume(constraint);
        if (isBottom() || isEquivalentTo(*before) == CheckResult::True)
            return;
    }
}

NumericalState::RawBuffer NumericalState::serializeRaw() const
{
    Writer writer;
    writer.writeMagic();
    writer.writeU16(RawVersion);
    const DomainTag tag = domainTag(*this);
    writer.writeByte(static_cast<std::uint8_t>(tag));
    writer.writeByte(configurationFlags(*this, tag));
    writeEnvironment(writer, environment());
    writer.writeByte(isBottom() ? 1U : 0U);
    writeConstraints(writer, canonicalConstraints(*this, tag));
    return writer.finish();
}

void NumericalState::substitute(Variable target,
                                const TreeExpression& expression)
{
    if (const std::optional<LinearExpression> linear = expression.asLinear())
    {
        substitute(target, *linear);
        return;
    }
    // Existentially eliminate the unknown post-value. No fact involving that
    // value can soundly constrain the pre-state without nonlinear/machine
    // semantics for the right-hand side.
    forget(target);
    recordOperation(OperationKind::Substitution,
                    ApproximationKind::UnsupportedFallback, false,
                    "nonlinear backward substitution projected the output");
}

void NumericalState::substituteParallel(const TreeAssignmentList& assignments)
{
    std::set<Variable> targets;
    LinearAssignmentList affine;
    std::vector<Variable> unsupported;
    affine.reserve(assignments.size());
    unsupported.reserve(assignments.size());
    for (const TreeAssignment& assignment : assignments)
    {
        if (!environment().contains(assignment.target))
            throw std::invalid_argument(
                "parallel substitution target is not in environment");
        if (!targets.insert(assignment.target).second)
            throw std::invalid_argument(
                "parallel substitution contains a duplicate target");
        if (const std::optional<LinearExpression> linear =
                assignment.expression.asLinear())
            affine.push_back({assignment.target, *linear});
        else
            unsupported.push_back(assignment.target);
    }

    // Unknown output dimensions are existentially projected before the
    // remaining simultaneous affine preimage is formed.
    for (Variable target : unsupported)
        forget(target);
    substituteParallel(affine);
    if (!unsupported.empty())
        recordOperation(OperationKind::Substitution,
                        ApproximationKind::UnsupportedFallback, false,
                        "parallel nonlinear backward substitution projected "
                        "unsupported outputs");
}

Interval NumericalState::bound(const TreeExpression& expression) const
{
    if (const std::optional<LinearExpression> linear = expression.asLinear())
        return bound(*linear);
    if (isBottom())
        return bottomInterval();
    return evaluateTreeExpression(expression);
}

Interval NumericalState::evaluateTreeExpression(
    const TreeExpression& expression) const
{
    if (isBottom())
        return bottomInterval();
    return evaluateTree(*this, expression);
}

LinearConstraintSet NumericalState::treeConstraintConsequences(
    const TreeConstraint& constraint) const
{
    const Interval value = evaluateTreeExpression(constraint.expression());
    if (definitelyFalse(value, constraint.kind()))
        return {LinearConstraint(LinearExpression(Rational(1)),
                                 ConstraintKind::LessEqual)};
    if (definitelyTrue(value, constraint.kind()))
        return {};

    const TreeExpression& expression = constraint.expression();
    if (expression.type().kind == NumericKind::IEEEFloat)
        return {};
    BilinearDecomposition decomposition;
    if (!decomposeSingleProduct(expression, Rational(1), decomposition) ||
        !decomposition.hasProduct)
        return {};
    const Interval lhsBounds = bound(decomposition.lhs);
    const Interval rhsBounds = bound(decomposition.rhs);
    if (!lhsBounds.lower().isFinite() || !lhsBounds.upper().isFinite() ||
        !rhsBounds.lower().isFinite() || !rhsBounds.upper().isFinite())
        return {};

    const Rational& lx = lhsBounds.lower().value();
    const Rational& ux = lhsBounds.upper().value();
    const Rational& ly = rhsBounds.lower().value();
    const Rational& uy = rhsBounds.upper().value();
    std::vector<LinearExpression> productLowerForms{
        decomposition.rhs * lx + decomposition.lhs * ly -
            LinearExpression(lx * ly),
        decomposition.rhs * ux + decomposition.lhs * uy -
            LinearExpression(ux * uy)};
    std::vector<LinearExpression> productUpperForms{
        decomposition.rhs * ux + decomposition.lhs * ly -
            LinearExpression(ux * ly),
        decomposition.rhs * lx + decomposition.lhs * uy -
            LinearExpression(lx * uy)};
    if (decomposition.factor.sign() < 0)
        std::swap(productLowerForms, productUpperForms);
    std::vector<LinearExpression> lowerForms;
    std::vector<LinearExpression> upperForms;
    lowerForms.reserve(productLowerForms.size());
    upperForms.reserve(productUpperForms.size());
    for (const LinearExpression& form : productLowerForms)
        lowerForms.push_back(decomposition.affine +
                             form * decomposition.factor);
    for (const LinearExpression& form : productUpperForms)
        upperForms.push_back(decomposition.affine +
                             form * decomposition.factor);

    LinearConstraintSet result;
    const auto appendLower = [&](ConstraintKind kind) {
        for (const LinearExpression& form : lowerForms)
            result.emplace_back(form, kind);
    };
    const auto appendUpper = [&](ConstraintKind kind) {
        for (const LinearExpression& form : upperForms)
            result.emplace_back(form, kind);
    };
    switch (constraint.kind())
    {
    case ConstraintKind::LessEqual:
        appendLower(ConstraintKind::LessEqual);
        break;
    case ConstraintKind::LessThan:
        appendLower(ConstraintKind::LessThan);
        break;
    case ConstraintKind::GreaterEqual:
        appendUpper(ConstraintKind::GreaterEqual);
        break;
    case ConstraintKind::GreaterThan:
        appendUpper(ConstraintKind::GreaterThan);
        break;
    case ConstraintKind::Equal:
        appendLower(ConstraintKind::LessEqual);
        appendUpper(ConstraintKind::GreaterEqual);
        break;
    case ConstraintKind::NotEqual:
        break;
    }
    return result;
}

void NumericalState::assignInterval(Variable target, const Interval& value)
{
    if (!environment().contains(target))
        throw std::invalid_argument("assignment target is not in environment");
    if (isBottom())
        return;
    forget(target);
    if (value.isBottom())
    {
        assume(LinearConstraint(LinearExpression(Rational(1)),
                                ConstraintKind::LessEqual));
        return;
    }
    if (value.lower().isFinite())
        assume(LinearConstraint(
            LinearExpression(target) - LinearExpression(value.lower().value()),
            value.lower().isStrict() ? ConstraintKind::GreaterThan
                                     : ConstraintKind::GreaterEqual));
    if (value.upper().isFinite())
        assume(LinearConstraint(
            LinearExpression(target) - LinearExpression(value.upper().value()),
            value.upper().isStrict() ? ConstraintKind::LessThan
                                     : ConstraintKind::LessEqual));
}

void NumericalState::recordOperation(OperationKind operation,
                                     ApproximationKind approximation, bool best,
                                     std::string reason) const
{
    lastOperation_ = {operation, approximation,
                      approximation == ApproximationKind::Exact, best,
                      std::move(reason)};
}

VariableEnvironment NumericalState::unifyEnvironmentWith(
    NumericalState& other, bool initializeNewVariablesToZero)
{
    const VariableEnvironment merged = environment().merge(other.environment());
    changeEnvironment(merged, initializeNewVariablesToZero);
    other.changeEnvironment(merged, initializeNewVariablesToZero);
    return merged;
}

std::uint64_t NumericalState::hash() const
{
    const RawBuffer raw = serializeRaw();
    return readTrailingU64(raw);
}

std::unique_ptr<NumericalState> NumericalState::deserializeRaw(
    const RawBuffer& buffer)
{
    Reader reader(buffer);
    reader.readMagic();
    if (reader.readU16() != RawVersion)
        throw std::invalid_argument("raw state has an unsupported version");
    const DomainTag tag = decodeDomainTag(reader.readByte());
    const std::uint8_t flags = reader.readByte();
    const VariableEnvironment environment = readEnvironment(reader);
    const std::uint8_t bottomByte = reader.readByte();
    if (bottomByte > 1)
        throw std::invalid_argument("raw state has an invalid bottom flag");
    const LinearConstraintSet constraints =
        readConstraints(reader, environment);
    if (!reader.empty())
        throw std::invalid_argument("raw state has trailing data");
    if (bottomByte != 0 && !constraints.empty())
        throw std::invalid_argument(
            "raw bottom state unexpectedly contains constraints");
    return restore(tag, flags, environment, bottomByte != 0, constraints);
}

} // namespace SVF::AbstractDomain
