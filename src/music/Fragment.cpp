#include "mpl/music/Fragment.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace mpl {
namespace {

Duration noteEnd(const Note& note) {
    return note.offset + note.duration;
}

void validateNote(const Note& note) {
    if (note.pitch.midi() < 0 || note.pitch.midi() > 127) {
        throw Error("note pitch " + note.pitch.name() + " is outside the MIDI range", "note");
    }
    if (note.offset.inWholeNotes().isNegative()) {
        throw Error("note offset must not be negative", "note");
    }
    if (note.duration.inWholeNotes().isNegative() || note.duration.inWholeNotes().isZero()) {
        throw Error("note duration must be positive", "note");
    }
    if (note.velocity < 1 || note.velocity > 127) {
        throw Error("note velocity " + std::to_string(note.velocity) + " is outside 1..127",
                    "note");
    }
}

}  // namespace

Fragment Fragment::fromNotes(std::vector<Note> notes, std::optional<Duration> span) {
    for (const Note& note : notes) {
        validateNote(note);
    }

    std::stable_sort(notes.begin(), notes.end(), [](const Note& lhs, const Note& rhs) {
        return lhs.offset < rhs.offset;
    });

    Duration computed;
    for (const Note& note : notes) {
        computed = std::max(computed, noteEnd(note));
    }

    if (span.has_value()) {
        if (span->inWholeNotes().isNegative()) {
            throw Error("fragment span must not be negative", "fragment");
        }
        if (*span < computed) {
            throw Error("fragment span " + span->toString() +
                            " is shorter than its content " + computed.toString(),
                        "fragment");
        }
        computed = *span;
    }

    Fragment fragment;
    fragment.notes_ = std::move(notes);
    fragment.duration_ = computed;
    return fragment;
}

Fragment Fragment::fromMelody(const std::vector<std::pair<Pitch, Rational>>& values) {
    std::vector<Note> notes;
    notes.reserve(values.size());

    Duration cursor;
    for (const auto& [pitch, length] : values) {
        Note note;
        note.pitch = pitch;
        note.offset = cursor;
        note.duration = Duration(length);
        notes.push_back(note);
        cursor += Duration(length);
    }
    return fromNotes(std::move(notes), cursor);
}

Duration Fragment::contentEnd() const {
    Duration end;
    for (const Note& note : notes_) {
        end = std::max(end, noteEnd(note));
    }
    return end;
}

}  // namespace mpl
