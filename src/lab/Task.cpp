#include "mpl/lab/Task.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/analysis/SemanticDelta.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/lab/FlatEvents.hpp"
#include "mpl/lab/RenderArtifacts.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <chrono>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace mpl {
namespace {

struct CompiledProgram {
    bool ok = false;
    Compilation compilation;
    std::string diagnostics;
};

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

CompiledProgram compileFile(const std::filesystem::path& path) {
    CompiledProgram result;
    const std::string source = readFile(path);
    if (source.empty() && !std::filesystem::exists(path)) {
        result.diagnostics = "cannot read '" + path.generic_string() + "'";
        return result;
    }
    const FrontendResult frontend = compileSource(source, path.generic_string());
    if (!frontend.ok()) {
        for (const FrontendDiagnostic& diagnostic : frontend.diagnostics) {
            if (!result.diagnostics.empty()) {
                result.diagnostics += "; ";
            }
            result.diagnostics += diagnostic.code + ": " + diagnostic.message;
        }
        return result;
    }
    const Compiler compiler;
    result.compilation = compiler.compile(std::move(frontend.program->project));
    if (result.compilation.hasErrors()) {
        for (const Diagnostic& diagnostic : result.compilation.diagnostics) {
            if (!result.diagnostics.empty()) {
                result.diagnostics += "; ";
            }
            result.diagnostics += diagnostic.code + ": " + diagnostic.message;
        }
        return result;
    }
    result.ok = true;
    return result;
}

bool parseExpected(const std::string& text, JsonValue& value, std::string& error) {
    return parseJson(text, value, error);
}

const ObservationSpec* specFor(const std::string& name) {
    return findObservationSpec(name);
}

ObservationComparison compareObservation(const ObservationExpectation& expectation,
                                         const ObservationValue* actual,
                                         const JsonValue& expected,
                                         double taskTolerance) {
    ObservationComparison comparison;
    comparison.name = expectation.name;
    comparison.expected = expected.canonical();
    if (actual == nullptr) {
        comparison.status = "fail";
        comparison.reason = "observation is not present in the registry output";
        return comparison;
    }
    comparison.actual = actual->jsonValue;
    if (actual->status == "unverifiable" || !actual->available) {
        comparison.status = "unverifiable";
        comparison.reason = actual->reason.empty() ? "observation unavailable" : actual->reason;
        return comparison;
    }
    JsonValue actualValue;
    std::string error;
    if (!parseExpected(actual->jsonValue, actualValue, error)) {
        comparison.status = "fail";
        comparison.reason = "malformed observation value: " + error;
        return comparison;
    }
    const ObservationSpec* spec = specFor(expectation.name);
    if (spec != nullptr && spec->kind == ObservationKind::Tolerance) {
        if (!actualValue.isNumber() || !expected.isNumber()) {
            comparison.status = "fail";
            comparison.reason = "tolerance comparison requires numeric values";
            return comparison;
        }
        const double tolerance =
            std::max(taskTolerance, expectation.tolerance);
        comparison.status = std::fabs(actualValue.number - expected.number) <= tolerance
                                ? "pass"
                                : "fail";
        if (comparison.status == "fail") {
            comparison.reason = "numeric difference exceeds tolerance";
        }
        return comparison;
    }
    comparison.status = actualValue.canonical() == expected.canonical() ? "pass" : "fail";
    if (comparison.status == "fail") {
        comparison.reason = "exact structure differs";
    }
    return comparison;
}

bool loadTaskFile(const std::string& taskPath, Task& task, std::string& error) {    const std::filesystem::path path(taskPath);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "cannot read task '" + taskPath + "'";
        return false;
    }
    const std::string text{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
    TaskParseResult parsed = parseTaskJson(text);
    if (!parsed.ok) {
        error = parsed.error;
        return false;
    }
    task = std::move(parsed.task);
    std::filesystem::path baseline(task.baselinePath);
    if (baseline.is_relative()) {
        baseline = path.parent_path() / baseline;
    }
    task.baselinePath = baseline.lexically_normal().generic_string();
    return true;
}

}  // namespace

