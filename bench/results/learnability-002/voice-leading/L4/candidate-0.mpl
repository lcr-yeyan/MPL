project VoiceLeading {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}
asset PianoAsset = instrument("GrandPiano");
let V1 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V2 = voiceLead(voicingNotes(V1), chord("G7"), D3, D5, 7, 12, false);
let R = rhythm { hit 1/4 accent 2, hit 1/4, hit 1/4, hit 1/4 };
let M = sequence(cycle(R, voicingNotes(V1), 96), cycle(R, voicingNotes(V2), 96));
track Keys { asset = PianoAsset; role = harmony; }
section S(2 bars) { clip Keys "VoiceLeading" { play M; } }
arrangement { play S; }
export midi("voice-leading.mid");