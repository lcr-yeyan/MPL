#include "mpl/music/Instrument.hpp"

#include <algorithm>

namespace mpl {

std::vector<FretPosition> InstrumentCapability::positions(const Pitch& pitch) const {
    std::vector<FretPosition> result;
    if (!isFretted()) {
        if (contains(pitch)) {
            result.push_back(FretPosition{0, 0});
        }
        return result;
    }
    for (std::size_t stringIndex = 0; stringIndex < openStrings.size(); ++stringIndex) {
        const int fret = pitch.midi() - openStrings[stringIndex];
        if (fret >= 0 && fret <= fretCount) {
            result.push_back(FretPosition{static_cast<int>(stringIndex), fret});
        }
    }
    return result;
}

InstrumentCapability guitarCapability() {
    InstrumentCapability guitar;
    guitar.name = "guitar";
    guitar.openStrings = {40, 45, 50, 55, 59, 64};  // E2 A2 D3 G3 B3 E4
    guitar.lowestMidi = guitar.openStrings.front();
    guitar.highestMidi = guitar.openStrings.back() + 24;
    guitar.maxPolyphony = 6;
    guitar.fretCount = 24;
    return guitar;
}

bool fitsInstrument(const InstrumentCapability& instrument,
                    const std::vector<Pitch>& pitches) {
    if (static_cast<int>(pitches.size()) > instrument.maxPolyphony) {
        return false;
    }
    for (const Pitch& pitch : pitches) {
        if (!instrument.contains(pitch)) {
            return false;
        }
    }
    return true;
}

}  // namespace mpl
