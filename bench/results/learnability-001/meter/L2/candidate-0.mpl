project Meter {
  tempo = 120 bpm;
  meter = 7/8;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let M = notes span 7/8 whole {
  D4 @ 0 for 1/8 vel 104,
  E4 @ 1/8 for 1/8 vel 88,
  F4 @ 2/8 for 1/8 vel 88,
  G4 @ 3/8 for 1/8 vel 96,
  A4 @ 4/8 for 1/8 vel 88,
  Bb4 @ 5/8 for 1/8 vel 88,
  C5 @ 6/8 for 1/8 vel 92
};

track Keys {
  asset = PianoAsset;
  role = melodic;
}

section S(1 bars) meter 7/8 grouping 2+2+3 {
  clip Keys "Meter" {
    play M;
  }
}

arrangement {
  play S;
}

export midi("meter.mid");