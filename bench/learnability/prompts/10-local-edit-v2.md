Create one complete MPL program by editing the given program.

Change ONLY the last note F4 to G4. Keep every identifier unchanged: project Basic, asset PianoAsset, track Piano, clip Piano "Basic", section S, file basic.mid. Do not rename track Piano to Keys or copy names from examples. The final pitch sequence must be C4, D4, E4, G4.

Starting program:

```text
project Basic {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

let M = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  D4 @ 1/4 for 1/4 vel 96,
  E4 @ 1/2 for 1/4 vel 96,
  F4 @ 3/4 for 1/4 vel 96
};

track Keys { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Keys "Basic" { play M; } }
arrangement { play S; }
export midi("basic.mid");
```

Return only the complete edited program.
