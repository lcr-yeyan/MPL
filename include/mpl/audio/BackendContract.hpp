#pragma once

#include "mpl/ir/MixIR.hpp"

#include <vector>

namespace mpl {

enum class UnsupportedPolicy { Strict, Approximate, Ignore };

enum class RenderTermination { Fixed, SilenceThreshold };

struct BackendCapabilities {
    bool sampleAccurateEvents = true;
    bool processorAutomation = true;
    bool sidechain = true;
    bool busesAndSends = true;
    bool silenceTermination = true;
    int maxChannels = 2;
    std::vector<ProcessorKind> processors;
};

}  // namespace mpl
