#include "mpl/music/Chord.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace mpl {
namespace {

constexpr std::array<int, 7> kLetterSemitones = {0, 2, 4, 5, 7, 9, 11};
constexpr std::array<const char*, 12> kPitchClassNames = {
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

int positiveModulo(int value, int divisor) {
    return ((value % divisor) + divisor) % divisor;
}

std::string lowerAscii(std::string_view text) {
    std::string result(text);
    for (char& character : result) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return result;
}

bool startsWith(const std::string& text, std::string_view prefix) {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

/// Parses a pitch-class spelling at the start of `text`, returning the number
/// of consumed characters. No octave is accepted.
int parsePitchClassPrefix(std::string_view text, std::size_t& consumed) {
    if (text.empty()) {
        return -1;
    }
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    int index = -1;
    switch (letter) {
        case 'C': index = 0; break;
        case 'D': index = 2; break;
        case 'E': index = 4; break;
        case 'F': index = 5; break;
        case 'G': index = 7; break;
        case 'A': index = 9; break;
        case 'B': index = 11; break;
        default: return -1;
    }
    int value = index;
    std::size_t position = 1;
    while (position < text.size()) {
        if (text[position] == '#') {
            value += 1;
        } else if (text[position] == 'b') {
            value -= 1;
        } else {
            break;
        }
        ++position;
    }
    consumed = position;
    return positiveModulo(value, 12);
}

std::vector<int> triadIntervals(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major:
        case ChordQuality::Major6:
        case ChordQuality::Dominant7:
        case ChordQuality::Major7:
            return {0, 4, 7};
        case ChordQuality::Minor:
        case ChordQuality::Minor6:
        case ChordQuality::Minor7:
        case ChordQuality::MinorMajor7:
            return {0, 3, 7};
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
        case ChordQuality::Diminished7:
            return {0, 3, 6};
        case ChordQuality::Augmented:
            return {0, 4, 8};
        case ChordQuality::Sus2: return {0, 2, 7};
        case ChordQuality::Sus4: return {0, 5, 7};
        case ChordQuality::Power: return {0, 7};
        case ChordQuality::Custom: return {0, 4, 7};
    }
    return {0, 4, 7};
}

int seventhInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Major7:
        case ChordQuality::MinorMajor7:
            return 11;
        case ChordQuality::Diminished7:
            return 9;
        case ChordQuality::HalfDiminished7:
        case ChordQuality::Minor7:
        case ChordQuality::Dominant7:
            return 10;
        default:
            return 10;
    }
}

bool isSeventhQuality(ChordQuality quality) {
    return quality == ChordQuality::Dominant7 || quality == ChordQuality::Major7 ||
           quality == ChordQuality::Minor7 || quality == ChordQuality::HalfDiminished7 ||
           quality == ChordQuality::Diminished7 || quality == ChordQuality::MinorMajor7;
}

void replaceInterval(std::vector<int>& intervals, int from, int to) {
    for (int& interval : intervals) {
        if (interval == from) {
            interval = to;
        }
    }
}

std::string joinNotes(const std::vector<Pitch>& notes) {
    std::string text;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        if (index != 0) {
            text += " ";
        }
        text += notes[index].name();
    }
    return text;
}

}  // namespace

PitchClass PitchClass::fromPitch(const Pitch& pitch) {
    return PitchClass{positiveModulo(pitch.midi(), 12)};
}

PitchClass PitchClass::parse(std::string_view text) {
    std::size_t consumed = 0;
    const int value = parsePitchClassPrefix(text, consumed);
    if (value < 0 || consumed != text.size()) {
        throw Error("invalid pitch class '" + std::string(text) + "'", "chord");
    }
    return PitchClass{value};
}

std::string PitchClass::name() const {
    if (value < 0 || value > 11) {
        return "?";
    }
    return kPitchClassNames[static_cast<std::size_t>(value)];
}

PitchClassSet PitchClassSet::fromPitchClasses(std::vector<int> pitchClasses) {
    for (int& value : pitchClasses) {
        value = positiveModulo(value, 12);
    }
    std::sort(pitchClasses.begin(), pitchClasses.end());
    pitchClasses.erase(std::unique(pitchClasses.begin(), pitchClasses.end()),
                       pitchClasses.end());
    PitchClassSet set;
    set.values = std::move(pitchClasses);
    return set;
}

bool PitchClassSet::contains(int pitchClass) const {
    const int normalized = positiveModulo(pitchClass, 12);
    return std::find(values.begin(), values.end(), normalized) != values.end();
}

