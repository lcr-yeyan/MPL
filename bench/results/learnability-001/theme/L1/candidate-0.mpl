project Theme {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

let Cell = notes span 1 whole {
  C4 @ 0 for 1/4 vel 92,
  D4 @ 1/4 for 1/4 vel 88,
  E4 @ 1/2 for 1/4 vel 84
};

fn Answer() -> Fragment context key {
  return diatonicTranspose(Cell, 2);
}

let ThemeCell = sequence {
  play Cell;
  play Answer();
};

track Keys {
  asset = PianoAsset;
  role = melodic;
}

section S(2 bars) {
  clip Keys "Theme" {
    play ThemeCell;
  }
}

arrangement {
  play S;
}

export midi("theme.mid");