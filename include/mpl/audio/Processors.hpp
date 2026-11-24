#pragma once

#include "mpl/ir/MixIR.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mpl {

struct ParameterRamp {
    std::map<std::string, std::pair<double, double>> values;

    void set(const std::string& name, double start, double end) {
        values[name] = {start, end};
    }
    void setConstant(const std::string& name, double value) { values[name] = {value, value}; }
    double at(const std::string& name, double fraction, double fallback) const {
        const auto iterator = values.find(name);
        if (iterator == values.end()) {
            return fallback;
        }
        return iterator->second.first +
               (iterator->second.second - iterator->second.first) * fraction;
    }
};

class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;
    virtual void prepare(int sampleRate, int channels) = 0;
    virtual void reset() = 0;
    virtual void process(float* buffer, int frames, int channels, const ParameterRamp& parameters,
                         const float* sidechain) = 0;
    virtual double tailSeconds() const { return 0.0; }
};

std::unique_ptr<AudioProcessor> createProcessor(ProcessorKind kind, const std::string& mode);

}  // namespace mpl
