#include "mpl/lab/Harness.hpp"

#include "mpl/lab/FlatEvents.hpp"
#include "mpl/util/JsonWriter.hpp"
#include "mpl/util/JsonValue.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace mpl {
namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

std::filesystem::path resolveRelative(const std::filesystem::path& base,
                                      const std::string& path) {
    std::filesystem::path result(path);
    if (result.is_relative()) {
        result = base / result;
    }
    return result.lexically_normal();
}

bool usesStdlib(const std::string& source) {
    return source.find("import \"std/") != std::string::npos ||
           source.find("import 'std/") != std::string::npos;
}

VerificationResult runCandidate(const std::string& arm, const std::string& taskPath,
                                const std::string& candidatePath,
                                const std::string& audioPath = {},
                                const std::string& manifestPath = {}) {
    VerificationResult result;
    if (arm.rfind("A", 0) == 0) {
        return verifyFlatTaskFile(taskPath, candidatePath);
    }
    if (arm.rfind("B", 0) == 0 && usesStdlib(readText(candidatePath))) {
        result.taskId = "invalid-arm";
        result.status = "invalid-arm";
        result.validityStatus = "failed";
        result.diagnostics.push_back("MPL Core arm must not import the standard library");
        return result;
    }
    result = (!audioPath.empty() && !manifestPath.empty())
                 ? verifyTaskWithRenderManifest(taskPath, candidatePath, manifestPath,
                                                audioPath)
                 : verifyTaskFile(taskPath, candidatePath);
    if (arm.rfind("B", 0) == 0) {
        for (const std::string& module : result.modulesUsed) {
            if (module.rfind("Std", 0) == 0) {
                result.taskId = "invalid-arm";
                result.status = "invalid-arm";
                result.validityStatus = "failed";
                result.diagnostics.push_back(
                    "MPL Core arm must not use a standard-library module, directly or indirectly");
                return result;
            }
        }
    }
    return result;
}
}  // namespace

