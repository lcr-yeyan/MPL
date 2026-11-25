#pragma once

#include "mpl/observe/Observations.hpp"
#include "mpl/util/JsonValue.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

struct ObservationExpectation {
    std::string name;
    JsonValue expected;
    bool hasExpected = false;
    bool fromBaseline = false;
    double tolerance = 0.0;
};

struct FrameRule {
    std::string objectType = "*";
    std::string key = "*";
    std::string property = "*";
    std::string cause = "*";  // "*" / "direct" / "dependency" / "unknown"
};

struct FrameContract {
    bool specified = false;
    std::vector<FrameRule> allow;
    std::vector<FrameRule> deny;
};

struct FrameCoverage {
    std::vector<std::string> objectTypes;
    std::vector<std::string> properties;  // "object.property"
    std::vector<std::string> unobserved;
};

FrameCoverage frameCoverage();

struct TaskBudget {
    int maxRepairs = 3;
    double maxSeconds = 300.0;
    int maxTokens = 0;
};

struct Task {
    std::string id;
    std::string title;
    std::string category;
    std::string baselinePath;
    std::string allowedScope = "program";
    std::string templateId;
    int seed = 0;
    double tolerance = 0.0;
    TaskBudget budget;
    FrameContract frame;
    std::vector<ObservationExpectation> target;
    std::vector<ObservationExpectation> preserve;
};

struct TaskParseResult {
    bool ok = false;
    Task task;
    std::string error;
};

TaskParseResult parseTaskJson(std::string_view text);

struct ObservationComparison {
    std::string name;
    std::string status;  // pass / fail / unverifiable
    std::string expected;
    std::string actual;
    std::string reason;
};

struct VerificationResult {
    std::string taskId;
    std::string status;  // pass / fail / unverifiable / error / not-applicable
    std::string validityStatus;  // ok / failed
    std::string targetStatus;
    std::string preserveStatus;
    std::string frameStatus;
    std::vector<std::string> modulesUsed;
    std::vector<ObservationComparison> comparisons;
    std::vector<std::string> diagnostics;
    std::string toJson() const;
};

/// Verifies a candidate source against a task. The task baseline is compiled
/// independently; no claim by a generator is trusted.
VerificationResult verifyTaskSource(const Task& task, std::string_view candidateSource,
                                    const std::string& candidateFile,
                                    const ObservationOptions& options = {});

/// Compares precomputed baseline/candidate observations under a task contract.
/// Used by the MPL and flat-event adapters so both share one verifier.
VerificationResult verifyObservationSets(const Task& task,
                                         const ObservationSet& baseline,
                                         const ObservationSet& candidate);

/// Verifies a flat-event JSON candidate against a task. Observations the flat
/// representation cannot express make the task `not-applicable`; they are
/// never silently converted to pass.
VerificationResult verifyFlatTaskFile(const std::string& taskPath,
                                      const std::string& flatPath,
                                      const ObservationOptions& options = {});

/// Verifies a candidate and gates its audio through a Render Artifact Manifest.
/// A mismatched source/module/production/audio hash returns `error`; the WAV is
/// never used as the candidate's render.
VerificationResult verifyTaskWithRenderManifest(
    const std::string& taskPath, const std::string& candidatePath,
    const std::string& manifestPath, const std::string& audioPath);

/// Loads task JSON, resolves the baseline relative to the task file and verifies
/// a candidate file.
VerificationResult verifyTaskFile(const std::string& taskPath,
                                  const std::string& candidatePath,
                                  const ObservationOptions& options = {});

}  // namespace mpl
