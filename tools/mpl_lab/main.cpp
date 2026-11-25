#include "mpl/Compiler.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/knowledge/AdaptiveContext.hpp"
#include "mpl/knowledge/Knowledge.hpp"
#include "mpl/knowledge/KnowledgeResolver.hpp"
#include "mpl/knowledge/KnowledgeVerifier.hpp"
#include "mpl/knowledge/TaskRequirements.hpp"
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
#include <set>
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

int knowledgeCommand(const std::vector<std::filesystem::path>& args) {
    if (args.empty()) {
        std::cerr << "knowledge requires list/describe/verify/resolve\n";
        return 1;
    }
    const std::string sub = args[0].string();
    mpl::KnowledgeRegistry registry;
    std::string error;
    if (!registry.loadCatalog("knowledge/v1/catalog.json", error)) {
        std::cerr << "knowledge: " << error << "\n";
        return 1;
    }
    if (sub == "list") {
        if (args.size() > 1 && args[1].string() == "--json") {
            mpl::JsonWriter json;
            json.beginObject();
            json.key("format").value("mpl.knowledge-list");
            json.key("version").value(1);
            json.key("units").beginArray();
            for (const mpl::KnowledgeUnit& unit : registry.units()) {
                json.beginObject();
                json.key("knowledgeId").value(unit.knowledgeId);
                json.key("version").value(unit.version);
                json.key("category").value(unit.category);
                json.key("kind").value(unit.kind);
                json.key("verificationStatus").value(unit.verificationStatus);
                json.key("contentHash").value(unit.contentHash);
                json.key("resolvedHash").value(unit.resolvedHash);
                json.endObject();
            }
            json.endArray();
            json.endObject();
            std::cout << json.take() << "\n";
            return 0;
        }
        for (const mpl::KnowledgeUnit& unit : registry.units()) {
            std::cout << unit.knowledgeId << "@" << unit.version << " "
                      << unit.category << " " << unit.kind << " "
                      << unit.verificationStatus << "\n";
        }
        return 0;
    }
    if (sub == "describe" && args.size() > 1) {
        const mpl::KnowledgeUnit* unit = registry.resolve(args[1].string());
        if (unit == nullptr) {
            std::cerr << "knowledge: unknown id\n";
            return 1;
        }
        std::cout << mpl::knowledgeCanonicalJson(*unit, true) << "\n";
        return 0;
    }
    if (sub == "verify" && args.size() > 1) {
        const mpl::KnowledgeUnit* unit = registry.resolve(args[1].string());
        if (unit == nullptr) {
            std::cerr << "knowledge: unknown id\n";
            return 1;
        }
        const mpl::KnowledgeVerification verification =
            mpl::verifyKnowledge(*unit, registry);
        std::cout << verification.toJson() << "\n";
        return verification.status == mpl::KnowledgeCheckStatus::Pass ? 0 : 1;
    }
    if (sub == "resolve" && args.size() > 1) {
        mpl::TaskParseResult parsed =
            mpl::parseTaskJson(readText(args[1]));
        if (!parsed.ok) {
            std::cerr << "knowledge: " << parsed.error << "\n";
            return 1;
        }
        const std::filesystem::path baselinePath =
            std::filesystem::path(parsed.task.baselinePath).is_relative()
                ? args[1].parent_path() / parsed.task.baselinePath
                : std::filesystem::path(parsed.task.baselinePath);
        const std::string baseline = readText(baselinePath);
        const mpl::TaskRequirementSet requirements =
            mpl::analyzeTaskRequirements(parsed.task, baseline);
        const mpl::KnowledgeResolution resolution =
            mpl::resolveKnowledge(requirements, registry);
        mpl::JsonWriter wrapper;
        wrapper.beginObject();
        wrapper.key("format").value("mpl.task-analysis");
        wrapper.key("version").value(1);
        wrapper.key("requirements").value(requirements.toJson());
        wrapper.key("resolution").value(resolution.toJson());
        wrapper.endObject();
        std::cout << wrapper.take() << "\n";
        return 0;
    }
    std::cerr << "knowledge: unknown subcommand\n";
    return 1;
}

