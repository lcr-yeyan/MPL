#pragma once

#include "mpl/core/Rational.hpp"
#include "mpl/music/Fragment.hpp"
#include "mpl/music/Pitch.hpp"

#include <vector>

namespace mpl {

// Deterministic music operators. These are directly evaluable at compile time
// and form the initial std.melody / std.rhythm primitive set.

Fragment transpose(const Fragment& input, Interval interval);

/// Mirrors the fragment in time inside its own span.
Fragment reverse(const Fragment& input);

/// Scales offsets, note lengths and the fragment span by a positive factor.
Fragment stretch(const Fragment& input, const Rational& factor);

/// Concatenates `count` copies of the fragment in time.
Fragment repeat(const Fragment& input, int count);

/// Concatenates fragments in order.
Fragment sequence(const std::vector<Fragment>& parts);

/// Overlays fragments at the same start position.
Fragment parallel(const std::vector<Fragment>& parts);

/// Cyclic phase shift inside the fragment span. Onsets move by `amount`
/// modulo the span; durations are unchanged. This is a period/phase primitive,
/// not an artistic delay.
Fragment rotate(const Fragment& input, const Rational& amount);

}  // namespace mpl
