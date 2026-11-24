#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace mpl {

/// Diatonic letter name. Alterations are stored separately so that enharmonic
/// spelling such as Ab4 versus G#4 survives round trips through the IR.
enum class Step : std::uint8_t { C, D, E, F, G, A, B };

/// A pitch interval measured in semitones. Scale-degree intervals arrive with
/// the harmony library; V0.0.1 only needs the chromatic distance.
struct Interval {
    int semitones = 0;

    static Interval fromSemitones(int value) { return Interval{value}; }
    static Interval octaves(int count) { return Interval{count * 12}; }

    Interval inverted() const { return Interval{-semitones}; }
    Interval operator+(const Interval& rhs) const { return Interval{semitones + rhs.semitones}; }
    Interval operator-() const { return Interval{-semitones}; }

    friend bool operator==(const Interval& lhs, const Interval& rhs) {
        return lhs.semitones == rhs.semitones;
    }
    friend bool operator!=(const Interval& lhs, const Interval& rhs) { return !(lhs == rhs); }
};

std::ostream& operator<<(std::ostream& os, const Interval& interval);

/// A spelled pitch. The MIDI note number is derived from spelling, so the
/// pitch remains readable in the Music IR and in exported artifacts.
class Pitch {
public:
    constexpr Pitch() = default;
    constexpr Pitch(Step step, int alter, int octave)
        : step_(step), alter_(alter), octave_(octave) {
        midi_ = computeMidi();
    }

    /// Builds a pitch from a MIDI note number, choosing sharp or flat spelling.
    static Pitch fromMidi(int midi, bool preferFlats = false);

    /// Parses scientific pitch notation such as "F4", "Ab4", "C#5", "C-1".
    static Pitch parse(std::string_view name);

    Step step() const noexcept { return step_; }
    int alter() const noexcept { return alter_; }
    int octave() const noexcept { return octave_; }
    int midi() const noexcept { return midi_; }

    std::string name() const;
    Pitch transposed(Interval interval) const;

    friend bool operator==(const Pitch& lhs, const Pitch& rhs) { return lhs.midi_ == rhs.midi_; }
    friend bool operator!=(const Pitch& lhs, const Pitch& rhs) { return !(lhs == rhs); }
    friend bool operator<(const Pitch& lhs, const Pitch& rhs) { return lhs.midi_ < rhs.midi_; }

private:
    constexpr int computeMidi() const {
        constexpr int kStepSemitones[7] = {0, 2, 4, 5, 7, 9, 11};
        return (octave_ + 1) * 12 + kStepSemitones[static_cast<int>(step_)] + alter_;
    }

    Step step_ = Step::C;
    int alter_ = 0;
    int octave_ = 4;
    int midi_ = 60;
};

std::ostream& operator<<(std::ostream& os, const Pitch& pitch);

namespace literals {

/// "F4"_p, "Ab4"_p, "C#5"_p
Pitch operator""_p(const char* text, std::size_t length);

/// 12_st
Interval operator""_st(unsigned long long value);

}  // namespace literals

}  // namespace mpl