TaskParseResult parseTaskJson(std::string_view text) {
    TaskParseResult result;
    JsonValue root;
    std::string error;
    if (!parseJson(text, root, error)) {
        result.error = "invalid JSON: " + error;
        return result;
    }
    if (!root.isObject()) {
        result.error = "task must be a JSON object";
        return result;
    }
    const JsonValue* id = root.find("id");
    const JsonValue* baseline = root.find("baseline");
    if (id == nullptr || !id->isString() || baseline == nullptr || !baseline->isString()) {
        result.error = "task requires string id and baseline";
        return result;
    }
    Task& task = result.task;
    task.id = id->text;
    task.baselinePath = baseline->text;
    if (const JsonValue* title = root.find("title"); title != nullptr && title->isString()) {
        task.title = title->text;
    }
    if (const JsonValue* category = root.find("category");
        category != nullptr && category->isString()) {
        task.category = category->text;
    }
    if (const JsonValue* scope = root.find("allowedScope");
        scope != nullptr && scope->isString()) {
        task.allowedScope = scope->text;
    }
    if (const JsonValue* templateId = root.find("template");
        templateId != nullptr && templateId->isString()) {
        task.templateId = templateId->text;
    }
    if (const JsonValue* seed = root.find("seed");
        seed != nullptr && seed->isNumber()) {
        task.seed = static_cast<int>(seed->number);
    }
    if (const JsonValue* tolerance = root.find("tolerance");
        tolerance != nullptr && tolerance->isNumber()) {
        task.tolerance = tolerance->number;
    }
    if (const JsonValue* budget = root.find("budget"); budget != nullptr && budget->isObject()) {
        if (const JsonValue* repairs = budget->find("maxRepairs");
            repairs != nullptr && repairs->isNumber()) {
            task.budget.maxRepairs = static_cast<int>(repairs->number);
        }
        if (const JsonValue* seconds = budget->find("maxSeconds");
            seconds != nullptr && seconds->isNumber()) {
            task.budget.maxSeconds = seconds->number;
        }
        if (const JsonValue* tokens = budget->find("maxTokens");
            tokens != nullptr && tokens->isNumber()) {
            task.budget.maxTokens = static_cast<int>(tokens->number);
        }
    }
    if (const JsonValue* frame = root.find("frame"); frame != nullptr) {
        if (!frame->isObject()) {
            result.error = "frame must be an object";
            return result;
        }
        task.frame.specified = true;
        const auto parseRules = [&](const char* key, std::vector<FrameRule>& out) {
            const JsonValue* rules = frame->find(key);
            if (rules == nullptr) {
                return true;
            }
            if (!rules->isArray()) {
                result.error = std::string("frame.") + key + " must be an array";
                return false;
            }
            for (const JsonValue& entry : rules->array) {
                if (!entry.isObject()) {
                    result.error = std::string("frame.") + key + " entries must be objects";
                    return false;
                }
                FrameRule rule;
                if (const JsonValue* object = entry.find("object");
                    object != nullptr && object->isString()) {
                    rule.objectType = object->text;
                }
                if (const JsonValue* keyValue = entry.find("key");
                    keyValue != nullptr && keyValue->isString()) {
                    rule.key = keyValue->text;
                }
                if (const JsonValue* property = entry.find("property");
                    property != nullptr && property->isString()) {
                    rule.property = property->text;
                }
                if (const JsonValue* cause = entry.find("cause");
                    cause != nullptr && cause->isString()) {
                    rule.cause = cause->text;
                }
                out.push_back(std::move(rule));
            }
            return true;
        };
        if (!parseRules("allow", task.frame.allow) ||
            !parseRules("deny", task.frame.deny)) {
            return result;
        }
    }
    const auto parseList = [&](const char* key, std::vector<ObservationExpectation>& out,
                               bool requireExpected) {
        const JsonValue* list = root.find(key);
        if (list == nullptr) {
            return true;
        }
        if (!list->isArray()) {
            result.error = std::string(key) + " must be an array";
            return false;
        }
        for (const JsonValue& entry : list->array) {
            if (!entry.isObject()) {
                result.error = std::string(key) + " entries must be objects";
                return false;
            }
            const JsonValue* name = entry.find("name");
            if (name == nullptr || !name->isString()) {
                result.error = std::string(key) + " entry requires string name";
                return false;
            }
            if (findObservationSpec(name->text) == nullptr) {
                result.error = "unknown observation '" + name->text + "'";
                return false;
            }
            ObservationExpectation expectation;
            expectation.name = name->text;
            if (const JsonValue* value = entry.find("value"); value != nullptr) {
                expectation.expected = *value;
                expectation.hasExpected = true;
            }
            if (const JsonValue* fromBaseline = entry.find("fromBaseline");
                fromBaseline != nullptr && fromBaseline->isBool()) {
                expectation.fromBaseline = fromBaseline->boolean;
            }
            if (const JsonValue* tolerance = entry.find("tolerance");
                tolerance != nullptr && tolerance->isNumber()) {
                expectation.tolerance = tolerance->number;
            }
            if (requireExpected && !expectation.hasExpected && !expectation.fromBaseline) {
                result.error = "target observation '" + expectation.name +
                               "' requires value or fromBaseline";
                return false;
            }
            out.push_back(std::move(expectation));
        }
        return true;
    };
    if (!parseList("target", task.target, true) ||
        !parseList("preserve", task.preserve, false)) {
        return result;
    }
    if (task.target.empty()) {
        result.error = "task requires at least one target observation";
        return result;
    }
    if (task.allowedScope != "program" && task.allowedScope != "none") {
        result.error = "unsupported allowedScope '" + task.allowedScope + "'";
        return result;
    }
    result.ok = true;
    return result;
}

