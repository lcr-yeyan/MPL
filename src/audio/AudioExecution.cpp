#include "mpl/audio/AudioExecution.hpp"

#include "mpl/audio/WavFile.hpp"
#include "mpl/core/Error.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mpl {
namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

std::shared_ptr<AudioBuffer> makeDrumSample() {
    auto buffer = std::make_shared<AudioBuffer>();
    buffer->sampleRate = 48000;
    buffer->channels = 1;
    const int frames = 12000;
    buffer->samples.resize(static_cast<std::size_t>(frames));
    std::uint32_t state = 0x12345678u;
    for (int frame = 0; frame < frames; ++frame) {
        state = state * 1664525u + 1013904223u;
        const float noise = static_cast<float>(state) / 2147483648.0f - 1.0f;
        const double time = static_cast<double>(frame) / 48000.0;
        const double envelope = std::exp(-30.0 * time);
        buffer->samples[static_cast<std::size_t>(frame)] =
            noise * static_cast<float>(envelope) * 0.8f;
    }
    return buffer;
}

void addWarning(std::vector<Diagnostic>& diagnostics, std::string code, std::string message) {
    diagnostics.push_back(Diagnostic::warning(std::move(code), std::move(message)));
}

void addError(std::vector<Diagnostic>& diagnostics, std::string code, std::string message) {
    diagnostics.push_back(Diagnostic::error(std::move(code), std::move(message)));
}

}  // namespace

double midiToFrequency(int midi) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midi) - 69.0) / 12.0);
}

AudioClock::AudioClock(const std::vector<TempoEvent>& tempoMap) {    std::vector<TempoEvent> events = tempoMap;
    std::sort(events.begin(), events.end(), [](const TempoEvent& lhs, const TempoEvent& rhs) {
        return lhs.position < rhs.position;
    });
    if (events.empty() || events.front().position.inWholeNotes().toDouble() > 0.0) {
        events.insert(events.begin(), TempoEvent{Position{}, 120.0});
    }

    double previousWhole = 0.0;
    double previousBpm = 120.0;
    double seconds = 0.0;
    for (const TempoEvent& event : events) {
        const double whole = event.position.inWholeNotes().toDouble();
        if (!segments_.empty()) {
            seconds += (whole - previousWhole) * 240.0 / previousBpm;
        }
        segments_.push_back(TempoSegment{seconds, whole, event.bpm});
        previousWhole = whole;
        previousBpm = event.bpm;
    }
    if (segments_.empty()) {
        segments_.push_back(TempoSegment{0.0, 0.0, 120.0});
    }
}

double AudioClock::secondsAtWhole(double wholeNotes) const {
    const TempoSegment* segment = &segments_.front();
    for (const TempoSegment& candidate : segments_) {
        if (candidate.startWholeNotes <= wholeNotes) {
            segment = &candidate;
        } else {
            break;
        }
    }
    return segment->startSeconds + (wholeNotes - segment->startWholeNotes) * 240.0 / segment->bpm;
}

double AudioClock::wholeAtSeconds(double seconds) const {
    const TempoSegment* segment = &segments_.front();
    for (const TempoSegment& candidate : segments_) {
        if (candidate.startSeconds <= seconds) {
            segment = &candidate;
        } else {
            break;
        }
    }
    return segment->startWholeNotes + (seconds - segment->startSeconds) * segment->bpm / 240.0;
}

double AudioClock::secondsPerWhole(double wholeNotes) const {
    const TempoSegment* segment = &segments_.front();
    for (const TempoSegment& candidate : segments_) {
        if (candidate.startWholeNotes <= wholeNotes) {
            segment = &candidate;
        } else {
            break;
        }
    }
    return 240.0 / segment->bpm;
}

ReferenceAssetLibrary::ReferenceAssetLibrary(std::filesystem::path baseDirectory)
    : baseDirectory_(std::move(baseDirectory)) {}

