project VoiceLeading {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let V0 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V1 = voiceLead(voicingNotes(V0), chord("G7"), D3, D5, 7, 12, false);
let R = rhythm { hit 1/4 accent 2, hit 1/4, hit 1/4, hit 1/4 };
let M = sequence(cycle(R, voicingNotes(V0), 96), cycle(R, voicingNotes(V1), 96));

track Piano { asset = PianoAsset; role = harmony; }
section S(2 bars) { clip Piano "VoiceLeading" { play M; } }
arrangement { play S; }
export midi("voice-leading.mid");
