#pragma once

#include "mpl/Project.hpp"
#include "mpl/frontend/Ast.hpp"
#include "mpl/frontend/Diagnostic.hpp"
#include "mpl/frontend/Hir.hpp"

#include <string>
#include <vector>

namespace mpl {

struct ExportRequest {
    std::string format;
    std::string path;
};

struct LoweringResult {
    Project project;
    std::vector<ExportRequest> exports;
};

LoweringResult lowerToProject(const Module& module, const HirModule& hir,
                              DiagnosticSink& diagnostics);

/// Stable semantic instance identity used by the text frontend.
InstanceId stableInstanceId(std::string_view key);

}  // namespace mpl
