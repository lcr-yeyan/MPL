#pragma once

#include <string>
#include <utility>

namespace mpl {

enum class Severity { Warning, Error };

/// A structured compiler message produced by semantic analysis or a pass.
struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;
    std::string message;

    static Diagnostic error(std::string code, std::string message) {
        return Diagnostic{Severity::Error, std::move(code), std::move(message)};
    }

    static Diagnostic warning(std::string code, std::string message) {
        return Diagnostic{Severity::Warning, std::move(code), std::move(message)};
    }

    const char* severityName() const noexcept {
        return severity == Severity::Error ? "error" : "warning";
    }
};

}  // namespace mpl
