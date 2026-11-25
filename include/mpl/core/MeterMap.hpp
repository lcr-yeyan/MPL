#pragma once

#include "mpl/core/Rational.hpp"
#include "mpl/core/Time.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

/// How a bar is divided into felt beats. Groups are expressed in units of the
/// time signature denominator, so 7/8 as 2+2+3 is {2, 2, 3} and sums to the
/// numerator. Grouping is a metric property, not a performed accent: it never
/// changes note velocity by itself.
struct MetricGrouping {
    std::vector<int> groups;
    /// False when the grouping was defaulted to one unit per beat.
    bool explicitGrouping = false;

    static MetricGrouping uniform(const Meter& meter);

    int totalUnits() const;
    std::size_t groupCount() const { return groups.size(); }

    /// Index of the group containing `unit` (in denominator units), or -1.
    int groupAtUnit(int unit) const;
    /// First denominator unit of `groupIndex`, or -1.
    int groupStartUnit(int groupIndex) const;

    bool isValid(const Meter& meter, std::string* error = nullptr) const;

    /// Length of one bar under this grouping. Always equal to meter.barLength()
    /// when the grouping is valid; the function exists to make that invariant
    /// explicit and testable.
    Rational barLength(const Meter& meter) const;

    std::string toString() const;

    friend bool operator==(const MetricGrouping& lhs, const MetricGrouping& rhs) {
        return lhs.groups == rhs.groups && lhs.explicitGrouping == rhs.explicitGrouping;
    }
    friend bool operator!=(const MetricGrouping& lhs, const MetricGrouping& rhs) {
        return !(lhs == rhs);
    }
};

/// A position expressed as a bar index plus an exact offset inside the bar,
/// measured in denominator units. For 7/8, beat 5/2 is the second eighth note
/// of the fifth beat-slot (counting from zero). This is a coordinate over the
/// same absolute MusicalTime axis, not a new time domain.
struct MetricPosition {
    std::int64_t bar = 0;
    Rational beat{0, 1};

    friend bool operator==(const MetricPosition& lhs, const MetricPosition& rhs) {
        return lhs.bar == rhs.bar && lhs.beat == rhs.beat;
    }
    friend bool operator!=(const MetricPosition& lhs, const MetricPosition& rhs) {
        return !(lhs == rhs);
    }
    std::string toString() const;
};

/// One meter-map entry. `barIndex` is the absolute bar number at `position`.
struct MeterMapEvent {
    Position position;
    Meter meter;
    MetricGrouping grouping;
    std::int64_t barIndex = 0;
};

/// Query result for the metric hierarchy. Level 3 = bar, 2 = group, 1 = beat,
/// 0 = subdivision inside a beat. `weight` is a stable numeric projection of
/// the level for tests and external querying, not an aesthetic judgement.
struct MetricWeight {
    int level = 0;
    double weight = 0.0;
    bool barStart = false;
    bool groupStart = false;
    bool beatStart = false;
    int groupIndex = -1;
    int beatIndex = 0;
    Rational subdivision{0, 1};
};

enum class MetricUnit { Whole, Bars, Beats };

/// A length described in metric units. It only becomes an absolute Duration
/// after a MeterMap and an anchor position are supplied. This is the type-level
/// answer to "bars depend on where you start".
struct MetricDuration {
    Rational amount{0, 1};
    MetricUnit unit = MetricUnit::Whole;
};

std::string_view metricUnitName(MetricUnit unit);

/// Ordered, exact meter map. The first event is always at position zero and
/// every later event must start on a bar boundary: meter changes inside a bar
/// are rejected instead of approximated. Tempo is deliberately not part of
/// this type.
class MeterMap {
public:
    MeterMap();
    explicit MeterMap(Meter meter, MetricGrouping grouping = {});

    /// Adds a meter change. On failure returns false and fills `error`; the map
    /// is left unchanged. Strictly increasing positions and bar-boundary
    /// alignment are enforced.
    bool tryAdd(const Position& position, const Meter& meter,
                const MetricGrouping& grouping, std::string& error);

    /// Convenience wrapper that throws mpl::Error with the same message.
    void add(const Position& position, const Meter& meter,
             const MetricGrouping& grouping = {});

    const std::vector<MeterMapEvent>& events() const noexcept { return events_; }
    const MeterMapEvent& initial() const noexcept { return events_.front(); }
    std::size_t size() const noexcept { return events_.size(); }

    /// Event in effect at `position`. Positions before zero report the initial
    /// event; callers that care about domain validity must check
    /// isInRange()/validate() first.
    const MeterMapEvent& eventAt(const Position& position) const;
    std::size_t eventIndexAt(const Position& position) const;

    const Meter& meterAt(const Position& position) const;
    const MetricGrouping& groupingAt(const Position& position) const;
    Rational barLengthAt(const Position& position) const;

    bool isInRange(const Position& position) const;

    /// True when `position` is exactly the start of a bar.
    bool isBarStart(const Position& position) const;
    /// True when `position` falls exactly on a denominator-unit beat.
    bool isBeatStart(const Position& position) const;
    /// Floor bar index at `position`. Throws for negative positions.
    std::int64_t barIndexAt(const Position& position) const;
    /// Bar-relative offset in whole notes. Throws for negative positions.
    Rational barOffsetAt(const Position& position) const;
    /// Start of the bar containing `position`.
    Position barStart(const Position& position) const;
    /// Start of absolute bar `bar`. Throws for negative indices.
    Position positionOfBar(std::int64_t bar) const;

    /// Exact conversion. Throws for negative positions or malformed metric
    /// positions (negative beat, beat >= numerator).
    MetricPosition toMetric(const Position& position) const;
    Position toPosition(const MetricPosition& position) const;

    /// Resolves a metric duration at an anchor. For Bars the anchor must be a
    /// bar start and the amount must be a non-negative integer; for Beats the
    /// beat length of the meter in effect at the anchor is used. Returns
    /// nullopt and fills `error` on failure.
    std::optional<Rational> resolveDuration(const Position& anchor,
                                            const MetricDuration& duration,
                                            std::string& error) const;

    /// Anchor + resolved duration.
    std::optional<Position> resolveEnd(const Position& anchor,
                                       const MetricDuration& duration,
                                       std::string& error) const;

    /// Minimal metric hierarchy query. `position` must be non-negative.
    MetricWeight metricWeight(const Position& position) const;

    /// Checks sortedness, bar alignment, meter validity and grouping sums.
    bool validate(std::string& error) const;

    /// Human-readable ledger: "0 4/4; 3 7/8 (2+2+3)".
    std::string describe() const;

private:
    struct EventSearch {
        std::int64_t barIndex = 0;
        Rational offsetInBar{0, 1};
        const MeterMapEvent* event = nullptr;
    };
    EventSearch locate(const Position& position) const;
    static bool validMeter(const Meter& meter, std::string& error);

    std::vector<MeterMapEvent> events_;
};

}  // namespace mpl
