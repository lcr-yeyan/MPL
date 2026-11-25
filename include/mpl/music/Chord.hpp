#pragma once

#include "mpl/music/Pitch.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

/// Pitch class in [0, 11]. This is a spelling-free harmonic identity, distinct
/// from a spelled Pitch (which also carries an octave). MIDI's 0..127 range is
/// an encoding detail, not a definition of musical pitch.
struct PitchClass {
    int value = 0;

    static PitchClass fromPitch(const Pitch& pitch);
    static PitchClass parse(std::string_view text);

    std::string name() const;
    friend bool operator==(const PitchClass& lhs, const PitchClass& rhs) {
        return lhs.value == rhs.value;
    }
    friend bool operator!=(const PitchClass& lhs, const PitchClass& rhs) {
        return !(lhs == rhs);
    }
};

/// A set of pitch classes, used for chord-tone and common-tone queries.
struct PitchClassSet {
    std::vector<int> values;  // sorted, unique, each in [0, 11]

    static PitchClassSet fromPitchClasses(std::vector<int> pitchClasses);
    bool contains(int pitchClass) const;
    bool containsAll(const PitchClassSet& other) const;
    PitchClassSet intersection(const PitchClassSet& other) const;
    std::size_t size() const { return values.size(); }
    bool empty() const { return values.empty(); }
    std::string name() const;
};

/// Common chord qualities. `Custom` is used for parsed altered/extended chords
/// whose interval set is stored directly in ChordSymbol.
enum class ChordQuality {
    Major,
    Minor,
    Diminished,
    Augmented,
    Sus2,
    Sus4,
    Power,
    Major6,
    Minor6,
    Dominant7,
    Major7,
    Minor7,
    HalfDiminished7,
    Diminished7,
    MinorMajor7,
    Custom,
};

/// A chord symbol: harmonic identity plus an explicit interval set and optional
/// slash bass. Spelling of the root is retained for display; sounding pitch is
/// derived only when a voicing assigns octaves.
struct ChordSymbol {
    int rootPitchClass = -1;
    ChordQuality quality = ChordQuality::Major;
    std::vector<int> intervals;  // semitones above root, sorted, starting with 0
    int bassPitchClass = -1;     // -1 = root position
    std::string label;

    bool valid() const { return rootPitchClass >= 0 && rootPitchClass < 12; }
    static ChordSymbol parse(std::string_view text);
    static ChordSymbol none() { return ChordSymbol{}; }

    /// Chord tones as pitch classes, without doubling.
    PitchClassSet pitchClassSet() const;
    bool contains(int pitchClass) const;
    /// Interval above the root, if `pitchClass` is a chord tone.
    bool intervalOf(int pitchClass, int& semitones) const;
    int inversion() const;
    std::string name() const;
};

/// A spelled chord tone with an explicit octave.
struct VoicedNote {
    Pitch pitch;
};

/// A concrete voicing. Hard-constraint and search status are explicit so a
/// budget-limited search is never reported as a mathematical impossibility.
enum class VoicingStatus {
    StrictlySatisfied,
    SatisfiedWithinTolerance,
    Unsatisfied,
    BackendCapability,
    BudgetExhausted,
    ProvablyUnsatisfiable,
};

std::string_view voicingStatusName(VoicingStatus status);

struct Voicing {
    std::vector<Pitch> notes;  // low to high
    VoicingStatus status = VoicingStatus::Unsatisfied;
    int totalMovement = 0;
    int crossings = 0;
    bool complete = false;
    /// True only when the search enumerated all candidates within budget. A
    /// returned candidate is the best found, not a proof of global optimality
    /// when this flag is false.
    bool searchComplete = false;
    std::string detail;
};

/// Builds a deterministic close/open voicing of `chord` inside [low, high].
/// `voices` is the number of sounding voices; extra voices double the bass,
/// then the fifth, then the root. Missing upper chord tones are omitted from
/// the top down when voices are fewer than chord tones.
Voicing buildVoicing(const ChordSymbol& chord, const Pitch& low, const Pitch& high,
                     int voices, int inversion, int maxSpacing = 12);

/// Bounded voice-leading search from `previous` to `chord`.
/// Hard constraints: voice count, range, chord tones, ascending order when
/// `allowCrossing` is false, adjacent spacing <= maxSpacing.
/// Optimization: minimal total semitone movement; ties are resolved
/// lexicographically by the low-to-high pitch list.
/// `maxMove` is a hard target; +1 semitone is reported only as
/// SatisfiedWithinTolerance.
Voicing voiceLead(const std::vector<Pitch>& previous, const ChordSymbol& chord,
                  const Pitch& low, const Pitch& high, int maxMove, int maxSpacing,
                  bool allowCrossing, int budget = 20000);

int voiceCrossings(const std::vector<Pitch>& notes);
int totalVoiceMovement(const std::vector<Pitch>& from, const std::vector<Pitch>& to);
std::vector<Pitch> commonTones(const std::vector<Pitch>& a, const std::vector<Pitch>& b);
bool isChordTone(const ChordSymbol& chord, const Pitch& pitch);

}  // namespace mpl
