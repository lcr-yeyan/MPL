// MPL V0.3.0-rc — Math Rock Reference Project (MRP-001)
//
// The work moves through 7/8 (2+2+3), 5/8 (2+3) and 4/4 (2+2). Rhythm,
// harmony and pitch are separate values: rhythms are authored independently,
// one shared harmonic timeline spans the whole piece, and each voice chooses an
// explicit pitch strategy. The 3/8 theme cell still runs seven times against
// three 7/8 bars.
//
// Change `Variant` or `Seed` for a different version.

import "std/rhythm.mpl";
import "std/harmony.mpl";
import "std/voicing.mpl";

project MRP001 {
  tempo = 132 bpm;
  meter = 7/8;
  key = D minor;
}

asset GuitarSound = instrument("SquareLead");
asset BassSound = instrument("SubBass");
asset LeadSound = instrument("SawLead");
asset DrumKitSound = sample("DrumKit");

let Variant = 1;
let Seed = 7;

// --- independent rhythms ---------------------------------------------------

let R3 = rhythm {
  hit 1/8 accent 2,
  hit 1/8,
  hit 1/16,
  rest 1/16
};

let R7 = rhythm {
  hit 1/8 accent 3,
  hit 1/8,
  hit 1/8,
  hit 1/8 accent 2,
  hit 1/8,
  hit 1/8,
  rest 1/8
};

let R5 = rhythm {
  hit 1/8 accent 3,
  hit 1/8,
  hit 1/8 accent 2,
  hit 1/8,
  rest 1/8
};

let R4 = rhythm {
  hit 1/4 accent 3,
  hit 1/8,
  hit 1/8,
  hit 1/4 accent 2,
  hit 1/8,
  rest 1/8
};

let Cycle = commonPeriod(R3, R7);
let CellCycle = realign(R3, Cycle);
let BarCycle = realign(R7, Cycle);
let CompositeCycle = union(CellCycle, BarCycle, 1/16);
let Shifted5 = shift(R5, 1/16);
let Displaced5 = intersect(R5, Shifted5, 1/8);

// --- shared harmonic timeline (absolute whole notes) -----------------------

let H = harmony span 10 whole {
  "Dm9" @ 0 whole for 7/4 whole,
  "G7b9" @ 7/4 whole for 7/4 whole,
  "Bbmaj7" @ 7/2 whole for 5/4 whole,
  "A7b9" @ 19/4 whole for 5/4 whole,
  "Dm9" @ 6 whole for 2 whole,
  "Bbmaj7" @ 8 whole for 1 whole,
  "A7b9" @ 9 whole for 1 whole
};

// A reharmonised timeline with the same event boundaries, used to show that
// harmony can be replaced while the rhythm stays fixed.
let HAlt = harmony span 10 whole {
  "Fmaj7" @ 0 whole for 7/4 whole,
  "G7" @ 7/4 whole for 7/4 whole,
  "Gm7" @ 7/2 whole for 5/4 whole,
  "A7#9" @ 19/4 whole for 5/4 whole,
  "Fmaj7" @ 6 whole for 2 whole,
  "Gm7" @ 8 whole for 1 whole,
  "A7#9" @ 9 whole for 1 whole
};

// --- thematic material -----------------------------------------------------

let Theme = cycle(R3, [D4, F4, A4], 92);
let VerseTheme = diatonicTranspose(Theme, round(Variant));

// Guitar A: four 7/8 bars driven by the shared harmony.
let GuitarA =
  sequence(
    arpBarAt(R7, H, 0 whole, D3, D5, 4, 96),
    arpBarAt(R7, H, 7/8 whole, D3, D5, 4, 94),
    arpBarAt(R7, H, 7/4 whole, D3, D5, 4, 98),
    arpBarAt(R7, H, 21/8 whole, D3, D5, 4, 96)
  );

// Guitar B: four 5/8 bars, a different rhythm over the same timeline.
let GuitarB =
  sequence(
    voicedBarAt(R5, H, 7/2 whole, A3, A5, 4, 1, 19, 94),
    voicedBarAt(R5, H, 33/8 whole, A3, A5, 4, 1, 19, 96),
    voicedBarAt(R5, H, 19/4 whole, A3, A5, 4, 1, 19, 98),
    voicedBarAt(R5, H, 43/8 whole, A3, A5, 4, 1, 19, 96)
  );

// Guitar Reprise: explicit open-voicing, first-inversion, bounded
// voice-leading strategy, one bar per chord.
let V0 = voicing(chordAt(H, 6 whole), D3, D5, 4, 1, 19);
let V1 = leadBarAt(voicingNotes(V0), H, 7 whole, D3, D5, 7, 19, false);
let V2 = leadBarAt(voicingNotes(V1), H, 8 whole, D3, D5, 7, 19, false);
let V3 = leadBarAt(voicingNotes(V2), H, 9 whole, D3, D5, 7, 19, false);
let GuitarReprise =
  sequence(
    cycle(R4, voicingNotes(V0), 96),
    cycle(R4, voicingNotes(V1), 96),
    cycle(R4, voicingNotes(V2), 96),
    cycle(R4, voicingNotes(V3), 96)
  );

let BassA =
  sequence(
    bassBarAt(R7, H, 0 whole, D2, D3, 2, 90),
    bassBarAt(R7, H, 7/8 whole, D2, D3, 2, 92),
    bassBarAt(R7, H, 7/4 whole, D2, D3, 2, 94),
    bassBarAt(R7, H, 21/8 whole, D2, D3, 2, 92)
  );
