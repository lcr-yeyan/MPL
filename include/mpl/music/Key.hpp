#pragma once

#include "mpl/music/Pitch.hpp"

#include <array>
#include <ostream>
#include <string>
#include <string_view>

namespace mpl {

enum class Mode { Major, NaturalMinor };

/// A tonal key: tonic spelling plus mode. The first version of the harmony
/// library only needs the seven-note major and natural-minor scales.
struct Key {
    Step tonic = Step::C;
    int alter = 0;
    Mode mode = Mode::Major;

    /// Parses names such as "F minor", "Bb major" or "C# minor".
    static Key parse(std::string_view text);
    static Key fromTonic(Step tonic, int alter, Mode mode);

    std::string name() const;
    std::string_view modeName() const;

    /// Semitone offsets of the seven scale degrees relative to the tonic.
    std::array<int, 7> scaleSemitones() const;

    int tonicPitchClass() const;
};

std::ostream& operator<<(std::ostream& os, const Key& key);

}  // namespace mpl