std::optional<InstrumentAsset> ReferenceAssetLibrary::resolve(
    const TrackIR& track, const AssetBinding& binding,
    std::vector<Diagnostic>& diagnostics) const {
    if (binding.assetId.empty() || binding.provider.empty()) {
        addError(diagnostics, "audio-missing-asset",
                 "track '" + track.name + "' has an unresolved asset binding");
        return std::nullopt;
    }

    std::string assetId = binding.assetId;
    if (assetId.rfind("file:", 0) == 0) {
        assetId = assetId.substr(5);
    }
    const bool fileAsset = binding.provider == "file" || assetId.size() > 4 &&
                           lower(assetId.substr(assetId.size() - 4)) == ".wav";
    if (fileAsset) {
        std::filesystem::path path(assetId);
        if (path.is_relative()) {
            path = baseDirectory_ / path;
        }
        std::optional<AudioBuffer> sample = loadWavFile(path, diagnostics);
        if (!sample.has_value()) {
            return std::nullopt;
        }
        InstrumentAsset asset;
        asset.id = assetId;
        asset.version = "file";
        asset.oscillator = Oscillator::Sine;
        asset.envelope = AdsrEnvelope{0.001, 0.0, 1.0, 0.05};
        asset.gain = 0.8f;
        asset.sample = std::make_shared<AudioBuffer>(std::move(*sample));
        asset.sampleRootMidi = 60;
        return asset;
    }

    const std::string name = lower(binding.assetId + " " + track.instrument);
    InstrumentAsset asset;
    asset.id = binding.assetId;
    asset.version = "reference-1";
    if (name.find("drum") != std::string::npos || name.find("kick") != std::string::npos ||
        name.find("snare") != std::string::npos) {
        asset.oscillator = Oscillator::Noise;
        asset.envelope = AdsrEnvelope{0.001, 0.0, 1.0, 0.02};
        asset.gain = 0.8f;
        asset.sample = makeDrumSample();
        asset.sampleRootMidi = 36;
        return asset;
    }
    if (name.find("piano") != std::string::npos) {
        asset.oscillator = Oscillator::Triangle;
        asset.envelope = AdsrEnvelope{0.005, 0.15, 0.45, 0.35};
        asset.gain = 0.45f;
        return asset;
    }
    if (name.find("pad") != std::string::npos) {
        asset.oscillator = Oscillator::Saw;
        asset.envelope = AdsrEnvelope{0.25, 0.2, 0.75, 1.0};
        asset.gain = 0.22f;
        return asset;
    }
    if (name.find("bass") != std::string::npos) {
        asset.oscillator = Oscillator::Sine;
        asset.envelope = AdsrEnvelope{0.01, 0.1, 0.8, 0.2};
        asset.gain = 0.5f;
        return asset;
    }
    if (name.find("lead") != std::string::npos) {
        asset.oscillator = Oscillator::Square;
        asset.envelope = AdsrEnvelope{0.01, 0.05, 0.7, 0.25};
        asset.gain = 0.25f;
        return asset;
    }
    addError(diagnostics, "audio-unknown-asset",
             "no reference instrument for asset '" + binding.assetId + "'");
    return std::nullopt;
}

int addSignalNode(SignalGraphPlan& graph, SignalNodePlan node) {
    node.id = static_cast<int>(graph.nodes.size());
    graph.nodes.push_back(std::move(node));
    return graph.nodes.back().id;
}

void connectSignal(SignalGraphPlan& graph, int from, int to, double gain = 1.0,
                   bool sidechain = false) {
    SignalConnectionPlan connection;
    connection.from = from;
    connection.to = to;
    connection.gain = gain;
    connection.sidechain = sidechain;
    graph.connections.push_back(connection);
}

double processorParameter(const ProcessorSpec& processor, const std::string& name,
                          double fallback) {
    const auto iterator = processor.parameters.find(name);
    return iterator != processor.parameters.end() ? iterator->second : fallback;
}

void attachProcessorAutomation(SignalNodePlan& node, const MixSpec& mix,
                               const std::string& target, const std::string& processor) {
    for (const ProcessorAutomationSpec& automation : mix.processorAutomation) {
        if (automation.target != target || automation.processor != processor) {
            continue;
        }
        ProcessorAutomationPlan plan;
        plan.parameter = automation.parameter;
        plan.curve = automation.curve;
        plan.mode = automation.mode;
        plan.minimum = automation.minimum;
        plan.maximum = automation.maximum;
        node.automation.push_back(std::move(plan));
    }
}

