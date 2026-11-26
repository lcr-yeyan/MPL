// MPL Studio starter — "Neon Cartridge"
// A two-minute 8-bit arcade piece at 150 BPM in E minor.
//
// 75 bars of 4/4 at 150 BPM render exactly 120 seconds before the render tail.
// The arrangement is Intro(3) + A(8) + B(8) + C(8) + D(8) + E(8) + F(8) +
// G(8) + H(8) + Outro(8) = 75 bars.

project NeonCartridge {
  tempo = 150 bpm;
  meter = 4/4;
  key = E minor;
}

asset ChipLead = instrument("PulseLead");
asset HarmonyLead = instrument("HarmonyLead");
asset ChipBass = instrument("TriangleBass");
asset ChipDrums = sample("NoiseDrums");

// --- one-bar arpeggio cells ------------------------------------------------

let EmLead = notes span 1 bars {
  E5 @ 0 for 1/8 vel 96,
  G5 @ 1/8 for 1/8 vel 92,
  B5 @ 1/4 for 1/8 vel 98,
  E6 @ 3/8 for 1/8 vel 100,
  B5 @ 1/2 for 1/8 vel 92,
  G5 @ 5/8 for 1/8 vel 90,
  E5 @ 3/4 for 1/8 vel 88,
  B5 @ 7/8 for 1/8 vel 92
};

let CLead = notes span 1 bars {
  C5 @ 0 for 1/8 vel 94,
  E5 @ 1/8 for 1/8 vel 92,
  G5 @ 1/4 for 1/8 vel 98,
  C6 @ 3/8 for 1/8 vel 100,
  G5 @ 1/2 for 1/8 vel 92,
  E5 @ 5/8 for 1/8 vel 90,
  C5 @ 3/4 for 1/8 vel 88,
  G5 @ 7/8 for 1/8 vel 92
};

let GLead = notes span 1 bars {
  G4 @ 0 for 1/8 vel 94,
  B4 @ 1/8 for 1/8 vel 92,
  D5 @ 1/4 for 1/8 vel 98,
  G5 @ 3/8 for 1/8 vel 100,
  D5 @ 1/2 for 1/8 vel 92,
  B4 @ 5/8 for 1/8 vel 90,
  G4 @ 3/4 for 1/8 vel 88,
  B4 @ 7/8 for 1/8 vel 92
};

let DLead = notes span 1 bars {
  D5 @ 0 for 1/8 vel 96,
  F#5 @ 1/8 for 1/8 vel 94,
  A5 @ 1/4 for 1/8 vel 100,
  D6 @ 3/8 for 1/8 vel 102,
  A5 @ 1/2 for 1/8 vel 94,
  F#5 @ 5/8 for 1/8 vel 92,
  D5 @ 3/4 for 1/8 vel 90,
  A5 @ 7/8 for 1/8 vel 94
};

let LeadA = repeat(sequence(EmLead, CLead, GLead, DLead), 2);
let LeadB = syncopate(LeadA, 1/16);
let LeadC = sequence(repeat(EmLead, 2), repeat(CLead, 2), repeat(GLead, 2), repeat(DLead, 2));
let IntroArp = sequence(EmLead, DLead, EmLead);

// --- chord pad and bass ----------------------------------------------------

let EmHarmony = notes span 1 bars {
  B4 @ 0 for 1/2 vel 62,
  E5 @ 1/2 for 1/2 vel 68
};
let CHarmony = notes span 1 bars {
  G4 @ 0 for 1/2 vel 62,
  C5 @ 1/2 for 1/2 vel 68
};
let GHarmony = notes span 1 bars {
  D4 @ 0 for 1/2 vel 62,
  G4 @ 1/2 for 1/2 vel 68
};
let DHarmony = notes span 1 bars {
  A4 @ 0 for 1/2 vel 62,
  D5 @ 1/2 for 1/2 vel 68
};
let HarmonyA = repeat(sequence(EmHarmony, CHarmony, GHarmony, DHarmony), 2);

let EmBass = notes span 1 bars {
  E2 @ 0 for 1/8 vel 102,
  E3 @ 1/8 for 1/8 vel 78,
  E2 @ 1/4 for 1/8 vel 92,
  B2 @ 3/8 for 1/8 vel 80,
  E2 @ 1/2 for 1/8 vel 98,
  E3 @ 5/8 for 1/8 vel 78,
  E2 @ 3/4 for 1/8 vel 92,
  B2 @ 7/8 for 1/8 vel 80
};
let CBass = notes span 1 bars {
  C2 @ 0 for 1/8 vel 102,
  C3 @ 1/8 for 1/8 vel 78,
  C2 @ 1/4 for 1/8 vel 92,
  G2 @ 3/8 for 1/8 vel 80,
  C2 @ 1/2 for 1/8 vel 98,
  C3 @ 5/8 for 1/8 vel 78,
  C2 @ 3/4 for 1/8 vel 92,
  G2 @ 7/8 for 1/8 vel 80
};
let GBass = notes span 1 bars {
  G1 @ 0 for 1/8 vel 102,
  G2 @ 1/8 for 1/8 vel 78,
  G1 @ 1/4 for 1/8 vel 92,
  D2 @ 3/8 for 1/8 vel 80,
  G1 @ 1/2 for 1/8 vel 98,
  G2 @ 5/8 for 1/8 vel 78,
  G1 @ 3/4 for 1/8 vel 92,
  D2 @ 7/8 for 1/8 vel 80
};
let DBass = notes span 1 bars {
  D2 @ 0 for 1/8 vel 102,
  D3 @ 1/8 for 1/8 vel 78,
  D2 @ 1/4 for 1/8 vel 92,
  A2 @ 3/8 for 1/8 vel 80,
  D2 @ 1/2 for 1/8 vel 98,
  D3 @ 5/8 for 1/8 vel 78,
  D2 @ 3/4 for 1/8 vel 92,
  A2 @ 7/8 for 1/8 vel 80
};
let BassA = repeat(sequence(EmBass, CBass, GBass, DBass), 2);
let BassB = transpose(BassA, -12 st);

