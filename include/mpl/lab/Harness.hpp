#pragma once

#include "mpl/lab/Task.hpp"

#include <string>
#include <vector>

namespace mpl {

struct ExperimentSubmission {
    std::string arm;  // A0/A1/B0/B1/C0/C1, or any external label
    std::string candidatePath;
    std::string initialCandidatePath;
    std::string audioPath;
    std::string renderManifestPath;
    int inputTokens = 0;
    int outputTokens = 0;
    int cacheTokens = 0;
    int modelCalls = 0;
    int repairs = 0;
    double latencyMilliseconds = 0.0;
    std::string model;
    std::string modelVersion;
    std::string sampling;
    std::string promptVersion;
    std::string error;
    bool feedback = false;
};

struct ExperimentRun {
    std::string arm;
    std::string candidatePath;
    std::string audioPath;
    std::string renderManifestPath;
    std::string status;  // pass / fail / unverifiable / not-applicable / error / unsupported / invalid-arm / not-run
    std::string initialStatus;
    std::string reason;
    long long elapsedMilliseconds = 0;
    std::size_t candidateBytes = 0;
    int inputTokens = 0;
    int outputTokens = 0;
    int cacheTokens = 0;
    int modelCalls = 0;
    int repairs = 0;
    double latencyMilliseconds = 0.0;
    std::string model;
    std::string modelVersion;
    std::string sampling;
    std::string promptVersion;
    std::string error;
    bool feedback = false;
    VerificationResult verification;
};

struct ExperimentResult {
    std::string experimentId;
    std::string taskId;
    bool modelGenerated = false;
    std::vector<ExperimentRun> runs;
    std::string toJson() const;
};

/// Runs the model-agnostic harness over already-produced candidate files.
/// This function never calls a model; token/call counts come from the imported
/// submission record so offline outputs and live runs share the same schema.
ExperimentResult runExperiment(const std::string& experimentPath);

/// Verifies one candidate for a specific experimental arm. A arms use the flat
/// verifier; B/C arms use the MPL verifier and enforce the B-arm stdlib ban.
VerificationResult verifyArmCandidate(const std::string& arm,
                                      const std::string& taskPath,
                                      const std::string& candidatePath,
                                      const std::string& audioPath = {},
                                      const std::string& renderManifestPath = {});

/// Schema documentation for external generators.
std::string experimentSchemaJson();

}  // namespace mpl
