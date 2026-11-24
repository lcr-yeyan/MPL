#pragma once

#include "mpl/core/Ids.hpp"
#include "mpl/core/Rational.hpp"
#include "mpl/core/Time.hpp"
#include "mpl/music/Fragment.hpp"
#include "mpl/music/Key.hpp"
#include "mpl/music/Pitch.hpp"

#include <vector>

namespace mpl {

// Experimental harmony operators introduced in V0.0.2. They are pure music
// functions; the compiler supplies the key/meter context and records the
// dependency in the Music IR node.

/// Moves a pitch by diatonic scale steps inside `key`, preserving the input's
/// chromatic alteration and its spelling where possible.
Pitch diatonicTranspose(const Pitch& pitch, int scaleSteps, const Key& key);

Fragment diatonicTranspose(const Fragment& input, int scaleSteps, const Key& key);

/// Adds chord tones at the given scale-degree offsets above every note.
/// Generated notes carry `generatedOrigin` as provenance.
Fragment harmonize(const Fragment& input, const std::vector<int>& degreeOffsets, const Key& key,
                   int velocity = 80, ObjectId generatedOrigin = kInvalidObjectId);

/// Displaces notes that start exactly on a beat by `displacement`. This is a
/// deliberately simple metric operator; it is not a model of musical tension.
Fragment syncopate(const Fragment& input, const Meter& meter, const Rational& displacement);

}  // namespace mpl
