// MPL V0.1.0 suite regression case: equivalent to buildSuiteDemoProject().

project Suite {
  tempo = 160 bpm;
  meter = 4/4;
  key = F minor;
}

asset GrandPiano = instrument("GrandPiano");
asset WarmPad = instrument("WarmPad");
asset SubBass = instrument("SubBass");
asset LeadSynth = instrument("LeadSynth");
asset DrumKit = sample("DrumKit");

let A = melody {
  F4: 1/8,
  Ab4: 1/8,
  C5: 1/4,
  Eb5: 1/4,
  C5: 1/4
};

let A1 = diatonicTranspose(A, 1);
let B = diatonicTranspose(A, 2);
let RevA = reverse(A);
let RevB = reverse(B);

let IntroPiano = sequence(A, A1, RevA, A);
let DropAPiano = sequence(A, B, A1, RevB, A, B, RevA, A1);
let DropBPiano = sequence(B, A1, A, B, RevB, A1, A, RevA);
let OutroPiano = sequence(A, RevA, A1, A);

let RootF = melody { F2: 1 whole };
let RootDb = melody { Db3: 1 whole };
let RootAb = melody { Ab2: 1 whole };
let RootEb = melody { Eb3: 1 whole };
let Progression = sequence(RootF, RootDb, RootAb, RootEb);
let ProgressionEight = repeat(Progression, 2);
let PadVoicing = harmonize(ProgressionEight, [2, 4]);
let BassRoots = transpose(ProgressionEight, -12 st);

let SyncopatedA = syncopate(A, 1/8);
let LeadMotif = repeat(SyncopatedA, 8);

let DrumPattern = notes span 1 bars {
  C2 @ 0 for 1/8 vel 110,
  D2 @ 1/4 for 1/8 vel 100,
  C2 @ 1/2 for 1/8 vel 110,
  D2 @ 3/4 for 1/8 vel 100,
  F#2 @ 0 for 1/16 vel 72,
  F#2 @ 1/8 for 1/16 vel 72,
  F#2 @ 1/4 for 1/16 vel 72,
  F#2 @ 3/8 for 1/16 vel 72,
  F#2 @ 1/2 for 1/16 vel 72,
  F#2 @ 5/8 for 1/16 vel 72,
  F#2 @ 3/4 for 1/16 vel 72,
  F#2 @ 7/8 for 1/16 vel 72
};

let DrumLoop = repeat(DrumPattern, 8);

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

track Lead {
  asset = LeadSynth;
  role = melodic;
}

track Drums {
  asset = DrumKit;
  role = percussion;
}

section Intro(4 bars) {
  clip Piano "Intro Piano" {
    play IntroPiano;
  }
}

section DropA(8 bars) {
  clip Piano "Drop A" {
    play DropAPiano;
  }
  clip Pad "Drop A" {
    play PadVoicing;
  }
  clip Bass "Drop A" {
    play BassRoots;
  }
  clip Drums "Drop A" {
    play DrumLoop;
  }}

section DropB(8 bars) {
  clip Piano "Drop B" {
    play DropBPiano;
  }
  clip Pad "Drop B" {
    play PadVoicing;
  }
  clip Bass "Drop B" {
    play BassRoots;
  }
  clip Lead "Drop B" {
    play LeadMotif;
  }
  clip Drums "Drop B" {
    play DrumLoop;
  }}

section Outro(4 bars) {
  clip Piano "Outro" {
    play OutroPiano;
  }
}

arrangement {
  play Intro;
  play DropA;
  play DropB;
  play Outro;
}

export midi("suite.mid");
