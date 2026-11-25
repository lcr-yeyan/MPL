#include "TestSupport.hpp"

#include "mpl/music/Analysis.hpp"
#include "mpl/music/Chord.hpp"
#include "mpl/music/HarmonyTimeline.hpp"
#include "mpl/music/Instrument.hpp"

#include <string>
#include <vector>

using namespace mpl;

namespace {

ChordSymbol chord(const std::string& text) {
    return ChordSymbol::parse(text);
}

std::vector<int> pcs(const ChordSymbol& symbol) {
    return symbol.pitchClassSet().values;
}

std::string pitchesText(const std::vector<Pitch>& notes) {
    std::string text;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        if (index != 0) {
            text += " ";
        }
        text += notes[index].name();
    }
    return text;
}

}  // namespace

MPL_TEST(ChordParsesTriadsAndSevenths) {
    MPL_CHECK(pcs(chord("Dm7")) == (std::vector<int>{0, 2, 5, 9}));
    MPL_CHECK(pcs(chord("G7")) == (std::vector<int>{2, 5, 7, 11}));
    MPL_CHECK(pcs(chord("Cmaj7")) == (std::vector<int>{0, 4, 7, 11}));
    MPL_CHECK(pcs(chord("Bbdim")) == (std::vector<int>{1, 4, 10}));
    MPL_CHECK(pcs(chord("F#dim7")) == (std::vector<int>{0, 3, 6, 9}));
    MPL_CHECK(pcs(chord("Asus4")) == (std::vector<int>{2, 4, 9}));
    MPL_CHECK(pcs(chord("C5")) == (std::vector<int>{0, 7}));
}

MPL_TEST(ChordParsesExtensionsAndAlterations) {
    MPL_CHECK(pcs(chord("Dm9")) == (std::vector<int>{0, 2, 4, 5, 9}));
    const ChordSymbol a7b9 = chord("A7b9");
    MPL_CHECK(a7b9.contains(9));   // A
    MPL_CHECK(a7b9.contains(1));   // C#
    MPL_CHECK(a7b9.contains(4));   // E
    MPL_CHECK(a7b9.contains(7));   // G
    MPL_CHECK(a7b9.contains(10));  // Bb
    const ChordSymbol g7sharp5 = chord("G7#5");
    std::string intervalText;
    for (const int interval : g7sharp5.intervals) {
        intervalText += std::to_string(interval) + ",";
    }
    MPL_CHECK_EQ(intervalText, std::string("0,4,8,10,"));
    MPL_CHECK(g7sharp5.contains(3));  // D# / Eb
    MPL_CHECK(!g7sharp5.contains(2)); // no D natural
    const ChordSymbol cmaj13 = chord("Cmaj13");
    MPL_CHECK(cmaj13.contains(9));  // A (13)
    MPL_CHECK(cmaj13.contains(5));  // F (11)
}

MPL_TEST(ChordHandlesSlashBass) {
    const ChordSymbol cOverE = chord("C/E");
    MPL_CHECK(cOverE.valid());
    MPL_CHECK_EQ(cOverE.bassPitchClass, 4);
    MPL_CHECK_EQ(cOverE.inversion(), 1);
    MPL_CHECK(pcs(cOverE) == (std::vector<int>{0, 4, 7}));
}

MPL_TEST(ChordRejectsInvalidSymbols) {
    MPL_CHECK_THROWS(chord(""));
    MPL_CHECK_THROWS(chord("H7"));
    MPL_CHECK_THROWS(chord("Cxyz"));
    MPL_CHECK_THROWS(chord("C/"));
}

MPL_TEST(PitchClassSetOperations) {
    const PitchClassSet a = PitchClassSet::fromPitchClasses({0, 4, 7, 11});
    const PitchClassSet b = PitchClassSet::fromPitchClasses({0, 2, 4, 9});
    MPL_CHECK(a.contains(7));
    MPL_CHECK(!a.contains(2));
    MPL_CHECK(a.intersection(b).values == (std::vector<int>{0, 4}));
    MPL_CHECK(a.containsAll(PitchClassSet::fromPitchClasses({0, 4})));
    MPL_CHECK(!a.containsAll(b));
}

