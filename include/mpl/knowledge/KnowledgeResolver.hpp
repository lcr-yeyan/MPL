#pragma once

#include "mpl/knowledge/Knowledge.hpp"
#include "mpl/knowledge/TaskRequirements.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

enum class KnowledgeApplicability {
    Applicable,
    ConditionallyApplicable,
    Inapplicable,
    Unknown,
};

std::string_view knowledgeApplicabilityName(KnowledgeApplicability value);

struct KnowledgeMatch {
    std::string knowledgeId;
    std::string version;
    KnowledgeApplicability applicability = KnowledgeApplicability::Unknown;
    int score = 0;
    std::vector<std::string> reasons;
    std::vector<std::string> missingDependencies;
};

struct KnowledgeResolution {
    std::string taskId;
    std::string scoringVersion = "1";
    bool requirementsValid = true;
    std::vector<std::string> unknown;
    std::vector<std::string> unsupported;
    std::vector<KnowledgeMatch> matches;
    std::vector<std::string> unresolved;
    std::string toJson() const;
};

/// Deterministic rule-based matching. A match is a context candidate, not proof
/// that the task is satisfied; generated candidates still require compilation
/// and contract verification.
KnowledgeResolution resolveKnowledge(const TaskRequirementSet& requirements,
                                     const KnowledgeRegistry& registry,
                                     const std::string& domain = {},
                                     std::size_t maxMatches = 32);

}  // namespace mpl
