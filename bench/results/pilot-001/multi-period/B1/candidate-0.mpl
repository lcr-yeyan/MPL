project Poly {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
let A = rhythm { hit 1/8, hit 1/8, hit 1/8 };
let B = rhythm { hit 1/4, hit 1/4 };
let P = commonPeriod(A, B);
let RA = realign(A, P);
let M = cycle(RA, [C4, E4, G4], 96);
track Keys { asset = PianoAsset; role = melodic; }
section S(2 bars) { clip Keys "Poly" { play M; } }
arrangement { play S; }
export midi("poly.mid");