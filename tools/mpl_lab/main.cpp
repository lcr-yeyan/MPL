#include "mpl/Compiler.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/lab/Harness.hpp"
#include "mpl/lab/ModelAdapters.hpp"
#include "mpl/lab/Repair.hpp"
#include "mpl/lab/Task.hpp"
#include "mpl/lab/Statistics.hpp"
#include "mpl/lab/TrustedExecution.hpp"
#include "mpl/util/JsonValue.hpp"
#include "mpl/util/Sha256.hpp"
#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <chrono>

namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

int observe(const std::filesystem::path& program, const std::filesystem::path& audio) {
    const std::string source = readText(program);
    const mpl::FrontendResult frontend = mpl::compileSource(source, program.generic_string());
    if (!frontend.ok()) {
        for (const mpl::FrontendDiagnostic& diagnostic : frontend.diagnostics) {
            std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
        }
        return 1;
    }
    const mpl::Compiler compiler;
    const mpl::Compilation compilation =
        compiler.compile(std::move(frontend.program->project));
    mpl::ObservationOptions options;
    options.audioPath = audio;
    const mpl::ObservationSet observations = mpl::observeCompilation(compilation, options);
    std::cout << observations.toJson() << "\n";
    return 0;
}

int verify(const std::filesystem::path& task, const std::filesystem::path& candidate,
           const std::filesystem::path& audio, const std::filesystem::path& manifestPath) {
    if (!audio.empty() && manifestPath.empty()) {
        std::cerr << "verify: --audio requires --manifest; external WAVs are not trusted "
                     "by default\n";
        return 1;
    }
    if (!manifestPath.empty() && audio.empty()) {
        std::cerr << "verify: --manifest requires --audio\n";
        return 1;
    }
    mpl::ObservationOptions options;
    options.audioPath = audio;
    const mpl::VerificationResult result =
        manifestPath.empty()
            ? mpl::verifyTaskFile(task.generic_string(), candidate.generic_string(), options)
            : mpl::verifyTaskWithRenderManifest(task.generic_string(),
                                                candidate.generic_string(),
                                                manifestPath.generic_string(),
                                                audio.generic_string());
    std::cout << result.toJson() << "\n";
    return result.status == "pass" ? 0 : 1;
}

int modelRun(const std::string& arm, const std::filesystem::path& taskPath,
             const std::filesystem::path& promptPath,
             const std::filesystem::path& contextPath,
             const std::filesystem::path& outDirectory, int attempts,
             const std::string& label) {
    const std::string promptBase = readText(promptPath);
    const std::string context = readText(contextPath);
    mpl::TaskParseResult parsed = mpl::parseTaskJson(readText(taskPath));
    if (!parsed.ok) {
        std::cerr << "model: invalid task: " << parsed.error << "\n";
        return 1;
    }
    std::filesystem::create_directories(outDirectory);
    mpl::OpenAiCompatibleAdapter adapter;
    std::string previousCandidate;
    std::string feedback;
    std::string finalCandidate;
    std::string finalStatus = "not-run";
    std::string actualModel = adapter.name();
    int totalInput = 0;
    int totalOutput = 0;
    int totalCalls = 0;
    long long totalLatency = 0;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        mpl::ModelRequest request;
        request.taskId = parsed.task.id;
        request.prompt = promptBase + (previousCandidate.empty()
                                           ? ""
                                           : "\n\nPrevious candidate:\n" + previousCandidate) +
                         (feedback.empty() ? "" : "\n\nFeedback:\n" + feedback);
        request.languageContext = context;
        request.attempt = attempt;
        request.maxAttempts = attempts;
        request.maxOutputTokens = 900;
        request.promptVersion = label;
        const auto callStart = std::chrono::steady_clock::now();
        mpl::ModelResponse response = adapter.generate(request);
        const auto callEnd = std::chrono::steady_clock::now();
        const long long measuredLatency =
            std::chrono::duration_cast<std::chrono::milliseconds>(callEnd - callStart)
                .count();
        totalInput += response.inputTokens;
        totalOutput += response.outputTokens;
        totalCalls += response.modelCalls;
        totalLatency += measuredLatency;
        if (!response.model.empty()) {
            actualModel = response.model;
        }        if (!response.ok) {
            std::ofstream(outDirectory / "error.txt", std::ios::binary) << response.error;
            finalStatus = "model-error";
            break;
        }
        const bool flat = arm.rfind("A", 0) == 0;
        const std::string candidateName =
            "candidate-" + std::to_string(attempt) + (flat ? ".json" : ".mpl");
        const std::filesystem::path candidatePath = outDirectory / candidateName;
        std::ofstream(candidatePath, std::ios::binary | std::ios::trunc)
            << response.candidateSource;
        const mpl::VerificationResult verification =
            mpl::verifyArmCandidate(arm, taskPath.generic_string(),
                                    candidatePath.generic_string());
        std::ofstream(outDirectory / ("verification-" + std::to_string(attempt) + ".json"),
                      std::ios::binary)
            << verification.toJson();
        finalCandidate = candidatePath.generic_string();
        finalStatus = verification.status;
        if (verification.status == "pass") {
            break;
        }
        if (attempt + 1 >= attempts) {
            break;
        }
        previousCandidate = response.candidateSource;
        const bool retryOnly = !arm.empty() && arm.back() == 'R';
        if (retryOnly) {
            feedback = "Retry the same task. No semantic diagnostics are provided.";
        } else {
            mpl::JsonWriter feedbackJson;
            feedbackJson.beginObject();
            feedbackJson.key("validityStatus").value(verification.validityStatus);
            feedbackJson.key("targetStatus").value(verification.targetStatus);
            feedbackJson.key("preserveStatus").value(verification.preserveStatus);
            feedbackJson.key("frameStatus").value(verification.frameStatus);
            feedbackJson.key("diagnostics").beginArray();
            for (const std::string& diagnostic : verification.diagnostics) {
                feedbackJson.value(diagnostic);
            }
            feedbackJson.endArray();
            feedbackJson.key("comparisons").beginArray();
            for (const mpl::ObservationComparison& comparison : verification.comparisons) {
                if (comparison.status == "pass") {
                    continue;
                }
                feedbackJson.beginObject();
                feedbackJson.key("name").value(comparison.name);
                feedbackJson.key("status").value(comparison.status);
                feedbackJson.key("reason").value(comparison.reason);
                feedbackJson.endObject();
            }
            feedbackJson.endArray();
            feedbackJson.endObject();
            feedback = feedbackJson.take();
        }
    }
    mpl::JsonWriter result;
    result.beginObject();
    result.key("format").value("mpl.model-run");
    result.key("version").value(1);
    result.key("label").value(label);
    result.key("arm").value(arm);
    result.key("task").value(parsed.task.id);
    result.key("model").value(actualModel);
    result.key("attempts").value(attempts);
    result.key("status").value(finalStatus);
    result.key("candidate").value(finalCandidate);
    result.key("inputTokens").value(totalInput);
    result.key("outputTokens").value(totalOutput);
    result.key("modelCalls").value(totalCalls);
    result.key("latencyMilliseconds").value(totalLatency);
    result.endObject();
    std::ofstream(outDirectory / "result.json", std::ios::binary) << result.take();
    std::cout << readText(outDirectory / "result.json") << "\n";
    return finalStatus == "pass" ? 0 : 1;
}

