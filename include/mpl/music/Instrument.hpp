#pragma once

#include "mpl/music/Pitch.hpp"

#include <string>
#include <vector>

namespace mpl {

/// One playable position on a fretted instrument.
struct FretPosition {
    int stringIndex = 0;  // 0 = lowest-pitched string
    int fret = 0;         // 0 = open string

    friend bool operator==(const FretPosition& lhs, const FretPosition& rhs) {
        return lhs.stringIndex == rhs.stringIndex && lhs.fret == rhs.fret;
    }
};

/// Instrument capability boundary. Range, polyphony and physical positions are
/// instrument-specific extensions; they never change Pitch/Chord semantics.
struct InstrumentCapability {
    std::string name;
    int lowestMidi = 0;
    int highestMidi = 127;
    int maxPolyphony = 1;
    /// Open-string MIDI numbers, low to high. Empty for non-fretted instruments.
    std::vector<int> openStrings;
    int fretCount = 0;

    bool contains(const Pitch& pitch) const {
        return pitch.midi() >= lowestMidi && pitch.midi() <= highestMidi;
    }
    bool isFretted() const { return !openStrings.empty(); }

    /// All string/fret positions that can produce `pitch`. Empty when the pitch
    /// is out of range or not playable on the available strings.
    std::vector<FretPosition> positions(const Pitch& pitch) const;
};

/// Standard six-string guitar, E2..E4 open strings, 24 frets.
InstrumentCapability guitarCapability();

/// A simple polyphony check over a set of pitches: true when the instrument
/// range contains every pitch and the count does not exceed maxPolyphony.
bool fitsInstrument(const InstrumentCapability& instrument,
                    const std::vector<Pitch>& pitches);

}  // namespace mpl
