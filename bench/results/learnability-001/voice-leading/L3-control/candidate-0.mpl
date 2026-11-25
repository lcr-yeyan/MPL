project VoiceLeading {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let C1 = chord("Dm9");
let V1 = voicing(C1, D3, D5, 4, 0, 12);
let P1 = voicingNotes(V1);

let C2 = chord("G7");
let V2 = voiceLead(P1, C2, D3, D5, 7, 12, false);
let P2 = voicingNotes(V2);

fn barVoicing(pitches: [Pitch]) -> Fragment {
  return sequence(
    note(pitches[0], 1/4, 96),
    note(pitches[1], 1/4, 88),
    note(pitches[2], 1/4, 88),
    note(pitches[3], 1/4, 88)
  );
}

let B1 = barVoicing(P1);
let B2 = barVoicing(P2);
let M = sequence(B1, B2);

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