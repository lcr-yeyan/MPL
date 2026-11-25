project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let R = rhythm { hit 1/16, hit 1/16, rest 1/16, hit 1/16, hit 1/16, rest 1/16 };
let Sparse = onsetSet(R, 1/8);
let RESULT = cycle(Sparse, [C4], 96);

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
