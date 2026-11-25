// MPL standard library: harmonic timeline and rhythm/harmony combination.
module StdHarmony;

/// Chord tones as concrete pitches inside a range.
export fn chordNotes(name: String, low: Pitch, high: Pitch, voices: Int,
                     inversion: Int) -> [Pitch] {
  return voicingNotes(voicing(chord(name), low, high, voices, inversion, 12));
}

/// One bar of a rhythm played against the chord active at an absolute time.
export fn voicedBarAt(r: Rhythm, h: Harmony, at: Duration, low: Pitch, high: Pitch,
                      voices: Int, inversion: Int, maxSpacing: Int,
                      velocity: Int) -> Fragment {
  let c = chordAt(h, at);
  return cycle(r, voicingNotes(voicing(c, low, high, voices, inversion, maxSpacing)),
               velocity);
}

/// Close-position version of voicedBarAt.
export fn arpBarAt(r: Rhythm, h: Harmony, at: Duration, low: Pitch, high: Pitch,
                   voices: Int, velocity: Int) -> Fragment {
  return voicedBarAt(r, h, at, low, high, voices, 0, 12, velocity);
}

/// Bass register: root-position voicing, usually one or two voices.
export fn bassBarAt(r: Rhythm, h: Harmony, at: Duration, low: Pitch, high: Pitch,
                    voices: Int, velocity: Int) -> Fragment {
  return voicedBarAt(r, h, at, low, high, voices, 0, 12, velocity);
}

/// Voice-leading result to the chord active at an absolute time.
export fn leadBarAt(previous: [Pitch], h: Harmony, at: Duration, low: Pitch,
                    high: Pitch, maxMove: Int, maxSpacing: Int,
                    allowCrossing: Bool) -> Voicing {
  return voiceLead(previous, chordAt(h, at), low, high, maxMove, maxSpacing,
                   allowCrossing);
}

/// One bar of a rhythm played against the chord active at that bar.
export fn arpBar(r: Rhythm, h: Harmony, bar: Int, low: Pitch, high: Pitch,
                 voices: Int, velocity: Int) -> Fragment context meter {
  let c = chordAtBar(h, bar);
  return cycle(r, voicingNotes(voicing(c, low, high, voices, 0, 12)), velocity);
}

/// Voice-leads a previous voicing to the chord active at `bar`.
export fn leadBar(previous: [Pitch], h: Harmony, bar: Int, low: Pitch, high: Pitch,
                  maxMove: Int) -> [Pitch] context meter {
  let c = chordAtBar(h, bar);
  return voicingNotes(voiceLead(previous, c, low, high, maxMove, 12, false));
}

/// The chord active at an integer bar index.
export fn harmonyBarChord(h: Harmony, bar: Int) -> Chord context meter {
  return chordAtBar(h, bar);
}
