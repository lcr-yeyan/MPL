#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/backend/MidiBackend.hpp"
#include "mpl/core/TimeTypes.hpp"
#include "mpl/examples/CoreDemo.hpp"
#include "mpl/ir/Automation.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

std::vector<MidiEvent> eventsOf(const std::vector<MidiTrack>& tracks, const std::string& name,
                                MidiEventKind kind) {
    std::vector<MidiEvent> result;
    for (const MidiTrack& track : tracks) {
        if (track.name != name) {
            continue;
        }
        for (const MidiEvent& event : track.events) {
            if (event.kind == kind) {
                result.push_back(event);
            }
        }
    }
    return result;
}

std::vector<int> velocitiesOf(const std::vector<MidiTrack>& tracks, const std::string& name) {
    std::vector<int> result;
    for (const MidiEvent& event : eventsOf(tracks, name, MidiEventKind::NoteOn)) {
        result.push_back(event.b);
    }
    return result;
}

std::vector<MidiEvent> ccOf(const std::vector<MidiTrack>& tracks, const std::string& name,
                            int controller) {
    std::vector<MidiEvent> result;
    for (const MidiEvent& event : eventsOf(tracks, name, MidiEventKind::ControlChange)) {
        if (event.a == controller) {
            result.push_back(event);
        }
    }
    return result;
}

