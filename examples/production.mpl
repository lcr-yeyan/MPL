// MPL V0.2.0-beta Production Demo
//
// Exercises the independent runtime end to end:
// multi-section arrangement, multiple tracks and timbres, performance
// expression, volume and filter automation, EQ, compressor, reverb send,
// sidechain ducking and a mastered stereo WAV.
//
// Build with:
//   mplc build examples/production.mpl -o build/production --tail 1.5

project Production {
  tempo = 96 bpm;
  meter = 4/4;
  key = D minor;
}

asset GrandPiano = instrument("GrandPiano");
asset WarmPad = instrument("WarmPad");
asset SubBass = instrument("SubBass");
asset DrumKit = sample("DrumKit");
asset SquareLead = instrument("SquareLead");

let IntroMotif = notes span 2 bars {
  D4 @ 0 for 1/2 vel 82,
  F4 @ 1/2 for 1/4 vel 76,
  A4 @ 3/4 for 1/4 vel 78,
  G4 @ 1 for 1/2 vel 74,
  F4 @ 3/2 for 1/4 vel 70,
  E4 @ 7/4 for 1/4 vel 68
};

let VerseMotif = notes span 4 bars {
  D4 @ 0 for 1/2 vel 80,
  F4 @ 1/2 for 1/4 vel 76,
  A4 @ 3/4 for 1/4 vel 78,
  C5 @ 1 for 1/2 vel 86,
  A4 @ 3/2 for 1/2 vel 78,
  G4 @ 2 for 1/2 vel 74,
  F4 @ 5/2 for 1/4 vel 72,
  E4 @ 11/4 for 1/4 vel 70,
  D4 @ 3 for 1/2 vel 76,
  A3 @ 7/2 for 1/2 vel 70
};

let ChorusMotif = notes span 4 bars {
  A4 @ 0 for 1/2 vel 92,
  D5 @ 1/2 for 1/2 vel 96,
  C5 @ 1 for 1/4 vel 90,
  A4 @ 5/4 for 1/4 vel 86,
  G4 @ 3/2 for 1/2 vel 88,
  F4 @ 2 for 1/2 vel 84,
  A4 @ 5/2 for 1/2 vel 88,
  D5 @ 3 for 1/2 vel 94,
  F5 @ 7/2 for 1/2 vel 98
};

let ChorusLead = notes span 4 bars {
  D5 @ 0 for 1/4 vel 100,
  F5 @ 1/4 for 1/4 vel 104,
  A5 @ 1/2 for 1/2 vel 108,
  G5 @ 1 for 1/4 vel 102,
  F5 @ 5/4 for 1/4 vel 98,
  D5 @ 3/2 for 1/2 vel 96,
  C5 @ 2 for 1/4 vel 94,
  D5 @ 9/4 for 1/4 vel 98,
  F5 @ 5/2 for 1/2 vel 104,
  A5 @ 3 for 1/2 vel 110,
  D6 @ 7/2 for 1/2 vel 112
};

let IntroPad = melody { D3: 2 bars };
let VersePad = sequence(
  melody { D3: 1 bars },
  melody { Bb2: 1 bars },
  melody { F2: 1 bars },
  melody { C3: 1 bars }
);
let BassLine = sequence(
  melody { D2: 1 bars },
  melody { Bb1: 1 bars },
  melody { F1: 1 bars },
  melody { C2: 1 bars }
);

let DrumBeat = notes span 1 bars {
  C2 @ 0 for 1/8 vel 112,
  D2 @ 1/4 for 1/8 vel 104,
  C2 @ 1/2 for 1/8 vel 112,
  D2 @ 3/4 for 1/8 vel 104,
  F#2 @ 1/8 for 1/16 vel 62,
  F#2 @ 3/8 for 1/16 vel 62,
  F#2 @ 5/8 for 1/16 vel 62,
  F#2 @ 7/8 for 1/16 vel 62
};
let DrumLoop = repeat(DrumBeat, 4);

