#include "TestSupport.hpp"

#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/music/Analysis.hpp"
#include "mpl/music/Chord.hpp"
#include "mpl/music/HarmonyTimeline.hpp"
#include "mpl/music/Instrument.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace mpl;

namespace {

Compilation compileMrp() {
    const std::filesystem::path path =
        std::filesystem::path(MPL_EXAMPLES_DIR) / "math-rock.mpl";
    return mpltest::compileText(mpltest::readText(path));
}

const ClipIR* findClip(const ProductionIR& production, const std::string& trackName,
                       const std::string& clipName) {
    for (const TrackIR& track : production.tracks) {
        if (track.name != trackName) {
            continue;
        }
        for (const ClipIR& clip : track.clips) {
            if (clip.name == clipName) {
                return &clip;
            }
        }
    }
    return nullptr;
}

}  // namespace

MPL_TEST(BenchmarkMeterMapMatchesMrpPlan) {
    const Compilation compilation = compileMrp();
    MPL_CHECK(!compilation.hasErrors());
    const auto& meters = compilation.production.meterMap;
    MPL_CHECK_EQ(meters.size(), std::size_t{3});
    MPL_CHECK(meters[0].position == Position{});
    MPL_CHECK(meters[0].meter.toString() == std::string("7/8"));
    MPL_CHECK(meters[1].position == Position(Rational(7, 2)));
    MPL_CHECK(meters[1].meter.toString() == std::string("5/8"));
    MPL_CHECK(meters[2].position == Position(Rational(6, 1)));
    MPL_CHECK(meters[2].meter.toString() == std::string("4/4"));
    MPL_CHECK_EQ(meters[0].barIndex, std::int64_t{0});
    MPL_CHECK_EQ(meters[1].barIndex, std::int64_t{4});
    MPL_CHECK_EQ(meters[2].barIndex, std::int64_t{8});
    MPL_CHECK(compilation.production.logicalExtent == Duration(Rational(10, 1)));
}

MPL_TEST(BenchmarkNonUniformGroupingAndHierarchy) {
    const Compilation compilation = compileMrp();
    const auto& meters = compilation.production.meterMap;
    MPL_CHECK(meters[0].grouping.toString() == std::string("2+2+3"));
    MPL_CHECK(meters[1].grouping.toString() == std::string("2+3"));
    MPL_CHECK(meters[2].grouping.toString() == std::string("2+2"));
    MeterMap map(Meter{7, 8}, MetricGrouping{{2, 2, 3}, true});
    MPL_CHECK_EQ(map.metricWeight(Position{}).level, 3);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(2, 8))).level, 2);
    MPL_CHECK_EQ(map.metricWeight(Position(Rational(1, 8))).level, 1);
    MeterMap alternate(Meter{7, 8}, MetricGrouping{{3, 2, 2}, true});
    MPL_CHECK_EQ(alternate.metricWeight(Position(Rational(2, 8))).level, 1);
    MPL_CHECK_EQ(alternate.metricWeight(Position(Rational(3, 8))).level, 2);
}

MPL_TEST(BenchmarkIndependentRhythmObjects) {
    const Compilation compilation = compileMrp();
    std::size_t rhythmCount = 0;
    for (const MusicNode& node : compilation.music.nodes()) {
        if (compilation.music.isRhythmObject(node.id)) {
            ++rhythmCount;
        }
    }
    MPL_CHECK(rhythmCount >= 10);
    for (const char* label : {"R3", "R7", "R5", "R4", "CellCycle", "BarCycle",
                              "CompositeCycle", "Displaced5"}) {
        MPL_CHECK(mpltest::rhythmByLabel(compilation, label) != nullptr);
    }
    const Rhythm* r3 = mpltest::rhythmByLabel(compilation, "R3");
    MPL_CHECK(r3 != nullptr);
    if (r3 != nullptr) {
        MPL_CHECK(r3->logicalLength() == Rational(3, 8));
        MPL_CHECK_EQ(r3->attackCount(), std::size_t{3});
    }
}

