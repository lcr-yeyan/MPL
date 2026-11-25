#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mpl {

/// A model-agnostic request. The harness, not the model, owns compilation,
/// rendering and verification.
struct ModelRequest {
    std::string taskId;
    std::string prompt;
    std::string languageContext;
    std::string standardLibraryContext;
    std::string previousCandidate;
    std::string semanticFeedback;
    int attempt = 0;
    int maxAttempts = 1;
    int maxOutputTokens = 0;
    std::string promptVersion;
    std::string sampling;
};

struct ModelResponse {
    bool ok = false;
    std::string candidateSource;
    std::string model;
    std::string modelVersion;
    std::string sampling;
    std::string promptVersion;
    int inputTokens = 0;
    int outputTokens = 0;
    int cacheTokens = 0;
    int modelCalls = 0;
    double latencyMilliseconds = 0.0;
    std::string error;
};

/// Implementations are external adapters. No network/API code belongs in MPL
/// Core or Runtime; the harness may load an adapter and record its responses.
class ModelAdapter {
public:
    virtual ~ModelAdapter() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const { return "unknown"; }
    /// Returns a response even on provider errors; `ok=false` and `error` must
    /// be set rather than throwing across the harness boundary.
    virtual ModelResponse generate(const ModelRequest& request) = 0;
};

}  // namespace mpl
