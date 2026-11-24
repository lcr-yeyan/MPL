#include "mpl/core/TimeTypes.hpp"

#include "mpl/core/Error.hpp"

#include <cmath>

namespace mpl {

PhysicalTime::PhysicalTime(double seconds) : seconds_(seconds) {
    if (!std::isfinite(seconds_) || seconds_ < 0.0) {
        throw Error("physical time must be finite and non-negative", "time");
    }
}

RelativeTime::RelativeTime(Rational fraction) : fraction_(std::move(fraction)) {
    if (fraction_.isNegative() || fraction_ > Rational(1, 1)) {
        throw Error("relative time must be in [0, 1], got " + fraction_.toString(), "time");
    }
}

SampleTime::SampleTime(std::int64_t samples) : samples_(samples) {
    if (samples_ < 0) {
        throw Error("sample time must be non-negative", "time");
    }
}

std::string_view timeDomainName(TimeDomain domain) {
    switch (domain) {
        case TimeDomain::Musical: return "musical";
        case TimeDomain::Relative: return "relative";
        case TimeDomain::Physical: return "physical";
        case TimeDomain::Sample: return "sample";
    }
    return "unknown";
}

}  // namespace mpl
