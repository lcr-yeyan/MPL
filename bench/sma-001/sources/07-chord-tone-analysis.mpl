project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

let N = voicingNotes(voicing(chord("Cmaj7"), C4, C5, 4, 0, 12));
let RESULT = cycle(rhythm { hit 1/2, hit 1/2 }, [N[0], N[2]], 96);

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
