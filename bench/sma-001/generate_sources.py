#!/usr/bin/env python3
"""Generate SMA-001 self-hosted MPL sources.

The sources are the benchmark; this script only keeps the boilerplate
consistent. It is not a compiler or a substitute for executing them.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "bench" / "sma-001" / "sources"
OUT.mkdir(parents=True, exist_ok=True)

HEADER = """project SMA001 {
  tempo = 120 bpm;
  meter = 4/4;
  key = C major;
}
asset PianoAsset = instrument("GrandPiano");
"""

FOOTER = """track Piano { asset = PianoAsset; role = melodic; }
section S(BARS bars) { clip Piano "SMA" { play RESULT; } }
arrangement { play S; }
export midi("sma-001.mid");
"""

TASKS = {
    "01-interval-extraction": """
// Adjacent interval extraction over a pitch list.
fn intervalsOf(ps: [Pitch]) -> [Int] {
  return for i in range(0, length(ps) - 1) {
    interval(ps[i], ps[i + 1])
  };
}
let N = voicingNotes(voicing(chord("Cmaj7"), C4, C5, 4, 0, 12));
let I = intervalsOf(N);
let RESULT = cycle(rhythm { hit 1/4, hit 1/4, hit 1/4 }, [N[0], N[1], N[2]], 96);
""",
    "02-interval-reconstruction": """
// Partial: extract intervals, but MPL has no list append/fold to rebuild a
// pitch list from a seed and an interval list.
fn intervalsOf(ps: [Pitch]) -> [Int] {
  return for i in range(0, length(ps) - 1) {
    interval(ps[i], ps[i + 1])
  };
}
let N = voicingNotes(voicing(chord("Cmaj7"), C4, C5, 4, 0, 12));
let I = intervalsOf(N);
let RESULT = cycle(rhythm { hit 1/4 }, [N[0]], 96);
""",
    "03-melodic-slice-transform": """
let A = note(C4, 1/4, 96);
let B = note(E4, 1/4, 96);
let C = note(G4, 1/4, 96);
let L = [A, B, C];
let RESULT = transpose(sequenceAll(L[1:3]), 2 st);
""",
    "04-scale-pitch-filter": """
// Partial: selecting pitches by pitch class requires filter/append; the
// current list-for expression cannot flatten [Pitch] branches.
let N = voicingNotes(voicing(chord("Cmaj9"), C4, C5, 5, 0, 12));
let RESULT = cycle(rhythm { hit 1/4, hit 1/4 }, [N[0], N[1]], 96);
""",
    "05-periodic-rhythm": """
let R = rhythm { hit 1/4, rest 1/4, hit 1/4, hit 1/4 };
let Period = repeat(R, 2);
let RESULT = cycle(Period, [C4, E4], 96);
""",
    "06-rhythm-density-reduction": """
let R = rhythm { hit 1/16, hit 1/16, rest 1/16, hit 1/16, hit 1/16, rest 1/16 };
let Sparse = onsetSet(R, 1/8);
let RESULT = cycle(Sparse, [C4], 96);
""",
    "07-chord-tone-analysis": """
let N = voicingNotes(voicing(chord("Cmaj7"), C4, C5, 4, 0, 12));
let RESULT = cycle(rhythm { hit 1/2, hit 1/2 }, [N[0], N[2]], 96);
""",
    "08-voice-movement": """
let V0 = voicing(chord("Dm9"), D3, D5, 4, 0, 12);
let V1 = voiceLead(voicingNotes(V0), chord("G7b9"), D3, D5, 7, 12, false);
let RESULT = cycle(rhythm { hit 1/2, hit 1/2 }, voicingNotes(V1), 96);
""",
    "09-theme-variation": """
let Theme = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  D4 @ 1/4 for 1/4 vel 96,
  E4 @ 1/2 for 1/4 vel 96
};
let RESULT = sequence(transpose(Theme, 2 st), stretch(Theme, 2/1));
""",
    "10-form-construction": """
let Theme = notes span 1 whole {
  C4 @ 0 for 1/4 vel 96,
  E4 @ 1/4 for 1/4 vel 96
};
track Piano { asset = PianoAsset; role = melodic; }
section A(1 bars) { clip Piano "A" { play Theme; } }
section B(1 bars) { clip Piano "B" { play transpose(Theme, 2 st); } }
arrangement { play A; play B; }
export midi("sma-001.mid");
""",
}

for task, body in TASKS.items():
    if task == "10-form-construction":
        source = HEADER + body
    else:
        bars = "3" if task == "09-theme-variation" else ("2" if task == "05-periodic-rhythm" else "1")
        source = HEADER + body + "\n" + FOOTER.replace("BARS", bars)
    (OUT / f"{task}.mpl").write_text(source, encoding="utf-8")
print(f"wrote {len(TASKS)} SMA sources to {OUT}")