MPL_TEST(BenchmarkTwoCycleLengthsAndCommonPeriod) {
    const Compilation compilation = compileMrp();
    const Rhythm* r3 = mpltest::rhythmByLabel(compilation, "R3");
    const Rhythm* r7 = mpltest::rhythmByLabel(compilation, "R7");
    const Rhythm* cycle = mpltest::rhythmByLabel(compilation, "CellCycle");
    MPL_CHECK(r3 != nullptr);
    MPL_CHECK(r7 != nullptr);
    MPL_CHECK(cycle != nullptr);
    if (r3 != nullptr && r7 != nullptr && cycle != nullptr) {
        MPL_CHECK(r3->logicalLength() == Rational(3, 8));
        MPL_CHECK(r7->logicalLength() == Rational(7, 8));
        const std::optional<Rational> common = rhythmCommonPeriod(*r3, *r7);
        MPL_CHECK(common.has_value());
        if (common.has_value()) {
            MPL_CHECK(*common == Rational(21, 8));
        }
        MPL_CHECK(cycle->logicalLength() == Rational(21, 8));
    }
}

MPL_TEST(BenchmarkExactRealignment) {
    const Compilation compilation = compileMrp();
    const Rhythm* cellCycle = mpltest::rhythmByLabel(compilation, "CellCycle");
    const Rhythm* barCycle = mpltest::rhythmByLabel(compilation, "BarCycle");
    MPL_CHECK(cellCycle != nullptr);
    MPL_CHECK(barCycle != nullptr);
    if (cellCycle != nullptr && barCycle != nullptr) {
        MPL_CHECK(cellCycle->logicalLength() == Rational(21, 8));
        MPL_CHECK(barCycle->logicalLength() == Rational(21, 8));
        MPL_CHECK_EQ(cellCycle->attackCount(), std::size_t{21});
        MPL_CHECK_EQ(barCycle->attackCount(), std::size_t{18});
        // 21/8 is exactly 7 copies of 3/8 and 3 copies of 7/8.
        MPL_CHECK(cellCycle->contentEnd() <= Rational(21, 8));
        MPL_CHECK(barCycle->contentEnd() <= Rational(21, 8));
    }
}

MPL_TEST(BenchmarkCompositeOnsetUnion) {
    const Compilation compilation = compileMrp();
    const Rhythm* composite = mpltest::rhythmByLabel(compilation, "CompositeCycle");
    const Rhythm* cellCycle = mpltest::rhythmByLabel(compilation, "CellCycle");
    MPL_CHECK(composite != nullptr);
    MPL_CHECK(cellCycle != nullptr);
    if (composite != nullptr && cellCycle != nullptr) {
        MPL_CHECK(composite->logicalLength() == Rational(21, 8));
        MPL_CHECK(composite->attackCount() >= cellCycle->attackCount());
        MPL_CHECK(composite->events().front().onset == Rational(0, 1));
    }
}

MPL_TEST(BenchmarkRhythmPitchCombinationCounts) {
    const Compilation compilation = compileMrp();
    const ClipIR* guitarA = findClip(compilation.production, "Guitar", "Riff A");
    const ClipIR* guitarB = findClip(compilation.production, "Guitar", "Riff B");
    MPL_CHECK(guitarA != nullptr);
    MPL_CHECK(guitarB != nullptr);
    if (guitarA != nullptr) {
        // Four 7/8 bars, each with six attacks from R7.
        MPL_CHECK_EQ(guitarA->content.noteCount(), std::size_t{24});
    }
    if (guitarB != nullptr) {
        // Four 5/8 bars, each with four attacks.
        MPL_CHECK_EQ(guitarB->content.noteCount(), std::size_t{16});
    }
    // Rests never consume pitches: every produced onset has a valid rhythm
    // origin and event index.
    for (const Note& note : guitarA->content.notes()) {
        MPL_CHECK(note.rhythmOrigin != kInvalidObjectId);
        MPL_CHECK(note.rhythmEventIndex != kInvalidRhythmEvent);
    }
}

MPL_TEST(BenchmarkThemeRecapitulation) {
    const Compilation compilation = compileMrp();
    const ClipIR* verse = findClip(compilation.production, "Lead", "Theme A");
    const ClipIR* reprise = findClip(compilation.production, "Lead", "Theme Reprise");
    MPL_CHECK(verse != nullptr);
    MPL_CHECK(reprise != nullptr);
    if (verse == nullptr || reprise == nullptr) {
        return;
    }
    // The A-section statement is transposed by Variant; the reprise restores the
    // theme at its original pitch level. Both share the same rhythmic cell.
    MPL_CHECK(verse->content.notes().size() >= 3);
    MPL_CHECK(reprise->content.notes().size() >= 3);
    if (verse->content.notes().size() >= 3 && reprise->content.notes().size() >= 3) {
        MPL_CHECK(verse->content.notes()[0].pitch.name() == std::string("E4"));
        MPL_CHECK(reprise->content.notes()[0].pitch.name() == std::string("D4"));
        MPL_CHECK(verse->content.notes()[1].pitch.name() == std::string("G4"));
        MPL_CHECK(reprise->content.notes()[1].pitch.name() == std::string("F4"));
        MPL_CHECK(verse->content.notes()[2].pitch.name() == std::string("Bb4"));
        MPL_CHECK(reprise->content.notes()[2].pitch.name() == std::string("A4"));
        for (std::size_t index = 0; index < 3; ++index) {
            MPL_CHECK(verse->content.notes()[index].offset ==
                      reprise->content.notes()[index].offset);
        }
    }
}