MPL_TEST(HarmonyTimelineQueriesBoundariesAndGaps) {
    HarmonyTimeline timeline = HarmonyTimeline::fromEvents(
        {{Rational(0, 1), Rational(1, 1), chord("Dm7"), kInvalidObjectId},
         {Rational(1, 1), Rational(1, 1), chord("G7"), kInvalidObjectId},
         {Rational(2, 1), Rational(2, 1), chord("Cmaj7"), kInvalidObjectId}},
        Rational(4, 1));
    MPL_CHECK(timeline.eventAt(Position(Rational(0, 1)))->chord.name() == std::string("Dm7"));
    MPL_CHECK(timeline.eventAt(Position(Rational(1, 1)))->chord.name() == std::string("G7"));
    MPL_CHECK(timeline.eventAt(Position(Rational(3, 1)))->chord.name() == std::string("Cmaj7"));
    MPL_CHECK(timeline.eventAt(Position(Rational(4, 1))) == nullptr);
    MPL_CHECK_EQ(timeline.eventsAt(Position(Rational(1, 1))).size(), std::size_t{1});
    MPL_CHECK(timeline.logicalLength() == Rational(4, 1));
}

MPL_TEST(HarmonyTimelineOverlapDeterminism) {
    const HarmonyTimeline timeline = HarmonyTimeline::fromEvents(
        {{Rational(0, 1), Rational(3, 1), chord("Dm7"), kInvalidObjectId},
         {Rational(1, 1), Rational(1, 1), chord("G7"), kInvalidObjectId},
         {Rational(1, 1), Rational(1, 1), chord("A7"), kInvalidObjectId}},
        Rational(4, 1));
    // The greatest start wins; the later declaration wins the tie.
    MPL_CHECK(timeline.eventAt(Position(Rational(1, 1)))->chord.name() == std::string("A7"));
    MPL_CHECK_EQ(timeline.eventsAt(Position(Rational(1, 1))).size(), std::size_t{3});
    MPL_CHECK(timeline.eventsAt(Position(Rational(1, 1)))[0]->chord.name() ==
              std::string("Dm7"));
}

MPL_TEST(HarmonyTimelineRejectsInvalidEvents) {
    MPL_CHECK_THROWS(HarmonyTimeline::fromEvents(
        {{Rational(0, 1), Rational(5, 1), chord("Dm7"), kInvalidObjectId}},
        Rational(4, 1)));
    MPL_CHECK_THROWS(HarmonyTimeline::fromEvents(
        {{Rational(0, 1), Rational(0, 1), chord("Dm7"), kInvalidObjectId}},
        Rational(4, 1)));
    MPL_CHECK_THROWS(HarmonyTimeline::fromEvents(
        {{Rational(0, 1), Rational(1, 1), ChordSymbol::none(), kInvalidObjectId}},
        Rational(1, 1)));
}

MPL_TEST(VoicingBuildsInversionAndRange) {
    const Voicing root = buildVoicing(chord("Dm7"), Pitch::parse("D3"), Pitch::parse("D5"), 4,
                                      0, 12);
    MPL_CHECK(root.status == VoicingStatus::StrictlySatisfied);
    MPL_CHECK(pitchesText(root.notes) == std::string("D3 F3 A3 C4"));
    const Voicing firstInversion = buildVoicing(chord("Dm7"), Pitch::parse("D3"),
                                                Pitch::parse("D5"), 4, 1, 12);
    MPL_CHECK(firstInversion.status == VoicingStatus::StrictlySatisfied);
    MPL_CHECK(pitchesText(firstInversion.notes) == std::string("F3 A3 C4 D4"));
    MPL_CHECK(firstInversion.complete);
}

MPL_TEST(VoicingOmissionAndDoublingRules) {
    const Voicing three = buildVoicing(chord("Dm7"), Pitch::parse("D3"), Pitch::parse("D5"), 3,
                                       0, 12);
    MPL_CHECK_EQ(three.notes.size(), std::size_t{3});
    MPL_CHECK(!three.complete);  // the seventh is omitted
    const Voicing five = buildVoicing(chord("Dm7"), Pitch::parse("D3"), Pitch::parse("D5"), 5,
                                      0, 12);
    MPL_CHECK_EQ(five.notes.size(), std::size_t{5});
    MPL_CHECK(five.complete);
    MPL_CHECK(five.notes[0].name() == std::string("D3"));
    MPL_CHECK(five.notes[4].name() == std::string("D4"));  // doubled bass
}

