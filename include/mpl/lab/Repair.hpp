#pragma once

#include "mpl/lab/ModelAdapter.hpp"
#include "mpl/lab/Task.hpp"

#include <string>
#include <vector>

namespace mpl {

enum class RepairFailure {
    None,
    ModelFailure,
    CompileFailure,
    TaskFailure,
    InfrastructureError,
    BudgetExhausted,
};

std::string_view repairFailureName(RepairFailure failure);

struct RepairBudget {
    int maxAttempts = 3;
    int maxInputTokens = 0;   // 0 = unlimited
    int maxOutputTokens = 0;  // 0 = unlimited
    int maxSeconds = 0;       // 0 = unlimited
};

struct RepairAttempt {
    int index = 0;
    std::string candidateSource;
    std::string sourcePath;
    VerificationResult verification;
    std::string feedback;
    int inputTokens = 0;
    int outputTokens = 0;
    int cacheTokens = 0;
    int modelCalls = 0;
};

struct RepairResult {
    bool success = false;
    RepairFailure failure = RepairFailure::None;
    std::string reason;
    std::string finalSource;
    std::vector<RepairAttempt> attempts;
    int totalInputTokens = 0;
    int totalOutputTokens = 0;
    int totalCacheTokens = 0;
    int totalModelCalls = 0;
    std::string toJson() const;
};

/// Runs a bounded repair loop through a model-agnostic adapter. The task is
/// immutable: target, preserve and frame are never relaxed. If
/// `redactExpected` is true (held-out use), expected reference values are
/// removed from feedback; only observation names/status/reasons are exposed.
RepairResult runRepairLoop(ModelAdapter& adapter, const Task& task,
                           const std::string& taskPath,
                           const ModelResponse& initialResponse,
                           const RepairBudget& budget, bool redactExpected);

}  // namespace mpl
