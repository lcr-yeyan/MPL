#include "mpl/ir/MixIR.hpp"

#include <algorithm>

namespace mpl {
namespace {

const std::vector<ProcessorParameterDescriptor> kGainParameters{
    {"gain", ParameterUnit::Decibel, -96.0, 24.0, 0.0, false}};
const std::vector<ProcessorParameterDescriptor> kPanParameters{
    {"pan", ParameterUnit::Pan, -1.0, 1.0, 0.0, false}};
const std::vector<ProcessorParameterDescriptor> kBiquadParameters{
    {"frequency", ParameterUnit::Hertz, 20.0, 20000.0, 1000.0, false},
    {"q", ParameterUnit::Generic, 0.1, 20.0, 0.707, false},
    {"gain", ParameterUnit::Decibel, -24.0, 24.0, 0.0, false}};
const std::vector<ProcessorParameterDescriptor> kCompressorParameters{
    {"threshold", ParameterUnit::Decibel, -80.0, 0.0, -18.0, false},
    {"ratio", ParameterUnit::Ratio, 1.0, 20.0, 4.0, false},
    {"attack", ParameterUnit::Seconds, 0.0, 1.0, 0.01, false},
    {"release", ParameterUnit::Seconds, 0.0, 2.0, 0.12, false},
    {"makeup", ParameterUnit::Decibel, 0.0, 24.0, 0.0, false}};
const std::vector<ProcessorParameterDescriptor> kDelayParameters{
    {"time", ParameterUnit::Seconds, 0.0, 2.0, 0.25, false},
    {"feedback", ParameterUnit::Ratio, 0.0, 0.95, 0.25, false},
    {"mix", ParameterUnit::Ratio, 0.0, 1.0, 0.2, false}};
const std::vector<ProcessorParameterDescriptor> kReverbParameters{
    {"room", ParameterUnit::Ratio, 0.0, 1.0, 0.6, false},
    {"damping", ParameterUnit::Ratio, 0.0, 1.0, 0.4, false},
    {"mix", ParameterUnit::Ratio, 0.0, 1.0, 0.3, false}};
const std::vector<ProcessorParameterDescriptor> kLimiterParameters{
    {"ceiling", ParameterUnit::Decibel, -24.0, 0.0, -1.0, false},
    {"attack", ParameterUnit::Seconds, 0.001, 0.05, 0.005, false},
    {"release", ParameterUnit::Seconds, 0.01, 2.0, 0.1, false}};

}  // namespace

std::string_view processorKindName(ProcessorKind kind) {
    switch (kind) {
        case ProcessorKind::Gain: return "gain";
        case ProcessorKind::Pan: return "pan";
        case ProcessorKind::Biquad: return "biquad";
        case ProcessorKind::Compressor: return "compressor";
        case ProcessorKind::Delay: return "delay";
        case ProcessorKind::Reverb: return "reverb";
        case ProcessorKind::Limiter: return "limiter";
    }
    return "unknown";
}

bool processorKindFromName(std::string_view name, ProcessorKind& kind) {
    if (name == "gain") {
        kind = ProcessorKind::Gain;
    } else if (name == "pan") {
        kind = ProcessorKind::Pan;
    } else if (name == "biquad" || name == "eq") {
        kind = ProcessorKind::Biquad;
    } else if (name == "compressor") {
        kind = ProcessorKind::Compressor;
    } else if (name == "delay") {
        kind = ProcessorKind::Delay;
    } else if (name == "reverb") {
        kind = ProcessorKind::Reverb;
    } else if (name == "limiter") {
        kind = ProcessorKind::Limiter;
    } else {
        return false;
    }
    return true;
}

const std::vector<ProcessorParameterDescriptor>& processorParameters(ProcessorKind kind) {
    switch (kind) {
        case ProcessorKind::Gain: return kGainParameters;
        case ProcessorKind::Pan: return kPanParameters;
        case ProcessorKind::Biquad: return kBiquadParameters;
        case ProcessorKind::Compressor: return kCompressorParameters;
        case ProcessorKind::Delay: return kDelayParameters;
        case ProcessorKind::Reverb: return kReverbParameters;
        case ProcessorKind::Limiter: return kLimiterParameters;
    }
    return kGainParameters;
}

const ProcessorSpec* MixSpec::findProcessor(std::string_view name) const {
    for (const ProcessorSpec& processor : processors) {
        if (processor.name == name) {
            return &processor;
        }
    }
    return nullptr;
}

ProcessorSpec* MixSpec::findProcessor(std::string_view name) {
    for (ProcessorSpec& processor : processors) {
        if (processor.name == name) {
            return &processor;
        }
    }
    return nullptr;
}

const BusSpec* MixSpec::findBus(std::string_view name) const {
    for (const BusSpec& bus : buses) {
        if (bus.strip.name == name) {
            return &bus;
        }
    }
    return nullptr;
}

const ChannelStripSpec* MixSpec::findTrack(std::string_view name) const {
    for (const ChannelStripSpec& track : tracks) {
        if (track.name == name) {
            return &track;
        }
    }
    return nullptr;
}

void applyProcessorDefaults(ProcessorSpec& processor) {
    if (processor.kind == ProcessorKind::Biquad && processor.mode.empty()) {
        processor.mode = "lowpass";
    }
    for (const ProcessorParameterDescriptor& descriptor : processorParameters(processor.kind)) {
        if (processor.parameters.find(std::string(descriptor.name)) ==
            processor.parameters.end()) {
            processor.parameters[std::string(descriptor.name)] = descriptor.defaultValue;
        }
    }
}

}  // namespace mpl