MPL_TEST(BenchmarkUserFunctionsAndCalls) {
    const Compilation compilation = compileMrp();
    std::vector<std::string> names;
    for (const FunctionDefinitionRecord& function : compilation.functionDefinitions) {
        names.push_back(function.name);
    }
    for (const char* expected : {"cell", "counter", "lead", "drums7", "drums5", "drums4"}) {
        MPL_CHECK(std::find(names.begin(), names.end(), expected) != names.end());
    }
    MPL_CHECK(!compilation.functionCalls.empty());
    bool sawSectionContext = false;
    for (const FunctionCallRecord& call : compilation.functionCalls) {
        sawSectionContext = sawSectionContext ||
                            call.metricContext.find("section:") != std::string::npos;
    }
    MPL_CHECK(sawSectionContext);
}

MPL_TEST(BenchmarkMidiMeterTimeline) {
    const Compilation compilation = compileMrp();
    const std::vector<MidiEvent> meters =
        mpltest::conductorEvents(compilation, MidiEventKind::MetaTimeSignature);
    MPL_CHECK_EQ(meters.size(), std::size_t{3});
    MPL_CHECK_EQ(meters[0].tick, std::int64_t{0});
    MPL_CHECK_EQ(meters[1].tick, std::int64_t{6720});
    MPL_CHECK_EQ(meters[2].tick, std::int64_t{11520});
    MPL_CHECK_EQ(meters[1].a, 5);
    MPL_CHECK_EQ(meters[1].b, 8);
    MPL_CHECK_EQ(meters[2].a, 4);
    MPL_CHECK_EQ(meters[2].b, 4);
    MPL_CHECK(meters[0].text == std::string("2+2+3"));
}

MPL_TEST(BenchmarkSharedHarmonyTimeline) {
    const Compilation compilation = compileMrp();
    const HarmonyTimeline* harmony = nullptr;
    for (const MusicNode& node : compilation.music.nodes()) {
        if (node.label == "H") {
            harmony = compilation.music.cachedHarmony(node.id);
        }
    }
    MPL_CHECK(harmony != nullptr);
    if (harmony == nullptr) {
        return;
    }
    MPL_CHECK(harmony->logicalLength() == Rational(10, 1));
    MPL_CHECK_EQ(harmony->size(), std::size_t{7});
    MPL_CHECK(harmony->eventAt(Position(Rational(0, 1)))->chord.name() ==
              std::string("Dm9"));
    MPL_CHECK(harmony->eventAt(Position(Rational(7, 2)))->chord.name() ==
              std::string("Bbmaj7"));
    MPL_CHECK(harmony->eventAt(Position(Rational(9, 1)))->chord.name() ==
              std::string("A7b9"));
}

MPL_TEST(BenchmarkExtendedChordsAndInversions) {
    const ChordSymbol dm9 = ChordSymbol::parse("Dm9");
    MPL_CHECK(dm9.pitchClassSet().containsAll(
        PitchClassSet::fromPitchClasses({2, 5, 9, 0, 4})));
    const ChordSymbol a7b9 = ChordSymbol::parse("A7b9");
    MPL_CHECK(a7b9.contains(10));  // Bb
    const Voicing firstInversion = buildVoicing(dm9, Pitch::parse("C3"),
                                                Pitch::parse("C5"), 4, 1, 12);
    MPL_CHECK(firstInversion.status == VoicingStatus::StrictlySatisfied);
    MPL_CHECK(firstInversion.notes.front().name() == std::string("F3"));
}