bool matchPattern(const std::string& pattern, const std::string& value) {
    return pattern == "*" || pattern == value;
}

std::string causeName(DeltaCause cause) {
    return cause == DeltaCause::Direct
               ? "direct"
               : cause == DeltaCause::Dependency ? "dependency" : "unknown";
}

bool frameRuleMatches(const FrameRule& rule, const SemanticChange& change) {
    return matchPattern(rule.objectType, change.objectType) &&
           matchPattern(rule.key, change.key) && matchPattern(rule.property, change.property) &&
           matchPattern(rule.cause, causeName(change.cause));
}

FrameCoverage frameCoverage() {
    FrameCoverage coverage;
    coverage.objectTypes = {"track", "clip", "section", "harmony", "production"};
    coverage.properties = {
        "track.instrument", "track.role", "track.assetBindings",
        "clip.start", "clip.duration", "clip.symbolicEvents", "clip.instanceId",
        "clip.sourceObject", "section.start", "section.duration", "harmony.timeline",
        "production.tempo", "production.meter", "production.logicalExtent",
        "production.mix", "production.automation", "production.temporalCurves"};
    coverage.unobserved = {"objectIds"};
    return coverage;
}

bool frameRuleCovered(const FrameRule& rule) {
    if (rule.objectType == "*" || rule.property == "*") {
        return true;
    }
    const FrameCoverage coverage = frameCoverage();
    if (std::find(coverage.objectTypes.begin(), coverage.objectTypes.end(),
                  rule.objectType) == coverage.objectTypes.end()) {
        return false;
    }
    const std::string property = rule.objectType + "." + rule.property;
    return std::find(coverage.properties.begin(), coverage.properties.end(), property) !=
           coverage.properties.end();
}

