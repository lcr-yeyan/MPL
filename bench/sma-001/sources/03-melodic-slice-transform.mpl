project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let A = note(C4, 1/4, 96);
let B = note(E4, 1/4, 96);
let C = note(G4, 1/4, 96);
let L = [A, B, C];
let RESULT = transpose(sequenceAll(L[1:3]), 2 st);

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
