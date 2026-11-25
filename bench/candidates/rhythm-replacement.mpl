import "std/harmony.mpl";

project Replacement {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let H = harmony span 2 whole {
  "Dm9" @ 0 whole for 1 whole,
  "G7b9" @ 1 whole for 1 whole
};
let HAlt = harmony span 2 whole {
  "Fmaj7" @ 0 whole for 1 whole,
  "G7" @ 1 whole for 1 whole
};
let R = rhythm { hit 1/4, hit 1/4, hit 1/4, hit 1/4 };
let RAlt = rhythm {
  hit 1/8, hit 1/8, hit 1/8, hit 1/8,
  hit 1/8, hit 1/8, hit 1/8, hit 1/8
};
let HarmonyBase = sequence(
  arpBarAt(R, H, 0 whole, D3, D5, 4, 94),
  arpBarAt(R, H, 1 whole, D3, D5, 4, 94)
);
let RhythmSwap = sequence(
  arpBarAt(RAlt, H, 0 whole, D3, D5, 4, 90),
  arpBarAt(RAlt, H, 1 whole, D3, D5, 4, 90)
);
let HarmonySwap = sequence(
  arpBarAt(R, HAlt, 0 whole, D3, D5, 4, 90),
  arpBarAt(R, HAlt, 1 whole, D3, D5, 4, 90)
);

track Piano { asset = PianoAsset; role = harmony; }
section S(2 bars) { clip Piano "Base" { play RhythmSwap; } }
arrangement { play S; }
export midi("replacement.mid");
