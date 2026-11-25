project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

// Adjacent interval extraction over a pitch list.
fn intervalsOf(ps: [Pitch]) -> [Int] {
  return for i in range(0, length(ps) - 1) {
    interval(ps[i], ps[i + 1])
  };
}
let N = voicingNotes(voicing(chord("Cmaj7"), C4, C5, 4, 0, 12));
let I = intervalsOf(N);
let RESULT = cycle(rhythm { hit 1/4, hit 1/4, hit 1/4 }, [N[0], N[1], N[2]], 96);

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
