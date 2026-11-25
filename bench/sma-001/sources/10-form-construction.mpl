project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let Theme = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  E4 @ 1/4 for 1/4 vel 96
};
track Piano { asset = PianoAsset; role = melodic; }
section A(1 bars) { clip Piano "A" { play Theme; } }
section B(1 bars) { clip Piano "B" { play transpose(Theme, 2 st); } }
arrangement { play A; play B; }
export midi("sma-001.mid");
