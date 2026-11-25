project Mix {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}

asset PianoAsset = instrument("GrandPiano");

let M = notes span 2 whole {
  C4 @ 0 for 1/2 vel 100,
  E4 @ 1/2 for 1/2 vel 90,
  G4 @ 1 for 1/2 vel 86,
  C5 @ 3/2 for 1/2 vel 80
};

curve Volume on musical for 2 whole = linear(1, 0.5);
track Piano {
  asset = PianoAsset;
  role = melodic;
  gain = 0 dB;
}
automation Piano.volume = Volume;
section S(2 bars) { clip Piano "Mix" { play M; } }
arrangement { play S; }
export midi("mix.mid");
export wav("mix.wav");
