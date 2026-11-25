#include "TestSupport.hpp"

#include "mpl/knowledge/Knowledge.hpp"
#include "mpl/knowledge/KnowledgeResolver.hpp"
#include "mpl/knowledge/KnowledgeVerifier.hpp"
#include "mpl/knowledge/AdaptiveContext.hpp"
#include "mpl/knowledge/TaskRequirements.hpp"

#include <filesystem>
#include <string>

using namespace mpl;

namespace {
std::string unitJson(const std::string& id, const std::string& dependency = "",
                     const std::string& description = "test") {
    std::string text = R"({"knowledgeId":")" + id + R"(","version":"1","category":"Rhythm","kind":"executable","description":")" + description + R"(","implementationRef":"core:rhythm.hit-rest","verificationStatus":"unverified")";
    if (!dependency.empty()) {
        text += R"(,"dependencies":[")" + dependency + R"("])";
    }
    text += "}";
    return text;
}
std::string catalogJson(const std::string& units) {
    return R"({"format":"mpl.knowledge-catalog","version":1,"units":[)" + units + "]}";
}
}  // namespace

MPL_TEST(KnowledgeCatalogLoadsAndIndexes) {
    KnowledgeRegistry registry;
    std::string error;
    MPL_CHECK(registry.loadCatalog(std::string(MPL_KNOWLEDGE_DIR) + "/v1/catalog.json", error));
    MPL_CHECK_EQ(registry.units().size(), std::size_t{33});
    const KnowledgeUnit* arp = registry.resolve("harmony.arp");
    MPL_CHECK(arp != nullptr);
    if (arp != nullptr) {
        MPL_CHECK_EQ(arp->version, std::string("1"));
        MPL_CHECK(!arp->resolvedHash.empty());
        MPL_CHECK(!arp->contentHash.empty());
    }
    MPL_CHECK(!registry.byCategory("Rhythm").empty());
    MPL_CHECK(!registry.byObservation("pitch_sequence").empty());
    MPL_CHECK(!registry.byType("Voicing").empty());
    std::vector<const KnowledgeUnit*> closure;
    error.clear();
    MPL_CHECK(registry.dependencyClosure("harmony.arp", closure, error));
    MPL_CHECK(!closure.empty());
    MPL_CHECK_EQ(closure.back()->knowledgeId, std::string("harmony.arp"));
    const std::string first = registry.toJson();
    const std::string second = registry.toJson();
    MPL_CHECK_EQ(first, second);
}

MPL_TEST(KnowledgeRegistryRejectsDuplicateAndBadVersion) {
    KnowledgeRegistry registry;
    std::string error;
    const std::string text = catalogJson(unitJson("a") + "," + unitJson("a"));
    MPL_CHECK(!registry.loadCatalogText(text, error));
    MPL_CHECK(error.find("duplicate") != std::string::npos);
    KnowledgeRegistry versions;
    MPL_CHECK(versions.loadCatalogText(catalogJson(unitJson("a")), error));
    MPL_CHECK(versions.resolve("a", "2") == nullptr);
}

MPL_TEST(KnowledgeRegistryDetectsMissingAndCycle) {
    KnowledgeRegistry missing;
    std::string error;
    MPL_CHECK(!missing.loadCatalogText(catalogJson(unitJson("a", "missing")), error));
    MPL_CHECK(error.find("missing") != std::string::npos);
    KnowledgeRegistry cycle;
    MPL_CHECK(!cycle.loadCatalogText(
        catalogJson(unitJson("a", "b") + "," + unitJson("b", "a")), error));
    MPL_CHECK(error.find("cyclic") != std::string::npos);
}

