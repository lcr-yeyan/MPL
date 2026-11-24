#include "mpl/music/Pitch.hpp"

#include "mpl/core/Error.hpp"

#include <ostream>

namespace mpl {
namespace {

struct Spelling {
    Step step;
    int alter;
};

constexpr Spelling kSharpSpellings[12] = {
    {Step::C, 0}, {Step::C, 1}, {Step::D, 0}, {Step::D, 1}, {Step::E, 0}, {Step::F, 0},
    {Step::F, 1}, {Step::G, 0}, {Step::G, 1}, {Step::A, 0}, {Step::A, 1}, {Step::B, 0},
};

constexpr Spelling kFlatSpellings[12] = {
    {Step::C, 0}, {Step::D, -1}, {Step::D, 0}, {Step::E, -1}, {Step::E, 0}, {Step::F, 0},
    {Step::G, -1}, {Step::G, 0}, {Step::A, -1}, {Step::A, 0}, {Step::B, -1}, {Step::B, 0},
};

char stepLetter(Step step) {
    switch (step) {
        case Step::C: return 'C';
        case Step::D: return 'D';
        case Step::E: return 'E';
        case Step::F: return 'F';
        case Step::G: return 'G';
        case Step::A: return 'A';
        case Step::B: return 'B';
    }
    return 'C';
}

Step parseStep(char letter) {
    switch (letter) {
        case 'C': case 'c': return Step::C;
        case 'D': case 'd': return Step::D;
        case 'E': case 'e': return Step::E;
        case 'F': case 'f': return Step::F;
        case 'G': case 'g': return Step::G;
        case 'A': case 'a': return Step::A;
        case 'B': case 'b': return Step::B;
        default: break;
    }
    throw Error(std::string("invalid pitch letter '") + letter + "'", "pitch");
}

void requireMidiRange(int midi) {
    if (midi < 0 || midi > 127) {
        throw Error("pitch " + std::to_string(midi) + " is outside the MIDI range 0..127", "pitch");
    }
}

}  // namespace

Pitch Pitch::fromMidi(int midi, bool preferFlats) {
    requireMidiRange(midi);
    const Spelling& spelling = preferFlats ? kFlatSpellings[midi % 12] : kSharpSpellings[midi % 12];
    const Pitch result(spelling.step, spelling.alter, midi / 12 - 1);
    return result;
}

Pitch Pitch::parse(std::string_view name) {
    if (name.size() < 2) {
        throw Error("invalid pitch: '" + std::string(name) + "'", "pitch");
    }

    const Step step = parseStep(name[0]);
    std::size_t cursor = 1;
    int alter = 0;
    while (cursor < name.size() && (name[cursor] == '#' || name[cursor] == 'b')) {
        alter += name[cursor] == '#' ? 1 : -1;
        ++cursor;
    }

    bool negativeOctave = false;
    if (cursor < name.size() && name[cursor] == '-') {
        negativeOctave = true;
        ++cursor;
    }
    if (cursor >= name.size()) {
        throw Error("invalid pitch: '" + std::string(name) + "'", "pitch");
    }

    int octave = 0;
    for (; cursor < name.size(); ++cursor) {
        const char digit = name[cursor];
        if (digit < '0' || digit > '9') {
            throw Error("invalid pitch: '" + std::string(name) + "'", "pitch");
        }
        octave = octave * 10 + (digit - '0');
        if (octave > 99) {
            throw Error("invalid pitch octave: '" + std::string(name) + "'", "pitch");
        }
    }
    if (negativeOctave) {
        octave = -octave;
    }

    const Pitch result(step, alter, octave);
    requireMidiRange(result.midi());
    return result;
}

std::string Pitch::name() const {
    std::string result;
    result.push_back(stepLetter(step_));
    if (alter_ > 0) {
        result.append(static_cast<std::size_t>(alter_), '#');
    } else if (alter_ < 0) {
        result.append(static_cast<std::size_t>(-alter_), 'b');
    }
    result += std::to_string(octave_);
    return result;
}

Pitch Pitch::transposed(Interval interval) const {
    const int target = midi_ + interval.semitones;
    requireMidiRange(target);
    return Pitch::fromMidi(target, alter_ < 0);
}

std::ostream& operator<<(std::ostream& os, const Pitch& pitch) {
    return os << pitch.name();
}

std::ostream& operator<<(std::ostream& os, const Interval& interval) {
    return os << interval.semitones << "st";
}

namespace literals {

Pitch operator""_p(const char* text, std::size_t length) {
    return Pitch::parse(std::string_view(text, length));
}

Interval operator""_st(unsigned long long value) {
    return Interval{static_cast<int>(value)};
}

}  // namespace literals

}  // namespace mpl
