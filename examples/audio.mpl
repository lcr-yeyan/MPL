// MPL V0.2.0 audio case: multi-track, multiple timbres, expression and
// automation rendered by the reference audio backend.

project Audio {
  tempo = 160 bpm;
  meter = 4/4;
  key = F minor;
}

asset GrandPiano = instrument("GrandPiano");
asset WarmPad = instrument("WarmPad");
asset SubBass = instrument("SubBass");
asset DrumKit = sample("DrumKit");

let A = melody {
  F4: 1/8,
  Ab4: 1/8,
  C5: 1/4,
  Eb5: 1/4,
  C5: 1/4
};

let RootF = melody { F2: 1 whole };
let RootDb = melody { Db3: 1 whole };
let RootAb = melody { Ab2: 1 whole };
let RootEb = melody { Eb3: 1 whole };
let Progression = sequence(RootF, RootDb, RootAb, RootEb);
let ProgressionEight = repeat(Progression, 2);
let PadVoicing = harmonize(ProgressionEight, [2, 4]);
let BassRoots = transpose(ProgressionEight, -12 st);

let DrumPattern = notes span 1 bars {
  C2 @ 0 for 1/8 vel 110,
  D2 @ 1/4 for 1/8 vel 100,
  C2 @ 1/2 for 1/8 vel 110,
  D2 @ 3/4 for 1/8 vel 100,
  F#2 @ 1/8 for 1/16 vel 70,
  F#2 @ 3/8 for 1/16 vel 70,
  F#2 @ 5/8 for 1/16 vel 70,
  F#2 @ 7/8 for 1/16 vel 70
};
let DrumLoop = repeat(DrumPattern, 8);

curve Crescendo on relative = linear(0.5, 1.1);
curve Humanize on relative = keyframes {
  0: 0.0 linear,
  1/4: 0.02 step,
  3/4: -0.02 linear,
  1: 0.0 linear
};
curve Volume on musical for 8 bars = linear(0.55, 1.0);

track Piano {
  asset = GrandPiano;
  role = melodic;
}
track Pad {
  asset = WarmPad;
  role = harmony;
}
track Bass {
  asset = SubBass;
  role = bass;
}
track Drums {
  asset = DrumKit;
  role = percussion;
}

section Audio(8 bars) {
  clip Piano "Crescendo A" at 0 bars {
    play A;
    velocity = Crescendo;
  }
  clip Piano "Humanized A" at 2 bars {
    play A;
    velocity = Crescendo;
    timing = Humanize;
  }
  clip Pad "Pad" {
    play PadVoicing;
  }
  clip Bass "Bass" {
    play BassRoots;
  }
  clip Drums "Drums" {
    play DrumLoop;
  }
}

arrangement {
  play Audio;
}

automation Piano.volume = Volume;
automation master.volume = Volume;

export midi("audio.mid");
export wav("audio.wav");