MPL_TEST(BenchmarkVoiceLeadingInMrpReprise) {
    const Compilation compilation = compileMrp();
    const ClipIR* clip = findClip(compilation.production, "Guitar", "Riff A Reprise");
    MPL_CHECK(clip != nullptr);
    if (clip == nullptr) {
        return;
    }
    MPL_CHECK_EQ(clip->content.noteCount(), std::size_t{20});
    // Each 4/4 bar uses one voicing, repeated by R4 (five attacks). The first
    // four attacks of each bar are ascending chord tones.
    for (std::size_t bar = 0; bar < 4; ++bar) {
        for (std::size_t voice = 1; voice < 4; ++voice) {
            const std::size_t previous = bar * 5 + voice - 1;
            const std::size_t current = bar * 5 + voice;
            MPL_CHECK(clip->content.notes()[current].pitch.midi() >
                      clip->content.notes()[previous].pitch.midi());
        }
    }
    MPL_CHECK_EQ(compilation.production.tracks.size(), std::size_t{4});
}

MPL_TEST(BenchmarkHarmonyAndRhythmReplacement) {
    const Compilation compilation = compileMrp();
    const Fragment* harmonySwap = mpltest::fragmentByLabel(compilation, "GuitarHarmonySwap");
    const Fragment* rhythmSwap = mpltest::fragmentByLabel(compilation, "GuitarRhythmSwap");
    const Fragment* guitarA = mpltest::fragmentByLabel(compilation, "GuitarA");
    MPL_CHECK(harmonySwap != nullptr);
    MPL_CHECK(rhythmSwap != nullptr);
    MPL_CHECK(guitarA != nullptr);
    if (harmonySwap == nullptr || rhythmSwap == nullptr || guitarA == nullptr) {
        return;
    }
    // Same rhythm (R7, 6 attacks x 4 bars), different harmony: same count.
    MPL_CHECK_EQ(harmonySwap->noteCount(), guitarA->noteCount());
    // Different rhythm (R5), same harmony: different count.
    MPL_CHECK_EQ(rhythmSwap->noteCount(), std::size_t{16});
    MPL_CHECK(harmonySwap->notes().front().pitch != guitarA->notes().front().pitch ||
              harmonySwap->notes().back().pitch != guitarA->notes().back().pitch);
}

MPL_TEST(BenchmarkModuleIdentityAndStdlibUse) {
    const Compilation compilation = compileMrp();
    MPL_CHECK_EQ(compilation.modules.size(), std::size_t{4});
    bool sawStdHarmony = false;
    for (const ModuleRecord& module : compilation.modules) {
        MPL_CHECK(module.contentHash != 0);
        sawStdHarmony = sawStdHarmony || module.name == "StdHarmony";
    }
    MPL_CHECK(sawStdHarmony);
    const std::string trace = compilation.toTraceJson(false);
    MPL_CHECK(trace.find("\"modules\"") != std::string::npos);
    MPL_CHECK(trace.find("StdHarmony") != std::string::npos);
    bool sawStdFunction = false;
    for (const FunctionDefinitionRecord& function : compilation.functionDefinitions) {
        sawStdFunction = sawStdFunction || function.name == "arpBarAt" ||
                         function.name == "leadBarAt";
    }
    MPL_CHECK(sawStdFunction);
}

MPL_TEST(BenchmarkAnalysisAndRelationValidation) {
    const Compilation compilation = compileMrp();
    const Fragment* guitar = mpltest::fragmentByLabel(compilation, "GuitarA");
    const Fragment* bass = mpltest::fragmentByLabel(compilation, "BassA");
    MPL_CHECK(guitar != nullptr);
    MPL_CHECK(bass != nullptr);
    if (guitar != nullptr && bass != nullptr) {
        MPL_CHECK(noteIntervalOverlap(*guitar, *bass) > 0);
        MPL_CHECK(soundingRange(*guitar) > 0);
    }
    const std::vector<Pitch> lower = {Pitch::parse("C4"), Pitch::parse("D4")};
    const std::vector<Pitch> upper = {Pitch::parse("G4"), Pitch::parse("A4")};
    MPL_CHECK_EQ(parallelPerfects(lower, upper), 1);
    const InstrumentCapability guitarCapability = mpl::guitarCapability();
    MPL_CHECK(guitarCapability.contains(Pitch::parse("E2")));
    MPL_CHECK(!guitarCapability.positions(Pitch::parse("E7")).size());
}

