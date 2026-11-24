#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/audio/WavFile.hpp"
#include "mpl/frontend/Frontend.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

Compilation compileProject(Project project) {
    const Compiler compiler;
    return compiler.compile(std::move(project));
}

Project oneNoteProject(double bpm, Position start = Position{}, Rational length = Rational(1, 4)) {
    Project project;
    project.setTempo(bpm);
    project.setMeter(4, 4);
    const ObjectId part = project.music().melody({{"C4"_p, length}}, "A");
    project.addTrack("Piano", "GrandPiano", part, start);
    return project;
}

InstrumentAsset testInstrument(float gain = 1.0f) {
    InstrumentAsset instrument;
    instrument.id = "test-sine";
    instrument.version = "1";
    instrument.oscillator = Oscillator::Sine;
    instrument.envelope = AdsrEnvelope{0.0, 0.0, 1.0, 0.0};
    instrument.gain = gain;
    return instrument;
}

TrackPlan testTrack(std::string name, std::vector<PlannedNote> notes, float gain = 1.0f,
                    float pan = 0.0f) {
    TrackPlan track;
    track.name = std::move(name);
    track.instrument = testInstrument(gain);
    track.notes = std::move(notes);
    (void)pan;
    return track;
}

PlannedNote plannedNote(int midi, int startSample, int endSample, float velocity = 0.8f,
                        bool hasPan = false, float pan = 0.0f) {
    PlannedNote note;
    note.midi = midi;
    note.startSample = startSample;
    note.endSample = endSample;
    note.velocity = velocity;
    note.hasPan = hasPan;
    note.pan = pan;
    return note;
}

AudioExecutionPlan manualPlan(const AudioRenderConfig& config, std::vector<TrackPlan> tracks,
                              int totalFrames) {
    AudioExecutionPlan plan;
    plan.config = config;
    plan.tracks = std::move(tracks);
    plan.tempo.push_back(TempoSegment{0.0, 0.0, 120.0});
    plan.totalFrames = totalFrames;
    plan.renderedSeconds = static_cast<double>(totalFrames) / config.sampleRate;
    return plan;
}

double energy(const AudioBuffer& buffer) {
    double total = 0.0;
    for (const float sample : buffer.samples) {
        total += static_cast<double>(sample) * sample;
    }
    return total;
}

double peak(const AudioBuffer& buffer) {
    double maximum = 0.0;
    for (const float sample : buffer.samples) {
        maximum = std::max(maximum, std::fabs(static_cast<double>(sample)));
    }
    return maximum;
}

double channelEnergy(const AudioBuffer& buffer, int channel) {
    double total = 0.0;
    for (std::size_t frame = 0; frame < buffer.frameCount(); ++frame) {
        const double sample = buffer.samples[frame * buffer.channels + channel];
        total += sample * sample;
    }
    return total;
}