int contextCommand(const std::vector<std::filesystem::path>& args) {
    if (args.size() < 2) {
        std::cerr << "context requires build/compare\n";
        return 1;
    }
    const std::string sub = args[0].string();
    mpl::KnowledgeRegistry registry;
    std::string error;
    if (!registry.loadCatalog("knowledge/v1/catalog.json", error)) {
        std::cerr << "context: " << error << "\n";
        return 1;
    }
    auto loadTask = [&](const std::filesystem::path& path, mpl::Task& task) {
        mpl::TaskParseResult parsed = mpl::parseTaskJson(readText(path));
        if (!parsed.ok) {
            return false;
        }
        task = parsed.task;
        std::filesystem::path baseline(task.baselinePath);
        if (baseline.is_relative()) {
            baseline = path.parent_path() / baseline;
        }
        task.baselinePath = baseline.lexically_normal().generic_string();
        return true;
    };
    if (sub == "build" && args.size() >= 4) {
        mpl::Task task;
        if (!loadTask(args[1], task)) {
            std::cerr << "context: invalid task\n";
            return 1;
        }
        const std::string modeText = args[2].string();
        mpl::ContextBuildOptions options;
        options.mode = modeText == "full" ? mpl::ContextMode::Full
                       : modeText == "domain" ? mpl::ContextMode::Domain
                                                : mpl::ContextMode::Adaptive;
        options.foundationPath = "lcp/v1.1/contexts/L1.md";
        options.fullPath = "lcp/v1.1/contexts/Core.md";
        if (args.size() > 4) {
            options.tokenBudget = static_cast<std::size_t>(std::stoi(args[4].string()));
        }
        if (args.size() > 5) {
            options.domain = args[5].string();
        }
        const std::string baseline = readText(task.baselinePath);
        const mpl::TaskRequirementSet requirements =
            mpl::analyzeTaskRequirements(task, baseline);
        const mpl::KnowledgeResolution resolution =
            mpl::resolveKnowledge(requirements, registry, options.domain);
        const mpl::ContextBuildReport report =
            mpl::buildTaskContext(task, resolution, registry, options);
        std::ofstream(args[3], std::ios::binary | std::ios::trunc) << report.content;
        std::ofstream(std::string(args[3].string()) + ".report.json", std::ios::binary)
            << report.toJson();
        std::cout << report.toJson() << "\n";
        return report.budgetStatus == "insufficient-budget" ? 1 : 0;
    }
    if (sub == "validate" && args.size() >= 3) {
        const std::string context = readText(args[2]);
        const bool hasTask = context.find("Task Contract") != std::string::npos;
        const bool hasLanguage = context.find("MPL") != std::string::npos;
        const bool ok = !context.empty() && hasTask && hasLanguage;
        mpl::JsonWriter json;
        json.beginObject();
        json.key("format").value("mpl.context-validation");
        json.key("version").value(1);
        json.key("status").value(ok ? "pass" : "fail");
        json.key("bytes").value(static_cast<std::int64_t>(context.size()));
        json.key("sha256").value(mpl::sha256Hex(context));
        json.key("hasTaskContract").value(hasTask);
        json.key("hasLanguage").value(hasLanguage);
        json.endObject();
        std::cout << json.take() << "\n";
        return ok ? 0 : 1;
    }
    if (sub == "compare" && args.size() >= 3) {        mpl::JsonValue config;
        std::string parseError;
        if (!mpl::parseJson(readText(args[1]), config, parseError) ||
            !config.isObject()) {
            std::cerr << "context: invalid compare config\n";
            return 1;
        }
        const mpl::JsonValue* tasks = config.find("tasks");
        if (tasks == nullptr || !tasks->isArray()) {
            std::cerr << "context: compare config requires tasks\n";
            return 1;
        }
        mpl::JsonWriter output;
        output.beginObject();
        output.key("format").value("mpl.context-accessibility");
        output.key("version").value(1);
        output.key("tasks").beginArray();
        std::size_t totalFullBytes = 0;
        std::size_t totalDomainBytes = 0;
        std::size_t totalAdaptiveBytes = 0;
        std::size_t fullCount = 0;
        std::size_t domainCount = 0;
        std::size_t adaptiveCount = 0;
        for (const mpl::JsonValue& entry : tasks->array) {
            if (!entry.isObject()) continue;
            const mpl::JsonValue* taskValue = entry.find("task");
            if (taskValue == nullptr || !taskValue->isString()) continue;
            const std::filesystem::path taskPath =
                std::filesystem::path(args[1]).parent_path() / taskValue->text;
            mpl::Task task;
            if (!loadTask(taskPath, task)) continue;
            const std::string domain = entry.find("domain") != nullptr &&
                                               entry.find("domain")->isString()
                                           ? entry.find("domain")->text
                                           : "";
            const std::string baseline = readText(task.baselinePath);
            const mpl::TaskRequirementSet requirements =
                mpl::analyzeTaskRequirements(task, baseline);
            const mpl::KnowledgeResolution resolution =
                mpl::resolveKnowledge(requirements, registry, domain);
            output.beginObject();
            output.key("task").value(task.id);
            output.key("domain").value(domain);
            output.key("modes").beginArray();
            for (const mpl::ContextMode mode :
                 {mpl::ContextMode::Full, mpl::ContextMode::Domain,
                  mpl::ContextMode::Adaptive}) {
                mpl::ContextBuildOptions options;
                options.mode = mode;
                options.domain = domain;
                options.tokenBudget = 3000;
                options.foundationPath = "lcp/v1.1/contexts/L1.md";
                options.fullPath = "lcp/v1.1/contexts/Core.md";
                const mpl::ContextBuildReport first =
                    mpl::buildTaskContext(task, resolution, registry, options);
                const mpl::ContextBuildReport second =
                    mpl::buildTaskContext(task, resolution, registry, options);
                std::set<std::string> required;
                for (const mpl::KnowledgeUnit* unit : registry.byCategory(domain)) {
                    required.insert(unit->knowledgeId);
                }
                std::set<std::string> selected(first.selectedKnowledge.begin(),
                                               first.selectedKnowledge.end());
                std::size_t covered = 0;
                for (const std::string& id : required) {
                    covered += selected.count(id) != 0 ? 1 : 0;
                }
                bool depsComplete = true;
                for (const std::string& id : selected) {
                    const mpl::KnowledgeUnit* unit = registry.resolve(id);
                    if (unit == nullptr) continue;
                    for (const std::string& dependency : unit->dependencies) {
                        depsComplete = depsComplete && selected.count(dependency) != 0;
                    }
                }
                output.beginObject();
                output.key("mode").value(std::string(mpl::contextModeName(mode)));
                output.key("bytes").value(static_cast<std::int64_t>(first.bytes));
                output.key("estimatedTokens")
                    .value(static_cast<std::int64_t>(first.estimatedTokens));
                output.key("budgetStatus").value(first.budgetStatus);
                output.key("overflow").value(first.overflow);
                output.key("coverage")
                    .value(required.empty() ? 1.0
                                            : static_cast<double>(covered) /
                                                  static_cast<double>(required.size()));
                output.key("dependencyComplete").value(depsComplete);
                output.key("duplicateLines")
                    .value(static_cast<std::int64_t>(first.duplicateLines));
                output.key("elapsedMilliseconds").value(first.elapsedMilliseconds);
                output.key("deterministic")
                    .value(first.contextHash == second.contextHash);
                output.key("contextHash").value(first.contextHash);
                output.key("selectedKnowledge")
                    .value(static_cast<std::int64_t>(first.selectedKnowledge.size()));
                std::size_t irrelevant = 0;
                for (const std::string& id : first.selectedKnowledge) {
                    irrelevant += required.count(id) == 0 ? 1 : 0;
                }
                output.key("irrelevantKnowledge")
                    .value(static_cast<std::int64_t>(irrelevant));
                output.key("excludedKnowledge")
                    .value(static_cast<std::int64_t>(first.excludedKnowledge.size()));
                output.endObject();
                if (mode == mpl::ContextMode::Full) {
                    totalFullBytes += first.bytes;
                    ++fullCount;
                } else if (mode == mpl::ContextMode::Domain) {
                    totalDomainBytes += first.bytes;
                    ++domainCount;
                } else {
                    totalAdaptiveBytes += first.bytes;
                    ++adaptiveCount;
                }
            }
            output.endArray();
            output.endObject();
        }
        output.endArray();
        output.key("averages").beginObject();
        output.key("fullBytes").value(fullCount == 0 ? 0 : static_cast<std::int64_t>(totalFullBytes / fullCount));
        output.key("domainBytes").value(domainCount == 0 ? 0 : static_cast<std::int64_t>(totalDomainBytes / domainCount));
        output.key("adaptiveBytes").value(adaptiveCount == 0 ? 0 : static_cast<std::int64_t>(totalAdaptiveBytes / adaptiveCount));
        output.endObject();
        output.endObject();
        std::ofstream(args[2], std::ios::binary | std::ios::trunc) << output.take();
        std::cout << readText(args[2]) << "\n";
        return 0;
    }
    std::cerr << "context: unknown subcommand\n";
    return 1;
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
                     "mpl_lab model <arm> <task> <prompt> <context> <outdir> [attempts] [label]\n"
                     "mpl_lab knowledge list|describe|verify|resolve ...\n"
                     "mpl_lab context build|compare ...\n";
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
    if (command == "knowledge") {
        return knowledgeCommand(positionalArgs);
    }
    if (command == "context") {
        return contextCommand(positionalArgs);
    }
    std::cerr << "mpl_lab: unknown command\n";
    return 1;
}