void evaluateFrame(const Task& task, const Compilation& baseline,
                   const Compilation& candidate, VerificationResult& result) {
    if (!task.frame.specified) {
        result.frameStatus = task.allowedScope == "none" ? "restricted" : "unrestricted";
        return;
    }
    const SemanticDelta delta = computeSemanticDelta(baseline, candidate);
    bool failed = false;
    bool coverageIssue = false;
    const auto checkCoverage = [&](const FrameRule& rule) {
        if (!frameRuleCovered(rule)) {
            coverageIssue = true;
            ObservationComparison comparison;
            comparison.name = "frame-coverage";
            comparison.status = "unverifiable";
            comparison.reason = "frame rule references an object/property outside the "
                                "instrumented semantic delta: " +
                                rule.objectType + "." + rule.property;
            result.comparisons.push_back(std::move(comparison));
        }
    };
    for (const FrameRule& rule : task.frame.allow) {
        checkCoverage(rule);
    }
    for (const FrameRule& rule : task.frame.deny) {
        checkCoverage(rule);
    }
    for (const SemanticChange& change : delta.changes) {
        bool allowed = false;
        for (const FrameRule& rule : task.frame.allow) {
            if (frameRuleMatches(rule, change)) {
                allowed = true;
                break;
            }
        }
        for (const FrameRule& rule : task.frame.deny) {
            if (frameRuleMatches(rule, change)) {
                allowed = false;
                break;
            }
        }
        if (!allowed) {
            failed = true;
            ObservationComparison comparison;
            comparison.name = "frame:" + change.objectType + "." + change.property;
            comparison.status = "fail";
            comparison.expected = "allowed by frame contract";
            comparison.actual = change.after;
            comparison.reason = "unauthorized " + change.change + " of '" + change.key +
                                "' (" + change.property + "); before=" + change.before +
                                "; source=" + change.source + "; cause=" +
                                (change.cause == DeltaCause::Direct
                                     ? "direct"
                                     : change.cause == DeltaCause::Dependency ? "dependency"
                                                                              : "unknown");
            result.comparisons.push_back(std::move(comparison));
        }
    }
    result.frameStatus = failed ? "failed" : coverageIssue ? "unverifiable" : "pass";
    if (failed) {
        result.status = "fail";
    } else if (coverageIssue && result.status == "pass") {
        result.status = "unverifiable";
    }
}

VerificationResult verifyObservationSets(const Task& task,
                                         const ObservationSet& baselineObservations,
                                         const ObservationSet& candidateObservations) {
    VerificationResult result;
    result.taskId = task.id;
    result.validityStatus = "ok";
    bool failed = false;
    bool unverifiable = false;
    bool targetFailed = false;
    bool targetUnverifiable = false;
    bool preserveFailed = false;
    bool preserveUnverifiable = false;
    for (const ObservationExpectation& expectation : task.target) {
        JsonValue expected;
        if (expectation.fromBaseline) {
            const ObservationValue* baselineValue = baselineObservations.find(expectation.name);
            if (baselineValue == nullptr || baselineValue->status == "unverifiable") {
                unverifiable = true;
                targetUnverifiable = true;
                ObservationComparison comparison;
                comparison.name = expectation.name;
                comparison.status = "unverifiable";
                comparison.reason = "baseline observation is unavailable";
                result.comparisons.push_back(std::move(comparison));
                continue;
            }
            std::string error;
            if (!parseJson(baselineValue->jsonValue, expected, error)) {
                failed = true;
                targetFailed = true;
                ObservationComparison comparison;
                comparison.name = expectation.name;
                comparison.status = "fail";
                comparison.reason = "baseline observation is malformed: " + error;
                result.comparisons.push_back(std::move(comparison));
                continue;
            }
        } else {
            expected = expectation.expected;
        }
        const ObservationComparison comparison = compareObservation(
            expectation, candidateObservations.find(expectation.name), expected, task.tolerance);
        if (comparison.status == "fail") {
            failed = true;
            targetFailed = true;
        } else if (comparison.status == "unverifiable") {
            unverifiable = true;
            targetUnverifiable = true;
        }
        result.comparisons.push_back(comparison);
    }
    for (const ObservationExpectation& expectation : task.preserve) {
        const ObservationValue* baselineValue = baselineObservations.find(expectation.name);
        if (baselineValue == nullptr || baselineValue->status == "unverifiable") {
            unverifiable = true;
            preserveUnverifiable = true;
            ObservationComparison comparison;
            comparison.name = "preserve:" + expectation.name;
            comparison.status = "unverifiable";
            comparison.reason = "baseline observation is unavailable";
            result.comparisons.push_back(std::move(comparison));
            continue;
        }
        JsonValue expected;
        std::string error;
        if (!parseJson(baselineValue->jsonValue, expected, error)) {
            failed = true;
            preserveFailed = true;
            ObservationComparison comparison;
            comparison.name = "preserve:" + expectation.name;
            comparison.status = "fail";
            comparison.reason = "baseline observation is malformed: " + error;
            result.comparisons.push_back(std::move(comparison));
            continue;
        }
        ObservationComparison comparison = compareObservation(
            expectation, candidateObservations.find(expectation.name), expected, task.tolerance);
        comparison.name = "preserve:" + expectation.name;
        if (comparison.status == "fail") {
            failed = true;
            preserveFailed = true;
        } else if (comparison.status == "unverifiable") {
            unverifiable = true;
            preserveUnverifiable = true;
        }
        result.comparisons.push_back(std::move(comparison));
    }
    result.targetStatus = targetFailed ? "fail"
                                       : targetUnverifiable ? "unverifiable" : "pass";
    result.preserveStatus = preserveFailed ? "fail"
                                           : preserveUnverifiable ? "unverifiable" : "pass";
    result.status = failed ? "fail" : unverifiable ? "unverifiable" : "pass";
    return result;
}

