#include "TestHarness.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/audio/AudioAnalysis.hpp"
#include "mpl/audio/Processors.hpp"
#include "mpl/audio/ReferenceAudioBackend.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/mastering/Mastering.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

using namespace mpl;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
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
        throw ::mpltest::AssertionFailure(name + " failed to compile");
    }
    const Compiler compiler;
    return compiler.compile(std::move(result.program->project));
}

AudioBuffer sineBuffer(int sampleRate, int channels, double frequency, double amplitude,
                       double seconds) {
    AudioBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.channels = channels;
    const int frames = static_cast<int>(std::llround(seconds * sampleRate));
    buffer.samples.resize(static_cast<std::size_t>(frames) * channels);
    for (int frame = 0; frame < frames; ++frame) {
        const double value =
            amplitude * std::sin(2.0 * kPi * frequency * static_cast<double>(frame) / sampleRate);
        for (int channel = 0; channel < channels; ++channel) {
            buffer.samples[static_cast<std::size_t>(frame) * channels + channel] =
                static_cast<float>(value);
        }
    }
    return buffer;
}

double peakOf(const AudioBuffer& buffer) {
    double peak = 0.0;
    for (const float sample : buffer.samples) {
        peak = std::max(peak, std::fabs(static_cast<double>(sample)));
    }
    return peak;
}

}  // namespace

MPL_TEST(AnalysisPeakRmsAndCrest) {
    std::vector<Diagnostic> diagnostics;
    AudioBuffer constant;
    constant.sampleRate = 48000;
    constant.channels = 1;
    constant.samples.assign(4800, 0.5f);
    const AudioAnalysis dc = analyzeAudio(constant, diagnostics);
    MPL_CHECK_NEAR(dc.samplePeak, 0.5, 1e-9);
    MPL_CHECK_NEAR(dc.samplePeakDb, -6.020599913, 1e-6);
    MPL_CHECK_NEAR(dc.rms, 0.5, 1e-9);
    MPL_CHECK_NEAR(dc.crestFactorDb, 0.0, 1e-9);
    MPL_CHECK_NEAR(dc.truePeak, 0.5, 0.002);

    const AudioBuffer sine = sineBuffer(48000, 2, 997.0, 1.0, 1.0);
    const AudioAnalysis analysed = analyzeAudio(sine, diagnostics);
    MPL_CHECK_NEAR(analysed.samplePeak, 1.0, 0.002);
    MPL_CHECK_NEAR(analysed.rms, 1.0 / std::sqrt(2.0), 0.005);
    MPL_CHECK_NEAR(analysed.crestFactorDb, 3.0103, 0.05);
    MPL_CHECK(!analysed.clipped);
}

MPL_TEST(AnalysisSilenceAndShortBuffers) {
    std::vector<Diagnostic> diagnostics;
    AudioBuffer silence;
    silence.sampleRate = 48000;
    silence.channels = 2;
    silence.samples.assign(48000 * 2, 0.0f);
    const AudioAnalysis analysed = analyzeAudio(silence, diagnostics);
    MPL_CHECK_NEAR(analysed.samplePeak, 0.0, 1e-12);
    MPL_CHECK_NEAR(analysed.rms, 0.0, 1e-12);
    MPL_CHECK_NEAR(analysed.crestFactorDb, 0.0, 1e-12);
    MPL_CHECK_NEAR(analysed.truePeak, 0.0, 1e-12);
    MPL_CHECK(!std::isfinite(analysed.integratedLoudness));
    MPL_CHECK(!std::isfinite(analysed.maxShortTermLoudness));

    const AudioBuffer shortBuffer = sineBuffer(48000, 2, 997.0, 0.1, 0.2);
    const AudioAnalysis shortAnalysis = analyzeAudio(shortBuffer, diagnostics);
    MPL_CHECK_NEAR(shortAnalysis.samplePeak, 0.1, 0.002);
    MPL_CHECK(!std::isfinite(shortAnalysis.integratedLoudness));
    MPL_CHECK(!std::isfinite(shortAnalysis.maxShortTermLoudness));
}

