#pragma once

#include "mpl/Project.hpp"
#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Hir.hpp"
#include "mpl/frontend/Lowering.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

struct FrontendProgram {
    Module module;
    HirModule hir;
    Project project;
    std::vector<ExportRequest> exports;
};

struct FrontendResult {
    std::optional<FrontendProgram> program;
    std::vector<FrontendDiagnostic> diagnostics;

    bool ok() const noexcept { return program.has_value(); }
    bool hasErrors() const noexcept;
};

/// Full closure: lex, parse, resolve/type-check, lower to Core. Does not write
/// artifacts or run the MIDI backend.
FrontendResult compileSource(std::string_view source, std::string file);

}  // namespace mpl