MPL_TEST(VoicingReportsImpossibleRange) {
    const Voicing tooNarrow = buildVoicing(chord("Dm7"), Pitch::parse("C4"),
                                           Pitch::parse("D4"), 4, 0, 2);
    MPL_CHECK(tooNarrow.status == VoicingStatus::ProvablyUnsatisfiable);
    const Voicing noTones = buildVoicing(chord("Dm7"), Pitch::parse("C4"),
                                         Pitch::parse("C4"), 4, 0, 12);
    MPL_CHECK(noTones.status == VoicingStatus::ProvablyUnsatisfiable);
}

MPL_TEST(VoiceLeadMinimizesMovement) {
    const std::vector<Pitch> previous = {Pitch::parse("D3"), Pitch::parse("F3"),
                                         Pitch::parse("A3"), Pitch::parse("C4")};
    const Voicing result = voiceLead(previous, chord("G7"), Pitch::parse("D3"),
                                     Pitch::parse("D5"), 7, 12, false);
    MPL_CHECK(result.status == VoicingStatus::StrictlySatisfied);
    MPL_CHECK(result.searchComplete);
    MPL_CHECK_EQ(result.notes.size(), std::size_t{4});
    MPL_CHECK_EQ(result.crossings, 0);
    MPL_CHECK(result.totalMovement <= 4);
    MPL_CHECK(pitchesText(result.notes) == std::string("D3 F3 G3 B3"));
    MPL_CHECK(result.complete);
}

MPL_TEST(VoiceLeadToleranceAndUnsatisfiedStatuses) {
    const std::vector<Pitch> previous = {Pitch::parse("D3"), Pitch::parse("F3"),
                                         Pitch::parse("A3"), Pitch::parse("C4")};
    const Voicing tolerance = voiceLead(previous, chord("G7"), Pitch::parse("D3"),
                                        Pitch::parse("D5"), 1, 12, false);
    MPL_CHECK_EQ(std::string(voicingStatusName(tolerance.status)), std::string("satisfied-within-tolerance"));
    MPL_CHECK(tolerance.totalMovement > 0);
    const Voicing unsatisfied = voiceLead(previous, chord("G7"), Pitch::parse("D3"),
                                          Pitch::parse("D5"), -1, 12, false);
    MPL_CHECK(unsatisfied.status == VoicingStatus::ProvablyUnsatisfiable);
}

MPL_TEST(VoiceLeadBudgetIsExplicit) {
    const std::vector<Pitch> previous = {Pitch::parse("D2"), Pitch::parse("F2"),
                                         Pitch::parse("A2"), Pitch::parse("C3")};
    const Voicing result = voiceLead(previous, chord("G7"), Pitch::parse("D2"),
                                     Pitch::parse("D6"), 7, 24, false, 1);
    MPL_CHECK(result.status == VoicingStatus::BudgetExhausted);
    MPL_CHECK(!result.searchComplete);
}

MPL_TEST(VoiceLeadCrossingPolicy) {
    const std::vector<Pitch> previous = {Pitch::parse("C4"), Pitch::parse("E4")};
    const Voicing strict = voiceLead(previous, chord("G7"), Pitch::parse("C4"),
                                     Pitch::parse("G5"), 7, 12, false);
    MPL_CHECK(strict.crossings == 0);
    for (std::size_t index = 1; index < strict.notes.size(); ++index) {
        MPL_CHECK(strict.notes[index].midi() > strict.notes[index - 1].midi());
    }
}

MPL_TEST(AnalysisHelpersComputeRelations) {
    const std::vector<Pitch> a = {Pitch::parse("D4"), Pitch::parse("F4"), Pitch::parse("A4")};
    const std::vector<Pitch> b = {Pitch::parse("F4"), Pitch::parse("A4"), Pitch::parse("C5")};
    MPL_CHECK_EQ(totalVoiceMovement(a, b), 10);
    MPL_CHECK_EQ(commonTones(a, b).size(), std::size_t{2});
    MPL_CHECK_EQ(voiceCrossings(a), 0);
    MPL_CHECK(isChordTone(chord("Dm7"), Pitch::parse("F4")));
    MPL_CHECK(!isChordTone(chord("Dm7"), Pitch::parse("G4")));
}

