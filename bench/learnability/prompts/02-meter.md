Create one complete MPL program for this task.

Project: name `Meter`, tempo 120 bpm, meter 7/8, key D minor.
Sound: `asset PianoAsset = instrument("GrandPiano");`
Notes: exactly seven eighth notes in one 7/8 bar: D4, E4, F4, G4, A4, Bb4, C5 at offsets 0, 1/8, 2/8, 3/8, 4/8, 5/8, 6/8, velocities 104/88/88/96/88/88/92.
Track: `Keys`, role melodic.
Section: `S` of 1 bar with `meter 7/8 grouping 2+2+3`.
Arrangement plays S. Export `midi("meter.mid")`.

Return only the program.
