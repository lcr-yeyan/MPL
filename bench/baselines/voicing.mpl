project Voicing {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let V = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let R = rhythm { hit 1/4 accent 2, hit 1/4, hit 1/4, hit 1/4 };
let M = cycle(R, voicingNotes(V), 96);

track Piano { asset = PianoAsset; role = harmony; }
section S(1 bars) { clip Piano "Voicing" { play M; } }
arrangement { play S; }
export midi("voicing.mid");