int trustedRender(const std::filesystem::path& program,
                  const std::filesystem::path& wavPath) {
    const std::string source = readText(program);
    const mpl::RenderConfiguration configuration;
    const std::filesystem::path manifestPath = wavPath.string() + ".manifest.json";
    const mpl::TrustedRenderResult result =
        mpl::renderTrusted(source, program, configuration, wavPath, manifestPath);
    if (!result.ok) {
        std::cerr << "render: " << result.error << "\n";
        for (const mpl::Diagnostic& diagnostic : result.diagnostics) {
            std::cerr << diagnostic.code << ": " << diagnostic.message << "\n";
        }
        return 1;
    }
    std::cout << result.manifest.toJson() << "\n";
    return 0;
}

int manifest(const std::filesystem::path& program, const std::filesystem::path& audio) {
    const std::string source = readText(program);
    const mpl::FrontendResult frontend = mpl::compileSource(source, program.generic_string());
    if (!frontend.ok()) {
        std::cerr << "manifest: compile failed\n";
        return 1;
    }
    const mpl::Compiler compiler;
    const mpl::Compilation compilation =
        compiler.compile(std::move(frontend.program->project));
    const mpl::RenderConfiguration configuration;
    const mpl::RenderArtifactManifest value =
        mpl::buildRenderManifest(source, compilation, configuration, audio);
    std::cout << value.toJson() << "\n";
    return 0;
}

int checkManifest(const std::filesystem::path& manifestPath,
                  const std::filesystem::path& program,
                  const std::filesystem::path& audio) {
    mpl::RenderArtifactManifest parsed;
    std::string error;
    if (!mpl::parseRenderManifest(readText(manifestPath), parsed, error)) {
        std::cerr << "check-manifest: " << error << "\n";
        return 1;
    }
    const std::string source = readText(program);
    const mpl::FrontendResult frontend = mpl::compileSource(source, program.generic_string());
    if (!frontend.ok()) {
        std::cerr << "check-manifest: compile failed\n";
        return 1;
    }
    const mpl::Compiler compiler;
    const mpl::Compilation compilation =
        compiler.compile(std::move(frontend.program->project));
    if (!mpl::verifyRenderManifest(parsed, source, compilation, parsed.configuration,
                                   audio, error)) {
        std::cerr << "check-manifest: " << error << "\n";
        return 1;
    }
    std::cout << "{\"format\":\"mpl.render-manifest-check\",\"status\":\"pass\"}\n";
    return 0;
}

