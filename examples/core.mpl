// MPL V0.1.0 reference regression case: equivalent to buildCoreDemoProject().

project Core {
  tempo = 160 bpm;
  meter = 4/4;
  key = F minor;
}

asset GrandPiano = instrument("GrandPiano");
asset SubBass = instrument("SubBass");

let A = melody {
  F4: 1/8,
  Ab4: 1/8,
  C5: 1/4,
  Eb5: 1/4,
  C5: 1/4
};

let B = transpose(A, 12 st);
let C = reverse(A);
let Main = sequence(A, B, C, A);
let BassLine = transpose(Main, -24 st);

track Piano {
  asset = GrandPiano;
  role = melodic;
}

track Bass {
  asset = SubBass;
  role = bass;
}

section Core(4 bars) {
  clip Piano "Main" {
    play Main;
  }

  clip Bass "Bass" {
    play BassLine;
  }
}

arrangement {
  play Core;
}

export midi("core.mid");
