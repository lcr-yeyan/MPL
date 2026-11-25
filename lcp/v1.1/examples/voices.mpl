project Voices {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
let V1 = voicing(chord("Cmaj7"), C3, C5, 4, 0, 12);
let V2 = voiceLead(voicingNotes(V1), chord("Fmaj7"), C3, C5, 7, 12, false);
let R = rhythm { hit 1/8 accent 2, hit 1/8, hit 1/8, hit 1/8 };
let M = sequence(cycle(R, voicingNotes(V1), 96), cycle(R, voicingNotes(V2), 96));
track Keys { asset = PianoAsset; role = harmony; }
section S(2 bars) { clip Keys "Voices" { play M; } }
arrangement { play S; }
export midi("voices.mid");
