#include "mpl/music/HarmonyTimeline.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <utility>

namespace mpl {

HarmonyTimeline HarmonyTimeline::fromEvents(std::vector<HarmonyEvent> events,
                                            const Rational& logicalLength) {
    std::stable_sort(events.begin(), events.end(),
                     [](const HarmonyEvent& lhs, const HarmonyEvent& rhs) {
                         return lhs.start < rhs.start;
                     });
    HarmonyTimeline timeline;
    timeline.events_ = std::move(events);
    timeline.logicalLength_ = logicalLength;
    std::string error;
    if (!timeline.isValid(&error)) {
        throw Error(error, "harmony-timeline");
    }
    return timeline;
}

Rational HarmonyTimeline::contentEnd() const {
    Rational end(0, 1);
    for (const HarmonyEvent& event : events_) {
        end = std::max(end, event.start + event.duration);
    }
    return end;
}

const HarmonyEvent* HarmonyTimeline::eventAt(const Position& position) const {
    const Rational time = position.inWholeNotes();
    if (time.isNegative()) {
        return nullptr;
    }
    const HarmonyEvent* best = nullptr;
    for (const HarmonyEvent& event : events_) {
        if (event.start <= time && time < event.start + event.duration) {
            if (best == nullptr || !(event.start < best->start)) {
                best = &event;
            }
        }
    }
    return best;
}

std::vector<const HarmonyEvent*> HarmonyTimeline::eventsAt(const Position& position) const {
    std::vector<const HarmonyEvent*> result;
    const Rational time = position.inWholeNotes();
    if (time.isNegative()) {
        return result;
    }
    for (const HarmonyEvent& event : events_) {
        if (event.start <= time && time < event.start + event.duration) {
            result.push_back(&event);
        }
    }
    return result;
}

bool HarmonyTimeline::isValid(std::string* error) const {
    const auto fail = [&](std::string message) {
        if (error != nullptr) {
            *error = std::move(message);
        }
        return false;
    };
    if (logicalLength_.isNegative()) {
        return fail("harmony logical length must not be negative");
    }
    if (logicalLength_.isZero() && !events_.empty()) {
        return fail("zero-length harmony timeline must not contain events");
    }
    Rational previous(0, 1);
    bool first = true;
    for (const HarmonyEvent& event : events_) {
        if (event.start.isNegative()) {
            return fail("harmony event start must not be negative");
        }
        if (!event.duration.isPositive()) {
            return fail("harmony event duration must be positive");
        }
        if (!event.chord.valid()) {
            return fail("harmony event has an invalid chord");
        }
        if (event.start + event.duration > logicalLength_) {
            return fail("harmony event extends past the logical length");
        }
        if (!first && event.start < previous) {
            return fail("harmony events must be sorted by start");
        }
        previous = event.start;
        first = false;
    }
    return true;
}

std::string HarmonyTimeline::describe() const {
    std::string text = "len=" + logicalLength_.toString();
    for (const HarmonyEvent& event : events_) {
        text += " " + event.chord.name() + "@" + event.start.toString() + ":" +
                event.duration.toString();
    }
    return text;
}

HarmonyTimeline HarmonyTimeline::withOriginFallback(ObjectId origin) const {
    HarmonyTimeline copy = *this;
    for (HarmonyEvent& event : copy.events_) {
        if (event.origin == kInvalidObjectId) {
            event.origin = origin;
        }
    }
    return copy;
}

}  // namespace mpl
