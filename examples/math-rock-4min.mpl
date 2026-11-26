// MPL Studio starter — "Seven Against Four"
// A four-minute math-rock piece. 160 bars of 7/8 at 140 BPM = 4:00 exactly
// before the render tail. Grouping is 2+2+3; the 3/8 cell runs against the
// 7/8 bar throughout the main riff.

project SevenAgainstFour {
  tempo = 140 bpm;
  meter = 7/8;
  key = D minor;
}

asset Guitar = instrument("SquareLead");
asset GuitarClean = instrument("SawLead");
asset Bass = instrument("SubBass");
asset Drums = sample("DrumKit");

// --- the 7/8 engine -------------------------------------------------------

let R7 = rhythm {
  hit 1/8 accent 3,
  hit 1/8,
  hit 1/8,
  hit 1/8 accent 2,
  hit 1/8,
  hit 1/8,
  rest 1/8
};

let DmArp = cycle(R7, [D4, F4, A4, C5], 74);
let IntroArp = repeat(DmArp, 16);

let RiffA1 = cycle(R7, [D3, F3, A3, C4], 92);
let RiffA2 = cycle(R7, [D3, F3, A3, E4], 94);
let RiffA3 = cycle(R7, [Bb2, D3, F3, A3], 92);
let RiffA4 = cycle(R7, [C3, E3, G3, Bb3], 94);
let RiffA = repeat(sequence(RiffA1, RiffA2, RiffA3, RiffA4), 4);

let RiffB1 = cycle(R7, [D3, F3, A3, C4], 96);
let RiffB2 = cycle(R7, [F3, A3, C4, E4], 94);
let RiffB3 = cycle(R7, [C3, E3, G3, Bb3], 96);
let RiffB4 = cycle(R7, [G3, B3, D4, F4], 94);
let RiffB = repeat(sequence(RiffB1, RiffB2, RiffB3, RiffB4), 4);

let RiffC1 = cycle(R7, [Bb2, D3, F3, A3], 94);
let RiffC2 = cycle(R7, [C3, E3, G3, Bb3], 96);
let RiffC3 = cycle(R7, [D3, F3, A3, C4], 98);
let RiffC4 = cycle(R7, [A2, C3, E3, G3], 96);
let RiffC = repeat(sequence(RiffC1, RiffC2, RiffC3, RiffC4), 4);

let RiffD1 = cycle(R7, [D4, A3, F3, D3], 100);
let RiffD2 = cycle(R7, [F4, C4, A3, F3], 98);
let RiffD3 = cycle(R7, [C4, G3, E3, C3], 100);
let RiffD4 = cycle(R7, [G4, D4, B3, G3], 98);
let RiffD = repeat(sequence(RiffD1, RiffD2, RiffD3, RiffD4), 4);

// --- bass -----------------------------------------------------------------

let DmBass = cycle(R7, [D2], 100);
let FBass = cycle(R7, [F2], 102);
let CBass = cycle(R7, [C2], 100);
let GBass = cycle(R7, [G2], 102);
let BassA = repeat(sequence(DmBass, FBass, CBass, GBass), 4);

let BbBass = cycle(R7, [Bb1], 100);
let BassB = repeat(sequence(DmBass, BbBass, FBass, CBass), 4);
let BassC = repeat(sequence(BbBass, CBass, DmBass, DmBass), 4);
let BassD = repeat(sequence(DmBass, DmBass, FBass, GBass), 4);

// --- drums ----------------------------------------------------------------

let DrumCell = cycle(metricAccent(R7), [C2], 106);
let DrumsA = repeat(DrumCell, 16);

let Fill = notes span 1 bars {
  C2 @ 0 for 1/8 vel 112,
  D2 @ 1/8 for 1/8 vel 78,
  D2 @ 2/8 for 1/8 vel 90,
  D2 @ 3/8 for 1/8 vel 98,
  D2 @ 4/8 for 1/8 vel 104,
  D2 @ 5/8 for 1/8 vel 110,
  D2 @ 6/8 for 1/8 vel 116
};
let DrumsH = sequence(repeat(DrumCell, 15), Fill);