std::uint64_t fnv1a(const AudioBuffer& buffer) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto* bytes = reinterpret_cast<const unsigned char*>(buffer.samples.data());
    for (std::size_t index = 0; index < buffer.samples.size() * sizeof(float); ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string audioStatsJson(const AudioBuffer& buffer) {
    const double rms = buffer.samples.empty()
                           ? 0.0
                           : std::sqrt(energy(buffer) / static_cast<double>(buffer.samples.size()));
    std::ostringstream hash;
    hash << std::hex << fnv1a(buffer);
    std::ostringstream out;
    out << "{\n";
    out << "  \"sampleRate\": " << buffer.sampleRate << ",\n";
    out << "  \"channels\": " << buffer.channels << ",\n";
    out << "  \"frames\": " << buffer.frameCount() << ",\n";
    out << "  \"peak\": " << peak(buffer) << ",\n";
    out << "  \"rms\": " << rms << ",\n";
    out << "  \"hash\": \"" << hash.str() << "\"\n";
    out << "}\n";
    return out.str();
}

void checkGolden(const std::string& actual, const std::string& name) {
    const std::filesystem::path golden = std::filesystem::path(MPL_GOLDEN_DIR) / name;
    if (::mpltest::updateGoldenMode()) {
        std::filesystem::create_directories(golden.parent_path());
        std::ofstream stream(golden, std::ios::binary | std::ios::trunc);
        stream << actual;
        return;
    }
    const std::string expected = readText(golden);
    if (actual == expected) {
        return;
    }
    throw ::mpltest::AssertionFailure("audio golden mismatch: " + golden.string());
}

class FixedResolver final : public AssetResolver {
public:
    FixedResolver(Oscillator oscillator, bool fail = false)
        : oscillator_(oscillator), fail_(fail) {}

    std::optional<InstrumentAsset> resolve(const TrackIR&,
                                           const AssetBinding& binding,
                                           std::vector<Diagnostic>& diagnostics) const override {
        if (fail_) {
            diagnostics.push_back(
                Diagnostic::error("audio-missing-asset", "missing test asset"));
            return std::nullopt;
        }
        InstrumentAsset asset;
        asset.id = binding.assetId;
        asset.version = "fixed";
        asset.oscillator = oscillator_;
        asset.envelope = AdsrEnvelope{0.0, 0.0, 1.0, 0.0};
        asset.gain = 0.5f;
        return asset;
    }

private:
    Oscillator oscillator_;
    bool fail_ = false;
};

FrontendResult compileExample(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path(MPL_EXAMPLES_DIR) / name;
    return compileSource(readText(path), path.string());
}

}  // namespace

MPL_TEST(AudioPitchAndClockConversions) {
    MPL_CHECK_NEAR(midiToFrequency(69), 440.0, 1e-9);
    MPL_CHECK_NEAR(midiToFrequency(60), 261.6255653006, 1e-6);
    MPL_CHECK_NEAR(midiToFrequency(81), 880.0, 1e-9);

    const std::vector<TempoEvent> tempo{TempoEvent{Position{}, 120.0}};
    const AudioClock clock(tempo);
    MPL_CHECK_NEAR(clock.secondsAtWhole(1.0), 2.0, 1e-12);
    MPL_CHECK_NEAR(clock.secondsPerWhole(0.0), 2.0, 1e-12);
    MPL_CHECK_NEAR(clock.wholeAtSeconds(3.0), 1.5, 1e-12);

    const std::vector<TempoEvent> changing{TempoEvent{Position{}, 120.0},
                                           TempoEvent{Position(Rational(2, 1)), 240.0}};
    const AudioClock changingClock(changing);
    MPL_CHECK_NEAR(changingClock.secondsAtWhole(2.0), 4.0, 1e-12);
    MPL_CHECK_NEAR(changingClock.secondsAtWhole(3.0), 5.0, 1e-12);
    MPL_CHECK_NEAR(changingClock.wholeAtSeconds(5.0), 3.0, 1e-12);
}

MPL_TEST(AudioPlanPlacesNotesAtExactSamples) {
    const Compilation compilation =
        compileProject(oneNoteProject(120.0, Position(Rational(1, 1)), Rational(1, 4)));
    const AudioRenderConfig config{8000, 128, 2, 0.0};
    const ReferenceAssetLibrary resolver;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, resolver, config, diagnostics);

    MPL_CHECK(diagnostics.empty());
    MPL_CHECK_EQ(plan.tracks.size(), std::size_t{1});
    MPL_CHECK_EQ(plan.tracks.front().notes.size(), std::size_t{1});
    const PlannedNote& note = plan.tracks.front().notes.front();
    MPL_CHECK_EQ(note.startSample, 16000);  // 1 whole at 120 BPM = 2 s
    MPL_CHECK_EQ(note.endSample, 20000);    // quarter note = 0.5 s
    MPL_CHECK_NEAR(plan.contentEndSeconds, 2.5, 1e-9);
    MPL_CHECK_EQ(plan.totalFrames, static_cast<int>(std::llround(
                                       (2.5 + plan.tracks.front().instrument.envelope.release) *
                                       8000.0)));
}

