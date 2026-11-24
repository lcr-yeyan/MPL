#include "mpl/ir/Automation.hpp"

#include <string>

namespace mpl {

std::string_view parameterScopeName(ParameterScope scope) {
    switch (scope) {
        case ParameterScope::Track: return "track";
        case ParameterScope::Clip: return "clip";
        case ParameterScope::Master: return "master";
    }
    return "unknown";
}

std::string_view parameterUnitName(ParameterUnit unit) {
    switch (unit) {
        case ParameterUnit::Generic: return "generic";
        case ParameterUnit::Decibel: return "decibel";
        case ParameterUnit::Ratio: return "ratio";
        case ParameterUnit::Pan: return "pan";
        case ParameterUnit::Milliseconds: return "milliseconds";
        case ParameterUnit::Hertz: return "hertz";
        case ParameterUnit::Seconds: return "seconds";
    }
    return "unknown";
}

std::pair<double, double> defaultParameterRange(ParameterUnit unit) {
    switch (unit) {
        case ParameterUnit::Generic: return {0.0, 1.0};
        case ParameterUnit::Decibel: return {-60.0, 0.0};
        case ParameterUnit::Ratio: return {0.0, 1.0};
        case ParameterUnit::Pan: return {-1.0, 1.0};
        case ParameterUnit::Milliseconds: return {0.0, 1000.0};
        case ParameterUnit::Hertz: return {20.0, 20000.0};
        case ParameterUnit::Seconds: return {0.0, 10.0};
    }
    return {0.0, 1.0};
}

std::string ParameterId::str() const {
    std::string result(parameterScopeName(scope));
    if (scope != ParameterScope::Master && !track.empty()) {
        result += ":" + track;
    }
    if (scope == ParameterScope::Clip && !clip.empty()) {
        result += ":" + clip;
    }
    result += ":";
    result += parameter;
    return result;
}

std::string NoteInstanceId::str() const {
    return std::to_string(clipInstance) + ":" + std::to_string(noteIndex);
}

ParameterTarget makeParameterTarget(ParameterScope scope, std::string track, std::string clip,
                                    std::string parameter, ParameterUnit unit) {
    ParameterTarget target;
    target.id = ParameterId{scope, std::move(track), std::move(clip), std::move(parameter)};
    target.unit = unit;
    const std::pair<double, double> range = defaultParameterRange(unit);
    target.minimum = range.first;
    target.maximum = range.second;
    return target;
}

std::string_view automationModeName(AutomationMode mode) {
    switch (mode) {
        case AutomationMode::Replace: return "replace";
        case AutomationMode::Add: return "add";
        case AutomationMode::Multiply: return "multiply";
    }
    return "unknown";
}

}  // namespace mpl
