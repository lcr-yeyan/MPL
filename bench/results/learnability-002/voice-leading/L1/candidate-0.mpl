project VoiceLeading {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let V1 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V2 = voiceLead(V1, chord("G7"), D3, D5, 7, 12);

let R = rhythm {
  hit 1/4 accent 2,
  hit 1/4,
  hit 1/4,
  hit 1/4
};

let P1 = parallel(cycle(voicingNotes(V1)), R);
let P2 = parallel(cycle(voicingNotes(V2)), R);

track Keys { asset = PianoAsset; role = harmony; }

section S(2 bars) {
  clip Keys "Dm9" { play P1; }
  clip Keys "G7" { play P2; }
}

arrangement { play S; }
export midi("voice-leading.mid");