#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>

namespace mpl {

/// Exact rational number. Musical time is expressed in whole notes, so a
/// quarter note is 1/4, a half note 1/2 and a bar of 4/4 is 1.
class Rational {
public:
    Rational() = default;
    Rational(std::int64_t numerator, std::int64_t denominator = 1);

    static Rational integer(std::int64_t value) { return Rational(value, 1); }

    std::int64_t numerator() const noexcept { return numerator_; }
    std::int64_t denominator() const noexcept { return denominator_; }

    bool isZero() const noexcept { return numerator_ == 0; }
    bool isInteger() const noexcept { return denominator_ == 1; }
    bool isPositive() const noexcept { return numerator_ > 0; }
    bool isNegative() const noexcept { return numerator_ < 0; }

    double toDouble() const noexcept;
    std::string toString() const;

    static Rational abs(const Rational& value);

    Rational operator-() const noexcept { return Rational(-numerator_, denominator_); }

    Rational& operator+=(const Rational& rhs);
    Rational& operator-=(const Rational& rhs);
    Rational& operator*=(const Rational& rhs);
    Rational& operator/=(const Rational& rhs);

    friend Rational operator+(Rational lhs, const Rational& rhs) { return lhs += rhs; }
    friend Rational operator-(Rational lhs, const Rational& rhs) { return lhs -= rhs; }
    friend Rational operator*(Rational lhs, const Rational& rhs) { return lhs *= rhs; }
    friend Rational operator/(Rational lhs, const Rational& rhs) { return lhs /= rhs; }

    friend bool operator==(const Rational& lhs, const Rational& rhs) {
        return lhs.numerator_ == rhs.numerator_ && lhs.denominator_ == rhs.denominator_;
    }
    friend bool operator!=(const Rational& lhs, const Rational& rhs) { return !(lhs == rhs); }
    /// Exact Euclidean comparison; no floating-point approximation is used.
    friend bool operator<(const Rational& lhs, const Rational& rhs);
    friend bool operator>(const Rational& lhs, const Rational& rhs) { return rhs < lhs; }
    friend bool operator<=(const Rational& lhs, const Rational& rhs) { return !(rhs < lhs); }
    friend bool operator>=(const Rational& lhs, const Rational& rhs) { return !(lhs < rhs); }

    friend std::ostream& operator<<(std::ostream& os, const Rational& value);

private:
    void normalize();

    std::int64_t numerator_ = 0;
    std::int64_t denominator_ = 1;
};

/// Largest numerator/denominator that musical period arithmetic is allowed to
/// produce. Rational itself stays general; this is the explicit capacity
/// contract of the rhythm/meter layer, so an LCM cannot silently overflow.
inline constexpr std::int64_t kMusicalRationalLimit = 1000000000;

/// Least positive common multiple of two positive rationals. Returns nullopt
/// when either input is non-positive or the result exceeds the musical limit.
/// For 3/8 and 1/2 this yields 3/2: four 3/8 cycles equal three 1/2 cycles.
std::optional<Rational> rationalLcm(const Rational& lhs, const Rational& rhs);

/// `value / unit` when that quotient is an integer, otherwise nullopt.
std::optional<std::int64_t> integerQuotient(const Rational& value, const Rational& unit);

/// True when `value` is a non-negative integer multiple of `unit`.
bool isIntegerMultiple(const Rational& value, const Rational& unit);

}  // namespace mpl
