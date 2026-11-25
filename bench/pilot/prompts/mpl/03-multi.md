Create one complete MPL program for this task.

Project: name `Poly`, tempo 120 bpm, meter 4/4, key C major.
Sound: `asset PianoAsset = instrument("GrandPiano");`
Create rhythm A with three 1/8 hits. Create rhythm B with two 1/4 hits. Compute their common period. Realign A to that common period and cycle it over pitches C4, E4, G4 (three attacks per cell). The resulting fragment must have 12 attacks: onsets 0,1/8,1/4,3/8,1/2,5/8,3/4,7/8,1,9/8,5/4,11/8, pitches cycling C4,E4,G4.
Track `Keys`, role melodic. Section `S` 2 bars. Arrangement plays S. Export `midi("poly.mid")`.

Return only the program.
