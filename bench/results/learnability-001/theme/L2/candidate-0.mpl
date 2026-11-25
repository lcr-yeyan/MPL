project Theme {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

fn answerCell() -> Fragment context key {
  return notes span 1 whole {
    E4 @ 0 for 1/4 vel 92,
    F4 @ 1/4 for 1/4 vel 88,
    G4 @ 1/2 for 1/4 vel 84
  };
}

let Cell = notes span 1 whole {
  C4 @ 0 for 1/4 vel 92,
  D4 @ 1/4 for 1/4 vel 88,
  E4 @ 1/2 for 1/4 vel 84
};

let Answer = answerCell();

let M = sequence(Cell, Answer);

track Keys {
  asset = PianoAsset;
  role = melodic;
}

section S(2 bars) {
  clip Keys "Theme" {
    play M;
  }
}

arrangement {
  play S;
}

export midi("theme.mid");