#include "mpl/music/Transforms.hpp"

#include "mpl/core/Error.hpp"

#include <algorithm>
#include <utility>

namespace mpl {

Fragment transpose(const Fragment& input, Interval interval) {
    std::vector<Note> notes = input.notes();
    for (Note& note : notes) {
        note.pitch = note.pitch.transposed(interval);
    }
    return Fragment::fromNotes(std::move(notes), input.duration());
}

Fragment reverse(const Fragment& input) {
    std::vector<Note> notes = input.notes();
    const Duration span = input.duration();
    for (Note& note : notes) {
        note.offset = span - (note.offset + note.duration);
    }
    return Fragment::fromNotes(std::move(notes), span);
}

Fragment stretch(const Fragment& input, const Rational& factor) {
    if (!factor.isPositive()) {
        throw Error("stretch factor must be positive, got " + factor.toString(), "stretch");
    }
    std::vector<Note> notes = input.notes();
    for (Note& note : notes) {
        note.offset = Duration(note.offset.inWholeNotes() * factor);
        note.duration = Duration(note.duration.inWholeNotes() * factor);
    }
    return Fragment::fromNotes(std::move(notes), Duration(input.duration().inWholeNotes() * factor));
}

Fragment repeat(const Fragment& input, int count) {
    if (count < 1) {
        throw Error("repeat count must be at least 1, got " + std::to_string(count), "repeat");
    }
    if (count == 1) {
        return input;
    }

    std::vector<Note> notes;
    notes.reserve(input.noteCount() * static_cast<std::size_t>(count));
    const Duration span = input.duration();
    for (int copy = 0; copy < count; ++copy) {
        const Duration shift = span * Rational(copy, 1);
        for (const Note& note : input.notes()) {
            Note shifted = note;
            shifted.offset = note.offset + shift;
            notes.push_back(shifted);
        }
    }
    return Fragment::fromNotes(std::move(notes), span * Rational(count, 1));
}

Fragment sequence(const std::vector<Fragment>& parts) {
    if (parts.empty()) {
        throw Error("sequence requires at least one part", "sequence");
    }

    std::vector<Note> notes;
    Duration cursor;
    for (const Fragment& part : parts) {
        for (const Note& note : part.notes()) {
            Note shifted = note;
            shifted.offset = note.offset + cursor;
            notes.push_back(shifted);
        }
        cursor += part.duration();
    }
    return Fragment::fromNotes(std::move(notes), cursor);
}

Fragment parallel(const std::vector<Fragment>& parts) {
    if (parts.empty()) {
        throw Error("parallel requires at least one part", "parallel");
    }

    std::vector<Note> notes;
    Duration span;
    for (const Fragment& part : parts) {
        notes.insert(notes.end(), part.notes().begin(), part.notes().end());
        span = std::max(span, part.duration());
    }
    return Fragment::fromNotes(std::move(notes), span);
}

namespace {

Rational positiveModulo(const Rational& value, const Rational& period) {
    if (!period.isPositive()) {
        return Rational(0, 1);
    }
    Rational result = value;
    const Rational quotient = result / period;
    const std::int64_t whole = quotient.numerator() / quotient.denominator();
    result -= period * Rational(whole, 1);
    if (result.isNegative()) {
        result += period;
    }
    return result;
}

}  // namespace

Fragment rotate(const Fragment& input, const Rational& amount) {
    const Rational span = input.duration().inWholeNotes();
    if (!span.isPositive() || span.isZero()) {
        return input;
    }
    const Rational shift = positiveModulo(amount, span);
    std::vector<Note> notes;
    for (const Note& note : input.notes()) {
        Note rotated = note;
        const Rational offset = positiveModulo(note.offset.inWholeNotes() + shift, span);
        const Rational remaining = span - offset;
        const Rational duration = note.duration.inWholeNotes();
        rotated.offset = Duration(offset);
        rotated.duration = Duration(duration < remaining ? duration : remaining);
        if (rotated.duration.inWholeNotes().isPositive()) {
            notes.push_back(rotated);
        }
    }
    return Fragment::fromNotes(std::move(notes), input.duration());
}

}  // namespace mpl
