project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let V0 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V1 = voiceLead(voicingNotes(V0), chord("G7b9"), D3, D5, 7, 12, false);
let RESULT = cycle(rhythm { hit 1/2, hit 1/2 }, voicingNotes(V1), 96);

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
