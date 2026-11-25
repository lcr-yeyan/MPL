#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mpl {

/// Identity and dependency record for one source module. Content identity is a
/// deterministic hash of the module source; path and imports are provenance,
/// not musical semantics.
struct ModuleRecord {
    std::string name;
    std::string path;
    /// Hash of this module's own source.
    std::uint64_t contentHash = 0;
    /// Hash of this module's source plus all imported modules' resolved hashes.
    std::uint64_t resolvedHash = 0;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
};

}  // namespace mpl