curve Crescendo on relative = linear(0.6, 1.1);
curve LeadCrescendo on relative = linear(0.8, 1.0);
curve Humanize on relative = keyframes {
  0: 0.0 linear,
  1/4: 0.012 step,
  3/4: -0.012 linear,
  1: 0.0 linear
};
curve PianoVolume on musical for 10 bars = keyframes {
  0: 0.7 linear,
  2: 0.9 linear,
  6: 0.8 linear,
  10: 1.0 smoothstep
};
curve PianoSweep on musical for 10 bars = keyframes {
  0: 500 linear,
  3: 3500 smoothstep,
  6: 800 linear,
  10: 5000 smoothstep
};
curve HallDepth on musical for 10 bars = linear(0.2, 0.5);
curve DuckThreshold on musical for 10 bars = linear(-30, -20);

processor EqPiano = biquad {
  mode = peaking;
  frequency = 1200 Hz;
  q = 1.2;
  gain = 4 dB;
};

processor LeadFilter = biquad {
  mode = highpass;
  frequency = 300 Hz;
  q = 0.8;
};

processor BassComp = compressor {
  threshold = -20 dB;
  ratio = 4;
  attack = 8 ms;
  release = 140 ms;
  makeup = 2 dB;
};

processor Hall = reverb {
  room = 0.75;
  damping = 0.35;
  mix = 0.8;
};

processor MasterComp = compressor {
  threshold = -12 dB;
  ratio = 2;
  attack = 20 ms;
  release = 250 ms;
  makeup = 1 dB;
};

bus Reverb {
  insert Hall;
  gain = -4 dB;
}

track Piano {
  asset = GrandPiano;
  role = melodic;
  gain = -2 dB;
  pan = -0.15;
  insert EqPiano;
  send 0.3 to Reverb;
}

track Pad {
  asset = WarmPad;
  role = harmony;
  gain = -8 dB;
  pan = 0.2;
}

track Drums {
  asset = DrumKit;
  role = percussion;
  gain = -3 dB;
}

track Bass {
  asset = SubBass;
  role = bass;
  gain = -4 dB;
  insert BassComp;
  sidechain BassComp from Drums;
}

track Lead {
  asset = SquareLead;
  role = melodic;
  gain = -5 dB;
  pan = 0.25;
  insert LeadFilter;
  send 0.18 to Reverb;
}

master {
  insert MasterComp;
  gain = -1 dB;
}

section Intro(2 bars) {
  clip Piano "Intro Piano" {
    play IntroMotif;
    velocity = Crescendo;
  }
  clip Pad "Intro Pad" { play IntroPad; }
}

section Verse(4 bars) {
  clip Piano "Verse Piano" {
    play VerseMotif;
    velocity = Crescendo;
    timing = Humanize;
  }
  clip Pad "Verse Pad" { play VersePad; }
  clip Drums "Verse Drums" { play DrumLoop; }
  clip Bass "Verse Bass" { play BassLine; }
}

section Chorus(4 bars) {
  clip Piano "Chorus Piano" {
    play ChorusMotif;
    velocity = Crescendo;
    timing = Humanize;
  }
  clip Pad "Chorus Pad" { play VersePad; }
  clip Drums "Chorus Drums" { play DrumLoop; }
  clip Bass "Chorus Bass" { play BassLine; }
  clip Lead "Chorus Lead" {
    play ChorusLead;
    velocity = LeadCrescendo;
    timing = Humanize;
  }
}

arrangement {
  play Intro;
  play Verse;
  play Chorus;
}

automation Piano.volume = PianoVolume;
automation Pad.volume = PianoVolume;
automation Piano.EqPiano.frequency = PianoSweep;
automation Bass.BassComp.threshold = DuckThreshold;
automation Reverb.Hall.mix = HallDepth;
automation master.MasterComp.threshold = DuckThreshold;

export midi("production.mid");
export wav("production.wav");