MPL_TEST(AnalysisLoudnessReference) {
    std::vector<Diagnostic> diagnostics;
    const AudioBuffer buffer = sineBuffer(48000, 2, 997.0, 0.1, 5.0);
    const AudioAnalysis analysis = analyzeAudio(buffer, diagnostics);
    // BS.1770 / EBU Tech 3341: a 997 Hz stereo sine at -20 dBFS reads -20 LUFS.
    MPL_CHECK_NEAR(analysis.integratedLoudness, -20.0, 0.2);
    MPL_CHECK_NEAR(analysis.maxShortTermLoudness, -20.0, 0.2);
    MPL_CHECK(analysis.integratedLoudness < -19.5);
    MPL_CHECK(analysis.integratedLoudness > -20.5);
}

MPL_TEST(AnalysisTruePeakReference) {
    std::vector<Diagnostic> diagnostics;
    const int sampleRate = 48000;
    const int frames = sampleRate;
    AudioBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.channels = 2;
    buffer.samples.resize(static_cast<std::size_t>(frames) * 2);
    for (int frame = 0; frame < frames; ++frame) {
        const double value = 0.5 * std::sin(kPi / 2.0 * static_cast<double>(frame) + kPi / 4.0);
        buffer.samples[static_cast<std::size_t>(frame) * 2] = static_cast<float>(value);
        buffer.samples[static_cast<std::size_t>(frame) * 2 + 1] = static_cast<float>(value);
    }
    const AudioAnalysis analysis = analyzeAudio(buffer, diagnostics);
    MPL_CHECK_NEAR(analysis.samplePeak, 0.3535533906, 1e-3);
    MPL_CHECK_NEAR(analysis.truePeak, 0.5, 0.02);
    MPL_CHECK_NEAR(analysis.truePeakDb - analysis.samplePeakDb, 3.0103, 0.35);

    const AudioBuffer low = sineBuffer(sampleRate, 2, 997.0, 0.5, 1.0);
    const AudioAnalysis lowAnalysis = analyzeAudio(low, diagnostics);
    MPL_CHECK(lowAnalysis.truePeakDb - lowAnalysis.samplePeakDb < 0.2);
}

MPL_TEST(AnalysisGatingNonFiniteAndClipping) {
    std::vector<Diagnostic> diagnostics;
    AudioBuffer buffer;
    buffer.sampleRate = 48000;
    buffer.channels = 2;
    const int frames = 48000 * 10;
    buffer.samples.resize(static_cast<std::size_t>(frames) * 2, 0.0f);
    for (int frame = 0; frame < frames / 2; ++frame) {
        const double value = 0.1 * std::sin(2.0 * kPi * 997.0 * frame / 48000.0);
        for (int channel = 0; channel < 2; ++channel) {
            buffer.samples[static_cast<std::size_t>(frame) * 2 + channel] =
                static_cast<float>(value);
        }
    }
    const AudioAnalysis gated = analyzeAudio(buffer, diagnostics);
    MPL_CHECK_NEAR(gated.integratedLoudness, -20.0, 0.4);

    AudioBuffer invalid = sineBuffer(48000, 1, 997.0, 0.1, 1.0);
    invalid.samples[100] = std::numeric_limits<float>::quiet_NaN();
    invalid.samples[200] = std::numeric_limits<float>::infinity();
    std::vector<Diagnostic> invalidDiagnostics;
    const AudioAnalysis invalidAnalysis = analyzeAudio(invalid, invalidDiagnostics);
    MPL_CHECK(invalidAnalysis.nonFinite);
    MPL_CHECK(hasCode(invalidDiagnostics, "analysis-non-finite"));
    MPL_CHECK(std::isfinite(invalidAnalysis.samplePeak));

    AudioBuffer loud = sineBuffer(48000, 1, 997.0, 1.2, 1.0);
    std::vector<Diagnostic> loudDiagnostics;
    const AudioAnalysis loudAnalysis = analyzeAudio(loud, loudDiagnostics);
    MPL_CHECK(loudAnalysis.clipped);
    MPL_CHECK(loudAnalysis.samplePeak > 1.0);

    AudioAnalysisConfig config;
    config.startFrame = 0;
    config.endFrame = 24000;
    const AudioAnalysis ranged = analyzeAudio(invalid, config, invalidDiagnostics);
    MPL_CHECK_EQ(ranged.frames, std::size_t{24000});
    MPL_CHECK_EQ(ranged.startFrame, std::size_t{0});
}

