#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/MeterMap.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/ir/Assets.hpp"
#include "mpl/ir/Automation.hpp"
#include "mpl/ir/MixIR.hpp"
#include "mpl/music/Fragment.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mpl {

// Production IR is the lowered, DAW-facing form of a project: musical objects
// have been expanded into tracks, clips and absolute musical timeline
// positions. It no longer references the generative dependency graph.
//
// Backend-specific concerns (MIDI channels, programs, PPQ) are deliberately
// absent: those belong to the exporter, not to the production model.

struct TempoEvent {
    Position position;
    double bpm = 120.0;
};

struct MeterEvent {
    Position position;
    Meter meter;
    /// Empty means "uniform one-unit groups"; use effectiveGrouping().
    MetricGrouping grouping;
    std::int64_t barIndex = 0;
};

/// Grouping actually in effect for an event, defaulting to uniform groups.
inline MetricGrouping effectiveGrouping(const MeterEvent& event) {
    return event.grouping.groups.empty() ? MetricGrouping::uniform(event.meter)
                                         : event.grouping;
}

struct ClipIR {
    std::string name;
    Position start;
    Fragment content;
    ObjectId sourceObject = kInvalidObjectId;
    std::string sourceLabel;

    /// Stable id of this playback instance. The same source object may be
    /// instantiated many times with independent expressions.
    InstanceId instanceId = kInvalidInstanceId;
    PerformanceExpression expression;

    Position endPosition() const { return start + content.duration(); }
};

struct TrackIR {
    std::string name;
    std::string instrument;
    InstrumentRole role = InstrumentRole::Melodic;
    std::vector<AssetSlot> assetSlots;
    std::vector<AssetBinding> assetBindings;
    std::vector<ClipIR> clips;

    Position endPosition() const;
    std::size_t noteCount() const;
    const AssetBinding* findBinding(std::string_view slot) const;
};

struct ProductionIR {
    std::vector<TempoEvent> tempoMap;
    std::vector<MeterEvent> meterMap;
    std::vector<TrackIR> tracks;

    /// Backend-neutral automation bindings targeting tracks, clips or master.
    std::vector<AutomationBinding> automation;

    /// Mix graph description: processors, track strips, buses and master.
    MixSpec mix;

    /// Logical arrangement extent; may exceed the last note's end. Audio
    /// rendering must not delete this padding.
    Duration logicalExtent{Rational(0, 1)};

    Position endPosition() const;
    Duration totalDuration() const;
    std::size_t noteCount() const;
    std::size_t clipCount() const;

    std::string toJson() const;
};

}  // namespace mpl
