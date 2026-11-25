#include "mpl/audio/Processors.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

constexpr double kPi = 3.14159265358979323846;

double dbToLinear(double db) {
    return std::pow(10.0, db / 20.0);
}

class GainProcessor final : public AudioProcessor {
public:
    void prepare(int, int) override { reset(); }
    void reset() override {}
    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float*) override {
        for (int frame = 0; frame < frames; ++frame) {
            const double fraction = frames > 1 ? static_cast<double>(frame) / frames : 0.0;
            const double gain = dbToLinear(parameters.at("gain", fraction, 0.0)) *
                                parameters.at("linear", fraction, 1.0);
            for (int channel = 0; channel < channels; ++channel) {
                buffer[static_cast<std::size_t>(frame) * channels + channel] *=
                    static_cast<float>(gain);
            }
        }
    }
};

class PanProcessor final : public AudioProcessor {
public:
    void prepare(int, int) override { reset(); }
    void reset() override {}
    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float*) override {
        if (channels < 2) {
            return;
        }
        for (int frame = 0; frame < frames; ++frame) {
            const double fraction = frames > 1 ? static_cast<double>(frame) / frames : 0.0;
            const double pan = std::clamp(parameters.at("pan", fraction, 0.0), -1.0, 1.0);
            const double leftGain = pan <= 0.0 ? 1.0 : 1.0 - pan;
            const double rightGain = pan >= 0.0 ? 1.0 : 1.0 + pan;
            buffer[static_cast<std::size_t>(frame) * channels] *= static_cast<float>(leftGain);
            buffer[static_cast<std::size_t>(frame) * channels + 1] *=
                static_cast<float>(rightGain);
        }
    }
};

struct BiquadState {
    double x1 = 0.0;
    double x2 = 0.0;
    double y1 = 0.0;
    double y2 = 0.0;
};

class BiquadProcessor final : public AudioProcessor {
public:
    explicit BiquadProcessor(std::string mode) : mode_(std::move(mode)) {}

    void prepare(int sampleRate, int channels) override {
        sampleRate_ = sampleRate;
        channels_ = channels;
        states_.assign(static_cast<std::size_t>(channels), BiquadState{});
        smoothedFrequency_ = 1000.0;
        reset();
    }

    void reset() override {
        for (BiquadState& state : states_) {
            state = BiquadState{};
        }
        z1_ = z2_ = 0.0;
        smoothedFrequency_ = 1000.0;
    }

    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float*) override {
        const double smoothing = std::exp(-1.0 / (0.005 * sampleRate_));
        for (int frame = 0; frame < frames; ++frame) {
            const double fraction = frames > 1 ? static_cast<double>(frame) / frames : 0.0;
            const double targetFrequency =
                std::clamp(parameters.at("frequency", fraction, 1000.0), 20.0,
                           sampleRate_ * 0.45);
            smoothedFrequency_ =
                smoothing * smoothedFrequency_ + (1.0 - smoothing) * targetFrequency;
            updateCoefficients(smoothedFrequency_, parameters.at("q", fraction, 0.707),
                               parameters.at("gain", fraction, 0.0));

            for (int channel = 0; channel < channels; ++channel) {
                const std::size_t index =
                    static_cast<std::size_t>(frame) * channels + channel;
                const double input = buffer[index];
                const double output = b0_ * input + b1_ * states_[channel].x1 +
                                      b2_ * states_[channel].x2 - a1_ * states_[channel].y1 -
                                      a2_ * states_[channel].y2;
                states_[channel].x2 = states_[channel].x1;
                states_[channel].x1 = input;
                states_[channel].y2 = states_[channel].y1;
                states_[channel].y1 = output;
                buffer[index] = static_cast<float>(output);
            }
        }
    }

    double tailSeconds() const override { return 0.05; }

