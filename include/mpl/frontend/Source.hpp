#pragma once

#include <cstddef>
#include <string>

namespace mpl {

/// Half-open source range with a human-readable start position. Lines and
/// columns are 1-based; offsets are byte offsets into the source file.
struct SourceSpan {
    std::string file;
    std::size_t begin = 0;
    std::size_t end = 0;
    int line = 1;
    int column = 1;
    int endLine = 1;
    int endColumn = 1;

    bool valid() const noexcept { return !file.empty(); }
};

}  // namespace mpl