MPL_TEST(AudioRenderHonoursReleaseAndTailExtent) {
    AudioRenderConfig config{8000, 64, 2, 0.5};
    TrackPlan track = testTrack("t", {plannedNote(69, 0, 4000)}, 1.0f);
    track.instrument.envelope = AdsrEnvelope{0.0, 0.0, 1.0, 0.25};
    AudioExecutionPlan plan = manualPlan(config, {track}, 0);
    plan.contentEndSeconds = 0.5;
    plan.renderedSeconds = 0.5 + 0.25 + 0.5;
    plan.totalFrames = static_cast<int>(plan.renderedSeconds * 8000.0);

    ReferenceAudioBackend backend(config);
    std::vector<Diagnostic> diagnostics;
    const AudioBuffer buffer = backend.renderPlan(plan, TemporalIR{}, diagnostics);
    MPL_CHECK_EQ(static_cast<int>(buffer.frameCount()), plan.totalFrames);
    MPL_CHECK(peak(buffer) > 0.01);
    for (std::size_t frame = buffer.frameCount() - 100; frame < buffer.frameCount(); ++frame) {
        MPL_CHECK(std::fabs(buffer.samples[frame * 2]) < 1e-6);
    }
}

MPL_TEST(AudioPolyphonyGainAndPan) {
    AudioRenderConfig config{8000, 128, 2, 0.0};
    ReferenceAudioBackend backend(config);
    std::vector<Diagnostic> diagnostics;

    const PlannedNote one = plannedNote(69, 0, 4000);
    const PlannedNote two = plannedNote(76, 0, 4000);
    const AudioBuffer single =
        backend.renderPlan(manualPlan(config, {testTrack("a", {one})}, 4000), TemporalIR{},
                           diagnostics);
    const AudioBuffer poly =
        backend.renderPlan(manualPlan(config, {testTrack("a", {one, two})}, 4000), TemporalIR{},
                           diagnostics);
    MPL_CHECK(energy(poly) > energy(single) * 1.2);

    PlannedNote left = plannedNote(69, 0, 4000, 0.8f, true, -1.0f);
    const AudioBuffer leftBuffer =
        backend.renderPlan(manualPlan(config, {testTrack("a", {left})}, 4000), TemporalIR{},
                           diagnostics);
    MPL_CHECK(channelEnergy(leftBuffer, 0) > channelEnergy(leftBuffer, 1) * 10.0);

    PlannedNote right = plannedNote(69, 0, 4000, 0.8f, true, 1.0f);
    const AudioBuffer rightBuffer =
        backend.renderPlan(manualPlan(config, {testTrack("a", {right})}, 4000), TemporalIR{},
                           diagnostics);
    MPL_CHECK(channelEnergy(rightBuffer, 1) > channelEnergy(rightBuffer, 0) * 10.0);

    const AudioBuffer quiet =
        backend.renderPlan(manualPlan(config, {testTrack("a", {one}, 0.5f)}, 4000), TemporalIR{},
                           diagnostics);
    const double ratio = std::sqrt(energy(quiet) / energy(single));
    MPL_CHECK_NEAR(ratio, 0.5, 0.02);
}

MPL_TEST(AudioAdsrShapesAmplitude) {
    AudioRenderConfig config{8000, 64, 1, 0.0};
    TrackPlan track = testTrack("adsr", {plannedNote(57, 0, 8000)}, 1.0f);
    track.instrument.envelope = AdsrEnvelope{0.5, 0.1, 0.4, 0.2};
    AudioExecutionPlan plan = manualPlan(config, {track}, 0);
    plan.totalFrames = 8000 + static_cast<int>(0.2 * 8000);
    plan.renderedSeconds = plan.totalFrames / 8000.0;
    ReferenceAudioBackend backend(config);
    std::vector<Diagnostic> diagnostics;
    const AudioBuffer buffer = backend.renderPlan(plan, TemporalIR{}, diagnostics);

    double early = 0.0;
    double sustain = 0.0;
    double late = 0.0;
    for (std::size_t frame = 0; frame < 400; ++frame) {
        early += std::fabs(buffer.samples[frame]);
    }
    for (std::size_t frame = 6400; frame < 7200; ++frame) {
        sustain += std::fabs(buffer.samples[frame]);
    }
    for (std::size_t frame = 9300; frame < 9500; ++frame) {
        late += std::fabs(buffer.samples[frame]);
    }
    MPL_CHECK(early < sustain);
    MPL_CHECK(late < sustain * 0.05);
}

