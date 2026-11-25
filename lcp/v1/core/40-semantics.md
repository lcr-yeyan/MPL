# MPL Semantic Contract

- All musical time is exact rational whole notes. `1/4` is a quarter note.
- `bars`/`beats` resolve against the enclosing section MeterMap; all positive
  bars require a bar-boundary anchor.
- A Section has its own meter/grouping. Duration must end on a bar boundary.
- A Clip's `play` expression is evaluated at its offset; clips on one track
  overlap only if their positions overlap.
- Track role is metadata; it does not change playback.
- MIDI/WAV export uses the arrangement; sections play in arrangement order.
- `random(seed,index)` is deterministic: same seed/index gives same number.
- No recursion, no global mutation, no I/O inside MPL.
- Functions are pure; a function must `return` an expression.
- `context meter` is required for bars/beats/metric functions.
- A program that compiles is not automatically musically correct: target
  observations must be checked independently.