MPL_TEST(HarmonyLiteralAndChordQueriesInLanguage) {
    const std::string source =
        mpltest::header("4/4", "D minor") +
        "let H = harmony span 4 whole {\n"
        "  \"Dm7\" @ 0 whole for 1 whole,\n"
        "  \"G7\" @ 1 whole for 1 whole,\n"
        "  \"Cmaj7\" @ 2 whole for 2 whole\n"
        "};\n"
        "let Dm = chordAt(H, 0 whole);\n"
        "let G7 = chordAt(H, 1 whole);\n"
        "let Cmaj = chordAt(H, 3 whole);\n"
        "let Names = sequence(rest(1/8), rest(1/8));\n"
        "let R = rhythm { hit 1/4, hit 1/4 };\n"
        "let M = cycle(R, voicingNotes(voicing(G7, D3, D5, 4, 0, 12)), 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(4 bars) { clip Piano \"H\" { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    MPL_CHECK(!compilation.hasErrors());
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{2});
    MPL_CHECK(notes[0].pitch.name() == std::string("G3"));
    MPL_CHECK(notes[1].pitch.name() == std::string("B3"));
    MPL_CHECK(compilation.music.isHarmonyObject(0));
    MPL_CHECK(compilation.music.isChordObject(1));
}

MPL_TEST(AnalysisOverlapRangeAndParallelMotion) {
    const Fragment a = Fragment::fromNotes(
        {Note{Pitch::parse("C4"), Duration(Rational(0, 1)), Duration(Rational(1, 2)), 90,
              kInvalidObjectId, kInvalidRhythmEvent},
         Note{Pitch::parse("E4"), Duration(Rational(1, 2)), Duration(Rational(1, 2)), 90,
              kInvalidObjectId, kInvalidRhythmEvent}},
        Duration(Rational(1, 1)));
    const Fragment b = Fragment::fromNotes(
        {Note{Pitch::parse("G4"), Duration(Rational(1, 4)), Duration(Rational(1, 2)), 90,
              kInvalidObjectId, kInvalidRhythmEvent}},
        Duration(Rational(1, 1)));
    MPL_CHECK_EQ(noteIntervalOverlap(a, b), 2);
    MPL_CHECK_EQ(soundingRange(a), 4);
    const std::vector<Pitch> lower = {Pitch::parse("C4"), Pitch::parse("D4")};
    const std::vector<Pitch> upper = {Pitch::parse("G4"), Pitch::parse("A4")};
    MPL_CHECK_EQ(parallelPerfects(lower, upper), 1);
    const std::vector<Pitch> oblique = {Pitch::parse("C4"), Pitch::parse("C4")};
    MPL_CHECK_EQ(parallelPerfects(lower, oblique), 0);
}

MPL_TEST(HarmonySustainedNoteKeepsOnsetChord) {
    const Compilation compilation = mpltest::compileText(
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset Piano = instrument(\"GrandPiano\");\n"
        "let H = harmony span 2 whole {\n"
        "  \"Dm9\" @ 0 whole for 1 whole,\n"
        "  \"G7\" @ 1 whole for 1 whole\n"
        "};\n"
        "let M = notes span 2 whole { D4 @ 0 for 2 whole vel 90 };\n"
        "track P { asset = Piano; role = harmony; }\n"
        "section S(2 bars) { clip P { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n");
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{1});
    MPL_CHECK(notes.front().duration == Duration(Rational(2, 1)));
    const HarmonyTimeline* harmony = nullptr;
    for (const MusicNode& node : compilation.music.nodes()) {
        if (node.label == "H") {
            harmony = compilation.music.cachedHarmony(node.id);
        }
    }
    MPL_CHECK(harmony != nullptr);
    if (harmony != nullptr) {
        MPL_CHECK(harmony->eventAt(Position{})->chord.name() == std::string("Dm9"));
        MPL_CHECK(harmony->eventAt(Position(Rational(1, 1)))->chord.name() ==
                  std::string("G7"));
    }
}

MPL_TEST(VoiceLeadSearchCompletenessIsExplicit) {
    const std::vector<Pitch> previous = {Pitch::parse("D3"), Pitch::parse("F3"),
                                         Pitch::parse("A3"), Pitch::parse("C4")};
    const Voicing strict = voiceLead(previous, chord("G7"), Pitch::parse("D3"),
                                     Pitch::parse("D5"), 7, 12, false);
    MPL_CHECK(strict.searchComplete);
    const Voicing budget = voiceLead(previous, chord("G7"), Pitch::parse("D2"),
                                     Pitch::parse("D6"), 7, 24, false, 1);
    MPL_CHECK(!budget.searchComplete);
}