MPL_TEST(AudioAutomationIsBlockSizeIndependent) {
    TemporalIR temporal;
    const TemporalId volume =
        temporal.linear(0.0, 1.0, TimeDomain::Musical, Rational(8, 1), "volume");
    // TimeDomain is declared in core/TimeTypes via TemporalIR.

    const AudioRenderConfig base{4000, 64, 1, 0.0};
    const auto buildPlan = [&](int blockSize) {
        AudioRenderConfig config = base;
        config.blockSize = blockSize;
        TrackPlan track = testTrack("long", {plannedNote(57, 0, 8 * 2 * 4000)}, 1.0f);
        ControlBindingPlan control;
        control.parameter = "volume";
        control.curve = volume;
        control.mode = AutomationMode::Replace;
        control.unit = ParameterUnit::Ratio;
        control.minimum = 0.0;
        control.maximum = 1.0;
        track.controls.push_back(control);
        AudioExecutionPlan plan = manualPlan(config, {track}, 8 * 2 * 4000);
        for (TempoSegment& segment : plan.tempo) {
            segment.bpm = 120.0;
        }
        return plan;
    };

    ReferenceAudioBackend backend64(base);
    std::vector<Diagnostic> diagnostics;
    const AudioBuffer small = backend64.renderPlan(buildPlan(64), temporal, diagnostics);
    const AudioBuffer large = backend64.renderPlan(buildPlan(1024), temporal, diagnostics);
    MPL_CHECK_EQ(small.samples.size(), large.samples.size());
    double maximumDifference = 0.0;
    for (std::size_t index = 0; index < small.samples.size(); ++index) {
        maximumDifference =
            std::max(maximumDifference,
                     std::fabs(static_cast<double>(small.samples[index] - large.samples[index])));
    }
    MPL_CHECK(maximumDifference < 1e-6);
}

MPL_TEST(AudioBpmChangesEventPositions) {
    const Compilation fast = compileProject(oneNoteProject(120.0));
    const Compilation slow = compileProject(oneNoteProject(240.0));
    const AudioRenderConfig config{8000, 128, 2, 0.0};
    const ReferenceAssetLibrary resolver;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan fastPlan = buildAudioExecutionPlan(
        fast.production, fast.temporal, resolver, config, diagnostics);
    const AudioExecutionPlan slowPlan = buildAudioExecutionPlan(
        slow.production, slow.temporal, resolver, config, diagnostics);
    MPL_CHECK_EQ(fastPlan.tracks.front().notes.front().startSample, 0);
    MPL_CHECK_EQ(fastPlan.tracks.front().notes.front().endSample, 4000);
    MPL_CHECK_EQ(slowPlan.tracks.front().notes.front().endSample, 2000);
}

MPL_TEST(AudioMultitrackMixingIsAdditive) {
    const AudioRenderConfig config{8000, 128, 2, 0.0};
    ReferenceAudioBackend backend(config);
    std::vector<Diagnostic> diagnostics;

    const AudioBuffer a = backend.renderPlan(
        manualPlan(config, {testTrack("a", {plannedNote(57, 0, 4000)})}, 4000), TemporalIR{},
        diagnostics);
    const AudioBuffer b = backend.renderPlan(
        manualPlan(config, {testTrack("b", {plannedNote(64, 0, 4000)})}, 4000), TemporalIR{},
        diagnostics);
    const AudioBuffer mix = backend.renderPlan(
        manualPlan(config,
                   {testTrack("a", {plannedNote(57, 0, 4000)}),
                    testTrack("b", {plannedNote(64, 0, 4000)})},
                   4000),
        TemporalIR{}, diagnostics);

    for (std::size_t index = 0; index < mix.samples.size(); ++index) {
        MPL_CHECK_NEAR(mix.samples[index], a.samples[index] + b.samples[index], 1e-6);
    }
}

