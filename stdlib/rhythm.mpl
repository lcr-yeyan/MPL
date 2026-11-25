// MPL standard library: rhythm construction helpers.
// Library modules may only declare functions, lets and curves.
module StdRhythm;

/// A uniform attack grid.
export fn pulseGrid(root: Pitch, unit: Duration, count: Int, velocity: Int) -> Fragment {
  let parts = for i in range(0, count) {
    note(root, unit, velocity)
  };
  return sequenceAll(parts);
}

/// A pulse grid with a heavier downbeat every four attacks.
export fn accentGrid(root: Pitch, unit: Duration, count: Int, velocity: Int) -> Fragment {
  let parts = for i in range(0, count) {
    if i % 4 == 0 {
      note(root, unit, velocity + 12)
    } else {
      note(root, unit, velocity)
    }
  };
  return sequenceAll(parts);
}

/// Applies a rhythm's metric accents to a single pitch.
export fn accentRhythm(r: Rhythm, root: Pitch, velocity: Int) -> Fragment context meter {
  return cycle(metricAccent(r), [root], velocity);
}
