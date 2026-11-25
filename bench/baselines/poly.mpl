project Poly {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

let A = rhythm { hit 1/8, hit 1/8, hit 1/8 };
let B = rhythm { hit 1/4, hit 1/4 };
let Period = commonPeriod(A, B);
let M = cycle(realign(A, Period), [C4, E4, G4], 92);

track Piano { asset = PianoAsset; role = melodic; }
section S(2 bars) { clip Piano "Poly" { play M; } }
arrangement { play S; }
export midi("poly.mid");
