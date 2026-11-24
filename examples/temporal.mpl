// MPL V0.1.0 temporal regression case: equivalent to buildTemporalDemoProject().

project Temporal {
  tempo = 160 bpm;
  meter = 4/4;
  key = F minor;
}

asset GrandPiano = instrument("GrandPiano");
asset WarmPad = instrument("WarmPad");
asset SubBass = instrument("SubBass");

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

curve Crescendo on relative = linear(0.55, 1.15);
curve Decrescendo on relative = smoothstep(1.15, 0.5);
curve Humanize on relative = keyframes {
  0: 0.0 linear,
  1/4: 0.04 step,
  3/4: -0.03 linear,
  1: 0.0 linear
};
curve Steady on relative = constant(0.9);
curve Gate on relative = constant(1.0);
curve PerNotePan on relative = constant(0.0);

curve Volume on musical for 4 bars = linear(0.55, 1.0);
let VolumeX2 = stretch(Volume, 2);
let VolumeRev = reverse(Volume);

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

section Temporal(8 bars) {
  clip Piano "A Linear Crescendo" at 0 bars {
    play A;
    velocity = Crescendo;
  }

  clip Piano "A Smooth Decrescendo" at 1 bars {
    play A;
    velocity = Decrescendo;
  }

  clip Piano "A Humanized" at 2 bars {
    play A;
    velocity = Steady;
    timing = Humanize;
    gate = Gate;
    pan = PerNotePan;
  }

  clip Pad "Pad" {
    play PadVoicing;
  }

  clip Bass "Bass" {
    play BassRoots;
  }
}

arrangement {
  play Temporal;
}

automation Piano.volume = Volume;
automation Piano.modulation = VolumeX2 add;
automation Piano.expression = VolumeRev;
automation master.volume = VolumeRev;

export midi("temporal.mid");
