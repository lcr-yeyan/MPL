#pragma once

#include "mpl/util/JsonValue.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

struct KnowledgeExample {
    std::string role;  // legal / boundary / negative
    std::string program;
    std::string description;
};

struct KnowledgeCheck {
    std::string phase;  // precondition / postcondition / invariant / effect
    std::string program;
    std::string observation;
    std::string comparator;  // equals / gte / lte / tolerance
    JsonValue expected;
    double tolerance = 0.0;
    std::string note;
};

struct KnowledgeUnit {
    std::string knowledgeId;
    std::string version = "1";
    std::string category;
    std::string kind;  // executable / relation / verification / construction / practice
    /// Carrier of the implementation: callable / syntax / stdlib /
    /// observation / task / benchmark / deferred. Separate from `kind`.
    std::string referenceKind;
    std::string description;
    std::vector<std::string> inputTypes;
    std::vector<std::string> outputTypes;
    std::vector<std::string> parameters;
    std::vector<std::string> preconditions;
    std::vector<std::string> postconditions;
    std::vector<std::string> invariants;
    std::vector<std::string> allowedEffects;
    std::vector<std::string> requiredContext;
    std::vector<std::string> dependencies;
    std::string implementationRef;
    std::vector<std::string> observationRefs;
    std::vector<KnowledgeExample> examples;
    std::vector<KnowledgeCheck> checks;
    std::string source;
    std::string license;
    std::string verificationStatus = "unverified";
    std::string unsupportedReason;
    std::vector<std::string> tags;
    std::string contentHash;
    std::string resolvedHash;
};

struct KnowledgeLoadError {
    std::string code;
    std::string message;
    std::string knowledgeId;
};

class KnowledgeRegistry {
public:
    bool loadCatalog(const std::string& path, std::string& error);
    bool loadCatalogText(std::string_view text, std::string& error);
    bool registerUnit(KnowledgeUnit unit, std::string& error);

    const KnowledgeUnit* resolve(const std::string& id, const std::string& version = "") const;
    const std::vector<KnowledgeUnit>& units() const noexcept { return units_; }

    std::vector<const KnowledgeUnit*> byCategory(const std::string& category) const;
    std::vector<const KnowledgeUnit*> byType(const std::string& type) const;
    std::vector<const KnowledgeUnit*> byObservation(const std::string& observation) const;
    std::vector<const KnowledgeUnit*> byTag(const std::string& tag) const;

    /// Deterministic topological closure (dependencies first). On missing or
    /// cyclic dependencies returns false and fills `error`.
    bool dependencyClosure(const std::string& id, std::vector<const KnowledgeUnit*>& out,
                           std::string& error) const;
    std::vector<std::string> missingDependencies(const std::string& id) const;

    bool recomputeHashes(std::string& error);
    std::string toJson() const;
    const std::vector<KnowledgeLoadError>& errors() const noexcept { return errors_; }

private:
    std::vector<KnowledgeUnit> units_;
    std::map<std::string, std::size_t> byKey_;
    std::map<std::string, std::vector<std::size_t>> categoryIndex_;
    std::map<std::string, std::vector<std::size_t>> typeIndex_;
    std::map<std::string, std::vector<std::size_t>> observationIndex_;
    std::map<std::string, std::vector<std::size_t>> tagIndex_;
    std::vector<KnowledgeLoadError> errors_;
};

std::string knowledgeCanonicalJson(const KnowledgeUnit& unit, bool includeHashes = false);
std::string knowledgeKindName(const std::string& kind);
bool knowledgeKindIsHardConstraint(const std::string& kind);

}  // namespace mpl
