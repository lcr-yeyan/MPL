#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/core/Diagnostic.hpp"
#include "mpl/examples/CoreDemo.hpp"
#include "mpl/ir/Assets.hpp"
#include "mpl/music/Harmony.hpp"
#include "mpl/music/Key.hpp"

#include <string>

using namespace mpl;
using namespace mpl::literals;

namespace {

Note noteAt(Pitch pitch, const Rational& offset, const Rational& length) {
    Note note;
    note.pitch = pitch;
    note.offset = Duration(offset);
    note.duration = Duration(length);
    return note;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

MPL_TEST(KeyParsesNamesAndScales) {
    const Key fMinor = Key::parse("F minor");
    MPL_CHECK_EQ(fMinor.name(), std::string("F minor"));
    MPL_CHECK_EQ(fMinor.tonicPitchClass(), 5);
    const std::array<int, 7> scale = fMinor.scaleSemitones();
    MPL_CHECK_EQ(scale[2], 3);
    MPL_CHECK_EQ(scale[6], 10);
    MPL_CHECK_EQ(Key::parse("C# major").tonicPitchClass(), 1);
    MPL_CHECK_THROWS(Key::parse("H minor"));
    MPL_CHECK_THROWS(Key::parse("F lydian"));
}

MPL_TEST(DiatonicTransposeFollowsTheScale) {
    const Key fMinor = Key::parse("F minor");
    MPL_CHECK_EQ(diatonicTranspose("F4"_p, 2, fMinor).name(), std::string("Ab4"));
    MPL_CHECK_EQ(diatonicTranspose("Ab4"_p, 2, fMinor).name(), std::string("C5"));
    MPL_CHECK_EQ(diatonicTranspose("C5"_p, 2, fMinor).name(), std::string("Eb5"));
    MPL_CHECK_EQ(diatonicTranspose("Eb5"_p, 2, fMinor).name(), std::string("G5"));
    MPL_CHECK_EQ(diatonicTranspose("B3"_p, 1, fMinor).name(), std::string("C#4"));
    MPL_CHECK_EQ(diatonicTranspose("F4"_p, -2, fMinor).name(), std::string("Db4"));
}

MPL_TEST(DiatonicTransposePreservesChromaticAlteration) {
    const Key fMinor = Key::parse("F minor");
    // B natural is a raised fourth degree in F minor; moving one degree up
    // therefore produces a raised fifth (C#), not a diatonic C.
    MPL_CHECK_EQ(diatonicTranspose("B3"_p, 1, fMinor).name(), std::string("C#4"));
    // E natural is a raised seventh; one degree up is a raised tonic (F#).
    MPL_CHECK_EQ(diatonicTranspose("E4"_p, 1, fMinor).name(), std::string("F#4"));
    // C# is a raised fifth; two degrees up is a raised sixth (E natural).
    MPL_CHECK_EQ(diatonicTranspose("C#4"_p, 2, fMinor).name(), std::string("E4"));
    // In C major the same spelling rule produces a plain diatonic step.
    MPL_CHECK_EQ(diatonicTranspose("F4"_p, 1, Key::parse("C major")).name(),
                 std::string("G4"));
}

MPL_TEST(HarmonizeAddsDiatonicChordTones) {
    const Fragment root = Fragment::fromMelody({{"F2"_p, Rational(1, 1)}});
    const Fragment chord = harmonize(root, {2, 4}, Key::parse("F minor"), 80, 7);

    MPL_CHECK_EQ(chord.noteCount(), std::size_t{3});
    MPL_CHECK_EQ(chord.notes()[0].pitch.name(), std::string("F2"));
    MPL_CHECK_EQ(chord.notes()[1].pitch.name(), std::string("Ab2"));
    MPL_CHECK_EQ(chord.notes()[2].pitch.name(), std::string("C3"));
    MPL_CHECK(chord.notes()[0].origin == kInvalidObjectId);
    MPL_CHECK(chord.notes()[1].origin == ObjectId{7});
    MPL_CHECK(chord.notes()[2].origin == ObjectId{7});
    MPL_CHECK(chord.duration() == Duration(Rational(1, 1)));
}

MPL_TEST(SyncopateShiftsOnlyBeatAlignedNotes) {
    const Fragment source = Fragment::fromNotes({
        noteAt("C4"_p, Rational(0, 1), Rational(1, 4)),
        noteAt("D4"_p, Rational(1, 8), Rational(1, 8)),
        noteAt("E4"_p, Rational(1, 4), Rational(1, 4)),
    });

    const Fragment shifted = syncopate(source, Meter{4, 4}, Rational(1, 8));
    MPL_CHECK_EQ(shifted.noteCount(), std::size_t{3});
    MPL_CHECK(shifted.notes()[0].offset == Duration(Rational(1, 8)));
    MPL_CHECK(shifted.notes()[1].offset == Duration(Rational(1, 8)));
    MPL_CHECK(shifted.notes()[2].offset == Duration(Rational(3, 8)));
    MPL_CHECK(shifted.duration() == Duration(Rational(5, 8)));
}

MPL_TEST(ContextInvalidatesOnlyDependentOperators) {
    MusicIR music;
    music.setKey(Key::parse("F minor"));
    const ObjectId a = music.melody({{"F4"_p, Rational(1, 4)}}, "A");
    const ObjectId t = music.transpose(a, Interval::octaves(1), "T");
    const ObjectId d = music.diatonicTranspose(a, 2, "D");
    const ObjectId h = music.harmonize(a, {2}, "H");
    const ObjectId s = music.syncopate(a, Rational(1, 8), "S");

    MPL_CHECK_EQ(contextDependencies(NodeKind::Harmonize).size(), std::size_t{1});
    MPL_CHECK_EQ(contextDependencies(NodeKind::Syncopate).size(), std::size_t{1});
    MPL_CHECK_EQ(contextDependencies(NodeKind::Transpose).size(), std::size_t{0});

    music.evaluateAll();
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{5});
    MPL_CHECK_EQ(music.node(d).value->notes().front().pitch.name(), std::string("Ab4"));

    music.setKey(Key::parse("C major"));
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{3});
    MPL_CHECK(music.node(a).value.has_value());
    MPL_CHECK(music.node(t).value.has_value());
    MPL_CHECK(music.node(s).value.has_value());
    MPL_CHECK(!music.node(d).value.has_value());
    MPL_CHECK(!music.node(h).value.has_value());

