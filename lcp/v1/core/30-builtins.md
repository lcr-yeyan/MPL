# MPL Builtin Signatures (Core)

Fragment:

- `note(pitch: Pitch, duration: Duration, velocity: Int) -> Fragment`
- `rest(duration: Duration) -> Fragment`
- `sequence(a: Fragment, b: Fragment, ...) -> Fragment`
- `parallel(a: Fragment, b: Fragment, ...) -> Fragment`
- `repeat(f: Fragment, count: Int) -> Fragment`
- `stretch(f: Fragment, factor: Number) -> Fragment`
- `transpose(f: Fragment, semitones: Int) -> Fragment`
- `diatonicTranspose(f: Fragment, degrees: Int) -> Fragment context key`
- `reverse(f: Fragment) -> Fragment`
- `shift(f: Fragment, offset: Duration) -> Fragment`
- `syncopate(f: Fragment, amount: Duration) -> Fragment context meter`
- `harmonize(f: Fragment, key: Key, mode: String) -> Fragment` (legacy helper)

Lists/numbers:

- `range(start: Int, count: Int) -> [Int]`
- `length(list) -> Int`
- `round(x: Number) -> Int`
- `ratio(a: Number, b: Number) -> Number`
- `random(seed: Int, index: Int) -> Number`

Rhythm:

- `rhythm { hit 1/8, rest 1/8 }`
- `overlay`, `union`, `intersect`, `difference`, `complement`, `subdivide`,
  `accent`, `realign`, `commonPeriod`, `period`, `metricAccent`, `zip`, `cycle`.

Harmony/voicing (from RC):

- `chord(name: String) -> Chord`
- `chordAt(h: Harmony, at: Duration) -> Chord`
- `chordAtBar(h: Harmony, bar: Int) -> Chord context meter`
- `voicing(c: Chord, low: Pitch, high: Pitch, voices: Int, inversion: Int, maxSpacing: Int) -> Voicing`
- `voiceLead(previous: [Pitch], c: Chord, low: Pitch, high: Pitch, maxMove: Int, maxSpacing: Int, allowCrossing: Bool) -> Voicing`
- `voicingNotes(v: Voicing) -> [Pitch]`
- `voicingStatus`, `voicingMovement`, `voicingCrossings`, `voicingComplete`
- `commonTones(a: [Pitch], b: [Pitch]) -> [Pitch]`
- `interval(a: Pitch, b: Pitch) -> Int`, `pitchClass(p: Pitch) -> Int`