ExperimentResult runExperiment(const std::string& experimentPath) {    ExperimentResult result;
    const std::filesystem::path path(experimentPath);
    const std::string text = readText(path);
    JsonValue root;
    std::string error;
    if (text.empty() || !parseJson(text, root, error) || !root.isObject()) {
        result.experimentId = "invalid";
        ExperimentRun run;
        run.status = "error";
        run.reason = "invalid experiment JSON: " + error;
        result.runs.push_back(std::move(run));
        return result;
    }
    const JsonValue* id = root.find("id");
    result.experimentId = id != nullptr && id->isString() ? id->text : "unnamed";
    const JsonValue* task = root.find("task");
    if (task == nullptr || !task->isString()) {
        ExperimentRun run;
        run.status = "error";
        run.reason = "experiment requires string task";
        result.runs.push_back(std::move(run));
        return result;
    }
    const std::filesystem::path taskPath = resolveRelative(path.parent_path(), task->text);
    const TaskParseResult parsed = parseTaskJson(readText(taskPath));
    if (!parsed.ok) {
        ExperimentRun run;
        run.status = "error";
        run.reason = "invalid task: " + parsed.error;
        result.runs.push_back(std::move(run));
        return result;
    }
    result.taskId = parsed.task.id;

    const JsonValue* submissions = root.find("submissions");
    if (submissions == nullptr || !submissions->isArray()) {
        ExperimentRun run;
        run.status = "not-run";
        run.reason = "no submissions supplied";
        result.runs.push_back(std::move(run));
        return result;
    }
    for (const JsonValue& entry : submissions->array) {
        ExperimentRun run;
        if (!entry.isObject()) {
            run.status = "error";
            run.reason = "submission must be an object";
            result.runs.push_back(std::move(run));
            continue;
        }
        const JsonValue* arm = entry.find("arm");
        if (arm == nullptr || !arm->isString()) {
            run.status = "error";
            run.reason = "submission requires string arm";
            result.runs.push_back(std::move(run));
            continue;
        }
        run.arm = arm->text;
        run.feedback = run.arm.size() > 1 && run.arm.back() == '1';
        if (const JsonValue* candidate = entry.find("candidate");
            candidate != nullptr && candidate->isString()) {
            run.candidatePath =
                resolveRelative(path.parent_path(), candidate->text).generic_string();
        }
        if (const JsonValue* audio = entry.find("audio");
            audio != nullptr && audio->isString()) {
            run.audioPath =
                resolveRelative(path.parent_path(), audio->text).generic_string();
        }
        if (const JsonValue* manifest = entry.find("renderManifest");
            manifest != nullptr && manifest->isString()) {
            run.renderManifestPath =
                resolveRelative(path.parent_path(), manifest->text).generic_string();
        }
        std::string initialPath;
        if (const JsonValue* initial = entry.find("initialCandidate");
            initial != nullptr && initial->isString()) {
            initialPath =
                resolveRelative(path.parent_path(), initial->text).generic_string();
        }
        const auto readInt = [&](const char* key, int& out) {
            if (const JsonValue* value = entry.find(key);
                value != nullptr && value->isNumber()) {
                out = static_cast<int>(value->number);
            }
        };
        readInt("inputTokens", run.inputTokens);
        readInt("outputTokens", run.outputTokens);
        readInt("cacheTokens", run.cacheTokens);
        readInt("modelCalls", run.modelCalls);
        readInt("repairs", run.repairs);
        if (const JsonValue* latency = entry.find("latencyMilliseconds");
            latency != nullptr && latency->isNumber()) {
            run.latencyMilliseconds = latency->number;
        }
        const auto readString = [&](const char* key, std::string& out) {
            if (const JsonValue* value = entry.find(key);
                value != nullptr && value->isString()) {
                out = value->text;
            }
        };
        readString("model", run.model);
        readString("modelVersion", run.modelVersion);
        readString("sampling", run.sampling);
        readString("promptVersion", run.promptVersion);
        readString("error", run.error);
        result.modelGenerated = result.modelGenerated || run.modelCalls > 0;
        const auto start = std::chrono::steady_clock::now();
        if (run.arm == "A" || run.arm == "A0" || run.arm == "A1") {
            if (run.arm == "A") {
                run.status = "unsupported";
                run.reason =
                    "direct MIDI-event JSON is not compiled by the MPL task verifier";
            } else if (run.candidatePath.empty()) {
                run.status = "not-run";
                run.reason = "no flat-event candidate supplied";
            } else if (!std::filesystem::exists(run.candidatePath)) {
                run.status = "error";
                run.reason = "candidate file does not exist";
            } else {
                run.candidateBytes = std::filesystem::file_size(run.candidatePath);
                if (!initialPath.empty() && std::filesystem::exists(initialPath)) {
                    run.initialStatus =
                        runCandidate(run.arm, taskPath.generic_string(), initialPath).status;
                }
                run.verification = verifyFlatTaskFile(taskPath.generic_string(),
                                                      run.candidatePath);
                run.status = run.verification.status;
            }
        } else if (run.arm.rfind("B", 0) == 0 || run.arm.rfind("C", 0) == 0) {
            if (run.candidatePath.empty()) {
                run.status = "not-run";
                run.reason = "no candidate path supplied";
            } else if (!std::filesystem::exists(run.candidatePath)) {
                run.status = "error";
                run.reason = "candidate file does not exist";
            } else {
                run.candidateBytes = std::filesystem::file_size(run.candidatePath);
                if (!initialPath.empty() && std::filesystem::exists(initialPath)) {
                    const VerificationResult initial =
                        runCandidate(run.arm, taskPath.generic_string(), initialPath,
                                     run.audioPath, run.renderManifestPath);
                    run.initialStatus = initial.status;
                }
                run.verification =
                    runCandidate(run.arm, taskPath.generic_string(), run.candidatePath,
                                 run.audioPath, run.renderManifestPath);
                run.status = run.verification.status;
                for (const std::string& diagnostic : run.verification.diagnostics) {
                    if (!run.reason.empty()) {
                        run.reason += "; ";
                    }
                    run.reason += diagnostic;
                }
            }
        } else {
            run.status = "unsupported";
            run.reason = "unknown experiment arm";
        }
        const auto end = std::chrono::steady_clock::now();
        const long long measured =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (run.latencyMilliseconds <= 0.0) {
            run.latencyMilliseconds = static_cast<double>(measured);
        }
        run.elapsedMilliseconds = measured;
        result.runs.push_back(std::move(run));
    }
    return result;
}