VerificationResult verifyTaskSource(const Task& task, std::string_view candidateSource,
                                    const std::string& candidateFile,
                                    const ObservationOptions& options) {
    VerificationResult result;
    result.taskId = task.id;

    const std::filesystem::path baselinePath(task.baselinePath);
    const CompiledProgram baseline = compileFile(baselinePath);
    if (!baseline.ok) {
        result.status = "error";
        result.validityStatus = "failed";
        result.diagnostics.push_back("baseline compile failed: " + baseline.diagnostics);
        return result;
    }
    const CompiledProgram candidate = compileFile(candidateFile);
    if (!candidate.ok) {
        result.status = "error";
        result.validityStatus = "failed";
        result.diagnostics.push_back("candidate compile failed: " + candidate.diagnostics);
        return result;
    }
    if (task.allowedScope == "none") {
        const std::string baselineSource = readFile(baselinePath);
        if (std::string(candidateSource) != baselineSource) {
            ObservationComparison comparison;
            comparison.name = "allowed_scope";
            comparison.status = "fail";
            comparison.reason = "allowedScope is 'none' but candidate source changed";
            result.comparisons.push_back(std::move(comparison));
            result.status = "fail";
            return result;
        }
    }

    result = verifyObservationSets(
        task, observeCompilation(baseline.compilation, options),
        observeCompilation(candidate.compilation, options));
    result.validityStatus = "ok";
    for (const ModuleRecord& module : candidate.compilation.modules) {
        result.modulesUsed.push_back(module.name);
    }
    evaluateFrame(task, baseline.compilation, candidate.compilation, result);
    return result;
}

VerificationResult verifyTaskFile(const std::string& taskPath,
                                  const std::string& candidatePath,
                                  const ObservationOptions& options) {
    Task task;
    std::string error;
    if (!loadTaskFile(taskPath, task, error)) {
        VerificationResult result;
        result.status = "error";
        result.diagnostics.push_back(error);
        return result;
    }
    std::ifstream stream(candidatePath, std::ios::binary);
    if (!stream) {
        VerificationResult result;
        result.taskId = task.id;
        result.status = "error";
        result.diagnostics.push_back("cannot read candidate '" + candidatePath + "'");
        return result;
    }
    const std::string source{std::istreambuf_iterator<char>(stream),
                             std::istreambuf_iterator<char>()};
    return verifyTaskSource(task, source, candidatePath, options);
}