bool PitchClassSet::containsAll(const PitchClassSet& other) const {
    for (const int value : other.values) {
        if (!contains(value)) {
            return false;
        }
    }
    return true;
}

PitchClassSet PitchClassSet::intersection(const PitchClassSet& other) const {
    PitchClassSet result;
    for (const int value : values) {
        if (other.contains(value)) {
            result.values.push_back(value);
        }
    }
    return result;
}

std::string PitchClassSet::name() const {
    std::string text = "{";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            text += ",";
        }
        text += PitchClass{values[index]}.name();
    }
    text += "}";
    return text;
}

ChordSymbol ChordSymbol::parse(std::string_view text) {
    if (text.empty()) {
        throw Error("empty chord symbol", "chord");
    }
    std::string body(text);
    ChordSymbol chord;
    chord.label = body;

    // Slash bass.
    const std::size_t slash = body.find('/');
    if (slash != std::string::npos) {
        std::size_t consumed = 0;
        const int bass = parsePitchClassPrefix(body.substr(slash + 1), consumed);
        if (bass < 0 || slash + 1 + consumed != body.size()) {
            throw Error("invalid slash bass in chord '" + std::string(text) + "'", "chord");
        }
        chord.bassPitchClass = bass;
        body = body.substr(0, slash);
    }

    std::size_t consumed = 0;
    const int root = parsePitchClassPrefix(body, consumed);
    if (root < 0) {
        throw Error("invalid chord root in '" + std::string(text) + "'", "chord");
    }
    chord.rootPitchClass = root;
    const std::string rawSuffix = body.substr(consumed);
    std::string suffix = lowerAscii(rawSuffix);
    const bool uppercaseMajor = !rawSuffix.empty() && rawSuffix.front() == 'M';

    ChordQuality quality = ChordQuality::Major;
    int extension = 0;
    bool explicitMajor = false;
    bool explicitMinor = false;
    std::vector<std::pair<int, int>> alterations;  // (degree, altered semitone)

    if (suffix.empty()) {
        quality = ChordQuality::Major;
    } else if (startsWith(suffix, "maj")) {
        explicitMajor = true;
        suffix = suffix.substr(3);
    } else if (uppercaseMajor) {
        explicitMajor = true;
        suffix = suffix.substr(1);
    } else if (startsWith(suffix, "min")) {
        explicitMinor = true;
        suffix = suffix.substr(3);
    } else if (startsWith(suffix, "m") || startsWith(suffix, "-")) {
        explicitMinor = true;
        suffix = suffix.substr(1);
    } else if (startsWith(suffix, "dim") || startsWith(suffix, "o")) {
        quality = ChordQuality::Diminished;
        suffix = suffix.substr(startsWith(suffix, "dim") ? 3 : 1);
    } else if (startsWith(suffix, "aug") || startsWith(suffix, "+")) {
        quality = ChordQuality::Augmented;
        suffix = suffix.substr(startsWith(suffix, "aug") ? 3 : 1);
    } else if (startsWith(suffix, "sus2")) {
        quality = ChordQuality::Sus2;
        suffix = suffix.substr(4);
    } else if (startsWith(suffix, "sus4")) {
        quality = ChordQuality::Sus4;
        suffix = suffix.substr(4);
    } else if (startsWith(suffix, "sus")) {
        quality = ChordQuality::Sus4;
        suffix = suffix.substr(3);
    } else if (startsWith(suffix, "5")) {
        quality = ChordQuality::Power;
        suffix = suffix.substr(1);
    }

    // Extension number.
    std::size_t numberEnd = 0;
    while (numberEnd < suffix.size() &&
           std::isdigit(static_cast<unsigned char>(suffix[numberEnd]))) {
        ++numberEnd;
    }
    if (numberEnd > 0) {
        extension = std::stoi(suffix.substr(0, numberEnd));
        suffix = suffix.substr(numberEnd);
    }

    // Alterations: b5 #5 b9 #9 #11 b13.
    while (!suffix.empty()) {
        int direction = 0;
        if (suffix[0] == 'b') {
            direction = -1;
        } else if (suffix[0] == '#') {
            direction = 1;
        } else {
            break;
        }
        std::size_t digitsEnd = 1;
        while (digitsEnd < suffix.size() &&
               std::isdigit(static_cast<unsigned char>(suffix[digitsEnd]))) {
            ++digitsEnd;
        }
        if (digitsEnd == 1) {
            throw Error("invalid chord alteration in '" + std::string(text) + "'", "chord");
        }
        const int degree = std::stoi(suffix.substr(1, digitsEnd - 1));
        int base = 0;
        switch (degree) {
            case 5: base = 7; break;
            case 9: base = 14; break;
            case 11: base = 17; break;
            case 13: base = 21; break;
            default:
                throw Error("unsupported chord alteration degree " + std::to_string(degree),
                            "chord");
        }
        alterations.emplace_back(degree, base + direction);
        suffix = suffix.substr(digitsEnd);
    }
    if (!suffix.empty()) {
        throw Error("invalid chord suffix in '" + std::string(text) + "'", "chord");
    }

    // Quality from explicit prefix plus extension.
    if (quality == ChordQuality::Major) {
        if (explicitMinor) {
            quality = ChordQuality::Minor;
        }
    }
    if (extension == 6) {
        if (quality == ChordQuality::Major || quality == ChordQuality::Minor) {
            quality = explicitMinor ? ChordQuality::Minor6 : ChordQuality::Major6;
        } else if (quality == ChordQuality::Diminished) {
            quality = ChordQuality::Diminished7;  // dim6 is unusual; treat as dim7
        }
    } else if (extension == 7) {
        if (explicitMinor) {
            quality = ChordQuality::Minor7;
        } else if (explicitMajor) {
            quality = ChordQuality::Major7;
        } else if (quality == ChordQuality::Diminished) {
            quality = ChordQuality::Diminished7;
        } else if (quality == ChordQuality::Augmented) {
            quality = ChordQuality::Custom;
        } else if (quality == ChordQuality::Major) {
            quality = ChordQuality::Dominant7;
        }
    } else if (extension == 9 || extension == 11 || extension == 13) {
        if (quality == ChordQuality::Diminished) {
            quality = ChordQuality::Diminished7;
        } else if (explicitMinor) {
            quality = ChordQuality::Minor7;
        } else if (explicitMajor) {
            quality = ChordQuality::Major7;
        } else {
            quality = ChordQuality::Dominant7;
        }
    } else if (extension != 0) {
        throw Error("unsupported chord extension " + std::to_string(extension), "chord");
    }
    chord.quality = quality;

    std::vector<int> intervals;
    if (quality == ChordQuality::Major6) {
        intervals = {0, 4, 7, 9};
    } else if (quality == ChordQuality::Minor6) {
        intervals = {0, 3, 7, 9};
    } else {
        intervals = triadIntervals(quality);
        if (isSeventhQuality(quality)) {
            intervals.push_back(seventhInterval(quality));
        }
    }
    if (extension == 6 && quality != ChordQuality::Major6 &&
        quality != ChordQuality::Minor6) {
        intervals.push_back(9);
    }
    if (extension >= 9) {
        intervals.push_back(14);
    }
    if (extension >= 11) {
        intervals.push_back(17);
    }
    if (extension >= 13) {
        intervals.push_back(21);
    }
    for (const auto& [degree, altered] : alterations) {
        // Replace the natural degree interval, adding it first when an
        // extension was not explicitly present (for example A7b9 or G7#5).
        int base = 0;
        switch (degree) {
            case 5: base = 7; break;
            case 9: base = 14; break;
            case 11: base = 17; break;
            case 13: base = 21; break;
            default:
                throw Error("unsupported chord alteration degree " + std::to_string(degree),
                            "chord");
        }
        if (std::find(intervals.begin(), intervals.end(), base) == intervals.end()) {
            intervals.push_back(base);
        }
        replaceInterval(intervals, base, altered);
    }
    std::sort(intervals.begin(), intervals.end());
    intervals.erase(std::unique(intervals.begin(), intervals.end()), intervals.end());
    chord.intervals = std::move(intervals);
    if (chord.intervals.empty() || chord.intervals.front() != 0) {
        throw Error("chord '" + std::string(text) + "' has no root interval", "chord");
    }
    return chord;
}

