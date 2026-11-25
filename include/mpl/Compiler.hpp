#pragma once

#include "mpl/Project.hpp"
#include "mpl/core/Diagnostic.hpp"
#include "mpl/core/ModuleInfo.hpp"
#include "mpl/core/Provenance.hpp"
#include "mpl/ir/ProductionIR.hpp"
#include "mpl/passes/PassManager.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mpl {

/// Everything a backend needs from one compilation, plus the trace that
/// documents how the artifacts were produced.
struct Compilation {
    MusicIR music;
    TemporalIR temporal;
    ProductionIR production;
    std::vector<PassStats> passes;
    std::vector<Diagnostic> diagnostics;
    std::vector<FunctionDefinitionRecord> functionDefinitions;
    std::vector<FunctionCallRecord> functionCalls;
    std::vector<ModuleRecord> modules;
    std::vector<SectionRecord> sections;

    bool hasErrors() const;
    std::size_t warningCount() const;

    /// Deterministic machine-readable report for synthesis systems: versions,
    /// module hashes, phase status, diagnostics and passes.
    std::string toReportJson() const;

    /// Hash of the standard-library modules used by this compilation (0 when no
    /// `Std*` module is present).
    std::uint64_t stdlibHash() const;

    /// `includePassTimings` is disabled by golden tests because wall-clock
    /// measurements are not deterministic.
    std::string toTraceJson(bool includePassTimings = true) const;
};

class Compiler {
public:
    /// Evaluates the Music IR, lowers every track into Production IR and runs
    /// the production passes. Semantic errors in the graph throw; production
    /// problems are reported through `Compilation::diagnostics`.
    Compilation compile(Project project) const;
};

}  // namespace mpl
