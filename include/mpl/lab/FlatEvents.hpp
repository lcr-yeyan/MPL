#pragma once

#include "mpl/core/Rational.hpp"
#include "mpl/observe/Observations.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mpl {

struct FlatEvent {
    int pitch = 60;
    Rational onset;
    Rational duration{Rational(1, 4)};
    int velocity = 90;
};

struct FlatTrack {
    std::string id;
    std::vector<FlatEvent> events;
};

struct FlatMeter {
    Rational position;
    int numerator = 4;
    int denominator = 4;
    std::string grouping;
};

/// Independent, direct-event baseline. Times are exact rational whole-note
/// strings ("0", "1/4", "3/8"); no floating-point beat positions.
struct FlatEventDocument {
    Rational logicalExtent;
    double tempoBpm = 120.0;
    std::vector<FlatMeter> meters;
    std::vector<FlatTrack> tracks;
    std::string taskContext;
};

struct FlatParseResult {
    bool ok = false;
    FlatEventDocument document;
    std::string error;
};

FlatParseResult parseFlatEvents(std::string_view text);
ObservationSet observeFlatEvents(const FlatEventDocument& document,
                                 const ObservationOptions& options = {});

/// The flat representation does not have generation provenance, harmony
/// timelines, module dependencies or an explicit voicing sequence. Such
/// observations are reported as unsupported, not unverifiable or passing.
bool flatSupportsObservation(std::string_view name);
std::string flatUnsupportedReason(std::string_view name);

}  // namespace mpl