PitchClassSet ChordSymbol::pitchClassSet() const {
    std::vector<int> classes;
    classes.reserve(intervals.size());
    for (const int interval : intervals) {
        classes.push_back(positiveModulo(rootPitchClass + interval, 12));
    }
    return PitchClassSet::fromPitchClasses(std::move(classes));
}

bool ChordSymbol::contains(int pitchClass) const {
    const int relative = positiveModulo(pitchClass - rootPitchClass, 12);
    for (const int interval : intervals) {
        if (positiveModulo(interval, 12) == relative) {
            return true;
        }
    }
    return false;
}

bool ChordSymbol::intervalOf(int pitchClass, int& semitones) const {
    const int relative = positiveModulo(pitchClass - rootPitchClass, 12);
    for (const int interval : intervals) {
        if (positiveModulo(interval, 12) == relative) {
            semitones = interval;
            return true;
        }
    }
    return false;
}

int ChordSymbol::inversion() const {
    if (bassPitchClass < 0) {
        return 0;
    }
    int index = 0;
    for (const int interval : intervals) {
        if (positiveModulo(rootPitchClass + interval, 12) == bassPitchClass) {
            return index;
        }
        ++index;
    }
    return 0;
}

std::string ChordSymbol::name() const {
    return label.empty() ? "none" : label;
}

