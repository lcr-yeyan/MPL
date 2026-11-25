#pragma once

#include "mpl/Compiler.hpp"

#include <string>
#include <vector>

namespace mpl {

enum class DeltaIdentity {
    Value,       /// semantic value identity (name/key/notes)
    Generation,  /// source object / provenance identity
    Instance,    /// clip performance instance identity
};

enum class DeltaCause {
    Direct,
    Dependency,
    Unknown,
};

struct SemanticChange {
    std::string change;    // added / removed / modified
    std::string objectType;  // track / clip / section / harmony / production
    std::string key;
    std::string property;
    std::string before;
    std::string after;
    DeltaIdentity identity = DeltaIdentity::Value;
    DeltaCause cause = DeltaCause::Unknown;
    std::string source;  // module / declaration label when known
};

struct ChangedObservation {
    std::string name;
    std::string status;  // changed / unverifiable / only-in-one
    std::string before;
    std::string after;
    std::string reason;
};

struct SemanticDelta {
    std::vector<SemanticChange> changes;
    std::vector<ChangedObservation> changedObservations;
    std::vector<std::string> unobserved;
    std::string causeSummary;  // direct / dependency / unknown / mixed
    std::string toJson() const;
};

/// Compares two compilations by semantic identity, not by ObjectId. ObjectId
/// changes alone are reported as generation/instance identity, not as musical
/// content changes. Attributes not covered by this comparison are listed in
/// `unobserved`; the delta never claims they are unchanged.
SemanticDelta computeSemanticDelta(const Compilation& before, const Compilation& after);

}  // namespace mpl