MPL_TEST(InstrumentCapabilityGuitarBoundaries) {
    const InstrumentCapability guitar = guitarCapability();
    MPL_CHECK_EQ(guitar.openStrings.size(), std::size_t{6});
    MPL_CHECK_EQ(guitar.maxPolyphony, 6);
    MPL_CHECK(guitar.contains(Pitch::parse("E2")));
    MPL_CHECK(!guitar.contains(Pitch::parse("Eb2")));
    const std::vector<FretPosition> e3 = guitar.positions(Pitch::parse("E3"));
    MPL_CHECK(!e3.empty());
    bool foundOpenLow = false;
    bool foundDString = false;
    for (const FretPosition& position : e3) {
        foundOpenLow = foundOpenLow || (position.stringIndex == 0 && position.fret == 12);
        foundDString = foundDString || (position.stringIndex == 2 && position.fret == 2);
        MPL_CHECK_EQ(guitar.openStrings[static_cast<std::size_t>(position.stringIndex)] +
                         position.fret,
                     Pitch::parse("E3").midi());
    }
    MPL_CHECK(foundOpenLow);
    MPL_CHECK(foundDString);
    MPL_CHECK(guitar.positions(Pitch::parse("E7")).empty());
    MPL_CHECK(fitsInstrument(guitar, {Pitch::parse("E2"), Pitch::parse("B3"),
                                      Pitch::parse("E4")}));
    MPL_CHECK(!fitsInstrument(guitar, {Pitch::parse("C2")}));
    // The advertised highest pitch must be reachable by the highest string.
    const std::vector<FretPosition> highest = guitar.positions(Pitch::fromMidi(guitar.highestMidi));
    MPL_CHECK(!highest.empty());
    for (const FretPosition& position : highest) {
        MPL_CHECK_EQ(position.stringIndex, 5);
        MPL_CHECK_EQ(position.fret, guitar.fretCount);
    }
}

MPL_TEST(AnalysisBuiltinsAreAvailableInLanguage) {
    const std::string source =
        mpltest::header() +
        "let A = notes { C4 @ 0 for 1/2 };\n"
        "let B = notes { G4 @ 1/4 for 1/2 };\n"
        "let O = noteOverlap(A, B);\n"
        "let Range = fragmentRange(A);\n"
        "let P = parallelPerfects([C4, D4], [G4, A4]);\n"
        "let M = note(C4, 1/8, 50 + O + Range + P);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    const auto& notes = compilation.production.tracks.front().clips.front().content.notes();
    MPL_CHECK_EQ(notes.size(), std::size_t{1});
    MPL_CHECK_EQ(notes.front().velocity, 52);
}

MPL_TEST(HarmonyAcrossVariableMeter) {
    const std::string source =
        mpltest::header("4/4") +
        "let H = harmony span 4 whole {\n"
        "  \"Dm7\" @ 0 whole for 2 whole,\n"
        "  \"G7\" @ 2 whole for 2 whole\n"
        "};\n"
        "let G = chordAt(H, 2 whole);\n"
        "let R = rhythm { hit 1/4, hit 1/4 };\n"
        "let M = cycle(R, voicingNotes(voicing(G, D3, D5, 4, 0, 12)), 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(2 bars) meter 7/8 grouping 2+2+3 {\n"
        "  clip Piano \"A\" { play M; }\n"
        "}\n"
        "section B(2 bars) meter 5/8 grouping 2+3 {\n"
        "  clip Piano \"B\" { play M; }\n"
        "}\n"
        "arrangement { play A; play B; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.production.meterMap.size(), std::size_t{2});
}

MPL_TEST(LanguageReportsChordAndVoicingErrors) {
    const std::string source =
        mpltest::header() +
        "let C = chord(\"H7\");\n"
        "let R = rhythm { hit 1/4, hit 1/4 };\n"
        "let M = cycle(R, voicingNotes(voicing(C, C4, C4, 4, 0, 2)), 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const FrontendResult result = mpltest::frontend(source);
    MPL_CHECK(mpltest::hasCode(result, "type-chord"));
}
