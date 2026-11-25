#pragma once

#include "mpl/knowledge/Knowledge.hpp"
#include "mpl/knowledge/KnowledgeResolver.hpp"
#include "mpl/lab/Task.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

enum class ContextMode { Full, Domain, Adaptive };
std::string_view contextModeName(ContextMode mode);

struct ContextBuildOptions {
    ContextMode mode = ContextMode::Adaptive;
    std::string domain;
    std::size_t tokenBudget = 0;  // 0 = unlimited
    std::size_t maxKnowledge = 8; // adaptive selected knowledge cap before dependencies
    bool includeStdlib = false;
    bool heldout = false;
    bool allowFullFallback = true;
    std::string foundationPath;  // default lcp/v1.1/contexts/Core.md
    std::string fullPath;        // default lcp/v1.1/contexts/C.md or Core.md
};

struct ContextSource {
    std::string id;
    std::string kind;  // foundation / knowledge / task / example
    bool mandatory = false;
    std::size_t bytes = 0;
    std::string sha256;
};

struct ContextBuildReport {
    std::string taskId;
    ContextMode mode = ContextMode::Adaptive;
    std::string domain;
    std::string content;
    std::string contextHash;
    std::size_t bytes = 0;
    std::size_t estimatedTokens = 0;
    std::string budgetStatus;  // ok / insufficient-budget / unlimited
    bool fallbackUsed = false;
    bool overflow = false;
    bool heldoutLeakageBlocked = false;
    std::size_t duplicateLines = 0;
    long long elapsedMilliseconds = 0;
    std::vector<ContextSource> sources;
    std::vector<std::string> selectedKnowledge;
    std::vector<std::string> excludedKnowledge;
    std::vector<std::string> dependencies;
    std::vector<std::string> diagnostics;
    std::string toJson() const;
};

/// Builds a deterministic context from frozen LCP content, selected knowledge
/// and the task contract. It never pads with held-out expected values.
ContextBuildReport buildTaskContext(const Task& task,
                                    const KnowledgeResolution& resolution,
                                    const KnowledgeRegistry& registry,
                                    const ContextBuildOptions& options = {});

}  // namespace mpl