// --- lead -----------------------------------------------------------------

let LeadPhrase = sequence(
  cycle(R7, [D5, F5, A5, C6], 88),
  cycle(R7, [D5, F5, A5, E6], 90),
  cycle(R7, [Bb4, D5, F5, A5], 88),
  cycle(R7, [C5, E5, G5, Bb5], 90)
);
let LeadA = repeat(LeadPhrase, 4);
let LeadB = diatonicTranspose(LeadA, 2);
let LeadHigh = transpose(LeadA, 12 st);

// --- tracks ---------------------------------------------------------------

track GuitarMain {
  asset = Guitar;
  role = melodic;
  gain = -5 dB;
  pan = -0.18;
}
track GuitarEcho {
  asset = GuitarClean;
  role = harmony;
  gain = -11 dB;
  pan = 0.24;
}
track BassTrack {
  asset = Bass;
  role = bass;
  gain = -5 dB;
}
track Kit {
  asset = Drums;
  role = percussion;
  gain = -4 dB;
}

// --- 160 bars: 16 + 8×16 + 16 ---------------------------------------------

section Intro(16 bars) {
  clip GuitarMain "Arp intro" { play IntroArp; }
  clip GuitarEcho "Arp echo" { play transpose(IntroArp, 12 st); }
}

section A(16 bars) {
  clip GuitarMain "Riff A" { play RiffA; }
  clip BassTrack "Bass A" { play BassA; }
  clip Kit "Drums A" { play DrumsA; }
  clip GuitarEcho "Lead A" { play LeadA; }
}

section B(16 bars) {
  clip GuitarMain "Riff B" { play RiffB; }
  clip BassTrack "Bass B" { play BassB; }
  clip Kit "Drums B" { play DrumsA; }
  clip GuitarEcho "Lead B" { play LeadB; }
}

section C(16 bars) {
  clip GuitarMain "Riff C" { play RiffC; }
  clip BassTrack "Bass C" { play BassB; }
  clip Kit "Drums C" { play DrumsH; }
  clip GuitarEcho "Lead C" { play LeadHigh; }
}

section D(16 bars) {
  clip GuitarMain "Riff D" { play RiffD; }
  clip BassTrack "Bass D" { play BassD; }
  clip Kit "Drums D" { play DrumsA; }
}

section E(16 bars) {
  clip GuitarMain "Riff A reprise" { play RiffA; }
  clip BassTrack "Bass A reprise" { play BassA; }
  clip Kit "Drums E" { play DrumsH; }
  clip GuitarEcho "Lead E" { play LeadB; }
}

section F(16 bars) {
  clip GuitarMain "Riff B reprise" { play RiffB; }
  clip GuitarEcho "Riff B echo" { play transpose(RiffB, 12 st); }
  clip BassTrack "Bass B reprise" { play BassB; }
  clip Kit "Drums F" { play DrumsA; }
}

section G(16 bars) {
  clip GuitarMain "Riff C reprise" { play RiffC; }
  clip BassTrack "Bass C reprise" { play BassC; }
  clip Kit "Drums G" { play DrumsA; }
  clip GuitarEcho "Lead G" { play LeadA; }
}

section H(16 bars) {
  clip GuitarMain "Riff D reprise" { play RiffD; }
  clip GuitarEcho "Riff D echo" { play transpose(RiffD, 12 st); }
  clip BassTrack "Bass H" { play BassD; }
  clip Kit "Drums H" { play DrumsH; }
  clip GuitarEcho "Lead H" { play LeadHigh; }
}

section Outro(16 bars) {
  clip GuitarMain "Last arp" { play IntroArp; }
  clip GuitarEcho "Last echo" { play transpose(IntroArp, 12 st); }
  clip BassTrack "Last bass" { play repeat(DmBass, 16); }
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

export midi("seven-against-four.mid");
export wav("seven-against-four.wav");