private:
    void updateCoefficients(double frequency, double q, double gainDb) {
        const double w0 = 2.0 * kPi * frequency / sampleRate_;
        const double cosine = std::cos(w0);
        const double sine = std::sin(w0);
        const double alpha = sine / (2.0 * std::max(0.05, q));
        const double a = std::pow(10.0, gainDb / 40.0);

        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a0 = 1.0;
        double a1 = 0.0;
        double a2 = 0.0;

        if (mode_ == "highpass") {
            b0 = (1.0 + cosine) / 2.0;
            b1 = -(1.0 + cosine);
            b2 = (1.0 + cosine) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosine;
            a2 = 1.0 - alpha;
        } else if (mode_ == "peaking") {
            b0 = 1.0 + alpha * a;
            b1 = -2.0 * cosine;
            b2 = 1.0 - alpha * a;
            a0 = 1.0 + alpha / a;
            a1 = -2.0 * cosine;
            a2 = 1.0 - alpha / a;
        } else if (mode_ == "lowshelf") {
            const double beta = 2.0 * std::sqrt(a) * alpha;
            b0 = a * ((a + 1.0) - (a - 1.0) * cosine + beta);
            b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosine);
            b2 = a * ((a + 1.0) - (a - 1.0) * cosine - beta);
            a0 = (a + 1.0) + (a - 1.0) * cosine + beta;
            a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cosine);
            a2 = (a + 1.0) + (a - 1.0) * cosine - beta;
        } else if (mode_ == "highshelf") {
            const double beta = 2.0 * std::sqrt(a) * alpha;
            b0 = a * ((a + 1.0) + (a - 1.0) * cosine + beta);
            b1 = -2.0 * a * ((a - 1.0) + (a + 1.0) * cosine);
            b2 = a * ((a + 1.0) + (a - 1.0) * cosine - beta);
            a0 = (a + 1.0) - (a - 1.0) * cosine + beta;
            a1 = 2.0 * ((a - 1.0) - (a + 1.0) * cosine);
            a2 = (a + 1.0) - (a - 1.0) * cosine - beta;
        } else {
            b0 = (1.0 - cosine) / 2.0;
            b1 = 1.0 - cosine;
            b2 = (1.0 - cosine) / 2.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosine;
            a2 = 1.0 - alpha;
        }

        b0_ = b0 / a0;
        b1_ = b1 / a0;
        b2_ = b2 / a0;
        a1_ = a1 / a0;
        a2_ = a2 / a0;
    }

    std::string mode_;
    int sampleRate_ = 48000;
    int channels_ = 2;
    std::vector<BiquadState> states_;
    double b0_ = 1.0;
    double b1_ = 0.0;
    double b2_ = 0.0;
    double a1_ = 0.0;
    double a2_ = 0.0;
    double z1_ = 0.0;
    double z2_ = 0.0;
    double smoothedFrequency_ = 1000.0;
};

class CompressorProcessor final : public AudioProcessor {
public:
    void prepare(int sampleRate, int) override {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() override { envelope_ = 0.0; }

    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float* sidechain) override {
        const double attack = std::max(0.0001, parameters.at("attack", 1.0, 0.01));
        const double release = std::max(0.001, parameters.at("release", 1.0, 0.12));
        const double attackCoefficient = std::exp(-1.0 / (attack * sampleRate_));
        const double releaseCoefficient = std::exp(-1.0 / (release * sampleRate_));

        for (int frame = 0; frame < frames; ++frame) {
            const double fraction = frames > 1 ? static_cast<double>(frame) / frames : 0.0;
            const double thresholdDb = parameters.at("threshold", fraction, -18.0);
            const double ratio = std::max(1.0, parameters.at("ratio", fraction, 4.0));
            const double makeup = parameters.at("makeup", fraction, 0.0);
            double detector = 0.0;
            for (int channel = 0; channel < channels; ++channel) {
                detector = std::max(
                    detector, std::fabs(static_cast<double>(
                                  buffer[static_cast<std::size_t>(frame) * channels + channel])));
            }
            if (sidechain != nullptr) {
                for (int channel = 0; channel < channels; ++channel) {
                    detector = std::max(
                        detector,
                        std::fabs(static_cast<double>(
                            sidechain[static_cast<std::size_t>(frame) * channels + channel])));
                }
            }
            const double coefficient =
                detector > envelope_ ? attackCoefficient : releaseCoefficient;
            envelope_ = coefficient * envelope_ + (1.0 - coefficient) * detector;

            double gain = 1.0;
            if (envelope_ > 0.0000001) {
                const double inputDb = 20.0 * std::log10(envelope_);
                if (inputDb > thresholdDb) {
                    const double outputDb = thresholdDb + (inputDb - thresholdDb) / ratio;
                    gain = dbToLinear(outputDb - inputDb + makeup);
                } else {
                    gain = dbToLinear(makeup);
                }
            }
            for (int channel = 0; channel < channels; ++channel) {
                buffer[static_cast<std::size_t>(frame) * channels + channel] *=
                    static_cast<float>(gain);
            }
        }
    }