    music.setMeter(Meter{3, 4});
    MPL_CHECK_EQ(music.evaluatedCount(), std::size_t{2});

    music.evaluateAll();
    MPL_CHECK_EQ(music.node(d).value->notes().front().pitch.name(), std::string("A4"));
}

MPL_TEST(IncrementalEvaluationMatchesFullRebuild) {
    MusicIR incremental;
    incremental.setKey(Key::parse("F minor"));
    const ObjectId a = incremental.melody({{"F4"_p, Rational(1, 4)}}, "A");
    const ObjectId dependent = incremental.diatonicTranspose(a, 2, "D");
    const ObjectId phrase = incremental.sequence({a, dependent}, "Phrase");
    const ObjectId independent = incremental.melody({{"C2"_p, Rational(1, 4)}}, "I");
    (void)independent;
    incremental.evaluateAll();
    MPL_CHECK_EQ(incremental.evaluationCount(), std::size_t{4});

    incremental.updateLiteral(a, {noteAt("A4"_p, Rational(0, 1), Rational(1, 4))});
    incremental.evaluateAll();
    MPL_CHECK_EQ(incremental.evaluationCount(), std::size_t{7});
    MPL_CHECK_EQ(incremental.cacheHitCount(), std::size_t{7});

    MusicIR fresh;
    fresh.setKey(Key::parse("F minor"));
    const ObjectId freshA = fresh.melody({{"A4"_p, Rational(1, 4)}}, "A");
    const ObjectId freshDependent = fresh.diatonicTranspose(freshA, 2, "D");
    (void)fresh.sequence({freshA, freshDependent}, "Phrase");
    (void)fresh.melody({{"C2"_p, Rational(1, 4)}}, "I");
    fresh.evaluateAll();

    MPL_CHECK_EQ(incremental.toJson(), fresh.toJson());
    MPL_CHECK(incremental.node(phrase).value->notes()[1].pitch.name() == "C#5");
}

MPL_TEST(ProvenanceSurvivesTransformsAndStampsGeneratedNotes) {
    MusicIR music;
    music.setKey(Key::parse("F minor"));
    const ObjectId a = music.melody({{"F4"_p, Rational(1, 4)}}, "A");
    const ObjectId t = music.transpose(a, Interval::octaves(1), "T");
    const ObjectId h = music.harmonize(t, {2}, "H");
    music.evaluateAll();

    MPL_CHECK(music.node(t).value->notes().front().origin == a);
    const Fragment& chord = *music.node(h).value;
    MPL_CHECK_EQ(chord.notes()[0].origin, a);
    MPL_CHECK_EQ(chord.notes()[1].origin, h);
    MPL_CHECK_EQ(chord.notes()[1].pitch.name(), std::string("Ab5"));
}

