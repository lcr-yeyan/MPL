#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/Rational.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/music/Chord.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace mpl {

/// One harmonic event. Events cover [start, start + duration). Overlaps and
/// gaps are both legal; queries make the resulting behavior explicit.
struct HarmonyEvent {
    Rational start{0, 1};
    Rational duration{0, 1};
    ChordSymbol chord;
    ObjectId origin = kInvalidObjectId;
};

/// A serializable harmonic timeline. It is independent of rhythm and pitch
/// register; voicings are derived later with an explicit range and strategy.
class HarmonyTimeline {
public:
    HarmonyTimeline() = default;

    static HarmonyTimeline fromEvents(std::vector<HarmonyEvent> events,
                                      const Rational& logicalLength);

    const std::vector<HarmonyEvent>& events() const noexcept { return events_; }
    const Rational& logicalLength() const noexcept { return logicalLength_; }

    bool empty() const noexcept { return events_.empty(); }
    std::size_t size() const noexcept { return events_.size(); }
    Rational contentEnd() const;

    /// Right-continuous query: among all events covering `position`, returns the
    /// one with the greatest start; ties are resolved by the later declaration
    /// (stable sort index). Returns nullptr in a gap.
    const HarmonyEvent* eventAt(const Position& position) const;

    /// All events covering `position`, sorted by start (and declaration order).
    std::vector<const HarmonyEvent*> eventsAt(const Position& position) const;

    bool isValid(std::string* error = nullptr) const;
    std::string describe() const;

    /// Returns a copy where events without an origin inherit `origin`.
    HarmonyTimeline withOriginFallback(ObjectId origin) const;

private:
    std::vector<HarmonyEvent> events_;
    Rational logicalLength_{0, 1};
};

}  // namespace mpl