// --- drums ----------------------------------------------------------------

let Beat = notes span 1 bars {
  C2 @ 0 for 1/8 vel 112,
  F#2 @ 1/8 for 1/16 vel 46,
  D2 @ 1/4 for 1/8 vel 90,
  F#2 @ 3/8 for 1/16 vel 44,
  C2 @ 1/2 for 1/8 vel 108,
  F#2 @ 5/8 for 1/16 vel 46,
  D2 @ 3/4 for 1/8 vel 90,
  F#2 @ 7/8 for 1/16 vel 44
};
let Fill = notes span 1 bars {
  C2 @ 0 for 1/8 vel 112,
  D2 @ 1/8 for 1/8 vel 76,
  D2 @ 1/4 for 1/8 vel 86,
  D2 @ 3/8 for 1/8 vel 92,
  D2 @ 1/2 for 1/8 vel 100,
  D2 @ 5/8 for 1/8 vel 106,
  D2 @ 3/4 for 1/8 vel 112,
  D2 @ 7/8 for 1/8 vel 118
};
let DrumsA = repeat(Beat, 8);
let DrumsH = sequence(repeat(Beat, 7), Fill);

// --- tracks ---------------------------------------------------------------

track Lead {
  asset = ChipLead;
  role = melodic;
  gain = -5 dB;
  pan = -0.08;
}
track Harmony {
  asset = HarmonyLead;
  role = harmony;
  gain = -12 dB;
  pan = 0.14;
}
track Bass {
  asset = ChipBass;
  role = bass;
  gain = -5 dB;
}
track Drums {
  asset = ChipDrums;
  role = percussion;
  gain = -6 dB;
}

// --- arrangement: 3 + 8 + 8 + 8 + 8 + 8 + 8 + 8 + 8 + 8 = 75 bars ----------

section Intro(3 bars) {
  clip Lead "Boot" { play IntroArp; }
  clip Harmony "Boot" { play repeat(EmHarmony, 3); }
}

section A(8 bars) {
  clip Lead "Main A" { play LeadA; }
  clip Bass "Bass A" { play BassA; }
  clip Drums "Drums A" { play DrumsA; }
}

section B(8 bars) {
  clip Lead "Main B" { play LeadB; }
  clip Harmony "Pad B" { play HarmonyA; }
  clip Bass "Bass A" { play BassA; }
  clip Drums "Drums A" { play DrumsA; }
}

section C(8 bars) {
  clip Lead "Main C" { play LeadC; }
  clip Harmony "Pad C" { play HarmonyA; }
  clip Bass "Bass B" { play BassB; }
  clip Drums "Drums A" { play DrumsA; }
}

section D(8 bars) {
  clip Harmony "Space" { play HarmonyA; }
  clip Bass "Bass A" { play BassA; }
}

section E(8 bars) {
  clip Lead "Main E" { play LeadC; }
  clip Harmony "Pad E" { play HarmonyA; }
  clip Bass "Bass B" { play BassB; }
  clip Drums "Drums E" { play DrumsA; }
}

section F(8 bars) {
  clip Harmony "Pad F" { play HarmonyA; }
  clip Bass "Bass A" { play BassA; }
  clip Drums "Drums F" { play DrumsA; }
}

section G(8 bars) {
  clip Lead "Main G" { play LeadA; }
  clip Harmony "Pad G" { play HarmonyA; }
  clip Bass "Bass B" { play BassB; }
  clip Drums "Drums G" { play DrumsA; }
}

section H(8 bars) {
  clip Lead "Final H" { play LeadB; }
  clip Harmony "Pad H" { play HarmonyA; }
  clip Bass "Bass H" { play BassA; }
  clip Drums "Drums H" { play DrumsH; }
}

section Outro(8 bars) {
  clip Lead "Power down" { play repeat(EmLead, 8); }
  clip Harmony "Last chord" { play repeat(EmHarmony, 8); }
  clip Bass "Last bass" { play repeat(EmBass, 8); }
}

arrangement {
  play Intro;
  play A;
  play B;
  play C;
  play D;
  play E;
  play F;
  play G;
  play H;
  play Outro;
}

export midi("neon-cartridge.mid");
export wav("neon-cartridge.wav");
