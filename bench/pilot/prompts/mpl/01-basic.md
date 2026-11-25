Create one complete MPL program for this task.

Project: name `Basic`, tempo 120 bpm, meter 4/4, key C major.
Sound: `asset PianoAsset = instrument("GrandPiano");`
Notes: exactly four quarter notes in one bar: C4, D4, E4, F4 at offsets 0, 1/4, 1/2, 3/4, velocity 96.
Track: `Keys`, role melodic, using PianoAsset.
Section: `S` of 1 bar, clip `Keys` playing the notes.
Arrangement plays S. Export `midi("basic.mid")`.

Return only the program.
