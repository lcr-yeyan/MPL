#include "mpl/audio/AudioAnalysis.hpp"

#include "mpl/util/JsonWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace mpl {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kLoudnessOffset = 0.691;
constexpr double kAbsoluteGateLufs = -70.0;
constexpr int kTruePeakTaps = 49;
constexpr int kTruePeakFactor = 4;

struct KFilter {
    double b[5] = {};
    double a[5] = {};

    void design(int sampleRate) {
        // BS.1770 pre-filter (stage 1: high shelf, stage 2: high pass). The
        // prototype parameters are the values used by the reference
        // implementation; at 48 kHz they reproduce the published coefficients.
        double f0 = 1681.974450955533;
        double gainDb = 3.999843853973347;
        double q = 0.7071752369554196;
        double k = std::tan(kPi * f0 / static_cast<double>(sampleRate));
        const double vh = std::pow(10.0, gainDb / 20.0);
        const double vb = std::pow(vh, 0.4996667741545416);
        double pb[3] = {0.0, 0.0, 0.0};
        double pa[3] = {1.0, 0.0, 0.0};
        const double a0 = 1.0 + k / q + k * k;
        pb[0] = (vh + vb * k / q + k * k) / a0;
        pb[1] = 2.0 * (k * k - vh) / a0;
        pb[2] = (vh - vb * k / q + k * k) / a0;
        pa[1] = 2.0 * (k * k - 1.0) / a0;
        pa[2] = (1.0 - k / q + k * k) / a0;

        f0 = 38.13547087602444;
        q = 0.5003270373238773;
        k = std::tan(kPi * f0 / static_cast<double>(sampleRate));
        const double rb[3] = {1.0, -2.0, 1.0};
        double ra[3] = {1.0, 0.0, 0.0};
        ra[1] = 2.0 * (k * k - 1.0) / (1.0 + k / q + k * k);
        ra[2] = (1.0 - k / q + k * k) / (1.0 + k / q + k * k);

        b[0] = pb[0] * rb[0];
        b[1] = pb[0] * rb[1] + pb[1] * rb[0];
        b[2] = pb[0] * rb[2] + pb[1] * rb[1] + pb[2] * rb[0];
        b[3] = pb[1] * rb[2] + pb[2] * rb[1];
        b[4] = pb[2] * rb[2];

        a[0] = pa[0] * ra[0];
        a[1] = pa[0] * ra[1] + pa[1] * ra[0];
        a[2] = pa[0] * ra[2] + pa[1] * ra[1] + pa[2] * ra[0];
        a[3] = pa[1] * ra[2] + pa[2] * ra[1];
        a[4] = pa[2] * ra[2];
    }

    double process(double* state, double input) const {
        const double v0 = input - a[1] * state[1] - a[2] * state[2] - a[3] * state[3] -
                          a[4] * state[4];
        const double output =
            b[0] * v0 + b[1] * state[1] + b[2] * state[2] + b[3] * state[3] + b[4] * state[4];
        state[4] = state[3];
        state[3] = state[2];
        state[2] = state[1];
        state[1] = v0;
        return output;
    }
};

struct PhaseFilter {
    std::vector<int> delay;
    std::vector<double> coefficient;
};

/// 4x polyphase Hann-windowed sinc, 49 taps. Each phase is normalised to unity
/// DC so a constant signal is not boosted by the interpolation filter.
std::vector<PhaseFilter> makeTruePeakPhases() {
    std::vector<double> taps(kTruePeakTaps, 0.0);
    for (int index = 0; index < kTruePeakTaps; ++index) {
        const double m = static_cast<double>(index) -
                         static_cast<double>(kTruePeakTaps - 1) / 2.0;
        double value = 1.0;
        if (std::fabs(m) > 1e-12) {
            const double x = m * kPi / static_cast<double>(kTruePeakFactor);
            value = std::sin(x) / x;
        }
        value *= 0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(index) /
                                      static_cast<double>(kTruePeakTaps - 1)));
        taps[static_cast<std::size_t>(index)] = value;
    }

    std::vector<PhaseFilter> phases(static_cast<std::size_t>(kTruePeakFactor));
    for (int phase = 0; phase < kTruePeakFactor; ++phase) {
        PhaseFilter& filter = phases[static_cast<std::size_t>(phase)];
        for (int index = phase; index < kTruePeakTaps; index += kTruePeakFactor) {
            if (std::fabs(taps[static_cast<std::size_t>(index)]) < 1e-12) {
                continue;
            }
            filter.delay.push_back(index / kTruePeakFactor);
            filter.coefficient.push_back(taps[static_cast<std::size_t>(index)]);
        }
        double sum = 0.0;
        for (const double coefficient : filter.coefficient) {
            sum += coefficient;
        }
        if (std::fabs(sum) > 1e-12) {
            for (double& coefficient : filter.coefficient) {
                coefficient /= sum;
            }
        }
    }
    return phases;
}

double mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const double value : values) {
        sum += value;
    }
    return sum / static_cast<double>(values.size());
}

}  // namespace

double linearToDb(double linear) {
    return linear > 0.0 ? 20.0 * std::log10(linear)
                        : -std::numeric_limits<double>::infinity();
}

double dbToLinear(double db) { return std::pow(10.0, db / 20.0); }

double energyToLoudness(double energy) {
    return energy > 0.0 ? 10.0 * std::log10(energy) - kLoudnessOffset
                        : -std::numeric_limits<double>::infinity();
}

AudioAnalysis analyzeAudio(const AudioBuffer& buffer, const AudioAnalysisConfig& config,
                           std::vector<Diagnostic>& diagnostics) {
    AudioAnalysis analysis;
    analysis.sampleRate = buffer.sampleRate;
    analysis.channels = std::max(1, buffer.channels);

    if (config.truePeakOversampling != 4) {
        diagnostics.push_back(Diagnostic::error(
            "analysis-config", "true-peak analysis supports only 4x oversampling"));
    }
    if (buffer.sampleRate <= 0) {
        diagnostics.push_back(
            Diagnostic::error("analysis-config", "audio buffer has an invalid sample rate"));
        return analysis;
    }

    const std::size_t totalFrames = buffer.frameCount();
    const std::size_t start = std::min(config.startFrame, totalFrames);
    const std::size_t end = std::min(config.endFrame, totalFrames);
    if (end <= start) {
        diagnostics.push_back(
            Diagnostic::warning("analysis-empty-range", "audio analysis range is empty"));
        analysis.startFrame = start;
        analysis.integratedLoudness = -std::numeric_limits<double>::infinity();
        analysis.maxShortTermLoudness = -std::numeric_limits<double>::infinity();
        return analysis;
    }

    const std::size_t frames = end - start;
    const int channels = analysis.channels;
    analysis.frames = frames;
    analysis.startFrame = start;

    std::vector<double> clean(frames * static_cast<std::size_t>(channels), 0.0);
    double sumSquares = 0.0;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const std::size_t index =
                (start + frame) * static_cast<std::size_t>(channels) +
                static_cast<std::size_t>(channel);
            const double value = static_cast<double>(buffer.samples[index]);
            if (!std::isfinite(value)) {
                analysis.nonFinite = true;
                continue;
            }
            clean[frame * static_cast<std::size_t>(channels) +
                  static_cast<std::size_t>(channel)] = value;
            const double magnitude = std::fabs(value);
            analysis.samplePeak = std::max(analysis.samplePeak, magnitude);
            sumSquares += value * value;
        }
    }
    if (analysis.nonFinite) {
        diagnostics.push_back(Diagnostic::warning(
            "analysis-non-finite", "audio analysis replaced NaN/Inf samples with zero"));
    }

    const double sampleCount = static_cast<double>(frames) * channels;
    analysis.rms = sampleCount > 0.0 ? std::sqrt(sumSquares / sampleCount) : 0.0;
    analysis.samplePeakDb = linearToDb(analysis.samplePeak);
    analysis.rmsDb = linearToDb(analysis.rms);
    analysis.crestFactorDb =
        analysis.samplePeak > 0.0 && analysis.rms > 0.0
            ? 20.0 * std::log10(analysis.samplePeak / analysis.rms)
            : 0.0;
    analysis.clipped = analysis.samplePeak > 1.0;

    const std::vector<PhaseFilter> phases = makeTruePeakPhases();
    double truePeak = analysis.samplePeak;
    for (int channel = 0; channel < channels; ++channel) {
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (const PhaseFilter& phase : phases) {
                double value = 0.0;
                for (std::size_t tap = 0; tap < phase.delay.size(); ++tap) {
                    const std::size_t delay = static_cast<std::size_t>(phase.delay[tap]);
                    // Edge samples are held instead of zero-padded; zero padding
                    // creates artificial ringing at the start of a block.
                    const std::size_t sourceFrame = delay > frame ? 0 : frame - delay;
                    value += phase.coefficient[tap] *
                             clean[sourceFrame * static_cast<std::size_t>(channels) +
                                   static_cast<std::size_t>(channel)];
                }
                truePeak = std::max(truePeak, std::fabs(value));
            }
        }
    }
    analysis.truePeak = truePeak;
    analysis.truePeakDb = linearToDb(truePeak);

    KFilter filter;
    filter.design(buffer.sampleRate);
    std::vector<double> filtered(frames * static_cast<std::size_t>(channels), 0.0);
    for (int channel = 0; channel < channels; ++channel) {
        double state[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::size_t index =
                frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel);
            filtered[index] = filter.process(state, clean[index]);
        }
    }

    const auto blockEnergy = [&](std::size_t blockStart, std::size_t blockFrames) {
        double sum = 0.0;
        for (int channel = 0; channel < channels; ++channel) {
            double channelSum = 0.0;
            for (std::size_t frame = 0; frame < blockFrames; ++frame) {
                const double value =
                    filtered[(blockStart + frame) * static_cast<std::size_t>(channels) +
                             static_cast<std::size_t>(channel)];
                channelSum += value * value;
            }
            sum += channelSum;
        }
        return blockFrames > 0 ? sum / static_cast<double>(blockFrames) : 0.0;
    };

    const std::size_t blockFrames = static_cast<std::size_t>(
        std::max(1, static_cast<int>(std::llround(config.integratedBlockSeconds *
                                                  buffer.sampleRate))));
    const std::size_t hopFrames = static_cast<std::size_t>(
        std::max(1, static_cast<int>(std::llround(config.blockHopSeconds * buffer.sampleRate))));

    std::vector<double> blockEnergies;
    if (frames >= blockFrames) {
        for (std::size_t blockStart = 0; blockStart + blockFrames <= frames;
             blockStart += hopFrames) {
            const double energy = blockEnergy(blockStart, blockFrames);
            if (energy >= std::pow(10.0, (kAbsoluteGateLufs + kLoudnessOffset) / 10.0)) {
                blockEnergies.push_back(energy);
            }
        }
    }
    if (!blockEnergies.empty()) {
        const double relativeGate = mean(blockEnergies) * 0.1;
        std::vector<double> gated;
        for (const double energy : blockEnergies) {
            if (energy >= relativeGate) {
                gated.push_back(energy);
            }
        }
        if (!gated.empty()) {
            analysis.integratedLoudness = energyToLoudness(mean(gated));
        }
    }

    const std::size_t shortTermFrames = static_cast<std::size_t>(
        std::max(1, static_cast<int>(std::llround(config.shortTermSeconds * buffer.sampleRate))));
    if (frames >= shortTermFrames) {
        for (std::size_t blockStart = 0; blockStart + shortTermFrames <= frames;
             blockStart += hopFrames) {
            const double energy = blockEnergy(blockStart, shortTermFrames);
            if (energy >= std::pow(10.0, (kAbsoluteGateLufs + kLoudnessOffset) / 10.0)) {
                analysis.maxShortTermLoudness =
                    std::max(analysis.maxShortTermLoudness, energyToLoudness(energy));
            }
        }
    }

    return analysis;
}