MPL_TEST(AnalysisJsonHasUnitsAndNulls) {
    std::vector<Diagnostic> diagnostics;
    AudioBuffer silence;
    silence.sampleRate = 48000;
    silence.channels = 2;
    silence.samples.assign(4800 * 2, 0.0f);
    const AudioAnalysis analysis = analyzeAudio(silence, diagnostics);
    const std::string json = audioAnalysisToJson(analysis, AudioAnalysisConfig{});
    MPL_CHECK(json.find("\"format\": \"mpl.audio-analysis\"") != std::string::npos);
    MPL_CHECK(json.find("\"truePeakOversampling\": 4") != std::string::npos);
    MPL_CHECK(json.find("\"integratedLoudnessLufs\": null") != std::string::npos);
    MPL_CHECK(json.find("\"samplePeakDbfs\": null") != std::string::npos);
}

MPL_TEST(LimiterCapsSamplePeakAndReportsLatency) {
    const int sampleRate = 8000;
    std::vector<float> samples = sineBuffer(sampleRate, 1, 440.0, 1.0, 1.0).samples;
    auto limiter = createProcessor(ProcessorKind::Limiter, "");
    ParameterRamp ramp;
    ramp.setConstant("ceiling", -6.0);
    ramp.setConstant("attack", 0.01);
    ramp.setConstant("release", 0.1);
    limiter->prepare(sampleRate, 1);
    const std::size_t stride = 64;
    for (std::size_t offset = 0; offset < samples.size(); offset += stride) {
        const int frames = static_cast<int>(std::min<std::size_t>(stride, samples.size() - offset));
        limiter->process(samples.data() + offset, frames, 1, ramp, nullptr);
    }
    MPL_CHECK_NEAR(limiter->latencySeconds(), 0.01, 1e-9);
    const double ceiling = std::pow(10.0, -6.0 / 20.0);
    for (const float sample : samples) {
        MPL_CHECK(std::fabs(static_cast<double>(sample)) <= ceiling + 1e-5);
    }
    double early = 0.0;
    for (std::size_t frame = 0; frame < 60; ++frame) {
        early = std::max(early, std::fabs(static_cast<double>(samples[frame])));
    }
    MPL_CHECK_NEAR(early, 0.0, 1e-6);

    limiter->reset();
    std::vector<float> silence(512, 0.0f);
    for (std::size_t offset = 0; offset < silence.size(); offset += stride) {
        limiter->process(silence.data() + offset, 64, 1, ramp, nullptr);
    }
    for (const float sample : silence) {
        MPL_CHECK_NEAR(sample, 0.0, 1e-9);
    }
}

MPL_TEST(LimiterLatencyIsCompensatedAtOutput) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = notes span 1 bars { C4 @ 0 for 1/4 vel 100 };\n"
        "processor Lim = limiter { ceiling = -6 dB; attack = 10 ms; release = 100 ms; };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "master { insert Lim; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    FrontendResult frontend = compileSource(source, "<test>");
    MPL_CHECK(frontend.ok());
    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));

    const AudioRenderConfig config{8000, 128, 1, 0.0};
    const ReferenceAssetLibrary resolver;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, resolver, config, diagnostics);
    MPL_CHECK_NEAR(plan.outputLatencySeconds, 0.01, 1e-9);
    MPL_CHECK_EQ(plan.outputLatencyFrames, 80);
    MPL_CHECK(!plan.latencyMisaligned);
    MPL_CHECK_EQ(plan.totalFrames, 16000 + 80);

    const AudioBuffer buffer =
        ReferenceAudioBackend(config).renderPlan(plan, compilation.temporal, diagnostics);
    MPL_CHECK_EQ(static_cast<int>(buffer.frameCount()), 16000);
    std::size_t firstAudible = buffer.frameCount();
    for (std::size_t frame = 0; frame < buffer.frameCount(); ++frame) {
        if (std::fabs(buffer.samples[frame]) > 1e-4) {
            firstAudible = frame;
            break;
        }
    }
    MPL_CHECK(firstAudible < 400);
}