    double tailSeconds() const override { return 0.2; }

private:
    int sampleRate_ = 48000;
    double envelope_ = 0.0;
};

class DelayProcessor final : public AudioProcessor {
public:
    void prepare(int sampleRate, int channels) override {
        sampleRate_ = sampleRate;
        channels_ = channels;
        const std::size_t size =
            static_cast<std::size_t>(std::ceil(2.5 * sampleRate)) + 2;
        buffers_.assign(static_cast<std::size_t>(channels), std::vector<float>(size, 0.0f));
        writeIndex_ = 0;
        reset();
    }

    void reset() override {
        for (std::vector<float>& channel : buffers_) {
            std::fill(channel.begin(), channel.end(), 0.0f);
        }
        writeIndex_ = 0;
    }

    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float*) override {
        if (buffers_.empty() || channels != channels_) {
            return;
        }

        for (int frame = 0; frame < frames; ++frame) {
            const double fraction = frames > 1 ? static_cast<double>(frame) / frames : 0.0;
            const double time = std::max(0.0, parameters.at("time", fraction, 0.25));
            const double feedback =
                std::clamp(parameters.at("feedback", fraction, 0.25), 0.0, 0.95);
            const double mix = std::clamp(parameters.at("mix", fraction, 0.2), 0.0, 1.0);
            const double delaySamples =
                std::clamp(time * sampleRate_, 1.0,
                           static_cast<double>(buffers_.front().size() - 2));
            const double readPosition =
                static_cast<double>(writeIndex_) - delaySamples;
            double wrapped = std::fmod(readPosition, static_cast<double>(buffers_.front().size()));
            if (wrapped < 0.0) {
                wrapped += static_cast<double>(buffers_.front().size());
            }
            const std::size_t index0 = static_cast<std::size_t>(wrapped);
            const std::size_t index1 = (index0 + 1) % buffers_.front().size();
            const double readFraction = wrapped - static_cast<double>(index0);

            for (int channel = 0; channel < channels; ++channel) {
                float& slot = buffers_[static_cast<std::size_t>(channel)][writeIndex_];
                const double delayed =
                    buffers_[static_cast<std::size_t>(channel)][index0] * (1.0 - readFraction) +
                    buffers_[static_cast<std::size_t>(channel)][index1] * readFraction;
                const double dry =
                    buffer[static_cast<std::size_t>(frame) * channels + channel];
                slot = static_cast<float>(dry + delayed * feedback);
                buffer[static_cast<std::size_t>(frame) * channels + channel] =
                    static_cast<float>(dry * (1.0 - mix) + delayed * mix);
            }
            writeIndex_ = (writeIndex_ + 1) % buffers_.front().size();
        }
    }

    double tailSeconds() const override { return 2.5; }

private:
    int sampleRate_ = 48000;
    int channels_ = 2;
    std::vector<std::vector<float>> buffers_;
    std::size_t writeIndex_ = 0;
};

class ReverbProcessor final : public AudioProcessor {
public:
    void prepare(int sampleRate, int channels) override {
        sampleRate_ = sampleRate;
        channels_ = std::max(1, channels);
        combDelays_ = {1116, 1188, 1277, 1356};
        allpassDelays_ = {556, 441};
        const double scale = static_cast<double>(sampleRate) / 44100.0;
        combBuffers_.assign(combDelays_.size(), {});
        combIndices_.assign(combDelays_.size(), 0);
        combDamping_.assign(combDelays_.size(), 0.0);
        for (std::size_t index = 0; index < combDelays_.size(); ++index) {
            const std::size_t size =
                static_cast<std::size_t>(std::max(4.0, combDelays_[index] * scale));
            combBuffers_[index] = std::vector<float>(size, 0.0f);
        }
        allpassBuffers_.assign(allpassDelays_.size(), {});
        allpassIndices_.assign(allpassDelays_.size(), 0);
        for (std::size_t index = 0; index < allpassDelays_.size(); ++index) {
            const std::size_t size =
                static_cast<std::size_t>(std::max(4.0, allpassDelays_[index] * scale));
            allpassBuffers_[index] = std::vector<float>(size, 0.0f);
        }
        reset();
    }

