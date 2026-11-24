#pragma once

#include "mpl/ir/Automation.hpp"

#include <string_view>

namespace mpl {

struct ParameterInfo {
    std::string_view name;
    ParameterUnit unit = ParameterUnit::Ratio;
    double minimum = 0.0;
    double maximum = 1.0;
    bool trackScope = true;
    bool masterScope = true;
};

const ParameterInfo* findParameter(std::string_view name);

struct ExpressionLaneInfo {
    std::string_view name;
    double minimum = 0.0;
    double maximum = 1.0;
};

const ExpressionLaneInfo* findExpressionLane(std::string_view name);

bool isInstrumentRole(std::string_view name);

}  // namespace mpl
