project Function {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
fn cell(root: Pitch, velocity: Int) -> Fragment {
  return sequence(note(root, 1/4, velocity), note(E4, 1/4, velocity));
}
let M = cell(C4, 96);
track Keys { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Keys "Cell" { play M; } }
arrangement { play S; }
export midi("function.mid");