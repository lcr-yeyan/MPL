#include "mpl/core/MeterMap.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace mpl {
namespace {

bool isPowerOfTwo(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

std::int64_t floorPositive(const Rational& value) {
    if (value.isNegative()) {
        // Metric coordinates are only defined on the non-negative timeline.
        return -1;
    }
    return value.numerator() / value.denominator();
}

std::string meterText(const Meter& meter) {
    return meter.toString();
}

}  // namespace

MetricGrouping MetricGrouping::uniform(const Meter& meter) {
    MetricGrouping grouping;
    grouping.groups.assign(static_cast<std::size_t>(std::max(1, meter.numerator)), 1);
    grouping.explicitGrouping = false;
    return grouping;
}

int MetricGrouping::totalUnits() const {
    int total = 0;
    for (const int group : groups) {
        total += group;
    }
    return total;
}

int MetricGrouping::groupAtUnit(int unit) const {
    int cursor = 0;
    for (std::size_t index = 0; index < groups.size(); ++index) {
        cursor += groups[index];
        if (unit < cursor) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int MetricGrouping::groupStartUnit(int groupIndex) const {
    if (groupIndex < 0) {
        return -1;
    }
    int cursor = 0;
    for (int index = 0; index < groupIndex; ++index) {
        cursor += groups[static_cast<std::size_t>(index)];
    }
    return cursor;
}

bool MetricGrouping::isValid(const Meter& meter, std::string* error) const {
    const auto fail = [&](std::string message) {
        if (error != nullptr) {
            *error = std::move(message);
        }
        return false;
    };
    if (groups.empty()) {
        return fail("metric grouping is empty");
    }
    int total = 0;
    for (const int group : groups) {
        if (group < 1) {
            return fail("metric grouping contains a non-positive group");
        }
        total += group;
        if (total > 1000000) {
            return fail("metric grouping is unreasonably large");
        }
    }
    if (total != meter.numerator) {
        return fail("metric grouping sums to " + std::to_string(total) + ", expected " +
                    std::to_string(meter.numerator) + " for " + meterText(meter));
    }
    return true;
}

Rational MetricGrouping::barLength(const Meter& meter) const {
    return Rational(totalUnits(), meter.denominator);
}

std::string MetricGrouping::toString() const {
    std::string text;
    for (std::size_t index = 0; index < groups.size(); ++index) {
        if (index != 0) {
            text += "+";
        }
        text += std::to_string(groups[index]);
    }
    return text;
}

std::string MetricPosition::toString() const {
    return std::to_string(bar) + ":" + beat.toString();
}

std::string_view metricUnitName(MetricUnit unit) {
    switch (unit) {
        case MetricUnit::Whole: return "whole";
        case MetricUnit::Bars: return "bars";
        case MetricUnit::Beats: return "beats";
    }
    return "whole";
}

MeterMap::MeterMap() : MeterMap(Meter{4, 4}) {}

MeterMap::MeterMap(Meter meter, MetricGrouping grouping) {
    std::string error;
    if (!validMeter(meter, error)) {
        throw Error(error, "meter");
    }
    if (grouping.groups.empty()) {
        grouping = MetricGrouping::uniform(meter);
    }
    if (!grouping.isValid(meter, &error)) {
        throw Error(error, "meter");
    }
    MeterMapEvent event;
    event.position = Position{};
    event.meter = meter;
    event.grouping = std::move(grouping);
    event.barIndex = 0;
    events_.push_back(std::move(event));
}

bool MeterMap::validMeter(const Meter& meter, std::string& error) {
    if (meter.numerator < 1) {
        error = "meter numerator must be positive";
        return false;
    }
    if (!isPowerOfTwo(meter.denominator)) {
        error = "meter denominator must be a power of two";
        return false;
    }
    if (meter.numerator > 1000000) {
        error = "meter numerator is unreasonably large";
        return false;
    }
    return true;
}

bool MeterMap::tryAdd(const Position& position, const Meter& meter,
                      const MetricGrouping& grouping, std::string& error) {
    std::string local;
    if (!validMeter(meter, local)) {
        error = local;
        return false;
    }
    MetricGrouping normalized = grouping;
    if (normalized.groups.empty()) {
        normalized = MetricGrouping::uniform(meter);
    }
    if (!normalized.isValid(meter, &local)) {
        error = local;
        return false;
    }
    if (position.inWholeNotes().isNegative()) {
        error = "meter event position must not be negative";
        return false;
    }
    if (!(events_.back().position < position)) {
        error = "meter events must have strictly increasing positions";
        return false;
    }
    if (!isBarStart(position)) {
        error = "meter change at " + position.toString() +
                " is not on a bar boundary; mid-bar meter changes are unsupported";
        return false;
    }
    const std::int64_t barIndex = barIndexAt(position);
    if (barIndex <= events_.back().barIndex) {
        error = "meter change does not advance the bar index";
        return false;
    }
    MeterMapEvent event;
    event.position = position;
    event.meter = meter;
    event.grouping = std::move(normalized);
    event.barIndex = barIndex;
    events_.push_back(std::move(event));
    return true;
}

void MeterMap::add(const Position& position, const Meter& meter,
                   const MetricGrouping& grouping) {
    std::string error;
    if (!tryAdd(position, meter, grouping, error)) {
        throw Error(error, "meter");
    }
}

const MeterMapEvent& MeterMap::eventAt(const Position& position) const {
    return events_[eventIndexAt(position)];
}

std::size_t MeterMap::eventIndexAt(const Position& position) const {
    if (events_.size() == 1 || position.inWholeNotes().isNegative()) {
        return 0;
    }
    // First event with position >= `position`; an event exactly at the query
    // point is in effect from that point onward.
    std::size_t low = 0;
    std::size_t high = events_.size();
    while (low < high) {
        const std::size_t middle = (low + high) / 2;
        if (events_[middle].position < position) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low < events_.size() && events_[low].position == position) {
        return low;
    }
    return low == 0 ? 0 : low - 1;
}

const Meter& MeterMap::meterAt(const Position& position) const {
    return eventAt(position).meter;
}

const MetricGrouping& MeterMap::groupingAt(const Position& position) const {
    return eventAt(position).grouping;
}

Rational MeterMap::barLengthAt(const Position& position) const {
    return eventAt(position).meter.barLength();
}

bool MeterMap::isInRange(const Position& position) const {
    return !position.inWholeNotes().isNegative();
}

bool MeterMap::isBarStart(const Position& position) const {
    if (position.inWholeNotes().isNegative()) {
        return false;
    }
    const MeterMapEvent& event = eventAt(position);
    const Rational delta = position.inWholeNotes() - event.position.inWholeNotes();
    const Rational barLength = event.meter.barLength();
    const Rational quotient = delta / barLength;
    return quotient.isInteger();
}

bool MeterMap::isBeatStart(const Position& position) const {
    if (position.inWholeNotes().isNegative()) {
        return false;
    }
    return toMetric(position).beat.isInteger();
}

MeterMap::EventSearch MeterMap::locate(const Position& position) const {
    if (position.inWholeNotes().isNegative()) {
        throw Error("metric position must not be negative", "meter");
    }
    const MeterMapEvent& event = eventAt(position);
    const Rational delta = position.inWholeNotes() - event.position.inWholeNotes();
    const Rational barLength = event.meter.barLength();
    const Rational quotient = delta / barLength;
    const std::int64_t wholeBars = quotient.numerator() / quotient.denominator();
    EventSearch search;
    search.barIndex = event.barIndex + wholeBars;
    search.offsetInBar = delta - barLength * Rational(wholeBars, 1);
    search.event = &event;
    return search;
}

std::int64_t MeterMap::barIndexAt(const Position& position) const {
    return locate(position).barIndex;
}

Rational MeterMap::barOffsetAt(const Position& position) const {
    return locate(position).offsetInBar;
}

Position MeterMap::barStart(const Position& position) const {
    const EventSearch search = locate(position);
    return Position(search.event->position.inWholeNotes() +
                    search.event->meter.barLength() * Rational(search.barIndex - search.event->barIndex, 1));
}

Position MeterMap::positionOfBar(std::int64_t bar) const {
    if (bar < 0) {
        throw Error("bar index must not be negative", "meter");
    }
    // Last event whose barIndex <= bar.
    std::size_t low = 0;
    std::size_t high = events_.size();
    while (low < high) {
        const std::size_t middle = (low + high) / 2;
        if (events_[middle].barIndex <= bar) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    const std::size_t index = low == 0 ? 0 : low - 1;
    const MeterMapEvent& event = events_[index];
    return Position(event.position.inWholeNotes() +
                    event.meter.barLength() * Rational(bar - event.barIndex, 1));
}

MetricPosition MeterMap::toMetric(const Position& position) const {
    const EventSearch search = locate(position);
    MetricPosition metric;
    metric.bar = search.barIndex;
    metric.beat = search.offsetInBar * Rational(search.event->meter.denominator, 1);
    return metric;
}

Position MeterMap::toPosition(const MetricPosition& position) const {
    if (position.bar < 0) {
        throw Error("metric bar index must not be negative", "meter");
    }
    if (position.beat.isNegative()) {
        throw Error("metric beat must not be negative", "meter");
    }
    const Position bar = positionOfBar(position.bar);
    const Meter& meter = meterAt(bar);
    if (position.beat >= Rational(meter.numerator, 1)) {
        throw Error("metric beat " + position.beat.toString() + " is outside " +
                        meter.toString() + " bar " + std::to_string(position.bar),
                    "meter");
    }
    return Position(bar.inWholeNotes() + position.beat * meter.beatLength());
}

std::optional<Rational> MeterMap::resolveDuration(const Position& anchor,
                                                  const MetricDuration& duration,
                                                  std::string& error) const {
    if (anchor.inWholeNotes().isNegative()) {
        error = "metric anchor must not be negative";
        return std::nullopt;
    }
    switch (duration.unit) {
        case MetricUnit::Whole:
            return duration.amount;
        case MetricUnit::Bars: {
            if (duration.amount.isNegative()) {
                // Negative bar offsets are a linear metric displacement from the
                // anchor; walking backwards across meter changes has no unique
                // meaning, so the anchor's bar length is used explicitly.
                return duration.amount * barLengthAt(anchor);
            }
            // Every positive bar count is anchored at a bar boundary. Integer
            // counts walk real bar boundaries (so they span meter changes);
            // fractional counts scale the bar length in effect at the anchor.
            if (!isBarStart(anchor)) {
                error = "bars are anchored at " + anchor.toString() +
                        ", which is not a bar boundary";
                return std::nullopt;
            }
            if (!duration.amount.isInteger()) {
                return duration.amount * barLengthAt(anchor);
            }
            const std::int64_t bars = duration.amount.numerator();
            const std::int64_t startBar = barIndexAt(anchor);
            if (bars > std::numeric_limits<std::int64_t>::max() - startBar) {
                error = "bar index overflow while resolving metric duration";
                return std::nullopt;
            }
            const Position end = positionOfBar(startBar + bars);
            return (end - anchor).inWholeNotes();
        }
        case MetricUnit::Beats:
            return duration.amount * meterAt(anchor).beatLength();
    }
    error = "unknown metric unit";
    return std::nullopt;
}

std::optional<Position> MeterMap::resolveEnd(const Position& anchor,
                                             const MetricDuration& duration,
                                             std::string& error) const {
    const std::optional<Rational> resolved = resolveDuration(anchor, duration, error);
    if (!resolved.has_value()) {
        return std::nullopt;
    }
    return Position(anchor.inWholeNotes() + *resolved);
}

MetricWeight MeterMap::metricWeight(const Position& position) const {
    const MetricPosition metric = toMetric(position);
    const MetricGrouping& grouping = groupingAt(position);
    const std::int64_t beatIndex = floorPositive(metric.beat);
    const Rational unitBeat(beatIndex, 1);
    MetricWeight weight;
    weight.beatIndex = static_cast<int>(beatIndex);
    weight.subdivision = metric.beat - unitBeat;
    weight.groupIndex = grouping.groupAtUnit(weight.beatIndex);
    const int groupStart = grouping.groupStartUnit(weight.groupIndex);
    weight.barStart = metric.beat.isZero();
    weight.groupStart = !weight.barStart && groupStart >= 0 && metric.beat == Rational(groupStart, 1);
    weight.beatStart = weight.subdivision.isZero();
    if (weight.barStart) {
        weight.level = 3;
    } else if (weight.groupStart) {
        weight.level = 2;
    } else if (weight.beatStart) {
        weight.level = 1;
    } else {
        weight.level = 0;
    }
    weight.weight = static_cast<double>(weight.level) / 3.0;
    return weight;
}

bool MeterMap::validate(std::string& error) const {
    if (events_.empty()) {
        error = "meter map has no events";
        return false;
    }
    if (!events_.front().position.inWholeNotes().isZero()) {
        error = "meter map must start at position zero";
        return false;
    }
    MeterMap rebuilt(events_.front().meter, events_.front().grouping);
    if (events_.front().barIndex != 0) {
        error = "initial meter event has a non-zero bar index";
        return false;
    }
    for (std::size_t index = 1; index < events_.size(); ++index) {
        const MeterMapEvent& event = events_[index];
        std::string local;
        if (!rebuilt.tryAdd(event.position, event.meter, event.grouping, local)) {
            error = "meter event #" + std::to_string(index) + ": " + local;
            return false;
        }
        if (rebuilt.events().back().barIndex != event.barIndex) {
            error = "meter event #" + std::to_string(index) + " has an inconsistent bar index";
            return false;
        }
    }
    return true;
}

std::string MeterMap::describe() const {
    std::string text;
    for (const MeterMapEvent& event : events_) {
        if (!text.empty()) {
            text += "; ";
        }
        text += event.position.toString() + " " + event.meter.toString();
        if (event.grouping.explicitGrouping) {
            text += " (" + event.grouping.toString() + ")";
        }
    }
    return text;
}

}  // namespace mpl
