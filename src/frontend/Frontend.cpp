#include "mpl/frontend/Frontend.hpp"

#include "mpl/frontend/Lexer.hpp"
#include "mpl/frontend/Lowering.hpp"
#include "mpl/frontend/Parser.hpp"
#include "mpl/frontend/Sema.hpp"

#include <utility>

namespace mpl {

bool FrontendResult::hasErrors() const noexcept {
    for (const FrontendDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.severity == DiagnosticSeverity::Error) {
            return true;
        }
    }
    return false;
}

FrontendResult compileSource(std::string_view source, std::string file) {
    FrontendResult result;
    DiagnosticSink diagnostics;

    Lexer lexer(source, file, diagnostics);
    Parser parser(lexer.lex(), diagnostics);
    Module module = parser.parse();
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    Sema sema(module, diagnostics);
    HirModule hir = sema.analyze();
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    LoweringResult lowered = lowerToProject(module, hir, diagnostics);
    if (diagnostics.hasErrors()) {
        result.diagnostics = diagnostics.diagnostics();
        return result;
    }

    FrontendProgram program;
    program.module = std::move(module);
    program.hir = std::move(hir);
    program.project = std::move(lowered.project);
    program.exports = std::move(lowered.exports);
    result.program = std::move(program);
    result.diagnostics = diagnostics.diagnostics();
    return result;
}

}  // namespace mpl