MPL_TEST(ProductionIrStaysBackendNeutral) {
    const Compiler compiler;
    const Compilation result = compiler.compile(examples::buildCoreDemoProject());
    const std::string json = result.production.toJson();

    MPL_CHECK(!contains(json, "midiChannel"));
    MPL_CHECK(!contains(json, "midiProgram"));
    MPL_CHECK(!contains(json, "\"percussion\""));
    MPL_CHECK(!contains(json, "\"ppq\""));
    MPL_CHECK(contains(json, "\"role\""));
    MPL_CHECK(contains(json, "\"assetSlots\""));
    MPL_CHECK(contains(json, "\"assetBindings\""));
}

MPL_TEST(TraceConnectsGeneratedNotesToOperators) {
    Project project;
    project.setKey(Key::parse("F minor"));
    MusicIR& music = project.music();
    const ObjectId root = music.melody({{"F2"_p, Rational(1, 1)}}, "Root");
    const ObjectId voicing = music.harmonize(root, {2, 4}, "Pad Voicing");
    project.addTrack("Pad", "WarmPad", voicing, {}, InstrumentRole::Harmony);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    MPL_CHECK(!result.hasErrors());
    const std::string trace = result.toTraceJson();

    MPL_CHECK(contains(trace, "\"format\": \"mpl.trace\""));
    MPL_CHECK(contains(trace, "\"kind\": \"harmonize\""));
    MPL_CHECK(contains(trace, "\"originLabel\": \"Pad Voicing\""));
    MPL_CHECK(contains(trace, "\"derivation\""));
}

MPL_TEST(TraceTracksMultiSourceOriginChains) {
    Project project;
    project.setKey(Key::parse("F minor"));
    MusicIR& music = project.music();
    const ObjectId source = music.melody({{"F4"_p, Rational(1, 4)}}, "Source");
    const ObjectId chord = music.harmonize(source, {2}, "Chord");
    const ObjectId phrase = music.sequence({source, chord}, "Phrase");
    project.addTrack("Piano", "GrandPiano", phrase);

    const Compiler compiler;
    const Compilation result = compiler.compile(std::move(project));
    const std::string trace = result.toTraceJson();

    MPL_CHECK(contains(trace, "\"originChain\""));
    MPL_CHECK(contains(trace, "\"originLabel\": \"Source\""));
    MPL_CHECK(contains(trace, "\"originLabel\": \"Chord\""));
    MPL_CHECK(contains(trace, "\"noteInstance\""));
    MPL_CHECK(contains(trace, "\"derivation\""));
}

MPL_TEST(AssetInterfacesAreMinimalAndResolvable) {
    const AssetBinding symbolic{"main", "GrandPiano", "symbolic"};
    MPL_CHECK(symbolic.isResolved());
    MPL_CHECK(!AssetBinding{}.isResolved());

    MPL_CHECK_EQ(std::string(instrumentRoleName(InstrumentRole::Percussion)),
                 std::string("percussion"));
    MPL_CHECK(instrumentRoleFromName("harmony") == InstrumentRole::Harmony);
    MPL_CHECK_THROWS(instrumentRoleFromName("brass"));
}

MPL_TEST(SuiteDemoHasSectionsRolesAndHarmonicDependency) {
    const Compiler compiler;
    const Compilation result = compiler.compile(examples::buildSuiteDemoProject());
    MPL_CHECK(!result.hasErrors());
    MPL_CHECK_EQ(result.production.tracks.size(), std::size_t{5});
    MPL_CHECK_EQ(result.production.clipCount(), std::size_t{11});
    MPL_CHECK_EQ(result.production.noteCount(), std::size_t{416});
    MPL_CHECK(result.production.totalDuration() == Duration(Rational(24, 1)));
    MPL_CHECK_NEAR(result.production.totalDuration().toSeconds(160.0), 36.0, 1e-9);

    MPL_CHECK(result.production.tracks[0].role == InstrumentRole::Melodic);
    MPL_CHECK(result.production.tracks[1].role == InstrumentRole::Harmony);
    MPL_CHECK(result.production.tracks[2].role == InstrumentRole::Bass);
    MPL_CHECK(result.production.tracks[4].role == InstrumentRole::Percussion);

    const TrackIR& piano = result.production.tracks[0];
    MPL_CHECK_EQ(piano.clips.size(), std::size_t{4});
    MPL_CHECK_EQ(piano.clips[0].name, std::string("Intro Piano"));
    MPL_CHECK_EQ(piano.clips[1].name, std::string("Drop A"));
    MPL_CHECK_EQ(piano.clips[2].name, std::string("Drop B"));
    MPL_CHECK_EQ(piano.clips[3].name, std::string("Outro"));
    MPL_CHECK(piano.clips[3].start == Position(Rational(20, 1)));

    const std::string trace = result.toTraceJson();
    MPL_CHECK(contains(trace, "\"key\": \"F minor\""));
    MPL_CHECK(contains(trace, "\"originLabel\": \"Pad Voicing\""));
}
