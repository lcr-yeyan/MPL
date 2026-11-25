#pragma once

#include "mpl/ir/Automation.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

enum class ProcessorKind { Gain, Pan, Biquad, Compressor, Delay, Reverb, Limiter };

std::string_view processorKindName(ProcessorKind kind);

bool processorKindFromName(std::string_view name, ProcessorKind& kind);

struct ProcessorSpec {
    std::string name;
    ProcessorKind kind = ProcessorKind::Gain;
    std::string mode;  // biquad filter mode
    std::map<std::string, double> parameters;
};

struct SendSpec {
    std::string bus;
    double gain = 0.0;
};

struct SidechainSpec {
    std::string processor;
    std::string source;
};

struct ProcessorAutomationSpec {
    std::string target;  // track/bus/master name
    std::string processor;
    std::string parameter;
    TemporalId curve = kInvalidTemporalId;
    AutomationMode mode = AutomationMode::Replace;
    double minimum = 0.0;
    double maximum = 1.0;
};

struct ChannelStripSpec {
    std::string name;
    double gainDb = 0.0;
    double pan = 0.0;
    std::vector<std::string> inserts;
    std::vector<SendSpec> sends;
    std::vector<SidechainSpec> sidechains;
};

struct BusSpec {
    ChannelStripSpec strip;
};

struct MixSpec {
    std::vector<ProcessorSpec> processors;
    std::vector<BusSpec> buses;
    std::vector<ChannelStripSpec> tracks;
    ChannelStripSpec master{"master"};
    std::vector<ProcessorAutomationSpec> processorAutomation;

    const ProcessorSpec* findProcessor(std::string_view name) const;
    ProcessorSpec* findProcessor(std::string_view name);
    const BusSpec* findBus(std::string_view name) const;
    const ChannelStripSpec* findTrack(std::string_view name) const;
};

struct ProcessorParameterDescriptor {
    std::string_view name;
    ParameterUnit unit = ParameterUnit::Generic;
    double minimum = 0.0;
    double maximum = 1.0;
    double defaultValue = 0.0;
    bool text = false;
};

const std::vector<ProcessorParameterDescriptor>& processorParameters(ProcessorKind kind);

/// Fills defaults for any parameter omitted by the program.
void applyProcessorDefaults(ProcessorSpec& processor);

}  // namespace mpl
