#pragma once

#include "mpl/core/Ids.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mpl {

/// One evaluated user-function call. Records are compiler provenance, not
/// musical content; they are copied into Compilation and written to the trace.
struct FunctionCallRecord {
    std::string function;
    std::string callFile;
    int callLine = 0;
    int callColumn = 0;
    int instance = 0;
    int depth = 0;
    bool cacheHit = false;
    std::string arguments;
    std::string contextKey;
    std::string contextMeter;
    /// Meter map identity plus anchor, for example "section:2@0" or "base@0".
    std::string metricContext;
    std::vector<ObjectId> objects;
    std::vector<std::string> operators;
};

/// Serializable function signature summary for trace output.
struct FunctionDefinitionRecord {
    std::string name;
    std::string returnType;
    std::vector<std::pair<std::string, std::string>> parameters;
    bool contextKey = false;
    bool contextMeter = false;
    std::uint64_t version = 0;
};

}  // namespace mpl