MPL_TEST(AudioAssetSubstitutionKeepsStructure) {
    const Compilation compilation = compileProject(oneNoteProject(120.0));
    const AudioRenderConfig config{8000, 128, 1, 0.0};
    std::vector<Diagnostic> diagnostics;

    const FixedResolver sine(Oscillator::Sine);
    const FixedResolver saw(Oscillator::Saw);
    const AudioExecutionPlan sinePlan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, sine, config, diagnostics);
    const AudioExecutionPlan sawPlan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, saw, config, diagnostics);
    MPL_CHECK_EQ(sinePlan.tracks.front().notes.front().startSample,
                 sawPlan.tracks.front().notes.front().startSample);
    MPL_CHECK_EQ(sinePlan.tracks.front().notes.front().endSample,
                 sawPlan.tracks.front().notes.front().endSample);
    MPL_CHECK_EQ(sinePlan.tracks.front().instrument.version, std::string("fixed"));

    const AudioBuffer sineBuffer =
        ReferenceAudioBackend(config).renderPlan(sinePlan, compilation.temporal, diagnostics);
    const AudioBuffer sawBuffer =
        ReferenceAudioBackend(config).renderPlan(sawPlan, compilation.temporal, diagnostics);
    MPL_CHECK(sineBuffer.samples != sawBuffer.samples);
}

MPL_TEST(AudioMissingAssetAndCapabilityDiagnostics) {
    const Compilation compilation = compileProject(oneNoteProject(120.0));
    const AudioRenderConfig config{8000, 128, 1, 0.0};
    std::vector<Diagnostic> diagnostics;
    const FixedResolver failing(Oscillator::Sine, true);
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, failing, config, diagnostics);
    MPL_CHECK(plan.tracks.empty());
    bool missing = false;
    for (const Diagnostic& diagnostic : diagnostics) {
        missing = missing || diagnostic.code == "audio-missing-asset";
    }
    MPL_CHECK(missing);

    ProductionIR production = compilation.production;
    TemporalIR temporal;
    const TemporalId curve = temporal.constant(0.5, TimeDomain::Musical, Rational(1, 1));
    production.automation.push_back(AutomationBinding{
        "modulation", makeParameterTarget(ParameterScope::Track, "Piano", "", "modulation",
                                          ParameterUnit::Ratio),
        curve, TimeDomain::Musical, AutomationMode::Replace, true});
    production.automation.push_back(AutomationBinding{
        "clip", makeParameterTarget(ParameterScope::Clip, "Piano", "A", "volume",
                                    ParameterUnit::Ratio),
        curve, TimeDomain::Musical, AutomationMode::Replace, true});
    diagnostics.clear();
    const AudioExecutionPlan supported = buildAudioExecutionPlan(
        production, temporal, ReferenceAssetLibrary{}, config, diagnostics);
    MPL_CHECK_EQ(supported.tracks.size(), std::size_t{1});
    MPL_CHECK_EQ(supported.tracks.front().controls.size(), std::size_t{0});
    bool unsupportedParameter = false;
    bool unsupportedAutomation = false;
    for (const Diagnostic& diagnostic : diagnostics) {
        unsupportedParameter = unsupportedParameter || diagnostic.code == "audio-unsupported-parameter";
        unsupportedAutomation =
            unsupportedAutomation || diagnostic.code == "audio-unsupported-automation";
    }
    MPL_CHECK(unsupportedParameter);
    MPL_CHECK(unsupportedAutomation);
}