std::string_view voicingStatusName(VoicingStatus status) {
    switch (status) {
        case VoicingStatus::StrictlySatisfied: return "strictly-satisfied";
        case VoicingStatus::SatisfiedWithinTolerance: return "satisfied-within-tolerance";
        case VoicingStatus::Unsatisfied: return "unsatisfied";
        case VoicingStatus::BackendCapability: return "backend-capability";
        case VoicingStatus::BudgetExhausted: return "budget-exhausted";
        case VoicingStatus::ProvablyUnsatisfiable: return "provably-unsatisfiable";
    }
    return "unknown";
}

namespace {

bool ascendingAndSpaced(const std::vector<Pitch>& notes, int maxSpacing) {
    for (std::size_t index = 1; index < notes.size(); ++index) {
        const int gap = notes[index].midi() - notes[index - 1].midi();
        if (gap <= 0 || gap > maxSpacing) {
            return false;
        }
    }
    return true;
}

bool completeChord(const ChordSymbol& chord, const std::vector<Pitch>& notes) {
    std::vector<int> classes;
    classes.reserve(notes.size());
    for (const Pitch& note : notes) {
        classes.push_back(positiveModulo(note.midi(), 12));
    }
    return PitchClassSet::fromPitchClasses(std::move(classes))
        .containsAll(chord.pitchClassSet());
}

/// Ascending chord-tone pitches in [low, high], ordered by MIDI number.
std::vector<Pitch> chordTonePitches(const ChordSymbol& chord, const Pitch& low,
                                    const Pitch& high) {
    std::vector<Pitch> result;
    for (int midi = low.midi(); midi <= high.midi(); ++midi) {
        if (chord.contains(positiveModulo(midi, 12))) {
            result.push_back(Pitch::fromMidi(midi));
        }
    }
    return result;
}

}  // namespace

Voicing buildVoicing(const ChordSymbol& chord, const Pitch& low, const Pitch& high,
                     int voices, int inversion, int maxSpacing) {
    Voicing result;
    if (!chord.valid()) {
        result.status = VoicingStatus::ProvablyUnsatisfiable;
        result.detail = "invalid chord";
        return result;
    }
    if (voices < 1 || voices > 8 || maxSpacing < 1 || low.midi() > high.midi()) {
        result.status = VoicingStatus::ProvablyUnsatisfiable;
        result.detail = "invalid voice count, range or spacing";
        return result;
    }
    const std::vector<Pitch> tones = chordTonePitches(chord, low, high);
    if (tones.empty()) {
        result.status = VoicingStatus::ProvablyUnsatisfiable;
        result.detail = "no chord tones in range";
        return result;
    }

    // Bass = the lowest chord tone with the requested inversion pitch class.
    const int inversionIndex = positiveModulo(inversion, static_cast<int>(chord.intervals.size()));
    const int bassClass = positiveModulo(
        chord.rootPitchClass + chord.intervals[static_cast<std::size_t>(inversionIndex)], 12);
    std::optional<Pitch> bass;
    for (const Pitch& tone : tones) {
        if (positiveModulo(tone.midi(), 12) == bassClass) {
            bass = tone;
            break;
        }
    }
    if (!bass.has_value()) {
        result.status = VoicingStatus::ProvablyUnsatisfiable;
        result.detail = "bass pitch class is outside the range";
        return result;
    }
    result.notes.push_back(*bass);

    // Walk chord tones upward from the inversion, cycling for doublings.
    std::vector<int> order;
    const int toneCount = static_cast<int>(chord.intervals.size());
    for (int index = 0; index < toneCount; ++index) {
        order.push_back(positiveModulo(inversionIndex + index, toneCount));
    }
    int orderCursor = 1;
    while (static_cast<int>(result.notes.size()) < voices) {
        const Pitch& previous = result.notes.back();
        const int desiredClass = positiveModulo(
            chord.rootPitchClass +
                chord.intervals[static_cast<std::size_t>(order[static_cast<std::size_t>(orderCursor % toneCount)])],
            12);
        ++orderCursor;
        int foundMidi = -1;
        for (int midi = previous.midi() + 1; midi <= high.midi(); ++midi) {
            if (positiveModulo(midi, 12) == desiredClass && midi - previous.midi() <= maxSpacing) {
                foundMidi = midi;
                break;
            }
        }
        if (foundMidi < 0) {
            result.status = VoicingStatus::ProvablyUnsatisfiable;
            result.detail = "voice range or spacing cannot accommodate the chord";
            return result;
        }
        result.notes.push_back(Pitch::fromMidi(foundMidi));
    }

    result.crossings = voiceCrossings(result.notes);
    result.complete = completeChord(chord, result.notes);
    if (!ascendingAndSpaced(result.notes, maxSpacing) || result.crossings != 0) {
        result.status = VoicingStatus::ProvablyUnsatisfiable;
        result.detail = "voicing violates ordering or spacing";
        return result;
    }
    result.status = VoicingStatus::StrictlySatisfied;
    result.searchComplete = true;
    result.detail = joinNotes(result.notes);
    return result;
}

