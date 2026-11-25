project VoiceLeading {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let Dm9 = chord("Dm9");
let G7 = chord("G7");

let V1 = voicing(Dm9, D3, D5, 4, 0, 12);
let P1 = voicingNotes(V1);
let V2 = voiceLead(P1, G7, D3, D5, 7, 12, false);
let P2 = voicingNotes(V2);

fn barOf(pitches: [Pitch]) -> Fragment {
  return parallel(
    note(pitches[0], 1 whole, 96),
    note(pitches[1], 1 whole, 88),
    note(pitches[2], 1 whole, 88),
    note(pitches[3], 1 whole, 84)
  );
}

let M1 = barOf(P1);
let M2 = barOf(P2);
let M = sequence(M1, M2);

track Keys {
  asset = PianoAsset;
  role = harmony;
}

section S(2 bars) {
  clip Keys "VoiceLeading" {
    play M;
  }
}

arrangement {
  play S;
}

export midi("voice-leading.mid");