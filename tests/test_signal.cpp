#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/audio/AudioExecution.hpp"
#include "mpl/audio/Processors.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/music/Pitch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace mpl;
using namespace mpl::literals;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string diagnosticText(const FrontendResult& result) {
    DiagnosticSink sink;
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        sink.add(diagnostic);
    }
    return sink.toHuman();
}

bool hasCode(const std::vector<Diagnostic>& diagnostics, const std::string& code) {
    for (const Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasFrontendCode(const FrontendResult& result, const std::string& code) {
    for (const FrontendDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

Compilation compileExample(const std::string& name) {
    const std::filesystem::path path = std::filesystem::path(MPL_EXAMPLES_DIR) / name;
    FrontendResult result = compileSource(readText(path), path.string());
    if (!result.ok()) {
        throw ::mpltest::AssertionFailure(name + " failed:\n" + diagnosticText(result));
    }
    const Compiler compiler;
    return compiler.compile(std::move(result.program->project));
}

std::string mixProgram(const std::string& declarations, const std::string& tracks,
                       const std::string& afterExports = {}) {
    return "project T {\n tempo = 120 bpm; meter = 4/4; key = C major;\n}\n"
           "asset PianoAsset = instrument(\"GrandPiano\");\n"
           "asset DrumKit = sample(\"DrumKit\");\n"
           "let A = melody { C4: 1/4 };\n" +
           declarations + tracks +
           "section S(1 bars) { clip Piano { play A; } }\n"
           "arrangement { play S; }\n"
           "export midi(\"x.mid\");\n" +
           afterExports;
}

std::string defaultTracks() {
    return "track Piano { asset = PianoAsset; role = melodic; }\n"
           "track Drums { asset = DrumKit; role = percussion; }\n";
}

AudioExecutionPlan buildPlan(const ProductionIR& production, const TemporalIR& temporal,
                             const AudioRenderConfig& config,
                             std::vector<Diagnostic>& diagnostics) {
    const ReferenceAssetLibrary resolver;
    return buildAudioExecutionPlan(production, temporal, resolver, config, diagnostics);
}

int addNode(SignalGraphPlan& graph, SignalNodeKind kind, std::string name, int inputChannels,
            int outputChannels) {
    SignalNodePlan node;
    node.id = static_cast<int>(graph.nodes.size());
    node.kind = kind;
    node.name = std::move(name);
    node.inputChannels = inputChannels;
    node.outputChannels = outputChannels;
    graph.nodes.push_back(std::move(node));
    return static_cast<int>(graph.nodes.size() - 1);
}

void connectNode(SignalGraphPlan& graph, int from, int to, double gain = 1.0,
                 bool sidechain = false) {
    SignalConnectionPlan connection;
    connection.from = from;
    connection.to = to;
    connection.gain = gain;
    connection.sidechain = sidechain;
    graph.connections.push_back(connection);
}

const SignalNodePlan* findNode(const SignalGraphPlan& graph, const std::string& name) {
    for (const SignalNodePlan& node : graph.nodes) {
        if (node.name == name) {
            return &node;
        }
    }
    return nullptr;
}

bool hasConnection(const SignalGraphPlan& graph, int from, int to, bool sidechain, double gain) {
    for (const SignalConnectionPlan& connection : graph.connections) {
        if (connection.from == from && connection.to == to &&
            connection.sidechain == sidechain &&
            std::fabs(connection.gain - gain) < 1e-9) {
            return true;
        }
    }
    return false;
}

std::vector<float> sineBuffer(int frames, int channels, double frequency, double amplitude,
                              int sampleRate) {
    std::vector<float> buffer(static_cast<std::size_t>(frames) * channels);
    for (int frame = 0; frame < frames; ++frame) {
        const double value =
            amplitude * std::sin(2.0 * kPi * frequency * static_cast<double>(frame) / sampleRate);
        for (int channel = 0; channel < channels; ++channel) {
            buffer[static_cast<std::size_t>(frame) * channels + channel] =
                static_cast<float>(value);
        }
    }
    return buffer;
}

void processBuffer(AudioProcessor& processor, std::vector<float>& buffer, int channels,
                   const ParameterRamp& ramp, int blockSize,
                   const std::vector<float>& sidechain) {
    const std::size_t stride = static_cast<std::size_t>(blockSize) * channels;
    for (std::size_t offset = 0; offset < buffer.size(); offset += stride) {
        const int frames = static_cast<int>(
            std::min<std::size_t>(blockSize, (buffer.size() - offset) / channels));
        processor.process(buffer.data() + offset, frames, channels, ramp,
                          sidechain.empty() ? nullptr : sidechain.data() + offset);
    }
}

double rms(const std::vector<float>& buffer, int channels, std::size_t startFrame = 0) {
    double total = 0.0;
    std::size_t count = 0;
    for (std::size_t index = startFrame * static_cast<std::size_t>(channels);
         index < buffer.size(); ++index) {
        total += static_cast<double>(buffer[index]) * buffer[index];
        ++count;
    }
    return count == 0 ? 0.0 : std::sqrt(total / static_cast<double>(count));
}

double energy(const std::vector<float>& buffer, std::size_t start, std::size_t end) {
    double total = 0.0;
    for (std::size_t index = start; index < end && index < buffer.size(); ++index) {
        total += static_cast<double>(buffer[index]) * buffer[index];
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

double bufferEnergy(const AudioBuffer& buffer) {
    double total = 0.0;
    for (const float sample : buffer.samples) {
        total += static_cast<double>(sample) * sample;
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
    const double bufferRms =
        buffer.samples.empty()
            ? 0.0
            : std::sqrt(bufferEnergy(buffer) / static_cast<double>(buffer.samples.size()));
    std::ostringstream hash;
    hash << std::hex << fnv1a(buffer);
    std::ostringstream out;
    out << "{\n";
    out << "  \"sampleRate\": " << buffer.sampleRate << ",\n";
    out << "  \"channels\": " << buffer.channels << ",\n";
    out << "  \"frames\": " << buffer.frameCount() << ",\n";
    out << "  \"peak\": " << peak(buffer) << ",\n";
    out << "  \"rms\": " << bufferRms << ",\n";
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
    std::filesystem::path outputDirectory(MPL_TEST_OUTPUT_DIR);
    std::filesystem::create_directories(outputDirectory);
    const std::filesystem::path actualPath = outputDirectory / (name + ".actual");
    std::ofstream(actualPath, std::ios::binary) << actual;
    throw ::mpltest::AssertionFailure("golden mismatch: " + golden.string() +
                                      " (actual written to " + actualPath.string() + ")");
}

}  // namespace

MPL_TEST(SignalGraphBuildsMixTopology) {
    const Compilation compilation = compileExample("production.mpl");
    MPL_CHECK(!compilation.hasErrors());
    const AudioRenderConfig config{8000, 256, 2, 0.25};
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan =
        buildPlan(compilation.production, compilation.temporal, config, diagnostics);
    MPL_CHECK(!hasCode(diagnostics, "graph-cycle"));
    MPL_CHECK_EQ(plan.tracks.size(), std::size_t{5});

    const SignalGraphPlan& graph = plan.graph;
    const SignalNodePlan* pianoEq = findNode(graph, "Piano / EqPiano");
    const SignalNodePlan* bassComp = findNode(graph, "Bass / BassComp");
    const SignalNodePlan* leadFilter = findNode(graph, "Lead / LeadFilter");
    const SignalNodePlan* masterComp = findNode(graph, "master / MasterComp");
    const SignalNodePlan* reverbMixer = findNode(graph, "Reverb Mixer");
    const SignalNodePlan* pianoPan = findNode(graph, "Piano Pan");
    MPL_CHECK(pianoEq != nullptr);
    MPL_CHECK(bassComp != nullptr);
    MPL_CHECK(leadFilter != nullptr);
    MPL_CHECK(masterComp != nullptr);
    MPL_CHECK(reverbMixer != nullptr);
    MPL_CHECK(pianoPan != nullptr);
    MPL_CHECK(bassComp->hasSidechainInput);

    const SignalNodePlan* pianoSource = findNode(graph, "Piano Source");
    const SignalNodePlan* drumsSource = findNode(graph, "Drums Source");
    MPL_CHECK(pianoSource != nullptr);
    MPL_CHECK(drumsSource != nullptr);
    MPL_CHECK(hasConnection(graph, pianoPan->id, reverbMixer->id, false, 0.3));
    MPL_CHECK(hasConnection(graph, drumsSource->id, bassComp->id, true, 1.0));
    MPL_CHECK(hasConnection(graph, pianoSource->id, pianoEq->id, false, 1.0));

    MPL_CHECK_EQ(graph.order.size(), graph.nodes.size());
    SignalGraphPlan copy = graph;
    std::vector<Diagnostic> validation;
    MPL_CHECK(validateSignalGraph(copy, validation));
}

MPL_TEST(SignalGraphRejectsIllegalTopology) {
    {
        SignalGraphPlan graph;
        const int source = addNode(graph, SignalNodeKind::Source, "s", 0, 2);
        const int first = addNode(graph, SignalNodeKind::Processor, "a", 2, 2);
        const int second = addNode(graph, SignalNodeKind::Processor, "b", 2, 2);
        const int output = addNode(graph, SignalNodeKind::Output, "out", 2, 0);
        connectNode(graph, source, first);
        connectNode(graph, first, second);
        connectNode(graph, second, first);
        connectNode(graph, second, output);
        std::vector<Diagnostic> diagnostics;
        MPL_CHECK(!validateSignalGraph(graph, diagnostics));
        MPL_CHECK(hasCode(diagnostics, "graph-cycle"));
    }
    {
        SignalGraphPlan graph;
        const int source = addNode(graph, SignalNodeKind::Source, "s", 0, 2);
        const int processor = addNode(graph, SignalNodeKind::Processor, "mono", 1, 1);
        const int output = addNode(graph, SignalNodeKind::Output, "out", 1, 0);
        connectNode(graph, source, processor);
        connectNode(graph, processor, output);
        std::vector<Diagnostic> diagnostics;
        MPL_CHECK(!validateSignalGraph(graph, diagnostics));
        MPL_CHECK(hasCode(diagnostics, "graph-channel-mismatch"));
    }
    {
        SignalGraphPlan graph;
        const int source = addNode(graph, SignalNodeKind::Source, "s", 0, 2);
        const int processor = addNode(graph, SignalNodeKind::Processor, "p", 2, 2);
        const int output = addNode(graph, SignalNodeKind::Output, "out", 2, 0);
        connectNode(graph, source, processor);
        connectNode(graph, processor, source);
        connectNode(graph, processor, output);
        std::vector<Diagnostic> diagnostics;
        MPL_CHECK(!validateSignalGraph(graph, diagnostics));
        MPL_CHECK(hasCode(diagnostics, "graph-source-input"));
    }
    {
        SignalGraphPlan graph;
        const int source = addNode(graph, SignalNodeKind::Source, "s", 0, 2);
        const int key = addNode(graph, SignalNodeKind::Source, "key", 0, 2);
        const int processor = addNode(graph, SignalNodeKind::Processor, "comp", 2, 2);
        const int output = addNode(graph, SignalNodeKind::Output, "out", 2, 0);
        connectNode(graph, source, processor);
        connectNode(graph, key, processor, 1.0, true);
        connectNode(graph, key, processor, 1.0, true);
        connectNode(graph, processor, output);
        std::vector<Diagnostic> diagnostics;
        MPL_CHECK(!validateSignalGraph(graph, diagnostics));
        MPL_CHECK(hasCode(diagnostics, "graph-sidechain"));
    }
    {
        SignalGraphPlan graph;
        const int source = addNode(graph, SignalNodeKind::Source, "s", 0, 2);
        const int processor = addNode(graph, SignalNodeKind::Processor, "p", 2, 2);
        connectNode(graph, source, processor);
        std::vector<Diagnostic> diagnostics;
        MPL_CHECK(!validateSignalGraph(graph, diagnostics));
        MPL_CHECK(hasCode(diagnostics, "graph-output"));
    }
}

MPL_TEST(SignalGraphCapabilityPolicies) {
    SignalGraphPlan graph;
    const int source = addNode(graph, SignalNodeKind::Source, "s", 0, 2);
    const int reverb = addNode(graph, SignalNodeKind::Processor, "rv", 2, 2);
    graph.nodes[static_cast<std::size_t>(reverb)].processor.kind = ProcessorKind::Reverb;
    const int output = addNode(graph, SignalNodeKind::Output, "out", 2, 0);
    connectNode(graph, source, reverb);
    connectNode(graph, reverb, output);

    BackendCapabilities capabilities;
    capabilities.processors = {ProcessorKind::Gain};
    capabilities.maxChannels = 1;

    std::vector<Diagnostic> strict;
    MPL_CHECK(!checkBackendCapabilities(capabilities, graph, UnsupportedPolicy::Strict, strict));
    MPL_CHECK(hasCode(strict, "audio-unsupported-processor"));
    MPL_CHECK(hasCode(strict, "audio-unsupported-channels"));

    std::vector<Diagnostic> approximate;
    MPL_CHECK(checkBackendCapabilities(capabilities, graph, UnsupportedPolicy::Approximate,
                                       approximate));
    MPL_CHECK(hasCode(approximate, "audio-approximated-processor"));

    std::vector<Diagnostic> ignore;
    MPL_CHECK(checkBackendCapabilities(capabilities, graph, UnsupportedPolicy::Ignore, ignore));
    MPL_CHECK(hasCode(ignore, "audio-ignored-processor"));

    capabilities.processors = {ProcessorKind::Gain, ProcessorKind::Reverb};
    capabilities.maxChannels = 2;
    std::vector<Diagnostic> supported;
    MPL_CHECK(checkBackendCapabilities(capabilities, graph, UnsupportedPolicy::Strict, supported));
    MPL_CHECK(supported.empty());
}

MPL_TEST(DspGainAndPan) {
    std::vector<float> gainBuffer{0.5f, 0.5f};
    auto gain = createProcessor(ProcessorKind::Gain, "");
    ParameterRamp gainRamp;
    gainRamp.setConstant("gain", -6.0);
    gain->prepare(8000, 2);
    processBuffer(*gain, gainBuffer, 2, gainRamp, 2, {});
    MPL_CHECK_NEAR(gainBuffer[0], 0.5 * std::pow(10.0, -6.0 / 20.0), 1e-6);

    std::vector<float> leftBuffer{1.0f, 1.0f};
    auto panLeft = createProcessor(ProcessorKind::Pan, "");
    ParameterRamp leftRamp;
    leftRamp.setConstant("pan", -1.0);
    panLeft->prepare(8000, 2);
    processBuffer(*panLeft, leftBuffer, 2, leftRamp, 2, {});
    MPL_CHECK_NEAR(leftBuffer[0], 1.0, 1e-6);
    MPL_CHECK_NEAR(leftBuffer[1], 0.0, 1e-6);

    std::vector<float> rightBuffer{1.0f, 1.0f};
    auto panRight = createProcessor(ProcessorKind::Pan, "");
    ParameterRamp rightRamp;
    rightRamp.setConstant("pan", 1.0);
    panRight->prepare(8000, 2);
    processBuffer(*panRight, rightBuffer, 2, rightRamp, 2, {});
    MPL_CHECK_NEAR(rightBuffer[0], 0.0, 1e-6);
    MPL_CHECK_NEAR(rightBuffer[1], 1.0, 1e-6);
}

MPL_TEST(DspEqFrequencyResponse) {
    const int sampleRate = 8000;
    const int frames = sampleRate;
    const auto measure = [&](const std::string& mode, double frequency, double gainDb,
                             double inputFrequency) {
        std::vector<float> buffer = sineBuffer(frames, 1, inputFrequency, 0.5, sampleRate);
        auto eq = createProcessor(ProcessorKind::Biquad, mode);
        ParameterRamp ramp;
        ramp.setConstant("frequency", frequency);
        ramp.setConstant("q", 1.0);
        ramp.setConstant("gain", gainDb);
        eq->prepare(sampleRate, 1);
        processBuffer(*eq, buffer, 1, ramp, 64, {});
        return rms(buffer, 1, static_cast<std::size_t>(sampleRate / 2));
    };

    const double passed = measure("lowpass", 800.0, 0.0, 200.0);
    const double rejected = measure("lowpass", 800.0, 0.0, 4000.0);
    MPL_CHECK(passed > rejected * 5.0);

    const double boosted = measure("peaking", 1000.0, 6.0, 1000.0);
    const double flat = measure("peaking", 1000.0, 0.0, 1000.0);
    MPL_CHECK_NEAR(boosted / flat, 2.0, 0.25);
}

MPL_TEST(DspCompressorDynamicAndSidechain) {
    const int sampleRate = 8000;
    const int frames = sampleRate;
    const auto measure = [&](double amplitude, const std::vector<float>& sidechain) {
        std::vector<float> buffer = sineBuffer(frames, 1, 440.0, amplitude, sampleRate);
        auto compressor = createProcessor(ProcessorKind::Compressor, "");
        ParameterRamp ramp;
        ramp.setConstant("threshold", -18.0);
        ramp.setConstant("ratio", 4.0);
        ramp.setConstant("attack", 0.005);
        ramp.setConstant("release", 0.1);
        ramp.setConstant("makeup", 0.0);
        compressor->prepare(sampleRate, 1);
        processBuffer(*compressor, buffer, 1, ramp, 64, sidechain);
        return rms(buffer, 1, static_cast<std::size_t>(sampleRate / 2));
    };

    const double loud = measure(0.5, {});
    const double quiet = measure(0.05, {});
    MPL_CHECK(loud < 0.2);
    MPL_CHECK(loud > 0.08);
    MPL_CHECK_NEAR(quiet, 0.05 / std::sqrt(2.0), 0.01);

    const std::vector<float> key = sineBuffer(frames, 1, 440.0, 0.5, sampleRate);
    const double ducked = measure(0.05, key);
    MPL_CHECK(ducked < quiet * 0.7);
}

MPL_TEST(DspDelayImpulsePosition) {
    const int sampleRate = 8000;
    std::vector<float> buffer(400, 0.0f);
    buffer[0] = 1.0f;
    auto delay = createProcessor(ProcessorKind::Delay, "");
    ParameterRamp ramp;
    ramp.setConstant("time", 0.01);
    ramp.setConstant("feedback", 0.0);
    ramp.setConstant("mix", 0.5);
    delay->prepare(sampleRate, 1);
    processBuffer(*delay, buffer, 1, ramp, 64, {});
    MPL_CHECK_NEAR(buffer[0], 0.5, 1e-6);
    MPL_CHECK_NEAR(buffer[40], 0.0, 1e-6);
    MPL_CHECK_NEAR(buffer[80], 0.5, 1e-4);
    MPL_CHECK_NEAR(buffer[81], 0.0, 1e-4);
}

MPL_TEST(DspReverbTailAndReset) {
    const int sampleRate = 8000;
    const int frames = sampleRate;
    std::vector<float> buffer(frames, 0.0f);
    buffer[0] = 1.0f;
    auto reverb = createProcessor(ProcessorKind::Reverb, "");
    ParameterRamp ramp;
    ramp.setConstant("room", 0.8);
    ramp.setConstant("damping", 0.3);
    ramp.setConstant("mix", 0.8);
    reverb->prepare(sampleRate, 1);
    processBuffer(*reverb, buffer, 1, ramp, 64, {});

    const double early = energy(buffer, static_cast<std::size_t>(0.03 * sampleRate),
                                static_cast<std::size_t>(0.2 * sampleRate));
    const double late = energy(buffer, static_cast<std::size_t>(0.7 * sampleRate),
                               static_cast<std::size_t>(0.95 * sampleRate));
    MPL_CHECK(early > 1e-8);
    MPL_CHECK(late < early);
    MPL_CHECK(reverb->tailSeconds() > 0.0);

    reverb->reset();
    std::vector<float> silence(256, 0.0f);
    processBuffer(*reverb, silence, 1, ramp, 64, {});
    for (const float sample : silence) {
        MPL_CHECK_NEAR(sample, 0.0, 1e-9);
    }
}

MPL_TEST(MixLanguageLowersStripsAndAutomation) {
    const Compilation compilation = compileExample("production.mpl");
    const ProductionIR& production = compilation.production;
    MPL_CHECK_EQ(production.mix.tracks.size(), std::size_t{5});
    MPL_CHECK_EQ(production.mix.buses.size(), std::size_t{1});
    MPL_CHECK_EQ(production.mix.processors.size(), std::size_t{5});
    MPL_CHECK_EQ(production.mix.master.inserts.size(), std::size_t{1});
    MPL_CHECK_EQ(production.mix.processorAutomation.size(), std::size_t{4});
    MPL_CHECK(production.logicalExtent.inWholeNotes() == Rational(10, 1));

    const ChannelStripSpec* piano = production.mix.findTrack("Piano");
    MPL_CHECK(piano != nullptr);
    MPL_CHECK_EQ(piano->inserts.size(), std::size_t{1});
    MPL_CHECK_EQ(piano->sends.size(), std::size_t{1});
    MPL_CHECK_EQ(piano->sends.front().bus, std::string("Reverb"));
    MPL_CHECK_NEAR(piano->sends.front().gain, 0.3, 1e-9);

    const BusSpec* reverb = production.mix.findBus("Reverb");
    MPL_CHECK(reverb != nullptr);
    MPL_CHECK_EQ(reverb->strip.inserts.size(), std::size_t{1});

    const ChannelStripSpec* bass = production.mix.findTrack("Bass");
    MPL_CHECK(bass != nullptr);
    MPL_CHECK_EQ(bass->sidechains.size(), std::size_t{1});
    MPL_CHECK_EQ(bass->sidechains.front().source, std::string("Drums"));

    const ProcessorSpec* eq = production.mix.findProcessor("EqPiano");
    MPL_CHECK(eq != nullptr);
    MPL_CHECK(eq->kind == ProcessorKind::Biquad);
    MPL_CHECK_EQ(eq->mode, std::string("peaking"));
    MPL_CHECK_NEAR(eq->parameters.at("frequency"), 1200.0, 1e-9);
    MPL_CHECK_NEAR(eq->parameters.at("gain"), 4.0, 1e-9);
}

MPL_TEST(MixLanguageDiagnostics) {
    {
        const FrontendResult result = compileSource(
            mixProgram("processor X = flanger { };\n", defaultTracks()), "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-processor"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("processor X = biquad { frequency = 1000 Hz; };\n", defaultTracks()),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-processor-mode"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("processor G = gain { amount = 3 dB; };\n", defaultTracks()), "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-processor-parameter"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("processor C = compressor { threshold = 10; };\n", defaultTracks()),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-mismatch"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("processor G = gain { gain = 40 dB; };\n", defaultTracks()), "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-parameter-range"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("", "track Piano { asset = PianoAsset; role = melodic; insert Missing; }\n"
                           "track Drums { asset = DrumKit; role = percussion; }\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-unknown-name"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("bus Reverb { }\n",
                       "track Piano { asset = PianoAsset; role = melodic; send 2.0 to Reverb; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-parameter-range"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("processor Eq = biquad { mode = peaking; };\n",
                       "track Piano { asset = PianoAsset; role = melodic; insert Eq; "
                       "sidechain Eq from Drums; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-sidechain"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("processor Comp = compressor { };\n",
                       "track Piano { asset = PianoAsset; role = melodic; "
                       "sidechain Comp from Drums; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-unknown-insert"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("curve C on musical for 1 bars = linear(100, 1000);\n"
                       "processor Eq = biquad { mode = peaking; frequency = 1000 Hz; };\n",
                       "track Piano { asset = PianoAsset; role = melodic; insert Eq; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n",
                       "automation Piano.Missing.frequency = C;\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-unknown-name"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("curve C on musical for 1 bars = linear(100, 1000);\n"
                       "processor Eq = biquad { mode = peaking; frequency = 1000 Hz; };\n",
                       "track Piano { asset = PianoAsset; role = melodic; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n",
                       "automation Piano.Eq.frequency = C;\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-unknown-insert"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("curve C on musical for 1 bars = linear(1, 10);\n"
                       "processor Eq = biquad { mode = peaking; frequency = 1000 Hz; };\n",
                       "track Piano { asset = PianoAsset; role = melodic; insert Eq; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n",
                       "automation Piano.Eq.frequency = C;\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "sema-parameter-range"));
    }
    {
        const FrontendResult result = compileSource(
            mixProgram("curve C on relative = linear(0.2, 0.8);\n"
                       "processor Eq = biquad { mode = peaking; frequency = 1000 Hz; };\n",
                       "track Piano { asset = PianoAsset; role = melodic; insert Eq; }\n"
                       "track Drums { asset = DrumKit; role = percussion; }\n",
                       "automation Piano.Eq.frequency = C;\n"),
            "<test>");
        MPL_CHECK(hasFrontendCode(result, "type-domain"));
    }
}

MPL_TEST(BusSendAndReturnAffectsOutput) {
    const AudioRenderConfig config{8000, 256, 2, 0.0};
    const auto render = [&](double send) {
        Project project;
        project.setTempo(120.0);
        project.setMeter(4, 4);
        const ObjectId part =
            project.music().melody({{"C4"_p, Rational(1, 4)}}, "A");
        project.addTrack("Piano", "GrandPiano", part);
        project.mix().processors.push_back(
            ProcessorSpec{"Hall", ProcessorKind::Reverb, "", {{"room", 0.7},
                                                             {"damping", 0.3},
                                                             {"mix", 0.8}}});
        BusSpec bus;
        bus.strip.name = "Reverb";
        bus.strip.inserts = {"Hall"};
        project.mix().buses.push_back(bus);
        ChannelStripSpec piano;
        piano.name = "Piano";
        piano.sends.push_back(SendSpec{"Reverb", send});
        project.mix().tracks.push_back(piano);

        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(!compilation.hasErrors());
        std::vector<Diagnostic> diagnostics;
        const AudioExecutionPlan plan =
            buildPlan(compilation.production, compilation.temporal, config, diagnostics);
        const AudioBuffer buffer =
            ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
        return buffer;
    };

    const AudioBuffer dry = render(0.0);
    const AudioBuffer wet = render(1.0);
    MPL_CHECK(bufferEnergy(wet) > bufferEnergy(dry) * 1.2);
}

MPL_TEST(SidechainDucksTheTargetTrack) {
    const AudioRenderConfig config{8000, 256, 2, 0.0};
    const auto build = [&]() {
        Project project;
        project.setTempo(120.0);
        project.setMeter(4, 4);
        Note bassNote;
        bassNote.pitch = "C2"_p;
        bassNote.offset = Duration(Rational(0, 1));
        bassNote.duration = Duration(Rational(1, 2));
        bassNote.velocity = 30;
        const ObjectId bass = project.music().notes({bassNote}, std::nullopt, "BassNote");
        Note keyNote;
        keyNote.pitch = "C4"_p;
        keyNote.offset = Duration(Rational(0, 1));
        keyNote.duration = Duration(Rational(1, 2));
        keyNote.velocity = 127;
        const ObjectId key =
            project.music().notes({keyNote}, std::nullopt, "KeyNote");
        project.addTrack("Bass", "SubBass", bass);
        project.addTrack("SidechainKey", "SubBass", key);
        project.mix().processors.push_back(ProcessorSpec{
            "BassComp", ProcessorKind::Compressor, "",
            {{"threshold", -20.0},
             {"ratio", 12.0},
             {"attack", 0.002},
             {"release", 0.1},
             {"makeup", 0.0}}});
        ChannelStripSpec keyStrip;
        keyStrip.name = "SidechainKey";
        keyStrip.gainDb = -96.0;
        project.mix().tracks.push_back(keyStrip);
        ChannelStripSpec bassStrip;
        bassStrip.name = "Bass";
        bassStrip.inserts = {"BassComp"};
        bassStrip.sidechains.push_back(SidechainSpec{"BassComp", "SidechainKey"});
        project.mix().tracks.push_back(bassStrip);
        return project;
    };

    const auto render = [&](Project project, bool expectSidechain) {
        const Compiler compiler;
        const Compilation compilation = compiler.compile(std::move(project));
        MPL_CHECK(!compilation.hasErrors());
        std::vector<Diagnostic> diagnostics;
        const AudioExecutionPlan plan =
            buildPlan(compilation.production, compilation.temporal, config, diagnostics);
        bool sidechainEdge = false;
        for (const SignalConnectionPlan& connection : plan.graph.connections) {
            sidechainEdge = sidechainEdge || connection.sidechain;
        }
        MPL_CHECK_EQ(sidechainEdge, expectSidechain);
        return ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
    };

    Project withSidechain = build();
    Project withoutSidechain = build();
    MPL_CHECK(withSidechain.mix().findTrack("Bass") != nullptr);
    MPL_CHECK_EQ(withSidechain.mix().findTrack("Bass")->sidechains.size(), std::size_t{1});
    withoutSidechain.mix().tracks.back().sidechains.clear();
    MPL_CHECK_EQ(withoutSidechain.mix().findTrack("Bass")->sidechains.size(), std::size_t{0});
    const AudioBuffer ducked = render(std::move(withSidechain), true);
    const AudioBuffer plain = render(std::move(withoutSidechain), false);
    const double ratio = bufferEnergy(ducked) / bufferEnergy(plain);
    MPL_CHECK_NEAR(ratio, 0.0, 0.5);
}

MPL_TEST(LogicalPaddingSurvivesRender) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(2 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    FrontendResult frontend = compileSource(source, "<test>");
    MPL_CHECK(frontend.ok());
    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));

    const AudioRenderConfig config{8000, 128, 2, 0.0};
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan =
        buildPlan(compilation.production, compilation.temporal, config, diagnostics);
    MPL_CHECK_NEAR(plan.logicalExtentSeconds, 4.0, 1e-9);
    MPL_CHECK_EQ(plan.totalFrames, 32000);

    const AudioBuffer buffer =
        ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
    MPL_CHECK_EQ(static_cast<int>(buffer.frameCount()), 32000);
    for (std::size_t frame = static_cast<std::size_t>(1.5 * 8000);
         frame < buffer.frameCount(); ++frame) {
        MPL_CHECK_NEAR(buffer.samples[frame * 2], 0.0, 1e-6);
    }
}

MPL_TEST(RenderTerminationPolicies) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    FrontendResult frontend = compileSource(source, "<test>");
    MPL_CHECK(frontend.ok());
    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));

    AudioRenderConfig fixedConfig{8000, 128, 2, 2.0};
    fixedConfig.termination = RenderTermination::Fixed;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan fixedPlan =
        buildPlan(compilation.production, compilation.temporal, fixedConfig, diagnostics);
    MPL_CHECK_EQ(fixedPlan.totalFrames, 16000 + 16000);

    AudioRenderConfig trimConfig = fixedConfig;
    trimConfig.termination = RenderTermination::SilenceThreshold;
    std::vector<Diagnostic> trimDiagnostics;
    const AudioExecutionPlan trimPlan =
        buildPlan(compilation.production, compilation.temporal, trimConfig, trimDiagnostics);
    const AudioBuffer trimmed =
        ReferenceAudioBackend(trimConfig).renderPlan(trimPlan, compilation.temporal,
                                                     trimDiagnostics);
    MPL_CHECK(trimmed.frameCount() < static_cast<std::size_t>(fixedPlan.totalFrames));
    MPL_CHECK(trimmed.frameCount() >=
              static_cast<std::size_t>(trimPlan.contentEndSeconds * trimConfig.sampleRate));
}

MPL_TEST(StepAutomationLandsWithinOneBlock) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = notes span 1 bars { C4 @ 0 for 1 bars vel 100 };\n"
        "curve Gate on musical for 1 bars = step(0.0, 1.0, 1/2);\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "automation Piano.volume = Gate;\n"
        "export midi(\"x.mid\");\n";
    FrontendResult frontend = compileSource(source, "<test>");
    MPL_CHECK(frontend.ok());
    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));

    const AudioRenderConfig config{8000, 256, 2, 0.0};
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan =
        buildPlan(compilation.production, compilation.temporal, config, diagnostics);
    const AudioBuffer buffer =
        ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);

    std::size_t firstAudible = buffer.frameCount();
    for (std::size_t frame = 0; frame < buffer.frameCount(); ++frame) {
        if (std::fabs(buffer.samples[frame * 2]) > 0.01) {
            firstAudible = frame;
            break;
        }
    }
    MPL_CHECK(firstAudible >= 7900);
    MPL_CHECK(firstAudible <= 8300);
}

MPL_TEST(MaxTailIsCappedWithDiagnostic) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "processor Hall = reverb { room = 0.8; damping = 0.3; mix = 0.8; };\n"
        "bus Reverb { insert Hall; }\n"
        "track Piano { asset = PianoAsset; role = melodic; send 0.5 to Reverb; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    FrontendResult frontend = compileSource(source, "<test>");
    MPL_CHECK(frontend.ok());
    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));

    AudioRenderConfig config{8000, 128, 2, 0.0};
    config.maxTailSeconds = 0.5;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan =
        buildPlan(compilation.production, compilation.temporal, config, diagnostics);
    MPL_CHECK(hasCode(diagnostics, "audio-tail-capped"));
    MPL_CHECK(plan.processorTailSeconds <= 0.5 + 1e-9);
}

MPL_TEST(ProcessorAutomationIsBlockSizeStable) {
    const Compilation compilation = compileExample("production.mpl");
    const auto render = [&](int blockSize) {
        AudioRenderConfig config{8000, blockSize, 2, 0.25};
        std::vector<Diagnostic> diagnostics;
        const AudioExecutionPlan plan =
            buildPlan(compilation.production, compilation.temporal, config, diagnostics);
        return ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
    };
    const AudioBuffer small = render(64);
    const AudioBuffer large = render(512);
    MPL_CHECK_EQ(small.samples.size(), large.samples.size());
    double maximumDifference = 0.0;
    for (std::size_t index = 0; index < small.samples.size(); ++index) {
        maximumDifference =
            std::max(maximumDifference,
                     std::fabs(static_cast<double>(small.samples[index] - large.samples[index])));
    }
    MPL_CHECK_NEAR(maximumDifference, 0.0, 2e-3);
}

MPL_TEST(RenderingIsDeterministicAndResetsState) {
    const Compilation compilation = compileExample("production.mpl");
    const AudioRenderConfig config{8000, 128, 2, 0.1};
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan =
        buildPlan(compilation.production, compilation.temporal, config, diagnostics);
    const ReferenceAudioBackend backend(config);
    const AudioBuffer first = backend.renderPlan(plan, compilation.temporal, diagnostics);
    const AudioBuffer second = backend.renderPlan(plan, compilation.temporal, diagnostics);
    MPL_CHECK(first.samples == second.samples);
}

MPL_TEST(RendererAppliesSidechainKey) {
    const AudioRenderConfig config{8000, 64, 2, 0.0};
    InstrumentAsset instrument;
    instrument.id = "sine";
    instrument.oscillator = Oscillator::Sine;
    instrument.envelope = AdsrEnvelope{0.0, 0.0, 1.0, 0.0};
    instrument.gain = 1.0f;

    TrackPlan main;
    main.name = "main";
    main.instrument = instrument;
    main.notes.push_back(PlannedNote{57, 0, 4000, 0.05f, 0.0f, false});
    TrackPlan key;
    key.name = "key";
    key.instrument = instrument;
    key.notes.push_back(PlannedNote{69, 0, 4000, 1.0f, 0.0f, false});

    const auto render = [&](bool withSidechain) {
        AudioExecutionPlan plan;
        plan.config = config;
        plan.tracks = {main, key};
        plan.tempo.push_back(TempoSegment{0.0, 0.0, 120.0});
        plan.totalFrames = 4000;
        plan.renderedSeconds = 0.5;
        SignalGraphPlan graph;
        const int mainSource = addNode(graph, SignalNodeKind::Source, "main", 0, 2);
        graph.nodes[static_cast<std::size_t>(mainSource)].trackIndex = 0;
        const int keySource = addNode(graph, SignalNodeKind::Source, "key", 0, 2);
        graph.nodes[static_cast<std::size_t>(keySource)].trackIndex = 1;
        const int compressor = addNode(graph, SignalNodeKind::Processor, "comp", 2, 2);
        graph.nodes[static_cast<std::size_t>(compressor)].processor =
            ProcessorSpec{"comp", ProcessorKind::Compressor, "",
                          {{"threshold", -20.0},
                           {"ratio", 12.0},
                           {"attack", 0.002},
                           {"release", 0.1},
                           {"makeup", 0.0}}};
        graph.nodes[static_cast<std::size_t>(compressor)].hasSidechainInput = true;
        const int output = addNode(graph, SignalNodeKind::Output, "out", 2, 0);
        connectNode(graph, mainSource, compressor);
        if (withSidechain) {
            connectNode(graph, keySource, compressor, 1.0, true);
        }
        connectNode(graph, compressor, output);
        graph.outputNode = output;
        std::vector<Diagnostic> diagnostics;
        MPL_CHECK(validateSignalGraph(graph, diagnostics));
        plan.graph = std::move(graph);
        return ReferenceAudioBackend(config).renderPlan(plan, TemporalIR{}, diagnostics);
    };

    const AudioBuffer ducked = render(true);
    const AudioBuffer plain = render(false);
    MPL_CHECK(bufferEnergy(ducked) < bufferEnergy(plain) * 0.2);
}

MPL_TEST(ProductionDemoRendersGoldenStats) {
    const Compilation compilation = compileExample("production.mpl");
    MPL_CHECK(!compilation.hasErrors());
    MPL_CHECK_EQ(compilation.production.tracks.size(), std::size_t{5});
    MPL_CHECK(compilation.production.logicalExtent.inWholeNotes() == Rational(10, 1));

    const AudioRenderConfig config{8000, 128, 2, 0.5};
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan =
        buildPlan(compilation.production, compilation.temporal, config, diagnostics);
    MPL_CHECK(plan.renderedSeconds > 25.0);
    MPL_CHECK(plan.processorTailSeconds > 0.0);
    const AudioBuffer buffer =
        ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
    MPL_CHECK(peak(buffer) > 0.05);
    MPL_CHECK(peak(buffer) <= 1.0);

    const std::filesystem::path wavPath =
        std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "production-demo.wav";
    ReferenceAudioBackend(config).writeWav(buffer, wavPath, diagnostics);
    MPL_CHECK(std::filesystem::exists(wavPath));
    checkGolden(audioStatsJson(buffer), "production-demo.json");
}
