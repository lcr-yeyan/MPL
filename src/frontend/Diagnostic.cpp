#include "mpl/frontend/Diagnostic.hpp"

#include "mpl/util/JsonWriter.hpp"

#include <sstream>
#include <utility>

namespace mpl {
namespace {

std::string location(const SourceSpan& span) {
    std::ostringstream stream;
    if (!span.file.empty()) {
        stream << span.file << ":";
    }
    stream << span.line << ":" << span.column;
    return stream.str();
}

}  // namespace

std::string_view diagnosticSeverityName(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Error: return "error";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Note: return "note";
    }
    return "unknown";
}

void DiagnosticSink::add(FrontendDiagnostic diagnostic) {
    if (diagnostic.severity == DiagnosticSeverity::Error) {
        ++errorCount_;
    }
    diagnostics_.push_back(std::move(diagnostic));
}

void DiagnosticSink::error(std::string code, const SourceSpan& span, std::string message,
                           std::string expected, std::string actual, std::string related) {
    FrontendDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.span = span;
    diagnostic.message = std::move(message);
    diagnostic.expected = std::move(expected);
    diagnostic.actual = std::move(actual);
    diagnostic.related = std::move(related);
    add(std::move(diagnostic));
}

void DiagnosticSink::warning(std::string code, const SourceSpan& span, std::string message) {
    FrontendDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.severity = DiagnosticSeverity::Warning;
    diagnostic.span = span;
    diagnostic.message = std::move(message);
    add(std::move(diagnostic));
}

std::string DiagnosticSink::toHuman() const {
    std::ostringstream out;
    for (const FrontendDiagnostic& diagnostic : diagnostics_) {
        out << location(diagnostic.span) << ": " << diagnosticSeverityName(diagnostic.severity)
            << "[" << diagnostic.code << "]: " << diagnostic.message << "\n";
        if (!diagnostic.expected.empty()) {
            out << "  expected: " << diagnostic.expected << "\n";
        }
        if (!diagnostic.actual.empty()) {
            out << "  actual:   " << diagnostic.actual << "\n";
        }
        if (!diagnostic.related.empty()) {
            out << "  related:  " << diagnostic.related << "\n";
        }
        for (const std::string& suggestion : diagnostic.suggestions) {
            out << "  help:     " << suggestion << "\n";
        }
    }
    return out.str();
}

std::string DiagnosticSink::toJson() const {
    JsonWriter json;
    json.beginArray();
    for (const FrontendDiagnostic& diagnostic : diagnostics_) {
        json.beginObject();
        json.key("code").value(diagnostic.code);
        json.key("severity").value(diagnosticSeverityName(diagnostic.severity));
        json.key("file").value(diagnostic.span.file);
        json.key("line").value(diagnostic.span.line);
        json.key("column").value(diagnostic.span.column);
        json.key("endLine").value(diagnostic.span.endLine);
        json.key("endColumn").value(diagnostic.span.endColumn);
        json.key("offset").value(static_cast<std::int64_t>(diagnostic.span.begin));
        json.key("length").value(static_cast<std::int64_t>(diagnostic.span.end -
                                                           diagnostic.span.begin));
        json.key("message").value(diagnostic.message);
        json.key("expected").value(diagnostic.expected);
        json.key("actual").value(diagnostic.actual);
        json.key("related").value(diagnostic.related);
        json.key("suggestions").beginArray();
        for (const std::string& suggestion : diagnostic.suggestions) {
            json.value(suggestion);
        }
        json.endArray();
        json.endObject();
    }
    json.endArray();
    return json.take();
}

}  // namespace mpl
