#include "mpl/music/Harmony.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <utility>

namespace mpl {
namespace {

constexpr int kLetterSemitones[7] = {0, 2, 4, 5, 7, 9, 11};

int letterIndex(Step step) {
    return static_cast<int>(step);
}

Step stepFromIndex(int index) {
    return static_cast<Step>(index);
}

int floorDivide(int value, int divisor) {
    int quotient = value / divisor;
    if ((value % divisor != 0) && ((value < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

int positiveModulo(int value, int divisor) {
    return ((value % divisor) + divisor) % divisor;
}

int nearestAlteration(int pitchClassDifference) {
    const int wrapped = positiveModulo(pitchClassDifference, 12);
    return wrapped > 6 ? wrapped - 12 : wrapped;
}

int absoluteDiatonic(const Pitch& pitch) {
    return pitch.octave() * 7 + letterIndex(pitch.step());
}

}  // namespace

Pitch diatonicTranspose(const Pitch& pitch, int scaleSteps, const Key& key) {
    const std::array<int, 7> scale = key.scaleSemitones();
    const int tonicLetter = letterIndex(key.tonic);
    const int inputDiatonic = absoluteDiatonic(pitch);

    const int inputDegree = positiveModulo(inputDiatonic - tonicLetter, 7);
    const int inputBasePitchClass =
        positiveModulo(key.tonicPitchClass() + scale[static_cast<std::size_t>(inputDegree)], 12);
    const int alteration =
        nearestAlteration(static_cast<int>(pitch.midi()) % 12 - inputBasePitchClass);

    const int targetDiatonic = inputDiatonic + scaleSteps;
    const int targetDegree = positiveModulo(targetDiatonic - tonicLetter, 7);
    const int targetLetter = positiveModulo(targetDiatonic, 7);
    const int targetOctave = floorDivide(targetDiatonic, 7);
    const int targetPitchClass = positiveModulo(
        key.tonicPitchClass() + scale[static_cast<std::size_t>(targetDegree)] + alteration, 12);

    const int naturalPitchClass = kLetterSemitones[targetLetter];
    const int targetAlteration = nearestAlteration(targetPitchClass - naturalPitchClass);
    const int targetMidi = 12 * (targetOctave + 1) + naturalPitchClass + targetAlteration;
    if (targetMidi < 0 || targetMidi > 127) {
        throw Error("diatonic transposition leaves the MIDI range", "harmony");
    }

    return Pitch(stepFromIndex(targetLetter), targetAlteration, targetOctave);
}

Fragment diatonicTranspose(const Fragment& input, int scaleSteps, const Key& key) {
    std::vector<Note> notes = input.notes();
    for (Note& note : notes) {
        note.pitch = diatonicTranspose(note.pitch, scaleSteps, key);
    }
    return Fragment::fromNotes(std::move(notes), input.duration());
}

Fragment harmonize(const Fragment& input, const std::vector<int>& degreeOffsets, const Key& key,
                   int velocity, ObjectId generatedOrigin) {
    if (degreeOffsets.empty()) {
        throw Error("harmonize requires at least one scale-degree offset", "harmonize");
    }
    if (velocity < 1 || velocity > 127) {
        throw Error("harmonize velocity must be in 1..127", "harmonize");
    }

    std::vector<Note> notes;
    notes.reserve(input.noteCount() * (degreeOffsets.size() + 1));
    for (const Note& note : input.notes()) {
        notes.push_back(note);
        for (const int offset : degreeOffsets) {
            Note harmony = note;
            harmony.pitch = diatonicTranspose(note.pitch, offset, key);
            harmony.velocity = velocity;
            harmony.origin = generatedOrigin;
            notes.push_back(harmony);
        }
    }
    return Fragment::fromNotes(std::move(notes), input.duration());
}

Fragment syncopate(const Fragment& input, const Meter& meter, const Rational& displacement) {
    if (displacement.isZero()) {
        return input;
    }

    std::vector<Note> notes = input.notes();
    Duration span = input.duration();
    const Rational beatLength = meter.beatLength();

    for (Note& note : notes) {
        const Rational beatPosition = note.offset.inWholeNotes() / beatLength;
        if (!beatPosition.isInteger()) {
            continue;
        }
        Rational shifted = note.offset.inWholeNotes() + displacement;
        if (shifted.isNegative()) {
            shifted = Rational(0, 1);
        }
        note.offset = Duration(shifted);
        span = std::max(span, note.offset + note.duration);
    }
    return Fragment::fromNotes(std::move(notes), span);
}

}  // namespace mpl
