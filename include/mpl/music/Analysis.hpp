#pragma once

#include "mpl/music/Fragment.hpp"
#include "mpl/music/Pitch.hpp"

#include <vector>

namespace mpl {

/// Basic analysis helpers. They report facts; they do not declare a musical
/// error unless a caller explicitly asks for a validation rule.

/// Counts pairs of notes whose sounding intervals overlap. Coordinates are the
/// fragments' own local time; use clips for absolute-time analysis.
int noteIntervalOverlap(const Fragment& a, const Fragment& b);

/// Highest minus lowest MIDI note in the fragment (0 for an empty fragment).
int soundingRange(const Fragment& fragment);

/// Counts parallel perfect fifths/octaves between two equal-length voice lines.
/// This is a validation helper, not a default rule: callers must invoke it to
/// make parallel motion a violation.
int parallelPerfects(const std::vector<Pitch>& lower, const std::vector<Pitch>& upper);

/// True when two notes share any positive-length sounding interval.
bool notesOverlap(const Note& a, const Note& b);

}  // namespace mpl