MPL_TEST(BenchmarkVoiceLeadingStatusTaxonomy) {
    const std::vector<Pitch> previous = {Pitch::parse("D3"), Pitch::parse("F3"),
                                         Pitch::parse("A3"), Pitch::parse("C4")};
    const Voicing strict = voiceLead(previous, ChordSymbol::parse("G7"),
                                     Pitch::parse("D3"), Pitch::parse("D5"), 7, 12, false);
    MPL_CHECK(strict.status == VoicingStatus::StrictlySatisfied);
    const Voicing tolerance = voiceLead(previous, ChordSymbol::parse("G7"),
                                        Pitch::parse("D3"), Pitch::parse("D5"), 1, 12,
                                        false);
    MPL_CHECK(tolerance.status == VoicingStatus::SatisfiedWithinTolerance);
    const Voicing impossible = voiceLead(previous, ChordSymbol::parse("G7"),
                                         Pitch::parse("C2"), Pitch::parse("C3"), 7, 12,
                                         false);
    MPL_CHECK(impossible.status == VoicingStatus::Unsatisfied ||
              impossible.status == VoicingStatus::ProvablyUnsatisfiable);
    const Voicing budget = voiceLead(previous, ChordSymbol::parse("G7"),
                                     Pitch::parse("D2"), Pitch::parse("D6"), 7, 24,
                                     false, 1);
    MPL_CHECK(budget.status == VoicingStatus::BudgetExhausted);
}

MPL_TEST(BenchmarkCompilerReport) {
    const Compilation compilation = compileMrp();
    const std::string report = compilation.toReportJson();
    MPL_CHECK(report.find("\"format\": \"mpl.compiler-report\"") != std::string::npos);
    MPL_CHECK(report.find("\"compilerVersion\": \"0.4.0\"") != std::string::npos);
    MPL_CHECK(report.find("\"languageVersion\": \"0.3.0\"") != std::string::npos);
    MPL_CHECK(report.find("\"benchmarkVersion\": \"0.4.0\"") != std::string::npos);
    MPL_CHECK(report.find("\"observationVersion\": \"1.0.0\"") != std::string::npos);
    MPL_CHECK(report.find("\"compile\": \"ok\"") != std::string::npos);
    MPL_CHECK(report.find("\"validate\": \"ok\"") != std::string::npos);
    MPL_CHECK(report.find("\"execute\": \"not-run\"") != std::string::npos);
    MPL_CHECK(report.find("\"stdlibHash\": \"0\"") == std::string::npos);
    MPL_CHECK(compilation.stdlibHash() != 0);
}

MPL_TEST(BenchmarkMrpExtentAudit) {
    const Compilation compilation = compileMrp();
    MPL_CHECK(compilation.production.logicalExtent == Duration(Rational(10, 1)));
    const AudioRenderConfig config{8000, 128, 2, 1.0};
    const ReferenceAssetLibrary resolver;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, resolver, config, diagnostics);
    MPL_CHECK(diagnostics.empty());
    MPL_CHECK_NEAR(plan.logicalExtentSeconds, 10.0 * 240.0 / 132.0, 1e-9);
    MPL_CHECK_NEAR(plan.contentEndSeconds, 9.875 * 240.0 / 132.0, 1e-9);
    MPL_CHECK_NEAR(plan.processorTailSeconds, 0.0, 1e-12);
    MPL_CHECK(plan.renderedSeconds >= plan.logicalExtentSeconds + config.tailSeconds - 1e-9);
    MPL_CHECK(plan.renderedSeconds <= plan.logicalExtentSeconds + 1.5);
    MPL_CHECK_EQ(plan.totalFrames,
                 static_cast<int>(std::llround(plan.renderedSeconds * config.sampleRate)) +
                     plan.outputLatencyFrames);
}

MPL_TEST(BenchmarkDeterministicLocalModification) {
    const std::string base = mpltest::readText(
        std::filesystem::path(MPL_EXAMPLES_DIR) / "math-rock.mpl");
    const Compilation first = compileMrp();
    const Compilation second = compileMrp();
    MPL_CHECK(mpltest::midiOf(first) == mpltest::midiOf(second));
    std::string modified = base;
    const std::string token = "\"Dm9\" @ 0 whole";
    const std::size_t position = modified.find(token);
    MPL_CHECK(position != std::string::npos);
    modified.replace(position, token.size(), "\"Fmaj7\" @ 0 whole");
    const FrontendResult frontend = mpltest::frontend(
        modified, (std::filesystem::path(MPL_EXAMPLES_DIR) / "math-rock.mpl").string());
    MPL_CHECK(frontend.ok());
    if (!frontend.ok()) {
        return;
    }
    const Compiler compiler;
    const Compilation changed = compiler.compile(std::move(frontend.program->project));
    MPL_CHECK(mpltest::midiOf(first) != mpltest::midiOf(changed));
    MPL_CHECK_EQ(changed.production.tracks.size(), std::size_t{4});
}

