#include "mpl/passes/PassManager.hpp"

#include <chrono>
#include <utility>

namespace mpl {

void PassManager::add(std::unique_ptr<Pass> pass) {
    if (pass != nullptr) {
        passes_.push_back(std::move(pass));
    }
}

std::vector<PassStats> PassManager::run(ProductionIR& production,
                                        std::vector<Diagnostic>& diagnostics) const {
    std::vector<PassStats> stats;
    stats.reserve(passes_.size());

    for (const std::unique_ptr<Pass>& pass : passes_) {
        const auto start = std::chrono::steady_clock::now();
        pass->run(production, diagnostics);
        const auto finish = std::chrono::steady_clock::now();
        const std::chrono::duration<double, std::milli> elapsed = finish - start;
        stats.push_back(PassStats{std::string(pass->name()), elapsed.count()});
    }
    return stats;
}

}  // namespace mpl
