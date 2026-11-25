#include "mpl/music/Analysis.hpp"

#include <algorithm>

namespace mpl {
namespace {

Duration noteEnd(const Note& note) {
    return note.offset + note.duration;
}

int perfectClass(int semitones) {
    return ((semitones % 12) + 12) % 12;
}

}  // namespace

bool notesOverlap(const Note& a, const Note& b) {
    const Rational aStart = a.offset.inWholeNotes();
    const Rational aEnd = noteEnd(a).inWholeNotes();
    const Rational bStart = b.offset.inWholeNotes();
    const Rational bEnd = noteEnd(b).inWholeNotes();
    return aStart < bEnd && bStart < aEnd;
}

int noteIntervalOverlap(const Fragment& a, const Fragment& b) {
    int count = 0;
    for (const Note& left : a.notes()) {
        for (const Note& right : b.notes()) {
            if (notesOverlap(left, right)) {
                ++count;
            }
        }
    }
    return count;
}

int soundingRange(const Fragment& fragment) {
    if (fragment.notes().empty()) {
        return 0;
    }
    int low = fragment.notes().front().pitch.midi();
    int high = low;
    for (const Note& note : fragment.notes()) {
        low = std::min(low, note.pitch.midi());
        high = std::max(high, note.pitch.midi());
    }
    return high - low;
}

int parallelPerfects(const std::vector<Pitch>& lower, const std::vector<Pitch>& upper) {
    if (lower.size() != upper.size() || lower.size() < 2) {
        return 0;
    }
    int count = 0;
    for (std::size_t index = 1; index < lower.size(); ++index) {
        const int previousClass =
            perfectClass(upper[index - 1].midi() - lower[index - 1].midi());
        const int currentClass = perfectClass(upper[index].midi() - lower[index].midi());
        const bool perfect = (currentClass == 0 || currentClass == 7) &&
                             (previousClass == 0 || previousClass == 7);
        if (!perfect) {
            continue;
        }
        const int lowerMove = lower[index].midi() - lower[index - 1].midi();
        const int upperMove = upper[index].midi() - upper[index - 1].midi();
        if (lowerMove != 0 && upperMove != 0 &&
            ((lowerMove > 0) == (upperMove > 0))) {
            ++count;
        }
    }
    return count;
}

}  // namespace mpl
