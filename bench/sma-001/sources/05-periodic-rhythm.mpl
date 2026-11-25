project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let R = rhythm { hit 1/4, rest 1/4, hit 1/4, hit 1/4 };
let Period = repeat(R, 2);
let RESULT = cycle(Period, [C4, E4], 96);

track Piano { asset = PianoAsset; role = melodic; }
section S(2 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
