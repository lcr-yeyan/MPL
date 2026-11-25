project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");

// Partial: selecting pitches by pitch class requires filter/append; the
// current list-for expression cannot flatten [Pitch] branches.
let N = voicingNotes(voicing(chord("Cmaj9"), C4, C5, 5, 0, 12));
let RESULT = cycle(rhythm { hit 1/4, hit 1/4 }, [N[0], N[1]], 96);

track Piano { asset = PianoAsset; role = melodic; }
section S(1 bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