MPL_TEST(ParallelLimiterLatencyIsDiagnosed) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = notes span 1 bars { C4 @ 0 for 1/4 vel 100 };\n"
        "processor Lim = limiter { ceiling = -6 dB; attack = 10 ms; release = 100 ms; };\n"
        "bus B { insert Lim; }\n"
        "track Piano { asset = PianoAsset; role = melodic; send 1.0 to B; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n";
    FrontendResult frontend = compileSource(source, "<test>");
    MPL_CHECK(frontend.ok());
    const Compiler compiler;
    const Compilation compilation = compiler.compile(std::move(frontend.program->project));
    const AudioRenderConfig config{8000, 128, 2, 0.0};
    const ReferenceAssetLibrary resolver;
    std::vector<Diagnostic> diagnostics;
    const AudioExecutionPlan plan = buildAudioExecutionPlan(
        compilation.production, compilation.temporal, resolver, config, diagnostics);
    MPL_CHECK(plan.latencyMisaligned);
    MPL_CHECK(hasCode(diagnostics, "audio-latency-misaligned"));
}

MPL_TEST(LimiterAttackAutomationIsRejected) {
    const std::string source =
        "project T { tempo = 120 bpm; meter = 4/4; key = C major; }\n"
        "asset PianoAsset = instrument(\"GrandPiano\");\n"
        "let A = melody { C4: 1/4 };\n"
        "curve C on musical for 1 bars = linear(0.002, 0.01);\n"
        "processor Lim = limiter { ceiling = -1 dB; attack = 5 ms; release = 100 ms; };\n"
        "track Piano { asset = PianoAsset; role = melodic; }\n"
        "master { insert Lim; }\n"
        "section S(1 bars) { clip Piano { play A; } }\n"
        "arrangement { play S; }\n"
        "export midi(\"x.mid\");\n"
        "automation master.Lim.attack = C;\n";
    const FrontendResult result = compileSource(source, "<test>");
    MPL_CHECK(hasFrontendCode(result, "sema-processor-automation"));
}

MPL_TEST(MasteringMeetsLoudnessAndPeakTargets) {
    const Compilation compilation = compileExample("production.mpl");
    const AudioRenderConfig config{8000, 256, 2, 0.25};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_EXAMPLES_DIR));
    std::vector<Diagnostic> diagnostics;
    MasteringTarget target;
    target.targetLufs = -14.0;
    target.loudnessToleranceLu = 0.5;
    target.peakCeilingDb = -0.5;
    target.maxIterations = 4;
    const MasteringResult result = runMasteringFeedback(
        compilation.production, compilation.temporal, resolver, config, target, diagnostics);

    MPL_CHECK(result.satisfied);
    MPL_CHECK(result.status == MasteringStatus::StrictlySatisfied ||
              result.status == MasteringStatus::SatisfiedWithinTolerance);
    MPL_CHECK(!hasCode(diagnostics, "mastering-unsatisfiable"));
    MPL_CHECK_NEAR(result.after.integratedLoudness, -14.0, 0.5);
    MPL_CHECK(result.after.truePeakDb <= -0.4);
    MPL_CHECK(result.finalMasterGainDb > result.trace.front().masterGainDb);
    MPL_CHECK(result.trace.size() >= 2);
    MPL_CHECK_NEAR(result.trace.back().masterGainDb, result.finalMasterGainDb, 1e-12);
    MPL_CHECK_NEAR(result.trace.back().analysis.integratedLoudness,
                   result.after.integratedLoudness, 1e-12);
    MPL_CHECK(peakOf(result.mastered) > peakOf(result.raw));

    const AudioAnalysis independent = analyzeAudio(result.mastered, diagnostics);
    MPL_CHECK_NEAR(independent.integratedLoudness, result.after.integratedLoudness, 1e-9);
    MPL_CHECK_NEAR(independent.truePeakDb, result.after.truePeakDb, 1e-9);

    const std::filesystem::path path =
        std::filesystem::path(MPL_TEST_OUTPUT_DIR) / "mastering-test.wav";
    std::vector<Diagnostic> writeDiagnostics;
    ReferenceAudioBackend(config).writeWav(result.mastered, path, writeDiagnostics);
    MPL_CHECK(std::filesystem::exists(path));
}

