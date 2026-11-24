#pragma once

#include "mpl/core/Rational.hpp"

#include <ostream>
#include <string>

namespace mpl {

/// Time signature. All derived lengths are expressed in whole notes, which is
/// the base unit of the Music IR.
struct Meter {
    int numerator = 4;
    int denominator = 4;

    /// Length of one bar in whole notes: (numerator / denominator).
    Rational barLength() const { return Rational(numerator, denominator); }

    /// Length of one beat in whole notes: (1 / denominator).
    Rational beatLength() const { return Rational(1, denominator); }

    std::string toString() const {
        return std::to_string(numerator) + "/" + std::to_string(denominator);
    }
};

inline std::ostream& operator<<(std::ostream& os, const Meter& meter) {
    return os << meter.toString();
}

/// A musical length. Internally a rational number of whole notes, so bar and
/// beat arithmetic is exact and independent of tempo and PPQ.
class Duration {
public:
    Duration() = default;
    explicit Duration(Rational wholeNotes) : wholeNotes_(std::move(wholeNotes)) {}

    static Duration fromWholeNotes(const Rational& value) { return Duration(value); }
    static Duration fromQuarterNotes(const Rational& value) { return Duration(value / Rational(4)); }
    static Duration fromBars(const Rational& bars, const Meter& meter) {
        return Duration(bars * meter.barLength());
    }
    static Duration fromBeats(const Rational& beats, const Meter& meter) {
        return Duration(beats * meter.beatLength());
    }

    const Rational& inWholeNotes() const noexcept { return wholeNotes_; }
    double toSeconds(double bpm) const noexcept {
        if (bpm <= 0.0) {
            return 0.0;
        }
        return wholeNotes_.toDouble() * 4.0 * 60.0 / bpm;
    }

    Duration operator+(const Duration& rhs) const { return Duration(wholeNotes_ + rhs.wholeNotes_); }
    Duration operator-(const Duration& rhs) const { return Duration(wholeNotes_ - rhs.wholeNotes_); }
    Duration operator*(const Rational& factor) const { return Duration(wholeNotes_ * factor); }
    Duration operator/(const Rational& factor) const { return Duration(wholeNotes_ / factor); }

    Duration& operator+=(const Duration& rhs) { wholeNotes_ += rhs.wholeNotes_; return *this; }
    Duration& operator-=(const Duration& rhs) { wholeNotes_ -= rhs.wholeNotes_; return *this; }

    friend bool operator==(const Duration& lhs, const Duration& rhs) {
        return lhs.wholeNotes_ == rhs.wholeNotes_;
    }
    friend bool operator!=(const Duration& lhs, const Duration& rhs) { return !(lhs == rhs); }
    friend bool operator<(const Duration& lhs, const Duration& rhs) {
        return lhs.wholeNotes_ < rhs.wholeNotes_;
    }
    friend bool operator>(const Duration& lhs, const Duration& rhs) { return rhs < lhs; }
    friend bool operator<=(const Duration& lhs, const Duration& rhs) { return !(rhs < lhs); }
    friend bool operator>=(const Duration& lhs, const Duration& rhs) { return !(lhs < rhs); }

    std::string toString() const { return wholeNotes_.toString(); }

private:
    Rational wholeNotes_{0, 1};
};

inline std::ostream& operator<<(std::ostream& os, const Duration& duration) {
    return os << duration.toString();
}

/// A point in musical time, measured in whole notes from the start of the
/// project.
class Position {
public:
    Position() = default;
    explicit Position(Rational wholeNotes) : wholeNotes_(std::move(wholeNotes)) {}

    static Position fromWholeNotes(const Rational& value) { return Position(value); }
    static Position fromBars(const Rational& bars, const Meter& meter) {
        return Position(bars * meter.barLength());
    }
    static Position fromBeats(const Rational& beats, const Meter& meter) {
        return Position(beats * meter.beatLength());
    }

    const Rational& inWholeNotes() const noexcept { return wholeNotes_; }

    Position operator+(const Duration& duration) const {
        return Position(wholeNotes_ + duration.inWholeNotes());
    }
    Position operator-(const Duration& duration) const {
        return Position(wholeNotes_ - duration.inWholeNotes());
    }
    Duration operator-(const Position& rhs) const {
        return Duration(wholeNotes_ - rhs.wholeNotes_);
    }

    Position& operator+=(const Duration& duration) {
        wholeNotes_ += duration.inWholeNotes();
        return *this;
    }

    friend bool operator==(const Position& lhs, const Position& rhs) {
        return lhs.wholeNotes_ == rhs.wholeNotes_;
    }
    friend bool operator!=(const Position& lhs, const Position& rhs) { return !(lhs == rhs); }
    friend bool operator<(const Position& lhs, const Position& rhs) {
        return lhs.wholeNotes_ < rhs.wholeNotes_;
    }
    friend bool operator>(const Position& lhs, const Position& rhs) { return rhs < lhs; }
    friend bool operator<=(const Position& lhs, const Position& rhs) { return !(rhs < lhs); }
    friend bool operator>=(const Position& lhs, const Position& rhs) { return !(lhs < rhs); }

    std::string toString() const { return wholeNotes_.toString(); }

private:
    Rational wholeNotes_{0, 1};
};

inline std::ostream& operator<<(std::ostream& os, const Position& position) {
    return os << position.toString();
}

}  // namespace mpl
