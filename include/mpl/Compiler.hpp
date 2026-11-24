#pragma once

#include "mpl/Project.hpp"
#include "mpl/core/Diagnostic.hpp"
#include "mpl/ir/ProductionIR.hpp"
#include "mpl/passes/PassManager.hpp"
#include "mpl/temporal/TemporalIR.hpp"

#include <cstddef>
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

    bool hasErrors() const;
    std::size_t warningCount() const;

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
