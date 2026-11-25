#include "mpl/knowledge/KnowledgeVerifier.hpp"

#include "mpl/Compiler.hpp"
#include "mpl/frontend/Frontend.hpp"
#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace mpl {
namespace {

std::string readText(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

KnowledgeCheckStatus combine(KnowledgeCheckStatus current,
                             KnowledgeCheckStatus next) {
    if (current == KnowledgeCheckStatus::Fail || next == KnowledgeCheckStatus::Fail) {
        return KnowledgeCheckStatus::Fail;
    }
    if (current == KnowledgeCheckStatus::Error || next == KnowledgeCheckStatus::Error) {
        return KnowledgeCheckStatus::Error;
    }
    if (current == KnowledgeCheckStatus::Unsupported ||
        next == KnowledgeCheckStatus::Unsupported) {
        return KnowledgeCheckStatus::Unsupported;
    }
    if (current == KnowledgeCheckStatus::Unverified ||
        next == KnowledgeCheckStatus::Unverified) {
        return KnowledgeCheckStatus::Unverified;
    }
    return KnowledgeCheckStatus::Pass;
}

}  // namespace

std::string_view knowledgeCheckStatusName(KnowledgeCheckStatus status) {
    switch (status) {
        case KnowledgeCheckStatus::Pass: return "pass";
        case KnowledgeCheckStatus::Fail: return "fail";
        case KnowledgeCheckStatus::Unverified: return "unverified";
        case KnowledgeCheckStatus::Unsupported: return "unsupported";
        case KnowledgeCheckStatus::Error: return "error";
    }
    return "error";
}

KnowledgeVerification verifyKnowledge(const KnowledgeUnit& unit,
                                      const KnowledgeRegistry& registry) {
    KnowledgeVerification result;
    result.knowledgeId = unit.knowledgeId;
    result.version = unit.version;
    if (unit.verificationStatus == "unsupported" || unit.verificationStatus == "deferred") {
        result.status = KnowledgeCheckStatus::Unsupported;
        return result;
    }
    for (const std::string& dependency : unit.dependencies) {
        if (registry.resolve(dependency) == nullptr) {
            result.status = KnowledgeCheckStatus::Error;
            result.exampleResults.push_back("missing dependency: " + dependency);
            return result;
        }
    }

    bool anyCheck = false;
    bool hadUnverified = false;
    for (const KnowledgeExample& example : unit.examples) {
        const std::string source = readText(example.program);
        const FrontendResult frontend = compileSource(source, example.program);
        if (!frontend.ok()) {
            result.status = KnowledgeCheckStatus::Fail;
            result.exampleResults.push_back(example.program + ": does not compile");
        } else {
            result.exampleResults.push_back(example.program + ": verified");
        }
    }
    for (const KnowledgeCheck& check : unit.checks) {
        anyCheck = true;
        KnowledgeCheckResult checkResult;
        checkResult.phase = check.phase;
        checkResult.observation = check.observation;
        checkResult.expected = check.expected.canonical();
        const std::string source = readText(check.program);
        const FrontendResult frontend = compileSource(source, check.program);
        if (!frontend.ok()) {
            checkResult.status = KnowledgeCheckStatus::Error;
            checkResult.reason = "check program does not compile: " + check.program;
            result.checks.push_back(std::move(checkResult));
            result.status = combine(result.status, KnowledgeCheckStatus::Error);
            continue;
        }
        const Compiler compiler;
        const Compilation compilation =
            compiler.compile(std::move(frontend.program->project));
        if (compilation.hasErrors()) {
            checkResult.status = KnowledgeCheckStatus::Error;
            checkResult.reason = "check program compilation failed";
            result.checks.push_back(std::move(checkResult));
            result.status = combine(result.status, KnowledgeCheckStatus::Error);
            continue;
        }
        const ObservationSet observations = observeCompilation(compilation);
        const ObservationValue* actual = observations.find(check.observation);
        if (actual == nullptr) {
            checkResult.status = KnowledgeCheckStatus::Unsupported;
            checkResult.reason = "unknown observation";
        } else {
            checkResult.actual = actual->jsonValue;
            if (actual->status == "unverifiable") {
                checkResult.status = KnowledgeCheckStatus::Unverified;
                checkResult.reason = actual->reason;
            } else if (check.comparator == "gte" || check.comparator == "lte") {
                JsonValue expectedValue;
                JsonValue actualValue;
                std::string error;
                if (!parseJson(check.expected.canonical(), expectedValue, error) ||
                    !parseJson(actual->jsonValue, actualValue, error) ||
                    !expectedValue.isNumber() || !actualValue.isNumber()) {
                    checkResult.status = KnowledgeCheckStatus::Fail;
                    checkResult.reason = "numeric comparison requires numbers";
                } else {
                    const bool ok = check.comparator == "gte"
                                        ? actualValue.number >= expectedValue.number
                                        : actualValue.number <= expectedValue.number;
                    checkResult.status = ok ? KnowledgeCheckStatus::Pass
                                            : KnowledgeCheckStatus::Fail;
                }
            } else if (check.comparator == "tolerance") {
                JsonValue expectedValue;
                JsonValue actualValue;
                std::string error;
                if (!parseJson(check.expected.canonical(), expectedValue, error) ||
                    !parseJson(actual->jsonValue, actualValue, error) ||
                    !expectedValue.isNumber() || !actualValue.isNumber()) {
                    checkResult.status = KnowledgeCheckStatus::Fail;
                    checkResult.reason = "tolerance comparison requires numbers";
                } else {
                    const bool ok = std::abs(actualValue.number - expectedValue.number) <=
                                    check.tolerance;
                    checkResult.status = ok ? KnowledgeCheckStatus::Pass
                                            : KnowledgeCheckStatus::Fail;
                }
            } else {
                JsonValue expectedValue;
                JsonValue actualValue;
                std::string error;
                if (!parseJson(check.expected.canonical(), expectedValue, error) ||
                    !parseJson(actual->jsonValue, actualValue, error)) {
                    checkResult.status = KnowledgeCheckStatus::Fail;
                    checkResult.reason = "malformed expected/actual JSON";
                } else {
                    checkResult.status = expectedValue.canonical() == actualValue.canonical()
                                             ? KnowledgeCheckStatus::Pass
                                             : KnowledgeCheckStatus::Fail;
                }
            }
        }
        result.checks.push_back(std::move(checkResult));
        if (result.checks.back().status == KnowledgeCheckStatus::Unverified) {
            hadUnverified = true;
        }
        result.status = combine(result.status,
                                result.checks.back().status);
    }
    if (result.status == KnowledgeCheckStatus::Fail) {
        return result;
    }
    if (result.status == KnowledgeCheckStatus::Error) {
        return result;
    }
    if (result.status == KnowledgeCheckStatus::Unsupported) {
        return result;
    }
    if (hadUnverified) {
        result.status = KnowledgeCheckStatus::Unverified;
        return result;
    }
    if (!anyCheck) {
        result.status = KnowledgeCheckStatus::Unverified;
        return result;
    }
    if (unit.verificationStatus != "verified") {
        result.status = KnowledgeCheckStatus::Unverified;
        return result;
    }
    result.status = KnowledgeCheckStatus::Pass;
    return result;
}

std::string KnowledgeVerification::toJson() const {
    JsonWriter json;
    json.beginObject();
    json.key("format").value("mpl.knowledge-verification");
    json.key("version").value(1);
    json.key("knowledgeId").value(knowledgeId);
    json.key("versionId").value(version);
    json.key("status").value(std::string(knowledgeCheckStatusName(status)));
    json.key("checks").beginArray();
    for (const KnowledgeCheckResult& check : checks) {
        json.beginObject();
        json.key("phase").value(check.phase);
        json.key("observation").value(check.observation);
        json.key("status").value(std::string(knowledgeCheckStatusName(check.status)));
        json.key("expected").value(check.expected);
        json.key("actual").value(check.actual);
        json.key("reason").value(check.reason);
        json.endObject();
    }
    json.endArray();
    json.key("examples").beginArray();
    for (const std::string& example : exampleResults) json.value(example);
    json.endArray();
    json.endObject();
    return json.take();
}

}  // namespace mpl
