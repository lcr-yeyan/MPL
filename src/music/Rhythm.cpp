#include "mpl/music/Rhythm.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>

namespace mpl {
namespace {

Rational positiveModulo(const Rational& value, const Rational& period) {
    if (!period.isPositive()) {
        throw Error("rhythm period must be positive", "rhythm");
    }
    const Rational quotient = value / period;
    std::int64_t whole = quotient.numerator() / quotient.denominator();
    if (quotient.numerator() % quotient.denominator() != 0 && quotient.isNegative()) {
        --whole;
    }
    Rational result = value - period * Rational(whole, 1);
    if (result.isNegative()) {
        result += period;
    }
    return result;
}

std::int64_t floorDivision(const Rational& value) {
    std::int64_t whole = value.numerator() / value.denominator();
    if (value.numerator() % value.denominator() != 0 && value.isNegative()) {
        --whole;
    }
    return whole;
}

/// Nearest grid multiple, ties toward positive infinity. Shared by quantize and
/// by grid-mode onset relations so both use one quantization rule.
std::int64_t nearestGridBucket(const Rational& value, const Rational& grid) {
    const Rational scaled = value / grid;
    const std::int64_t floor = floorDivision(scaled);
    const Rational fraction = scaled - Rational(floor, 1);
    return fraction >= Rational(1, 2) ? floor + 1 : floor;
}

void requirePositivePeriod(const Rational& period) {
    if (!period.isPositive()) {
        throw Error("rhythm logical length must be positive, got " + period.toString(),
                    "rhythm");
    }
}

void requireEventBudget(std::size_t count) {
    if (count > kMaxRhythmEvents) {
        throw Error("rhythm event count " + std::to_string(count) + " exceeds limit " +
                        std::to_string(kMaxRhythmEvents),
                    "rhythm-event-limit");
    }
}

void requireValid(const Rhythm& rhythm) {
    std::string error;
    if (!rhythm.isValid(&error)) {
        throw Error(error, "rhythm");
    }
}

std::vector<RhythmEvent> sortedEvents(std::vector<RhythmEvent> events) {
    std::stable_sort(events.begin(), events.end(),
                     [](const RhythmEvent& lhs, const RhythmEvent& rhs) {
                         return lhs.onset < rhs.onset;
                     });
    return events;
}

using OnsetMap = std::map<Rational, RhythmEvent>;

OnsetMap attacksByOnset(const Rhythm& rhythm, const Rational& period) {
    OnsetMap result;
    for (const RhythmEvent& event : rhythm.events()) {
        if (!isRhythmAttack(event)) {
            continue;
        }
        const Rational key = positiveModulo(event.onset, period);
        if (result.find(key) == result.end()) {
            result.emplace(key, event);
        }
    }
    return result;
}

OnsetMap attacksByBucket(const Rhythm& rhythm, const Rational& period,
                         const Rational& grid) {
    OnsetMap result;
    for (const RhythmEvent& event : rhythm.events()) {
        if (!isRhythmAttack(event)) {
            continue;
        }
        const Rational normalized = positiveModulo(event.onset, period);
        const std::int64_t bucket = nearestGridBucket(normalized, grid);
        const Rational key = positiveModulo(Rational(bucket, 1) * grid, period);
        if (result.find(key) == result.end()) {
            result.emplace(key, event);
        }
    }
    return result;
}

std::vector<RhythmEvent> eventsFromKeys(const std::vector<Rational>& keys,
                                        const OnsetMap& primary, const OnsetMap& secondary,
                                        bool gridMode, const Rational& grid) {
    std::vector<RhythmEvent> events;
    events.reserve(keys.size());
    for (const Rational& key : keys) {
        RhythmEvent event;
        event.onset = key;
        event.duration = gridMode ? grid : Rational(1, 8);
        event.rest = false;
        const auto primaryIt = primary.find(key);
        if (primaryIt != primary.end()) {
            event.duration = gridMode ? grid : primaryIt->second.duration;
            event.accent = primaryIt->second.accent;
            event.origin = primaryIt->second.origin;
        } else {
            const auto secondaryIt = secondary.find(key);
            if (secondaryIt != secondary.end()) {
                event.duration = gridMode ? grid : secondaryIt->second.duration;
                event.accent = secondaryIt->second.accent;
                event.origin = secondaryIt->second.origin;
            }
        }
        events.push_back(event);
    }
    requireEventBudget(events.size());
    return events;
}

std::vector<Rational> sortedKeys(const OnsetMap& primary, const OnsetMap& secondary,
                                 int mode) {
    // mode 0 = intersection, 1 = union, 2 = difference.
    std::vector<Rational> keys;
    if (mode == 0) {
        for (const auto& entry : primary) {
            if (secondary.find(entry.first) != secondary.end()) {
                keys.push_back(entry.first);
            }
        }
    } else if (mode == 1) {
        for (const auto& entry : primary) {
            keys.push_back(entry.first);
        }
        for (const auto& entry : secondary) {
            if (primary.find(entry.first) == primary.end()) {
                keys.push_back(entry.first);
            }
        }
        std::sort(keys.begin(), keys.end());
    } else {
        for (const auto& entry : primary) {
            if (secondary.find(entry.first) == secondary.end()) {
                keys.push_back(entry.first);
            }
        }
    }
    return keys;
}

Rhythm relation(const Rhythm& lhs, const Rhythm& rhs, const std::optional<Rational>& grid,
                int mode) {
    requirePositivePeriod(lhs.logicalLength());
    requirePositivePeriod(rhs.logicalLength());
    const std::optional<Rational> common = rhythmCommonPeriod(lhs, rhs);
    if (!common.has_value()) {
        throw Error("common period of " + lhs.logicalLength().toString() + " and " +
                        rhs.logicalLength().toString() + " exceeds the rational limit",
                    "rhythm-period-overflow");
    }
    OnsetMap primary;
    OnsetMap secondary;
    if (grid.has_value()) {
        if (!grid->isPositive()) {
            throw Error("relation grid must be positive", "rhythm");
        }
        primary = attacksByBucket(lhs, *common, *grid);
        secondary = attacksByBucket(rhs, *common, *grid);
    } else {
        primary = attacksByOnset(lhs, *common);
        secondary = attacksByOnset(rhs, *common);
    }
    const std::vector<Rational> keys = sortedKeys(primary, secondary, mode);
    return Rhythm::fromEvents(eventsFromKeys(keys, primary, secondary, grid.has_value(),
                                              grid.value_or(Rational(0, 1))),
                              *common);
}

}  // namespace

Rhythm Rhythm::fromEvents(std::vector<RhythmEvent> events, const Rational& logicalLength) {
    Rhythm rhythm;
    rhythm.events_ = sortedEvents(std::move(events));
    rhythm.logicalLength_ = logicalLength;
    requireEventBudget(rhythm.events_.size());
    std::string error;
    if (!rhythm.isValid(&error)) {
        throw Error(error, "rhythm");
    }
    return rhythm;
}

std::size_t Rhythm::attackCount() const {
    return static_cast<std::size_t>(
        std::count_if(events_.begin(), events_.end(),
                      [](const RhythmEvent& event) { return isRhythmAttack(event); }));
}

bool Rhythm::hasRests() const {
    return std::any_of(events_.begin(), events_.end(),
                       [](const RhythmEvent& event) { return event.rest; });
}

Rational Rhythm::contentEnd() const {
    Rational end(0, 1);
    for (const RhythmEvent& event : events_) {
        end = std::max(end, event.onset + event.duration);
    }
    return end;
}

bool Rhythm::isValid(std::string* error) const {
    const auto fail = [&](std::string message) {
        if (error != nullptr) {
            *error = std::move(message);
        }
        return false;
    };
    if (logicalLength_.isNegative()) {
        return fail("rhythm logical length must not be negative");
    }
    if (logicalLength_.isZero() && !events_.empty()) {
        return fail("zero-length rhythm must not contain events");
    }
    if (events_.size() > kMaxRhythmEvents) {
        return fail("rhythm event count exceeds limit");
    }
    for (const RhythmEvent& event : events_) {
        if (!event.duration.isPositive()) {
            return fail("rhythm event duration must be positive");
        }
        if (event.rest && event.continuation) {
            return fail("a rest event cannot be a split continuation");
        }
        if (event.accent < 0 || event.accent > kMaxAccentLevel) {
            return fail("rhythm accent must be in [0, " + std::to_string(kMaxAccentLevel) + "]");
        }
    }
    // A continuation is a split segment of a sounding event; it must be
    // contiguous with a predecessor that ends exactly at its onset. At the
    // cycle start the predecessor may end at the logical length (the wrapped
    // tail of the previous cycle).
    for (const RhythmEvent& event : events_) {
        if (!event.continuation) {
            continue;
        }
        bool hasPredecessor = false;
        for (const RhythmEvent& candidate : events_) {
            if (candidate.rest || &candidate == &event) {
                continue;
            }
            const Rational end = candidate.onset + candidate.duration;
            const bool contiguous = candidate.onset < event.onset && end == event.onset;
            const bool wrapped = event.onset.isZero() && end == logicalLength_;
            if (contiguous || wrapped) {
                hasPredecessor = true;
                break;
            }
        }
        if (!hasPredecessor) {
            return fail("rhythm continuation at " + event.onset.toString() +
                        " has no preceding sounding event");
        }
    }
    return true;
}

Rhythm Rhythm::withOriginFallback(ObjectId id) const {
    Rhythm copy = *this;
    for (RhythmEvent& event : copy.events_) {
        if (event.origin == kInvalidObjectId) {
            event.origin = id;
        }
    }
    return copy;
}

std::string Rhythm::describe() const {
    std::string text = "len=" + logicalLength_.toString();
    for (const RhythmEvent& event : events_) {
        text += " ";
        text += event.rest ? "rest@" : "hit@";
        text += event.onset.toString();
        text += ":" + event.duration.toString();
        if (event.continuation) {
            text += "+cont";
        }
        if (!event.rest) {
            text += "a" + std::to_string(event.accent);
        }
    }
    return text;
}

Rhythm rhythmSequence(const std::vector<Rhythm>& parts) {
    if (parts.empty()) {
        throw Error("rhythm sequence requires at least one part", "rhythm");
    }
    std::vector<RhythmEvent> events;
    Rational cursor(0, 1);
    for (const Rhythm& part : parts) {
        requireValid(part);
        for (const RhythmEvent& event : part.events()) {
            RhythmEvent shifted = event;
            shifted.onset = event.onset + cursor;
            events.push_back(std::move(shifted));
        }
        cursor += part.logicalLength();
    }
    return Rhythm::fromEvents(std::move(events), cursor);
}

Rhythm rhythmOverlay(const std::vector<Rhythm>& parts) {
    if (parts.empty()) {
        throw Error("rhythm overlay requires at least one part", "rhythm");
    }
    std::vector<RhythmEvent> events;
    Rational length(0, 1);
    for (const Rhythm& part : parts) {
        requireValid(part);
        length = std::max(length, part.logicalLength());
        events.insert(events.end(), part.events().begin(), part.events().end());
    }
    return Rhythm::fromEvents(std::move(events), length);
}

Rhythm rhythmRepeat(const Rhythm& input, int count) {
    requireValid(input);
    if (count < 1 || count > kMaxRhythmRepeat) {
        throw Error("rhythm repeat count must be in [1, " +
                        std::to_string(kMaxRhythmRepeat) + "], got " + std::to_string(count),
                    "rhythm");
    }
    if (count == 1) {
        return input;
    }
    requireEventBudget(input.events().size() * static_cast<std::size_t>(count));
    std::vector<RhythmEvent> events;
    events.reserve(input.events().size() * static_cast<std::size_t>(count));
    for (int copy = 0; copy < count; ++copy) {
        const Rational shift = input.logicalLength() * Rational(copy, 1);
        for (const RhythmEvent& event : input.events()) {
            RhythmEvent shifted = event;
            shifted.onset = event.onset + shift;
            events.push_back(std::move(shifted));
        }
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength() * Rational(count, 1));
}

Rhythm rhythmRotate(const Rhythm& input, const Rational& amount) {
    requireValid(input);
    const Rational period = input.logicalLength();
    requirePositivePeriod(period);
    const Rational shift = positiveModulo(amount, period);
    std::vector<RhythmEvent> events;
    for (const RhythmEvent& event : input.events()) {
        const Rational start = positiveModulo(event.onset + shift, period);
        Rational remaining = event.duration;
        Rational cursor = start;
        int segments = 0;
        while (remaining.isPositive()) {
            if (++segments > 256) {
                throw Error("rhythm rotate split exceeded 256 segments for one event",
                            "rhythm-event-limit");
            }
            const Rational room = period - cursor;
            const Rational length = remaining < room ? remaining : room;
            RhythmEvent segment = event;
            segment.onset = cursor;
            segment.duration = length;
            // The first segment keeps the source attack status; later segments
            // are continuations of the same sounding event. Splitting therefore
            // never silently creates a retrigger.
            segment.continuation = event.continuation || segments > 1;
            events.push_back(std::move(segment));
            requireEventBudget(events.size());
            remaining -= length;
            cursor = Rational(0, 1);
        }
        if (segments == 0) {
            events.push_back(event);
        }
    }
    return Rhythm::fromEvents(std::move(events), period);
}

Rhythm rhythmShift(const Rhythm& input, const Rational& amount) {
    requireValid(input);
    std::vector<RhythmEvent> events = input.events();
    for (RhythmEvent& event : events) {
        event.onset += amount;
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength());
}

Rhythm rhythmStretch(const Rhythm& input, const Rational& factor) {
    requireValid(input);
    if (!factor.isPositive()) {
        throw Error("rhythm stretch factor must be positive, got " + factor.toString(),
                    "rhythm");
    }
    std::vector<RhythmEvent> events;
    events.reserve(input.events().size());
    for (const RhythmEvent& event : input.events()) {
        RhythmEvent stretched = event;
        stretched.onset = event.onset * factor;
        stretched.duration = event.duration * factor;
        events.push_back(std::move(stretched));
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength() * factor);
}

Rhythm rhythmSubdivide(const Rhythm& input, int parts) {
    requireValid(input);
    if (parts < 1 || parts > kMaxRhythmSubdivision) {
        throw Error("rhythm subdivide parts must be in [1, " +
                        std::to_string(kMaxRhythmSubdivision) + "], got " +
                        std::to_string(parts),
                    "rhythm");
    }
    if (parts == 1) {
        return input;
    }
    requireEventBudget(input.events().size() * static_cast<std::size_t>(parts));
    std::vector<RhythmEvent> events;
    events.reserve(input.events().size() * static_cast<std::size_t>(parts));
    for (const RhythmEvent& event : input.events()) {
        const Rational piece = event.duration / Rational(parts, 1);
        for (int index = 0; index < parts; ++index) {
            RhythmEvent split = event;
            split.onset = event.onset + piece * Rational(index, 1);
            split.duration = piece;
            // subdivide is a duration operation: its pieces are fresh attacks.
            split.continuation = false;
            events.push_back(std::move(split));
        }
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength());
}

Rhythm rhythmAccent(const Rhythm& input, const std::vector<int>& levels) {
    requireValid(input);
    if (levels.empty()) {
        throw Error("rhythm accent requires at least one level", "rhythm");
    }
    for (const int level : levels) {
        if (level < 0 || level > kMaxAccentLevel) {
            throw Error("rhythm accent level must be in [0, " +
                            std::to_string(kMaxAccentLevel) + "]",
                        "rhythm");
        }
    }
    std::vector<RhythmEvent> events = input.events();
    std::size_t attack = 0;
    for (RhythmEvent& event : events) {
        if (!isRhythmAttack(event)) {
            continue;
        }
        event.accent = levels[attack % levels.size()];
        ++attack;
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength());
}

Rhythm rhythmRealign(const Rhythm& input, const Rational& targetPeriod) {
    requireValid(input);
    requirePositivePeriod(input.logicalLength());
    if (!targetPeriod.isPositive()) {
        throw Error("rhythm realign target must be positive", "rhythm");
    }
    const std::optional<std::int64_t> multiple =
        integerQuotient(targetPeriod, input.logicalLength());
    if (!multiple.has_value() || *multiple < 1) {
        throw Error("rhythm realign target " + targetPeriod.toString() +
                        " is not an integer multiple of period " +
                        input.logicalLength().toString(),
                    "rhythm-realign");
    }
    if (*multiple > kMaxRhythmRepeat) {
        throw Error("rhythm realign repeats " + std::to_string(*multiple) + " exceed limit " +
                        std::to_string(kMaxRhythmRepeat),
                    "rhythm-event-limit");
    }
    return rhythmRepeat(input, static_cast<int>(*multiple));
}

Rhythm rhythmQuantize(const Rhythm& input, const Rational& grid) {
    requireValid(input);
    if (!grid.isPositive()) {
        throw Error("rhythm quantize grid must be positive", "rhythm");
    }
    std::vector<RhythmEvent> events = input.events();
    for (RhythmEvent& event : events) {
        const std::int64_t rounded = nearestGridBucket(event.onset, grid);
        event.onset = Rational(rounded, 1) * grid;
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength());
}

Rhythm rhythmAccentByMeter(const Rhythm& input, const MeterMap& map,
                           const Position& anchor) {    requireValid(input);
    std::vector<RhythmEvent> events = input.events();
    for (RhythmEvent& event : events) {
        if (!isRhythmAttack(event)) {
            continue;
        }
        const Position absolute(anchor.inWholeNotes() + event.onset);
        event.accent = absolute.inWholeNotes().isNegative()
                           ? 0
                           : map.metricWeight(absolute).level;
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength());
}

Rhythm rhythmRetrigger(const Rhythm& input) {
    requireValid(input);
    std::vector<RhythmEvent> events = input.events();
    for (RhythmEvent& event : events) {
        event.continuation = false;
    }
    return Rhythm::fromEvents(std::move(events), input.logicalLength());
}

Rhythm rhythmIntersect(const Rhythm& lhs, const Rhythm& rhs,
                       const std::optional<Rational>& grid) {
    return relation(lhs, rhs, grid, 0);
}

Rhythm rhythmUnion(const Rhythm& lhs, const Rhythm& rhs,
                   const std::optional<Rational>& grid) {
    return relation(lhs, rhs, grid, 1);
}

Rhythm rhythmDifference(const Rhythm& lhs, const Rhythm& rhs,
                        const std::optional<Rational>& grid) {
    return relation(lhs, rhs, grid, 2);
}

Rhythm rhythmComplement(const Rhythm& input, const Rational& span, const Rational& grid) {
    requireValid(input);
    if (!span.isPositive()) {
        throw Error("rhythm complement span must be positive", "rhythm");
    }
    if (!grid.isPositive()) {
        throw Error("rhythm complement grid must be positive", "rhythm");
    }
    const std::optional<std::int64_t> count = integerQuotient(span, grid);
    if (!count.has_value() || *count < 1) {
        throw Error("rhythm complement span must be an integer multiple of the grid",
                    "rhythm");
    }
    if (*count > static_cast<std::int64_t>(kMaxRhythmEvents)) {
        throw Error("rhythm complement grid point count exceeds limit", "rhythm-event-limit");
    }
    OnsetMap onsets;
    for (const RhythmEvent& event : input.events()) {
        if (isRhythmAttack(event)) {
            onsets.emplace(event.onset, event);
        }
    }
    std::vector<RhythmEvent> events;
    for (std::int64_t index = 0; index < *count; ++index) {
        const Rational onset = Rational(index, 1) * grid;
        if (onsets.find(onset) != onsets.end()) {
            continue;
        }
        RhythmEvent event;
        event.onset = onset;
        event.duration = grid;
        event.rest = false;
        event.accent = 0;
        events.push_back(std::move(event));
    }
    return Rhythm::fromEvents(std::move(events), span);
}

Rhythm rhythmAccentFollow(const Rhythm& source, const Rhythm& target,
                          const std::optional<Rational>& grid) {
    requireValid(source);
    requireValid(target);
    OnsetMap accents;
    if (grid.has_value()) {
        if (!grid->isPositive()) {
            throw Error("accent-follow grid must be positive", "rhythm");
        }
        for (const RhythmEvent& event : source.events()) {
            if (!isRhythmAttack(event)) {
                continue;
            }
            const std::int64_t bucket = nearestGridBucket(event.onset, *grid);
            const Rational key = Rational(bucket, 1) * *grid;
            if (accents.find(key) == accents.end()) {
                accents.emplace(key, event);
            }
        }
    } else {
        for (const RhythmEvent& event : source.events()) {
            if (!isRhythmAttack(event)) {
                continue;
            }
            if (accents.find(event.onset) == accents.end()) {
                accents.emplace(event.onset, event);
            }
        }
    }
    std::vector<RhythmEvent> events = target.events();
    for (RhythmEvent& event : events) {
        if (!isRhythmAttack(event)) {
            continue;
        }
        Rational key = event.onset;
        if (grid.has_value()) {
            key = Rational(nearestGridBucket(event.onset, *grid), 1) * *grid;
        }
        const auto found = accents.find(key);
        event.accent = found == accents.end() ? 0 : found->second.accent;
    }
    return Rhythm::fromEvents(std::move(events), target.logicalLength());
}

std::optional<Rational> rhythmCommonPeriod(const Rhythm& lhs, const Rhythm& rhs) {
    if (!lhs.logicalLength().isPositive() || !rhs.logicalLength().isPositive()) {
        return std::nullopt;
    }
    return rationalLcm(lhs.logicalLength(), rhs.logicalLength());
}

}  // namespace mpl
