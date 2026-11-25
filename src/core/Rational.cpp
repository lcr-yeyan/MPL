#include "mpl/core/Rational.hpp"

#include "mpl/core/Error.hpp"

#include <limits>
#include <numeric>
#include <ostream>
#include <sstream>

namespace mpl {
namespace {

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

[[noreturn]] void throwOverflow(const char* operation) {
    throw Error(std::string("rational ") + operation + " overflowed int64", "rational");
}

std::int64_t checkedAdd(std::int64_t lhs, std::int64_t rhs) {
    if (rhs > 0 && lhs > kMax - rhs) {
        throwOverflow("addition");
    }
    if (rhs < 0 && lhs < kMin - rhs) {
        throwOverflow("addition");
    }
    return lhs + rhs;
}

std::int64_t checkedSubtract(std::int64_t lhs, std::int64_t rhs) {
    if (rhs > 0 && lhs < kMin + rhs) {
        throwOverflow("subtraction");
    }
    if (rhs < 0 && lhs > kMax + rhs) {
        throwOverflow("subtraction");
    }
    return lhs - rhs;
}

std::int64_t checkedMultiply(std::int64_t lhs, std::int64_t rhs) {
    if (lhs > 0) {
        if (rhs > 0) {
            if (lhs > kMax / rhs) {
                throwOverflow("multiplication");
            }
        } else if (rhs < 0) {
            if (rhs < kMin / lhs) {
                throwOverflow("multiplication");
            }
        }
    } else if (lhs < 0) {
        if (rhs > 0) {
            if (lhs < kMin / rhs) {
                throwOverflow("multiplication");
            }
        } else if (rhs < 0) {
            if (lhs < kMax / rhs) {
                throwOverflow("multiplication");
            }
        }
    }
    return lhs * rhs;
}

std::int64_t magnitude(std::int64_t value) {
    if (value == kMin) {
        throwOverflow("magnitude");
    }
    return value < 0 ? -value : value;
}

/// Compares a/b with c/d using the Euclidean continued-fraction algorithm.
/// Both denominators must be positive; the loop never forms a product that can
/// overflow int64.
int compareFractions(std::int64_t a, std::int64_t b, std::int64_t c, std::int64_t d) {
    int direction = 1;
    while (true) {
        std::int64_t quotientA = a / b;
        std::int64_t remainderA = a % b;
        if (remainderA != 0 && remainderA < 0) {
            --quotientA;
            remainderA += b;
        }

        std::int64_t quotientC = c / d;
        std::int64_t remainderC = c % d;
        if (remainderC != 0 && remainderC < 0) {
            --quotientC;
            remainderC += d;
        }

        if (quotientA != quotientC) {
            const int result = quotientA < quotientC ? -1 : 1;
            return result * direction;
        }
        if (remainderA == 0 || remainderC == 0) {
            if (remainderA == remainderC) {
                return 0;
            }
            const int result = remainderA < remainderC ? -1 : 1;
            return result * direction;
        }

        // Compare the remainders by comparing their reciprocals; this flips
        // the ordering, so remember the direction.
        a = b;
        b = remainderA;
        c = d;
        d = remainderC;
        direction = -direction;
    }
}

}  // namespace

Rational::Rational(std::int64_t numerator, std::int64_t denominator)
    : numerator_(numerator), denominator_(denominator) {
    if (denominator_ == 0) {
        throw Error("rational denominator must not be zero", "rational");
    }
    if (numerator_ == kMin || denominator_ == kMin) {
        throwOverflow("construction");
    }
    normalize();
}

void Rational::normalize() {
    if (numerator_ == 0) {
        denominator_ = 1;
        return;
    }
    if (denominator_ < 0) {
        numerator_ = -numerator_;
        denominator_ = -denominator_;
    }
    const std::int64_t divisor = std::gcd(magnitude(numerator_), denominator_);
    numerator_ /= divisor;
    denominator_ /= divisor;
}

double Rational::toDouble() const noexcept {
    return static_cast<double>(numerator_) / static_cast<double>(denominator_);
}

std::string Rational::toString() const {
    if (denominator_ == 1) {
        return std::to_string(numerator_);
    }
    return std::to_string(numerator_) + "/" + std::to_string(denominator_);
}

Rational Rational::abs(const Rational& value) {
    return value.isNegative() ? -value : value;
}

Rational& Rational::operator+=(const Rational& rhs) {
    const std::int64_t left = checkedMultiply(numerator_, rhs.denominator_);
    const std::int64_t right = checkedMultiply(rhs.numerator_, denominator_);
    numerator_ = checkedAdd(left, right);
    denominator_ = checkedMultiply(denominator_, rhs.denominator_);
    normalize();
    return *this;
}

Rational& Rational::operator-=(const Rational& rhs) {
    const std::int64_t left = checkedMultiply(numerator_, rhs.denominator_);
    const std::int64_t right = checkedMultiply(rhs.numerator_, denominator_);
    numerator_ = checkedSubtract(left, right);
    denominator_ = checkedMultiply(denominator_, rhs.denominator_);
    normalize();
    return *this;
}

Rational& Rational::operator*=(const Rational& rhs) {
    // Cross-cancel before multiplying so musically sized values stay exact
    // without overflowing the intermediate 64-bit products.
    const std::int64_t left = std::gcd(magnitude(numerator_), magnitude(rhs.denominator_));
    const std::int64_t right = std::gcd(magnitude(rhs.numerator_), magnitude(denominator_));
    numerator_ = checkedMultiply(numerator_ / left, rhs.numerator_ / right);
    denominator_ = checkedMultiply(denominator_ / right, rhs.denominator_ / left);
    normalize();
    return *this;
}

Rational& Rational::operator/=(const Rational& rhs) {
    if (rhs.isZero()) {
        throw Error("division by zero", "rational");
    }
    return *this *= Rational(rhs.denominator_, rhs.numerator_);
}

std::ostream& operator<<(std::ostream& os, const Rational& value) {
    os << value.toString();
    return os;
}

bool operator<(const Rational& lhs, const Rational& rhs) {
    return compareFractions(lhs.numerator_, lhs.denominator_, rhs.numerator_,
                            rhs.denominator_) < 0;
}

std::optional<Rational> rationalLcm(const Rational& lhs, const Rational& rhs) {
    if (!lhs.isPositive() || !rhs.isPositive()) {
        return std::nullopt;
    }
    // The musical layer caps components at kMusicalRationalLimit, so the
    // intermediate lcm of two numerators (<= 1e18) fits int64. This avoids
    // non-portable 128-bit arithmetic and keeps the capacity contract explicit.
    const auto withinLimit = [](const Rational& value) {
        return value.numerator() <= kMusicalRationalLimit &&
               value.denominator() <= kMusicalRationalLimit;
    };
    if (!withinLimit(lhs) || !withinLimit(rhs)) {
        return std::nullopt;
    }
    const std::int64_t numeratorGcd = std::gcd(lhs.numerator(), rhs.numerator());
    const std::int64_t denominatorGcd = std::gcd(lhs.denominator(), rhs.denominator());
    const std::int64_t numerator =
        (lhs.numerator() / numeratorGcd) * rhs.numerator();
    const std::int64_t denominator = denominatorGcd;
    const std::int64_t divisor = std::gcd(numerator, denominator);
    const std::int64_t finalNumerator = numerator / divisor;
    const std::int64_t finalDenominator = denominator / divisor;
    if (finalNumerator > kMusicalRationalLimit ||
        finalDenominator > kMusicalRationalLimit) {
        return std::nullopt;
    }
    return Rational(finalNumerator, finalDenominator);
}

std::optional<std::int64_t> integerQuotient(const Rational& value, const Rational& unit) {
    if (!unit.isPositive()) {
        return std::nullopt;
    }
    try {
        const Rational quotient = value / unit;
        if (!quotient.isInteger()) {
            return std::nullopt;
        }
        return quotient.numerator();
    } catch (const Error&) {
        return std::nullopt;
    }
}

bool isIntegerMultiple(const Rational& value, const Rational& unit) {
    return integerQuotient(value, unit).has_value();
}

}  // namespace mpl
