# MPL Standard Library (Stdlib 0.3.0)

Only programs importing a module may use these exports. Core-only experiments
must not include this file or import `std/...`.

`std/rhythm.mpl`:
- `pulseGrid(root: Pitch, unit: Duration, count: Int, velocity: Int) -> Fragment`
- `accentGrid(root: Pitch, unit: Duration, count: Int, velocity: Int) -> Fragment`
- `accentRhythm(r: Rhythm, root: Pitch, velocity: Int) -> Fragment context meter`

`std/harmony.mpl`:
- `chordNotes(name: String, low: Pitch, high: Pitch, voices: Int, inversion: Int) -> [Pitch]`
- `arpBar(r: Rhythm, h: Harmony, bar: Int, low: Pitch, high: Pitch, voices: Int, velocity: Int) -> Fragment context meter`
- `leadBar(previous: [Pitch], h: Harmony, bar: Int, low: Pitch, high: Pitch, maxMove: Int) -> [Pitch] context meter`
- `voicedBarAt(r, h, at: Duration, low, high, voices, inversion, maxSpacing, velocity) -> Fragment`
- `arpBarAt(...)`, `bassBarAt(...)`, `leadBarAt(...)`

`std/voicing.mpl`:
- `closeVoicing(name, low, high, voices, inversion) -> [Pitch]`
- `nextVoicing(previous, name, low, high, maxMove) -> [Pitch]`
- `nextVoicingResult(...) -> Voicing`
- `sharedTones(a: [Pitch], b: [Pitch]) -> [Pitch]`

Imports must appear before `project`:

```text
import "std/harmony.mpl";
project X { ... }
```

Do not claim a stdlib function exists if it is not listed here.
