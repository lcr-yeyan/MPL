#include "mpl/audio/ReferenceAudioBackend.hpp"

#include "mpl/audio/Processors.hpp"
#include "mpl/audio/WavFile.hpp"
#include "mpl/core/Error.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace mpl {
namespace {

constexpr double kPi = 3.14159265358979323846;

double wholeAtSeconds(const std::vector<TempoSegment>& tempo, double seconds) {
    const TempoSegment* segment = &tempo.front();
    for (const TempoSegment& candidate : tempo) {
        if (candidate.startSeconds <= seconds) {
            segment = &candidate;
        } else {
            break;
        }
    }
    return segment->startWholeNotes + (seconds - segment->startSeconds) * segment->bpm / 240.0;
}

struct ControlValues {
    double volume = 1.0;
    double pan = 0.0;
};

bool hasControl(const std::vector<ControlBindingPlan>& controls, const std::string& name) {
    for (const ControlBindingPlan& control : controls) {
        if (control.parameter == name) {
            return true;
        }
    }
    return false;
}

ControlValues evaluateControls(const std::vector<ControlBindingPlan>& controls,
                               const TemporalIR& temporal, double wholeNotes) {
    ControlValues values;
    bool volumeReplace = false;
    bool panReplace = false;
    double volumeValue = 0.0;
    double panValue = 0.0;
    double volumeAdd = 0.0;
    double panAdd = 0.0;
    double volumeMultiply = 1.0;
    double panMultiply = 1.0;

    for (const ControlBindingPlan& control : controls) {
        if (control.curve == kInvalidTemporalId) {
            continue;
        }
        const double value = temporal.sampleDouble(control.curve, wholeNotes);
        if (control.parameter == "volume") {
            switch (control.mode) {
                case AutomationMode::Replace:
                    volumeReplace = true;
                    volumeValue = value;
                    break;
                case AutomationMode::Add:
                    volumeAdd += value;
                    break;
                case AutomationMode::Multiply:
                    volumeMultiply *= value;
                    break;
            }
        } else if (control.parameter == "pan") {
            switch (control.mode) {
                case AutomationMode::Replace:
                    panReplace = true;
                    panValue = value;
                    break;
                case AutomationMode::Add:
                    panAdd += value;
                    break;
                case AutomationMode::Multiply:
                    panMultiply *= value;
                    break;
            }
        }
    }

    values.volume = (volumeReplace ? volumeValue : 1.0) + volumeAdd;
    values.volume *= volumeMultiply;
    values.pan = (panReplace ? panValue : 0.0) + panAdd;
    values.pan *= panMultiply;
    values.volume = std::clamp(values.volume, 0.0, 2.0);
    values.pan = std::clamp(values.pan, -1.0, 1.0);
    return values;
}

double evaluateVolumeDb(double value) {
    return value <= 0.000001 ? -96.0 : 20.0 * std::log10(value);
}

ParameterRamp buildParameterRamp(const SignalNodePlan& node, const AudioExecutionPlan& plan,
                                 const TemporalIR& temporal, double wholeStart,
                                 double wholeEnd) {
    std::map<std::string, std::pair<double, double>> values;
    for (const auto& parameter : node.processor.parameters) {
        values[parameter.first] = {parameter.second, parameter.second};
    }

    const TrackPlan* track = node.trackIndex >= 0 &&
                                     node.trackIndex < static_cast<int>(plan.tracks.size())
                                 ? &plan.tracks[static_cast<std::size_t>(node.trackIndex)]
                                 : nullptr;
    if (node.trackVolume && track != nullptr && hasControl(track->controls, "volume")) {
        const ControlValues start = evaluateControls(track->controls, temporal, wholeStart);
        const ControlValues end = evaluateControls(track->controls, temporal, wholeEnd);
        values["linear"] = {start.volume, end.volume};
    }
    if (node.trackPan && track != nullptr && hasControl(track->controls, "pan")) {
        const ControlValues start = evaluateControls(track->controls, temporal, wholeStart);
        const ControlValues end = evaluateControls(track->controls, temporal, wholeEnd);
        values["pan"] = {start.pan, end.pan};
    }
    if (node.masterVolume && hasControl(plan.masterControls, "volume")) {
        const ControlValues start = evaluateControls(plan.masterControls, temporal, wholeStart);
        const ControlValues end = evaluateControls(plan.masterControls, temporal, wholeEnd);
        values["linear"] = {start.volume, end.volume};
    }
    if (node.masterPan && hasControl(plan.masterControls, "pan")) {
        const ControlValues start = evaluateControls(plan.masterControls, temporal, wholeStart);
        const ControlValues end = evaluateControls(plan.masterControls, temporal, wholeEnd);
        values["pan"] = {start.pan, end.pan};
    }

    for (const ProcessorAutomationPlan& automation : node.automation) {
        auto& value = values[automation.parameter];
        const double start = temporal.sampleDouble(automation.curve, wholeStart);
        const double end = temporal.sampleDouble(automation.curve, wholeEnd);
        switch (automation.mode) {
            case AutomationMode::Replace:
                value.first = start;
                value.second = end;
                break;
            case AutomationMode::Add:
                value.first += start;
                value.second += end;
                break;
            case AutomationMode::Multiply:
                value.first *= start;
                value.second *= end;
                break;
        }
        value.first = std::clamp(value.first, automation.minimum, automation.maximum);
        value.second = std::clamp(value.second, automation.minimum, automation.maximum);
    }

    ParameterRamp ramp;
    for (const auto& value : values) {
        ramp.set(value.first, value.second.first, value.second.second);
    }
    return ramp;
}

struct Voice {
    const TrackPlan* track = nullptr;
    const PlannedNote* note = nullptr;
    std::uint32_t noiseState = 0x12345678u;
    double phase = 0.0;
    double samplePosition = 0.0;
    bool sampleDone = false;
    bool releasing = false;
    double releaseStartLevel = 0.0;
    double releaseElapsed = 0.0;
};

double oscillatorSample(Oscillator oscillator, double phase, std::uint32_t& noiseState) {
    switch (oscillator) {
        case Oscillator::Sine:
            return std::sin(2.0 * kPi * phase);
        case Oscillator::Triangle:
            return 2.0 * std::fabs(2.0 * phase - 1.0) - 1.0;
        case Oscillator::Saw:
            return 2.0 * phase - 1.0;
        case Oscillator::Square:
            return phase < 0.5 ? 1.0 : -1.0;
        case Oscillator::Noise:
            noiseState = noiseState * 1664525u + 1013904223u;
            return static_cast<double>(noiseState) / 2147483648.0 - 1.0;
    }
    return 0.0;
}

double envelopeLevel(const AudioExecutionPlan& plan, const Voice& voice, int globalSample) {
    const AdsrEnvelope& envelope = voice.track->instrument.envelope;
    const double sampleRate = static_cast<double>(plan.config.sampleRate);
    const double elapsed =
        std::max(0.0, static_cast<double>(globalSample - voice.note->startSample) / sampleRate);
    if (voice.releasing) {
        if (envelope.release <= 0.0) {
            return 0.0;
        }
        return voice.releaseStartLevel *
               std::max(0.0, 1.0 - voice.releaseElapsed / envelope.release);
    }
    if (envelope.attack > 0.0 && elapsed < envelope.attack) {
        return elapsed / envelope.attack;
    }
    const double afterAttack = elapsed - envelope.attack;
    if (envelope.decay > 0.0 && afterAttack < envelope.decay) {
        return 1.0 - (1.0 - envelope.sustain) * (afterAttack / envelope.decay);
    }
    return envelope.sustain;
}

int addDefaultNode(SignalGraphPlan& graph, SignalNodeKind kind, std::string name,
                   int inputChannels, int outputChannels) {
    SignalNodePlan node;
    node.id = static_cast<int>(graph.nodes.size());
    node.kind = kind;
    node.name = std::move(name);
    node.inputChannels = inputChannels;
    node.outputChannels = outputChannels;
    graph.nodes.push_back(std::move(node));
    return static_cast<int>(graph.nodes.size() - 1);
}

void buildDefaultGraph(AudioExecutionPlan& plan) {
    SignalGraphPlan graph;
    const int channels = plan.config.channels;
    std::vector<int> sourceNodes;
    for (const TrackPlan& track : plan.tracks) {
        const int source = addDefaultNode(graph, SignalNodeKind::Source, track.name + " Source",
                                          0, channels);
        graph.nodes[static_cast<std::size_t>(source)].trackIndex =
            static_cast<int>(sourceNodes.size());
        sourceNodes.push_back(source);
    }

    std::vector<int> trackOutputs;
    for (std::size_t index = 0; index < plan.tracks.size(); ++index) {
        SignalNodePlan gain;
        gain.id = static_cast<int>(graph.nodes.size());
        gain.kind = SignalNodeKind::Processor;
        gain.name = plan.tracks[index].name + " Gain";
        gain.role = StripRole::Track;
        gain.trackIndex = static_cast<int>(index);
        gain.trackVolume = true;
        gain.processor.kind = ProcessorKind::Gain;
        gain.processor.parameters["gain"] = 0.0;
        gain.inputChannels = channels;
        gain.outputChannels = channels;
        const int gainId = static_cast<int>(graph.nodes.size());
        graph.nodes.push_back(std::move(gain));

        SignalNodePlan pan;
        pan.id = static_cast<int>(graph.nodes.size());
        pan.kind = SignalNodeKind::Processor;
        pan.name = plan.tracks[index].name + " Pan";
        pan.role = StripRole::Track;
        pan.trackIndex = static_cast<int>(index);
        pan.trackPan = true;
        pan.processor.kind = ProcessorKind::Pan;
        pan.processor.parameters["pan"] = 0.0;
        pan.inputChannels = channels;
        pan.outputChannels = channels;
        const int panId = static_cast<int>(graph.nodes.size());
        graph.nodes.push_back(std::move(pan));

        graph.connections.push_back(SignalConnectionPlan{sourceNodes[index], gainId, 1.0, false});
        graph.connections.push_back(SignalConnectionPlan{gainId, panId, 1.0, false});
        trackOutputs.push_back(panId);
    }

    const int masterMixer = addDefaultNode(graph, SignalNodeKind::Mixer, "Master Mixer", channels,
                                           channels);
    for (const int trackOutput : trackOutputs) {
        graph.connections.push_back(
            SignalConnectionPlan{trackOutput, masterMixer, 1.0, false});
    }

    SignalNodePlan masterGain;
    masterGain.id = static_cast<int>(graph.nodes.size());
    masterGain.kind = SignalNodeKind::Processor;
    masterGain.name = "Master Gain";
    masterGain.role = StripRole::Master;
    masterGain.masterVolume = true;
    masterGain.processor.kind = ProcessorKind::Gain;
    masterGain.processor.parameters["gain"] = 0.0;
    masterGain.inputChannels = channels;
    masterGain.outputChannels = channels;
    const int masterGainId = static_cast<int>(graph.nodes.size());
    graph.nodes.push_back(std::move(masterGain));

    SignalNodePlan masterPan;
    masterPan.id = static_cast<int>(graph.nodes.size());
    masterPan.kind = SignalNodeKind::Processor;
    masterPan.name = "Master Pan";
    masterPan.role = StripRole::Master;
    masterPan.masterPan = true;
    masterPan.processor.kind = ProcessorKind::Pan;
    masterPan.processor.parameters["pan"] = 0.0;
    masterPan.inputChannels = channels;
    masterPan.outputChannels = channels;
    const int masterPanId = static_cast<int>(graph.nodes.size());
    graph.nodes.push_back(std::move(masterPan));

    const int output = addDefaultNode(graph, SignalNodeKind::Output, "Output", channels, 0);
    graph.connections.push_back(SignalConnectionPlan{masterMixer, masterGainId, 1.0, false});
    graph.connections.push_back(SignalConnectionPlan{masterGainId, masterPanId, 1.0, false});
    graph.connections.push_back(SignalConnectionPlan{masterPanId, output, 1.0, false});
    graph.outputNode = output;
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        graph.order.push_back(static_cast<int>(index));
    }
    plan.graph = std::move(graph);
}

