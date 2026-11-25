Create one complete MPL program for this task.

Project: name `Function`, tempo 120 bpm, meter 4/4, key C major.
Sound: `asset PianoAsset = instrument("GrandPiano");`
Define a function `cell(root: Pitch, velocity: Int) -> Fragment` that returns two quarter notes: `root`, then E4. Call it with C4 and velocity 96. The resulting fragment must have exactly two notes at offsets 0 and 1/4.
Track: `Keys`, role melodic.
Section: `S` of 1 bar playing the fragment. Arrangement plays S. Export `midi("function.mid")`.

Return only the program.