MPL_TEST(KnowledgeContractVerifierDetectsPassAndFault) {
    KnowledgeRegistry registry;
    std::string error;
    MPL_CHECK(registry.loadCatalog(std::string(MPL_KNOWLEDGE_DIR) + "/v1/catalog.json", error));
    const KnowledgeUnit* unit = registry.resolve("rhythm.hit-rest");
    MPL_CHECK(unit != nullptr);
    if (unit != nullptr) {
        MPL_CHECK(verifyKnowledge(*unit, registry).status == KnowledgeCheckStatus::Pass);
        KnowledgeUnit fault = *unit;
        fault.checks.clear();
        KnowledgeCheck broken;
        broken.phase = "postcondition";
        broken.program = "bench/baselines/basic.mpl";
        broken.observation = "event_count";
        broken.comparator = "equals";
        JsonValue expected;
        expected.type = JsonValue::Type::Number;
        expected.number = 99;
        broken.expected = expected;
        fault.checks.push_back(broken);
        MPL_CHECK(verifyKnowledge(fault, registry).status == KnowledgeCheckStatus::Fail);
        KnowledgeUnit unsupported = *unit;
        unsupported.checks.clear();
        KnowledgeCheck unknown;
        unknown.program = "bench/baselines/basic.mpl";
        unknown.observation = "not_an_observation";
        unknown.comparator = "equals";
        unsupported.checks.push_back(unknown);
        MPL_CHECK(verifyKnowledge(unsupported, registry).status ==
                  KnowledgeCheckStatus::Unsupported);
    }
    const KnowledgeUnit* practice = registry.resolve("practice.voice-leading-preference");
    MPL_CHECK(practice != nullptr);
    if (practice != nullptr) {
        MPL_CHECK(verifyKnowledge(*practice, registry).status ==
                  KnowledgeCheckStatus::Unverified);
    }
}

#if defined(MPL_EXPERIMENTAL_AI)
MPL_TEST(KnowledgeResolutionAndAdaptiveContext) {
    KnowledgeRegistry registry;
    std::string error;
    MPL_CHECK(registry.loadCatalog(std::string(MPL_KNOWLEDGE_DIR) + "/v1/catalog.json", error));
    const std::string taskPath =
        std::string(MPL_BENCH_DIR) + "/tasks/01-basic-notes.json";
    TaskParseResult parsed = parseTaskJson(mpltest::readText(taskPath));
    MPL_CHECK(parsed.ok);
    if (!parsed.ok) {
        return;
    }
    parsed.task.baselinePath =
        std::string(MPL_BENCH_DIR) + "/baselines/basic.mpl";
    TaskRequirementSet requirements = analyzeTaskRequirements(parsed.task);
    MPL_CHECK(requirements.valid);
    KnowledgeResolution resolution = resolveKnowledge(requirements, registry, "Rhythm");
    MPL_CHECK(!resolution.matches.empty());
    bool sawApplicable = false;
    for (const KnowledgeMatch& match : resolution.matches) {
        sawApplicable = sawApplicable ||
                        match.applicability == KnowledgeApplicability::Applicable;
    }
    MPL_CHECK(sawApplicable);

    ContextBuildOptions adaptiveOptions;
    adaptiveOptions.mode = ContextMode::Adaptive;
    adaptiveOptions.foundationPath = "lcp/v1.1/contexts/Core.md";
    adaptiveOptions.fullPath = "lcp/v1.1/contexts/C.md";
    const ContextBuildReport adaptive =
        buildTaskContext(parsed.task, resolution, registry, adaptiveOptions);
    MPL_CHECK(!adaptive.content.empty());
    MPL_CHECK(!adaptive.selectedKnowledge.empty());
    MPL_CHECK(!adaptive.contextHash.empty());
    MPL_CHECK(adaptive.budgetStatus == std::string("unlimited"));
    MPL_CHECK(adaptive.duplicateLines > 0);

    ContextBuildOptions fullOptions = adaptiveOptions;
    fullOptions.mode = ContextMode::Full;
    const ContextBuildReport full =
        buildTaskContext(parsed.task, resolution, registry, fullOptions);
    MPL_CHECK(!full.content.empty());
    MPL_CHECK(!full.contextHash.empty());

    ContextBuildOptions tiny = adaptiveOptions;
    tiny.tokenBudget = 10;
    const ContextBuildReport overflow =
        buildTaskContext(parsed.task, resolution, registry, tiny);
    MPL_CHECK(overflow.budgetStatus == std::string("insufficient-budget"));

    const ContextBuildReport again =
        buildTaskContext(parsed.task, resolution, registry, adaptiveOptions);
    MPL_CHECK_EQ(adaptive.contextHash, again.contextHash);
}
#endif

