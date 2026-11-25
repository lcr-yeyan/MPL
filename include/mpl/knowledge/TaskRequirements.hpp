#pragma once

#include "mpl/knowledge/Knowledge.hpp"
#include "mpl/lab/Task.hpp"

#include <string>
#include <vector>

namespace mpl {

struct TaskRequirementSet {
    std::string taskId;
    std::string category;
    std::vector<std::string> targetObservations;
    std::vector<std::string> preserveObservations;
    std::vector<std::string> frameObjects;
    std::vector<std::string> frameProperties;
    std::vector<std::string> allowedEffects;
    std::vector<std::string> requiredTypes;
    std::vector<std::string> requiredCapabilities;
    std::vector<std::string> unknown;
    std::vector<std::string> unsupported;
    TaskBudget budget;
    bool valid = true;
    std::string toJson() const;
};

/// Deterministic derivation from the structured Task Contract. No natural
/// language intent inference.
TaskRequirementSet analyzeTaskRequirements(const Task& task,
                                           const std::string& baselineSource = {});

}  // namespace mpl
