Create one complete MPL program for this task.

Project: name `Theme`, tempo 120 bpm, meter 4/4, key C major.
Sound: `asset PianoAsset = instrument("GrandPiano");`
Create a 3/4 theme cell: C4, D4, E4 each a quarter note at offsets 0, 1/4, 1/2, velocities 92/88/84. Then create a diatonic transposed answer two degrees up, and sequence cell then answer.
Track: `Keys`, role melodic.
Section: `S` of 2 bars, one clip playing the six notes.
Arrangement plays S. Export `midi("theme.mid")`.

Return only the program.
