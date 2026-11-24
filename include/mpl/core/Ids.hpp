#pragma once

#include <cstdint>

namespace mpl {

/// Identity of an object inside the Music IR. Object ids are dense indices
/// into the graph, which keeps serialization and debugging trivial.
using ObjectId = std::uint32_t;

inline constexpr ObjectId kInvalidObjectId = 0xFFFFFFFFu;

}  // namespace mpl
