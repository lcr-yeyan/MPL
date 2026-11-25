#pragma once

#include "mpl/audio/AudioBuffer.hpp"
#include "mpl/audio/BackendContract.hpp"
#include "mpl/core/Diagnostic.hpp"
#include "mpl/ir/Automation.hpp"
#include "mpl/ir/MixIR.hpp"
#include "mpl/ir/ProductionIR.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mpl {

enum class Oscillator { Sine, Triangle, Saw, Square, Noise };

struct AdsrEnvelope {
    double attack = 0.01;
    double decay = 0.1;
    double sustain = 0.7;
    double release = 0.3;
};

struct InstrumentAsset {
    std::string id;
    std::string version;
    Oscillator oscillator = Oscillator::Sine;
    AdsrEnvelope envelope;
    float gain = 0.5f;
    std::shared_ptr<AudioBuffer> sample;
    int sampleRootMidi = 60;
};

/// Backend-neutral asset resolution boundary. Implementations may resolve
/// symbolic names, sample files, plugin presets, or future SoundBoxer sources.
class AssetResolver {
public:
    virtual ~AssetResolver() = default;
    virtual std::optional<InstrumentAsset> resolve(const TrackIR& track,
                                                   const AssetBinding& binding,
                                                   std::vector<Diagnostic>& diagnostics) const = 0;
};

/// Minimal symbolic library used by the reference backend. Known instrument
/// families map to deterministic oscillator/envelope presets; `file:` or
/// `.wav` asset ids load a WAV sample relative to the source directory.
class ReferenceAssetLibrary final : public AssetResolver {
public:
    explicit ReferenceAssetLibrary(std::filesystem::path baseDirectory = {});

    std::optional<InstrumentAsset> resolve(const TrackIR& track, const AssetBinding& binding,
                                           std::vector<Diagnostic>& diagnostics) const override;

private:
    std::filesystem::path baseDirectory_;
};

struct AudioRenderConfig {
    int sampleRate = 48000;
    int blockSize = 256;
    int channels = 2;
    double tailSeconds = 1.0;
    double maxTailSeconds = 10.0;
    double silenceThresholdDb = -60.0;
    RenderTermination termination = RenderTermination::Fixed;
    UnsupportedPolicy policy = UnsupportedPolicy::Strict;
};

struct ControlBindingPlan {
    std::string parameter;
    TemporalId curve = kInvalidTemporalId;
    AutomationMode mode = AutomationMode::Replace;
    ParameterUnit unit = ParameterUnit::Ratio;
    double minimum = 0.0;
    double maximum = 1.0;
};

struct PlannedNote {
    int midi = 60;
    int startSample = 0;
    int endSample = 0;
    float velocity = 0.8f;
    float pan = 0.0f;
    bool hasPan = false;
};

struct TrackPlan {
    std::string name;
    InstrumentAsset instrument;
    std::vector<PlannedNote> notes;
    std::vector<ControlBindingPlan> controls;
};

struct TempoSegment {
    double startSeconds = 0.0;
    double startWholeNotes = 0.0;
    double bpm = 120.0;
};

struct ProcessorAutomationPlan {
    std::string parameter;
    TemporalId curve = kInvalidTemporalId;
    AutomationMode mode = AutomationMode::Replace;
    double minimum = 0.0;
    double maximum = 1.0;
};

enum class SignalNodeKind { Source, Mixer, Processor, Output };

enum class StripRole { None, Track, Bus, Master };

struct SignalNodePlan {
    int id = -1;
    SignalNodeKind kind = SignalNodeKind::Processor;
    std::string name;
    int trackIndex = -1;
    StripRole role = StripRole::None;
    ProcessorSpec processor;
    std::vector<ProcessorAutomationPlan> automation;
    bool trackVolume = false;
    bool trackPan = false;
    bool masterVolume = false;
    bool masterPan = false;
    int inputChannels = 2;
    int outputChannels = 2;
    bool hasSidechainInput = false;
    /// Processing latency (not artistic delay) introduced by this node.
    double latencySeconds = 0.0;
};

struct SignalConnectionPlan {
    int from = -1;
    int to = -1;
    double gain = 1.0;
    bool sidechain = false;
};

struct SignalGraphPlan {
    std::vector<SignalNodePlan> nodes;
    std::vector<SignalConnectionPlan> connections;
    std::vector<int> order;
    int outputNode = -1;
};

struct AudioExecutionPlan {
    AudioRenderConfig config;
    std::vector<TrackPlan> tracks;
    std::vector<ControlBindingPlan> masterControls;
    std::vector<TempoSegment> tempo;
    SignalGraphPlan graph;
    double logicalExtentSeconds = 0.0;
    double contentEndSeconds = 0.0;
    double processorTailSeconds = 0.0;
    double renderedSeconds = 0.0;
    /// Total processing latency of the graph; compensated by trimming the
    /// rendered output so musical positions stay aligned.
    double outputLatencySeconds = 0.0;
    int outputLatencyFrames = 0;
    bool latencyMisaligned = false;
    int totalFrames = 0;
};

AudioExecutionPlan buildAudioExecutionPlan(const ProductionIR& production,
                                           const TemporalIR& temporal,
                                           const AssetResolver& resolver,
                                           const AudioRenderConfig& config,
                                           std::vector<Diagnostic>& diagnostics);

/// Validates topology, ports, channel compatibility and feedback loops and
/// computes a deterministic topological execution order.
bool validateSignalGraph(SignalGraphPlan& graph, std::vector<Diagnostic>& diagnostics);

/// Applies the Strict / Approximate / Ignore policy when a graph requests
/// processors or channel counts a backend does not provide. Returns false when
/// a Strict policy rejected the graph.
bool checkBackendCapabilities(const BackendCapabilities& capabilities,
                              const SignalGraphPlan& graph, UnsupportedPolicy policy,
                              std::vector<Diagnostic>& diagnostics);

double midiToFrequency(int midi);

/// Musical-to-physical conversion used by the execution plan and renderer.
class AudioClock {
public:
    explicit AudioClock(const std::vector<TempoEvent>& tempoMap);

    double secondsAtWhole(double wholeNotes) const;
    double wholeAtSeconds(double seconds) const;
    double secondsPerWhole(double wholeNotes) const;

    const std::vector<TempoSegment>& segments() const noexcept { return segments_; }

private:
    std::vector<TempoSegment> segments_;
};

}  // namespace mpl