    void reset() override {
        for (std::vector<float>& buffer : combBuffers_) {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
        }
        for (std::vector<float>& buffer : allpassBuffers_) {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
        }
        std::fill(combIndices_.begin(), combIndices_.end(), 0);
        std::fill(allpassIndices_.begin(), allpassIndices_.end(), 0);
        std::fill(combDamping_.begin(), combDamping_.end(), 0.0);
    }

    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float*) override {
        for (int frame = 0; frame < frames; ++frame) {
            const double fraction = frames > 1 ? static_cast<double>(frame) / frames : 0.0;
            const double room = std::clamp(parameters.at("room", fraction, 0.6), 0.0, 1.0);
            const double damping =
                std::clamp(parameters.at("damping", fraction, 0.4), 0.0, 1.0);
            const double mix = std::clamp(parameters.at("mix", fraction, 0.3), 0.0, 1.0);
            const double feedback = 0.7 + room * 0.28;

            const float inputL = buffer[static_cast<std::size_t>(frame) * channels];
            const float inputR = channels > 1
                                     ? buffer[static_cast<std::size_t>(frame) * channels + 1]
                                     : inputL;
            const float input = (inputL + inputR) * 0.5f;
            double wet = 0.0;
            for (std::size_t index = 0; index < combBuffers_.size(); ++index) {
                std::vector<float>& line = combBuffers_[index];
                const std::size_t position = combIndices_[index];
                const double delayed = line[position];
                combDamping_[index] = delayed * (1.0 - damping) + combDamping_[index] * damping;
                line[position] = static_cast<float>(input + combDamping_[index] * feedback);
                combIndices_[index] = (position + 1) % line.size();
                wet += delayed;
            }
            wet /= static_cast<double>(combBuffers_.size());

            for (std::size_t index = 0; index < allpassBuffers_.size(); ++index) {
                std::vector<float>& line = allpassBuffers_[index];
                const std::size_t position = allpassIndices_[index];
                const double delayed = line[position];
                const double output = -wet + delayed;
                line[position] = static_cast<float>(wet + delayed * 0.5);
                allpassIndices_[index] = (position + 1) % line.size();
                wet = output;
            }

            buffer[static_cast<std::size_t>(frame) * channels] =
                static_cast<float>(inputL * (1.0 - mix) + wet * mix);
            if (channels > 1) {
                buffer[static_cast<std::size_t>(frame) * channels + 1] =
                    static_cast<float>(inputR * (1.0 - mix) + wet * mix);
            }
        }
    }

    double tailSeconds() const override { return 5.0; }

private:
    int sampleRate_ = 48000;
    int channels_ = 2;
    std::vector<int> combDelays_;
    std::vector<int> allpassDelays_;
    std::vector<std::vector<float>> combBuffers_;
    std::vector<std::size_t> combIndices_;
    std::vector<double> combDamping_;
    std::vector<std::vector<float>> allpassBuffers_;
    std::vector<std::size_t> allpassIndices_;
};

class LimiterProcessor final : public AudioProcessor {
public:
    void prepare(int sampleRate, int channels) override {
        sampleRate_ = sampleRate;
        channels_ = std::max(1, channels);
        const std::size_t maximum =
            static_cast<std::size_t>(std::ceil(0.05 * sampleRate)) + 2;
        delay_.assign(static_cast<std::size_t>(channels_), std::vector<float>(maximum, 0.0f));
        reset();
    }

    void reset() override {
        for (std::vector<float>& channel : delay_) {
            std::fill(channel.begin(), channel.end(), 0.0f);
        }
        writeIndex_ = 0;
        delayLength_ = 0;
        gain_ = 1.0;
        minimumQueue_.clear();
    }