struct SourceRuntime {
    std::size_t nextNote = 0;
    std::vector<Voice> voices;
};

struct NodeRuntime {
    const SignalNodePlan* node = nullptr;
    std::vector<float> buffer;
    SourceRuntime source;
    std::unique_ptr<AudioProcessor> processor;
};

}  // namespace

ReferenceAudioBackend::ReferenceAudioBackend(AudioRenderConfig config) : config_(config) {}

BackendCapabilities ReferenceAudioBackend::capabilities() const {
    return BackendCapabilities{true,
                               true,
                               true,
                               true,
                               true,
                               2,
                               {ProcessorKind::Gain, ProcessorKind::Pan, ProcessorKind::Biquad,
                                ProcessorKind::Compressor, ProcessorKind::Delay,
                                ProcessorKind::Reverb}};
}

AudioBuffer ReferenceAudioBackend::renderPlan(const AudioExecutionPlan& originalPlan,
                                              const TemporalIR& temporal,
                                              std::vector<Diagnostic>& diagnostics) const {
    AudioExecutionPlan plan = originalPlan;
    if (plan.graph.nodes.empty()) {
        buildDefaultGraph(plan);
    }
    (void)checkBackendCapabilities(capabilities(), plan.graph, plan.config.policy, diagnostics);

    AudioBuffer buffer;
    buffer.sampleRate = plan.config.sampleRate;
    buffer.channels = plan.config.channels;
    buffer.resizeFrames(static_cast<std::size_t>(std::max(0, plan.totalFrames)));
    if (plan.totalFrames <= 0 || plan.tempo.empty()) {
        return buffer;
    }

    const int channels = plan.config.channels;
    std::vector<std::vector<const SignalConnectionPlan*>> incoming(plan.graph.nodes.size());
    std::vector<const SignalConnectionPlan*> sidechainInput(plan.graph.nodes.size(), nullptr);
    for (const SignalConnectionPlan& connection : plan.graph.connections) {
        if (connection.to < 0 || connection.to >= static_cast<int>(plan.graph.nodes.size())) {
            continue;
        }
        if (connection.sidechain) {
            sidechainInput[static_cast<std::size_t>(connection.to)] = &connection;
        } else {
            incoming[static_cast<std::size_t>(connection.to)].push_back(&connection);
        }
    }

    std::vector<NodeRuntime> runtimes(plan.graph.nodes.size());
    for (std::size_t index = 0; index < plan.graph.nodes.size(); ++index) {
        runtimes[index].node = &plan.graph.nodes[index];
        runtimes[index].buffer.assign(static_cast<std::size_t>(plan.config.blockSize) * channels,
                                      0.0f);
        if (plan.graph.nodes[index].kind == SignalNodeKind::Processor) {
            runtimes[index].processor =
                createProcessor(plan.graph.nodes[index].processor.kind,
                                plan.graph.nodes[index].processor.mode);
            if (runtimes[index].processor != nullptr) {
                runtimes[index].processor->prepare(plan.config.sampleRate, channels);
                runtimes[index].processor->reset();
            }
        }
    }

    std::vector<int> order = plan.graph.order;
    if (order.size() != plan.graph.nodes.size()) {
        order.clear();
        for (std::size_t index = 0; index < plan.graph.nodes.size(); ++index) {
            order.push_back(static_cast<int>(index));
        }
    }

    const int blockSize = std::max(1, plan.config.blockSize);
    for (int blockStart = 0; blockStart < plan.totalFrames; blockStart += blockSize) {
        const int blockEnd = std::min(plan.totalFrames, blockStart + blockSize);
        const int blockLength = blockEnd - blockStart;
        for (NodeRuntime& runtime : runtimes) {
            std::fill(runtime.buffer.begin(),
                      runtime.buffer.begin() + static_cast<std::size_t>(blockLength) * channels,
                      0.0f);
        }

        const double wholeStart =
            wholeAtSeconds(plan.tempo, static_cast<double>(blockStart) / plan.config.sampleRate);
        const double wholeEnd =
            wholeAtSeconds(plan.tempo, static_cast<double>(blockEnd) / plan.config.sampleRate);

        for (const int nodeId : order) {
            NodeRuntime& runtime = runtimes[static_cast<std::size_t>(nodeId)];
            const SignalNodePlan& node = *runtime.node;
            if (node.kind == SignalNodeKind::Source) {
                const TrackPlan& track = plan.tracks[static_cast<std::size_t>(node.trackIndex)];
                SourceRuntime& source = runtime.source;
                while (source.nextNote < track.notes.size() &&
                       track.notes[source.nextNote].startSample < blockEnd) {
                    Voice voice;
                    voice.track = &track;
                    voice.note = &track.notes[source.nextNote];
                    voice.noiseState =
                        0x9E3779B9u ^
                        static_cast<std::uint32_t>(voice.note->midi * 2654435761u) ^
                        static_cast<std::uint32_t>(voice.note->startSample);
                    source.voices.push_back(voice);
                    ++source.nextNote;
                }

                for (int offset = 0; offset < blockLength; ++offset) {
                    const int globalSample = blockStart + offset;
                    float left = 0.0f;
                    float right = 0.0f;
                    for (Voice& voice : source.voices) {
                        if (globalSample < voice.note->startSample) {
                            continue;
                        }
                        if (!voice.releasing && globalSample >= voice.note->endSample) {
                            const double level = envelopeLevel(plan, voice, globalSample);
                            voice.releasing = true;
                            voice.releaseStartLevel = level;
                            voice.releaseElapsed = 0.0;
                        }
                        const double envelope = envelopeLevel(plan, voice, globalSample);
                        if (voice.releasing &&
                            (voice.track->instrument.envelope.release <= 0.0 ||
                             voice.releaseElapsed >= voice.track->instrument.envelope.release)) {
                            continue;
                        }

                        double sampleValue = 0.0;
                        if (voice.track->instrument.sample != nullptr) {
                            const AudioBuffer& sample = *voice.track->instrument.sample;
                            if (sample.frameCount() > 0 && sample.channels > 0 &&
                                !voice.sampleDone) {
                                const std::size_t frame =
                                    static_cast<std::size_t>(voice.samplePosition);
                                if (frame >= sample.frameCount()) {
                                    voice.sampleDone = true;
                                } else {
                                    double mono = 0.0;
                                    for (int channel = 0; channel < sample.channels; ++channel) {
                                        mono += sample.samples[
                                            frame * static_cast<std::size_t>(sample.channels) +
                                            static_cast<std::size_t>(channel)];
                                    }
                                    sampleValue = mono / sample.channels;
                                    const double ratio =
                                        std::pow(2.0,
                                                 (static_cast<double>(voice.note->midi) -
                                                  voice.track->instrument.sampleRootMidi) /
                                                     12.0);
                                    voice.samplePosition +=
                                        ratio * sample.sampleRate / plan.config.sampleRate;
                                }
                            } else {
                                voice.sampleDone = true;
                            }
                        } else {
                            sampleValue =
                                oscillatorSample(voice.track->instrument.oscillator, voice.phase,
                                                 voice.noiseState);
                            voice.phase +=
                                midiToFrequency(voice.note->midi) / plan.config.sampleRate;
                            voice.phase -= std::floor(voice.phase);
                        }

                        const double pan =
                            std::clamp(voice.note->hasPan ? voice.note->pan : 0.0, -1.0, 1.0);
                        const double angle = (pan + 1.0) * 0.25 * kPi;
                        const double gain = envelope * voice.note->velocity *
                                            voice.track->instrument.gain;
                        left += static_cast<float>(sampleValue * gain * std::cos(angle));
                        right += static_cast<float>(sampleValue * gain * std::sin(angle));
                        if (voice.releasing) {
                            voice.releaseElapsed += 1.0 / plan.config.sampleRate;
                        }
                    }
                    if (channels == 1) {
                        runtime.buffer[static_cast<std::size_t>(offset)] =
                            (left + right) * 0.5f;
                    } else {
                        runtime.buffer[static_cast<std::size_t>(offset) * 2] = left;
                        runtime.buffer[static_cast<std::size_t>(offset) * 2 + 1] = right;
                    }
                }

                source.voices.erase(
                    std::remove_if(source.voices.begin(), source.voices.end(),
                                   [&](const Voice& voice) {
                                       if (!voice.releasing) {
                                           return false;
                                       }
                                       return voice.track->instrument.envelope.release <= 0.0 ||
                                              voice.releaseElapsed >=
                                                  voice.track->instrument.envelope.release ||
                                              (voice.track->instrument.sample != nullptr &&
                                               voice.sampleDone);
                                   }),
                    source.voices.end());
            } else if (node.kind == SignalNodeKind::Mixer) {
                for (const SignalConnectionPlan* connection : incoming[static_cast<std::size_t>(nodeId)]) {
                    const NodeRuntime& from =
                        runtimes[static_cast<std::size_t>(connection->from)];
                    for (int index = 0; index < blockLength * channels; ++index) {
                        runtime.buffer[static_cast<std::size_t>(index)] +=
                            static_cast<float>(from.buffer[static_cast<std::size_t>(index)] *
                                               connection->gain);
                    }
                }
            } else if (node.kind == SignalNodeKind::Processor) {
                if (!incoming[static_cast<std::size_t>(nodeId)].empty()) {
                    const NodeRuntime& from = runtimes[static_cast<std::size_t>(
                        incoming[static_cast<std::size_t>(nodeId)].front()->from)];
                    for (int index = 0; index < blockLength * channels; ++index) {
                        runtime.buffer[static_cast<std::size_t>(index)] =
                            from.buffer[static_cast<std::size_t>(index)];
                    }
                }
                const float* sidechain = nullptr;
                if (sidechainInput[static_cast<std::size_t>(nodeId)] != nullptr) {
                    sidechain = runtimes[static_cast<std::size_t>(
                                             sidechainInput[static_cast<std::size_t>(nodeId)]->from)]
                                    .buffer.data();
                }
                if (runtime.processor != nullptr) {
                    const ParameterRamp ramp =
                        buildParameterRamp(node, plan, temporal, wholeStart, wholeEnd);
                    runtime.processor->process(runtime.buffer.data(), blockLength, channels, ramp,
                                               sidechain);
                }
            } else if (node.kind == SignalNodeKind::Output) {
                if (!incoming[static_cast<std::size_t>(nodeId)].empty()) {
                    const NodeRuntime& from = runtimes[static_cast<std::size_t>(
                        incoming[static_cast<std::size_t>(nodeId)].front()->from)];
                    for (int index = 0; index < blockLength * channels; ++index) {
                        runtime.buffer[static_cast<std::size_t>(index)] =
                            from.buffer[static_cast<std::size_t>(index)];
                    }
                }
            }
        }

        const NodeRuntime& output = runtimes[static_cast<std::size_t>(plan.graph.outputNode)];
        for (int index = 0; index < blockLength * channels; ++index) {
            buffer.samples[static_cast<std::size_t>(blockStart) * channels + index] =
                output.buffer[static_cast<std::size_t>(index)];
        }
    }

    if (plan.config.termination == RenderTermination::SilenceThreshold) {
        const double threshold = std::pow(10.0, plan.config.silenceThresholdDb / 20.0);
        std::size_t minimumFrames = std::min(
            buffer.frameCount(),
            static_cast<std::size_t>(std::max(0.0, std::max(plan.logicalExtentSeconds,
                                                            plan.contentEndSeconds) *
                                                       plan.config.sampleRate)));
        std::size_t keep = buffer.frameCount();
        while (keep > minimumFrames) {
            const std::size_t frame = keep - 1;
            bool silent = true;
            for (int channel = 0; channel < channels; ++channel) {
                if (std::fabs(buffer.samples[frame * channels + channel]) >= threshold) {
                    silent = false;
                    break;
                }
            }
            if (!silent) {
                break;
            }
            --keep;
        }
        buffer.samples.resize(keep * static_cast<std::size_t>(channels));
    }

    return buffer;
}

AudioBuffer ReferenceAudioBackend::render(const ProductionIR& production,
                                          const TemporalIR& temporal,
                                          const AssetResolver& resolver,
                                          std::vector<Diagnostic>& diagnostics) const {
    const AudioExecutionPlan plan =
        buildAudioExecutionPlan(production, temporal, resolver, config_, diagnostics);
    return renderPlan(plan, temporal, diagnostics);
}

void ReferenceAudioBackend::writeWav(const AudioBuffer& buffer,
                                     const std::filesystem::path& path,
                                     std::vector<Diagnostic>& diagnostics) const {
    writeWavFile(path, buffer, diagnostics);
}

}  // namespace mpl
