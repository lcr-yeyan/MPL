#include "mpl/lab/Repair.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/analysis/SemanticDelta.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace mpl {
namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

std::string writeCandidate(const std::string& source, int index) {
    const std::filesystem::path path =
        std::filesystem::current_path() /
        (".mpl-repair-" + std::to_string(index) + ".mpl");
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << source;
    stream.close();
    if (!stream) {
        return {};
    }
    return path.generic_string();
}

std::optional<Compilation> compileForDelta(const std::string& source,
                                           const std::string& file) {
    const FrontendResult frontend = compileSource(source, file);
    if (!frontend.ok()) {
        return std::nullopt;
    }
    const Compiler compiler;
    Compilation compilation = compiler.compile(std::move(frontend.program->project));
    if (compilation.hasErrors()) {
        return std::nullopt;
    }
    return compilation;
}

std::string buildFeedback(const Task& task, const VerificationResult& verification,
                          int attempt, bool redactExpected) {
    JsonWriter json;
    json.beginObject();
    json.key("taskId").value(task.id);
    json.key("attempt").value(attempt);
    json.key("validityStatus").value(verification.validityStatus);
    json.key("targetStatus").value(verification.targetStatus);
    json.key("preserveStatus").value(verification.preserveStatus);
    json.key("frameStatus").value(verification.frameStatus);
    json.key("diagnostics").beginArray();
    for (const std::string& diagnostic : verification.diagnostics) {
        json.value(diagnostic);
    }
    json.endArray();
    json.key("comparisons").beginArray();
    for (const ObservationComparison& comparison : verification.comparisons) {
        if (comparison.status == "pass") {
            continue;
        }
        json.beginObject();
        json.key("name").value(comparison.name);
        json.key("status").value(comparison.status);
        json.key("reason").value(comparison.reason);
        json.key("expected").value(redactExpected ? "<redacted>" : comparison.expected);
        json.key("actual").value(comparison.actual);
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

std::string semanticDeltaSummary(const Task& task, const std::string& candidatePath,
                                 bool redactExpected) {
    const std::optional<Compilation> baseline =
        compileForDelta(readFile(task.baselinePath), task.baselinePath);
    const std::optional<Compilation> candidate =
        compileForDelta(readFile(candidatePath), candidatePath);
    if (!baseline.has_value() || !candidate.has_value()) {
        return {};
    }
    const SemanticDelta delta = computeSemanticDelta(*baseline, *candidate);
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.semantic-feedback");
    json.key("causeSummary").value(delta.causeSummary);
    json.key("changes").beginArray();
    for (const SemanticChange& change : delta.changes) {
        json.beginObject();
        json.key("change").value(change.change);
        json.key("objectType").value(change.objectType);
        json.key("key").value(change.key);
        json.key("property").value(change.property);
        json.key("identity").value(change.identity == DeltaIdentity::Value
                                       ? "value"
                                       : change.identity == DeltaIdentity::Generation
                                             ? "generation"
                                             : "instance");
        json.key("cause").value(change.cause == DeltaCause::Direct
                                    ? "direct"
                                    : change.cause == DeltaCause::Dependency
                                          ? "dependency"
                                          : "unknown");
        if (!redactExpected) {
            json.key("before").value(change.before);
            json.key("after").value(change.after);
        }
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace

std::string_view repairFailureName(RepairFailure failure) {
    switch (failure) {
        case RepairFailure::None: return "none";
        case RepairFailure::ModelFailure: return "model-failure";
        case RepairFailure::CompileFailure: return "compile-failure";
        case RepairFailure::TaskFailure: return "task-failure";
        case RepairFailure::InfrastructureError: return "infrastructure-error";
        case RepairFailure::BudgetExhausted: return "budget-exhausted";
    }
    return "infrastructure-error";
}

RepairResult runRepairLoop(ModelAdapter& adapter, const Task& task,
                           const std::string& taskPath,
                           const ModelResponse& initialResponse,
                           const RepairBudget& budget, bool redactExpected) {
    RepairResult result;
    const auto start = std::chrono::steady_clock::now();
    ModelResponse response = initialResponse;
    for (int attempt = 0; attempt < budget.maxAttempts; ++attempt) {
        if (!response.ok) {
            result.failure = RepairFailure::ModelFailure;
            result.reason = response.error.empty() ? "model request failed" : response.error;
            return result;
        }
        if (response.candidateSource.empty()) {
            result.failure = RepairFailure::ModelFailure;
            result.reason = "model returned an empty candidate";
            return result;
        }
        if (budget.maxSeconds > 0) {
            const auto now = std::chrono::steady_clock::now();
            const double seconds =
                std::chrono::duration<double>(now - start).count();
            if (seconds > budget.maxSeconds) {
                result.failure = RepairFailure::BudgetExhausted;
                result.reason = "wall-clock budget exhausted";
                return result;
            }
        }
        RepairAttempt record;
        record.index = attempt;
        record.candidateSource = response.candidateSource;
        record.sourcePath = writeCandidate(response.candidateSource, attempt);
        if (record.sourcePath.empty()) {
            result.failure = RepairFailure::InfrastructureError;
            result.reason = "cannot write candidate to a local working file";
            return result;
        }
        record.inputTokens = response.inputTokens;
        record.outputTokens = response.outputTokens;
        record.cacheTokens = response.cacheTokens;
        record.modelCalls = response.modelCalls;
        result.totalInputTokens += response.inputTokens;
        result.totalOutputTokens += response.outputTokens;
        result.totalCacheTokens += response.cacheTokens;
        result.totalModelCalls += response.modelCalls;
        if (budget.maxInputTokens > 0 && result.totalInputTokens > budget.maxInputTokens) {
            result.failure = RepairFailure::BudgetExhausted;
            result.reason = "input token budget exhausted";
            result.attempts.push_back(std::move(record));
            return result;
        }
        if (budget.maxOutputTokens > 0 && result.totalOutputTokens > budget.maxOutputTokens) {
            result.failure = RepairFailure::BudgetExhausted;
            result.reason = "output token budget exhausted";
            result.attempts.push_back(std::move(record));
            return result;
        }
        record.verification = verifyTaskFile(taskPath, record.sourcePath);
        const VerificationResult verification = record.verification;
        record.feedback = buildFeedback(task, verification, attempt, redactExpected);
        const std::string delta = semanticDeltaSummary(task, record.sourcePath, redactExpected);
        if (!delta.empty()) {
            record.feedback = record.feedback + "\n" + delta;
        }
        result.attempts.push_back(record);
        if (verification.status == "pass") {
            result.success = true;
            result.failure = RepairFailure::None;
            result.finalSource = response.candidateSource;
            return result;
        }
        if (verification.status == "error") {
            result.failure = verification.validityStatus == "failed"
                                 ? RepairFailure::CompileFailure
                                 : RepairFailure::InfrastructureError;
            result.reason = record.feedback;
            // A compile error is repairable: continue if budget remains.
        } else {
            result.failure = RepairFailure::TaskFailure;
            result.reason = record.feedback;
        }
        result.finalSource = response.candidateSource;
        if (attempt + 1 >= budget.maxAttempts) {
            break;
        }
        ModelRequest request;
        request.taskId = task.id;
        request.prompt = "Repair the MPL program. Do not change the task contract.";
        request.previousCandidate = response.candidateSource;
        request.semanticFeedback = record.feedback;
        request.attempt = attempt + 1;
        request.maxAttempts = budget.maxAttempts;
        response = adapter.generate(request);
    }
    result.failure = RepairFailure::BudgetExhausted;
    if (result.reason.empty()) {
        result.reason = "repair attempt budget exhausted";
    }
    return result;
}

std::string RepairResult::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.repair-result");
    json.key("version").value(1);
    json.key("success").value(success);
    json.key("failure").value(std::string(repairFailureName(failure)));
    json.key("reason").value(reason);
    json.key("totalInputTokens").value(totalInputTokens);
    json.key("totalOutputTokens").value(totalOutputTokens);
    json.key("totalCacheTokens").value(totalCacheTokens);
    json.key("totalModelCalls").value(totalModelCalls);
    json.key("attempts").beginArray();
    for (const RepairAttempt& attempt : attempts) {
        json.beginObject();
        json.key("index").value(attempt.index);
        json.key("candidate").value(attempt.candidateSource);
        json.key("status").value(attempt.verification.status);
        json.key("feedback").value(attempt.feedback);
        json.key("inputTokens").value(attempt.inputTokens);
        json.key("outputTokens").value(attempt.outputTokens);
        json.key("cacheTokens").value(attempt.cacheTokens);
        json.key("modelCalls").value(attempt.modelCalls);
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
