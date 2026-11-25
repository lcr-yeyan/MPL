#pragma once

#include <string_view>

namespace mpl {

/// Version identity recorded in machine-readable compiler reports and traces.
/// The language and standard library versions are independent of the CLI build
/// version so external synthesis systems can pin semantics.
inline constexpr std::string_view kCompilerVersion = "0.5.0-beta";
inline constexpr std::string_view kLanguageVersion = "0.4.0";
inline constexpr std::string_view kStdlibVersion = "0.4.0";
inline constexpr std::string_view kBenchmarkVersion = "0.4.0";
inline constexpr std::string_view kObservationVersion = "1.0.0";
inline constexpr std::string_view kProtocolVersion = "0.4.0";

}  // namespace mpl
