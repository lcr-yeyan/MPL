// MPL standard library: chord voicing and voice-leading helpers.
module StdVoicing;

/// Close voicing with an explicit inversion, returned as pitches.
export fn closeVoicing(name: String, low: Pitch, high: Pitch, voices: Int,
                       inversion: Int) -> [Pitch] {
  return voicingNotes(voicing(chord(name), low, high, voices, inversion, 12));
}

/// Deterministic bounded voice leading, returned as pitches.
export fn nextVoicing(previous: [Pitch], name: String, low: Pitch, high: Pitch,
                      maxMove: Int) -> [Pitch] {
  return voicingNotes(voiceLead(previous, chord(name), low, high, maxMove, 12, false));
}

/// Full voice-leading result, for status/movement queries.
export fn nextVoicingResult(previous: [Pitch], name: String, low: Pitch, high: Pitch,
                            maxMove: Int) -> Voicing {
  return voiceLead(previous, chord(name), low, high, maxMove, 12, false);
}

/// Common tones between two pitch lists.
export fn sharedTones(a: [Pitch], b: [Pitch]) -> [Pitch] {
  return commonTones(a, b);
}