    void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                 const float*) override {
        if (delay_.empty() || channels != channels_) {
            return;
        }
        const double ceilingDb =
            std::clamp(parameters.at("ceiling", 1.0, -1.0), -24.0, 0.0);
        const double ceiling = dbToLinear(ceilingDb);
        const double attack =
            std::clamp(parameters.at("attack", 0.0, 0.005), 0.001, 0.05);
        const double release = std::max(0.01, parameters.at("release", 1.0, 0.1));
        latencySeconds_ = attack;

        const std::size_t length = static_cast<std::size_t>(
            std::max<long long>(1, std::llround(attack * sampleRate_)));
        if (length != delayLength_) {
            for (std::vector<float>& channel : delay_) {
                std::fill(channel.begin(), channel.end(), 0.0f);
            }
            writeIndex_ = 0;
            delayLength_ = length;
            minimumQueue_.clear();
        }
        if (delay_.front().size() < length) {
            return;
        }

        const double releaseCoefficient = std::exp(-1.0 / (release * sampleRate_));
        const double attackCoefficient =
            1.0 - std::exp(-1.0 / (std::max(0.0001, attack) * sampleRate_));
        for (int frame = 0; frame < frames; ++frame) {
            double peak = 0.0;
            for (int channel = 0; channel < channels; ++channel) {
                peak = std::max(
                    peak,
                    std::fabs(static_cast<double>(
                        buffer[static_cast<std::size_t>(frame) * channels + channel])));
            }
            const double desired = peak > ceiling && peak > 0.0 ? ceiling / peak : 1.0;

            const long long position =
                static_cast<long long>(writeIndex_) + totalFrames_;
            while (!minimumQueue_.empty() && minimumQueue_.back().second >= desired) {
                minimumQueue_.pop_back();
            }
            minimumQueue_.emplace_back(position, desired);
            const long long oldest =
                position - static_cast<long long>(delayLength_);
            while (!minimumQueue_.empty() && minimumQueue_.front().first < oldest) {
                minimumQueue_.pop_front();
            }

            const double target = minimumQueue_.front().second;
            const double coefficient =
                target < gain_ ? attackCoefficient : 1.0 - releaseCoefficient;
            gain_ += (target - gain_) * coefficient;

            for (int channel = 0; channel < channels; ++channel) {
                const std::size_t index =
                    static_cast<std::size_t>(frame) * channels + channel;
                float& slot = delay_[static_cast<std::size_t>(channel)][writeIndex_];
                const float delayed = slot;
                slot = buffer[index];
                const double limited = std::clamp(static_cast<double>(delayed) * gain_,
                                                  -ceiling, ceiling);
                buffer[index] = static_cast<float>(limited);
            }
            writeIndex_ = (writeIndex_ + 1) % delayLength_;
            ++totalFrames_;
        }
    }

    double latencySeconds() const override { return latencySeconds_; }

private:
    int sampleRate_ = 48000;
    int channels_ = 2;
    std::vector<std::vector<float>> delay_;
    std::size_t writeIndex_ = 0;
    std::size_t delayLength_ = 0;
    long long totalFrames_ = 0;
    double gain_ = 1.0;
    double latencySeconds_ = 0.0;
    std::deque<std::pair<long long, double>> minimumQueue_;
};

}  // namespace

std::unique_ptr<AudioProcessor> createProcessor(ProcessorKind kind, const std::string& mode) {
    switch (kind) {
        case ProcessorKind::Gain: return std::make_unique<GainProcessor>();
        case ProcessorKind::Pan: return std::make_unique<PanProcessor>();
        case ProcessorKind::Biquad: return std::make_unique<BiquadProcessor>(mode);
        case ProcessorKind::Compressor: return std::make_unique<CompressorProcessor>();
        case ProcessorKind::Delay: return std::make_unique<DelayProcessor>();
        case ProcessorKind::Reverb: return std::make_unique<ReverbProcessor>();
        case ProcessorKind::Limiter: return std::make_unique<LimiterProcessor>();
    }
    return nullptr;
}

}  // namespace mpl
