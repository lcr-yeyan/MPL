#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/music/Pitch.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace mpl {

/// A single sounding note. Offsets are relative to the enclosing fragment.
///
/// `origin` is compiler provenance, not musical content: it records the Music
/// IR object that introduced the note (a literal or an operator such as
/// harmonize). Free music transforms copy it unchanged, which lets the trace
/// follow every note back to its generating operator.
struct Note {
    Pitch pitch;
    Duration offset;
    Duration duration;
    int velocity = 96;
    ObjectId origin = kInvalidObjectId;
};

/// A span of music. Motif, Phrase and Pattern all lower to this value in
/// V0.0.1; their distinct semantics live in the IR node that produced them.
///
/// A fragment carries an explicit span so that reverse() and structural
/// operators are well defined even when the musical content does not fill the
/// whole span.
class Fragment {
public:
    Fragment() = default;

    static Fragment fromNotes(std::vector<Note> notes,
                              std::optional<Duration> span = std::nullopt);

    /// Lays out (pitch, duration) pairs one after another, starting at zero.
    static Fragment fromMelody(const std::vector<std::pair<Pitch, Rational>>& values);

    const std::vector<Note>& notes() const noexcept { return notes_; }
    const Duration& duration() const noexcept { return duration_; }
    bool empty() const noexcept { return notes_.empty(); }
    std::size_t noteCount() const noexcept { return notes_.size(); }

    /// Position just after the last sounding note.
    Duration contentEnd() const;

private:
    std::vector<Note> notes_;
    Duration duration_;
};

/// Motif and Phrase are roles carried by Music IR nodes. In V0.0.1 they share
/// the fragment value type; later versions can attach role-specific analysis.
using Motif = Fragment;
using Phrase = Fragment;

}  // namespace mpl
