#pragma once

#include "mpl/core/Rational.hpp"
#include "mpl/core/Time.hpp"

#include <cstdint>
#include <string_view>

namespace mpl {

/// A point on the musical timeline, measured in exact whole notes.
class MusicalTime {
public:
    MusicalTime() = default;
    explicit MusicalTime(Position position) : position_(position) {}

    const Position& position() const noexcept { return position_; }
    const Rational& wholeNotes() const noexcept { return position_.inWholeNotes(); }

    friend bool operator==(const MusicalTime& lhs, const MusicalTime& rhs) {
        return lhs.position_ == rhs.position_;
    }
    friend bool operator<(const MusicalTime& lhs, const MusicalTime& rhs) {
        return lhs.position_ < rhs.position_;
    }

private:
    Position position_;
};

/// Wall-clock time in seconds. Used by audio/transport backends, never as the
/// primary axis of musical structure.
class PhysicalTime {
public:
    PhysicalTime() = default;
    explicit PhysicalTime(double seconds);

    double seconds() const noexcept { return seconds_; }

    friend bool operator==(const PhysicalTime& lhs, const PhysicalTime& rhs) {
        return lhs.seconds_ == rhs.seconds_;
    }
    friend bool operator<(const PhysicalTime& lhs, const PhysicalTime& rhs) {
        return lhs.seconds_ < rhs.seconds_;
    }

private:
    double seconds_ = 0.0;
};

/// Normalized position inside an instance, in [0, 1].
class RelativeTime {
public:
    RelativeTime() = default;
    explicit RelativeTime(Rational fraction);

    const Rational& fraction() const noexcept { return fraction_; }

    friend bool operator==(const RelativeTime& lhs, const RelativeTime& rhs) {
        return lhs.fraction_ == rhs.fraction_;
    }
    friend bool operator<(const RelativeTime& lhs, const RelativeTime& rhs) {
        return lhs.fraction_ < rhs.fraction_;
    }

private:
    Rational fraction_{0, 1};
};

/// Discrete audio-sample position. Defined now so that the temporal model has
/// a clear boundary, even though sample-domain curves are not evaluated yet.
class SampleTime {
public:
    SampleTime() = default;
    explicit SampleTime(std::int64_t samples);

    std::int64_t samples() const noexcept { return samples_; }

    friend bool operator==(const SampleTime& lhs, const SampleTime& rhs) {
        return lhs.samples_ == rhs.samples_;
    }
    friend bool operator<(const SampleTime& lhs, const SampleTime& rhs) {
        return lhs.samples_ < rhs.samples_;
    }

private:
    std::int64_t samples_ = 0;
};

enum class TimeDomain { Musical, Relative, Physical, Sample };

std::string_view timeDomainName(TimeDomain domain);

}  // namespace mpl