MPL_TEST(BenchmarkAudioTimeline) {
    const Compilation compilation = compileMrp();
    const AudioRenderConfig config{8000, 128, 1, 0.0};
    const ReferenceAssetLibrary resolver;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, resolver, config, diagnostics);
    MPL_CHECK(diagnostics.empty());
    // Musical content ends at 9.875 whole notes (the last drum hit), while the
    // production keeps a logical extent of 10 whole notes for the arrangement.
    MPL_CHECK_NEAR(plan.contentEndSeconds, 9.875 * 240.0 / 132.0, 1e-6);
    MPL_CHECK(compilation.production.logicalExtent == Duration(Rational(10, 1)));
    // Section B starts at 7/2 whole notes.
    const int expectedSample =
        static_cast<int>(std::llround(3.5 * 240.0 / 132.0 * 8000.0));
    bool found = false;
    for (const TrackPlan& track : plan.tracks) {
        for (const PlannedNote& note : track.notes) {
            if (note.startSample == expectedSample) {
                found = true;
            }
        }
    }
    MPL_CHECK(found);
    const AudioBuffer buffer =
        ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
    MPL_CHECK(diagnostics.empty());
    MPL_CHECK(!buffer.samples.empty());
    MPL_CHECK_EQ(buffer.samples.size(),
                 static_cast<std::size_t>(plan.totalFrames) *
                     static_cast<std::size_t>(config.channels));
}

MPL_TEST(BenchmarkTemporalIrUsesAbsoluteTime) {
    const std::string source =
        mpltest::header("7/8") +
        "curve Auto on musical for 4 whole = linear(0, 1);\n"
        "let M = note(C4, 1/8, 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(2 bars) meter 7/8 grouping 2+2+3 {\n"
        "  clip Piano \"A\" { play M; }\n"
        "}\n"
        "section B(2 bars) meter 5/8 grouping 2+3 {\n"
        "  clip Piano \"B\" { play M; }\n"
        "}\n"
        "automation Piano.volume = Auto;\n"
        "arrangement { play A; play B; }\n"
        "export midi(\"x.mid\");\n";
    const Compilation compilation = mpltest::compileText(source);
    MPL_CHECK_EQ(compilation.production.automation.size(), std::size_t{1});
    const AutomationBinding& binding = compilation.production.automation.front();
    MPL_CHECK(binding.domain == TimeDomain::Musical);
    // The curve lives on absolute whole notes; meter changes do not rescale it.
    MPL_CHECK_NEAR(compilation.temporal.sample(binding.curve, Rational(0, 1)), 0.0, 1e-12);
    MPL_CHECK_NEAR(compilation.temporal.sample(binding.curve, Rational(2, 1)), 0.5, 1e-12);
    MPL_CHECK_NEAR(compilation.temporal.sample(binding.curve, Rational(7, 4)), 0.4375, 1e-12);
}

MPL_TEST(BenchmarkRandomContractIsRecorded) {
    const Compilation compilation = compileMrp();
    const std::string trace = compilation.toTraceJson(false);
    MPL_CHECK(trace.find("\"algorithm\": \"splitmix64\"") != std::string::npos);
    MPL_CHECK(trace.find("\"version\": 1") != std::string::npos);
    MPL_CHECK(trace.find("\"primitive\": \"random(seed,index)\"") != std::string::npos);
}

MPL_TEST(BenchmarkUnsupportedMidBarMeterIsDiagnosed) {
    const std::string source =
        mpltest::header("4/4") +
        "let M = note(C4, 1/8, 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section A(2 bars) meter 4/4 {\n"
        "  meter 7/8 at 1/8 bars grouping 2+2+3;\n"
        "  clip Piano { play M; }\n"
        "}\n"
        "arrangement { play A; }\n"
        "export midi(\"x.mid\");\n";
    const FrontendResult result = mpltest::frontend(source);
    MPL_CHECK(mpltest::hasCode(result, "meter-change-invalid"));
}

MPL_TEST(BenchmarkStrictZipMismatchIsDiagnosed) {
    const std::string source =
        mpltest::header("7/8") +
        "let R = rhythm { hit 1/8, hit 1/8, hit 1/8 };\n"
        "let M = zip(R, [C4, E4], 90);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) meter 7/8 grouping 2+2+3 { clip Piano { play M; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    const FrontendResult result = mpltest::frontend(source);
    MPL_CHECK(mpltest::hasCode(result, "rhythm-pitch-count"));
}