MPL_TEST(TaskRequirementsReportUnknownObservationsAndFrameGaps) {
    Task task;
    task.id = "requirements-edge";
    ObservationExpectation unknown;
    unknown.name = "not_an_observation";
    task.target.push_back(unknown);
    FrameRule rule;
    rule.objectType = "metadata";
    rule.property = "checksum";
    task.frame.specified = true;
    task.frame.allow.push_back(rule);
    const TaskRequirementSet requirements = analyzeTaskRequirements(task);
    MPL_CHECK(!requirements.valid);
    MPL_CHECK(!requirements.unknown.empty());
    MPL_CHECK(!requirements.unsupported.empty());
}

#if defined(MPL_EXPERIMENTAL_AI)
MPL_TEST(KnowledgeResolutionIsDeterministicAndFiltersEffects) {
    KnowledgeRegistry registry;
    std::string error;
    MPL_CHECK(registry.loadCatalog(std::string(MPL_KNOWLEDGE_DIR) + "/v1/catalog.json", error));
    TaskRequirementSet requirements;
    requirements.taskId = "effect-test";
    requirements.targetObservations = {"event_count"};
    requirements.allowedEffects = {"production.gain"};
    const KnowledgeResolution first = resolveKnowledge(requirements, registry);
    const KnowledgeResolution second = resolveKnowledge(requirements, registry);
    MPL_CHECK_EQ(first.toJson(), second.toJson());
    bool filtered = false;
    for (const KnowledgeMatch& match : first.matches) {
        if (match.knowledgeId == "rhythm.hit-rest") {
            filtered = match.applicability == KnowledgeApplicability::Inapplicable;
        }
    }
    MPL_CHECK(filtered);
}
#endif

#if defined(MPL_EXPERIMENTAL_AI)
MPL_TEST(AdaptiveContextReportsMissingFoundationAndLeakage) {
    KnowledgeRegistry registry;
    std::string error;
    const std::string leakUnit = R"({
      "knowledgeId":"test.leak","version":"1","category":"Rhythm","kind":"executable",
      "description":"LEAK-MARKER-12345","implementationRef":"core:test",
      "observationRefs":["event_count"],"verificationStatus":"unverified"
    })";
    MPL_CHECK(registry.loadCatalogText(catalogJson(leakUnit), error));
    Task task;
    task.id = "context-edge";
    task.category = "Rhythm";
    ObservationExpectation target;
    target.name = "event_count";
    target.expected.type = JsonValue::Type::String;
    target.expected.text = "LEAK-MARKER-12345";
    target.hasExpected = true;
    task.target.push_back(target);
    TaskRequirementSet requirements = analyzeTaskRequirements(task);
    KnowledgeResolution resolution = resolveKnowledge(requirements, registry);
    ContextBuildOptions missing;
    missing.foundationPath = "does/not/exist.md";
    const ContextBuildReport missingReport =
        buildTaskContext(task, resolution, registry, missing);
    MPL_CHECK(missingReport.budgetStatus == std::string("insufficient-budget"));
    ContextBuildOptions heldout;
    heldout.heldout = true;
    heldout.foundationPath = "lcp/v1.1/contexts/L1.md";
    const ContextBuildReport leak = buildTaskContext(task, resolution, registry, heldout);
    MPL_CHECK(leak.heldoutLeakageBlocked);
    MPL_CHECK(leak.budgetStatus == std::string("leakage-blocked"));
}
#endif

MPL_TEST(KnowledgeResolvedHashChangesWithDependency) {
    KnowledgeRegistry first;
    std::string error;
    MPL_CHECK(first.loadCatalogText(
        catalogJson(unitJson("child", "", "first") + "," + unitJson("parent", "child")),
        error));
    const KnowledgeUnit* parent = first.resolve("parent");
    MPL_CHECK(parent != nullptr);
    const std::string firstParentHash = parent == nullptr ? "" : parent->resolvedHash;
    KnowledgeRegistry second;
    MPL_CHECK(second.loadCatalogText(
        catalogJson(unitJson("child", "", "second") + "," + unitJson("parent", "child")),
        error));
    const KnowledgeUnit* secondParent = second.resolve("parent");
    MPL_CHECK(secondParent != nullptr);
    MPL_CHECK(firstParentHash != (secondParent == nullptr ? "" : secondParent->resolvedHash));
}
