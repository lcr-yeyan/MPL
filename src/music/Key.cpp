#include "mpl/music/Key.hpp"

#include "mpl/core/Error.hpp"

#include <cctype>
#include <ostream>

namespace mpl {
namespace {

constexpr int kLetterSemitones[7] = {0, 2, 4, 5, 7, 9, 11};

int letterIndex(Step step) {
    return static_cast<int>(step);
}

Step stepFromIndex(int index) {
    return static_cast<Step>(index);
}

char stepLetter(Step step) {
    constexpr char kLetters[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    return kLetters[letterIndex(step)];
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
    throw Error(std::string("invalid key tonic letter '") + letter + "'", "key");
}

std::string lower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

int wrapPitchClass(int value) {
    const int wrapped = ((value % 12) + 12) % 12;
    return wrapped;
}

}  // namespace

Key Key::fromTonic(Step tonic, int alter, Mode mode) {
    Key key;
    key.tonic = tonic;
    key.alter = alter;
    key.mode = mode;
    return key;
}

Key Key::parse(std::string_view text) {
    const std::string_view trimmed = trim(text);
    const std::size_t space = trimmed.find(' ');
    if (space == std::string_view::npos) {
        throw Error("invalid key '" + std::string(trimmed) + "' (expected e.g. \"F minor\")",
                    "key");
    }

    const std::string_view tonicText = trimmed.substr(0, space);
    const std::string modeText = lower(trim(trimmed.substr(space + 1)));

    if (tonicText.empty()) {
        throw Error("invalid key tonic", "key");
    }
    const Step tonic = parseStep(tonicText.front());
    int alter = 0;
    for (std::size_t index = 1; index < tonicText.size(); ++index) {
        if (tonicText[index] == '#') {
            ++alter;
        } else if (tonicText[index] == 'b') {
            --alter;
        } else {
            throw Error("invalid key tonic '" + std::string(tonicText) + "'", "key");
        }
    }

    Mode mode = Mode::Major;
    if (modeText == "major") {
        mode = Mode::Major;
    } else if (modeText == "minor" || modeText == "natural minor") {
        mode = Mode::NaturalMinor;
    } else {
        throw Error("invalid key mode '" + std::string(modeText) + "'", "key");
    }
    return fromTonic(tonic, alter, mode);
}

std::string Key::name() const {
    std::string result;
    result.push_back(stepLetter(tonic));
    if (alter > 0) {
        result.append(static_cast<std::size_t>(alter), '#');
    } else if (alter < 0) {
        result.append(static_cast<std::size_t>(-alter), 'b');
    }
    result += mode == Mode::Major ? " major" : " minor";
    return result;
}

std::string_view Key::modeName() const {
    return mode == Mode::Major ? "major" : "minor";
}

std::array<int, 7> Key::scaleSemitones() const {
    if (mode == Mode::Major) {
        return {0, 2, 4, 5, 7, 9, 11};
    }
    return {0, 2, 3, 5, 7, 8, 10};
}

int Key::tonicPitchClass() const {
    return wrapPitchClass(kLetterSemitones[letterIndex(tonic)] + alter);
}

std::ostream& operator<<(std::ostream& os, const Key& key) {
    return os << key.name();
}

}  // namespace mpl