MPL_TEST(MasteringUnsatisfiableTargetIsDiagnosed) {
    const Compilation compilation = compileExample("production.mpl");
    const AudioRenderConfig config{8000, 256, 2, 0.25};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_EXAMPLES_DIR));
    std::vector<Diagnostic> diagnostics;
    MasteringTarget target;
    target.targetLufs = 6.0;
    target.loudnessToleranceLu = 0.2;
    target.peakCeilingDb = -0.5;
    target.maxIterations = 3;
    const MasteringResult result = runMasteringFeedback(
        compilation.production, compilation.temporal, resolver, config, target, diagnostics);
    MPL_CHECK(!result.satisfied);
    MPL_CHECK(hasCode(diagnostics, "mastering-unsatisfiable"));
    MPL_CHECK(hasCode(diagnostics, "mastering-max-iterations") ||
              static_cast<int>(result.trace.size()) <= target.maxIterations + 1);
}

MPL_TEST(MasteringMaximumIterationsAreRespected) {
    const Compilation compilation = compileExample("production.mpl");
    const AudioRenderConfig config{8000, 256, 2, 0.25};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_EXAMPLES_DIR));
    std::vector<Diagnostic> diagnostics;
    MasteringTarget target;
    target.targetLufs = 0.0;
    target.loudnessToleranceLu = 0.2;
    target.peakCeilingDb = -0.5;
    target.maxIterations = 1;
    const MasteringResult result = runMasteringFeedback(
        compilation.production, compilation.temporal, resolver, config, target, diagnostics);
    MPL_CHECK_EQ(result.trace.size(), std::size_t{2});
    MPL_CHECK(!result.satisfied);
    MPL_CHECK(hasCode(diagnostics, "mastering-max-iterations"));
}

MPL_TEST(MasteringWithoutLimiterUsesGainOnly) {
    const Compilation compilation = compileExample("production.mpl");
    const AudioRenderConfig config{8000, 256, 2, 0.25};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_EXAMPLES_DIR));
    std::vector<Diagnostic> diagnostics;
    MasteringTarget target;
    target.targetLufs = -18.0;
    target.loudnessToleranceLu = 0.5;
    target.peakCeilingDb = -0.5;
    target.maxIterations = 3;
    target.addLimiter = false;
    const MasteringResult result = runMasteringFeedback(
        compilation.production, compilation.temporal, resolver, config, target, diagnostics);
    MPL_CHECK(result.limiterName.empty());
    MPL_CHECK(result.satisfied);
    MPL_CHECK_NEAR(result.after.integratedLoudness, -18.0, 0.5);
    MPL_CHECK(result.after.truePeakDb <= -0.4);
}

MPL_TEST(MasteringTraceJsonIsComplete) {
    const Compilation compilation = compileExample("production.mpl");
    const AudioRenderConfig config{8000, 256, 2, 0.25};
    const ReferenceAssetLibrary resolver(std::filesystem::path(MPL_EXAMPLES_DIR));
    std::vector<Diagnostic> diagnostics;
    MasteringTarget target;
    target.targetLufs = -14.0;
    target.peakCeilingDb = -0.5;
    target.maxIterations = 4;
    const MasteringResult result = runMasteringFeedback(
        compilation.production, compilation.temporal, resolver, config, target, diagnostics);
    const std::string json = masteringResultToJson(result, AudioAnalysisConfig{});
    MPL_CHECK(json.find("\"format\": \"mpl.mastering-report\"") != std::string::npos);
    MPL_CHECK(json.find("\"targetLufs\": -14") != std::string::npos);
    MPL_CHECK(json.find("\"limiterCeilingDb\"") != std::string::npos);
    MPL_CHECK(json.find("\"iterations\"") != std::string::npos);
    MPL_CHECK(json.find("\"before\"") != std::string::npos);
    MPL_CHECK(json.find("\"after\"") != std::string::npos);
    MPL_CHECK(json.find("\"satisfied\": true") != std::string::npos);
    MPL_CHECK(json.find("\"status\": \"strictly-satisfied\"") != std::string::npos ||
              json.find("\"status\": \"satisfied-within-tolerance\"") != std::string::npos);
    MPL_CHECK(json.find("\"finalParameters\"") != std::string::npos);
    MPL_CHECK(json.find("\"peakToleranceDb\"") != std::string::npos);
}
