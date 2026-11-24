#include "mpl/frontend/ParameterRegistry.hpp"

#include <array>

namespace mpl {
namespace {

constexpr std::array<ParameterInfo, 8> kParameters{{
    {"volume", ParameterUnit::Ratio, 0.0, 1.0, true, true},
    {"pan", ParameterUnit::Pan, -1.0, 1.0, true, true},
    {"expression", ParameterUnit::Ratio, 0.0, 1.0, true, true},
    {"modulation", ParameterUnit::Ratio, 0.0, 1.0, true, true},
    {"cutoff", ParameterUnit::Ratio, 0.0, 1.0, true, true},
    {"resonance", ParameterUnit::Ratio, 0.0, 1.0, true, true},
    {"reverb", ParameterUnit::Ratio, 0.0, 1.0, true, true},
    {"chorus", ParameterUnit::Ratio, 0.0, 1.0, true, true},
}};

constexpr std::array<ExpressionLaneInfo, 4> kExpressionLanes{{
    {"velocity", 0.05, 2.0},
    {"timing", -0.5, 0.5},
    {"gate", 0.1, 2.0},
    {"pan", -1.0, 1.0},
}};

}  // namespace

const ParameterInfo* findParameter(std::string_view name) {
    for (const ParameterInfo& parameter : kParameters) {
        if (parameter.name == name) {
            return &parameter;
        }
    }
    return nullptr;
}

const ExpressionLaneInfo* findExpressionLane(std::string_view name) {
    for (const ExpressionLaneInfo& lane : kExpressionLanes) {
        if (lane.name == name) {
            return &lane;
        }
    }
    return nullptr;
}

bool isInstrumentRole(std::string_view name) {
    return name == "melodic" || name == "bass" || name == "harmony" ||
           name == "percussion" || name == "texture";
}

}  // namespace mpl