let BassB =
  sequence(
    bassBarAt(R5, H, 7/2 whole, A1, A2, 2, 94),
    bassBarAt(R5, H, 33/8 whole, A1, A2, 2, 92),
    bassBarAt(R5, H, 19/4 whole, A1, A2, 2, 96),
    bassBarAt(R5, H, 43/8 whole, A1, A2, 2, 94)
  );
let BassReprise =
  sequence(
    bassBarAt(R4, H, 6 whole, D2, D3, 2, 94),
    bassBarAt(R4, H, 7 whole, D2, D3, 2, 92),
    bassBarAt(R4, H, 8 whole, D2, D3, 2, 96),
    bassBarAt(R4, H, 9 whole, D2, D3, 2, 94)
  );

// Explicit replacements: one keeps rhythm R7 but swaps H for HAlt; the other
// keeps H but swaps R7 for R5.
let GuitarHarmonySwap =
  sequence(
    arpBarAt(R7, HAlt, 0 whole, D3, D5, 4, 90),
    arpBarAt(R7, HAlt, 7/8 whole, D3, D5, 4, 90),
    arpBarAt(R7, HAlt, 7/4 whole, D3, D5, 4, 90),
    arpBarAt(R7, HAlt, 21/8 whole, D3, D5, 4, 90)
  );
let GuitarRhythmSwap =
  sequence(
    arpBarAt(R5, H, 0 whole, D3, D5, 4, 90),
    arpBarAt(R5, H, 5/8 whole, D3, D5, 4, 90),
    arpBarAt(R5, H, 5/4 whole, D3, D5, 4, 90),
    arpBarAt(R5, H, 15/8 whole, D3, D5, 4, 90)
  );

// --- validation-informed material ------------------------------------------

let VoiceStatus = voicingStatus(V1);
let StatusBoost = if VoiceStatus == "strictly-satisfied" { 4 } else { 0 };
let VoiceMovement = voicingMovement(V1);
let Overlap = noteOverlap(GuitarA, BassA);
let RelationBoost = if Overlap > 0 { 2 } else { 0 };

let LeadA = if VoiceStatus == "strictly-satisfied" {
  lead(D5, Seed, 12)
} else {
  lead(D5, Seed + 1, 12)
};
let LeadB = lead(F5, Seed + 3, 12);

// --- user-defined functions ------------------------------------------------

fn cell(root: Pitch, degree: Int, velocity: Int) -> Fragment context key {
  let a = note(root, 1/8, velocity);
  let b = diatonicTranspose(a, degree);
  return sequence(a, b, note(root, 1/16, velocity - 20), rest(1/16));
}

fn counter(root: Pitch, variant: Int) -> Fragment context key {
  return sequence(cell(root, variant, 82), cell(root, variant + 1, 76));
}

fn lead(root: Pitch, seed: Int, count: Int) -> Fragment context key {
  let parts = for i in range(0, count) {
    if random(seed, i) > 0.55 {
      diatonicTranspose(
        note(root, 1/8, round(84 + random(seed, i + 100) * 30)),
        i % 5
      )
    } else {
      rest(1/8)
    }
  };
  return sequenceAll(parts);
}

fn drums7(boost: Int) -> Fragment context meter {
  return cycle(metricAccent(R7), [C2], boost + 102);
}

fn drums5() -> Fragment context meter {
  return cycle(accentFollow(Displaced5, R5, 1/8), [C2], 104);
}

fn drums4(boost: Int) -> Fragment context meter {
  return cycle(metricAccent(R4), [C2], boost + 106);
}

// --- production ------------------------------------------------------------

track Guitar { asset = GuitarSound; role = melodic; gain = -4 dB; pan = -0.2; }
track Bass { asset = BassSound; role = bass; gain = -5 dB; }
track Lead { asset = LeadSound; role = melodic; gain = -7 dB; pan = 0.25; }
track Kit { asset = DrumKitSound; role = percussion; gain = -4 dB; }

section A(4 bars) meter 7/8 grouping 2+2+3 {
  clip Guitar "Riff A" { play GuitarA; }
  clip Bass "Bass A" { play BassA; }
  clip Lead "Theme A" { play repeat(VerseTheme, 7); }
  clip Kit "Drums A" { play repeat(drums7(StatusBoost), 4); }
}

section B(4 bars) meter 5/8 grouping 2+3 {
  clip Guitar "Riff B" { play GuitarB; }
  clip Bass "Bass B" { play BassB; }
  clip Lead "Lead A" { play LeadA; }
  clip Kit "Drums B" { play repeat(drums5(), 4); }
}

section Reprise(4 bars) meter 4/4 grouping 2+2 {
  clip Guitar "Riff A Reprise" { play GuitarReprise; }
  clip Bass "Bass Reprise" { play BassReprise; }
  clip Lead "Counter" { play repeat(counter(D4, Variant), 4); }
  clip Lead "Lead B" { play LeadB; }
  clip Lead "Theme Reprise" { play repeat(Theme, 4); }
  clip Kit "Drums Reprise" { play repeat(drums4(StatusBoost), 4); }
}

arrangement {
  play A;
  play B;
  play Reprise;
}

export midi("math-rock.mid");
export wav("math-rock.wav");