MPL_TEST(AudioWavSamplePlayback) {
    AudioBuffer sample;
    sample.sampleRate = 8000;
    sample.channels = 1;
    sample.samples.assign(4800, 0.5f);
    std::vector<Diagnostic> diagnostics;
    const std::filesystem::path samplePath =
        std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "wav-playback.wav";
    writeWavFile(samplePath, sample, diagnostics);
    MPL_CHECK(diagnostics.empty());

    ProductionIR production;
    production.tempoMap.push_back(TempoEvent{Position{}, 120.0});
    production.meterMap.push_back(MeterEvent{Position{}, Meter{4, 4}});

    ClipIR clip;
    clip.name = "Sample";
    clip.content = Fragment::fromMelody({{"C4"_p, Rational(1, 4)}});
    TrackIR track;
    track.name = "Sampler";
    track.instrument = "Sample";
    track.assetBindings.push_back(
        AssetBinding{"main", "wav-playback.wav", "file"});
    track.clips.push_back(clip);
    production.tracks.push_back(track);

    const AudioRenderConfig config{8000, 128, 1, 0.0};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_TEST_OUTPUT_DIR));
    diagnostics.clear();
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        production, TemporalIR{}, resolver, config, diagnostics);
    MPL_CHECK(diagnostics.empty());
    MPL_CHECK_EQ(plan.tracks.size(), std::size_t{1});
    MPL_CHECK(plan.tracks.front().instrument.sample != nullptr);
    MPL_CHECK_EQ(plan.tracks.front().instrument.version, std::string("file"));

    const AudioBuffer rendered =
        ReferenceAudioBackend(config).renderPlan(plan, TemporalIR{}, diagnostics);
    MPL_CHECK(peak(rendered) > 0.1);
}

MPL_TEST(AudioWavRoundTrip) {
    AudioBuffer buffer;
    buffer.sampleRate = 8000;
    buffer.channels = 2;
    buffer.samples = {0.0f, 0.1f, -0.2f, 0.3f, 0.5f, -0.5f};
    std::vector<Diagnostic> diagnostics;
    const std::filesystem::path path =
        std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "wav-roundtrip.wav";
    writeWavFile(path, buffer, diagnostics);
    MPL_CHECK(diagnostics.empty());

    std::vector<Diagnostic> readDiagnostics;
    const std::optional<AudioBuffer> loaded = loadWavFile(path, readDiagnostics);
    MPL_CHECK(loaded.has_value());
    MPL_CHECK(readDiagnostics.empty());
    MPL_CHECK_EQ(loaded->sampleRate, 8000);
    MPL_CHECK_EQ(loaded->channels, 2);
    MPL_CHECK_EQ(loaded->samples.size(), buffer.samples.size());
    for (std::size_t index = 0; index < buffer.samples.size(); ++index) {
        MPL_CHECK_NEAR(loaded->samples[index], buffer.samples[index], 1e-6);
    }

    std::vector<Diagnostic> missingDiagnostics;
    MPL_CHECK(!loadWavFile(std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "missing.wav",
                           missingDiagnostics)
                   .has_value());
    MPL_CHECK(!missingDiagnostics.empty());
}

MPL_TEST(AudioExampleRendersAndGoldenStats) {
    FrontendResult frontend = compileExample("audio.mpl");
    MPL_CHECK(frontend.diagnostics.empty());
    MPL_CHECK(frontend.ok());
    MPL_CHECK_EQ(frontend.program->exports.size(), std::size_t{2});

    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));
    MPL_CHECK(!compilation.hasErrors());
    const AudioRenderConfig config{8000, 128, 2, 0.1};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_EXAMPLES_DIR));
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, resolver, config, diagnostics);
    MPL_CHECK(!plan.tracks.empty());
    const AudioBuffer buffer = ReferenceAudioBackend(config).renderPlan(
        plan, compilation.temporal, diagnostics);
    MPL_CHECK(!buffer.samples.empty());
    MPL_CHECK(peak(buffer) > 0.01);

    const std::filesystem::path wavPath =
        std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "audio-example.wav";
    ReferenceAudioBackend(config).writeWav(buffer, wavPath, diagnostics);
    MPL_CHECK(std::filesystem::exists(wavPath));
    checkGolden(audioStatsJson(buffer), "audio-reference.json");
}
