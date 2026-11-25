#pragma once

#include "mpl/knowledge/Knowledge.hpp"

#include <string>
#include <vector>

namespace mpl {

enum class KnowledgeCheckStatus { Pass, Fail, Unverified, Unsupported, Error };
std::string_view knowledgeCheckStatusName(KnowledgeCheckStatus status);

struct KnowledgeCheckResult {
    std::string phase;
    std::string observation;
    KnowledgeCheckStatus status = KnowledgeCheckStatus::Unverified;
    std::string expected;
    std::string actual;
    std::string reason;
};

struct KnowledgeVerification {
    std::string knowledgeId;
    std::string version;
    KnowledgeCheckStatus status = KnowledgeCheckStatus::Unverified;
    std::vector<KnowledgeCheckResult> checks;
    std::vector<std::string> exampleResults;
    std::string toJson() const;
};

/// Reuses the frozen compiler and Observation Registry. It is a contract
/// verifier, not a general mathematical proof.
KnowledgeVerification verifyKnowledge(const KnowledgeUnit& unit,
                                      const KnowledgeRegistry& registry);

}  // namespace mpl