int freezeCheck(const std::filesystem::path& freezePath) {
    mpl::JsonValue root;
    std::string error;
    if (!mpl::parseJson(readText(freezePath), root, error) || !root.isObject()) {
        std::cerr << "freeze-check: invalid freeze JSON\n";
        return 1;
    }
    const mpl::JsonValue* files = root.find("files");
    if (files == nullptr || !files->isObject()) {
        std::cerr << "freeze-check: freeze has no files object\n";
        return 1;
    }
    int checked = 0;
    int mismatched = 0;
    for (const auto& [path, expected] : files->object) {
        if (!expected.isString()) {
            continue;
        }
        ++checked;
        const std::string actual = mpl::sha256FileHex(path);
        if (actual != expected.text) {
            ++mismatched;
            std::cerr << "freeze-check: mismatch " << path << "\n";
        }
    }
    std::cout << "{\"format\":\"mpl.freeze-check\",\"checked\":" << checked
              << ",\"mismatched\":" << mismatched
              << ",\"status\":\"" << (mismatched == 0 ? "pass" : "fail") << "\"}\n";
    return mismatched == 0 ? 0 : 1;
}

int verifyFlat(const std::filesystem::path& task, const std::filesystem::path& flat) {
    const mpl::VerificationResult result =
        mpl::verifyFlatTaskFile(task.generic_string(), flat.generic_string());
    std::cout << result.toJson() << "\n";
    return result.status == "pass" ? 0 : 1;
}

int statistics(const std::filesystem::path& path) {
    std::vector<std::string> files;
    if (std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                files.push_back(entry.path().generic_string());
            }
        }
    } else {
        files.push_back(path.generic_string());
    }
    std::cout << mpl::analyzeExperimentResults(files) << "\n";
    return 0;
}

int validateTasks(const std::filesystem::path& directory) {
    int failures = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        const mpl::TaskParseResult parsed =
            mpl::parseTaskJson(readText(entry.path()));
        if (!parsed.ok) {
            std::cerr << entry.path().filename().string() << ": " << parsed.error << "\n";
            ++failures;
            continue;
        }
        std::cout << entry.path().filename().string() << " -> " << parsed.task.id
                  << " (" << parsed.task.category << ")\n";
    }
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "mpl_lab observe <program.mpl> [--audio wav]\n"
                     "mpl_lab verify <task.json> <candidate.mpl> [--audio wav --manifest json]\n"
                     "mpl_lab verify-flat <task.json> <candidate-flat.json>\n"
                     "mpl_lab manifest <program.mpl> [--audio wav]\n"
                     "mpl_lab render <program.mpl> <out.wav>\n"
                     "mpl_lab check-manifest <manifest.json> <program.mpl> [--audio wav]\n"
                     "mpl_lab validate <tasks-dir>\n"
                     "mpl_lab run <experiment.json>\n"
                     "mpl_lab stats <results-dir-or-json>\n"
                     "mpl_lab freeze-check <freeze.json>\n"
                     "mpl_lab model <arm> <task> <prompt> <context> <outdir> [attempts] [label]\n";
        return 1;
    }
    const std::string command = argv[1];
    std::filesystem::path audio;
    std::filesystem::path manifestPath;
    int positional = 0;
    std::filesystem::path first;
    std::filesystem::path second;
    std::vector<std::filesystem::path> positionalArgs;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--audio" && index + 1 < argc) {
            audio = argv[++index];
        } else if (argument == "--manifest" && index + 1 < argc) {
            manifestPath = argv[++index];
        } else {
            positionalArgs.push_back(argument);
        }
    }
    if (!positionalArgs.empty()) {
        first = positionalArgs[0];
    }
    if (positionalArgs.size() > 1) {
        second = positionalArgs[1];
    }
    if (command == "observe") {
        return observe(first, audio);
    }
    if (command == "verify") {
        return verify(first, second, audio, manifestPath);
    }
    if (command == "verify-flat") {
        return verifyFlat(first, second);
    }
    if (command == "validate") {
        return validateTasks(first);
    }
    if (command == "manifest") {
        return manifest(first, audio);
    }
    if (command == "render") {
        return trustedRender(first, second);
    }
    if (command == "check-manifest") {
        return checkManifest(first, second, audio);
    }
    if (command == "run") {
        const mpl::ExperimentResult result =
            mpl::runExperiment(first.generic_string());
        std::cout << result.toJson() << "\n";
        return result.runs.empty() ? 1 : 0;
    }
    if (command == "stats") {
        return statistics(first);
    }
    if (command == "freeze-check") {
        return freezeCheck(first);
    }
    if (command == "model") {
        if (positionalArgs.size() < 5) {
            std::cerr << "model requires <arm> <task> <prompt> <context> <outdir>\n";
            return 1;
        }
        const int attempts = positionalArgs.size() > 5
                                 ? std::stoi(positionalArgs[5].string())
                                 : 1;
        const std::string label = positionalArgs.size() > 6
                                      ? positionalArgs[6].string()
                                      : "model-run";
        return modelRun(positionalArgs[0].string(), positionalArgs[1],
                        positionalArgs[2], positionalArgs[3], positionalArgs[4],
                        attempts, label);
    }
    std::cerr << "mpl_lab: unknown command\n";
    return 1;
}
