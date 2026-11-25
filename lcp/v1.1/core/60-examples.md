# Verified Canonical Examples

Example 1 — basic notes:

```text
project Basic {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
let M = notes span 1 whole {
  G3 @ 0 for 1/4 vel 96,
  A3 @ 1/4 for 1/4 vel 96,
  B3 @ 1/2 for 1/4 vel 96,
  C4 @ 3/4 for 1/4 vel 96
};
track Keys { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Keys "Basic" { play M; } }
arrangement { play S; }
export midi("basic.mid");
```

Example 2 — user function:

```text
project Function {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
fn cell(root: Pitch, velocity: Int) -> Fragment {
  return sequence(note(root, 1/4, velocity), note(G4, 1/4, velocity));
}
let M = cell(D4, 96);
track Keys { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Keys "Fn" { play M; } }
arrangement { play S; }
export midi("function.mid");
```

Example 3 — 7/8 grouping:

```text
project Meter {
  tempo = 120 bpm;
  meter = 7/8;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
let M = notes span 7/8 whole {
  A3 @ 0 for 1/8 vel 104,
  B3 @ 1/8 for 1/8 vel 88,
  C4 @ 2/8 for 1/8 vel 88,
  D4 @ 3/8 for 1/8 vel 96,
  E4 @ 4/8 for 1/8 vel 88,
  F4 @ 5/8 for 1/8 vel 88,
  G4 @ 6/8 for 1/8 vel 92
};
track Keys { asset = PianoAsset; role = melodic; }
section S(1 bars) meter 7/8 grouping 2+2+3 { clip Keys "Meter" { play M; } }
arrangement { play S; }
export midi("meter.mid");
```

All examples are checked by the frozen compiler before inclusion.

Example 4 �� voicing plus rhythm (no list indexing):

```text
project Voices {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
let V1 = voicing(chord("Cmaj7"), C3, C5, 4, 0, 12);
let V2 = voiceLead(voicingNotes(V1), chord("Fmaj7"), C3, C5, 7, 12, false);
let R = rhythm { hit 1/8 accent 2, hit 1/8, hit 1/8, hit 1/8 };
let M = sequence(cycle(R, voicingNotes(V1), 96), cycle(R, voicingNotes(V2), 96));
track Keys { asset = PianoAsset; role = harmony; }
section S(2 bars) { clip Keys "Voices" { play M; } }
arrangement { play S; }
export midi("voices.mid");
```
