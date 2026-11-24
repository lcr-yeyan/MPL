#pragma once

#include "mpl/frontend/Source.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace mpl {

enum class DiagnosticSeverity { Error, Warning, Note };

struct FrontendDiagnostic {
    std::string code;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    SourceSpan span;
    std::string message;
    std::string expected;
    std::string actual;
    std::string related;
    std::vector<std::string> suggestions;
};

std::string_view diagnosticSeverityName(DiagnosticSeverity severity);

class DiagnosticSink {
public:
    void add(FrontendDiagnostic diagnostic);
    void error(std::string code, const SourceSpan& span, std::string message,
               std::string expected = {}, std::string actual = {}, std::string related = {});
    void warning(std::string code, const SourceSpan& span, std::string message);

    bool hasErrors() const noexcept { return errorCount_ > 0; }
    std::size_t errorCount() const noexcept { return errorCount_; }
    const std::vector<FrontendDiagnostic>& diagnostics() const noexcept { return diagnostics_; }
    std::vector<FrontendDiagnostic>& diagnostics() noexcept { return diagnostics_; }

    /// Multi-line human-readable diagnostics.
    std::string toHuman() const;

    /// Machine-readable JSON array with stable field names.
    std::string toJson() const;

private:
    std::vector<FrontendDiagnostic> diagnostics_;
    std::size_t errorCount_ = 0;
};

}  // namespace mpl
