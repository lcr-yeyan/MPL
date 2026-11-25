project Master {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

let M = notes span 2 whole {
  C3 @ 0 for 1/2 vel 110,
  G3 @ 1/2 for 1/2 vel 100,
  C4 @ 1 for 1/2 vel 96,
  E4 @ 3/2 for 1/2 vel 90
};

track Piano { asset = PianoAsset; role = melodic; gain = -6 dB; }
section S(2 bars) { clip Piano "Master" { play M; } }
arrangement { play S; }
export midi("master.mid");
export wav("master.wav");