Voicing voiceLead(const std::vector<Pitch>& previous, const ChordSymbol& chord,
                  const Pitch& low, const Pitch& high, int maxMove, int maxSpacing,
                  bool allowCrossing, int budget) {
    Voicing result;
    result.status = VoicingStatus::ProvablyUnsatisfiable;
    if (!chord.valid() || previous.empty() || previous.size() > 8 || maxMove < 0 ||
        maxSpacing < 1 || budget < 1 || low.midi() > high.midi()) {
        result.detail = "invalid voice-leading parameters";
        return result;
    }
    const int voices = static_cast<int>(previous.size());
    const int tolerance = 1;
    const std::vector<Pitch> chordPitches = chordTonePitches(chord, low, high);
    if (chordPitches.empty()) {
        result.searchComplete = true;
        result.detail = "no chord tones in range";
        return result;
    }

    std::vector<std::vector<Pitch>> candidates(static_cast<std::size_t>(voices));
    for (int voice = 0; voice < voices; ++voice) {
        const int center = previous[static_cast<std::size_t>(voice)].midi();
        for (const Pitch& pitch : chordPitches) {
            const int movement = std::abs(pitch.midi() - center);
            if (movement <= maxMove + tolerance) {
                candidates[static_cast<std::size_t>(voice)].push_back(pitch);
            }
        }
        if (candidates[static_cast<std::size_t>(voice)].empty()) {
            result.status = VoicingStatus::Unsatisfied;
            result.searchComplete = true;
            result.detail = "voice " + std::to_string(voice) +
                            " has no candidate within maxMove plus tolerance";
            return result;
        }
        std::sort(candidates[static_cast<std::size_t>(voice)].begin(),
                  candidates[static_cast<std::size_t>(voice)].end(),
                  [center](const Pitch& lhs, const Pitch& rhs) {
                      const int left = std::abs(lhs.midi() - center);
                      const int right = std::abs(rhs.midi() - center);
                      if (left != right) {
                          return left < right;
                      }
                      return lhs.midi() < rhs.midi();
                  });
    }

    std::vector<Pitch> current;
    current.reserve(static_cast<std::size_t>(voices));
    int explored = 0;
    bool exhausted = false;
    bool foundStrict = false;
    bool foundTolerance = false;
    bool foundAny = false;
    Voicing bestStrict;
    Voicing bestTolerance;
    Voicing bestAny;

    const auto score = [&](const std::vector<Pitch>& notes) {
        int total = 0;
        for (int voice = 0; voice < voices; ++voice) {
            total += std::abs(notes[static_cast<std::size_t>(voice)].midi() -
                              previous[static_cast<std::size_t>(voice)].midi());
        }
        return total;
    };
    const auto consider = [&](const std::vector<Pitch>& notes) {
        int total = 0;
        int worst = 0;
        for (int voice = 0; voice < voices; ++voice) {
            const int movement = std::abs(notes[static_cast<std::size_t>(voice)].midi() -
                                          previous[static_cast<std::size_t>(voice)].midi());
            total += movement;
            worst = std::max(worst, movement);
        }
        Voicing candidate;
        candidate.notes = notes;
        candidate.totalMovement = total;
        candidate.crossings = voiceCrossings(notes);
        candidate.complete = completeChord(chord, notes);
        candidate.detail = joinNotes(notes);
        const bool orderOk = allowCrossing || ascendingAndSpaced(notes, maxSpacing);
        if (!orderOk || (!allowCrossing && candidate.crossings != 0)) {
            return;
        }
        const auto better = [](const Voicing& lhs, const Voicing& rhs) {
            if (lhs.notes.empty()) {
                return true;
            }
            if (lhs.totalMovement != rhs.totalMovement) {
                return rhs.totalMovement < lhs.totalMovement;
            }
            for (std::size_t index = 0; index < lhs.notes.size(); ++index) {
                if (lhs.notes[index].midi() != rhs.notes[index].midi()) {
                    return rhs.notes[index].midi() < lhs.notes[index].midi();
                }
            }
            return false;
        };
        if (worst <= maxMove) {
            candidate.status = VoicingStatus::StrictlySatisfied;
            if (better(bestStrict, candidate)) {
                bestStrict = candidate;
                foundStrict = true;
            }
        } else if (worst <= maxMove + tolerance) {
            candidate.status = VoicingStatus::SatisfiedWithinTolerance;
            if (better(bestTolerance, candidate)) {
                bestTolerance = candidate;
                foundTolerance = true;
            }
        } else {
            candidate.status = VoicingStatus::Unsatisfied;
            if (better(bestAny, candidate)) {
                bestAny = candidate;
                foundAny = true;
            }
        }
    };

    // Recursive bounded search. Candidates are already sorted by movement and
    // pitch, so the first strict candidate is also a deterministic tie-break.
    std::function<void(int)> visit = [&](int voice) {
        if (++explored > budget) {
            exhausted = true;
            return;
        }
        if (exhausted) {
            return;
        }
        if (voice == voices) {
            consider(current);
            return;
        }
        for (const Pitch& pitch : candidates[static_cast<std::size_t>(voice)]) {
            if (exhausted) {
                return;
            }
            if (!current.empty()) {
                if (!allowCrossing) {
                    if (pitch.midi() <= current.back().midi()) {
                        continue;
                    }
                    if (pitch.midi() - current.back().midi() > maxSpacing) {
                        continue;
                    }
                }
            }
            current.push_back(pitch);
            visit(voice + 1);
            current.pop_back();
        }
    };
    visit(0);

    if (exhausted) {
        result = foundStrict ? bestStrict : foundTolerance ? bestTolerance : bestAny;
        if (result.notes.empty()) {
            result.detail = "search budget exhausted before a valid candidate";
        }
        result.status = VoicingStatus::BudgetExhausted;
        result.searchComplete = false;
        result.detail = "voice-leading search budget exhausted";
        return result;
    }
    if (foundStrict) {
        bestStrict.searchComplete = true;
        return bestStrict;
    }
    if (foundTolerance) {
        bestTolerance.searchComplete = true;
        return bestTolerance;
    }
    if (foundAny) {
        bestAny.searchComplete = true;
        return bestAny;
    }
    result.searchComplete = true;
    result.detail = "no voicing satisfies the hard constraints";
    return result;
}

