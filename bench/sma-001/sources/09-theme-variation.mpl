project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let Theme = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  D4 @ 1/4 for 1/4 vel 96,
  E4 @ 1/2 for 1/4 vel 96
};
let RESULT = sequence(transpose(Theme, 2 st), stretch(Theme, 2/1));

track Piano { asset = PianoAsset; role = melodic; }
section S(3 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
