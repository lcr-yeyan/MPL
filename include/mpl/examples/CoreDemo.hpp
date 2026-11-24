#pragma once

#include "mpl/Project.hpp"

namespace mpl::examples {

/// The V0.0.1 acceptance program, kept as the minimal regression case: a
/// one-bar motif developed into a four-bar phrase and doubled in the bass.
Project buildCoreDemoProject();

/// The V0.0.2 integration case: four sections, five roles, harmonic
/// dependency (pad and bass derive from a progression) and motivic
/// development through diatonic transposition and syncopation.
Project buildSuiteDemoProject();

/// The V0.0.3 temporal case: three independent instances of one motif with
/// different performance expressions, track/master automation, curve reuse and
/// time transforms.
Project buildTemporalDemoProject();

}  // namespace mpl::examples