double processorTailSeconds(const ProcessorSpec& processor) {
    switch (processor.kind) {
        case ProcessorKind::Gain:
        case ProcessorKind::Pan:
            return 0.0;
        case ProcessorKind::Biquad:
            return 0.05;
        case ProcessorKind::Compressor:
            return 0.2;
        case ProcessorKind::Delay: {
            const double time = std::max(0.0, processorParameter(processor, "time", 0.25));
            const double feedback = std::clamp(processorParameter(processor, "feedback", 0.25),
                                               0.0, 0.95);
            if (feedback <= 0.001) {
                return time;
            }
            const double repeats = std::log(0.001) / std::log(feedback);
            return std::min(10.0, time * repeats);
        }
        case ProcessorKind::Reverb: {
            const double room = std::clamp(processorParameter(processor, "room", 0.6), 0.0, 1.0);
            return std::min(10.0, 2.5 + room * 2.5);
        }
        case ProcessorKind::Limiter:
            return 0.0;
    }
    return 0.0;
}

double processorLatencySeconds(const ProcessorSpec& processor) {
    if (processor.kind != ProcessorKind::Limiter) {
        return 0.0;
    }
    const double attack = processorParameter(processor, "attack", 0.005);
    return std::clamp(attack, 0.001, 0.05);
}

bool validateSignalGraph(SignalGraphPlan& graph, std::vector<Diagnostic>& diagnostics) {
    bool valid = true;
    if (graph.nodes.empty()) {
        diagnostics.push_back(Diagnostic::error("graph-empty", "signal graph has no nodes"));
        return false;
    }
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        if (graph.nodes[index].id != static_cast<int>(index)) {
            diagnostics.push_back(Diagnostic::error("graph-node-id", "signal node ids are not dense"));
            valid = false;
        }
    }

    std::vector<int> incomingMain(graph.nodes.size(), 0);
    std::vector<int> incomingSide(graph.nodes.size(), 0);
    for (const SignalConnectionPlan& connection : graph.connections) {
        if (connection.from < 0 || connection.from >= static_cast<int>(graph.nodes.size()) ||
            connection.to < 0 || connection.to >= static_cast<int>(graph.nodes.size())) {
            diagnostics.push_back(
                Diagnostic::error("graph-connection", "connection references an unknown node"));
            valid = false;
            continue;
        }
        const SignalNodePlan& from = graph.nodes[static_cast<std::size_t>(connection.from)];
        const SignalNodePlan& to = graph.nodes[static_cast<std::size_t>(connection.to)];
        if (connection.sidechain) {
            if (to.kind != SignalNodeKind::Processor || !to.hasSidechainInput) {
                diagnostics.push_back(Diagnostic::error(
                    "graph-sidechain", "sidechain connection targets a node without a key input"));
                valid = false;
            }
            if (from.kind != SignalNodeKind::Source) {
                diagnostics.push_back(Diagnostic::error(
                    "graph-sidechain", "sidechain source must be an instrument source node"));
                valid = false;
            }
            ++incomingSide[static_cast<std::size_t>(connection.to)];
        } else {
            if (from.outputChannels <= 0 || to.inputChannels <= 0) {
                diagnostics.push_back(Diagnostic::error(
                    "graph-port", "connection uses a non-audio port"));
                valid = false;
            } else if (from.outputChannels > to.inputChannels) {
                diagnostics.push_back(Diagnostic::error(
                    "graph-channel-mismatch", "stereo output cannot connect to a mono input"));
                valid = false;
            }
            ++incomingMain[static_cast<std::size_t>(connection.to)];
        }
    }

    int outputCount = 0;
    for (const SignalNodePlan& node : graph.nodes) {
        const std::size_t id = static_cast<std::size_t>(node.id);
        if (node.kind == SignalNodeKind::Source && incomingMain[id] != 0) {
            diagnostics.push_back(
                Diagnostic::error("graph-source-input", "source node cannot have audio inputs"));
            valid = false;
        }
        if (node.kind == SignalNodeKind::Processor && incomingMain[id] > 1) {
            diagnostics.push_back(
                Diagnostic::error("graph-fan-in", "processor has more than one audio input"));
            valid = false;
        }
        if (node.kind == SignalNodeKind::Processor && incomingSide[id] > 1) {
            diagnostics.push_back(
                Diagnostic::error("graph-sidechain", "processor has more than one sidechain input"));
            valid = false;
        }
        if (node.kind == SignalNodeKind::Output) {
            ++outputCount;
            if (incomingMain[id] != 1) {
                diagnostics.push_back(
                    Diagnostic::error("graph-output", "output node needs exactly one input"));
                valid = false;
            }
        }
    }
    if (outputCount != 1) {
        diagnostics.push_back(
            Diagnostic::error("graph-output", "signal graph needs exactly one output node"));
        valid = false;
    }

    std::vector<std::vector<int>> adjacency(graph.nodes.size());
    std::vector<int> indegree(graph.nodes.size(), 0);
    for (const SignalConnectionPlan& connection : graph.connections) {
        if (connection.from < 0 || connection.to < 0 ||
            connection.from >= static_cast<int>(graph.nodes.size()) ||
            connection.to >= static_cast<int>(graph.nodes.size())) {
            continue;
        }
        adjacency[static_cast<std::size_t>(connection.from)].push_back(connection.to);
        ++indegree[static_cast<std::size_t>(connection.to)];
    }
    std::priority_queue<int, std::vector<int>, std::greater<int>> ready;
    for (std::size_t index = 0; index < indegree.size(); ++index) {
        if (indegree[index] == 0) {
            ready.push(static_cast<int>(index));
        }
    }
    graph.order.clear();
    while (!ready.empty()) {
        const int node = ready.top();
        ready.pop();
        graph.order.push_back(node);
        for (const int next : adjacency[static_cast<std::size_t>(node)]) {
            if (--indegree[static_cast<std::size_t>(next)] == 0) {
                ready.push(next);
            }
        }
    }
    if (graph.order.size() != graph.nodes.size()) {
        diagnostics.push_back(Diagnostic::error(
            "graph-cycle", "signal graph contains a cycle; undelayed feedback is not supported"));
        valid = false;
    }
    return valid;
}

