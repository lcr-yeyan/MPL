#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/MeterMap.hpp"
#include "mpl/core/Rational.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace mpl {

/// Explicit capacity contract for rhythmic algebra. Every operator checks these
/// before allocating so a bad period/ratio cannot exhaust memory.
inline constexpr std::size_t kMaxRhythmEvents = 4096;
inline constexpr int kMaxRhythmRepeat = 256;
inline constexpr int kMaxRhythmSubdivision = 64;
inline constexpr int kMaxAccentLevel = 8;

/// One rhythmic event. `rest` events keep explicit silence in the structure
/// (they never consume a pitch). Onsets are relative to the rhythm's own
/// origin; they may be negative after shift() and may extend past the logical
/// length for sustained tails. Overlap is allowed and preserved.
struct RhythmEvent {
    Rational onset{0, 1};
    Rational duration{0, 1};
    int accent = 0;
    bool rest = false;
    ObjectId origin = kInvalidObjectId;
    /// True when this event is the continuation of an event split at a period
    /// boundary. Continuations are not attacks: rhythmCombine does not
    /// retrigger them. retrigger() clears the flag explicitly.
    bool continuation = false;
};

/// True when the event is a sounding attack (not a rest, not a split
/// continuation). Only attacks consume pitches and participate in onset sets.
inline bool isRhythmAttack(const RhythmEvent& event) {
    return !event.rest && !event.continuation;
}

/// A pitch-independent, instrument-independent, serializable rhythm value.
/// `logicalLength` is the period used by repeat/rotate/overlay; content may be
/// shorter (logical gap) or longer (sustain). Notes are produced only when a
/// rhythm is combined with a pitch sequence.
class Rhythm {
public:
    Rhythm() = default;

    static Rhythm fromEvents(std::vector<RhythmEvent> events, const Rational& logicalLength);

    const std::vector<RhythmEvent>& events() const noexcept { return events_; }
    const Rational& logicalLength() const noexcept { return logicalLength_; }

    bool empty() const noexcept { return events_.empty(); }
    std::size_t eventCount() const noexcept { return events_.size(); }
    std::size_t attackCount() const;
    bool hasRests() const;

    /// End of the last sounding or rest event, not the period.
    Rational contentEnd() const;

    bool isValid(std::string* error = nullptr) const;

    /// Returns a copy where events without an origin inherit `id`. Used by the
    /// Music IR to mark synthesized events (for example complement grid points).
    Rhythm withOriginFallback(ObjectId id) const;

    /// Serializable ledger, used by trace and tests.
    std::string describe() const;

private:
    std::vector<RhythmEvent> events_;
    Rational logicalLength_{0, 1};
};

/// Lays rhythms end to end. Logical length is the sum of the parts.
Rhythm rhythmSequence(const std::vector<Rhythm>& parts);

/// Aligns rhythms at zero. Logical length is the maximum; overlaps are kept.
Rhythm rhythmOverlay(const std::vector<Rhythm>& parts);

/// Concatenates `count` copies. count must be in [1, kMaxRhythmRepeat].
Rhythm rhythmRepeat(const Rhythm& input, int count);

/// Cyclic phase shift by `amount` modulo the logical length. Events crossing
/// the period boundary are split into exact segments. Negative amounts are
/// normalized into [0, length).
Rhythm rhythmRotate(const Rhythm& input, const Rational& amount);

/// Linear shift; negative amounts may move onsets before zero. No wrapping.
Rhythm rhythmShift(const Rhythm& input, const Rational& amount);

/// Scales onsets, durations and logical length by a positive factor.
Rhythm rhythmStretch(const Rhythm& input, const Rational& factor);

/// Splits every event (including rests) into `parts` equal pieces. Accents are
/// copied unchanged: this is a duration operation, not an accent rule.
Rhythm rhythmSubdivide(const Rhythm& input, int parts);

/// Assigns accent levels to attack events in onset order, cycling `levels`.
/// Rest events are unchanged. Levels must be in [0, kMaxAccentLevel].
Rhythm rhythmAccent(const Rhythm& input, const std::vector<int>& levels);

/// Extends a rhythm to `targetPeriod`, which must be an integer multiple of its
/// logical length. This is the exact realignment primitive for polymeter.
Rhythm rhythmRealign(const Rhythm& input, const Rational& targetPeriod);

/// Snaps onsets to the nearest multiple of `grid` (ties toward positive
/// infinity). Durations and logical length are unchanged; collisions are kept.
Rhythm rhythmQuantize(const Rhythm& input, const Rational& grid);

/// Accents every attack by the metric hierarchy level (3 bar, 2 group, 1 beat,
/// 0 subdivision) at `anchor + onset`.
Rhythm rhythmAccentByMeter(const Rhythm& input, const MeterMap& map, const Position& anchor);

/// Clears split-continuation flags: every sounding segment becomes a fresh
/// attack (an explicit retrigger). Used to opt out of the default
/// split-without-retrigger boundary policy.
Rhythm rhythmRetrigger(const Rhythm& input);

/// Onset-set relation. With a grid, onsets are bucketed to grid points and the
/// result uses grid-length events; without a grid, exact rational onsets are
/// compared and source durations are copied. All three use the rational common
/// period of the two rhythms as the result's logical length.
Rhythm rhythmIntersect(const Rhythm& lhs, const Rhythm& rhs,
                       const std::optional<Rational>& grid = std::nullopt);
Rhythm rhythmUnion(const Rhythm& lhs, const Rhythm& rhs,
                   const std::optional<Rational>& grid = std::nullopt);
Rhythm rhythmDifference(const Rhythm& lhs, const Rhythm& rhs,
                        const std::optional<Rational>& grid = std::nullopt);

/// Grid points in [0, span) that are not onsets of `input`. span and grid must
/// be positive and span must be an integer multiple of grid.
Rhythm rhythmComplement(const Rhythm& input, const Rational& span, const Rational& grid);

/// Copies `target`, replacing each attack accent with the accent of a `source`
/// event at the same onset (exact or grid bucket), or 0 when none matches.
Rhythm rhythmAccentFollow(const Rhythm& source, const Rhythm& target,
                          const std::optional<Rational>& grid = std::nullopt);

/// Least positive common period of two rhythms. nullopt on overflow.
std::optional<Rational> rhythmCommonPeriod(const Rhythm& lhs, const Rhythm& rhs);

/// Removes onsets after quantization? Not provided: no implicit merging.

}  // namespace mpl
