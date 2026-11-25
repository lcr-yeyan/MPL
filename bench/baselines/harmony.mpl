import "std/harmony.mpl";

project Harmony {
  tempo = 120 bpm;
  meter = 4/4;
  key = D minor;
}

asset PianoAsset = instrument("GrandPiano");

let H = harmony span 2 whole {
  "Dm9" @ 0 for 1,
  "G7b9" @ 1 for 1
};
let R = rhythm { hit 1/4 accent 2, hit 1/8, hit 1/8, hit 1/4, hit 1/8, rest 1/8 };
let M = sequence(
  arpBarAt(R, H, 0 whole, D3, D5, 4, 96),
  arpBarAt(R, H, 1 whole, D3, D5, 4, 96)
);

track Piano { asset = PianoAsset; role = melodic; }
section S(2 bars) { clip Piano "Harmony" { play M; } }
arrangement { play S; }
export midi("harmony.mid");