bool checkBackendCapabilities(const BackendCapabilities& capabilities,
                              const SignalGraphPlan& graph, UnsupportedPolicy policy,
                              std::vector<Diagnostic>& diagnostics) {
    bool supported = true;
    for (const SignalNodePlan& node : graph.nodes) {
        if (node.kind == SignalNodeKind::Processor) {
            const bool known =
                std::find(capabilities.processors.begin(), capabilities.processors.end(),
                          node.processor.kind) != capabilities.processors.end();
            if (!known) {
                const std::string name(processorKindName(node.processor.kind));
                if (policy == UnsupportedPolicy::Strict) {
                    addError(diagnostics, "audio-unsupported-processor",
                             "backend does not support processor '" + name + "'");
                    supported = false;
                } else if (policy == UnsupportedPolicy::Approximate) {
                    addWarning(diagnostics, "audio-approximated-processor",
                               "processor '" + name +
                                   "' has no native backend support and is approximated");
                } else {
                    addWarning(diagnostics, "audio-ignored-processor",
                               "processor '" + name + "' is ignored by the backend");
                }
            }
        }
        if (node.inputChannels > capabilities.maxChannels ||
            node.outputChannels > capabilities.maxChannels) {
            const std::string message = "node '" + node.name + "' exceeds backend channel limit";
            if (policy == UnsupportedPolicy::Strict) {
                addError(diagnostics, "audio-unsupported-channels", message);
                supported = false;
            } else {
                addWarning(diagnostics, "audio-unsupported-channels", message);
            }
        }
    }
    return supported;
}