VerificationResult verifyArmCandidate(const std::string& arm,
                                      const std::string& taskPath,
                                      const std::string& candidatePath,
                                      const std::string& audioPath,
                                      const std::string& renderManifestPath) {
    return runCandidate(arm, taskPath, candidatePath, audioPath, renderManifestPath);
}

std::string experimentSchemaJson() {    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.experiment-schema");
    json.key("version").value(2);
    json.key("submissions").beginArray();
    json.value("{arm, candidate, initialCandidate, inputTokens, outputTokens, cacheTokens, "
               "modelCalls, repairs, latencyMilliseconds, model, modelVersion, sampling, "
               "promptVersion, error}");
    json.endArray();
    json.key("arms").beginArray();
    json.value("A0 flat events; A1 flat + feedback; B0 MPL Core; B1 MPL Core + feedback; "
               "C0 MPL + stdlib; C1 MPL + stdlib + feedback");
    json.endArray();
    json.endObject();
    return json.take();
}

std::string ExperimentResult::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.experiment-result");
    json.key("version").value(2);
    json.key("experimentId").value(experimentId);
    json.key("taskId").value(taskId);
    json.key("modelGenerated").value(modelGenerated);

    int totalInput = 0;
    int totalOutput = 0;
    int totalCache = 0;
    int totalCalls = 0;
    int passed = 0;
    int completed = 0;
    for (const ExperimentRun& run : runs) {
        totalInput += run.inputTokens;
        totalOutput += run.outputTokens;
        totalCache += run.cacheTokens;
        totalCalls += run.modelCalls;
        if (run.status == "pass") {
            ++passed;
        }
        if (run.status == "pass" || run.status == "fail" || run.status == "unverifiable") {
            ++completed;
        }
    }
    json.key("totals").beginObject();
    json.key("runs").value(static_cast<std::int64_t>(runs.size()));
    json.key("inputTokens").value(totalInput);
    json.key("outputTokens").value(totalOutput);
    json.key("cacheTokens").value(totalCache);
    json.key("modelCalls").value(totalCalls);
    json.key("passed").value(passed);
    json.key("completed").value(completed);
    json.endObject();
    if (modelGenerated && completed > 0) {
        json.key("observedSuccessRate").value(static_cast<double>(passed) / completed);
    } else {
        json.key("observedSuccessRate").nullValue();
    }

    json.key("runs").beginArray();
    for (const ExperimentRun& run : runs) {
        json.beginObject();
        json.key("arm").value(run.arm);
        json.key("feedback").value(run.feedback);
        json.key("candidate").value(run.candidatePath);
        json.key("audio").value(run.audioPath);
        json.key("renderManifest").value(run.renderManifestPath);
        json.key("status").value(run.status);
        json.key("initialStatus").value(run.initialStatus);
        json.key("reason").value(run.reason);
        json.key("elapsedMilliseconds").value(run.elapsedMilliseconds);
        json.key("latencyMilliseconds").value(run.latencyMilliseconds);
        json.key("candidateBytes").value(static_cast<std::int64_t>(run.candidateBytes));
        json.key("inputTokens").value(run.inputTokens);
        json.key("outputTokens").value(run.outputTokens);
        json.key("cacheTokens").value(run.cacheTokens);
        json.key("modelCalls").value(run.modelCalls);
        json.key("repairs").value(run.repairs);
        json.key("model").value(run.model);
        json.key("modelVersion").value(run.modelVersion);
        json.key("sampling").value(run.sampling);
        json.key("promptVersion").value(run.promptVersion);
        json.key("error").value(run.error);
        if (!run.verification.taskId.empty()) {
            json.key("verification").value(run.verification.toJson());
        }
        json.endObject();
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