AudioAnalysis analyzeAudio(const AudioBuffer& buffer, std::vector<Diagnostic>& diagnostics) {
    return analyzeAudio(buffer, AudioAnalysisConfig{}, diagnostics);
}

std::string audioAnalysisToJson(const AudioAnalysis& analysis,
                                const AudioAnalysisConfig& config) {
    JsonWriter json;
    writeAudioAnalysisJson(json, analysis, config);
    return json.take();
}

void writeAudioAnalysisJson(JsonWriter& json, const AudioAnalysis& analysis,
                            const AudioAnalysisConfig& config) {
    json.beginObject();
    json.key("format").value("mpl.audio-analysis");
    json.key("version").value(1);
    json.key("sampleRate").value(analysis.sampleRate);
    json.key("channels").value(analysis.channels);
    json.key("frames").value(analysis.frames);
    json.key("startFrame").value(analysis.startFrame);
    json.key("endFrame").value(analysis.startFrame + analysis.frames);
    json.key("truePeakOversampling").value(config.truePeakOversampling);

    const auto writeOptional = [&json](std::string_view name, double value) {
        json.key(name);
        if (std::isfinite(value)) {
            json.value(value);
        } else {
            json.nullValue();
        }
    };

    json.key("samplePeak").value(analysis.samplePeak);
    writeOptional("samplePeakDbfs", analysis.samplePeakDb);
    json.key("rms").value(analysis.rms);
    writeOptional("rmsDbfs", analysis.rmsDb);
    json.key("crestFactorDb").value(analysis.crestFactorDb);
    json.key("truePeak").value(analysis.truePeak);
    writeOptional("truePeakDbtp", analysis.truePeakDb);

    writeOptional("integratedLoudnessLufs", analysis.integratedLoudness);
    writeOptional("maxShortTermLoudnessLufs", analysis.maxShortTermLoudness);

    json.key("clipped").value(analysis.clipped);
    json.key("nonFinite").value(analysis.nonFinite);
    json.endObject();
}

}  // namespace mpl