int voiceCrossings(const std::vector<Pitch>& notes) {
    int crossings = 0;
    for (std::size_t outer = 0; outer < notes.size(); ++outer) {
        for (std::size_t inner = outer + 1; inner < notes.size(); ++inner) {
            if (notes[outer].midi() > notes[inner].midi()) {
                ++crossings;
            }
        }
    }
    return crossings;
}

int totalVoiceMovement(const std::vector<Pitch>& from, const std::vector<Pitch>& to) {
    const std::size_t count = std::min(from.size(), to.size());
    int total = 0;
    for (std::size_t index = 0; index < count; ++index) {
        total += std::abs(from[index].midi() - to[index].midi());
    }
    return total;
}

std::vector<Pitch> commonTones(const std::vector<Pitch>& a, const std::vector<Pitch>& b) {
    std::vector<Pitch> result;
    for (const Pitch& pitch : a) {
        for (const Pitch& other : b) {
            if (positiveModulo(pitch.midi(), 12) == positiveModulo(other.midi(), 12)) {
                result.push_back(pitch);
                break;
            }
        }
    }
    return result;
}

bool isChordTone(const ChordSymbol& chord, const Pitch& pitch) {
    return chord.contains(positiveModulo(pitch.midi(), 12));
}

}  // namespace mpl