AudioExecutionPlan buildAudioExecutionPlan(const ProductionIR& production,
                                           const TemporalIR& temporal,
                                           const AssetResolver& resolver,
                                           const AudioRenderConfig& config,
                                           std::vector<Diagnostic>& diagnostics) {
    if (config.sampleRate <= 0 || config.blockSize <= 0 || config.channels < 1 ||
        config.channels > 2 || config.tailSeconds < 0.0) {
        throw Error("invalid audio render configuration", "audio-config");
    }

    AudioExecutionPlan plan;
    plan.config = config;
    const AudioClock clock(production.tempoMap);
    plan.tempo = clock.segments();

    double contentEnd = 0.0;
    double voiceEnd = 0.0;

    const auto collectControls = [&](const AutomationBinding& binding, const std::string& track,
                                     std::vector<ControlBindingPlan>& target) {
        if (!binding.enabled) {
            return;
        }
        if (binding.domain != TimeDomain::Musical) {
            addWarning(diagnostics, "audio-unsupported-automation",
                       "automation '" + binding.target.id.str() +
                           "' is not in the musical domain");
            return;
        }
        if (binding.target.id.scope == ParameterScope::Clip) {
            addWarning(diagnostics, "audio-unsupported-automation",
                       "clip-scope automation '" + binding.target.id.str() +
                           "' is not supported by the reference renderer");
            return;
        }
        if (binding.target.id.scope == ParameterScope::Track &&
            binding.target.id.track != track) {
            return;
        }
        const std::string& parameter = binding.target.id.parameter;
        if (parameter != "volume" && parameter != "pan") {
            addWarning(diagnostics, "audio-unsupported-parameter",
                       "parameter '" + parameter + "' has no reference audio mapping");
            return;
        }
        ControlBindingPlan control;
        control.parameter = parameter;
        control.curve = binding.curve;
        control.mode = binding.mode;
        control.unit = binding.target.unit;
        control.minimum = binding.target.minimum;
        control.maximum = binding.target.maximum;
        target.push_back(std::move(control));
    };

    for (const AutomationBinding& binding : production.automation) {
        if (binding.target.id.scope == ParameterScope::Clip) {
            addWarning(diagnostics, "audio-unsupported-automation",
                       "clip-scope automation '" + binding.target.id.str() +
                           "' is not supported by the reference renderer");
        } else if (binding.target.id.scope == ParameterScope::Master) {
            collectControls(binding, {}, plan.masterControls);
        }
    }

    for (const TrackIR& track : production.tracks) {
        const AssetBinding* binding = track.findBinding("main");
        if (binding == nullptr && !track.assetBindings.empty()) {
            binding = &track.assetBindings.front();
        }
        if (binding == nullptr) {
            addError(diagnostics, "audio-missing-binding",
                     "track '" + track.name + "' has no asset binding");
            continue;
        }
        const std::optional<InstrumentAsset> instrument =
            resolver.resolve(track, *binding, diagnostics);
        if (!instrument.has_value()) {
            continue;
        }

        TrackPlan trackPlan;
        trackPlan.name = track.name;
        trackPlan.instrument = *instrument;

        for (const AutomationBinding& automation : production.automation) {
            if (automation.target.id.scope == ParameterScope::Track &&
                automation.target.id.track == track.name) {
                collectControls(automation, track.name, trackPlan.controls);
            }
        }

        for (const ClipIR& clip : track.clips) {
            const Rational clipDuration = clip.content.duration().inWholeNotes();
            if (!clipDuration.isPositive()) {
                continue;
            }
            for (const Note& note : clip.content.notes()) {
                const Rational relative = note.offset.inWholeNotes() / clipDuration;
                double velocityScale = 1.0;
                double timingOffset = 0.0;
                double gateScale = 1.0;
                bool hasPan = false;
                double pan = 0.0;
                if (clip.expression.velocity != kInvalidTemporalId) {
                    velocityScale = temporal.sample(clip.expression.velocity, relative);
                }
                if (clip.expression.timing != kInvalidTemporalId) {
                    timingOffset = temporal.sample(clip.expression.timing, relative);
                }
                if (clip.expression.gate != kInvalidTemporalId) {
                    gateScale = temporal.sample(clip.expression.gate, relative);
                }
                if (clip.expression.pan != kInvalidTemporalId) {
                    hasPan = true;
                    pan = temporal.sample(clip.expression.pan, relative);
                }

                const double onsetWhole =
                    clip.start.inWholeNotes().toDouble() + note.offset.inWholeNotes().toDouble() +
                    timingOffset;
                const double durationWhole =
                    std::max(0.0, note.duration.inWholeNotes().toDouble() * gateScale);
                const double endWhole = onsetWhole + durationWhole;
                const double startSeconds = clock.secondsAtWhole(onsetWhole);
                const double endSeconds = clock.secondsAtWhole(endWhole);

                PlannedNote planned;
                planned.midi = note.pitch.midi();
                planned.startSample =
                    std::max(0, static_cast<int>(std::llround(startSeconds * config.sampleRate)));
                planned.endSample =
                    std::max(planned.startSample + 1,
                             static_cast<int>(std::llround(endSeconds * config.sampleRate)));
                planned.velocity = static_cast<float>(std::clamp(
                    static_cast<double>(note.velocity) / 127.0 * velocityScale, 0.0, 2.0));
                planned.hasPan = hasPan;
                planned.pan = static_cast<float>(std::clamp(pan, -1.0, 1.0));
                trackPlan.notes.push_back(planned);

                contentEnd = std::max(contentEnd, endSeconds);
                voiceEnd = std::max(voiceEnd, endSeconds + instrument->envelope.release);
            }
        }

        std::sort(trackPlan.notes.begin(), trackPlan.notes.end(),
                  [](const PlannedNote& lhs, const PlannedNote& rhs) {
                      if (lhs.startSample != rhs.startSample) {
                          return lhs.startSample < rhs.startSample;
                      }
                      return lhs.midi < rhs.midi;
                  });
        plan.tracks.push_back(std::move(trackPlan));
    }

    plan.contentEndSeconds = contentEnd;
    const double voiceContentEnd = std::max(contentEnd, voiceEnd);

    const double logicalWhole = production.logicalExtent.inWholeNotes().toDouble();
    plan.logicalExtentSeconds =
        logicalWhole > 0.0 ? clock.secondsAtWhole(logicalWhole) : 0.0;

    SignalGraphPlan& graph = plan.graph;
    std::vector<int> sourceNodes(plan.tracks.size(), -1);
    for (std::size_t index = 0; index < plan.tracks.size(); ++index) {
        SignalNodePlan source;
        source.kind = SignalNodeKind::Source;
        source.name = plan.tracks[index].name + " Source";
        source.trackIndex = static_cast<int>(index);
        source.inputChannels = 0;
        source.outputChannels = config.channels;
        sourceNodes[index] = addSignalNode(graph, std::move(source));
    }

    std::map<std::string, int> busMixers;
    for (const BusSpec& bus : production.mix.buses) {
        SignalNodePlan mixer;
        mixer.kind = SignalNodeKind::Mixer;
        mixer.name = bus.strip.name + " Mixer";
        mixer.role = StripRole::Bus;
        mixer.inputChannels = config.channels;
        mixer.outputChannels = config.channels;
        busMixers[bus.strip.name] = addSignalNode(graph, std::move(mixer));
    }

    SignalNodePlan masterMixer;
    masterMixer.kind = SignalNodeKind::Mixer;
    masterMixer.name = "Master Mixer";
    masterMixer.role = StripRole::Master;
    masterMixer.inputChannels = config.channels;
    masterMixer.outputChannels = config.channels;
    const int masterMixerId = addSignalNode(graph, std::move(masterMixer));

    std::map<std::string, int> processorNodes;

    const auto insertChain = [&](int previous, const ChannelStripSpec& strip, StripRole role,
                                 int trackIndex) {
        int current = previous;
        for (const std::string& insert : strip.inserts) {
            const ProcessorSpec* definition = production.mix.findProcessor(insert);
            if (definition == nullptr) {
                addError(diagnostics, "mix-missing-processor",
                         "strip '" + strip.name + "' inserts unknown processor '" + insert + "'");
                continue;
            }
            SignalNodePlan node;
            node.kind = SignalNodeKind::Processor;
            node.name = strip.name + " / " + insert;
            node.role = role;
            node.trackIndex = trackIndex;
            node.processor = *definition;
            applyProcessorDefaults(node.processor);
            node.inputChannels = config.channels;
            node.outputChannels = config.channels;
            node.hasSidechainInput = node.processor.kind == ProcessorKind::Compressor;
            node.latencySeconds = processorLatencySeconds(node.processor);
            attachProcessorAutomation(node, production.mix, strip.name, insert);
            const int nodeId = addSignalNode(graph, std::move(node));
            const std::string key = strip.name + "|" + insert;
            if (processorNodes.find(key) != processorNodes.end()) {
                addError(diagnostics, "mix-duplicate-insert",
                         "strip '" + strip.name + "' inserts processor '" + insert + "' twice");
            } else {
                processorNodes[key] = nodeId;
            }
            plan.processorTailSeconds =
                std::max(plan.processorTailSeconds,
                         processorTailSeconds(graph.nodes[static_cast<std::size_t>(nodeId)].processor));
            connectSignal(graph, current, nodeId, 1.0);
            current = nodeId;
        }
        return current;
    };

    const auto appendGainPan = [&](int previous, const ChannelStripSpec& strip, StripRole role,
                                   int trackIndex, bool trackVolume, bool trackPan,
                                   bool masterVolume, bool masterPan) {
        SignalNodePlan gain;
        gain.kind = SignalNodeKind::Processor;
        gain.name = strip.name + " Gain";
        gain.role = role;
        gain.trackIndex = trackIndex;
        gain.processor.name = gain.name;
        gain.processor.kind = ProcessorKind::Gain;
        gain.processor.parameters["gain"] = strip.gainDb;
        gain.inputChannels = config.channels;
        gain.outputChannels = config.channels;
        gain.trackVolume = trackVolume;
        gain.masterVolume = masterVolume;
        const int gainId = addSignalNode(graph, std::move(gain));
        connectSignal(graph, previous, gainId, 1.0);

        SignalNodePlan pan;
        pan.kind = SignalNodeKind::Processor;
        pan.name = strip.name + " Pan";
        pan.role = role;
        pan.trackIndex = trackIndex;
        pan.processor.name = pan.name;
        pan.processor.kind = ProcessorKind::Pan;
        pan.processor.parameters["pan"] = strip.pan;
        pan.inputChannels = config.channels;
        pan.outputChannels = config.channels;
        pan.trackPan = trackPan;
        pan.masterPan = masterPan;
        const int panId = addSignalNode(graph, std::move(pan));
        connectSignal(graph, gainId, panId, 1.0);
        return panId;
    };

    for (std::size_t index = 0; index < plan.tracks.size(); ++index) {
        const TrackPlan& track = plan.tracks[index];
        const ChannelStripSpec* found = production.mix.findTrack(track.name);
        ChannelStripSpec strip;
        if (found != nullptr) {
            strip = *found;
        } else {
            strip.name = track.name;
        }
        const int chain = insertChain(sourceNodes[index], strip, StripRole::Track,
                                      static_cast<int>(index));
        const int trackOutput = appendGainPan(chain, strip, StripRole::Track,
                                              static_cast<int>(index), true, true, false, false);
        connectSignal(graph, trackOutput, masterMixerId, 1.0);
        for (const SendSpec& send : strip.sends) {
            const auto bus = busMixers.find(send.bus);
            if (bus == busMixers.end()) {
                addError(diagnostics, "mix-missing-bus",
                         "track '" + track.name + "' sends to unknown bus '" + send.bus + "'");
                continue;
            }
            connectSignal(graph, trackOutput, bus->second, send.gain);
        }
    }

    for (const BusSpec& bus : production.mix.buses) {
        const int mixer = busMixers[bus.strip.name];
        const int chain = insertChain(mixer, bus.strip, StripRole::Bus, -1);
        const int output = appendGainPan(chain, bus.strip, StripRole::Bus, -1, false, false,
                                         false, false);
        connectSignal(graph, output, masterMixerId, 1.0);
    }

    const ChannelStripSpec& master = production.mix.master;
    const int masterChain = insertChain(masterMixerId, master, StripRole::Master, -1);
    const int masterOutput =
        appendGainPan(masterChain, master, StripRole::Master, -1, false, false, true, true);

    SignalNodePlan outputNode;
    outputNode.kind = SignalNodeKind::Output;
    outputNode.name = "Output";
    outputNode.role = StripRole::Master;
    outputNode.inputChannels = config.channels;
    outputNode.outputChannels = 0;
    const int outputId = addSignalNode(graph, std::move(outputNode));
    connectSignal(graph, masterOutput, outputId, 1.0);
    graph.outputNode = outputId;

    for (std::size_t index = 0; index < plan.tracks.size(); ++index) {
        const ChannelStripSpec* found = production.mix.findTrack(plan.tracks[index].name);
        if (found == nullptr) {
            continue;
        }
        for (const SidechainSpec& sidechain : found->sidechains) {
            const std::string key = found->name + "|" + sidechain.processor;
            const auto target = processorNodes.find(key);
            if (target == processorNodes.end()) {
                addError(diagnostics, "mix-sidechain-target",
                         "sidechain references processor '" + sidechain.processor +
                             "' which is not inserted on '" + found->name + "'");
                continue;
            }
            std::size_t sourceIndex = plan.tracks.size();
            for (std::size_t trackIndex = 0; trackIndex < plan.tracks.size(); ++trackIndex) {
                if (plan.tracks[trackIndex].name == sidechain.source) {
                    sourceIndex = trackIndex;
                    break;
                }
            }
            if (sourceIndex >= sourceNodes.size()) {
                addError(diagnostics, "mix-sidechain-source",
                         "sidechain source track '" + sidechain.source + "' does not exist");
                continue;
            }
            connectSignal(graph, sourceNodes[sourceIndex], target->second, 1.0, true);
        }
    }

    validateSignalGraph(graph, diagnostics);

    {
        std::vector<std::vector<int>> mainIncoming(graph.nodes.size());
        for (const SignalConnectionPlan& connection : graph.connections) {
            if (!connection.sidechain && connection.to >= 0 &&
                connection.to < static_cast<int>(graph.nodes.size()) &&
                connection.from >= 0) {
                mainIncoming[static_cast<std::size_t>(connection.to)].push_back(
                    connection.from);
            }
        }
        std::vector<double> arrival(graph.nodes.size(), 0.0);
        bool orderValid = graph.order.size() == graph.nodes.size();
        if (orderValid) {
            for (const int nodeId : graph.order) {
                double minimum = std::numeric_limits<double>::infinity();
                double maximum = 0.0;
                for (const int from : mainIncoming[static_cast<std::size_t>(nodeId)]) {
                    const double latency =
                        arrival[static_cast<std::size_t>(from)] +
                        graph.nodes[static_cast<std::size_t>(from)].latencySeconds;
                    minimum = std::min(minimum, latency);
                    maximum = std::max(maximum, latency);
                }
                if (!mainIncoming[static_cast<std::size_t>(nodeId)].empty()) {
                    arrival[static_cast<std::size_t>(nodeId)] = maximum;
                    if (minimum < maximum - 1e-9) {
                        plan.latencyMisaligned = true;
                        const std::string message =
                            "signal paths into '" + graph.nodes[static_cast<std::size_t>(nodeId)].name +
                            "' have different processing latencies; automatic delay "
                            "compensation is not implemented";
                        if (config.policy == UnsupportedPolicy::Strict) {
                            addError(diagnostics, "audio-latency-misaligned", message);
                        } else {
                            addWarning(diagnostics, "audio-latency-misaligned", message);
                        }
                    }
                }
            }
        }
        if (orderValid && graph.outputNode >= 0 &&
            graph.outputNode < static_cast<int>(graph.nodes.size())) {
            plan.outputLatencySeconds =
                arrival[static_cast<std::size_t>(graph.outputNode)] +
                graph.nodes[static_cast<std::size_t>(graph.outputNode)].latencySeconds;
            plan.outputLatencyFrames =
                std::max(0, static_cast<int>(std::llround(plan.outputLatencySeconds *
                                                          config.sampleRate)));
        }
    }

    double processorTail = plan.processorTailSeconds;
    if (processorTail > config.maxTailSeconds) {
        addWarning(diagnostics, "audio-tail-capped",
                   "processor tail was capped at maxTailSeconds");
        processorTail = config.maxTailSeconds;
    }
    double tail = config.tailSeconds;
    if (tail > config.maxTailSeconds) {
        addWarning(diagnostics, "audio-tail-capped", "render tail was capped at maxTailSeconds");
        tail = config.maxTailSeconds;
    }
    plan.processorTailSeconds = processorTail;
    const double base = std::max(plan.logicalExtentSeconds, voiceContentEnd + processorTail);
    plan.renderedSeconds = base + tail;
    plan.totalFrames =
        std::max(0, static_cast<int>(std::llround(plan.renderedSeconds * config.sampleRate))) +
        plan.outputLatencyFrames;
    return plan;
}

}  // namespace mpl
