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
  G4 @ 3/4 for 1/4 vel 96
};

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "Basic" { play M; } }
arrangement { play S; }
export midi("basic.mid");