bool hasCode(const Compilation& compilation, const std::string& code) {
    for (const Diagnostic& diagnostic : compilation.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

std::string diagnosticsText(const Compilation& compilation) {
    std::string text;
    for (const Diagnostic& diagnostic : compilation.diagnostics) {
        text += std::string(diagnostic.severityName()) + "[" + diagnostic.code + "]: " +
                diagnostic.message + "\n";
    }
    return text;
}

TemporalId findCurve(const TemporalIR& temporal, const std::string& label) {
    for (const TemporalNode& node : temporal.nodes()) {
        if (node.label == label) {
            return node.id;
        }
    }
    return kInvalidTemporalId;
}

Compilation compileTemporalDemo(std::vector<Diagnostic>* backendDiagnostics = nullptr) {
    Compiler compiler;
    Compilation compilation = compiler.compile(examples::buildTemporalDemoProject());
    MidiBackend backend;
    std::vector<Diagnostic> diagnostics;
    (void)backend.serialize(compilation.production, compilation.temporal, &diagnostics);
    compilation.diagnostics.insert(compilation.diagnostics.end(), diagnostics.begin(),
                                   diagnostics.end());
    if (backendDiagnostics != nullptr) {
        *backendDiagnostics = diagnostics;
    }
    return compilation;
}

Compilation compileTemporalDemoAtTempo(double bpm) {
    Project project = examples::buildTemporalDemoProject();
    project.setTempo(bpm);
    Compiler compiler;
    return compiler.compile(std::move(project));
}

Project tinyProject() {
    Project project;
    project.setTempo(120.0);
    project.setMeter(4, 4);
    const ObjectId part = project.music().melody({{"C4"_p, Rational(1, 4)}}, "A");
    project.addTrack("Piano", "GrandPiano", part);
    return project;
}

void expectSame(const std::string& actual, const std::string& expected, const std::string& name) {
    if (actual == expected) {
        return;
    }
    const std::filesystem::path outputDirectory(MPL_TEST_OUTPUT_DIR);
    std::filesystem::create_directories(outputDirectory);
    std::ofstream(outputDirectory / (name + ".actual"), std::ios::binary) << actual;
    std::ofstream(outputDirectory / (name + ".expected"), std::ios::binary) << expected;
    throw ::mpltest::AssertionFailure("mismatch: " + name + " (see " +
                                      outputDirectory.string() + ")");
}

}  // namespace

MPL_TEST(TemporalCurvesEvaluateAndClamp) {
    TemporalIR temporal;
    const TemporalId constant = temporal.constant(0.5);
    MPL_CHECK_NEAR(temporal.sample(constant, Rational(0, 1)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(constant, Rational(1, 1)), 0.5, 1e-12);

    const TemporalId linear = temporal.linear(0.0, 1.0);
    MPL_CHECK_NEAR(temporal.sample(linear, Rational(0, 1)), 0.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(linear, Rational(1, 2)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(linear, Rational(1, 1)), 1.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(linear, Rational(-1, 2)), 0.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(linear, Rational(2, 1)), 1.0, 1e-12);

    const TemporalId step = temporal.step(0.0, 1.0, Rational(1, 2));
    MPL_CHECK_NEAR(temporal.sample(step, Rational(1, 4)), 0.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(step, Rational(1, 2)), 1.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(step, Rational(3, 4)), 1.0, 1e-12);

    const TemporalId smooth = temporal.smoothstep(0.0, 1.0);
    MPL_CHECK_NEAR(temporal.sample(smooth, Rational(1, 2)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(smooth, Rational(1, 4)), 0.15625, 1e-12);

    const TemporalId keyframes = temporal.keyframes({
        CurvePoint{Rational(0, 1), 0.0, Interpolation::Linear},
        CurvePoint{Rational(1, 2), 1.0, Interpolation::Step},
        CurvePoint{Rational(1, 1), 0.0, Interpolation::Linear},
    });
    MPL_CHECK_NEAR(temporal.sample(keyframes, Rational(1, 4)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(keyframes, Rational(1, 2)), 1.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(keyframes, Rational(3, 4)), 1.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(keyframes, Rational(1, 1)), 0.0, 1e-12);
}

MPL_TEST(TemporalTransformsShiftStretchReverseRepeat) {
    TemporalIR temporal;
    const TemporalId base = temporal.linear(0.0, 1.0);

    const TemporalId shifted = temporal.shift(base, Rational(1, 4));
    MPL_CHECK(temporal.extent(shifted) == Rational(1, 1));
    MPL_CHECK_NEAR(temporal.sample(shifted, Rational(0, 1)), 0.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(shifted, Rational(1, 4)), 0.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(shifted, Rational(3, 4)), 0.5, 1e-12);

    const TemporalId stretched = temporal.stretch(base, Rational(2, 1));
    MPL_CHECK(temporal.extent(stretched) == Rational(2, 1));
    MPL_CHECK_NEAR(temporal.sample(stretched, Rational(1, 1)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(stretched, Rational(2, 1)), 1.0, 1e-12);

    const TemporalId reversed = temporal.reverse(base);
    MPL_CHECK_NEAR(temporal.sample(reversed, Rational(0, 1)), 1.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(reversed, Rational(1, 2)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(reversed, Rational(1, 1)), 0.0, 1e-12);

    const TemporalId repeated = temporal.repeat(base, 2);
    MPL_CHECK(temporal.extent(repeated) == Rational(2, 1));
    MPL_CHECK_NEAR(temporal.sample(repeated, Rational(1, 2)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(repeated, Rational(3, 2)), 0.5, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(repeated, Rational(1, 1)), 0.0, 1e-12);

    const TemporalId step = temporal.step(0.0, 1.0, Rational(1, 2));
    const TemporalId reversedStep = temporal.reverse(step);
    MPL_CHECK_NEAR(temporal.sample(reversedStep, Rational(1, 4)), 1.0, 1e-12);
    MPL_CHECK_NEAR(temporal.sample(reversedStep, Rational(3, 4)), 0.0, 1e-12);
}

MPL_TEST(TemporalCachingInvalidatesOnlyDependents) {
    TemporalIR temporal;
    const TemporalId a = temporal.constant(0.5);
    const TemporalId b = temporal.stretch(a, Rational(2, 1));
    const TemporalId c = temporal.reverse(b);
    const TemporalId independent = temporal.linear(0.0, 1.0);

    temporal.evaluateAll();
    MPL_CHECK_EQ(temporal.evaluatedCount(), std::size_t{4});
    const std::size_t before = temporal.evaluationCount();

    temporal.updateConstant(a, 0.8);
    MPL_CHECK(!temporal.node(a).value.has_value());
    MPL_CHECK(!temporal.node(b).value.has_value());
    MPL_CHECK(!temporal.node(c).value.has_value());
    MPL_CHECK(temporal.node(independent).value.has_value());
    MPL_CHECK_EQ(temporal.evaluatedCount(), std::size_t{1});

    temporal.evaluateAll();
    MPL_CHECK_EQ(temporal.evaluationCount(), before + 3);
    MPL_CHECK_NEAR(temporal.sample(a, Rational(0, 1)), 0.8, 1e-12);

    const std::vector<TemporalId> dependents = temporal.dependents(a);
    MPL_CHECK_EQ(dependents.size(), std::size_t{2});
    MPL_CHECK_EQ(dependents[0], b);
    MPL_CHECK_EQ(dependents[1], c);
}

MPL_TEST(TemporalRejectsInvalidInputs) {
    TemporalIR temporal;
    MPL_CHECK_THROWS(temporal.constant(0.5, TimeDomain::Relative, Rational(0, 1)));
    MPL_CHECK_THROWS(temporal.constant(0.5, TimeDomain::Physical));
    MPL_CHECK_THROWS(temporal.constant(0.5, TimeDomain::Sample));
    MPL_CHECK_THROWS(temporal.constant(std::numeric_limits<double>::quiet_NaN()));
    MPL_CHECK_THROWS(temporal.linear(0.0, std::numeric_limits<double>::infinity()));
    MPL_CHECK_THROWS(temporal.step(0.0, 1.0, Rational(3, 2)));
    MPL_CHECK_THROWS(temporal.step(0.0, 1.0, Rational(-1, 2)));
    MPL_CHECK_THROWS(temporal.repeat(temporal.constant(0.0), 0));
    MPL_CHECK_THROWS(temporal.stretch(temporal.constant(0.0), Rational(0, 1)));
    MPL_CHECK_THROWS(temporal.reverse(kInvalidTemporalId));
    MPL_CHECK_THROWS(temporal.keyframes({
        CurvePoint{Rational(1, 2), 1.0, Interpolation::Linear},
        CurvePoint{Rational(1, 4), 0.0, Interpolation::Linear},
    }));
    MPL_CHECK_THROWS(temporal.keyframes({
        CurvePoint{Rational(0, 1), 0.0, Interpolation::Linear},
        CurvePoint{Rational(0, 1), 1.0, Interpolation::Linear},
    }));
    MPL_CHECK_THROWS(temporal.keyframes({
        CurvePoint{Rational(2, 1), 0.0, Interpolation::Linear},
    }));

    const TemporalId relative = temporal.constant(0.5);
    MPL_CHECK_THROWS(temporal.sampleAt(relative, MusicalTime(Position(Rational(0, 1)))));
    MPL_CHECK_THROWS(temporal.sampleAt(relative, PhysicalTime(1.0)));
    MPL_CHECK_THROWS(temporal.sampleAt(relative, SampleTime(1)));
}

MPL_TEST(TemporalSerializesCurves) {
    TemporalIR temporal;
    const TemporalId linear = temporal.linear(0.25, 0.75, TimeDomain::Musical, Rational(4, 1),
                                             "Volume");
    (void)temporal.stretch(linear, Rational(2, 1), "Volume x2");
    temporal.evaluateAll();
    const std::string json = temporal.toJson();

    MPL_CHECK(json.find("\"format\": \"mpl.temporal-ir\"") != std::string::npos);
    MPL_CHECK(json.find("\"kind\": \"linear\"") != std::string::npos);
    MPL_CHECK(json.find("\"kind\": \"stretch\"") != std::string::npos);
    MPL_CHECK(json.find("\"domain\": \"musical\"") != std::string::npos);
    MPL_CHECK(json.find("\"factor\": \"2\"") != std::string::npos);
    MPL_CHECK(json.find("\"segments\"") != std::string::npos);
    MPL_CHECK(json.find("\"extent\": \"8\"") != std::string::npos);
}

MPL_TEST(DefinitionInstanceAndParameterIdsAreStable) {
    Project project = examples::buildTemporalDemoProject();
    const Compiler compiler;
    const Compilation first = compiler.compile(project);
    const Compilation second = compiler.compile(project);

    const TrackIR& piano = first.production.tracks[0];
    MPL_CHECK_EQ(piano.clips.size(), std::size_t{3});
    MPL_CHECK(piano.clips[0].sourceObject == piano.clips[1].sourceObject);
    MPL_CHECK(piano.clips[0].sourceObject == piano.clips[2].sourceObject);
    for (std::size_t index = 0; index < piano.clips.size(); ++index) {
        MPL_CHECK_EQ(piano.clips[index].instanceId, static_cast<InstanceId>(index));
        MPL_CHECK_EQ(piano.clips[index].instanceId,
                     second.production.tracks[0].clips[index].instanceId);
    }
    MPL_CHECK(piano.clips[0].expression.velocity != piano.clips[1].expression.velocity);
    MPL_CHECK(piano.clips[0].expression.velocity != piano.clips[2].expression.velocity);

    MPL_CHECK_EQ(makeParameterTarget(ParameterScope::Track, "Piano", "", "volume",
                                     ParameterUnit::Ratio)
                     .id.str(),
                 std::string("track:Piano:volume"));
    MPL_CHECK_EQ(makeParameterTarget(ParameterScope::Clip, "Piano", "Drop A", "gain",
                                     ParameterUnit::Decibel)
                     .id.str(),
                 std::string("clip:Piano:Drop A:gain"));
    MPL_CHECK_EQ(makeParameterTarget(ParameterScope::Master, "", "", "volume",
                                     ParameterUnit::Ratio)
                     .id.str(),
                 std::string("master:volume"));
}

MPL_TEST(TemporalDemoHasThreeExpressiveInstancesAndDiagnostics) {
    std::vector<Diagnostic> backendDiagnostics;
    const Compilation compilation = compileTemporalDemo(&backendDiagnostics);
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.production.tracks.size(), std::size_t{3});
    MPL_CHECK_EQ(compilation.production.clipCount(), std::size_t{5});
    MPL_CHECK_EQ(compilation.production.noteCount(), std::size_t{47});
    MPL_CHECK(compilation.production.totalDuration() == Duration(Rational(8, 1)));
    MPL_CHECK_EQ(compilation.temporal.size(), std::size_t{9});
    MPL_CHECK_EQ(compilation.production.automation.size(), std::size_t{4});

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<MidiTrack> tracks =
        backend.buildTracks(compilation.production, compilation.temporal);
    const std::vector<int> velocities = velocitiesOf(tracks, "Piano");
    MPL_CHECK_EQ(velocities.size(), std::size_t{15});

    for (std::size_t index = 0; index + 1 < 5; ++index) {
        MPL_CHECK(velocities[index] < velocities[index + 1]);
    }
    for (std::size_t index = 5; index + 1 < 10; ++index) {
        MPL_CHECK(velocities[index] > velocities[index + 1]);
    }

    bool panDiagnostic = false;
    bool quantizationDiagnostic = false;
    bool masterDiagnostic = false;
    for (const Diagnostic& diagnostic : backendDiagnostics) {
        panDiagnostic = panDiagnostic || diagnostic.code == "midi-unsupported-expression";
        quantizationDiagnostic = quantizationDiagnostic || diagnostic.code == "midi-quantization";
        masterDiagnostic =
            masterDiagnostic || diagnostic.code == "midi-unsupported-automation";
    }
    MPL_CHECK(panDiagnostic);
    MPL_CHECK(quantizationDiagnostic);
    MPL_CHECK(masterDiagnostic);
}

MPL_TEST(MidiExportsControlChangeAutomation) {
    const Compilation compilation = compileTemporalDemo();
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<MidiTrack> tracks =
        backend.buildTracks(compilation.production, compilation.temporal);

    const std::vector<MidiEvent> volume = ccOf(tracks, "Piano", 7);
    MPL_CHECK(volume.size() > std::size_t{30});
    MPL_CHECK_EQ(volume.front().tick, std::int64_t{0});
    MPL_CHECK_EQ(volume.back().tick, std::int64_t{7680});
    MPL_CHECK(volume.front().b < volume.back().b);
    for (std::size_t index = 0; index + 1 < volume.size(); ++index) {
        MPL_CHECK(volume[index].b <= volume[index + 1].b);
    }

    MPL_CHECK(!ccOf(tracks, "Piano", 1).empty());
    MPL_CHECK(!ccOf(tracks, "Piano", 11).empty());

    const std::string productionJson = compilation.production.toJson();
    MPL_CHECK(productionJson.find("controller") == std::string::npos);
    MPL_CHECK(productionJson.find("midiChannel") == std::string::npos);
    MPL_CHECK(productionJson.find("\"ppq\"") == std::string::npos);
}

MPL_TEST(AutomationValidationReportsConflictsAndRanges) {
    {
        Project project = tinyProject();
        const TemporalId curve = project.temporal().constant(0.5);
        const ParameterTarget target = makeParameterTarget(
            ParameterScope::Track, "Piano", "", "volume", ParameterUnit::Ratio);
        project.automation().push_back(
            AutomationBinding{"a", target, curve, TimeDomain::Musical, AutomationMode::Replace, true});
        project.automation().push_back(
            AutomationBinding{"b", target, curve, TimeDomain::Musical, AutomationMode::Replace, true});
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(compilation.hasErrors());
        MPL_CHECK(hasCode(compilation, "automation-conflict"));
    }
    {
        Project project = tinyProject();
        const TemporalId curve = project.temporal().constant(2.0);
        project.automation().push_back(AutomationBinding{
            "hot", makeParameterTarget(ParameterScope::Track, "Piano", "", "volume",
                                       ParameterUnit::Ratio),
            curve, TimeDomain::Musical, AutomationMode::Replace, true});
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(hasCode(compilation, "automation-range"));
    }
    {
        Project project = tinyProject();
        const TemporalId musical = project.temporal().constant(0.5, TimeDomain::Musical);
        project.automation().push_back(AutomationBinding{
            "ghost", makeParameterTarget(ParameterScope::Track, "Ghost", "", "volume",
                                         ParameterUnit::Ratio),
            musical, TimeDomain::Musical, AutomationMode::Replace, true});
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(hasCode(compilation, "unknown-target"));
    }
    {
        Project project = tinyProject();
        const TemporalId musical = project.temporal().constant(0.5, TimeDomain::Musical);
        project.tracks().front().clips.front().expression.velocity = musical;
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(hasCode(compilation, "expression-domain"));
    }
    {
        Project project = tinyProject();
        const TemporalId relative = project.temporal().constant(0.5);
        const TemporalId stretched = project.temporal().stretch(relative, Rational(2, 1));
        project.tracks().front().clips.front().expression.velocity = stretched;
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(hasCode(compilation, "expression-extent"));
    }
}

MPL_TEST(RelativeExpressionRemapsAfterDurationChange) {
    Project project;
    project.setTempo(120.0);
    project.setMeter(4, 4);

    std::vector<Note> notes;
    Note first;
    first.pitch = "F4"_p;
    first.duration = Duration(Rational(1, 8));
    notes.push_back(first);
    Note second;
    second.pitch = "G4"_p;
    second.offset = Duration(Rational(1, 2));
    second.duration = Duration(Rational(1, 8));
    notes.push_back(second);

    const ObjectId part = project.music().notes(notes, Duration(Rational(1, 1)), "A");
    const TemporalId ramp =
        project.temporal().linear(0.5, 1.0, TimeDomain::Relative, Rational(1, 1), "Ramp");
    project.addTrack("Piano", "GrandPiano", part);
    project.tracks().front().clips.front().expression.velocity = ramp;

    const Compiler compiler;
    const Compilation before = compiler.compile(project);
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<int> beforeVelocities =
        velocitiesOf(backend.buildTracks(before.production, before.temporal), "Piano");
    MPL_CHECK_EQ(beforeVelocities.size(), std::size_t{2});
    MPL_CHECK_EQ(beforeVelocities[0], 48);
    MPL_CHECK_EQ(beforeVelocities[1], 72);

    project.music().updateLiteral(part, notes, Duration(Rational(2, 1)));
    const Compilation after = compiler.compile(project);
    const std::string afterTrace = after.toTraceJson(false);
    const std::vector<int> afterVelocities =
        velocitiesOf(backend.buildTracks(after.production, after.temporal), "Piano");
    MPL_CHECK_EQ(afterVelocities[0], 48);
    MPL_CHECK_EQ(afterVelocities[1], 60);

    Project fresh;
    fresh.setTempo(120.0);
    fresh.setMeter(4, 4);
    const ObjectId freshPart = fresh.music().notes(notes, Duration(Rational(2, 1)), "A");
    const TemporalId freshRamp =
        fresh.temporal().linear(0.5, 1.0, TimeDomain::Relative, Rational(1, 1), "Ramp");
    fresh.addTrack("Piano", "GrandPiano", freshPart);
    fresh.tracks().front().clips.front().expression.velocity = freshRamp;
    const Compilation rebuilt = compiler.compile(std::move(fresh));
    MPL_CHECK_EQ(after.music.toJson(), rebuilt.music.toJson());
    MPL_CHECK_EQ(after.temporal.toJson(), rebuilt.temporal.toJson());
    MPL_CHECK_EQ(after.production.toJson(), rebuilt.production.toJson());
    expectSame(afterTrace, rebuilt.toTraceJson(false), "trace-remap");
}

MPL_TEST(BpmChangesPhysicalTimeNotMusicalTime) {
    const Compilation fast = compileTemporalDemoAtTempo(160.0);
    const Compilation slow = compileTemporalDemoAtTempo(120.0);

    MPL_CHECK(fast.production.totalDuration() == slow.production.totalDuration());
    MPL_CHECK(fast.production.totalDuration() == Duration(Rational(8, 1)));
    MPL_CHECK_NEAR(fast.production.totalDuration().toSeconds(160.0), 12.0, 1e-9);
    MPL_CHECK_NEAR(slow.production.totalDuration().toSeconds(120.0), 16.0, 1e-9);

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<MidiTrack> fastTracks =
        backend.buildTracks(fast.production, fast.temporal);
    const std::vector<MidiTrack> slowTracks =
        backend.buildTracks(slow.production, slow.temporal);
    const std::vector<MidiEvent> fastNotes =
        eventsOf(fastTracks, "Piano", MidiEventKind::NoteOn);
    const std::vector<MidiEvent> slowNotes =
        eventsOf(slowTracks, "Piano", MidiEventKind::NoteOn);
    MPL_CHECK_EQ(fastNotes.size(), slowNotes.size());
    for (std::size_t index = 0; index < fastNotes.size(); ++index) {
        MPL_CHECK_EQ(fastNotes[index].tick, slowNotes[index].tick);
        MPL_CHECK_EQ(fastNotes[index].b, slowNotes[index].b);
    }
    const std::vector<MidiEvent> fastCc = ccOf(fastTracks, "Piano", 7);
    const std::vector<MidiEvent> slowCc = ccOf(slowTracks, "Piano", 7);
    MPL_CHECK_EQ(fastCc.size(), slowCc.size());
    for (std::size_t index = 0; index < fastCc.size(); ++index) {
        MPL_CHECK_EQ(fastCc[index].tick, slowCc[index].tick);
        MPL_CHECK_EQ(fastCc[index].b, slowCc[index].b);
    }

    const std::vector<std::uint8_t> fastBytes = backend.serialize(fast.production, fast.temporal);
    const std::vector<std::uint8_t> slowBytes = backend.serialize(slow.production, slow.temporal);
    const std::vector<std::uint8_t> fastTempo = {0xFF, 0x51, 0x03, 0x05, 0xB8, 0xD8};
    const std::vector<std::uint8_t> slowTempo = {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20};
    MPL_CHECK(std::search(fastBytes.begin(), fastBytes.end(), fastTempo.begin(),
                          fastTempo.end()) != fastBytes.end());
    MPL_CHECK(std::search(slowBytes.begin(), slowBytes.end(), slowTempo.begin(),
                          slowTempo.end()) != slowBytes.end());
}

MPL_TEST(CurveEditsDoNotRecomputeMusicObjects) {
    Project project = examples::buildTemporalDemoProject();
    project.music().evaluateAll();
    const std::size_t evaluationsBefore = project.music().evaluationCount();
    const std::size_t evaluatedBefore = project.music().evaluatedCount();

    const TemporalId volume = findCurve(project.temporal(), "Volume 4 bars");
    MPL_CHECK(volume != kInvalidTemporalId);
    project.temporal().evaluateAll();
    project.temporal().updateLinear(volume, 0.2, 0.9);
    project.temporal().evaluateAll();

    MPL_CHECK_EQ(project.music().evaluationCount(), evaluationsBefore);
    MPL_CHECK_EQ(project.music().evaluatedCount(), evaluatedBefore);

    const Compiler compiler;
    const Compilation compilation = compiler.compile(project);
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<MidiEvent> volumeCc =
        ccOf(backend.buildTracks(compilation.production, compilation.temporal), "Piano", 7);
    MPL_CHECK(!volumeCc.empty());
    MPL_CHECK_EQ(volumeCc.front().b, 25);
}

MPL_TEST(IncrementalTemporalCompilationMatchesFullRebuild) {
    Project incremental = examples::buildTemporalDemoProject();
    const TemporalId incrementalVolume = findCurve(incremental.temporal(), "Volume 4 bars");
    incremental.temporal().updateLinear(incrementalVolume, 0.2, 0.9);

    Project fresh = examples::buildTemporalDemoProject();
    const TemporalId freshVolume = findCurve(fresh.temporal(), "Volume 4 bars");
    fresh.temporal().updateLinear(freshVolume, 0.2, 0.9);

    const Compiler compiler;
    const Compilation first = compiler.compile(std::move(incremental));
    const Compilation second = compiler.compile(std::move(fresh));

    MPL_CHECK_EQ(first.temporal.toJson(), second.temporal.toJson());
    MPL_CHECK_EQ(first.production.toJson(), second.production.toJson());
    MPL_CHECK_EQ(first.music.toJson(), second.music.toJson());

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    MPL_CHECK_EQ(hexDump(backend.serialize(first.production, first.temporal)),
                 hexDump(backend.serialize(second.production, second.temporal)));
}

MPL_TEST(MidiCombinesAutomationModes) {
    Project project = tinyProject();
    const TemporalId replace = project.temporal().constant(0.5, TimeDomain::Musical);
    const TemporalId add = project.temporal().constant(0.2, TimeDomain::Musical);
    const TemporalId multiply = project.temporal().constant(0.5, TimeDomain::Musical);
    const ParameterTarget target = makeParameterTarget(
        ParameterScope::Track, "Piano", "", "volume", ParameterUnit::Ratio);
    project.automation().push_back(
        AutomationBinding{"base", target, replace, TimeDomain::Musical, AutomationMode::Replace, true});
    project.automation().push_back(
        AutomationBinding{"lift", target, add, TimeDomain::Musical, AutomationMode::Add, true});
    project.automation().push_back(AutomationBinding{"shape", target, multiply,
                                                     TimeDomain::Musical,
                                                     AutomationMode::Multiply, true});

    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(project));
    MPL_CHECK_EQ(diagnosticsText(compilation), std::string());

    const MidiBackend backend(MidiOptions{480, "Conductor"});
    std::vector<Diagnostic> diagnostics;
    const std::vector<MidiTrack> tracks =
        backend.buildTracks(compilation.production, compilation.temporal, &diagnostics);
    const std::vector<MidiEvent> volume = ccOf(tracks, "Piano", 7);
    MPL_CHECK(!volume.empty());
    MPL_CHECK_EQ(volume.front().b, 44);
    for (const Diagnostic& diagnostic : diagnostics) {
        MPL_CHECK(diagnostic.code != "automation-conflict");
    }
}

MPL_TEST(AutomationAddAndMultiplyUseIdentityBases) {
    {
        Project project = tinyProject();
        const TemporalId curve = project.temporal().constant(0.2, TimeDomain::Musical);
        project.automation().push_back(AutomationBinding{
            "add", makeParameterTarget(ParameterScope::Track, "Piano", "", "volume",
                                       ParameterUnit::Ratio),
            curve, TimeDomain::Musical, AutomationMode::Add, true});
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(!compilation.hasErrors());
        const MidiBackend backend(MidiOptions{480, "Conductor"});
        const std::vector<MidiEvent> cc =
            ccOf(backend.buildTracks(compilation.production, compilation.temporal), "Piano", 7);
        MPL_CHECK(!cc.empty());
        MPL_CHECK_EQ(cc.front().b, 25);
    }
    {
        Project project = tinyProject();
        const TemporalId curve = project.temporal().constant(0.5, TimeDomain::Musical);
        project.automation().push_back(AutomationBinding{
            "multiply", makeParameterTarget(ParameterScope::Track, "Piano", "", "volume",
                                            ParameterUnit::Ratio),
            curve, TimeDomain::Musical, AutomationMode::Multiply, true});
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(!compilation.hasErrors());
        const MidiBackend backend(MidiOptions{480, "Conductor"});
        const std::vector<MidiEvent> cc =
            ccOf(backend.buildTracks(compilation.production, compilation.temporal), "Piano", 7);
        MPL_CHECK(!cc.empty());
        MPL_CHECK_EQ(cc.front().b, 64);
    }
}

MPL_TEST(UnsuitableTemporalBackendUsageIsDiagnosed) {
    const Compilation compilation = compileTemporalDemo();
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK(hasCode(compilation, "midi-unsupported-expression"));
    MPL_CHECK(hasCode(compilation, "midi-unsupported-automation"));
    MPL_CHECK(hasCode(compilation, "midi-quantization"));

    Project project = tinyProject();
    const TemporalId relative = project.temporal().constant(0.5);
    project.automation().push_back(AutomationBinding{
        "relative", makeParameterTarget(ParameterScope::Track, "Piano", "", "volume",
                                        ParameterUnit::Ratio),
        relative, TimeDomain::Relative, AutomationMode::Replace, true});
    const Compiler compiler;
    const Compilation relativeCompilation = compiler.compile(std::move(project));
    MPL_CHECK(!relativeCompilation.hasErrors());
    std::vector<Diagnostic> diagnostics;
    const MidiBackend backend(MidiOptions{480, "Conductor"});
    const std::vector<MidiTrack> tracks =
        backend.buildTracks(relativeCompilation.production, relativeCompilation.temporal,
                            &diagnostics);
    bool unsupported = false;
    for (const Diagnostic& diagnostic : diagnostics) {
        unsupported = unsupported || diagnostic.code == "midi-unsupported-automation";
    }
    MPL_CHECK(unsupported);
    MPL_CHECK(ccOf(tracks, "Piano", 7).empty());
}