VerificationResult verifyFlatTaskFile(const std::string& taskPath,
                                      const std::string& flatPath,
                                      const ObservationOptions& options) {
    Task task;
    std::string error;
    if (!loadTaskFile(taskPath, task, error)) {
        VerificationResult result;
        result.status = "error";
        result.diagnostics.push_back(error);
        return result;
    }
    VerificationResult result;
    result.taskId = task.id;
    const CompiledProgram baseline = compileFile(task.baselinePath);
    if (!baseline.ok) {
        result.status = "error";
        result.validityStatus = "failed";
        result.diagnostics.push_back("baseline compile failed: " + baseline.diagnostics);
        return result;
    }
    if (task.frame.specified) {
        result.status = "not-applicable";
        result.frameStatus = "not-applicable";
        result.validityStatus = "ok";
        result.diagnostics.push_back(
            "frame contract requires a compiled production; flat events do not provide one");
        return result;
    }
    std::ifstream stream(flatPath, std::ios::binary);
    if (!stream) {
        result.status = "error";
        result.diagnostics.push_back("cannot read flat event document '" + flatPath + "'");
        return result;
    }
    const std::string flatSource{std::istreambuf_iterator<char>(stream),
                                 std::istreambuf_iterator<char>()};
    const FlatParseResult flat = parseFlatEvents(flatSource);
    if (!flat.ok) {
        result.status = "error";
        result.diagnostics.push_back(flat.error);
        return result;
    }
    bool unsupported = false;
    for (const ObservationExpectation& expectation : task.target) {
        if (!flatSupportsObservation(expectation.name)) {
            unsupported = true;
            ObservationComparison comparison;
            comparison.name = expectation.name;
            comparison.status = "not-applicable";
            comparison.reason = flatUnsupportedReason(expectation.name);
            result.comparisons.push_back(std::move(comparison));
        }
    }
    for (const ObservationExpectation& expectation : task.preserve) {
        if (!flatSupportsObservation(expectation.name)) {
            unsupported = true;
            ObservationComparison comparison;
            comparison.name = "preserve:" + expectation.name;
            comparison.status = "not-applicable";
            comparison.reason = flatUnsupportedReason(expectation.name);
            result.comparisons.push_back(std::move(comparison));
        }
    }
    if (unsupported) {
        result.status = "not-applicable";
        return result;
    }
    const ObservationSet baselineObservations =
        observeCompilation(baseline.compilation, options);
    const ObservationSet candidateObservations = observeFlatEvents(flat.document, options);
    return verifyObservationSets(task, baselineObservations, candidateObservations);
}

VerificationResult verifyTaskWithRenderManifest(
    const std::string& taskPath, const std::string& candidatePath,
    const std::string& manifestPath, const std::string& audioPath) {
    VerificationResult result;
    std::ifstream manifestStream(manifestPath, std::ios::binary);
    if (!manifestStream) {
        result.status = "error";
        result.diagnostics.push_back("cannot read render manifest");
        return result;
    }
    const std::string manifestText{std::istreambuf_iterator<char>(manifestStream),
                                   std::istreambuf_iterator<char>()};
    RenderArtifactManifest manifest;
    std::string error;
    if (!parseRenderManifest(manifestText, manifest, error)) {
        result.status = "error";
        result.diagnostics.push_back(error);
        return result;
    }
    const CompiledProgram candidate = compileFile(candidatePath);
    if (!candidate.ok) {
        result.status = "error";
        result.validityStatus = "failed";
        result.diagnostics.push_back("candidate compile failed: " + candidate.diagnostics);
        return result;
    }
    if (audioPath.empty()) {
        result.status = "error";
        result.diagnostics.push_back("trusted render manifest requires an audio path");
        return result;
    }
    const std::string source = readFile(candidatePath);
    if (!verifyRenderManifest(manifest, source, candidate.compilation,
                              manifest.configuration, audioPath, error)) {
        result.status = "error";
        result.diagnostics.push_back("render manifest rejected: " + error);
        return result;
    }
    ObservationOptions options;
    options.audioPath = audioPath;
    options.expectedAudioHash = manifest.audioHash;
    return verifyTaskFile(taskPath, candidatePath, options);
}

std::string VerificationResult::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.task-verification");
    json.key("version").value(1);
    json.key("taskId").value(taskId);
    json.key("status").value(status);
    json.key("validityStatus").value(validityStatus);
    json.key("targetStatus").value(targetStatus);
    json.key("preserveStatus").value(preserveStatus);
    json.key("frameStatus").value(frameStatus);
    json.key("comparisons").beginArray();
    for (const ObservationComparison& comparison : comparisons) {
        json.beginObject();
        json.key("name").value(comparison.name);
        json.key("status").value(comparison.status);
        json.key("expected").value(comparison.expected);
        json.key("actual").value(comparison.actual);
        json.key("reason").value(comparison.reason);
        json.endObject();
    }
    json.endArray();
    json.key("diagnostics").beginArray();
    for (const std::string& diagnostic : diagnostics) {
        json.value(diagnostic);
    }
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
