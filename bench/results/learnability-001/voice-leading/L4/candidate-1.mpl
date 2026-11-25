project VoiceLeading {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let V1 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V2 = voiceLead(voicingNotes(V1), chord("G7"), D3, D5, 7, 12, false);

fn barVoicing(pitches: [Pitch]) -> Fragment {
  return parallel(
    note(pitches[0], 1 whole, 96),
    note(pitches[1], 1 whole, 88),
    note(pitches[2], 1 whole, 88),
    note(pitches[3], 1 whole, 84)
  );
}

let M1 = barVoicing(voicingNotes(V1));
let M2 = barVoicing(voicingNotes(V2));

track Keys {
  asset = PianoAsset;
  role = harmony;
}

section S(2 bars) {
  clip Keys "Dm9" {
    play M1;
  }
  clip Keys "G7" {
    play M2;
  }
}

arrangement {
  play S;
}

export midi("voice-leading.mid");